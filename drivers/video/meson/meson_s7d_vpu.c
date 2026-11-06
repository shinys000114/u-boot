// SPDX-License-Identifier: GPL-2.0+
/*
 * ODROID-C5 boot display: a 1920x1080 XRGB frame buffer on OSD1 of the S7D
 * VPU, scaled to the HDMI output that the sink's EDID gives, the way the
 * Linux S7D DRM driver would show its console.
 */

#include <common.h>
#include <div64.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <video_bridge.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include <amlogic/fb.h>
#include <amlogic/media/vout/aml_vout.h>
#include <amlogic/media/vout/hdmitx21/hdmitx.h>
#include <amlogic/media/vout/hdmitx_common/hdmitx_edid.h>
#include <amlogic/media/vpp/vpp.h>

#include "meson_s7d.h"

/* The frame buffer has one size for every output; OSD1 scales it. */
#define S7D_FB_WIDTH		1920
#define S7D_FB_HEIGHT		1080
#define S7D_FB_LINE		(S7D_FB_WIDTH * 4)
#define S7D_FB_SIZE		(S7D_FB_LINE * S7D_FB_HEIGHT)

/* do_hpd_detect(): some sinks raise HPD up to 1.5 s after the 5 V */
#define S7D_HPD_TRIES		16
#define S7D_HPD_WAIT_MS		100

struct s7d_vpu_priv {
	struct s7d_hdmi_mode mode;
	/* OSD1 scans the frame buffer */
	bool osd;
};

/* drm_helper_probe_detect(), with the HPD wait of the hdmitx command */
static bool s7d_vpu_detect(struct hdmitx_dev *hdev)
{
	int i;

	for (i = 0; i < S7D_HPD_TRIES; i++) {
		if (i)
			mdelay(S7D_HPD_WAIT_MS);
		hdev->hpd_state = hdev->hwop.get_hpd_state();
		if (hdev->hpd_state)
			return true;
	}
	return false;
}

/*
 * A bridge in front of the HDMI connector, the LT8619C of a VU7C panel,
 * which the board binds only for that panel. Starting it writes the panel
 * EDID into it; return that EDID for when it does not read back.
 */
static const u8 *s7d_vpu_bridge_edid(u8 *buf, int size)
{
	struct udevice *bridge;
	int ret;

	ret = uclass_first_device_err(UCLASS_VIDEO_BRIDGE, &bridge);
	if (ret) {
		if (ret != -ENODEV)
			printf("display: bridge did not start (%d)\n", ret);
		return NULL;
	}
	if (video_bridge_attach(bridge)) {
		printf("display: %s did not attach\n", bridge->name);
		return NULL;
	}
	if (video_bridge_read_edid(bridge, buf, size) < EDID_BLK_SIZE)
		return NULL;
	return buf;
}

/*
 * The capabilities that the transmitter driver keeps for its output
 * (hdev->RXCap), from the EDID that the mode was chosen from, as "hdmitx
 * get_parse_edid" parses them.
 */
static void s7d_vpu_parse_rxcap(struct hdmitx_dev *hdev)
{
	const u8 *edid;
	int blocks;

	edid = s7d_hdmi_edid(&blocks);
	memset(hdev->rawedid, 0, sizeof(hdev->rawedid));
	memcpy(hdev->rawedid, edid,
	       min_t(size_t, blocks * EDID_BLK_SIZE, sizeof(hdev->rawedid)));
	hdmitx_edid_parse(&hdev->RXCap, hdev->rawedid);
	hdmitx_cec_phy_addr_parse(&hdev->RXCap, hdev->rawedid);
	hdmitx_audio_parse(&hdev->RXCap, hdev->rawedid);
	memcpy(&hdev->tx_common.rxcap, &hdev->RXCap,
	       sizeof(hdev->tx_common.rxcap));
}

/* 480i and 576i run on ENCI, every other mode on ENCP */
static bool s7d_vpu_enci(const struct hdmi_timing *t)
{
	return !t->pi_mode && (t->h_active >> t->pixel_repetition_factor) == 720 &&
	       (t->v_active == 480 || t->v_active == 576);
}

/*
 * The frame buffer fills the output. On an output whose picture is not
 * 16:9, the window is the largest centred rectangle that shows it at 16:9,
 * so it keeps its aspect and the rest stays black. A CEA format has the
 * picture aspect of its VIC (720x480 is 4:3 or 16:9), any other output
 * square pixels. Outputs within a line of 16:9, like 1366x768, keep the
 * whole output.
 */
static void s7d_vpu_window(const struct hdmi_timing *t,
			   const struct vinfo_s *vinfo, int *win)
{
	u32 aw, ah, width = vinfo->width, height = vinfo->height;

	if (t->h_pict && t->v_pict) {
		aw = t->h_pict;
		ah = t->v_pict;
	} else {
		aw = vinfo->width;
		ah = vinfo->height;
	}
	if ((u64)aw * S7D_FB_HEIGHT <= (u64)ah * S7D_FB_WIDTH)
		height = DIV_ROUND_CLOSEST_ULL((u64)vinfo->height * aw *
					       S7D_FB_HEIGHT,
					       ah * S7D_FB_WIDTH);
	else
		width = DIV_ROUND_CLOSEST_ULL((u64)vinfo->width * ah *
					      S7D_FB_WIDTH,
					      aw * S7D_FB_HEIGHT);
	if (width + 1 >= vinfo->width && height + 1 >= vinfo->height) {
		width = vinfo->width;
		height = vinfo->height;
	} else {
		printf("display: %ux%u window for the 16:9 frame buffer\n",
		       width, height);
	}
	win[0] = (vinfo->width - width) / 2;
	win[1] = (vinfo->height - height) / 2;
	win[2] = win[0] + width - 1;
	win[3] = win[1] + height - 1;
}

/*
 * Start the output: OSD1 and its scaler, the VIU to the encoder and the
 * transmitter, in the order "osd open", "bmp scale" and "vout output" set
 * them up.
 */
static int s7d_vpu_start(struct udevice *dev, ulong fb)
{
	struct s7d_vpu_priv *priv = dev_get_priv(dev);
	const struct hdmi_timing *t = priv->mode.timing;
	struct vinfo_s *vinfo = vout_get_current_vinfo();
	struct hdmitx21_output out = {
		.timing = t,
		.pixel_khz = priv->mode.pixel_khz,
		.cs = priv->mode.yuv420 ? HDMI_COLORSPACE_YUV420 :
			HDMI_COLORSPACE_RGB,
		.dvi = priv->mode.dvi,
		.no_vic = priv->mode.no_vic,
	};
	bool enci = s7d_vpu_enci(t);
	u32 fmt;
	int win[4];
	int ret;

	/* vout_vmode_init(): the output as the VPP and OSD scan it */
	vinfo->width = t->h_active >> t->pixel_repetition_factor;
	vinfo->height = t->v_active;
	vinfo->field_height = t->pi_mode ? t->v_active : t->v_active / 2;
	vinfo->vd_base = (void *)fb;
	vinfo->vl_bpix = 32;

	/*
	 * The kernel takes a buffer over only on ENCP, so 480i and 576i show
	 * no frame buffer and OSD1 stays off.
	 */
	priv->osd = !enci;
	if (priv->osd) {
		s7d_vpu_window(t, vinfo, win);
		ret = osd_show_fb(fb, S7D_FB_WIDTH, S7D_FB_HEIGHT,
				  win[0], win[1], win[2], win[3]);
		if (ret) {
			printf("display: OSD1 not set up (%d)\n", ret);
			priv->osd = false;
		}
	}

	/* do_vout_output_job(): the matrix of the last output's format */
	fmt = vinfo->vpp_post_out_color_fmt;
	vout_viu_mux(VOUT_VIU1_SEL, enci ? VIU_MUX_ENCI : VIU_MUX_ENCP);
	vpp_matrix_update(fmt == 1 ? VPP_CM_RGB : VPP_CM_YUV);

	ret = hdmitx21_set_output(&out);
	if (ret)
		goto err;
	if (!enci && !(hd21_read_reg(ENCP_VIDEO_EN) & BIT(0))) {
		printf("display: ENCP does not run\n");
		ret = -EIO;
		goto err;
	}
	return 0;

err:
	/*
	 * Turn OSD1 off. Left on, the kernel finds a fetch that it cannot
	 * stop cleanly once no encoder runs.
	 */
	if (priv->osd)
		osd_enable_hw(OSD1, 0);
	priv->osd = false;
	return ret;
}

static int s7d_vpu_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct s7d_vpu_priv *priv = dev_get_priv(dev);
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	u8 panel_buf[EDID_BLK_SIZE * 2];
	const u8 *panel;
	int ret;

	panel = s7d_vpu_bridge_edid(panel_buf, sizeof(panel_buf));
	if (!s7d_vpu_detect(hdev)) {
		printf("display: no HDMI sink\n");
		return -ENODEV;
	}
	ret = s7d_hdmi_select_mode(hdev, panel, &priv->mode);
	if (ret)
		return ret;
	s7d_vpu_parse_rxcap(hdev);

	ret = s7d_vpu_start(dev, plat->base);
	if (ret)
		return ret;

	uc_priv->xsize = S7D_FB_WIDTH;
	uc_priv->ysize = S7D_FB_HEIGHT;
	uc_priv->bpix = VIDEO_BPP32;
	uc_priv->line_length = S7D_FB_LINE;
	/* OSD1 reads the buffer from DRAM */
	uc_priv->flush_dcache = true;
	return 0;
}

/*
 * The board removes the device when it shows no logo: OSD1 goes off and
 * the output runs on, black. Nothing calls this at boot (bootm removes
 * only devices flagged for it), so the logo stays up for the kernel.
 */
static int s7d_vpu_remove(struct udevice *dev)
{
	struct s7d_vpu_priv *priv = dev_get_priv(dev);

	if (priv->osd)
		osd_enable_hw(OSD1, 0);
	priv->osd = false;
	return 0;
}

/*
 * The frame buffer is the memory-region that the kernel DT reserves for
 * the boot logo, so that the kernel can take it over.
 */
static int s7d_vpu_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct ofnode_phandle_args mem;
	fdt_size_t size;
	fdt_addr_t base;

	if (dev_read_phandle_with_args(dev, "memory-region", NULL, 0, 0, &mem))
		return -EINVAL;
	base = ofnode_get_addr_size_index(mem.node, 0, &size);
	if (base == FDT_ADDR_T_NONE || size < S7D_FB_SIZE)
		return -EINVAL;
	plat->base = base;
	plat->size = S7D_FB_SIZE;
	/* The board draws its own logo. */
	plat->hide_logo = true;
	return 0;
}

static const struct udevice_id s7d_vpu_ids[] = {
	{ .compatible = "amlogic,s7d-vpu" },
	{ }
};

U_BOOT_DRIVER(s7d_vpu) = {
	.name		= "s7d_vpu",
	.id		= UCLASS_VIDEO,
	.of_match	= s7d_vpu_ids,
	.bind		= s7d_vpu_bind,
	.probe		= s7d_vpu_probe,
	.remove		= s7d_vpu_remove,
	.priv_auto	= sizeof(struct s7d_vpu_priv),
};
