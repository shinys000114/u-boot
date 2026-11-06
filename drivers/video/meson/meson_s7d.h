/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * ODROID-C5 boot display on the S7D HDMI output.
 */
#ifndef __MESON_S7D_H
#define __MESON_S7D_H

#include <linux/types.h>

struct hdmi_timing;
struct hdmitx_dev;

/* The HDMI output chosen from the sink, the way the Linux kernel does. */
struct s7d_hdmi_mode {
	/* VIC table entry, or the custom slot HDMI_ODROID_CUSTOM_VIDEO */
	const struct hdmi_timing *timing;
	/* Exact pixel clock to send, in kHz */
	u32 pixel_khz;
	/* The sink takes DVI only: no InfoFrames, full-range RGB */
	bool dvi;
	/* drm_match_cea_mode() finds no VIC for the mode */
	bool no_vic;
	/* 4:2:0 8-bit instead of RGB 8-bit */
	bool yuv420;
};

/*
 * Choose the output for the connected sink as the S7D DRM driver probes
 * it: read the EDID over DDC, build and merge the mode list like the DRM
 * core and pick the mode its fbdev shows. @panel_edid, when not NULL, is
 * the EDID that a bridge in front of the connector holds; its first
 * detailed timing is the mode when no EDID reads over DDC. Returns
 * -EINVAL when no mode fits.
 */
int s7d_hdmi_select_mode(struct hdmitx_dev *hdev, const u8 *panel_edid,
			 struct s7d_hdmi_mode *mode);

/* The EDID the last selection read, and its block count (0 for none). */
const u8 *s7d_hdmi_edid(int *blocks);

#endif
