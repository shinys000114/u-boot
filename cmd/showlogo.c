/*
 * Copyright (C) 2025 Hardkernel Co,. Ltd
 * Dongjin Kim <tobetter@gmail.com>
 *
 *  This driver has been modified to support ODROID-C5.
 *      Modified by Bob Shin <bob.shin@hardkernel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <fs.h>
#include <malloc.h>
#include <mmc.h>
#include <vsprintf.h>

#include <amlogic/lcd.h>
#include <amlogic/media/vout/hdmitx21/hdmitx.h>
#include <bmp_layout.h>

#define DEFAULT_LOGO_FB_BPP	32

static const struct hdmi_timing *ht = NULL;

extern const struct hdmi_timing *load_hdmi_timing_from_env(const char *modeline, const char *mode);
extern const struct hdmi_timing *load_hdmi_timing_from_display_dtbo(struct blk_desc *dev_desc, const char*dt);

extern const struct hdmi_timing *load_hdmi_timing_from_dt(struct blk_desc *dev_desc, const char *displaymode);
extern const struct hdmi_timing *lookup_hdmi_timing_from_display_dt(void);
extern const struct hdmi_timing *lookup_hdmi_timing_by_name(const char *mode);
extern const struct hdmi_timing *hdmitx21_gettiming_from_name(const char *name);
extern void hdmi_timing_printdebug(const struct hdmi_timing *ht);
extern struct blk_desc *amlogic_get_bootdev(void);
extern int load_from_mmc(unsigned long addr, int devnum, int partnum, const char *filename);

static const struct hdmi_timing *lookup_display_timing(const char *mode)
{
	if (!mode || !*mode)
		return NULL;

	return lookup_hdmi_timing_by_name(mode);
}

static const struct hdmi_timing *get_active_output_timing(void)
{
	const char *mode;
	const struct hdmi_timing *ht;

	ht = lookup_hdmi_timing_from_display_dt();
	if (ht)
		return ht;

	mode = env_get("displaymode");
	ht = lookup_display_timing(mode);
	if (ht)
		return ht;

	mode = env_get("outputmode");
	ht = lookup_display_timing(mode);
	if (ht)
		return ht;

	mode = env_get("hdmimode");
	return lookup_display_timing(mode);
}

static void set_logo_fb_env(int display_width, int display_height,
			    int fb_width, int fb_height, int bpp)
{
	env_set_ulong("display_bpp", bpp);
	env_set_ulong("display_color_index", bpp);
	env_set_ulong("display_width", display_width);
	env_set_ulong("display_height", display_height);
	env_set_ulong("fb_width", fb_width);
	env_set_ulong("fb_height", fb_height);
}

static void set_display_handoff_env(void)
{
	const char *colorattribute;
	const char *scan_info;
	const char *vout_init;
	char hdmitx[64];
	char vout[64];

	if (!ht)
		return;

	env_set("outputmode", ht->name);
	env_set("hdmimode", ht->name);
	env_set("displaymode", ht->vic == HDMI_ODROID_CUSTOM_VIDEO ?
			ht->name : NULL);
	env_set_ulong("fb_width", ht->h_active);
	env_set_ulong("fb_height", ht->v_active);

	vout_init = env_get("vout_init");
	if (ht->name && *ht->name && vout_init && *vout_init) {
		snprintf(vout, sizeof(vout), "%s,%s", ht->name, vout_init);
		env_set("vout", vout);
	} else {
		env_set("vout", NULL);
	}

	colorattribute = env_get("colorattribute");
	scan_info = env_get("scan_info");
	if (colorattribute && *colorattribute && scan_info && *scan_info) {
		snprintf(hdmitx, sizeof(hdmitx), "%s,scan_info:%s",
			 colorattribute, scan_info);
		env_set("hdmitx", hdmitx);
	} else {
		env_set("hdmitx", NULL);
	}
}

static int init_hdmi(const char* mode, bool bestmode) {
	/*
	 * TODO reboot mode
	 * run_command("get_rebootmode", 0);
	 * rbm = env_get("reboot_mode");
	 * if rbm in {quiescent, recovery_quiescent} then pass hdmi init
	 */
	int ret;
	env_set("hdmimode", mode);

	if (bestmode == false) {
		env_set("is.bestmode", "false");
	}

	/* hdmi config */
	ret = run_command("hdmitx hpd", 0);
	if (ret == 0) {
		printf("hdmi not connected!\n");
		return -1;
	}

	ret = run_command("hdmitx get_parse_edid", 0);
	if (ret < 0) {
		printf("edid parse fail!\n");
		return -1;
	}
	return 0;
}

static int load_boot_logo(void)
{
	const char *logofiles[] = {
		NULL,
#ifdef CONFIG_VIDEO_BMP_GZIP
		"boot-logo.bmp.gz",
#endif
		"boot-logo.bmp"
	};

	const char *bootlogo;
	struct blk_desc *dev_desc = amlogic_get_bootdev();
	int i;
	int ret;
	ulong addr;

	addr = simple_strtoull(env_get("bootlogo_addr"), NULL, 0);
	if (!addr || !dev_desc)
		return 0;

	logofiles[0] = env_get("bootlogo");

	for (i = 0; i < ARRAY_SIZE(logofiles); i++) {
		bootlogo = logofiles[i];
		if (!bootlogo)
			continue;

		ret = load_from_mmc(addr, dev_desc->devnum, 1, bootlogo);
		if (ret)
			ret = load_from_mmc(addr, dev_desc->devnum, 2, bootlogo);
		if (ret == 0)
			return 1;
	}

	return 0;
}

static int get_boot_logo_size(ulong addr, int *width, int *height)
{
	bmp_image_t *bmp = (bmp_image_t *)addr;
	void *bmp_alloc_addr = NULL;
	unsigned long len;
	int bmp_height;

	if (!bmp || !width || !height)
		return -EINVAL;

	if (!((bmp->header.signature[0] == 'B') &&
	      (bmp->header.signature[1] == 'M')))
		bmp = gunzip_bmp(addr, &len, &bmp_alloc_addr);

	if (!bmp)
		return -EINVAL;

	bmp_height = (int)le32_to_cpu(bmp->header.height);
	if (bmp_height < 0)
		bmp_height = -bmp_height;

	*width = le32_to_cpu(bmp->header.width);
	*height = bmp_height;

	if (bmp_alloc_addr)
		free(bmp_alloc_addr);

	return (*width > 0 && *height > 0) ? 0 : -EINVAL;
}

static int display_logo_raw(void)
{
	int ret;
	int bmp_load = false;
	int width;
	int height;
	int fb_width;
	int fb_height;
	char *force_hpd;
	ulong logo_addr;

	if (!ht)
		return -EINVAL;

	width = ht->h_active;
	height = ht->v_active;
	fb_width = width;
	fb_height = height;

	env_set("bootlogo_addr", "0x1f800000");

	set_display_handoff_env();

	bmp_load = load_boot_logo();
	if (bmp_load) {
		int bmp_width, bmp_height;

		logo_addr = simple_strtoull(env_get("bootlogo_addr"), NULL, 0);
		ret = get_boot_logo_size(logo_addr, &bmp_width, &bmp_height);
		if (!ret) {
			if (fb_width < bmp_width)
				fb_width = bmp_width;
			if (fb_height < bmp_height)
				fb_height = bmp_height;
		}

		set_logo_fb_env(width, height, fb_width, fb_height,
				DEFAULT_LOGO_FB_BPP);

		/* osd, bmp */
		ret = run_command("osd open; osd clear", 0);
		if (ret)
			return ret;

		ret = run_command("bmp display ${bootlogo_addr}", 0);
		if (ret)
			return ret;

		run_command("bmp scale", 0);
	} else {
		set_logo_fb_env(width, height, fb_width, fb_height,
				DEFAULT_LOGO_FB_BPP);
	}

	ret = run_command("vout output $outputmode", 0);
	if (ret)
		return ret;

	force_hpd = env_get("early_usb_start");
	if (force_hpd && !strncmp(force_hpd, "true", strlen("true")))
		run_command("usb hub-on 1", 0);

	return 0;
}

static int display_logo_(const char *mode)
{
	int ret;

	if (!mode)
		return -EINVAL;

	ret = init_hdmi(mode, true);
	if (ret) {
		printf("init hdmi fail\n");
		return ret;
	}

	return display_logo_raw();
}

static int display_logo(const char *mode)
{
	ht = lookup_display_timing(mode);
	if (!ht)
		return -EINVAL;

	return display_logo_(ht->name);
}

static int do_showlogo(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	int ret;

	if (argc >= 2) {
		if (argc > 2)
			return CMD_RET_USAGE;

		return display_logo(argv[1]);
	}

	ht = get_active_output_timing();
	if (!ht) {
		ret = init_hdmi("none", true);
		if (ret) {
			printf("init hdmi fail\n");
			return ret;
		}

		ht = lookup_display_timing(env_get("outputmode"));
		if (!ht)
			return -EINVAL;

		return display_logo_raw();
	}

	return display_logo_(ht->name);
}

U_BOOT_CMD(
		showlogo,		2,		0,	do_showlogo,
		"Displaying BMP logo file to HDMI screen with the specified resolution",
		"[resolution]\n"
		"	resolution - screen resoltuion on HDMI screen\n"
		"		current display timing will be detected if missing"
);
