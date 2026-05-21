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
#include <mmc.h>
#include <vsprintf.h>

#include <amlogic/media/vout/hdmitx21/hdmitx.h>

#define DEFAULT_LOGO_FB_WIDTH	1280
#define DEFAULT_LOGO_FB_HEIGHT	720
#define DEFAULT_LOGO_FB_BPP	32

extern const struct hdmi_timing *load_hdmi_timing_from_env(const char *modeline, const char *mode);
extern const struct hdmi_timing *load_hdmi_timing_from_dt(struct blk_desc *dev_desc, const char *displaymode);
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
	const char *mode = env_get("outputmode");
	const struct hdmi_timing *ht = lookup_display_timing(mode);

	if (ht)
		return ht;

	mode = env_get("hdmimode");
	ht = lookup_display_timing(mode);
	if (ht)
		return ht;

	mode = env_get("displaymode");
	return lookup_display_timing(mode);
}

static void set_logo_fb_env(int width, int height, int bpp)
{
	env_set_ulong("display_bpp", bpp);
	env_set_ulong("display_color_index", bpp);
	env_set_ulong("display_width", width);
	env_set_ulong("display_height", height);
	env_set_ulong("fb_width", width);
	env_set_ulong("fb_height", height);
}

static void set_display_handoff_env(const struct hdmi_timing *ht)
{
	if (!ht)
		return;

	env_set("outputmode", ht->name);
	env_set("hdmimode", ht->name);
	env_set_ulong("fb_width", ht->h_active);
	env_set_ulong("fb_height", ht->v_active);
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

static int display_logo_raw(int width, int height, int bpp)
{
	const struct hdmi_timing *ht;
	int ret;
	int bmp_load = false;

	env_set("bootlogo_addr", "0x1f800000");
	set_logo_fb_env(width, height, bpp);

	bmp_load = load_boot_logo();
	if (bmp_load) {
		/* osd, bmp */
		ret = run_command("osd open; osd clear", 0);
		if (ret)
			return ret;

		ret = run_command("bmp display ${bootlogo_addr}", 0);
		if (ret)
			return ret;

		run_command("bmp scale", 0);
	}

	ht = get_active_output_timing();
	if (!ht)
		return -EINVAL;

	ret = run_command("vout output $outputmode", 0);
	if (ret)
		return ret;

	set_display_handoff_env(ht);

	return 0;
}

static int display_logo_(const char *mode, int width, int height, int bpp)
{
	int ret;

	ret = init_hdmi(mode, true);
	if (ret) {
		printf("init hdmi fail\n");
		return ret;
	}

	return display_logo_raw(width, height, bpp);
}

static int display_logo(const char *mode, const char *bmp_width, const char *bmp_height)
{
	return display_logo_(mode,
			simple_strtoul(bmp_width, NULL, 10),
			simple_strtoul(bmp_height, NULL, 10),
			DEFAULT_LOGO_FB_BPP);
}

static int do_showlogo(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	const struct hdmi_timing *ht = NULL;
	const char *mode = env_get("displaymode");

	if (!lookup_display_timing(mode))
		mode = NULL;

	if (argc >= 2) {
		if (argc >= 4)
			return display_logo(argv[1], argv[2], argv[3]);

		ht = lookup_display_timing(argv[1]);
		if (!ht)
			return -EINVAL;

		return display_logo_(argv[1],
				DEFAULT_LOGO_FB_WIDTH,
				DEFAULT_LOGO_FB_HEIGHT,
				DEFAULT_LOGO_FB_BPP);
	}

	return display_logo_(mode,
			DEFAULT_LOGO_FB_WIDTH,
			DEFAULT_LOGO_FB_HEIGHT,
			DEFAULT_LOGO_FB_BPP);
}

U_BOOT_CMD(
		showlogo,		4,		0,	do_showlogo,
		"Displaying BMP logo file to HDMI screen with the specified resolution",
		"<resolution> [<bmp_width> <bmp_height>]\n"
		"	resolution - screen resoltuion on HDMI screen\n"
		"		current display timing will be detected if missing\n"
		"	bmp_width (optional) - width of logo bmp file\n"
		"		'1280' will be used by default if missing\n"
		"	bmp_height (optional) - height of logo bmp file\n"
		"		'720' will be used by default if missing"
);
