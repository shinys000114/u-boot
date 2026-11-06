// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <common.h>
#include <asm/io.h>
#include <malloc.h>
#include <errno.h>
#include <env.h>
#include <fdt_support.h>
#include <gzip.h>
#include <mapmem.h>
#include <splash.h>
#include <usb.h>
#include <video.h>
#include <bmp_layout.h>
#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/libfdt.h>
#include <amlogic/cpu_id.h>
#include <asm/amlogic/arch/secure_apb.h>
#include <asm/amlogic/arch/pinctrl_init.h>
#include <linux/sizes.h>
#include <asm-generic/gpio.h>
#include <dm.h>
#include <amlogic/aml_v3_burning.h>
#include <amlogic/aml_v2_burning.h>
#include <linux/mtd/partitions.h>
#include <asm/amlogic/arch/bl31_apis.h>
#include <amlogic/aml_mtd.h>
#include <amlogic/board.h>
#include <asm-generic/u-boot.h>
#include <command.h>
#include <asm/amlogic/arch/usb.h>
#include <asm/amlogic/arch/romboot.h>
#include <asm/amlogic/arch/stick_mem.h>
#include <fs.h>
#include <blk.h>
#include <mmc.h>

#include "dm/device-internal.h"
#include "dm/lists.h"
#include "dm/root.h"
#include "dm/uclass-internal.h"

#ifdef CONFIG_AML_VPU
#include <amlogic/media/vpu/vpu.h>
#endif
#ifdef CONFIG_AML_VPP
#include <amlogic/media/vpp/vpp.h>
#endif
#ifdef CONFIG_AML_HDMITX20
#include <amlogic/media/vout/hdmitx/hdmitx_module.h>
#endif
#ifdef CONFIG_AML_HDMITX21
#include <amlogic/media/vout/hdmitx21/hdmitx_module.h>
#include <amlogic/media/vout/hdmitx21/hdmi_common.h>
#endif
#ifdef CONFIG_AMLOGIC_AMFC
#include <amlogic/amfc.h>
#endif
#include <asm/amlogic/arch/efuse.h>

DECLARE_GLOBAL_DATA_PTR;
extern int cc_statue, bc_status;

int mmc_get_env_dev(void);
struct blk_desc *amlogic_get_bootdev(void);

/*
 * Internal function in 'drivers/amlogic/media/vout/hdmitx/hdmitx21/hdmi_param.c'
 */
extern const struct hdmi_timing *hdmitx21_gettiming_from_name(const char *name);

/*
 * Display state that older builds wrote and saved to the environment on
 * every boot, including the display keys they imported from config.ini,
 * and the user policy and debug keys of the vendor output. This U-Boot
 * keeps its display state in its video driver and saves none; a saved
 * copy would steer the vendor output code and reach the kernel through
 * the boot script.
 */
static const char *const odroid_stale_display_env[] = {
	/* kernel command line hand-off */
	"vout", "hdmitx", "vout_init", "voutmode", "actual_frac_rate",
	/* output policy state */
	"outputmode", "hdmimode", "colorattribute", "user_colorattribute",
	"hdmichecksum", "frac_rate_policy", "user_frac_rate", "qms_en",
	"is.bestmode",
	/* HPD debug key that "hdmitx hpd" reads */
	"hdmitx_hpd_bypass",
	/* USB hub power after the output, for force_hpd display overlays */
	"early_usb_start",
	/* logo framebuffer */
	"display_bpp", "display_color_index", "display_width", "display_height",
	"fb_width", "fb_height", "initrd_high", "bootlogo_addr",
	/* config.ini display keys */
	"displaymode", "displaymode_option", "video-timing", "disablehpd",
	"force_hpd", "enable_hdr", "bootlogo",
};

/* Drop them from RAM only; the saved environment stays as it is. */
static void odroid_drop_stale_display_env(void)
{
	const char *preboot = env_get("preboot");
	int i;

	for (i = 0; i < ARRAY_SIZE(odroid_stale_display_env); i++)
		env_set(odroid_stale_display_env[i], NULL);
	/* The default of older builds, which built the vendor bootargs. */
	if (preboot && !strcmp(preboot, "run init_display"))
		env_set("preboot", NULL);
	env_set("connector0_type", "HDMI-A-A");
}

/* The overlays list of config.ini. */
static char odroid_overlays[256];

/* Whether @word is one of the blank-separated words of @list. */
static bool odroid_list_has(const char *list, const char *word)
{
	size_t len = strlen(word), n;

	for (;;) {
		while (isspace(*list))
			list++;
		if (!*list)
			return false;
		for (n = 0; list[n] && !isspace(list[n]); n++)
			;
		if (n == len && !strncmp(list, word, len))
			return true;
		list += n;
	}
}

void sys_led_init(void)
{
	run_command("gpio set GPIODV_5", 0);
	run_command("gpio clear GPIODV_6", 0);
}

int serial_set_pin_port(unsigned long port_base)
{
	return 0;
}

int dram_init(void)
{
	gd->ram_size = (readl(SYSCTRL_SEC_STATUS_REG4) & ~0xffffUL) << 4;
	return 0;
}

/* secondary_boot_func
 * this function should be write with asm, here, is only for compiling pass
 */
void secondary_boot_func(void)
{
}

int board_eth_init(bd_t *bis)
{
	return 0;
}

#ifdef CONFIG_AML_HDMITX20
static void hdmitx_set_hdmi_5v(void)
{
	/*Power on VCC_5V for HDMI_5V */
}
#endif

void board_init_mem(void)
{
	/* config bootm low size, make sure whole dram/psram space can be used */
	phys_size_t ram_size;
	char *env_tmp;

	env_tmp = env_get("bootm_size");
	if (!env_tmp) {
		ram_size =
		    ((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4) >
		    0xe0000000 ? 0xe0000000 : ((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4);
		env_set_hex("bootm_low", 0);
		env_set_hex("bootm_size", ram_size);
	}
}

int eth_get_efuse_mac(struct udevice *dev) {
	char buf[32], str[37];
	loff_t offset = 0;
	int ret, i, n = 0;

	memset(buf, 0, sizeof(buf));
	ret = efuse_read_usr(buf, 32, (loff_t *)&offset);
	if (ret != 32) {
		printf("ERROR: efuse read user data fail!\n");
		return -1;
	}
	for (i = 0; i < 16; ++i) {
		if (buf[i + 16] != 0) {
			n = 16;
			break;
		}
	}

	sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		buf[n + 0], buf[n + 1], buf[n + 2], buf[n + 3],
		buf[n + 4], buf[n + 5], buf[n + 6], buf[n + 7],
		buf[n + 8], buf[n + 9], buf[n + 10], buf[n + 11],
		buf[n + 12], buf[n + 13], buf[n + 14], buf[n + 15]
	);
	printf("board id: %s\n", str);
	env_set("serial#", str);

	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
		buf[n + 10], buf[n + 11], buf[n + 12], buf[n + 13], buf[n + 14], buf[n + 15]
	);
	printf("mac address: %s\n", str);
	env_set("ethaddr", str);

	return 0;
}

#if defined(CONFIG_VIDEO_BRIDGE_LT8619C_QC4011)

static struct udevice *vu7c_hw_init(void)
{
	struct udevice *soc_dev, *i2c_dev, *bridge_dev;
	ofnode i2c_node, bridge_node;
	int ret;

	i2c_node = ofnode_path("/soc/i2c@fe066c00");
	if (!ofnode_valid(i2c_node)) {
		printf("vu7c: i2c3 node not found\n");
		return NULL;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_SIMPLE_BUS, ofnode_get_parent(i2c_node), &soc_dev);
	if (ret) {
		printf("vu7c: failed to get soc device (ret=%d)\n", ret);
		return NULL;
	}

	if (uclass_find_device_by_ofnode(UCLASS_I2C, i2c_node, &i2c_dev)) {
		ret = device_bind_driver_to_node(soc_dev, "i2c_meson", "i2c@fe066c00",
						 i2c_node, &i2c_dev);
		if (ret) {
			printf("vu7c: failed to bind i2c_meson (ret=%d)\n", ret);
			return NULL;
		}
	}

	if (i2c_dev && !i2c_dev->driver_data) {
		const struct udevice_id *id = i2c_dev->driver->of_match;
		while (id && id->compatible) {
			if (ofnode_device_is_compatible(i2c_node, id->compatible)) {
				i2c_dev->driver_data = id->data;
				break;
			}
			id++;
		}
	}

	bridge_node = ofnode_find_subnode(i2c_node, "hdmi-bridge@32");
	if (!ofnode_valid(bridge_node)) {
		printf("vu7c: bridge node not found\n");
		return NULL;
	}

	ret = device_bind_driver_to_node(i2c_dev, "lt8619c", "hdmi-bridge@32",
					 bridge_node, &bridge_dev);
	if (ret && ret != -EEXIST) {
		printf("vu7c: failed to bind lt8619c (ret=%d)\n", ret);
		return NULL;
	}

	return bridge_dev;
}

/*
 * A VU7C is attached when the overlays list names display_vu7c. Bind its
 * LT8619C bridge; the video driver starts it, which writes the panel EDID
 * into the bridge for the mode selection to read like any sink.
 */
static struct udevice *vu7c_init(void)
{
	if (!odroid_list_has(odroid_overlays, "display_vu7c"))
		return NULL;
	return vu7c_hw_init();
}
#endif

int board_init(void)
{
	printf("board init\n");

	sys_led_init();

#ifdef CONFIG_AML_HDMITX21
	hdmitx21_chip_type_init(MESON_CPU_ID_S7D);
	hdmitx21_init();
#endif

#if !defined(CONFIG_PXP_DDR)	//bypass below operations for pxp
	aml_set_bootsequence(0);

	run_command("gpio set GPIOH_7", 0);
#ifdef CONFIG_AML_HDMITX20
	hdmitx_set_hdmi_5v();
	hdmitx_init();
#endif
#endif // #if !defined(CONFIG_PXP_DDR) //bypass below operations for pxp

	pinctrl_devices_active(PIN_CONTROLLER_NUM);
#ifdef CONFIG_AMLOGIC_AMFC
	amfc_init();
#endif
	return 0;
}

int load_from_mmc(unsigned long addr, int devnum, int partnum, const char *filename)
{
	int ret;
	char buf[16];

	snprintf(buf, sizeof(buf), "%d:%d", devnum, partnum);

	ret = fs_set_blk_dev("mmc", buf, FS_TYPE_ANY);
	if (!ret) {
		loff_t len_read;
		ret = fs_read(filename, addr, 0, 0, &len_read);
		if (!ret) {
			env_set_ulong("filesize", len_read);
			printf("%llu bytes read\n", len_read);
			return 0;
		}
	}

	return ret;
}

int load_odroid_bios(struct blk_desc *dev_desc)
{
	const char *bootcmd = "cramfsload $loadaddr boot.scr; source $loadaddr";
	unsigned long addr;
	int n;
	int ret;

	if (!dev_desc)
		return -EIO;

	addr = env_get_hex("cramfsaddr", 0);
	if (!addr) {
		addr = 0x38000000;
		env_set_hex("cramfsaddr", addr);
	}

	/* Clear memory at $crarmfsaddr */
	memset((void*)addr, 0, 256);

	for (n = 1; n <= 3; n++) {
		ret = load_from_mmc(addr, dev_desc->devnum, n, "ODROIDBIOS.BIN");
		if (!ret) {
			env_set_hex("devnum", dev_desc->devnum);
			env_set("bootcmd", bootcmd);
			return 0;
		}
	}

	return -EIO;
}

/* As find_char_or_comment() in cmd/ini.c: a ';' after a blank starts a comment. */
static char *odroid_ini_find(char *s, char c)
{
	bool blank = false;

	while (*s && *s != c && !(blank && *s == ';')) {
		blank = isspace(*s);
		s++;
	}
	return s;
}

static char *odroid_ini_strip(char *s)
{
	char *e = s + strlen(s);

	while (e > s && isspace(e[-1]))
		*--e = '\0';
	while (isspace(*s))
		s++;
	return s;
}

/*
 * Copy the value of @key in [@section] of the INI text @buf into @val,
 * parsed as the "ini" command does (cmd/ini.c): ';' and '#' lines are
 * comments, pairs are name=value or name:value, a ';' after a blank ends
 * the value and one pair of double quotes is stripped. The last match
 * wins. Returns false if there is none.
 */
static bool odroid_ini_get(const char *buf, size_t size, const char *section,
			   const char *key, char *val, size_t len)
{
	const char *end = buf + size;
	bool in_section = false, found = false;
	char line[256];

	while (buf < end) {
		const char *nl = memchr(buf, '\n', end - buf);
		size_t n = (nl ? nl : end) - buf;
		char *s, *e, *v;

		memcpy(line, buf, min(n, sizeof(line) - 1));
		line[min(n, sizeof(line) - 1)] = '\0';
		buf = nl ? nl + 1 : end;

		s = odroid_ini_strip(line);
		if (*s == ';' || *s == '#' || !*s)
			continue;
		if (*s == '[') {
			e = odroid_ini_find(s + 1, ']');
			if (*e == ']') {
				*e = '\0';
				in_section = !strcmp(s + 1, section);
			}
			continue;
		}
		e = odroid_ini_find(s, '=');
		if (*e != '=')
			e = odroid_ini_find(s, ':');
		if (!in_section || (*e != '=' && *e != ':'))
			continue;
		*e = '\0';
		if (strcmp(odroid_ini_strip(s), key))
			continue;
		v = odroid_ini_strip(e + 1);
		e = odroid_ini_find(v, '\0');
		*e = '\0';
		v = odroid_ini_strip(v);
		n = strlen(v);
		if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
			v[n - 1] = '\0';
			v++;
		}
		strlcpy(val, v, len);
		found = true;
	}
	return found;
}

/*
 * @key of config.ini as the "ini generic" and "ini overlay_<profile>"
 * imports set it: from [generic], then from @section, the profile
 * section, when there is one.
 */
static bool odroid_ini_key(const char *buf, size_t size, const char *section,
			   const char *key, char *val, size_t len)
{
	bool found = odroid_ini_get(buf, size, "generic", key, val, len);

	if (*section)
		found |= odroid_ini_get(buf, size, section, key, val, len);
	return found;
}

/*
 * Read two keys from config.ini: the overlays list, and bootlogo, the
 * logo file that the boot display loads first. Only bootlogo goes into the
 * environment, in RAM; no other key of the file does: the boot script
 * imports config.ini itself.
 */
static void load_boot_config(struct blk_desc *dev_desc, const char *config)
{
	ulong addr = env_get_ulong("loadaddr", 16, 0);
	const char *buf = (const char *)addr;
	char profile[64], section[80] = "", bootlogo[128];
	ulong size;

	odroid_overlays[0] = '\0';
	if (load_from_mmc(addr, dev_desc->devnum, 1, config) &&
	    load_from_mmc(addr, dev_desc->devnum, 2, config))
		return;
	/* load_from_mmc() sets filesize in decimal. */
	size = env_get_ulong("filesize", 10, 0);

	if (odroid_ini_get(buf, size, "generic", "overlay_profile", profile,
			   sizeof(profile)) && *profile)
		snprintf(section, sizeof(section), "overlay_%s", profile);
	odroid_ini_key(buf, size, section, "overlays", odroid_overlays,
		       sizeof(odroid_overlays));
	printf("config.ini: overlays \"%s\"\n", odroid_overlays);
	if (odroid_ini_key(buf, size, section, "bootlogo", bootlogo,
			   sizeof(bootlogo))) {
		env_set("bootlogo", bootlogo);
		printf("config.ini: bootlogo \"%s\"\n", bootlogo);
	}
}

/* Where the BMP file is loaded, and the logo buffer size it must fit */
#define ODROID_BMP_ADDR		0x1f800000UL
#define ODROID_LOGO_WIDTH	1920
#define ODROID_LOGO_HEIGHT	1080

/*
 * Load the logo file into @addr: the bootlogo file of config.ini, then
 * boot-logo.bmp.gz and boot-logo.bmp, each from the first and then the
 * second partition of the boot device.
 */
static bool odroid_load_logo(ulong addr)
{
	const char *files[] = { env_get("bootlogo"), "boot-logo.bmp.gz",
				"boot-logo.bmp" };
	struct blk_desc *dev_desc = amlogic_get_bootdev();
	int i;

	if (!dev_desc)
		return false;
	for (i = 0; i < ARRAY_SIZE(files); i++) {
		if (!files[i])
			continue;
		if (!load_from_mmc(addr, dev_desc->devnum, 1, files[i]) ||
		    !load_from_mmc(addr, dev_desc->devnum, 2, files[i]))
			return true;
	}
	return false;
}

/*
 * The BMP at @addr, inflated into a new buffer of CONFIG_VIDEO_LOGO_MAX_SIZE
 * that *@alloc points to when it is a gzip file. NULL when it is neither a
 * BMP nor a gzip BMP that fits.
 */
static struct bmp_image *odroid_logo_bmp(ulong addr, void **alloc)
{
	struct bmp_image *bmp = (struct bmp_image *)addr;
	unsigned long len = CONFIG_VIDEO_LOGO_MAX_SIZE;
	void *dst;

	*alloc = NULL;
	if (bmp->header.signature[0] == 'B' && bmp->header.signature[1] == 'M')
		return bmp;
	dst = malloc(CONFIG_VIDEO_LOGO_MAX_SIZE);
	if (!dst)
		return NULL;
	if (gunzip(dst, CONFIG_VIDEO_LOGO_MAX_SIZE, (uchar *)addr, &len) ||
	    len < sizeof(bmp->header)) {
		free(dst);
		return NULL;
	}
	bmp = dst;
	if (bmp->header.signature[0] != 'B' || bmp->header.signature[1] != 'M') {
		free(dst);
		return NULL;
	}
	*alloc = dst;
	return bmp;
}

/*
 * Draw the logo into the frame buffer of @dev, centred. A BMP larger than
 * the 1920x1080 buffer is not shown.
 */
static bool odroid_draw_logo(struct udevice *dev)
{
	struct bmp_image *bmp;
	void *alloc;
	s32 width, height;
	bool shown = false;

	if (!odroid_load_logo(ODROID_BMP_ADDR)) {
		printf("logo: no logo file\n");
		return false;
	}
	bmp = odroid_logo_bmp(ODROID_BMP_ADDR, &alloc);
	if (!bmp) {
		printf("logo: not a BMP\n");
		return false;
	}
	width = get_unaligned_le32(&bmp->header.width);
	height = get_unaligned_le32(&bmp->header.height);
	if (height < 0)
		height = -height;
	if (width <= 0 || !height || width > ODROID_LOGO_WIDTH ||
	    height > ODROID_LOGO_HEIGHT)
		printf("logo: %dx%d BMP does not fit %dx%d, no logo\n",
		       width, height, ODROID_LOGO_WIDTH, ODROID_LOGO_HEIGHT);
	else
		shown = !video_bmp_display(dev, map_to_sysmem(bmp),
					   BMP_ALIGN_CENTER, BMP_ALIGN_CENTER,
					   true);
	free(alloc);
	if (shown)
		video_sync(dev, true);
	return shown;
}

/*
 * The boot display. A VU5, VU7 or VU7+ panel runs from a port of the USB
 * hub and raises HPD and answers EDID reads only once powered, so power
 * the hub ports first (the Type-A hub on crg2, USB bus 1); nothing on them
 * is enumerated. A VU7C gets its LT8619C bridge bound. The video driver
 * then starts the HDMI output with OSD1 on its frame buffer, and the logo
 * goes into it. Without a logo, the device goes away again: OSD1 turns off
 * and the output runs on, black.
 */
static void odroid_boot_display(void)
{
	struct udevice *dev, *bridge = NULL;
	struct gpio_desc backlight;
	int ret;

	printf("starting USB hub VBUS on bus 1...\n");
	if (usb_hub_on(1))
		printf("usb: hub on bus 1 not powered\n");
#if defined(CONFIG_VIDEO_BRIDGE_LT8619C_QC4011)
	bridge = vu7c_init();
#endif
	ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
	if (ret) {
		printf("display: not started (%d)\n", ret);
		return;
	}

	/* VU7C backlight on */
	if (bridge && device_active(bridge) &&
	    !dm_gpio_lookup_name("GPIOX_6", &backlight) &&
	    !dm_gpio_request(&backlight, "vu7c_backlight")) {
		dm_gpio_set_dir_flags(&backlight, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		dm_gpio_free(backlight.dev, &backlight);
	}

	if (!odroid_draw_logo(dev))
		device_remove(dev, DM_REMOVE_NORMAL);
}

int board_late_init(void)
{
	unsigned char chipid[16];
	struct blk_desc *dev_desc = amlogic_get_bootdev();

	board_init_mem();

	get_stick_reboot_flag_mbx();

#ifdef CONFIG_AML_VPU
	vpu_probe();
#endif
#ifdef CONFIG_AML_VPP
	vpp_init();
#endif

	odroid_drop_stale_display_env();

	if (dev_desc) {
		if (load_odroid_bios(dev_desc) != 0) {
			env_set("mmc_list",
					(dev_desc->devnum == 0) ? "0 1" : "1 0");
		}

		load_boot_config(dev_desc, "config.ini");
	}

	odroid_boot_display();

	memset(chipid, 0, 16);
	env_set("cpu_id", "1234567890");
	if (get_chip_id(chipid, 16) != -1) {
		char chipid_str[32];
		int i, j;
		char buf_tmp[4];

		memset(chipid_str, 0, 32);

		char *buff = &chipid_str[0];

		for (i = 0, j = 0; i < 12; ++i) {
			sprintf(&buf_tmp[0], "%02x", chipid[15 - i]);
			if (strcmp(buf_tmp, "00") != 0) {
				sprintf(buff + j, "%02x", chipid[15 - i]);
				j = j + 2;
			}
		}
		env_set("cpu_id", chipid_str);
		printf("buff: %s\n", buff);
	}

	return 0;
}

phys_size_t get_effective_memsize(void)
{
	// >>16 -> MB, <<20 -> real size, so >>16<<20 = <<4
#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	return ((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4) > 0xe0000000 ? 0xe0000000 :
	    (((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4) - CONFIG_SYS_MEM_TOP_HIDE);
#else
	return ((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4) > 0xe0000000 ? 0xe0000000 :
	    ((readl(SYSCTRL_SEC_STATUS_REG4) & 0xFFF00000) << 4);
#endif /* CONFIG_SYS_MEM_TOP_HIDE */
}

int mach_cpu_init(void)
{
	//printf("\nmach_cpu_init\n");
	return 0;
}

static int odroid_fixup_display(void *blob, const char *default_mode)
{
	const char *mode;
	const char *property = "fbdev_sizes";
	int bits_per_pixel;
	int err, nodeoffset;
	const struct hdmi_timing *timing;

	err = fdt_check_header(blob);
	if (err < 0)
		return err;

	nodeoffset = fdt_find_or_add_subnode(blob, 0, "drm-subsystem");
	if (nodeoffset < 0)
		return nodeoffset;

	mode = default_mode ? : env_get("outputmode");
	if (!mode)
		return -EINVAL;

	timing = hdmitx21_gettiming_from_name(mode);
	if (!timing)
		return -EINVAL;

	bits_per_pixel = env_get_ulong("display_bpp", 10, 32);

	fdt32_t fbset[] = {
		[0] = cpu_to_fdt32(timing->h_active),
		[1] = cpu_to_fdt32(timing->v_active),
		[2] = cpu_to_fdt32(timing->h_active),
		[3] = cpu_to_fdt32(timing->v_active * 2),
		[4] = cpu_to_fdt32(bits_per_pixel),
	};

	return fdt_setprop(blob, nodeoffset, property, fbset, sizeof(fbset));
}

static int odroid_fixup_reserved_heap_gfx(void *blob, u64 size)
{
	int err, nodeoffset;

	nodeoffset = fdt_path_offset(blob, "/reserved-memory");
	if (nodeoffset < 0)
		return nodeoffset;

	int size_cells = fdt_size_cells(blob, nodeoffset);
	if ((size_cells < 0) || (size_cells > 2))
		return -EINVAL;

	nodeoffset = fdt_subnode_offset(blob, nodeoffset, "heap-gfx");
	if (nodeoffset < 0)
		return nodeoffset;

	if (size_cells == 2)
		err = fdt_setprop_u64(blob, nodeoffset, "size", size);
	else
		err = fdt_setprop_u32(blob, nodeoffset, "size", size);

	return err;
}

int ft_board_setup(void *blob, bd_t *bd)
{
	char *gfx_heap_size;
	u64 size = 0;

	odroid_fixup_display(blob, env_get("outputmode"));

	// Use GFX heap buffer in size of 'gfx-heap-size'
	gfx_heap_size = env_get("gfx-heap-size");	// in MB
	if (gfx_heap_size)
		size = simple_strtoul(gfx_heap_size, NULL, 10) * SZ_1M;

	// So need to fix the heap size in the device tree
	if (size)
		odroid_fixup_reserved_heap_gfx(blob, size);

	return 0;
}

int __attribute__((weak)) mmc_initialize(bd_t *bis)
{
	return 0;
}

//int __attribute__((weak)) do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
//{
//	return 0;
//}

void __attribute__((weak)) set_working_fdt_addr(ulong addr)
{
}

//int __attribute__((weak)) ofnode_read_u32_default(ofnode node, const char *propname, u32 def)
//{
//	return 0;
//}

void __attribute__((weak)) md5_wd(unsigned char *input, int len, unsigned char output[16],
				  unsigned int chunk_sz)
{
}

const char *boot_device_name(int n)
{
        struct names {
                int id;
                const char* name;
        } names[] = {
                { BOOT_ID_RESERVED, "RESERVED" },
                { BOOT_ID_EMMC, "EMMC" },
                { BOOT_ID_NAND, "NAND" },
                { BOOT_ID_SPI, "SPI" },
                { BOOT_ID_SDCARD, "SD" },
                { BOOT_ID_USB, "USB" },
        };
        int i;

        for (i = 0; i < ARRAY_SIZE(names); i++)
                if (names[i].id == n)
                        return names[i].name;

        return NULL;
}

int get_boot_device(void)
{
	return (readl(SYSCTRL_SEC_STATUS_REG2) >> 4) & 0xf;
}

int mmc_get_env_dev(void)
{
	return (get_boot_device() == BOOT_ID_SDCARD) ? 1 : 0;
}

struct blk_desc *amlogic_get_bootdev(void)
{
	struct mmc *mmc = find_mmc_device(mmc_get_env_dev());
	if (mmc) {
		struct blk_desc *dev_desc = mmc_get_blk_desc(mmc);
		if (dev_desc)
			return dev_desc;
	}

	return NULL;
}
