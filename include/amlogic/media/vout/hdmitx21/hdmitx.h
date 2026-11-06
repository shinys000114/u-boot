/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_H_
#define __HDMITX_H_
#include "hdmi_common.h"
#include "hdmitx_module.h"
#include "hdmitx_reg.h"
#include "mach_reg.h"
#include <command.h>

int hdmitx_likely_frac_rate_mode(char *m);
unsigned int hdmi_outputmode_check(char *mode, unsigned int frac);
int do_hpd_detect(cmd_tbl_t *cmdtp, int flag, int argc,
		  char *const argv[]);

/*
 * An HDMI output chosen by the caller. It takes the place of the mode name
 * and of the colorattribute, voutmode and frac_rate_policy environment that
 * the "hdmitx output" command reads.
 */
struct hdmitx21_output {
	/* The encoder timing: a VIC table entry or the custom slot */
	const struct hdmi_timing *timing;
	/* The pixel clock to send, in kHz, without a 1000/1001 shift */
	u32 pixel_khz;
	/* HDMI_COLORSPACE_RGB or HDMI_COLORSPACE_YUV420, 8 bits */
	enum hdmi_colorspace cs;
	/* The sink takes DVI: no data islands */
	bool dvi;
	/*
	 * The mode is not the CTA mode of its timing entry: no VIC in the AVI
	 * InfoFrame and full-range RGB, as for a timing outside the CTA table
	 */
	bool no_vic;
	/*
	 * The VPP already compresses limited-range RGB (its gamma LUT), so the
	 * HDMI formatter passes RGB unchanged
	 */
	bool vpp_range;
};

/* Start @out on the transmitter. Returns 0, or -EINVAL for a bad @out. */
int hdmitx21_set_output(const struct hdmitx21_output *out);

#endif
