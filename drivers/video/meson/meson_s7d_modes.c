// SPDX-License-Identifier: GPL-2.0+
/*
 * ODROID-C5 boot display: choose the HDMI mode from the sink's EDID the way
 * the Linux kernel does.
 */

#include <common.h>
#include <div64.h>
#include <string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <amlogic/media/vout/hdmitx21/hdmitx.h>
#include <amlogic/media/vout/hdmitx21/hdmitx_module.h>
#include <amlogic/media/vout/hdmitx21/hdmi_common.h>

#include "meson_s7d.h"

/*
 * HDMI mode selection with the rules of Linux 6.12 (drm_edid.c, drm_modes.c,
 * drm_displayid.c, drm_probe_helper.c, drm_client_modeset.c) and the limits
 * of the S7D DRM driver (s7d-hdmi.c, s7d-caps.c, s7d-encp.c), without the
 * EDID quirk table. The probed modes are merged like
 * drm_connector_list_update() and pruned and sorted like
 * drm_helper_probe_single_connector_modes().
 */

/* DRM_MODE_FLAG_* */
#define MODE_FLAG_PHSYNC	BIT(0)
#define MODE_FLAG_NHSYNC	BIT(1)
#define MODE_FLAG_PVSYNC	BIT(2)
#define MODE_FLAG_NVSYNC	BIT(3)
#define MODE_FLAG_INTERLACE	BIT(4)
#define MODE_FLAG_DBLSCAN	BIT(5)
#define MODE_FLAG_DBLCLK	BIT(12)

/* DRM_MODE_TYPE_* */
#define MODE_TYPE_PREFERRED	BIT(3)
#define MODE_TYPE_DRIVER	BIT(6)

#define EDID_CEA_EXT		0x02
#define EDID_VTB_EXT		0x10
#define EDID_DISPLAYID_EXT	0x70
#define EDID_PT_HSYNC_POSITIVE	BIT(1)
#define EDID_PT_VSYNC_POSITIVE	BIT(2)
#define EDID_PT_STEREO		BIT(5)
#define EDID_PT_INTERLACED	BIT(7)
#define EDID_FEATURE_CONTINUOUS	BIT(0)
#define EDID_FEATURE_PREFERRED	BIT(1)
#define EDID_INPUT_DIGITAL	BIT(7)

#define EDID_DETAIL_EST_TIMINGS		0xf7
#define EDID_DETAIL_CVT_3BYTE		0xf8
#define EDID_DETAIL_STD_MODES		0xfa
#define EDID_DETAIL_MONITOR_RANGE	0xfd
#define EDID_RANGE_DEFAULT_GTF		0x00
#define EDID_RANGE_SECONDARY_GTF	0x02
#define EDID_RANGE_CVT			0x04
#define EDID_RANGE_CVT_REDUCED		BIT(4)

/* enum edid_block_status */
#define EDID_BLOCK_OK			0
#define EDID_BLOCK_ZERO			1
#define EDID_BLOCK_HEADER_CORRUPT	2
#define EDID_BLOCK_HEADER_REPAIR	3
#define EDID_BLOCK_HEADER_FIXED		4
#define EDID_BLOCK_CHECKSUM		5
#define EDID_BLOCK_VERSION		6
#define EDID_BLOCK_READ_FAIL		7

#define CTA_DB_VIDEO			2
#define CTA_DB_VENDOR			3
#define CTA_DB_EXTENDED_TAG		7
#define CTA_EXT_DB_420_VIDEO_DATA	0x0e
#define CTA_EXT_DB_420_VIDEO_CAP_MAP	0x0f
#define CTA_EXT_DB_HF_EEODB		0x78
#define CTA_EXT_DB_HF_SCDB		0x79
#define HDMI_IEEE_OUI			0x000c03
#define HDMI_FORUM_IEEE_OUI		0xc45dd8
#define MICROSOFT_IEEE_OUI		0xca125c

#define DISPLAYID_VER_20		0x20
#define DISPLAYID_TYPE_1_DETAILED	0x03
#define DISPLAYID_TILED_DISPLAY		0x12
#define DISPLAYID_2_TYPE_7_DETAILED	0x22
#define DISPLAYID_2_TILED_DISPLAY	0x28
#define DISPLAYID_CTA			0x81
#define DISPLAYID_USE_HMD_VR		7
#define DISPLAYID_USE_HMD_AR		8

/* standard_timing_level() */
#define LEVEL_DMT	0
#define LEVEL_GTF	1
#define LEVEL_GTF2	2
#define LEVEL_CVT	3

/* mode_config.max_width/max_height of the S7D DRM driver */
#define S7D_FB_MAX_WIDTH	4096
#define S7D_FB_MAX_HEIGHT	2304

#define CEA_VIC_COUNT		220
/*
 * The kernel's probed mode list has no limit. From EDID_MAX_BLOCK blocks
 * it gets at most this many distinct modes: 16 (CVT 3-byte) from each of
 * the 4 + 7 * 6 descriptors, 8 standard and 17 established timings, the
 * 88 DMT modes (est3, range limits), 7 inferred GTF, GTF2 and CVT modes
 * each, the 154 CTA VICs (the HDMI VICs are CTA VICs here) at both
 * clocks, and 5 DisplayID timings in each extension. An exact copy is
 * only counted.
 */
#define EDID_MODES_MAX		(46 * 16 + 8 + 17 + 88 + 3 * 7 + 2 * 154 + 7 * 5)
/* Seven CTA blocks hold at most 7 * 119 SVDs. */
#define EDID_VICS_MAX		840

/* The drm_display_mode fields the selection uses. */
struct edid_mode {
	u32 clock;		/* kHz */
	u16 hdisplay;
	u16 hsync_start;
	u16 hsync_end;
	u16 htotal;
	u16 vdisplay;
	u16 vsync_start;
	u16 vsync_end;
	u16 vtotal;
	u16 flags;		/* MODE_FLAG_* */
	u8 type;		/* MODE_TYPE_* */
	u8 aspect;		/* enum hdmi_picture_aspect */
};

/* The drm_display_info fields the selection uses. */
struct edid_sink {
	bool is_hdmi;
	bool scdc;
	bool scrambling;
	bool non_desktop;
	u32 max_tmds_clock;	/* kHz, 0 when the EDID gives none */
	u8 vics[EDID_VICS_MAX];
	int vics_len;
	u8 y420_vdb[CEA_VIC_COUNT / 8 + 1];
	u8 y420_cmdb[CEA_VIC_COUNT / 8 + 1];
};

/* The drm_connector tile fields. */
struct edid_tile {
	bool has_tile;
	u8 num_h_tile;
	u8 num_v_tile;
	u8 tile_h_loc;
	u8 tile_v_loc;
	u16 tile_h_size;
	u16 tile_v_size;
};

static u8 edid[EDID_BLK_SIZE * EDID_MAX_BLOCK];
/*
 * drm_edid->size in blocks. edid_load() reads EDID_MAX_BLOCK blocks at
 * most; the blocks past them are taken as all-zero blocks, which are
 * valid extensions that every parser skips.
 */
static int edid_size;
/* The blocks of a valid EDID the parsers walk. */
static int edid_blocks;
static const u8 edid_zero_block[EDID_BLK_SIZE];
static struct edid_sink sink;
static struct edid_tile tile;
/* connector->probed_modes, then connector->modes after the merge */
static struct edid_mode modes[EDID_MODES_MAX];
static int modes_len;
/* how often each probed mode was added */
static int modes_copies[EDID_MODES_MAX];
/* the return value of get_modes, dropped modes included */
static int modes_count;
static int modes_dropped;
/* drm_edid.c edid_cea_modes_1/_193 and their alternate clocks, by VIC */
static struct edid_mode cea_modes[CEA_VIC_COUNT];
static u32 cea_clock2[CEA_VIC_COUNT];
/* drm_monitor_supports_rb(), standard_timing_level() and find_gtf2() */
static bool edid_rb;
static int edid_level;
static const u8 *edid_gtf2;
static char custom_name[32];

#define DMT_PP		(MODE_FLAG_PHSYNC | MODE_FLAG_PVSYNC)
#define DMT_PN		(MODE_FLAG_PHSYNC | MODE_FLAG_NVSYNC)
#define DMT_NP		(MODE_FLAG_NHSYNC | MODE_FLAG_PVSYNC)
#define DMT_NN		(MODE_FLAG_NHSYNC | MODE_FLAG_NVSYNC)
#define DMT(c, hd, hss, hse, ht, vd, vss, vse, vt, f) {			\
	.clock = (c), .hdisplay = (hd), .hsync_start = (hss),		\
	.hsync_end = (hse), .htotal = (ht), .vdisplay = (vd),		\
	.vsync_start = (vss), .vsync_end = (vse), .vtotal = (vt),	\
	.flags = (f), .type = MODE_TYPE_DRIVER }

/* drm_dmt_modes[] */
static const struct edid_mode dmt_modes[] = {
	/* 0x01 - 640x350@85Hz */
	DMT(31500, 640, 672, 736, 832, 350, 382, 385, 445, DMT_PN),
	/* 0x02 - 640x400@85Hz */
	DMT(31500, 640, 672, 736, 832, 400, 401, 404, 445, DMT_NP),
	/* 0x03 - 720x400@85Hz */
	DMT(35500, 720, 756, 828, 936, 400, 401, 404, 446, DMT_NP),
	/* 0x04 - 640x480@60Hz */
	DMT(25175, 640, 656, 752, 800, 480, 490, 492, 525, DMT_NN),
	/* 0x05 - 640x480@72Hz */
	DMT(31500, 640, 664, 704, 832, 480, 489, 492, 520, DMT_NN),
	/* 0x06 - 640x480@75Hz */
	DMT(31500, 640, 656, 720, 840, 480, 481, 484, 500, DMT_NN),
	/* 0x07 - 640x480@85Hz */
	DMT(36000, 640, 696, 752, 832, 480, 481, 484, 509, DMT_NN),
	/* 0x08 - 800x600@56Hz */
	DMT(36000, 800, 824, 896, 1024, 600, 601, 603, 625, DMT_PP),
	/* 0x09 - 800x600@60Hz */
	DMT(40000, 800, 840, 968, 1056, 600, 601, 605, 628, DMT_PP),
	/* 0x0a - 800x600@72Hz */
	DMT(50000, 800, 856, 976, 1040, 600, 637, 643, 666, DMT_PP),
	/* 0x0b - 800x600@75Hz */
	DMT(49500, 800, 816, 896, 1056, 600, 601, 604, 625, DMT_PP),
	/* 0x0c - 800x600@85Hz */
	DMT(56250, 800, 832, 896, 1048, 600, 601, 604, 631, DMT_PP),
	/* 0x0d - 800x600@120Hz RB */
	DMT(73250, 800, 848, 880, 960, 600, 603, 607, 636, DMT_PN),
	/* 0x0e - 848x480@60Hz */
	DMT(33750, 848, 864, 976, 1088, 480, 486, 494, 517, DMT_PP),
	/* 0x0f - 1024x768@43Hz, interlace */
	DMT(44900, 1024, 1032, 1208, 1264, 768, 768, 776, 817,
	    DMT_PP | MODE_FLAG_INTERLACE),
	/* 0x10 - 1024x768@60Hz */
	DMT(65000, 1024, 1048, 1184, 1344, 768, 771, 777, 806, DMT_NN),
	/* 0x11 - 1024x768@70Hz */
	DMT(75000, 1024, 1048, 1184, 1328, 768, 771, 777, 806, DMT_NN),
	/* 0x12 - 1024x768@75Hz */
	DMT(78750, 1024, 1040, 1136, 1312, 768, 769, 772, 800, DMT_PP),
	/* 0x13 - 1024x768@85Hz */
	DMT(94500, 1024, 1072, 1168, 1376, 768, 769, 772, 808, DMT_PP),
	/* 0x14 - 1024x768@120Hz RB */
	DMT(115500, 1024, 1072, 1104, 1184, 768, 771, 775, 813, DMT_PN),
	/* 0x15 - 1152x864@75Hz */
	DMT(108000, 1152, 1216, 1344, 1600, 864, 865, 868, 900, DMT_PP),
	/* 0x55 - 1280x720@60Hz */
	DMT(74250, 1280, 1390, 1430, 1650, 720, 725, 730, 750, DMT_PP),
	/* 0x16 - 1280x768@60Hz RB */
	DMT(68250, 1280, 1328, 1360, 1440, 768, 771, 778, 790, DMT_PN),
	/* 0x17 - 1280x768@60Hz */
	DMT(79500, 1280, 1344, 1472, 1664, 768, 771, 778, 798, DMT_NP),
	/* 0x18 - 1280x768@75Hz */
	DMT(102250, 1280, 1360, 1488, 1696, 768, 771, 778, 805, DMT_NP),
	/* 0x19 - 1280x768@85Hz */
	DMT(117500, 1280, 1360, 1496, 1712, 768, 771, 778, 809, DMT_NP),
	/* 0x1a - 1280x768@120Hz RB */
	DMT(140250, 1280, 1328, 1360, 1440, 768, 771, 778, 813, DMT_PN),
	/* 0x1b - 1280x800@60Hz RB */
	DMT(71000, 1280, 1328, 1360, 1440, 800, 803, 809, 823, DMT_PN),
	/* 0x1c - 1280x800@60Hz */
	DMT(83500, 1280, 1352, 1480, 1680, 800, 803, 809, 831, DMT_NP),
	/* 0x1d - 1280x800@75Hz */
	DMT(106500, 1280, 1360, 1488, 1696, 800, 803, 809, 838, DMT_NP),
	/* 0x1e - 1280x800@85Hz */
	DMT(122500, 1280, 1360, 1496, 1712, 800, 803, 809, 843, DMT_NP),
	/* 0x1f - 1280x800@120Hz RB */
	DMT(146250, 1280, 1328, 1360, 1440, 800, 803, 809, 847, DMT_PN),
	/* 0x20 - 1280x960@60Hz */
	DMT(108000, 1280, 1376, 1488, 1800, 960, 961, 964, 1000, DMT_PP),
	/* 0x21 - 1280x960@85Hz */
	DMT(148500, 1280, 1344, 1504, 1728, 960, 961, 964, 1011, DMT_PP),
	/* 0x22 - 1280x960@120Hz RB */
	DMT(175500, 1280, 1328, 1360, 1440, 960, 963, 967, 1017, DMT_PN),
	/* 0x23 - 1280x1024@60Hz */
	DMT(108000, 1280, 1328, 1440, 1688, 1024, 1025, 1028, 1066, DMT_PP),
	/* 0x24 - 1280x1024@75Hz */
	DMT(135000, 1280, 1296, 1440, 1688, 1024, 1025, 1028, 1066, DMT_PP),
	/* 0x25 - 1280x1024@85Hz */
	DMT(157500, 1280, 1344, 1504, 1728, 1024, 1025, 1028, 1072, DMT_PP),
	/* 0x26 - 1280x1024@120Hz RB */
	DMT(187250, 1280, 1328, 1360, 1440, 1024, 1027, 1034, 1084, DMT_PN),
	/* 0x27 - 1360x768@60Hz */
	DMT(85500, 1360, 1424, 1536, 1792, 768, 771, 777, 795, DMT_PP),
	/* 0x28 - 1360x768@120Hz RB */
	DMT(148250, 1360, 1408, 1440, 1520, 768, 771, 776, 813, DMT_PN),
	/* 0x51 - 1366x768@60Hz */
	DMT(85500, 1366, 1436, 1579, 1792, 768, 771, 774, 798, DMT_PP),
	/* 0x56 - 1366x768@60Hz */
	DMT(72000, 1366, 1380, 1436, 1500, 768, 769, 772, 800, DMT_PP),
	/* 0x29 - 1400x1050@60Hz RB */
	DMT(101000, 1400, 1448, 1480, 1560, 1050, 1053, 1057, 1080, DMT_PN),
	/* 0x2a - 1400x1050@60Hz */
	DMT(121750, 1400, 1488, 1632, 1864, 1050, 1053, 1057, 1089, DMT_NP),
	/* 0x2b - 1400x1050@75Hz */
	DMT(156000, 1400, 1504, 1648, 1896, 1050, 1053, 1057, 1099, DMT_NP),
	/* 0x2c - 1400x1050@85Hz */
	DMT(179500, 1400, 1504, 1656, 1912, 1050, 1053, 1057, 1105, DMT_NP),
	/* 0x2d - 1400x1050@120Hz RB */
	DMT(208000, 1400, 1448, 1480, 1560, 1050, 1053, 1057, 1112, DMT_PN),
	/* 0x2e - 1440x900@60Hz RB */
	DMT(88750, 1440, 1488, 1520, 1600, 900, 903, 909, 926, DMT_PN),
	/* 0x2f - 1440x900@60Hz */
	DMT(106500, 1440, 1520, 1672, 1904, 900, 903, 909, 934, DMT_NP),
	/* 0x30 - 1440x900@75Hz */
	DMT(136750, 1440, 1536, 1688, 1936, 900, 903, 909, 942, DMT_NP),
	/* 0x31 - 1440x900@85Hz */
	DMT(157000, 1440, 1544, 1696, 1952, 900, 903, 909, 948, DMT_NP),
	/* 0x32 - 1440x900@120Hz RB */
	DMT(182750, 1440, 1488, 1520, 1600, 900, 903, 909, 953, DMT_PN),
	/* 0x53 - 1600x900@60Hz */
	DMT(108000, 1600, 1624, 1704, 1800, 900, 901, 904, 1000, DMT_PP),
	/* 0x33 - 1600x1200@60Hz */
	DMT(162000, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250, DMT_PP),
	/* 0x34 - 1600x1200@65Hz */
	DMT(175500, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250, DMT_PP),
	/* 0x35 - 1600x1200@70Hz */
	DMT(189000, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250, DMT_PP),
	/* 0x36 - 1600x1200@75Hz */
	DMT(202500, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250, DMT_PP),
	/* 0x37 - 1600x1200@85Hz */
	DMT(229500, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250, DMT_PP),
	/* 0x38 - 1600x1200@120Hz RB */
	DMT(268250, 1600, 1648, 1680, 1760, 1200, 1203, 1207, 1271, DMT_PN),
	/* 0x39 - 1680x1050@60Hz RB */
	DMT(119000, 1680, 1728, 1760, 1840, 1050, 1053, 1059, 1080, DMT_PN),
	/* 0x3a - 1680x1050@60Hz */
	DMT(146250, 1680, 1784, 1960, 2240, 1050, 1053, 1059, 1089, DMT_NP),
	/* 0x3b - 1680x1050@75Hz */
	DMT(187000, 1680, 1800, 1976, 2272, 1050, 1053, 1059, 1099, DMT_NP),
	/* 0x3c - 1680x1050@85Hz */
	DMT(214750, 1680, 1808, 1984, 2288, 1050, 1053, 1059, 1105, DMT_NP),
	/* 0x3d - 1680x1050@120Hz RB */
	DMT(245500, 1680, 1728, 1760, 1840, 1050, 1053, 1059, 1112, DMT_PN),
	/* 0x3e - 1792x1344@60Hz */
	DMT(204750, 1792, 1920, 2120, 2448, 1344, 1345, 1348, 1394, DMT_NP),
	/* 0x3f - 1792x1344@75Hz */
	DMT(261000, 1792, 1888, 2104, 2456, 1344, 1345, 1348, 1417, DMT_NP),
	/* 0x40 - 1792x1344@120Hz RB */
	DMT(333250, 1792, 1840, 1872, 1952, 1344, 1347, 1351, 1423, DMT_PN),
	/* 0x41 - 1856x1392@60Hz */
	DMT(218250, 1856, 1952, 2176, 2528, 1392, 1393, 1396, 1439, DMT_NP),
	/* 0x42 - 1856x1392@75Hz */
	DMT(288000, 1856, 1984, 2208, 2560, 1392, 1393, 1396, 1500, DMT_NP),
	/* 0x43 - 1856x1392@120Hz RB */
	DMT(356500, 1856, 1904, 1936, 2016, 1392, 1395, 1399, 1474, DMT_PN),
	/* 0x52 - 1920x1080@60Hz */
	DMT(148500, 1920, 2008, 2052, 2200, 1080, 1084, 1089, 1125, DMT_NN),
	/* 0x44 - 1920x1200@60Hz RB */
	DMT(154000, 1920, 1968, 2000, 2080, 1200, 1203, 1209, 1235, DMT_PN),
	/* 0x45 - 1920x1200@60Hz */
	DMT(193250, 1920, 2056, 2256, 2592, 1200, 1203, 1209, 1245, DMT_NP),
	/* 0x46 - 1920x1200@75Hz */
	DMT(245250, 1920, 2056, 2264, 2608, 1200, 1203, 1209, 1255, DMT_NP),
	/* 0x47 - 1920x1200@85Hz */
	DMT(281250, 1920, 2064, 2272, 2624, 1200, 1203, 1209, 1262, DMT_NP),
	/* 0x48 - 1920x1200@120Hz RB */
	DMT(317000, 1920, 1968, 2000, 2080, 1200, 1203, 1209, 1271, DMT_PN),
	/* 0x49 - 1920x1440@60Hz */
	DMT(234000, 1920, 2048, 2256, 2600, 1440, 1441, 1444, 1500, DMT_NP),
	/* 0x4a - 1920x1440@75Hz */
	DMT(297000, 1920, 2064, 2288, 2640, 1440, 1441, 1444, 1500, DMT_NP),
	/* 0x4b - 1920x1440@120Hz RB */
	DMT(380500, 1920, 1968, 2000, 2080, 1440, 1443, 1447, 1525, DMT_PN),
	/* 0x54 - 2048x1152@60Hz */
	DMT(162000, 2048, 2074, 2154, 2250, 1152, 1153, 1156, 1200, DMT_PP),
	/* 0x4c - 2560x1600@60Hz RB */
	DMT(268500, 2560, 2608, 2640, 2720, 1600, 1603, 1609, 1646, DMT_PN),
	/* 0x4d - 2560x1600@60Hz */
	DMT(348500, 2560, 2752, 3032, 3504, 1600, 1603, 1609, 1658, DMT_NP),
	/* 0x4e - 2560x1600@75Hz */
	DMT(443250, 2560, 2768, 3048, 3536, 1600, 1603, 1609, 1672, DMT_NP),
	/* 0x4f - 2560x1600@85Hz */
	DMT(505250, 2560, 2768, 3048, 3536, 1600, 1603, 1609, 1682, DMT_NP),
	/* 0x50 - 2560x1600@120Hz RB */
	DMT(552750, 2560, 2608, 2640, 2720, 1600, 1603, 1609, 1694, DMT_PN),
	/* 0x57 - 4096x2160@60Hz RB */
	DMT(556744, 4096, 4104, 4136, 4176, 2160, 2208, 2216, 2222, DMT_PN),
	/* 0x58 - 4096x2160@59.94Hz RB */
	DMT(556188, 4096, 4104, 4136, 4176, 2160, 2208, 2216, 2222, DMT_PN),
};

/* edid_est_modes[], by established timing bit */
static const struct edid_mode est_modes[] = {
	/* 800x600@60Hz */
	DMT(40000, 800, 840, 968, 1056, 600, 601, 605, 628, DMT_PP),
	/* 800x600@56Hz */
	DMT(36000, 800, 824, 896, 1024, 600, 601, 603, 625, DMT_PP),
	/* 640x480@75Hz */
	DMT(31500, 640, 656, 720, 840, 480, 481, 484, 500, DMT_NN),
	/* 640x480@72Hz */
	DMT(31500, 640, 664, 704, 832, 480, 489, 492, 520, DMT_NN),
	/* 640x480@67Hz */
	DMT(30240, 640, 704, 768, 864, 480, 483, 486, 525, DMT_NN),
	/* 640x480@60Hz */
	DMT(25175, 640, 656, 752, 800, 480, 490, 492, 525, DMT_NN),
	/* 720x400@88Hz */
	DMT(35500, 720, 738, 846, 900, 400, 421, 423, 449, DMT_NN),
	/* 720x400@70Hz */
	DMT(28320, 720, 738, 846, 900, 400, 412, 414, 449, DMT_NP),
	/* 1280x1024@75Hz */
	DMT(135000, 1280, 1296, 1440, 1688, 1024, 1025, 1028, 1066, DMT_PP),
	/* 1024x768@75Hz */
	DMT(78750, 1024, 1040, 1136, 1312, 768, 769, 772, 800, DMT_PP),
	/* 1024x768@70Hz */
	DMT(75000, 1024, 1048, 1184, 1328, 768, 771, 777, 806, DMT_NN),
	/* 1024x768@60Hz */
	DMT(65000, 1024, 1048, 1184, 1344, 768, 771, 777, 806, DMT_NN),
	/* 1024x768@43Hz */
	DMT(44900, 1024, 1032, 1208, 1264, 768, 768, 776, 817,
	    DMT_PP | MODE_FLAG_INTERLACE),
	/* 832x624@75Hz */
	DMT(57284, 832, 864, 928, 1152, 624, 625, 628, 667, DMT_NN),
	/* 800x600@75Hz */
	DMT(49500, 800, 816, 896, 1056, 600, 601, 604, 625, DMT_PP),
	/* 800x600@72Hz */
	DMT(50000, 800, 856, 976, 1040, 600, 637, 643, 666, DMT_PP),
	/* 1152x864@75Hz */
	DMT(108000, 1152, 1216, 1344, 1600, 864, 865, 868, 900, DMT_PP),
};

struct minimode {
	short w;
	short h;
	short r;
	short rb;
};

/* est3_modes[], by bit of bytes 6 to 11 of the descriptor */
static const struct minimode est3_modes[] = {
	/* byte 6 */
	{ 640, 350, 85, 0 },
	{ 640, 400, 85, 0 },
	{ 720, 400, 85, 0 },
	{ 640, 480, 85, 0 },
	{ 848, 480, 60, 0 },
	{ 800, 600, 85, 0 },
	{ 1024, 768, 85, 0 },
	{ 1152, 864, 75, 0 },
	/* byte 7 */
	{ 1280, 768, 60, 1 },
	{ 1280, 768, 60, 0 },
	{ 1280, 768, 75, 0 },
	{ 1280, 768, 85, 0 },
	{ 1280, 960, 60, 0 },
	{ 1280, 960, 85, 0 },
	{ 1280, 1024, 60, 0 },
	{ 1280, 1024, 85, 0 },
	/* byte 8 */
	{ 1360, 768, 60, 0 },
	{ 1440, 900, 60, 1 },
	{ 1440, 900, 60, 0 },
	{ 1440, 900, 75, 0 },
	{ 1440, 900, 85, 0 },
	{ 1400, 1050, 60, 1 },
	{ 1400, 1050, 60, 0 },
	{ 1400, 1050, 75, 0 },
	/* byte 9 */
	{ 1400, 1050, 85, 0 },
	{ 1680, 1050, 60, 1 },
	{ 1680, 1050, 60, 0 },
	{ 1680, 1050, 75, 0 },
	{ 1680, 1050, 85, 0 },
	{ 1600, 1200, 60, 0 },
	{ 1600, 1200, 65, 0 },
	{ 1600, 1200, 70, 0 },
	/* byte 10 */
	{ 1600, 1200, 75, 0 },
	{ 1600, 1200, 85, 0 },
	{ 1792, 1344, 60, 0 },
	{ 1792, 1344, 75, 0 },
	{ 1856, 1392, 60, 0 },
	{ 1856, 1392, 75, 0 },
	{ 1920, 1200, 60, 1 },
	{ 1920, 1200, 60, 0 },
	/* byte 11 */
	{ 1920, 1200, 75, 0 },
	{ 1920, 1200, 85, 0 },
	{ 1920, 1440, 60, 0 },
	{ 1920, 1440, 75, 0 },
};

/* extra_modes[]: the GTF and CVT modes inferred from range limits */
static const struct minimode extra_modes[] = {
	{ 1024, 576, 60, 0 },
	{ 1366, 768, 60, 0 },
	{ 1600, 900, 60, 0 },
	{ 1680, 945, 60, 0 },
	{ 1920, 1080, 60, 0 },
	{ 2048, 1152, 60, 0 },
	{ 2048, 1536, 60, 0 },
};

static bool vic_listed(const u8 *list, int len, u32 vic)
{
	int i;

	for (i = 0; i < len; i++)
		if (list[i] == vic)
			return true;
	return false;
}

/* cea_next_vic() */
static u32 cea_next_vic(u32 vic)
{
	return vic == 127 ? 193 : vic + 1;
}

/* drm_valid_cea_vic() */
static bool cea_vic_valid(u32 vic)
{
	return ((vic >= 1 && vic <= 127) || (vic >= 193 && vic < CEA_VIC_COUNT)) &&
		cea_modes[vic].clock;
}

static void edid_mode_from_timing(const struct hdmi_timing *t, bool dblclk,
				  struct edid_mode *m)
{
	u32 div = dblclk ? 2 : 1;
	/* Interlaced entries hold the porch and sync of one field. */
	u32 vmul = t->pi_mode ? 1 : 2;

	memset(m, 0, sizeof(*m));
	m->clock = t->pixel_freq / div;
	m->hdisplay = t->h_active / div;
	m->hsync_start = m->hdisplay + t->h_front / div;
	m->hsync_end = m->hsync_start + t->h_sync / div;
	m->htotal = t->h_total / div;
	m->vdisplay = t->v_active;
	m->vsync_start = m->vdisplay + t->v_front * vmul;
	m->vsync_end = m->vsync_start + t->v_sync * vmul;
	m->vtotal = t->v_total;
	m->flags = (t->h_pol ? MODE_FLAG_PHSYNC : MODE_FLAG_NHSYNC) |
		(t->v_pol ? MODE_FLAG_PVSYNC : MODE_FLAG_NVSYNC);
	if (!t->pi_mode)
		m->flags |= MODE_FLAG_INTERLACE;
	if (dblclk)
		m->flags |= MODE_FLAG_DBLCLK;
	m->type = MODE_TYPE_DRIVER;
	if (t->h_pict == 4 && t->v_pict == 3)
		m->aspect = HDMI_PICTURE_ASPECT_4_3;
	else if (t->h_pict == 16 && t->v_pict == 9)
		m->aspect = HDMI_PICTURE_ASPECT_16_9;
	else if (t->h_pict == 64 && t->v_pict == 27)
		m->aspect = HDMI_PICTURE_ASPECT_64_27;
	else if (t->h_pict == 256 && t->v_pict == 135)
		m->aspect = HDMI_PICTURE_ASPECT_256_135;
}

/*
 * drm_mode_vrefresh(): 0 where check_mul_overflow() fails on the int
 * clock or the int htotal * vtotal.
 */
static int mode_vrefresh(const struct edid_mode *m)
{
	u32 num = 1, den = 1, total;

	if (!m->htotal || !m->vtotal)
		return 0;
	if (m->flags & MODE_FLAG_INTERLACE)
		num *= 2;
	if (m->flags & MODE_FLAG_DBLSCAN)
		den *= 2;
	if (m->clock > INT_MAX || (u64)m->clock * num > U32_MAX)
		return 0;
	num *= m->clock;
	total = (u32)m->htotal * m->vtotal;
	if (total > INT_MAX || (u64)total * den > U32_MAX)
		return 0;
	den *= total;
	return DIV_ROUND_CLOSEST_ULL((u64)num * 1000, den);
}

/* drm_mode_hsync() */
static int mode_hsync(const struct edid_mode *m)
{
	if (!m->htotal)
		return 0;
	return (m->clock + m->htotal / 2) / m->htotal;
}

/* cea_mode_alternate_clock(): 60 Hz vs. 59.94 Hz */
static u32 cea_alternate_clock(const struct edid_mode *cea)
{
	u64 clock = cea->clock;

	if (mode_vrefresh(cea) % 6 != 0)
		return clock;
	if (cea->vdisplay == 240 || cea->vdisplay == 480)
		return DIV_ROUND_CLOSEST_ULL(clock * 1001, 1000);
	return DIV_ROUND_CLOSEST_ULL(clock * 1000, 1001);
}

/* The CEA modes as drm_edid.c has them, from the hdmitx VIC table. */
static void cea_modes_init(void)
{
	/* drm_edid.c marks these DRM_MODE_FLAG_DBLCLK; the table holds 2x pixels. */
	static const u8 dblclk[] = {
		6, 7, 8, 9, 21, 22, 23, 24, 44, 45, 50, 51, 54, 55, 58, 59,
	};
	/* The table holds the longest-vtotal variant, drm_edid.c the shortest. */
	static const u8 vtotal_262[] = { 8, 9, 12, 13 };
	static const u8 vtotal_312[] = { 23, 24, 27, 28 };
	const struct hdmi_timing *t = hdmitx21_get_timing_para0();
	int i, n = hdmitx21_timing_size();

	memset(cea_modes, 0, sizeof(cea_modes));
	memset(cea_clock2, 0, sizeof(cea_clock2));
	for (i = 0; i < n; i++, t++) {
		struct edid_mode *m;
		u32 vtotal = 0;

		if (!t->vic || t->vic >= CEA_VIC_COUNT ||
		    (t->vic > 127 && t->vic < 193))
			continue;
		m = &cea_modes[t->vic];
		edid_mode_from_timing(t, vic_listed(dblclk, ARRAY_SIZE(dblclk), t->vic), m);
		if (vic_listed(vtotal_262, ARRAY_SIZE(vtotal_262), t->vic))
			vtotal = 262;
		else if (vic_listed(vtotal_312, ARRAY_SIZE(vtotal_312), t->vic))
			vtotal = 312;
		if (vtotal && m->vtotal > vtotal) {
			m->vsync_start -= m->vtotal - vtotal;
			m->vsync_end -= m->vtotal - vtotal;
			m->vtotal = vtotal;
		}
		cea_clock2[t->vic] = cea_alternate_clock(m);
	}
}

/* s7d_mode_clock(): a DBLCLK mode sends each pixel twice. */
static u32 mode_clock(const struct edid_mode *m)
{
	return m->flags & MODE_FLAG_DBLCLK ? 2 * m->clock : m->clock;
}

static bool mode_match_timings(const struct edid_mode *a, const struct edid_mode *b)
{
	return a->hdisplay == b->hdisplay && a->hsync_start == b->hsync_start &&
		a->hsync_end == b->hsync_end && a->htotal == b->htotal &&
		a->vdisplay == b->vdisplay && a->vsync_start == b->vsync_start &&
		a->vsync_end == b->vsync_end && a->vtotal == b->vtotal;
}

/* drm_mode_match() with TIMINGS | FLAGS, plus ASPECT_RATIO when @a has one */
static bool cea_mode_match(const struct edid_mode *a, const struct edid_mode *b)
{
	return mode_match_timings(a, b) && a->flags == b->flags &&
		(!a->aspect || a->aspect == b->aspect);
}

/* drm_mode_equal(): the clocks compare as KHZ2PICOS() when both are set */
static bool mode_equal(const struct edid_mode *a, const struct edid_mode *b)
{
	if (!mode_match_timings(a, b))
		return false;
	if (a->clock && b->clock) {
		if (1000000000U / a->clock != 1000000000U / b->clock)
			return false;
	} else if (a->clock != b->clock) {
		return false;
	}
	return a->flags == b->flags && a->aspect == b->aspect;
}

/* cea_mode_alternate_timings() */
static bool cea_alternate_timings(u32 vic, struct edid_mode *m)
{
	if (((vic == 8 || vic == 9 || vic == 12 || vic == 13) && m->vtotal < 263) ||
	    ((vic == 23 || vic == 24 || vic == 27 || vic == 28) && m->vtotal < 314)) {
		m->vsync_start++;
		m->vsync_end++;
		m->vtotal++;
		return true;
	}
	return false;
}

/*
 * drm_match_cea_mode() for @tolerance < 0 (equal clock period in ps), else
 * drm_match_cea_mode_clock_tolerance(). The HDMI 1.4 4k list the kernel
 * tries next only repeats CEA VICs 93-95 and 98, which match first.
 */
static u32 cea_match(const struct edid_mode *m, int tolerance)
{
	u32 vic;

	if (!m->clock)
		return 0;
	for (vic = 1; vic < CEA_VIC_COUNT; vic = cea_next_vic(vic)) {
		struct edid_mode cea = cea_modes[vic];
		u32 clock1 = cea.clock;
		u32 clock2 = cea_clock2[vic];

		if (!clock1)
			continue;
		if (tolerance < 0) {
			u32 ps = 1000000000U / m->clock;

			if (ps != 1000000000U / clock1 && ps != 1000000000U / clock2)
				continue;
		} else if (abs((int)m->clock - (int)clock1) > tolerance &&
			   abs((int)m->clock - (int)clock2) > tolerance) {
			continue;
		}
		do {
			if (cea_mode_match(m, &cea))
				return vic;
		} while (cea_alternate_timings(vic, &cea));
	}
	return 0;
}

static bool vic_bit(const u8 *map, u32 vic)
{
	return vic < CEA_VIC_COUNT && (map[vic / 8] & BIT(vic % 8));
}

static void vic_bit_set(u8 *map, u32 vic)
{
	if (vic < CEA_VIC_COUNT)
		map[vic / 8] |= BIT(vic % 8);
}

static const u8 edid_header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

/* Block @i of the EDID; the blocks edid_load() does not read are zero. */
static const u8 *edid_block(int i)
{
	return i < EDID_MAX_BLOCK ? edid + i * EDID_BLK_SIZE : edid_zero_block;
}

static bool edid_block_is_zero(const u8 *b)
{
	int i;

	for (i = 0; i < EDID_BLK_SIZE; i++)
		if (b[i])
			return false;
	return true;
}

/* edid_block_check() with edid_fixup = 6 */
static int edid_block_check(const u8 *b, bool base)
{
	u8 csum = 0;
	int i, score = 0;

	if (base) {
		for (i = 0; i < ARRAY_SIZE(edid_header); i++)
			score += b[i] == edid_header[i];
		if (score < 6)
			return edid_block_is_zero(b) ? EDID_BLOCK_ZERO :
				EDID_BLOCK_HEADER_CORRUPT;
		if (score < 8)
			return EDID_BLOCK_HEADER_REPAIR;
	}
	for (i = 0; i < EDID_BLK_SIZE - 1; i++)
		csum += b[i];
	if ((u8)(0x100 - csum) != b[EDID_BLK_SIZE - 1])
		return edid_block_is_zero(b) ? EDID_BLOCK_ZERO : EDID_BLOCK_CHECKSUM;
	if (base && b[18] != 1)
		return EDID_BLOCK_VERSION;
	return EDID_BLOCK_OK;
}

/* edid_block_status_valid(): a CTA block with a bad checksum is kept. */
static bool edid_status_valid(int status, u8 tag)
{
	return status == EDID_BLOCK_OK || status == EDID_BLOCK_HEADER_FIXED ||
		(status == EDID_BLOCK_CHECKSUM && tag == EDID_CEA_EXT);
}

/* edid_block_valid() */
static bool edid_block_valid(const u8 *b, bool base)
{
	return edid_status_valid(edid_block_check(b, base), b[0]);
}

/* drm_do_probe_ddc_edid(): five tries, none more after a NAK */
static int edid_ddc_read(u8 *b, int block, int len)
{
	int ret, retries = 5;

	do {
		ret = hdmitx21_read_edid_block(b, block, len);
	} while (ret && ret != -ENXIO && --retries);
	return ret;
}

/*
 * edid_block_read(): read a block up to four times until it is valid,
 * repairing a base header with 6 good bytes.
 */
static int edid_block_read(u8 *b, int block)
{
	bool base = !block;
	int status = EDID_BLOCK_READ_FAIL;
	int try;

	for (try = 0; try < 4; try++) {
		if (edid_ddc_read(b, block, EDID_BLK_SIZE))
			return EDID_BLOCK_READ_FAIL;
		status = edid_block_check(b, base);
		if (status == EDID_BLOCK_HEADER_REPAIR) {
			memcpy(b, edid_header, sizeof(edid_header));
			status = edid_block_check(b, base);
			if (status == EDID_BLOCK_OK)
				status = EDID_BLOCK_HEADER_FIXED;
		}
		if (edid_status_valid(status, b[0]))
			break;
		/* an all-zero base block fails at once */
		if (!try && base && status == EDID_BLOCK_ZERO)
			break;
	}
	return status;
}

static int db_tag(const u8 *db)
{
	return db[0] >> 5;
}

static int db_len(const u8 *db)
{
	return db[0] & 0x1f;
}

/* cea_db_is_extended_tag() */
static bool db_extended(const u8 *db, int tag)
{
	return db_tag(db) == CTA_DB_EXTENDED_TAG && db_len(db) >= 1 && db[1] == tag;
}

/* cea_db_is_vendor() */
static bool db_vendor(const u8 *db, u32 oui)
{
	return db_tag(db) == CTA_DB_VENDOR && db_len(db) >= 3 &&
		(db[3] << 16 | db[2] << 8 | db[1]) == oui;
}

/* cea_db_is_hdmi_vsdb() */
static bool db_hdmi_vsdb(const u8 *db)
{
	return db_vendor(db, HDMI_IEEE_OUI) && db_len(db) >= 5;
}

/* cea_db_collection_size() */
static int cta_collection_size(const u8 *cta)
{
	u8 d = cta[2];

	if (d < 4 || d > 127)
		return 0;
	return d - 4;
}

/* edid_hfeeodb_block_count() on the blocks read so far */
static int edid_hfeeodb_block_count(void)
{
	const u8 *cta = edid_block(1);

	if (!edid[126])
		return 0;
	if (cta[0] != EDID_CEA_EXT || cta[1] < 3)
		return 0;
	if (cta_collection_size(cta) < 3)
		return 0;
	/* cea_db_is_hdmi_forum_eeodb() at bytes 4 to 6 of block 1 */
	if (!db_extended(cta + 4, CTA_EXT_DB_HF_EEODB) || db_len(cta + 4) < 2)
		return 0;
	return cta[6] ? cta[6] + 1 : 0;
}

/* __drm_edid_block_count(): HF-EEODB overrides the extension count. */
static int edid_block_count(void)
{
	int eeodb;

	if (edid_size >= 2) {
		eeodb = edid_hfeeodb_block_count();
		if (eeodb)
			return eeodb;
	}
	return edid[126] + 1;
}

/* edid_filter_invalid_blocks(): walks the base count, not HF-EEODB */
static void edid_filter_invalid_blocks(void)
{
	int i, valid = 0, n = edid[126] + 1;
	u8 csum = 0;

	for (i = 0; i < n; i++) {
		const u8 *src = edid_block(i);

		if (!edid_block_valid(src, i == 0))
			continue;
		if (valid < EDID_MAX_BLOCK && valid != i)
			memmove(edid + valid * EDID_BLK_SIZE, src, EDID_BLK_SIZE);
		valid++;
	}
	if (valid < EDID_MAX_BLOCK)
		memset(edid + valid * EDID_BLK_SIZE, 0,
		       (EDID_MAX_BLOCK - valid) * EDID_BLK_SIZE);
	edid[126] = valid - 1;
	for (i = 0; i < EDID_BLK_SIZE - 1; i++)
		csum += edid[i];
	edid[127] = 0x100 - csum;
	edid_size = valid;
}

/*
 * _drm_do_get_edid(): returns false where the kernel returns no EDID, also
 * when a block read fails.
 */
static bool edid_load(void)
{
	int i, n, eeodb, status, invalid = 0;

	memset(edid, 0, sizeof(edid));
	edid_size = 0;
	if (!edid_status_valid(edid_block_read(edid, 0), edid[0]))
		return false;
	n = edid[126] + 1;
	for (i = 1; i < n && i < EDID_MAX_BLOCK; i++) {
		u8 *b = edid + i * EDID_BLK_SIZE;

		status = edid_block_read(b, i);
		if (!edid_status_valid(status, b[0])) {
			if (status == EDID_BLOCK_READ_FAIL)
				return false;
			invalid++;
		} else if (i == 1) {
			eeodb = edid_hfeeodb_block_count();
			if (eeodb > n)
				n = eeodb;
		}
	}
	edid_size = n;
	if (invalid)
		edid_filter_invalid_blocks();
	return true;
}

/* drm_edid_valid(): the size must match the block count the EDID gives. */
static bool edid_valid(void)
{
	int i;

	if (!edid_size || edid_block_count() != edid_size)
		return false;
	for (i = 0; i < edid_size; i++)
		if (!edid_block_valid(edid_block(i), i == 0))
			return false;
	return true;
}

/*
 * s7d_hdmi_get_edid(): read again 500 ms later if the sink stays connected.
 * A sink that has dropped HPD gives no EDID; the connector stays connected
 * from the first detect, so it gets the no-EDID modes.
 */
static bool edid_read(struct hdmitx_dev *hdev)
{
	int attempt;
	u8 probe;

	edid_size = 0;
	for (attempt = 0; attempt < 2; attempt++) {
		if (attempt)
			mdelay(500);
		if (!hdev->hwop.get_hpd_state())
			return false;
		/* drm_edid_read_ddc(), at 50 kHz like the S7D DRM driver */
		hdmitx_config_i2c_rate(DDC_I2C_50K);
		/* drm_probe_ddc(), then _drm_do_get_edid() */
		if (!edid_ddc_read(&probe, 0, 1) && edid_load())
			return true;
	}
	return false;
}

/* struct displayid_iter */
struct did_iter {
	const u8 *section;
	int length;
	int idx;
	int ext_index;
	bool done;
	u8 version;
	u8 primary_use;
};

/* drm_find_displayid_extension() and validate_displayid() */
static const u8 *did_find_section(struct did_iter *it)
{
	const u8 *ext = NULL;
	int i, len;
	u8 csum = 0;

	for (i = it->ext_index; i < edid_blocks - 1; i++) {
		ext = edid_block(i + 1);
		if (ext[0] == EDID_DISPLAYID_EXT)
			break;
	}
	if (i >= edid_blocks - 1)
		return NULL;
	it->ext_index = i + 1;
	/* the EDID block checksum is not part of the section */
	it->length = EDID_BLK_SIZE - 1;
	it->idx = 1;
	len = 4 + ext[it->idx + 1] + 1;
	if (len > it->length - it->idx)
		return NULL;
	for (i = 0; i < len; i++)
		csum += ext[it->idx + i];
	if (csum)
		return NULL;
	it->length = it->idx + 4 + ext[it->idx + 1];
	return ext;
}

/* displayid_iter_block() */
static const u8 *did_block(const struct did_iter *it)
{
	const u8 *b;

	if (!it->section)
		return NULL;
	b = it->section + it->idx;
	if (it->idx + 3 <= it->length && it->idx + 3 + b[2] <= it->length)
		return b;
	return NULL;
}

/*
 * __displayid_iter_next(): the data blocks of the DisplayID extensions;
 * a section with a bad checksum ends the walk.
 */
static const u8 *did_next(struct did_iter *it)
{
	const u8 *b;

	if (it->done)
		return NULL;
	if (it->section) {
		b = did_block(it);
		it->idx += 3 + b[2];
		b = did_block(it);
		if (b)
			return b;
	}
	for (;;) {
		bool base_section = !it->section;

		it->section = did_find_section(it);
		if (!it->section) {
			it->done = true;
			return NULL;
		}
		if (base_section) {
			it->version = it->section[it->idx];
			it->primary_use = it->section[it->idx + 2];
		}
		it->idx += 4;
		b = did_block(it);
		if (b)
			return b;
	}
}

/* struct cea_db_iter */
struct cta_iter {
	int block;
	struct did_iter did;
	const u8 *collection;
	int index;
	int end;
};

/* __cea_db_iter_current_block() */
static const u8 *cta_current(const struct cta_iter *it)
{
	const u8 *db;

	if (!it->collection)
		return NULL;
	db = it->collection + it->index;
	if (it->index + 1 <= it->end && it->index + 1 + db_len(db) <= it->end)
		return db;
	return NULL;
}

/*
 * __cea_db_iter_next(): the data blocks of the CTA extensions of revision
 * 3 or later, then those of the DisplayID CTA data blocks.
 */
static const u8 *cta_next(struct cta_iter *it)
{
	const u8 *db;

	if (it->collection) {
		db = cta_current(it);
		it->index += 1 + db_len(db);
		db = cta_current(it);
		if (db)
			return db;
	}
	for (;;) {
		it->collection = NULL;
		/* __cea_db_iter_edid_next() */
		while (!it->collection && it->block < edid_blocks) {
			const u8 *ext = edid_block(it->block++);

			if (ext[0] != EDID_CEA_EXT || ext[1] < 3 ||
			    !cta_collection_size(ext))
				continue;
			it->collection = ext;
			it->index = 4;
			it->end = 4 + cta_collection_size(ext);
		}
		/* __cea_db_iter_displayid_next() */
		while (!it->collection) {
			const u8 *b = did_next(&it->did);

			if (!b)
				return NULL;
			if (b[0] != DISPLAYID_CTA)
				continue;
			it->collection = b;
			it->index = 3;
			it->end = 3 + b[2];
		}
		db = cta_current(it);
		if (db)
			return db;
	}
}

/* drm_edid_has_cta_extension() */
static bool edid_has_cta(void)
{
	struct did_iter did = { 0 };
	const u8 *b;
	int i;

	for (i = 1; i < edid_blocks; i++)
		if (edid_block(i)[0] == EDID_CEA_EXT)
			return true;
	while ((b = did_next(&did)))
		if (b[0] == DISPLAYID_CTA)
			return true;
	return false;
}

/* svd_to_vic() */
static u32 svd_to_vic(u8 svd)
{
	if ((svd >= 1 && svd <= 64) || (svd >= 129 && svd <= 192))
		return svd & 127;
	return svd;
}

/* update_display_info() with drm_parse_cea_ext() and update_displayid_info() */
static void edid_parse_sink(void)
{
	struct cta_iter it = { .block = 1 };
	struct did_iter did = { 0 };
	u64 y420cmdb_map = 0;
	const u8 *db;
	int i, j;

	memset(&sink, 0, sizeof(sink));
	if (edid[19] < 3 || !(edid[20] & EDID_INPUT_DIGITAL))
		return;
	while ((db = cta_next(&it))) {
		int len = db_len(db);

		if (db_hdmi_vsdb(db)) {
			/* drm_parse_hdmi_vsdb_video() */
			sink.is_hdmi = true;
			if (len >= 7)
				sink.max_tmds_clock = db[7] * 5000;
		} else if ((db_vendor(db, HDMI_FORUM_IEEE_OUI) ||
			    db_extended(db, CTA_EXT_DB_HF_SCDB)) && len >= 7) {
			/* drm_parse_hdmi_forum_scds() */
			if (db[6] & 0x80)
				sink.scdc = true;
			if (db[5]) {
				if (db[5] * 5000 > 340000)
					sink.max_tmds_clock = db[5] * 5000;
				if (sink.scdc)
					sink.scrambling = true;
			}
		} else if (db_vendor(db, MICROSOFT_IEEE_OUI) && len == 21) {
			/* drm_parse_microsoft_vsdb(): HMD or specialized display */
			if (db[4] == 1 || db[4] == 2 || (db[4] == 3 && !(db[5] & BIT(6))))
				sink.non_desktop = true;
		} else if (db_extended(db, CTA_EXT_DB_420_VIDEO_CAP_MAP)) {
			/* parse_cta_y420cmdb() */
			if (len == 1) {
				y420cmdb_map = U64_MAX;
			} else {
				y420cmdb_map = 0;
				for (j = 0; j < min(len - 1, 8); j++)
					y420cmdb_map |= (u64)db[2 + j] << (8 * j);
			}
		} else if (db_extended(db, CTA_EXT_DB_420_VIDEO_DATA)) {
			/* parse_cta_y420vdb() */
			for (j = 0; j < len - 1; j++) {
				u32 vic = svd_to_vic(db[2 + j]);

				if (cea_vic_valid(vic))
					vic_bit_set(sink.y420_vdb, vic);
			}
		} else if (db_tag(db) == CTA_DB_VIDEO) {
			/* parse_cta_vdb() */
			for (j = 0; j < len && sink.vics_len < EDID_VICS_MAX; j++) {
				u32 vic = svd_to_vic(db[1 + j]);

				sink.vics[sink.vics_len++] = cea_vic_valid(vic) ? vic : 0;
			}
		}
	}
	/* update_cta_y420cmdb() */
	for (i = 0; i < min(sink.vics_len, 64); i++)
		if (sink.vics[i] && (y420cmdb_map & BIT_ULL(i)))
			vic_bit_set(sink.y420_cmdb, sink.vics[i]);
	/* update_displayid_info(): the base section, read with its first data block */
	if (did_next(&did) && did.version == DISPLAYID_VER_20 &&
	    (did.primary_use == DISPLAYID_USE_HMD_VR ||
	     did.primary_use == DISPLAYID_USE_HMD_AR))
		sink.non_desktop = true;
}

/*
 * A byte of a DisplayID tiled block. drm_parse_tiled_block() does not check
 * the block length, so its fields may come from the next EDID block; past
 * the blocks read they are zero.
 */
static u8 did_byte(const u8 *b, int offset)
{
	return (b - edid) + offset < sizeof(edid) ? b[offset] : 0;
}

/* displayid_is_tiled_block() */
static bool did_is_tiled_block(const struct did_iter *it, const u8 *b)
{
	return (it->version < DISPLAYID_VER_20 && b[0] == DISPLAYID_TILED_DISPLAY) ||
		(it->version == DISPLAYID_VER_20 && b[0] == DISPLAYID_2_TILED_DISPLAY);
}

/* _drm_update_tile_info() and drm_parse_tiled_block() */
static void edid_parse_tile(void)
{
	struct did_iter it = { 0 };
	const u8 *b;
	u8 topo0, topo1, topo2;

	memset(&tile, 0, sizeof(tile));
	while ((b = did_next(&it))) {
		if (!did_is_tiled_block(&it, b))
			continue;
		topo0 = did_byte(b, 4);
		topo1 = did_byte(b, 5);
		topo2 = did_byte(b, 6);
		tile.has_tile = true;
		tile.num_v_tile = ((topo0 & 0xf) | (topo2 & 0x30)) + 1;
		tile.num_h_tile = ((topo0 >> 4) | ((topo2 >> 2) & 0x30)) + 1;
		tile.tile_v_loc = (topo1 & 0xf) | ((topo2 & 0x3) << 4);
		tile.tile_h_loc = (topo1 >> 4) | (((topo2 >> 2) & 0x3) << 4);
		tile.tile_h_size = (did_byte(b, 7) | did_byte(b, 8) << 8) + 1;
		tile.tile_v_size = (did_byte(b, 9) | did_byte(b, 10) << 8) + 1;
	}
}

typedef void (*detailed_cb)(const u8 *d, void *closure);

/*
 * drm_for_each_detailed_block(): the four base descriptors, then those of
 * the CTA and VTB extensions.
 */
static void edid_for_each_detailed(detailed_cb cb, void *closure)
{
	int b, i, n;

	for (i = 0; i < 4; i++)
		cb(edid + 54 + 18 * i, closure);
	for (b = 1; b < edid_blocks; b++) {
		const u8 *ext = edid_block(b);
		u8 d = ext[2];

		if (ext[0] == EDID_CEA_EXT) {
			/* cea_for_each_detailed_block() */
			if (d < 4 || d > 127)
				continue;
			n = (127 - d) / 18;
			for (i = 0; i < n; i++)
				cb(ext + d + 18 * i, closure);
		} else if (ext[0] == EDID_VTB_EXT && ext[1] == 1) {
			/* vtb_for_each_detailed_block() */
			n = min_t(int, d, 6);
			for (i = 0; i < n; i++)
				cb(ext + 5 + 18 * i, closure);
		}
	}
}

/* is_display_descriptor() */
static bool is_display_descriptor(const u8 *d, u8 type)
{
	return !d[0] && !d[1] && !d[2] && d[3] == type;
}

static void range_rb_cb(const u8 *d, void *closure)
{
	if (is_display_descriptor(d, EDID_DETAIL_MONITOR_RANGE) &&
	    d[10] == EDID_RANGE_CVT && (d[15] & EDID_RANGE_CVT_REDUCED))
		*(bool *)closure = true;
}

static void range_gtf2_cb(const u8 *d, void *closure)
{
	if (is_display_descriptor(d, EDID_DETAIL_MONITOR_RANGE) &&
	    d[10] == EDID_RANGE_SECONDARY_GTF)
		*(const u8 **)closure = d;
}

static void range_level_cb(const u8 *d, void *closure)
{
	if (!is_display_descriptor(d, EDID_DETAIL_MONITOR_RANGE))
		return;
	if (d[10] == EDID_RANGE_DEFAULT_GTF)
		*(int *)closure = LEVEL_GTF;
	else if (d[10] == EDID_RANGE_SECONDARY_GTF)
		*(int *)closure = LEVEL_GTF2;
	else if (d[10] == EDID_RANGE_CVT)
		*(int *)closure = LEVEL_CVT;
}

/* drm_gtf2_hbreak() */
static int gtf2_hbreak(void)
{
	return edid_gtf2 ? edid_gtf2[12] * 2 : 0;
}

/*
 * drm_monitor_supports_rb(), find_gtf2() and standard_timing_level(): they
 * depend on the EDID only.
 */
static void edid_ranges_init(void)
{
	edid_rb = false;
	if (edid[19] >= 4)
		edid_for_each_detailed(range_rb_cb, &edid_rb);
	else
		edid_rb = edid[20] & EDID_INPUT_DIGITAL;
	edid_gtf2 = NULL;
	edid_for_each_detailed(range_gtf2_cb, &edid_gtf2);
	if (edid[19] >= 4) {
		edid_level = LEVEL_CVT;
		edid_for_each_detailed(range_level_cb, &edid_level);
	} else if (edid[19] >= 3 && gtf2_hbreak()) {
		edid_level = LEVEL_GTF2;
	} else if (edid[19] >= 2) {
		edid_level = LEVEL_GTF;
	} else {
		edid_level = LEVEL_DMT;
	}
}

/*
 * drm_mode_probed_add(). A copy of a probed mode, equal in every field,
 * changes nothing the kernel derives from the list: it merges into the
 * first copy and matches the same standard and inferred timings. It is
 * only counted, for its alternate CEA mode; a copy of a mode before
 * @first is not, as the alternate modes are made from those.
 */
static void mode_store(const struct edid_mode *m, int first)
{
	int i;

	for (i = 0; i < modes_len; i++) {
		if (mode_match_timings(&modes[i], m) && modes[i].clock == m->clock &&
		    modes[i].flags == m->flags && modes[i].type == m->type &&
		    modes[i].aspect == m->aspect) {
			if (i >= first)
				modes_copies[i]++;
			return;
		}
	}
	if (modes_len < EDID_MODES_MAX) {
		modes_copies[modes_len] = 1;
		modes[modes_len++] = *m;
	} else {
		modes_dropped++;
	}
}

/* A mode get_modes adds; its count is the return value. */
static void mode_add_from(const struct edid_mode *m, int first)
{
	modes_count++;
	mode_store(m, first);
}

static void mode_add(const struct edid_mode *m)
{
	mode_add_from(m, 0);
}

/* drm_display_mode_from_cea_vic() */
static void cea_mode_add(u32 vic)
{
	if (cea_vic_valid(vic))
		mode_add(&cea_modes[vic]);
}

/* mode_is_rb() */
static bool mode_is_rb(const struct edid_mode *m)
{
	return m->htotal - m->hdisplay == 160 && m->hsync_end - m->hdisplay == 80 &&
		m->hsync_end - m->hsync_start == 32 && m->vsync_start - m->vdisplay == 3;
}

/* drm_mode_find_dmt() */
static const struct edid_mode *dmt_find(int hsize, int vsize, int fresh, bool rb)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dmt_modes); i++) {
		const struct edid_mode *m = &dmt_modes[i];

		if (m->hdisplay == hsize && m->vdisplay == vsize &&
		    mode_vrefresh(m) == fresh && mode_is_rb(m) == rb)
			return m;
	}
	return NULL;
}

/*
 * drm_cvt_mode() for a progressive mode without margins, the only kind the
 * EDID asks for, with the kernel's integer types. The mode type stays 0.
 */
static bool cvt_mode(struct edid_mode *m, int hdisplay, int vdisplay,
		     int vrefresh, bool reduced)
{
	unsigned int vfieldrate, hperiod;
	int vsync;
	u64 tmp;

	if (!hdisplay || !vdisplay)
		return false;
	memset(m, 0, sizeof(*m));
	if (!vrefresh)
		vrefresh = 60;
	vfieldrate = vrefresh;
	m->hdisplay = hdisplay - hdisplay % 8;
	m->vdisplay = vdisplay;

	if (!(vdisplay % 3) && vdisplay * 4 / 3 == hdisplay)
		vsync = 4;
	else if (!(vdisplay % 9) && vdisplay * 16 / 9 == hdisplay)
		vsync = 5;
	else if (!(vdisplay % 10) && vdisplay * 16 / 10 == hdisplay)
		vsync = 6;
	else if (!(vdisplay % 4) && vdisplay * 5 / 4 == hdisplay)
		vsync = 7;
	else if (!(vdisplay % 9) && vdisplay * 15 / 9 == hdisplay)
		vsync = 7;
	else
		vsync = 10;

	if (!reduced) {
		unsigned int hblank_percentage;
		int tmp1, tmp2, vsyncandback_porch, hblank;

		tmp1 = 1000 * 1000000 - 550 * 1000 * vfieldrate;
		tmp2 = (vdisplay + 3) * 2;
		hperiod = tmp1 * 2 / (tmp2 * vfieldrate);
		tmp1 = 550 * 1000 / hperiod + 1;
		if (tmp1 < vsync + 3)
			vsyncandback_porch = vsync + 3;
		else
			vsyncandback_porch = tmp1;
		m->vtotal = vdisplay + vsyncandback_porch + 3;
		/* CVT_C_PRIME * 1000 - CVT_M_PRIME * hperiod / 1000 */
		hblank_percentage = 30 * 1000 - 300 * hperiod / 1000;
		if (hblank_percentage < 20 * 1000)
			hblank_percentage = 20 * 1000;
		hblank = m->hdisplay * hblank_percentage /
			(100 * 1000 - hblank_percentage);
		hblank -= hblank % 16;
		m->htotal = m->hdisplay + hblank;
		m->hsync_end = m->hdisplay + hblank / 2;
		m->hsync_start = m->hsync_end - m->htotal * 8 / 100;
		/* this adds 8 also to a multiple of 8 (drm_modes.c:764) */
		m->hsync_start += 8 - m->hsync_start % 8;
		m->vsync_start = m->vdisplay + 3;
		m->vsync_end = m->vsync_start + vsync;
	} else {
		int tmp1, tmp2, vbilines;

		tmp1 = 1000 * 1000000 - 460 * 1000 * vfieldrate;
		tmp2 = vdisplay;
		hperiod = tmp1 / (tmp2 * vfieldrate);
		vbilines = 460 * 1000 / hperiod + 1;
		if (vbilines < 3 + vsync + 6)
			vbilines = 3 + vsync + 6;
		m->vtotal = vdisplay + vbilines;
		m->htotal = m->hdisplay + 160;
		m->hsync_end = m->hdisplay + 80;
		m->hsync_start = m->hsync_end - 32;
		m->vsync_start = m->vdisplay + 3;
		m->vsync_end = m->vsync_start + vsync;
	}
	/*
	 * The 250 kHz step subtracts clock % 250 with the clock still 0
	 * (drm_modes.c:806), so the clock is not rounded.
	 */
	tmp = m->htotal;
	tmp *= 1000 * 1000;
	do_div(tmp, hperiod);
	m->clock = tmp;
	m->flags = reduced ? MODE_FLAG_PHSYNC | MODE_FLAG_NVSYNC :
		MODE_FLAG_PVSYNC | MODE_FLAG_NHSYNC;
	return true;
}

/*
 * drm_gtf_mode_complex() for a progressive mode without margins, the only
 * kind the EDID asks for, with the kernel's integer types. C and J come
 * doubled. The mode type stays 0.
 */
static bool gtf_mode_complex(struct edid_mode *m, int hdisplay, int vdisplay,
			     int vrefresh, int gtf_m, int gtf_2c, int gtf_k,
			     int gtf_2j)
{
	unsigned int hdisplay_rnd, vdisplay_rnd, vfieldrate_rqd, hfreq_est;
	unsigned int vtotal_lines, total_active_pixels, ideal_duty_cycle;
	unsigned int hblank, total_pixels, pixel_freq, tmp1, tmp2;
	int vsync_plus_bp, hsync, hfront_porch;
	/* GTF_C_PRIME and GTF_M_PRIME */
	int c_prime = ((gtf_2c - gtf_2j) * gtf_k / 256 + gtf_2j) / 2;
	int m_prime = gtf_k * gtf_m / 256;

	if (!hdisplay || !vdisplay)
		return false;
	memset(m, 0, sizeof(*m));
	hdisplay_rnd = (hdisplay + 8 / 2) / 8;
	hdisplay_rnd = hdisplay_rnd * 8;
	vdisplay_rnd = vdisplay;
	vfieldrate_rqd = vrefresh;

	tmp1 = (1000000 - 550 * vfieldrate_rqd) / 500;
	tmp2 = (vdisplay_rnd + 1) * 2;
	hfreq_est = (tmp2 * 1000 * vfieldrate_rqd) / tmp1;
	vsync_plus_bp = 550 * hfreq_est / 1000;
	vsync_plus_bp = (vsync_plus_bp + 500) / 1000;
	vtotal_lines = vdisplay_rnd + vsync_plus_bp + 1;
	total_active_pixels = hdisplay_rnd;
	/* the kernel's int GTF_M_PRIME * 1000000 wraps to these bits */
	ideal_duty_cycle = c_prime * 1000 - ((u32)m_prime * 1000000U / hfreq_est);
	/* Secondary GTF parameters can make the divisor 0; arm64 divides to 0. */
	if (100000 - ideal_duty_cycle)
		hblank = total_active_pixels * ideal_duty_cycle /
			(100000 - ideal_duty_cycle);
	else
		hblank = 0;
	hblank = (hblank + 8) / (2 * 8);
	hblank = hblank * 2 * 8;
	total_pixels = total_active_pixels + hblank;
	pixel_freq = total_pixels * hfreq_est / 1000;
	hsync = 8 * total_pixels / 100;
	hsync = (hsync + 8 / 2) / 8;
	hsync = hsync * 8;
	hfront_porch = hblank / 2 - hsync;

	m->hdisplay = hdisplay_rnd;
	m->hsync_start = hdisplay_rnd + hfront_porch;
	m->hsync_end = m->hsync_start + hsync;
	m->htotal = total_pixels;
	m->vdisplay = vdisplay_rnd;
	m->vsync_start = vdisplay_rnd + 1;
	m->vsync_end = m->vsync_start + 3;
	m->vtotal = vtotal_lines;
	m->clock = pixel_freq;
	if (gtf_m == 600 && gtf_2c == 80 && gtf_k == 128 && gtf_2j == 40)
		m->flags = MODE_FLAG_NHSYNC | MODE_FLAG_PVSYNC;
	else
		m->flags = MODE_FLAG_PHSYNC | MODE_FLAG_NVSYNC;
	return true;
}

/* drm_gtf_mode() */
static bool gtf_mode(struct edid_mode *m, int hdisplay, int vdisplay, int vrefresh)
{
	return gtf_mode_complex(m, hdisplay, vdisplay, vrefresh,
				600, 40 * 2, 128, 20 * 2);
}

/* drm_gtf2_mode(): the secondary curve above its break frequency */
static bool gtf2_mode(struct edid_mode *m, int hsize, int vsize, int vrefresh)
{
	const u8 *d = edid_gtf2;

	if (!gtf_mode(m, hsize, vsize, vrefresh))
		return false;
	if (mode_hsync(m) > gtf2_hbreak())
		return gtf_mode_complex(m, hsize, vsize, vrefresh,
					d ? d[14] | d[15] << 8 : 0, d ? d[13] : 0,
					d ? d[16] : 0, d ? d[17] : 0);
	return true;
}

/* drm_mode_detailed() */
static bool edid_detailed_mode(const u8 *d, struct edid_mode *m)
{
	/* cea_interlaced[] of drm_mode_do_interlace_quirk() */
	static const u16 interlaced[][2] = {
		{ 1920, 1080 }, { 720, 480 }, { 1440, 480 }, { 2880, 480 },
		{ 720, 576 }, { 1440, 576 }, { 2880, 576 },
	};
	const u8 *pt = d + 2;
	u32 hactive = (pt[2] & 0xf0) << 4 | pt[0];
	u32 vactive = (pt[5] & 0xf0) << 4 | pt[3];
	u32 hblank = (pt[2] & 0xf) << 8 | pt[1];
	u32 vblank = (pt[5] & 0xf) << 8 | pt[4];
	u32 hsync_offset = (pt[9] & 0xc0) << 2 | pt[6];
	u32 hsync_width = (pt[9] & 0x30) << 4 | pt[7];
	u32 vsync_offset = (pt[9] & 0xc) << 2 | pt[8] >> 4;
	u32 vsync_width = (pt[9] & 0x3) << 4 | (pt[8] & 0xf);
	u8 misc = pt[15];
	int i;

	if (hactive < 64 || vactive < 64)
		return false;
	if (misc & EDID_PT_STEREO)
		return false;
	if (!hsync_width || !vsync_width)
		return false;

	memset(m, 0, sizeof(*m));
	m->clock = (d[0] | d[1] << 8) * 10;
	m->hdisplay = hactive;
	m->hsync_start = hactive + hsync_offset;
	m->hsync_end = m->hsync_start + hsync_width;
	m->htotal = hactive + hblank;
	m->vdisplay = vactive;
	m->vsync_start = vactive + vsync_offset;
	m->vsync_end = m->vsync_start + vsync_width;
	m->vtotal = vactive + vblank;
	if (m->hsync_end > m->htotal)
		m->hsync_end = m->htotal;
	if (m->vsync_end > m->vtotal)
		m->vsync_end = m->vtotal;

	if (misc & EDID_PT_INTERLACED) {
		for (i = 0; i < ARRAY_SIZE(interlaced); i++) {
			if (m->hdisplay == interlaced[i][0] &&
			    m->vdisplay == interlaced[i][1] / 2) {
				m->vdisplay *= 2;
				m->vsync_start *= 2;
				m->vsync_end *= 2;
				m->vtotal = m->vtotal * 2 | 1;
			}
		}
		m->flags |= MODE_FLAG_INTERLACE;
	}
	m->flags |= misc & EDID_PT_HSYNC_POSITIVE ? MODE_FLAG_PHSYNC : MODE_FLAG_NHSYNC;
	m->flags |= misc & EDID_PT_VSYNC_POSITIVE ? MODE_FLAG_PVSYNC : MODE_FLAG_NVSYNC;
	m->type = MODE_TYPE_DRIVER;
	return true;
}

/* fixup_detailed_cea_mode_clock(): DTD clocks have a 10 kHz resolution. */
static void cea_fixup_clock(struct edid_mode *m)
{
	u32 vic = cea_match(m, 5);
	int clock1, clock2;

	if (!vic)
		return;
	clock1 = cea_modes[vic].clock;
	clock2 = cea_clock2[vic];
	if (abs((int)m->clock - clock1) < abs((int)m->clock - clock2))
		m->clock = clock1;
	else
		m->clock = clock2;
}

/* do_detailed_mode(): the first detailed mode may be preferred. */
static void detailed_mode_cb(const u8 *d, void *closure)
{
	bool *preferred = closure;
	struct edid_mode m;

	if (!(d[0] | d[1]))
		return;
	if (!edid_detailed_mode(d, &m))
		return;
	if (*preferred)
		m.type |= MODE_TYPE_PREFERRED;
	cea_fixup_clock(&m);
	mode_add(&m);
	*preferred = false;
}

/* add_detailed_modes() */
static void edid_add_detailed_modes(void)
{
	bool preferred = edid[19] >= 4 || (edid[24] & EDID_FEATURE_PREFERRED);

	edid_for_each_detailed(detailed_mode_cb, &preferred);
}

/* do_cvt_mode() and drm_cvt_modes() */
static void cvt_modes_cb(const u8 *d, void *closure)
{
	static const int rates[] = { 60, 85, 75, 60, 50 };
	struct edid_mode m;
	int i, j, width, height;

	if (!is_display_descriptor(d, EDID_DETAIL_CVT_3BYTE))
		return;
	for (i = 0; i < 4; i++) {
		/*
		 * struct detailed_non_pixel puts the codes at byte 5, the
		 * VESA version byte, so the kernel reads each code one byte
		 * early (drm_edid.c:4019).
		 */
		const u8 *code = d + 5 + 3 * i;

		if (!code[0] && !code[1] && !code[2])
			continue;
		height = (code[0] + ((code[1] & 0xf0) << 4) + 1) * 2;
		switch (code[1] & 0x0c) {
		default:
		case 0x00:
			width = height * 4 / 3;
			break;
		case 0x04:
			width = height * 16 / 9;
			break;
		case 0x08:
			width = height * 16 / 10;
			break;
		case 0x0c:
			width = height * 15 / 9;
			break;
		}
		/* bit 0, reduced blanking, is never read (drm_edid.c:4042) */
		for (j = 1; j < 5; j++)
			if ((code[2] & (1 << j)) &&
			    cvt_mode(&m, width, height, rates[j], j == 0))
				mode_add(&m);
	}
}

/* add_cvt_modes() */
static void edid_add_cvt_modes(void)
{
	if (edid[19] >= 3)
		edid_for_each_detailed(cvt_modes_cb, NULL);
}

/* drm_mode_std() */
static bool mode_std(struct edid_mode *m, u8 hcode, u8 vfreq_aspect)
{
	u32 aspect_ratio = (vfreq_aspect & 0xc0) >> 6;
	u32 vfreq = vfreq_aspect & 0x3f;
	const struct edid_mode *dmt;
	int hsize, vsize, vrefresh_rate, i;

	/* bad_std_timing() */
	if ((hcode == 0x00 && vfreq_aspect == 0x00) ||
	    (hcode == 0x01 && vfreq_aspect == 0x01) ||
	    (hcode == 0x20 && vfreq_aspect == 0x20))
		return false;
	hsize = hcode * 8 + 248;
	vrefresh_rate = vfreq + 60;
	if (aspect_ratio == 0)
		vsize = edid[19] < 3 ? hsize : hsize * 10 / 16;
	else if (aspect_ratio == 1)
		vsize = hsize * 3 / 4;
	else if (aspect_ratio == 2)
		vsize = hsize * 4 / 5;
	else
		vsize = hsize * 9 / 16;
	/* HDTV hack, part 1 */
	if (vrefresh_rate == 60 &&
	    ((hsize == 1360 && vsize == 765) || (hsize == 1368 && vsize == 769))) {
		hsize = 1366;
		vsize = 768;
	}
	/* A probed mode of this size and refresh rate is used instead. */
	for (i = 0; i < modes_len; i++)
		if (modes[i].hdisplay == hsize && modes[i].vdisplay == vsize &&
		    mode_vrefresh(&modes[i]) == vrefresh_rate)
			return false;
	/* HDTV hack, part 2 */
	if (hsize == 1366 && vsize == 768 && vrefresh_rate == 60) {
		cvt_mode(m, 1366, 768, vrefresh_rate, false);
		m->hdisplay = 1366;
		m->hsync_start--;
		m->hsync_end--;
		return true;
	}
	dmt = edid_rb ? dmt_find(hsize, vsize, vrefresh_rate, true) : NULL;
	if (!dmt)
		dmt = dmt_find(hsize, vsize, vrefresh_rate, false);
	if (dmt) {
		*m = *dmt;
		return true;
	}
	switch (edid_level) {
	case LEVEL_GTF:
		return gtf_mode(m, hsize, vsize, vrefresh_rate);
	case LEVEL_GTF2:
		return gtf2_mode(m, hsize, vsize, vrefresh_rate);
	case LEVEL_CVT:
		return cvt_mode(m, hsize, vsize, vrefresh_rate, false);
	default:
		return false;
	}
}

/* do_standard_modes() */
static void std_modes_cb(const u8 *d, void *closure)
{
	struct edid_mode m;
	int i;

	if (!is_display_descriptor(d, EDID_DETAIL_STD_MODES))
		return;
	for (i = 0; i < 6; i++)
		if (mode_std(&m, d[5 + 2 * i], d[6 + 2 * i]))
			mode_add(&m);
}

/* add_standard_modes() */
static void edid_add_standard_modes(void)
{
	struct edid_mode m;
	int i;

	for (i = 0; i < 8; i++)
		if (mode_std(&m, edid[38 + 2 * i], edid[39 + 2 * i]))
			mode_add(&m);
	if (edid[19] >= 1)
		edid_for_each_detailed(std_modes_cb, NULL);
}

/* do_established_modes() and drm_est3_modes() */
static void est3_modes_cb(const u8 *d, void *closure)
{
	const u8 *est = d + 6;
	const struct edid_mode *dmt;
	int i, j, m;

	if (!is_display_descriptor(d, EDID_DETAIL_EST_TIMINGS))
		return;
	for (i = 0; i < 6; i++) {
		for (j = 7; j >= 0; j--) {
			m = i * 8 + (7 - j);
			if (m >= ARRAY_SIZE(est3_modes))
				break;
			if (!(est[i] & (1 << j)))
				continue;
			dmt = dmt_find(est3_modes[m].w, est3_modes[m].h,
				       est3_modes[m].r, est3_modes[m].rb);
			if (dmt)
				mode_add(dmt);
		}
	}
}

/* add_established_modes() */
static void edid_add_established_modes(void)
{
	u32 est_bits = edid[35] | edid[36] << 8 | (edid[37] & 0x80) << 9;
	int i;

	for (i = 0; i < ARRAY_SIZE(est_modes); i++)
		if (est_bits & BIT(i))
			mode_add(&est_modes[i]);
	if (edid[19] >= 1)
		edid_for_each_detailed(est3_modes_cb, NULL);
}

/*
 * do_hdmi_vsdb_modes(): the 2D HDMI VICs. The 3D modes it also adds have
 * stereo flags, which drm_mode_validate_flag() prunes on this connector;
 * they copy 2D modes already probed, so they change nothing else.
 */
static void hdmi_vsdb_modes_add(const u8 *db)
{
	/* edid_4k_modes[] are CEA VICs 95, 94, 93 and 98. */
	static const u8 hdmi_vic_to_cea[] = { 0, 95, 94, 93, 98 };
	int len = db_len(db);
	int offset = 0, vic_len, i;

	if (len < 8 || !(db[8] & BIT(5)))
		return;
	if (db[8] & BIT(7))
		offset += db[8] & BIT(6) ? 4 : 2;
	if (len < 8 + offset + 2)
		return;
	offset += 2;
	vic_len = db[8 + offset] >> 5;
	for (i = 0; i < vic_len && len >= 9 + offset + i; i++) {
		u8 vic = db[9 + offset + i];

		if (vic && vic < ARRAY_SIZE(hdmi_vic_to_cea))
			cea_mode_add(hdmi_vic_to_cea[vic]);
	}
}

/* add_cea_modes(): VDB, HDMI VSDB and Y420VDB modes */
static void edid_add_cea_modes(void)
{
	struct cta_iter it = { .block = 1 };
	const u8 *db;
	int i;

	for (i = 0; i < sink.vics_len; i++)
		if (sink.vics[i])
			cea_mode_add(sink.vics[i]);
	while ((db = cta_next(&it))) {
		if (db_hdmi_vsdb(db)) {
			hdmi_vsdb_modes_add(db);
		} else if (db_extended(db, CTA_EXT_DB_420_VIDEO_DATA)) {
			/* do_y420vdb_modes() */
			for (i = 0; i < db_len(db) - 1; i++)
				cea_mode_add(svd_to_vic(db[2 + i]));
		}
	}
}

/* add_alternate_cea_modes() */
static void edid_add_alternate_cea_modes(void)
{
	int i, j, n = modes_len;

	if (!edid_has_cta())
		return;
	for (i = 0; i < n; i++) {
		u32 vic = cea_match(&modes[i], -1);
		struct edid_mode m;
		u32 clock1, clock2;

		if (!vic)
			continue;
		clock1 = cea_modes[vic].clock;
		clock2 = cea_clock2[vic];
		if (clock1 == clock2 ||
		    (modes[i].clock != clock1 && modes[i].clock != clock2))
			continue;
		m = cea_modes[vic];
		m.clock = modes[i].clock != clock1 ? clock1 : clock2;
		/*
		 * One for each copy of the probed mode. The kernel adds them
		 * after the walk, so they do not count as copies of the
		 * modes it walks.
		 */
		for (j = 0; j < modes_copies[i]; j++)
			mode_add_from(&m, n);
	}
}

/* drm_mode_displayid_detailed() */
static void displayid_detailed_mode(const u8 *t, bool type_7, struct edid_mode *m)
{
	u32 pixel_clock = (t[0] | t[1] << 8 | t[2] << 16) + 1;
	u32 hactive = (t[4] | t[5] << 8) + 1;
	u32 hblank = (t[6] | t[7] << 8) + 1;
	u32 hsync = (t[8] | (t[9] & 0x7f) << 8) + 1;
	u32 hsync_width = (t[10] | t[11] << 8) + 1;
	u32 vactive = (t[12] | t[13] << 8) + 1;
	u32 vblank = (t[14] | t[15] << 8) + 1;
	u32 vsync = (t[16] | (t[17] & 0x7f) << 8) + 1;
	u32 vsync_width = (t[18] | t[19] << 8) + 1;

	memset(m, 0, sizeof(*m));
	/* kHz for type VII, 10 kHz for type I; the flags are not read */
	m->clock = type_7 ? pixel_clock : pixel_clock * 10;
	m->hdisplay = hactive;
	m->hsync_start = m->hdisplay + hsync;
	m->hsync_end = m->hsync_start + hsync_width;
	m->htotal = m->hdisplay + hblank;
	m->vdisplay = vactive;
	m->vsync_start = m->vdisplay + vsync;
	m->vsync_end = m->vsync_start + vsync_width;
	m->vtotal = m->vdisplay + vblank;
	m->flags = (t[9] & 0x80 ? MODE_FLAG_PHSYNC : MODE_FLAG_NHSYNC) |
		(t[17] & 0x80 ? MODE_FLAG_PVSYNC : MODE_FLAG_NVSYNC);
	m->type = MODE_TYPE_DRIVER;
	if (t[3] & 0x80)
		m->type |= MODE_TYPE_PREFERRED;
}

/* add_displayid_detailed_modes() and add_displayid_detailed_1_modes() */
static void edid_add_displayid_modes(void)
{
	struct did_iter it = { 0 };
	struct edid_mode m;
	const u8 *b;
	int i;

	while ((b = did_next(&it))) {
		bool type_7 = b[0] == DISPLAYID_2_TYPE_7_DETAILED;

		if ((b[0] != DISPLAYID_TYPE_1_DETAILED && !type_7) || b[2] % 20)
			continue;
		for (i = 0; i < b[2] / 20; i++) {
			displayid_detailed_mode(b + 3 + 20 * i, type_7, &m);
			mode_add(&m);
		}
	}
}

/* mode_in_range(): the range descriptor @t, with the 1.4 offsets */
static bool mode_in_range(const struct edid_mode *m, const u8 *t)
{
	bool rev4 = edid[19] >= 4;
	int hmin = t[7] + (rev4 && (t[4] & 0x04) ? 255 : 0);
	int hmax = t[8] + (rev4 && (t[4] & 0x08) ? 255 : 0);
	int vmin = t[5] + (rev4 && (t[4] & 0x01) ? 255 : 0);
	int vmax = t[6] + (rev4 && (t[4] & 0x02) ? 255 : 0);
	u32 max_clock;
	int sync;

	sync = mode_hsync(m);
	if (sync > hmax || sync < hmin)
		return false;
	sync = mode_vrefresh(m);
	if (sync > vmax || sync < vmin)
		return false;
	/* range_pixel_clock() */
	if (t[9] == 0 || t[9] == 255)
		max_clock = 0;
	else if (rev4 && t[10] == EDID_RANGE_CVT)
		max_clock = t[9] * 10000 - (t[12] >> 2) * 250;
	else
		max_clock = t[9] * 10000 + 5001;
	if (max_clock && m->clock > max_clock)
		return false;
	if (rev4 && t[10] == EDID_RANGE_CVT && t[13] &&
	    m->hdisplay > 8 * (t[13] + 256 * (t[12] & 3)))
		return false;
	return !mode_is_rb(m) || edid_rb;
}

/* valid_inferred_mode(): smaller than a probed mode, and not probed */
static bool valid_inferred_mode(const struct edid_mode *m)
{
	bool ok = false;
	int i;

	for (i = 0; i < modes_len; i++) {
		const struct edid_mode *p = &modes[i];

		if (m->hdisplay == p->hdisplay && m->vdisplay == p->vdisplay &&
		    mode_vrefresh(m) == mode_vrefresh(p))
			return false;
		if (m->hdisplay <= p->hdisplay && m->vdisplay <= p->vdisplay)
			ok = true;
	}
	return ok;
}

/* do_inferred_modes() with the drm_*_modes_for_range() helpers */
static void inferred_modes_cb(const u8 *t, void *closure)
{
	struct edid_mode m;
	int i;

	if (!is_display_descriptor(t, EDID_DETAIL_MONITOR_RANGE))
		return;
	for (i = 0; i < ARRAY_SIZE(dmt_modes); i++)
		if (mode_in_range(&dmt_modes[i], t) && valid_inferred_mode(&dmt_modes[i]))
			mode_add(&dmt_modes[i]);
	/* GTF is not defined before 1.2, CVT ranges before 1.4. */
	if (edid[19] < 2 || (t[10] == EDID_RANGE_CVT && edid[19] < 4))
		return;
	if (t[10] != EDID_RANGE_DEFAULT_GTF && t[10] != EDID_RANGE_SECONDARY_GTF &&
	    t[10] != EDID_RANGE_CVT)
		return;
	for (i = 0; i < ARRAY_SIZE(extra_modes); i++) {
		const struct minimode *e = &extra_modes[i];
		bool ok;

		if (t[10] == EDID_RANGE_SECONDARY_GTF)
			ok = gtf2_mode(&m, e->w, e->h, e->r);
		else if (t[10] == EDID_RANGE_DEFAULT_GTF)
			ok = gtf_mode(&m, e->w, e->h, e->r);
		else
			ok = cvt_mode(&m, e->w, e->h, e->r, edid_rb);
		if (!ok)
			return;
		/* drm_mode_fixup_1366x768() */
		if (m.hdisplay == 1368 && m.vdisplay == 768) {
			m.hdisplay = 1366;
			m.hsync_start--;
			m.hsync_end--;
		}
		if (mode_in_range(&m, t) && valid_inferred_mode(&m))
			mode_add(&m);
	}
}

/* add_inferred_modes() */
static void edid_add_inferred_modes(void)
{
	if (edid[19] >= 1)
		edid_for_each_detailed(inferred_modes_cb, NULL);
}

/* _drm_edid_connector_add_modes(), in the kernel's order */
static void edid_add_modes(void)
{
	edid_ranges_init();
	edid_add_detailed_modes();
	edid_add_cvt_modes();
	edid_add_standard_modes();
	edid_add_established_modes();
	edid_add_cea_modes();
	edid_add_alternate_cea_modes();
	edid_add_displayid_modes();
	if (edid[24] & EDID_FEATURE_CONTINUOUS)
		edid_add_inferred_modes();
}

/*
 * drm_add_modes_noedid(connector, 1024, 768): no mode is preferred, and
 * the get_modes count stays 0.
 */
static void noedid_modes_add(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dmt_modes); i++) {
		const struct edid_mode *m = &dmt_modes[i];

		if (m->hdisplay > 1024 || m->vdisplay > 768 || mode_vrefresh(m) > 61)
			continue;
		mode_store(m, 0);
	}
}

/*
 * drm_connector_list_update() on the first probe: a probed mode merges
 * into the first equal mode; a preferred one replaces a mode that is not.
 */
static void modes_merge(void)
{
	int i, j, n = 0;

	for (i = 0; i < modes_len; i++) {
		const struct edid_mode *p = &modes[i];

		for (j = 0; j < n; j++)
			if (mode_equal(p, &modes[j]))
				break;
		if (j == n) {
			if (n != i)
				modes[n] = *p;
			n++;
		} else if (!(modes[j].type & MODE_TYPE_PREFERRED) &&
			   (p->type & MODE_TYPE_PREFERRED)) {
			u8 type = p->type | modes[j].type;

			modes[j] = *p;
			modes[j].type = type;
		} else {
			modes[j].type |= p->type;
		}
	}
	modes_len = n;
}

/*
 * The core checks of __drm_helper_update_and_validate():
 * drm_mode_validate_basic(), _size(4096, 2304), _flag() with interlace
 * allowed, and _ycbcr420() with 4:2:0 allowed. The mode types and flags
 * built here are always valid ones.
 */
static bool mode_core_valid(const struct edid_mode *m)
{
	if (!m->clock || !m->hdisplay || m->hsync_start < m->hdisplay ||
	    m->hsync_end < m->hsync_start || m->htotal < m->hsync_end ||
	    !m->vdisplay || m->vsync_start < m->vdisplay ||
	    m->vsync_end < m->vsync_start || m->vtotal < m->vsync_end)
		return false;
	if (m->hdisplay > S7D_FB_MAX_WIDTH || m->vdisplay > S7D_FB_MAX_HEIGHT)
		return false;
	return !(m->flags & MODE_FLAG_DBLSCAN);
}

/* drm_mode_compare(): negative when @a comes first */
static int mode_compare(const struct edid_mode *a, const struct edid_mode *b)
{
	int diff;

	diff = !!(b->type & MODE_TYPE_PREFERRED) - !!(a->type & MODE_TYPE_PREFERRED);
	if (diff)
		return diff;
	diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
	if (diff)
		return diff;
	diff = mode_vrefresh(b) - mode_vrefresh(a);
	if (diff)
		return diff;
	return (int)b->clock - (int)a->clock;
}

/* drm_mode_prune_invalid() and drm_mode_sort(), a stable sort */
static void modes_prune_sort(void)
{
	struct edid_mode m;
	int i, j, n = 0;

	for (i = 0; i < modes_len; i++)
		if (mode_core_valid(&modes[i]))
			modes[n++] = modes[i];
	modes_len = n;
	for (i = 1; i < modes_len; i++) {
		m = modes[i];
		for (j = i; j > 0 && mode_compare(&modes[j - 1], &m) > 0; j--)
			modes[j] = modes[j - 1];
		modes[j] = m;
	}
}

/*
 * s7d_hdmi_output_build() and s7d_hdmi_link_calculate() for RGB 8-bit or
 * 4:2:0 8-bit: the TMDS rate in Hz is *num / *den.
 */
static bool link_rate(const struct edid_mode *m, bool y420, u64 *num, u64 *den)
{
	u64 pixel = (u64)mode_clock(m) * 1000;
	u64 serial, vco, scaled;
	u32 shift = 0;

	if (pixel < 25175000 || pixel > 594000000)
		return false;
	serial = (pixel * (y420 ? 20 : 40) + 2) / 4;
	/* s7d_hdmi_shared_calculate() with the 24 MHz reference */
	if (serial < 46875000 || serial >= 6000000000ULL)
		return false;
	for (vco = serial; vco < 3000000000ULL; vco *= 2)
		shift++;
	if (shift > 6)
		return false;
	scaled = ((vco << 17) + 6000000) / 12000000;
	if (scaled >= (500ULL << 17))
		scaled = (500ULL << 17) - 1;
	*num = scaled * 12000000;
	*den = (1ULL << (17 + shift)) * 10;
	return true;
}

/* s7d_hdmi_output_tmds_above() */
static bool tmds_above(u64 num, u64 den, u64 rate)
{
	return !den || num > rate * den;
}

/* s7d_hdmi_link_valid() */
static bool link_valid(const struct edid_mode *m, bool y420)
{
	u64 num, den;

	if (!link_rate(m, y420, &num, &den))
		return false;
	if (y420 && !sink.is_hdmi)
		return false;
	if (!sink.is_hdmi && tmds_above(num, den, 165000000))
		return false;
	if (tmds_above(num, den, 340000000) &&
	    (!sink.is_hdmi || !sink.scdc || !sink.scrambling))
		return false;
	if (sink.max_tmds_clock &&
	    tmds_above(num, den, (u64)sink.max_tmds_clock * 1000))
		return false;
	return true;
}

/* s7d_display_mode_valid() */
static bool s7d_display_mode_valid(const struct edid_mode *m)
{
	if (mode_clock(m) < 25175 || mode_clock(m) > 594000)
		return false;
	if (!m->hdisplay || m->hdisplay > 4096 || !m->htotal || m->htotal > 8192)
		return false;
	if (!m->vdisplay || m->vdisplay > 2160 || !m->vtotal || m->vtotal > 8192)
		return false;
	return mode_vrefresh(m) <= (m->vdisplay > 1080 ? 60 : 120);
}

/* s7d_encp_is_1080i() */
static bool s7d_encp_is_1080i(const struct edid_mode *m)
{
	return m->hdisplay == 1920 && m->vdisplay == 1080 && m->vtotal == 1125 &&
		(m->htotal == 2200 || m->htotal == 2640) &&
		(m->clock == 74250 || (m->htotal == 2200 && m->clock == 74176)) &&
		m->hsync_end - m->hsync_start == 44 && m->htotal - m->hsync_end == 148 &&
		m->flags == (MODE_FLAG_PHSYNC | MODE_FLAG_PVSYNC | MODE_FLAG_INTERLACE);
}

/* s7d_encp_is_enci() */
static bool s7d_encp_is_enci(const struct edid_mode *m)
{
	if (m->flags != (MODE_FLAG_NHSYNC | MODE_FLAG_NVSYNC |
			 MODE_FLAG_INTERLACE | MODE_FLAG_DBLCLK) ||
	    m->hdisplay != 720)
		return false;
	if (m->vdisplay == 480)
		return m->htotal == 858 && m->vtotal == 525 &&
			m->clock >= 13500 && m->clock <= 13514;
	return m->vdisplay == 576 && m->htotal == 864 && m->vtotal == 625 &&
		m->clock == 13500;
}

/* s7d_encp_check_mode(), the check of s7d_encp_build_state() */
static bool s7d_encp_mode_valid(const struct edid_mode *m)
{
	const u32 sync = MODE_FLAG_PHSYNC | MODE_FLAG_NHSYNC |
		MODE_FLAG_PVSYNC | MODE_FLAG_NVSYNC;

	if (m->flags & MODE_FLAG_INTERLACE)
		return (s7d_encp_is_1080i(m) || s7d_encp_is_enci(m)) &&
			s7d_display_mode_valid(m);
	if (m->flags & ~sync)
		return false;
	if ((m->flags & (MODE_FLAG_PHSYNC | MODE_FLAG_NHSYNC)) ==
	    (MODE_FLAG_PHSYNC | MODE_FLAG_NHSYNC) ||
	    (m->flags & (MODE_FLAG_PVSYNC | MODE_FLAG_NVSYNC)) ==
	    (MODE_FLAG_PVSYNC | MODE_FLAG_NVSYNC))
		return false;
	if (!s7d_display_mode_valid(m))
		return false;
	if (m->hdisplay >= m->hsync_start || m->hsync_start >= m->hsync_end ||
	    m->hsync_end >= m->htotal)
		return false;
	if (m->vdisplay >= m->vsync_start || m->vsync_start >= m->vsync_end ||
	    m->vsync_end >= m->vtotal)
		return false;
	return m->hsync_start - m->hdisplay >= 3 && m->vsync_start - m->vdisplay >= 2;
}

/* The S7D DRM checks of __drm_helper_update_and_validate() */
static bool mode_s7d_valid(const struct edid_mode *m)
{
	u32 vic;

	/* s7d_hdmi_mode_valid() */
	if (m->clock > 594000 || m->hdisplay > 4096 || m->vdisplay > 2160)
		return false;
	vic = cea_match(m, -1);
	if (vic_bit(sink.y420_vdb, vic)) {
		if (!link_valid(m, true))
			return false;
	} else if (!link_valid(m, false) &&
		   !(vic_bit(sink.y420_cmdb, vic) && link_valid(m, true))) {
		return false;
	}
	/* s7d_crtc_mode_valid() */
	if (!s7d_encp_mode_valid(m))
		return false;
	/*
	 * The custom slot is progressive. An interlaced mode runs on its VIC
	 * entry, and a 1080i mode without a VIC on the one of the 1080i
	 * encoder setup, as in s7d_encp_build_1080i().
	 */
	return !(m->flags & MODE_FLAG_INTERLACE) || vic || s7d_encp_is_1080i(m);
}

/*
 * drm_client_target_preferred() for one connector and no command line
 * mode: the first preferred mode, else the first mode, or with a tiled
 * display the tile rules. A non-desktop sink gets no mode
 * (drm_connector_enabled()). With @s7d only the modes the S7D DRM driver
 * accepts count, as on the board; without it the pick is the kernel's
 * from the list before those checks, to compare this code with the
 * kernel off the board.
 */
static const struct edid_mode *mode_pick(bool s7d)
{
	const struct edid_mode *first = NULL, *preferred = NULL;
	const struct edid_mode *tiled = NULL, *untiled = NULL;
	int i;

	if (sink.non_desktop)
		return NULL;
	for (i = 0; i < modes_len; i++) {
		const struct edid_mode *m = &modes[i];
		bool tile_size = m->hdisplay == tile.tile_h_size &&
			m->vdisplay == tile.tile_v_size;

		if (s7d && !mode_s7d_valid(m))
			continue;
		if (!first)
			first = m;
		if (!preferred && (m->type & MODE_TYPE_PREFERRED) &&
		    m->hdisplay <= S7D_FB_MAX_WIDTH && m->vdisplay <= S7D_FB_MAX_HEIGHT)
			preferred = m;
		if (tile_size && !tiled)
			tiled = m;
		if (!tile_size && !untiled)
			untiled = m;
	}
	if (!tile.has_tile)
		return preferred ? preferred : first;
	/* One tile connector: all tiles are present only for a 1x1 group. */
	if (tile.num_h_tile * tile.num_v_tile > 1 ||
	    (!tile.tile_h_loc && !tile.tile_v_loc && !tiled))
		return untiled;
	return tiled;
}

static void custom_video_clear(void)
{
	struct hdmi_timing *t = (struct hdmi_timing *)
		hdmitx21_gettiming_from_vic(HDMI_ODROID_CUSTOM_VIDEO);

	if (t) {
		memset(t, 0, sizeof(*t));
		t->vic = HDMI_ODROID_CUSTOM_VIDEO;
	}
}

/* A progressive mode outside the CEA table goes to the custom slot. */
static const struct hdmi_timing *custom_video_set(const struct edid_mode *m)
{
	struct hdmi_timing t = { 0 }, *slot;

	snprintf(custom_name, sizeof(custom_name), "%dx%dp%dhz",
		 m->hdisplay, m->vdisplay, mode_vrefresh(m));
	t.name = custom_name;
	t.pi_mode = 1;
	t.pixel_freq = m->clock;
	t.h_active = m->hdisplay;
	t.h_front = m->hsync_start - m->hdisplay;
	t.h_sync = m->hsync_end - m->hsync_start;
	t.h_back = m->htotal - m->hsync_end;
	t.h_blank = m->htotal - m->hdisplay;
	t.h_total = m->htotal;
	t.v_active = m->vdisplay;
	t.v_front = m->vsync_start - m->vdisplay;
	t.v_sync = m->vsync_end - m->vsync_start;
	t.v_back = m->vtotal - m->vsync_end;
	t.v_blank = m->vtotal - m->vdisplay;
	t.v_total = m->vtotal;
	t.h_pol = !!(m->flags & MODE_FLAG_PHSYNC);
	t.v_pol = !!(m->flags & MODE_FLAG_PVSYNC);
	/* h_pict/v_pict stay 0: a detailed timing has no picture aspect. */
	t.h_freq = DIV_ROUND_CLOSEST_ULL((u64)m->clock * 1000, m->htotal);
	t.v_freq = DIV_ROUND_CLOSEST_ULL((u64)m->clock * 1000000,
					 (u32)m->htotal * m->vtotal);
	t.v_sync_ln = 1;
	t.h_pixel = 1;
	t.v_pixel = 1;

	slot = (struct hdmi_timing *)
		hdmitx21_gettiming_from_vic(HDMI_ODROID_CUSTOM_VIDEO);
	if (!slot)
		return NULL;
	*slot = t;
	slot->vic = HDMI_ODROID_CUSTOM_VIDEO;
	return slot;
}

const u8 *s7d_hdmi_edid(int *blocks)
{
	*blocks = edid_blocks;
	return edid;
}

int s7d_hdmi_select_mode(struct hdmitx_dev *hdev, const u8 *panel_edid,
			 struct s7d_hdmi_mode *mode)
{
	const struct hdmi_timing *t;
	const struct edid_mode *best;
	struct edid_mode m;
	u32 timing_vic;
	u32 vic;

	if (!mode)
		return -EINVAL;
	memset(mode, 0, sizeof(*mode));

	cea_modes_init();
	memset(&sink, 0, sizeof(sink));
	memset(&tile, 0, sizeof(tile));
	modes_len = 0;
	modes_count = 0;
	modes_dropped = 0;
	edid_blocks = 0;

	/* drm_bridge_connector_get_modes_edid() */
	if (edid_read(hdev) && edid_valid()) {
		edid_blocks = min(edid_size, EDID_MAX_BLOCK);
		/* drm_edid_connector_update() */
		edid_parse_sink();
		edid_parse_tile();
		/* drm_edid_connector_add_modes() */
		edid_add_modes();
	} else if (panel_edid && edid_detailed_mode(panel_edid + 54, &m) &&
		   !(m.flags & MODE_FLAG_INTERLACE)) {
		/* The panel EDID of the bridge did not read back over DDC. */
		sink.is_hdmi = true;
		m.type |= MODE_TYPE_PREFERRED;
		mode_add(&m);
	}
	/* drm_helper_probe_single_connector_modes() with drm_add_modes_noedid() */
	if (!modes_count) {
		printf("display: no modes from the sink, DMT modes up to 1024x768\n");
		noedid_modes_add();
	}
	if (modes_dropped)
		printf("display: %d modes past %d dropped\n", modes_dropped,
		       EDID_MODES_MAX);
	modes_merge();
	modes_prune_sort();

	best = mode_pick(true);
	if (!best) {
		printf("display: %s\n", sink.non_desktop ?
		       "not a desktop display" : "no mode for this sink");
		return -EINVAL;
	}

	/* the VIC drm_match_cea_mode() gives */
	vic = cea_match(best, -1);
	/*
	 * s7d_encp_build_1080i() runs every 1080i mode on the VIC 5 or 20
	 * encoder setup and takes only the line length from the mode.
	 */
	timing_vic = vic;
	if (!vic && (best->flags & MODE_FLAG_INTERLACE))
		timing_vic = best->htotal == 2200 ? HDMI_5_1920x1080i60_16x9 :
			HDMI_20_1920x1080i50_16x9;
	if (timing_vic) {
		custom_video_clear();
		t = hdmitx21_gettiming_from_vic(timing_vic);
	} else {
		t = custom_video_set(best);
	}
	if (!t)
		return -EINVAL;

	mode->timing = t;
	mode->pixel_khz = mode_clock(best);
	mode->dvi = !sink.is_hdmi;
	mode->no_vic = !vic;
	/* __s7d_hdmi_select_output(): 4:2:0 only when RGB cannot be sent */
	mode->yuv420 = vic_bit(sink.y420_vdb, vic) ||
		(vic_bit(sink.y420_cmdb, vic) && !link_valid(best, false));

	printf("display: %s, %u kHz, %s, %s\n",
	       !t->pi_mode && t->sname ? t->sname : t->name, mode->pixel_khz,
	       mode->dvi ? "DVI" : "HDMI", mode->yuv420 ? "YCbCr 4:2:0" : "RGB");
	return 0;
}
