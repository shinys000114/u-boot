/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * board/amlogic/t5d_skt_v1/lcd.c
 *
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
 *
 */

#include <common.h>
#include <amlogic/media/vout/lcd/aml_lcd.h>

/***************************************************
 * lcd_0
 ***************************************************/
static char lcd0_cpu_gpio[LCD_CPU_GPIO_NUM_MAX][LCD_CPU_GPIO_NAME_MAX] = {
	"GPIOH_7",
	"invalid", /* ending flag */
};

static char lcd0_bl_gpio[BL_GPIO_NUM_MAX][LCD_CPU_GPIO_NAME_MAX] = {
	"invalid", /* ending flag */
};

static struct lcd_power_step_s lcd0_power_on_step[] = {
	{LCD_POWER_TYPE_CPU,    0, 1, 50,}, /* panel vcc */
	{LCD_POWER_TYPE_SIGNAL, 0, 0, 0,},
	{LCD_POWER_TYPE_MAX,   0, 0, 0,},  /* ending flag */
};

static struct lcd_power_step_s lcd0_power_off_step[] = {
	{LCD_POWER_TYPE_SIGNAL, 0, 0, 0,},
	{LCD_POWER_TYPE_CPU,   0, 0, 500,}, /* panel vcc */
	{LCD_POWER_TYPE_MAX,   0, 0, 0,},   /* ending flag */
};

static struct lcd_power_step_s lcd0_power_on_step_p2p[] = {
	{LCD_POWER_TYPE_CPU,    0, 1, 300,}, /* panel vcc */
	{LCD_POWER_TYPE_EXTERN, 0, 0,  50,}, /* pmu i2c init */
	{LCD_POWER_TYPE_SIGNAL, 0, 0,   0,},
	{LCD_POWER_TYPE_MAX,    0, 0,   0,}, /* ending flag */
};

#ifdef CONFIG_AML_LCD_EXTERN
static char lcd_ext_gpio[LCD_EXTERN_GPIO_NUM_MAX][LCD_CPU_GPIO_NAME_MAX] = {
	"invalid", /* ending flag */
};

static struct lcd_pinmux_ctrl_s lcd_ext_pinmux_ctrl[LCD_PINMUX_NUM] = {
	{
		.name = "invalid",
	},
};

static unsigned char init_on_table[] = {
	//HV550QUB-E13, CHPI
	0xc0,43,0x00,
		0xc8,0x19,0x19,0x01,0x21,0x23,0xf8,0x6a,0x7f,0x35,0x34,0xe3,0x1c,0x18,0x0b,0x0b,
		0x3d,0xf0,0x00,0x34,0x52,0xe8,0x2b,0x70,0x00,0x21,0x21,0xe2,0x00,0x01,0x3e,0x10,
		0xe0,0xae,0x00,0x00,0x11,0x1d,0x80,0x00,0x00,0x00,
	0xff, 0, /* ending */
};

static unsigned char init_off_table[LCD_EXTERN_INIT_OFF_MAX] = {
	0xff, 0,  /* ending */
};

static struct lcd_extern_common_s ext_common_dft = {
	.key_valid = 1,
	.ext_num = 1,
	.ext_gpio = lcd_ext_gpio,

	.i2c_bus = LCD_EXT_I2C_BUS_3,
	.i2c_sck_gpio = 0xff,
	.i2c_sck_gpio_off = 0,
	.i2c_sda_gpio = 0xff,
	.i2c_sda_gpio_off = 0,
	.ext_pinmux = lcd_ext_pinmux_ctrl,
};

static struct lcd_extern_config_s ext_config_dtf[LCD_EXTERN_DEV_MAX] = {
	{
		.index = 0,
		.name = "ext_default",
		/* LCD_EXTERN_I2C, LCD_EXTERN_SPI, LCD_EXTERN_MAX */
		.type = LCD_EXTERN_I2C,
		.status = 1, /* 0=disable, 1=enable */
		.i2c_addr = 0x33, /* 7bit i2c address, 0xff for none */
		.i2c_addr2 = 0xff,
		.i2c_addr3 = 0xff,
		.i2c_addr4 = 0xff,

		.cmd_size = 0xff,
		.table_init_on = init_on_table,
		.table_init_off = init_off_table,
		.table_init_on_cnt = ARRAY_SIZE(init_on_table),
		.table_init_off_cnt = ARRAY_SIZE(init_off_table),
		.table_init_loaded = 1,
	},
	{
		.index = LCD_EXTERN_INDEX_INVALID,
	},
};
#endif

struct ext_lcd_config_s ext_lcd0_config[LCD_NUM_MAX] = {
	{/* normal*/
	"lvds_0",LCD_LVDS,8,
	/* basic timing */
	1920,1080,2200,1125,44,148,0,5,36,0,
	/* clk_attr */
	2,0,1,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	/* lvds_attr */
	1,1,0,0,0,0x3,0x0,Rsv_val,Rsv_val,Rsv_val,
	NULL, NULL,
	/* power step */
	lcd0_power_on_step, lcd0_power_off_step,
	/* backlight */
	60,255,10,128,128,
	BL_CTRL_PWM,0,1,0,200,200,
	BL_PWM_POSITIVE,BL_PWM_D,180,100,25,1,0,
	Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	10,10,Rsv_val
	},

	{/*public 1-region vx1 : 3840x2160@60hz 8lane */
	"vbyone_1", LCD_VBYONE, 10,
	/* basic timing */
	3840, 2160, 4400, 2250, 33, 477, 0, 6, 81, 0,
	/* clk_attr */
	2, 0, 1, 60, 2, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	/* vbyone_attr */
	8, 1, 4, 4, 0x7, 0x1, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	NULL, NULL,
	/* power step */
	lcd0_power_on_step, lcd0_power_off_step,
	/* backlight */
	60, 255, 10, 128, 128,
	BL_CTRL_MAX, 0, 1, 0, 200, 200,
	BL_PWM_POSITIVE, BL_PWM_E, 180, 100, 25, 1, 0,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	10, 10, Rsv_val
	},

	{/*public 2-region vx1 : 3840x2160@120hz 16lane */
	"vbyone_2", LCD_VBYONE, 10,
	/* basic timing */
	3840, 2160, 4400, 2250, 33, 477, 0, 6, 65, 0,
	/* clk_attr */
	2, 0, 1, 120, 2, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	/* vbyone_attr */
	16, 1, 4, 4, 0x7, 0x1, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	NULL, NULL,
	/* power step */
	lcd0_power_on_step, lcd0_power_off_step,
	/* backlight */
	60, 255, 10, 128, 128,
	BL_CTRL_MAX, 0, 1, 0, 200, 200,
	BL_PWM_POSITIVE, BL_PWM_E, 180, 100, 25, 1, 0,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	10, 10, Rsv_val},

	{/*public 2-region vx1 : 3840x2160@144hz 16lane */
	"vbyone_3", LCD_VBYONE, 10,
	/* basic timing */
	3840, 2160, 4096, 2235, 32, 200, 0, 6, 50, 0,
	/* clk_attr */
	2, 0, 1, 144, 2, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	/* vbyone_attr */
	16, 1, 4, 4, 0x7, 0x1, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	NULL, NULL,
	/* power step */
	lcd0_power_on_step, lcd0_power_off_step,
	/* backlight */
	60, 255, 10, 128, 128,
	BL_CTRL_MAX, 0, 1, 0, 200, 200,
	BL_PWM_POSITIVE, BL_PWM_E, 180, 100, 25, 1, 0,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	Rsv_val, Rsv_val, Rsv_val, Rsv_val,
	10, 10, Rsv_val},
	{/*public p2p chpi : 3840x2160@60hz 12lane 2ppc */
	"p2p_0",LCD_P2P,8,
	/* basic timing */
	3840,2160,4400,2250,16,29,0,6,65,0,
	/* clk_attr */
	2,0,0x11,Rsv_val,2,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	/* p2p attr */
	0x10,12,0x76543210,0xba98,0,0,0x5,0x1,Rsv_val,Rsv_val,
	NULL, NULL,
	/* power step */
	lcd0_power_on_step_p2p, lcd0_power_off_step,
	/* backlight */
	60,255,10,128,128,
	BL_CTRL_MAX,0,1,0,200,200,
	BL_PWM_POSITIVE,BL_PWM_C,180,100,25,1,0,
	Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	Rsv_val,Rsv_val,Rsv_val,Rsv_val,
	10,10,Rsv_val},
};

static struct lcd_pinmux_ctrl_s lcd0_pinmux_ctrl[LCD_PINMUX_MAX] = {
	{
		.name = "lcd_vbyone_pin", //GPIOH_0/1
		.pinmux_set = {{0x7, 0x44}, {LCD_PINMUX_END, 0x0}},
		.pinmux_clr = {{0x7, 0xff}, {LCD_PINMUX_END, 0x0}},
	},
	{
		.name = "lcd_p2p_pin", //GPIOH_0~6
		.pinmux_set = {{7, 0x01111112}, {LCD_PINMUX_END, 0x0}},
		.pinmux_clr = {{7, 0x0fffffff}, {LCD_PINMUX_END, 0x0}},
	},
	{
		.name = "lcd_p2p_usit_pin", //GPIOH_0~6
		.pinmux_set = {{7, 0x01111113}, {LCD_PINMUX_END, 0x0}},
		.pinmux_clr = {{7, 0x0fffffff}, {LCD_PINMUX_END, 0x0}},
	},
	{
		.name = "invalid",
	},
};

static struct lcd_pinmux_ctrl_s lcd0_bl_pinmux_ctrl[BL_PINMUX_MAX] = {
	{
		.name = "bl_pwm_on_pin", /*GPIOH_12*/
		.pinmux_set = {{0x8, 0x40000}, {LCD_PINMUX_END, 0x0} },
		.pinmux_clr = {{0x8, 0xf0000}, {LCD_PINMUX_END, 0x0} },
	},
	{
		.name = "bl_pwm_vs_on_pin", //GPIOH_12
		.pinmux_set = {{0x8, 0x30000}, {LCD_PINMUX_END, 0x0}},
		.pinmux_clr = {{0x8, 0xf0000}, {LCD_PINMUX_END, 0x0}},
	},
	{
		.name = "invalid",
	},
};

/*********************************************
 * lcd default config
 ***************************************************/
static struct lcd_dft_config_s lcd_dft_conf[] = {
	{//index 0
		.lcd_gpio = lcd0_cpu_gpio,
		.key_valid = 0,
		.clk_path = 0,
		.mode = LCD_MODE_TV,
		.ext_lcd = ext_lcd0_config,
		.lcd_pinmux = lcd0_pinmux_ctrl,
#ifdef CONFIG_AML_LCD_EXTERN
		.ext_common = &ext_common_dft,
		.ext_conf = ext_config_dtf,
#endif
		.bl_gpio = lcd0_bl_gpio,
		.bl_pinmux = lcd0_bl_pinmux_ctrl,
	},
};

void lcd_config_bsp_init(void)
{
	struct aml_lcd_data_s *pdata = aml_lcd_get_data();

	if (pdata)
		pdata->dft_conf[0] = &lcd_dft_conf[0];
}
