// SPDX-License-Identifier: GPL-2.0-only
/*
 * ODROID-C5 boot display: OSD1, its scaler and the static VPP settings as
 * the Linux S7D DRM driver programs them (s7d-osd-pipeline.c,
 * s7d-osd-scaler.c, s7d-vpp.c, s7d-gamma.c, s7d-encp.c), so that the
 * kernel finds the registers it would have written itself.
 *
 * Copyright (c) 2026 Hardkernel Co., Ltd.
 * Filter coefficients Copyright (c) 2025 Amlogic, Inc.
 */

#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include <amlogic/media/vout/hdmitx21/hdmi_common.h>

#include "meson_s7d.h"

#define VCBUS(reg)		(0xff000000UL + (reg) * 4)

#define OSD1_BLK0_CFG_W4	0x1a13
#define OSD1_BLK0_CFG_W3	0x1a1e
#define OSD1_FIFO_CTRL		0x1a2b
#define OSD1_PROT_CTRL		0x1a2e
#define VPP_POSTBLEND_H_SIZE	0x1d21
#define VPP_DOLBY_CTRL		0x1d93
#define POST2_TO_OUTBUF_BYPASS	BIT(2)
#define VPP_OSD_SC_DIV_ALPHA	0x1dbf
#define VPP_OSD1_IN_SIZE	0x1df1
#define VPP_OSD_SCALE_CTRL	0x1dff
#define OSD1_HDR2_CTRL		0x38a0
#define OSD1_HDR2_MATRIXI_EN	0x38db
#define OSD_BLEND_DIN0_H	0x39b1
#define OSD_BLEND_DIN0_V	0x39b2
#define OSD_BLEND0_SIZE		0x39bb
#define OSD_BLEND1_SIZE		0x39bc
#define OSD_BLEND_CTRL1		0x39c0
#define VPP_GAMMA_CTRL		0x39d4
#define VPP_GAMMA_BIN_ADDR	0x39d5
#define VPP_GAMMA_BIN_DATA	0x39d6

#define S7D_FILTER_PHASES	33
#define SCALER_LINEBUFFER	1920
#define SCALER_MAX_INPUT_WIDTH	3840
#define SCALER_MAX_UPSCALE	4
#define SCALER_MAX_DOWNSCALE	2

static const u32 s7d_filter_bilinear[S7D_FILTER_PHASES] = {
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
	0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
	0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
	0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
	0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
	0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
	0x00404000,
};

static const u32 s7d_filter_bilinear_2point[S7D_FILTER_PHASES] = {
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000,
};

static const u32 s7d_filter_bicubic[S7D_FILTER_PHASES] = {
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8,
};

static const u32 s7d_filter_triangle[S7D_FILTER_PHASES] = {
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010,
};

static void vcbus_update(u32 reg, u32 mask, u32 value)
{
	writel((readl(VCBUS(reg)) & ~mask) | value, VCBUS(reg));
}

struct s7d_osd_scaler {
	u32 input_size;
	u32 output_h;
	u32 output_v;
	u32 h_phase_step;
	u32 v_phase_step;
	u32 h_control;
	u32 v_control;
};

/*
 * Vendor f2v_get_vertical_phase for a progressive input: the output line
 * sits at out_pos quarter lines (s7d_osd_scaler.c scaler_vphase()).
 */
static void scaler_vphase(u64 ratio, unsigned int out_pos, u32 bank,
			  u32 *rcv, u32 *rpt)
{
	u64 in = 0, out = (out_pos * ratio) >> 4;

	*rcv = bank;
	*rpt = bank >= 3;
	while (in + (4 << 16) <= out) {
		if (*rpt)
			*rpt = 0;
		else
			(*rcv)++;
		in += 4 << 16;
	}
}

/*
 * s7d_osd_scaler_build() for a progressive OSD1. The scaler also runs at
 * 1:1 here, as the boot display always scales its frame buffer.
 */
static int s7d_osd_scaler_build(u32 input_w, u32 input_h, u32 output_w,
				u32 output_h, struct s7d_osd_scaler *s)
{
	u32 v_control = BIT(25) | BIT(24), bank, rcv, rpt;
	u64 ratio;

	if (input_w < 4 || input_h < 4 || input_w > SCALER_MAX_INPUT_WIDTH ||
	    input_h > 2160 || !output_w || !output_h || output_w > 4096 ||
	    output_h > 2160 ||
	    input_w > output_w * SCALER_MAX_DOWNSCALE ||
	    input_h > output_h * SCALER_MAX_DOWNSCALE ||
	    output_w > input_w * SCALER_MAX_UPSCALE ||
	    output_h > input_h * SCALER_MAX_UPSCALE)
		return -ERANGE;
	bank = input_w > SCALER_LINEBUFFER ? 2 : 4;
	if (bank == 2)
		v_control |= BIT(21);
	ratio = lldiv((u64)input_h << 20, output_h);
	scaler_vphase(ratio, 0, bank, &rcv, &rpt);
	v_control |= (rpt << 8) | (rcv << 3);

	s->input_size = ((input_w - 1) << 16) | (input_h - 1);
	s->output_h = output_w - 1;
	s->output_v = output_h - 1;
	s->h_phase_step = (u32)lldiv((u64)input_w << 18, output_w) << 6;
	s->v_phase_step = (u32)ratio << 4;
	s->h_control = BIT(22) | BIT(8) | (4 << 3) | 4;
	s->v_control = v_control | bank;
	return 0;
}

/* scaler_tables(): the vertical and horizontal filters */
static void scaler_tables(const struct s7d_osd_scaler *s, const u32 *tables[2])
{
	u32 input_w = (s->input_size >> 16) + 1;
	u32 input_h = (s->input_size & 0xffff) + 1;

	tables[0] = s7d_filter_bilinear;
	tables[1] = s7d_filter_bilinear;
	if (input_w > SCALER_LINEBUFFER) {
		tables[0] = s7d_filter_bilinear_2point;
	} else if (s->output_h < 720 &&
		   (input_w > s->output_h + 1 || input_h > s->output_v + 1)) {
		tables[0] = input_h > s->output_v + 1 ?
			s7d_filter_triangle : s7d_filter_bicubic;
		tables[1] = input_w > s->output_h + 1 ?
			s7d_filter_triangle : s7d_filter_bicubic;
	}
}

/*
 * s7d_osd_scaler_setup() with OSD1 scaled and OSD2 unscaled: the scaler is
 * the OSD1 local scaler in front of the OSD blend (route 1).
 */
static void s7d_osd_scaler_setup(const struct s7d_osd_scaler *s)
{
	static const u32 base[] = { 0x1dc0, 0x3d00 };
	const u32 r = base[0];
	const u32 *tables[2];
	unsigned int i, direction, phase;

	vcbus_update(VPP_OSD_SCALE_CTRL, GENMASK(2, 0), 1);
	for (i = 0; i < ARRAY_SIZE(base); i++) {
		vcbus_update(base[i] + 8, GENMASK(13, 2), 0);
		vcbus_update(base[i] + 2, BIT(24) | BIT(23), 0);
		vcbus_update(base[i] + 5, BIT(22), 0);
	}
	vcbus_update(r + 9, 0x1fff1fff, s->input_size);
	vcbus_update(r + 10, 0x0fff0fff, s->output_h);
	vcbus_update(r + 11, 0x0fff0fff, s->output_v);
	vcbus_update(r, GENMASK(27, 0), s->v_phase_step);
	vcbus_update(r + 1, U32_MAX, 0);
	vcbus_update(r + 3, GENMASK(27, 0), s->h_phase_step);
	vcbus_update(r + 4, U32_MAX, 0);
	vcbus_update(r + 6, 0xff77, 0);
	/*
	 * The pad colour of s7d_vpp_setup(): the Linux driver loads 0x80808080
	 * only for a scaler it enables, and a full buffer needs no padding.
	 */
	vcbus_update(r + 7, GENMASK(23, 0), 0x808000);
	vcbus_update(VPP_OSD_SC_DIV_ALPHA, BIT(7) | GENMASK(5, 4) | GENMASK(2, 0),
		     0xb1);

	scaler_tables(s, tables);
	for (direction = 0; direction < 2; direction++) {
		writel(direction << 8, VCBUS(0x1dcc));
		for (phase = 0; phase < S7D_FILTER_PHASES; phase++)
			writel(tables[direction][phase], VCBUS(0x1dcd));
	}
	writel(0, VCBUS(0x1dcc));

	vcbus_update(r + 2, GENMASK(25, 19) | GENMASK(17, 16) | GENMASK(14, 11) |
		     GENMASK(9, 8) | GENMASK(6, 0), s->v_control);
	vcbus_update(r + 5, GENMASK(22, 19) | GENMASK(17, 16) | GENMASK(14, 11) |
		     GENMASK(9, 8) | GENMASK(6, 0), s->h_control);
	vcbus_update(r + 8, GENMASK(13, 2), BIT(3) | BIT(2));
}

/* s7d_gamma_code(): 10-bit identity, limited range folded in, at 12 bits */
static u32 s7d_gamma_code(u32 value)
{
	return (64 + DIV_ROUND_CLOSEST(value * 877, 1024)) << 2;
}

/*
 * s7d_gamma_program() without a LUT: for limited-range RGB the gamma LUT
 * compresses the range, otherwise it is off. Both leave the gamma stage
 * un-bypassed.
 */
static void s7d_gamma_set(bool limited)
{
	unsigned int channel, i;

	vcbus_update(VPP_GAMMA_CTRL, GENMASK(1, 0), 0);
	vcbus_update(VPP_DOLBY_CTRL, POST2_TO_OUTBUF_BYPASS, 0);
	if (!limited)
		return;
	writel(0, VCBUS(VPP_GAMMA_BIN_ADDR));
	for (channel = 0; channel < 3; channel++) {
		for (i = 0; i < 32; i++)
			writel(s7d_gamma_code(min(2 * i * 16, 1023U)) |
			       s7d_gamma_code(min((2 * i + 1) * 16, 1023U)) << 16,
			       VCBUS(VPP_GAMMA_BIN_DATA));
		writel(s7d_gamma_code(1023), VCBUS(VPP_GAMMA_BIN_DATA));
	}
	vcbus_update(VPP_GAMMA_CTRL, GENMASK(1, 0), 3);
}

int s7d_osd_align(u32 fb_w, u32 fb_h, const struct hdmi_timing *t,
		  const int win[4], bool limited)
{
	u32 width = t->h_active, height = t->v_active;
	/* s7d_encp_build_state(): de_v_begin from vs 1, and eight fill lines */
	u32 de_v_begin = t->v_sync + t->v_back + 1, hold = 8;
	struct s7d_osd_scaler s;
	int ret;

	if (!t->pi_mode || t->pixel_repetition_factor)
		return -EINVAL;
	ret = s7d_osd_scaler_build(fb_w, fb_h, win[2] - win[0] + 1,
				   win[3] - win[1] + 1, &s);
	if (ret)
		return ret;
	if (de_v_begin >= 1 + 16)
		hold = min_t(u32, 31, de_v_begin - 1 - 8);

	/* s7d_vpp_setup(): the static settings on the OSD1 path */
	vcbus_update(VPP_POSTBLEND_H_SIZE, 0x3fff3fff, (height << 16) | width);
	vcbus_update(VPP_OSD1_IN_SIZE, 0x1fff1fff, (height << 16) | width);
	vcbus_update(VPP_DOLBY_CTRL, GENMASK(25, 24) | BIT(20) | BIT(14) | BIT(12) |
		     GENMASK(10, 8) | GENMASK(3, 0), BIT(14) | 7);
	vcbus_update(OSD1_HDR2_CTRL, GENMASK(20, 13) | GENMASK(7, 2), 0);
	vcbus_update(OSD1_HDR2_MATRIXI_EN, GENMASK(1, 0), 0);

	/* s7d_osd_build_pipeline(): OSD1 into blend input 0, output sized */
	writel((0x82840501 & ~GENMASK(9, 5)) | hold << 5, VCBUS(OSD1_FIFO_CTRL));
	writel(0x80620200, VCBUS(OSD1_PROT_CTRL));
	writel((fb_w - 1) << 16, VCBUS(OSD1_BLK0_CFG_W3));
	writel((fb_h - 1) << 16, VCBUS(OSD1_BLK0_CFG_W4));
	writel(win[2] << 16 | win[0], VCBUS(OSD_BLEND_DIN0_H));
	writel(win[3] << 16 | win[1], VCBUS(OSD_BLEND_DIN0_V));
	writel((height << 16) | width, VCBUS(OSD_BLEND0_SIZE));
	writel((height << 16) | width, VCBUS(OSD_BLEND1_SIZE));
	writel(0x31031, VCBUS(OSD_BLEND_CTRL1));

	s7d_osd_scaler_setup(&s);
	s7d_gamma_set(limited);
	return 0;
}
