// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <asm/amlogic/arch/ddr_define.h>
#include <asm/amlogic/arch/secure_apb.h>
#include <asm/amlogic/arch/timing.h>

#define DDR_FUNC_CONFIG_ENABLE_PZQ_DET_DRAM_TYPE_RETURN (0 + (1 << 20))
#define DDR_FUNC_CONFIG_AUTO_DET_DQ_PINMUX_FUNCTION	(0 + (1 << 21))
// bit 6 adc_channel bit 0-5 adc value,chan 3 value 8 is layer 2
#define DDR_ID_ACS_ADC ((3 << 6) | (8))

#define DDR_RESV_CHECK_ID_ENABLE 0Xfe
#define SAR_ADC_DDR_ID_BASE	 0
#define SAR_ADC_DDR_ID_STEP	 80

#define DDR_TIMMING_OFFSET(X)	   (unsigned int)(unsigned long)(&(((ddr_set_ps0_only_t *)(0))->X))
#define DDR_TIMMING_OFFSET_SIZE(X) sizeof(((ddr_set_ps0_only_t *)(0))->X)
#define DDR_TIMMING_TUNE_TIMMING0(DDR_ID, PARA, VALUE) \
	(DDR_ID,                                       \
	 DDR_TIMMING_OFFSET(PARA),                     \
	 VALUE,                                        \
	 DDR_TIMMING_OFFSET_SIZE(PARA),                \
	 0,                                            \
	 DDR_RESV_CHECK_ID_ENABLE)
#define DDR_TIMMING_TUNE_TIMMING1(DDR_ID, PARA, VALUE)     \
	(DDR_ID,                                           \
	 (sizeof(ddr_set_t) + (DDR_TIMMING_OFFSET(PARA))), \
	 VALUE,                                            \
	 DDR_TIMMING_OFFSET_SIZE(PARA),                    \
	 0,                                                \
	 DDR_RESV_CHECK_ID_ENABLE)

// bit24-31 define ID and size
#define DDR_ID_FROM_EFUSE	   (0Xff000000)
#define DDR_ID_FROM_ADC		   (0Xfe000000)
#define DDR_ID_FROM_GPIO_CONFIG1   (0Xfd000000)
#define DDR_ID_FROM_EFUSE_F	   (0Xff << 0)
#define DDR_ID_FROM_ADC_F	   (0Xfe << 0)
#define DDR_ID_FROM_GPIO_CONFIG1_F (0Xfd << 0)
#define DDR_ID_FROM_ADC_MULT	   (0Xfc000000)
#define DDR_ID_FROM_ADC_MULT_F	   (0Xfc << 0)
#define DDR_ID_START_MASK	   (0XFFDDCCBB)

#define DDR_ADC_CH0 (0X0 << 5)
#define DDR_ADC_CH1 (0X1 << 5)
#define DDR_ADC_CH2 (0X2 << 5)
#define DDR_ADC_CH3 (0X3 << 5)
#define DDR_ADC_CH4 (0X4 << 5)

#define DDR_ADC_VALUE0	(0X0 << 0)
#define DDR_ADC_VALUE1	(0X1 << 0)
#define DDR_ADC_VALUE2	(0X2 << 0)
#define DDR_ADC_VALUE3	(0X3 << 0)
#define DDR_ADC_VALUE4	(0X4 << 0)
#define DDR_ADC_VALUE5	(0X5 << 0)
#define DDR_ADC_VALUE6	(0X6 << 0)
#define DDR_ADC_VALUE7	(0X7 << 0)
#define DDR_ADC_VALUE8	(0X8 << 0)
#define DDR_ADC_VALUE9	(0X9 << 0)
#define DDR_ADC_VALUE10 (0Xa << 0)
#define DDR_ADC_VALUE11 (0Xb << 0)
#define DDR_ADC_VALUE12 (0Xc << 0)
#define DDR_ADC_VALUE13 (0Xd << 0)
#define DDR_ADC_VALUE14 (0Xe << 0)
#define DDR_ADC_VALUE15 (0Xf << 0)
#define V0		(0X0 << 0)
#define V1		(0X1 << 0)
#define V2		(0X2 << 0)
#define V3		(0X3 << 0)
#define V4		(0X4 << 0)
#define V5		(0X5 << 0)
#define V6		(0X6 << 0)
#define V7		(0X7 << 0)
#define V8		(0X8 << 0)
#define V9		(0X9 << 0)
#define V10		(0Xa << 0)
#define V11		(0Xb << 0)
#define V12		(0Xc << 0)

#define VX (0Xf << 0)

typedef struct ddr_para_data {
	// start from	DDR_ID_START_MASK,ddr_id;//bit0-23
	// ddr_id value,bit 24-31 ddr_id source  ,0xfe source
	// from adc ,0xfd source from gpio_default_config
	// reg_offset
	// //bit 0-15 parameter offset value,bit16-23 overrid
	// size,bit24-31 mux ddr_id source unsigned int
	// reg_offset; unsigned int	value;
	uint32_t value : 16; // bit0-15 only support data size =1byte
	// or 2bytes,no support int value
	uint32_t reg_offset : 12; // bit16-27
	uint32_t data_size  : 4;  // bit28-31 if data size =15,then
				  // will mean DDR_ID start
} ddr_para_data_t;

typedef struct ddr_para_data_start {
	uint32_t id_value : 24; // bit0-23  efuse id or ddr id
	// uint32_t	id_adc_ch : 2;//bit6-7
	uint32_t id_src_from : 8; // bit24-31 ddr id from adc or gpio
} ddr_para_data_start_t;

#define DDR_TIMMING_TUNE_STRUCT_SIZE(a) sizeof(a)

#define DDR_TIMMING_TUNE_TIMMING0_F(PARA, VALUE) \
	((DDR_TIMMING_OFFSET(PARA)) << 16) | ((DDR_TIMMING_OFFSET_SIZE(PARA)) << 28) | VALUE
#define DDR_TIMMING_TUNE_TIMMING1_F(PARA, VALUE)                          \
	((sizeof(ddr_set_ps0_only_t) + DDR_TIMMING_OFFSET(PARA)) << 16) | \
	    ((DDR_TIMMING_OFFSET_SIZE(PARA)) << 28) | (VALUE)

#define DDR_TIMMING_TUNE_START(id_src_from, id_adc_ch, id_value) \
	(id_src_from) | (id_adc_ch) | (id_value)
#define DDR_TIMMING_TUNE_ADC_MULT_START(id_value, ch0, ch1, ch2, ch3, ch4, ch5)            \
	(id_value) | (ch0) | ((ch1) << 4) | ((ch2) << 8) | ((ch3) << 12) | ((ch4) << 16) | \
	    ((ch5) << 20)
#define DDR_TIMMING_TUNE_STRUCT_SIZE(a) sizeof(a)

#if 1
uint32_t __bl2_ddr_reg_data[] __attribute__((section(".ddr_2acs_data"))) = {
	DDR_ID_START_MASK,
	// DDR_TIMMING_TUNE_ADC_MULT_START(DDR_ID_FROM_ADC_MULT, V4, VX, VX, VX, VX, VX),
	// data start
	// DDR_TIMMING_TUNE_TIMMING0_F(cfg_board_common_setting.Is2Ttiming, CONFIG_USE_DDR_2T_MODE),
	// DDR_TIMMING_TUNE_TIMMING0_F(cfg_board_SI_setting_ps.DRAMFreq, 1320),
};

////_ddr_para_2nd_setting

uint32_t __ddr_parameter_reg_index[] __attribute__((section(".ddr_2acs_index"))) = {
	0,
};
#endif

#define LPDDR4_SKT 0
#define DDR4_SKT   1
#define DDR3_SKT   0
#define CONFIG_BOARD_TIMMING

ddr_set_ps0_only_t __ddr_setting[] __attribute__ ((section(".ddr_param"))) = {
//{{0,}},
// ddr_set_ps0_only_t __attribute__ ((aligned(8))) ddr_set_t_default[] = {
#if DDR4_SKT
// s7 signoff with 3200MBPS
#define CACLU_CLK_D4 1584 // 600 //1200 //1792//600 //1200 //(1900)// (1440)//(1008)
 //ddr_set_ps0_only_t __attribute__ ((aligned(8))) ddr_set_t_default_ddr4 = {
{
	.cfg_board_common_setting.timming_magic = 0,
	.cfg_board_common_setting.timming_max_valid_configs = 1,
		//sizeof(ddr_set_t_default) / sizeof(ddr_set_ps0_only_t),
	.cfg_board_common_setting.timming_struct_version = 0,
	.cfg_board_common_setting.timming_struct_org_size =
		sizeof(ddr_set_ps0_only_t),
	.cfg_board_common_setting.timming_struct_real_size = 0,
	.cfg_board_common_setting.fast_boot = {
		0, 0, 0, 0
	},
	.cfg_board_common_setting.ddr_func =
		DDR_FUNC_CONFIG_AUTO_DET_DQ_PINMUX_FUNCTION,
	//	DDR_FUNC_CONFIG_AUTO_DET_DQ_PINMUX_FUNCTION | DDR_FUNC_CONFIG_DFE_FUNCTION,
	.cfg_board_common_setting.board_id = CONFIG_BOARD_ID_MASK,
	.cfg_board_common_setting.DramType = CONFIG_DDR_TYPE_DDR4,
	.cfg_board_common_setting.enable_lpddr4x_mode = 0,
	.cfg_board_common_setting.dram_rank_config =
	//CONFIG_DDR0_16BIT_CH0,
	CONFIG_DDR0_32BIT_RANK0_CH0,
	//.cfg_board_common_setting.dram_cs0_base_add = 0,
	//.cfg_board_common_setting.dram_cs1_base_add = 0,
	.cfg_board_common_setting.dram_ch0_size_MB =
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_23_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_23_SIZE_256_ID_OFFSET),
	.cfg_board_common_setting.dram_ch1_size_MB =
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_23_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_23_SIZE_256_ID_OFFSET),
	.cfg_board_common_setting.dram_ch0_size_MB = 0xffff,
	.cfg_board_common_setting.dram_ch1_size_MB = 0xffff,

	.cfg_board_common_setting.DisabledDbyte[0] = 0x00,
	//bit 0 -3 ch0 cs0 ,bit 4-7 ch0 cs1,
	.cfg_board_common_setting.DisabledDbyte[1] = 0x00,
	//bit 0 -3 ch1 cs0 ,bit 4-7 ch1
	.cfg_board_common_setting.dram_x4x8x16_mode = CONFIG_DRAM_MODE_X16,
	.cfg_board_common_setting.Is2Ttiming = CONFIG_USE_DDR_2T_MODE,
	.cfg_board_common_setting.log_level = 0xff,
	//.cfg_board_common_setting.log_level = 4,
	//4,//LOG_LEVEL_BASIC,
	.cfg_board_common_setting.dbi_enable = DDR_WRITE_READ_DBI_DISABLE,
	.cfg_board_common_setting.pll_ssc_mode =
		(1 << 20) | (1 << 8) | (2 << 4) | 0,
	//center_ssc_1000ppm,//SSC_DISABLE,(1 << 20) | (0 << 8) | (2 << 4) | 0,
	.cfg_board_common_setting.org_tdqs2dq = 0,
	.cfg_board_common_setting.reserve1_test = {
		0
	},

	.cfg_board_common_setting.ddr_dmc_remap =
	//DDR_DMC_REMAP_DDR4_16BIT_S1A,
	//DDR_DMC_REMAP_DDR4_16BIT_2,
	//DDR_DMC_REMAP_DDR4_32BIT,
	DDR_DMC_REMAP_PXP_DDR4_32BIT,
	//DDR_DMC_REMAP_DDR4_32BIT,
	//DDR_DMC_REMAP_DDR4_32BIT,
	// .cfg_board_common_setting.ac_pinmux = {
	//2, 3, 1, 0, 5, 4, 0, 0, 0, 0, 1, 3, 5, 2, 4, 0 },
	.cfg_board_common_setting.ddr_dqs_swap = 0,
	.cfg_board_common_setting.ddr_vddee_setting = {
		0
	},
	.cfg_board_SI_setting_ps.DRAMFreq = CACLU_CLK_D4,
	// .cfg_ddr_training_delay_ps.DRAMFreq = 600,
	// 2112,//1176,cfg_ddr_training_delay_ps
	//.cfg_ddr_training_delay_ps.PllBypassEn = 0,
	.cfg_board_SI_setting_ps.training_SequenceCtrl = 0,
	.cfg_board_SI_setting_ps.dfi_odt_config = DDR_DRAM_ODT_W_CS0_ODT0,
	.cfg_board_SI_setting_ps.dfi_odt_config = DDR_DRAM_ODT_DDR4_PARK_ENABLE,
	//DDR_DRAM_ODT_W_CS0_ODT0,
	//.cfg_ddr_training_delay_ps.dfe_offset = 0,
	.cfg_board_SI_setting_ps.vref_ac_permil = 0,
	.cfg_board_SI_setting_ps.vref_soc_data_permil = 0,
	.cfg_board_SI_setting_ps.vref_dram_data_permil = 800,
	.cfg_board_SI_setting_ps.max_core_timmming_frequency = 0,
	// 1 //real chip stk lp4
	.cfg_board_common_setting.dbi_enable = 0x00000000,
	.cfg_board_common_setting.ddr_rfc_type = DDR_RFC_TYPE_DDR4_2Gbx4,//DDR_RFC_TYPE_DDR4_2Gbx8,
	.cfg_board_common_setting.pll_ssc_mode =
		(6 << 4) | (1 << 8) | 0,
	//.cfg_board_common_setting.max_core_timmming_frequency=0x00000e10,// 3600,0x0000006a

	//.cfg_board_common_setting.lpddr4_x8_mode=0x00000000,// 0,0x00000087
	//.cfg_board_common_setting.tdqs2dq=0x00000000,// 0,0x0000008a
	//.cfg_board_common_setting.dfe_offset_value=0x00000000,// 0,0x0000008e
	//.cfg_board_common_setting.training_offset=0x00000000,// 0,0x0000008f

	#ifdef CONFIG_BOARD_TIMMING // skt lp4 board
	#define DQ_PXP_OFFSET 0
	#define TDQS2DQ_D4    ((0 * 128 * CACLU_CLK_D4) / 1000000)
	// #define BOARD_DQS_DELAY   64+32
	#define TDQSCK_ADD_DQS_D4 ((0 * 128 * CACLU_CLK_D4) / 1000000) // clk should use 64 steps
	#define CLK_DELAY_D4	  (0)
	#define PHY_TDQS2DQ_D4	  ((0 * 128 * CACLU_CLK_D4) / 1000000)
	#define WL0		  ((0 * 128 * CACLU_CLK_D4 * 2) / 1000000)
	// for 1584M 100 STEP 250ps //((480 * 128 * CACLU_CLK_D4 * 2) / 1000000)
	#endif
	.cfg_board_SI_setting_ps.clk_drv_ohm = 40,
	.cfg_board_SI_setting_ps.cs_drv_ohm = 40,
	.cfg_board_SI_setting_ps.ac_drv_ohm = 40,
	.cfg_board_SI_setting_ps.soc_data_drv_ohm_p = 40,
	.cfg_board_SI_setting_ps.soc_data_drv_ohm_n = 40,
	.cfg_board_SI_setting_ps.soc_data_odt_ohm_p = 60,
	.cfg_board_SI_setting_ps.soc_data_odt_ohm_n = 0,
	.cfg_board_SI_setting_ps.dram_data_drv_ohm = 34,
	.cfg_board_SI_setting_ps.dram_data_odt_ohm = 60,
	.cfg_board_SI_setting_ps.dram_data_wr_odt_ohm = 0,
	.cfg_board_SI_setting_ps.dram_ac_odt_ohm = 120,
	.cfg_board_SI_setting_ps.dram_drv_pull_up_cal_ohm =
		DDR_DRAM_LPDDR4_ODT_40_OHM,
	.cfg_board_SI_setting_ps.lpddr4_dram_vout_range =
		DDR_DRAM_LPDDR4_OUTPUT_1_3_VDDQ,
	//.cfg_ddr_training_delay_ps.dfe_offset = 0,
	//.cfg_board_common_setting.ac_pinmux = {
	//	3,  6,	5,  0,	1,  8,	9,  7,	13, 2,
	//	11, 14, 4,  15, 10, 26, 27, 20, 23, 22, 21,
	//	19, 16, 18, 12, 17, 24, 25, 28, 29,
	//},
	.cfg_board_common_setting.ac_pinmux = {
		14,	13,	0,	9,	8,	10,	3,	2,	4,	21,
	17,	11,	7,	6,	1,	22,	22,	26,	5,	19,	20,
	17,	18,	12,	16,	15,	24,	25,	28,	29,
	},
	.cfg_board_common_setting.ddr_dq_remap = {
		32,
		0,
		1,
		3,
		2,
		5,
		4,
		7,
		6,
		33,
		9,
		8,
		10,
		11,
		13,
		12,
		15,
		14,
		16,
		18,
		19,
		17,
		21,
		20,
		23,
		22,
		34,
		26,
		24,
		27,
		25,
		28,
		29,
		30,
		31,
		35,
	},
	.cfg_board_common_setting.ddr_dq_remap = { 0, 0, 0},
	//.cfg_board_common_setting.ddr_dq_remap= {
	// 3, 0, 2, 1, 4, 6, 5, 7,  14, 12, 13, 15,
	//.cfg_ddr_training_delay_ps.rx_offset[0] = (0 << 7) | 0x0,
	#ifdef CONFIG_BOARD_TIMMING
	.cfg_ddr_training_delay_ps.dac_offset[0] = (0 << 7) | 0x1,
	.cfg_ddr_training_delay_ps.tx_offset[0] = (0 << 7) | 0x0,
	.cfg_ddr_training_delay_ps.rx_offset[0] = (0 << 7) | 0x0,
	.cfg_ddr_training_delay_ps.reserve_para[0] = (1 << 7) | 11,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[1] = (1 << 7) | 10,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[2] = (1 << 7) | 0,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[3] = (1 << 7) | 6,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[4] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[5] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[6] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[7] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 0] = (1 << 7) | 20,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 1] = (1 << 7) | 28,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 2] = (1 << 7) | 23,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 3] = (1 << 7) | 16,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 4] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 5] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 6] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 7] = (1 << 7) | 0x10,//read dqs

// #define  AC_OFFSET  (-128) //> 1650 M use -128  or use 0
// #define  AC_OFFSET  (128)
#endif

#define AC_OFFSET (128)
#define ac_off_1t (0)
#define CK_OFFSET (128)

	.cfg_ddr_training_delay_ps.ac_trace_delay[0] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[1] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[2] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[3] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[4] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[5] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[6] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[7] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[8] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[9] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[10] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[11] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[12] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[13] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[14] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[15] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[16] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[17] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[18] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[19] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[20] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[21] = 256 + AC_OFFSET + ac_off_1t,
	//cke 128 only 1UI margin
	.cfg_ddr_training_delay_ps.ac_trace_delay[22] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[23] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[24] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[25] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[26] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[27] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[28] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[29] = 256 + AC_OFFSET + ac_off_1t,

	.cfg_ddr_training_delay_ps.ac_trace_delay[15] = 128 + CK_OFFSET + 40,//copy ch1 cs0
	.cfg_ddr_training_delay_ps.ac_trace_delay[16] = 128 + CK_OFFSET + 60,//copy ch1 cs0
	.cfg_ddr_training_delay_ps.ac_trace_delay[28] = 128 + CK_OFFSET,    //ck
	.cfg_ddr_training_delay_ps.ac_trace_delay[29] = 128 + CK_OFFSET,    //ck
	.cfg_ddr_training_delay_ps.ac_trace_delay[27] = 128 + CK_OFFSET,     //cke  real chip
	.cfg_ddr_training_delay_ps.ac_trace_delay[26] = 128 + CK_OFFSET,     //cke1
	.cfg_ddr_training_delay_ps.ac_trace_delay[17] = 128 + CK_OFFSET - 16,        //odt1
	//.cfg_ddr_training_delay_ps.ac_trace_delay[11] = 128 + CK_OFFSET,       //odt0
	//.cfg_ddr_training_delay_ps.ac_trace_delay[16] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[18] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[19] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[20] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[22] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[24] = 100 + 256 + AC_OFFSET + ac_off_1t,
	//.cfg_ddr_training_delay_ps.ac_trace_delay[25] = 100 + 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[0] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[1] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[2] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[3] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[4] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[5] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[6] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[7] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[8] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[9] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[10] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[11] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[12] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[13] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[14] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[15] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[16] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[17] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[18] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[19] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[20] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[21] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[22] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[23] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[24] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[25] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[26] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[27] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[28] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[29] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[30] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[31] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[32] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[33] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[34] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[35] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[36] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[37] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[38] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[39] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[40] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[41] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[42] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[43] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[44] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[45] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[46] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[47] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[48] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[49] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[50] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[51] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[52] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[53] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[54] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[55] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[56] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[57] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[58] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[59] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[60] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[61] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[62] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[63] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[64] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[65] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[66] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[67] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[68] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[69] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[70] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[71] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[0] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[1] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[2] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[3] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[4] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[5] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[6] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[7] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[8] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[9] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[10] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[11] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[12] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[13] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[14] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[15] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[16] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[17] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[18] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[19] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[20] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[21] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[22] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[23] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[24] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[25] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[26] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[27] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[28] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[29] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[30] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[31] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[32] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[33] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[34] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[35] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[36] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[37] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[38] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[39] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[40] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[41] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[42] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[43] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[44] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[45] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[46] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[47] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[48] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[49] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[50] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[51] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[52] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[53] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[54] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[55] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[56] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[57] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[58] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[59] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[60] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[61] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[62] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[63] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[64] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[65] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[66] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[67] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[68] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[69] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[70] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[71] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.write_dqs_delay[0] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[1] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[2] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[3] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[4] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[5] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[6] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[7] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_wck_delay[0] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[1] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[2] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[3] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[4] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[5] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[6] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[7] = 0x00000200,
	.cfg_ddr_training_delay_ps.wdq_delay[0] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[1] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[2] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[3] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[4] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[5] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[6] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[7] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[8] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[9] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[10] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[11] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[12] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[13] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[14] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[15] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[16] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[17] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[18] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[19] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[20] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[21] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[22] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[23] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[24] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[25] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[26] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[27] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[28] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[29] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[30] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[31] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[32] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[33] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[34] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[35] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[36] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[37] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[38] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[39] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[40] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[41] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[42] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[43] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[44] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[45] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[46] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[47] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[48] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[49] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[50] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[51] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[52] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[53] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[54] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[55] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[56] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[57] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[58] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[59] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[60] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[61] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[62] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[63] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[64] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[65] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[66] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[67] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[68] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[69] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[70] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[71] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[0] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[1] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[2] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[3] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[4] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[5] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[6] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[7] = 128 * 3 + AC_OFFSET,

	.cfg_ddr_training_delay_ps.read_dqs_delay[0] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[1] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[2] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[3] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[4] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[5] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[6] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[7] = 128,
	.cfg_ddr_training_delay_ps.soc_bit_vref0[0] = 0x00000000,
	//0 for auto training
	.cfg_ddr_training_delay_ps.dram_vref[0] = 0x00000000,//1d
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dq_tx[0] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[1] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[2] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[3] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[4] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[5] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[6] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[7] = 0x00000002,
	.cfg_ddr_training_delay_ps.dfi_mrl[0] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[1] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[2] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[3] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_hwtmrl = 0x00000004,
	.cfg_ddr_training_delay_ps.csr_hwtctrl = 0x00000004,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[0] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[1] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[2] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[3] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][4] = 0x00000000,

.cfg_ddr_training_delay_ps.read_dq_delay_t[0] = 0x5b,//91,0x118
.cfg_ddr_training_delay_ps.read_dq_delay_t[1] = 0x66,//102,0x11a
.cfg_ddr_training_delay_ps.read_dq_delay_t[2] = 0x50,//80,0x11c
.cfg_ddr_training_delay_ps.read_dq_delay_t[3] = 0x75,//117,0x11e
.cfg_ddr_training_delay_ps.read_dq_delay_t[4] = 0x4f,//79,0x120
.cfg_ddr_training_delay_ps.read_dq_delay_t[5] = 0x5e,//94,0x122
.cfg_ddr_training_delay_ps.read_dq_delay_t[6] = 0x5b,//91,0x124
.cfg_ddr_training_delay_ps.read_dq_delay_t[7] = 0x4a,//74,0x126
.cfg_ddr_training_delay_ps.read_dq_delay_t[8] = 0x5b,//91,0x128
.cfg_ddr_training_delay_ps.read_dq_delay_t[9] = 0x52,//82,0x12a
.cfg_ddr_training_delay_ps.read_dq_delay_t[10] = 0x5a,//90,0x12c
.cfg_ddr_training_delay_ps.read_dq_delay_t[11] = 0x55,//85,0x12e
.cfg_ddr_training_delay_ps.read_dq_delay_t[12] = 0x5c,//92,0x130
.cfg_ddr_training_delay_ps.read_dq_delay_t[13] = 0x51,//81,0x132
.cfg_ddr_training_delay_ps.read_dq_delay_t[14] = 0x45,//69,0x134
.cfg_ddr_training_delay_ps.read_dq_delay_t[15] = 0x54,//84,0x136
.cfg_ddr_training_delay_ps.read_dq_delay_t[16] = 0x4c,//76,0x138
.cfg_ddr_training_delay_ps.read_dq_delay_t[17] = 0x55,//85,0x13a
.cfg_ddr_training_delay_ps.read_dq_delay_t[18] = 0x51,//81,0x13c
.cfg_ddr_training_delay_ps.read_dq_delay_t[19] = 0x62,//98,0x13e
.cfg_ddr_training_delay_ps.read_dq_delay_t[20] = 0x4b,//75,0x140
.cfg_ddr_training_delay_ps.read_dq_delay_t[21] = 0x54,//84,0x142
.cfg_ddr_training_delay_ps.read_dq_delay_t[22] = 0x50,//80,0x144
.cfg_ddr_training_delay_ps.read_dq_delay_t[23] = 0x43,//67,0x146
.cfg_ddr_training_delay_ps.read_dq_delay_t[24] = 0x60,//96,0x148
.cfg_ddr_training_delay_ps.read_dq_delay_t[25] = 0x58,//88,0x14a
.cfg_ddr_training_delay_ps.read_dq_delay_t[26] = 0x53,//83,0x14c
.cfg_ddr_training_delay_ps.read_dq_delay_t[27] = 0x48,//72,0x14e
.cfg_ddr_training_delay_ps.read_dq_delay_t[28] = 0x5d,//93,0x150
.cfg_ddr_training_delay_ps.read_dq_delay_t[29] = 0x4a,//74,0x152
.cfg_ddr_training_delay_ps.read_dq_delay_t[30] = 0x5a,//90,0x154
.cfg_ddr_training_delay_ps.read_dq_delay_t[31] = 0x4d,//77,0x156
.cfg_ddr_training_delay_ps.read_dq_delay_t[32] = 0x4c,//76,0x158
.cfg_ddr_training_delay_ps.read_dq_delay_t[33] = 0x50,//80,0x15a
.cfg_ddr_training_delay_ps.read_dq_delay_t[34] = 0x4c,//76,0x15c
.cfg_ddr_training_delay_ps.read_dq_delay_t[35] = 0x4f,//79,0x15e
.cfg_ddr_training_delay_ps.read_dq_delay_t[36] = 0x0,//0,0x160
.cfg_ddr_training_delay_ps.read_dq_delay_t[37] = 0x0,//0,0x162
.cfg_ddr_training_delay_ps.read_dq_delay_t[38] = 0x0,//0,0x164
.cfg_ddr_training_delay_ps.read_dq_delay_t[39] = 0x0,//0,0x166
.cfg_ddr_training_delay_ps.read_dq_delay_t[40] = 0x0,//0,0x168
.cfg_ddr_training_delay_ps.read_dq_delay_t[41] = 0x0,//0,0x16a
.cfg_ddr_training_delay_ps.read_dq_delay_t[42] = 0x0,//0,0x16c
.cfg_ddr_training_delay_ps.read_dq_delay_t[43] = 0x0,//0,0x16e
.cfg_ddr_training_delay_ps.read_dq_delay_t[44] = 0x0,//0,0x170
.cfg_ddr_training_delay_ps.read_dq_delay_t[45] = 0x0,//0,0x172
.cfg_ddr_training_delay_ps.read_dq_delay_t[46] = 0x0,//0,0x174
.cfg_ddr_training_delay_ps.read_dq_delay_t[47] = 0x0,//0,0x176
.cfg_ddr_training_delay_ps.read_dq_delay_t[48] = 0x0,//0,0x178
.cfg_ddr_training_delay_ps.read_dq_delay_t[49] = 0x0,//0,0x17a
.cfg_ddr_training_delay_ps.read_dq_delay_t[50] = 0x0,//0,0x17c
.cfg_ddr_training_delay_ps.read_dq_delay_t[51] = 0x0,//0,0x17e
.cfg_ddr_training_delay_ps.read_dq_delay_t[52] = 0x0,//0,0x180
.cfg_ddr_training_delay_ps.read_dq_delay_t[53] = 0x0,//0,0x182
.cfg_ddr_training_delay_ps.read_dq_delay_t[54] = 0x0,//0,0x184
.cfg_ddr_training_delay_ps.read_dq_delay_t[55] = 0x0,//0,0x186
.cfg_ddr_training_delay_ps.read_dq_delay_t[56] = 0x0,//0,0x188
.cfg_ddr_training_delay_ps.read_dq_delay_t[57] = 0x0,//0,0x18a
.cfg_ddr_training_delay_ps.read_dq_delay_t[58] = 0x0,//0,0x18c
.cfg_ddr_training_delay_ps.read_dq_delay_t[59] = 0x0,//0,0x18e
.cfg_ddr_training_delay_ps.read_dq_delay_t[60] = 0x0,//0,0x190
.cfg_ddr_training_delay_ps.read_dq_delay_t[61] = 0x0,//0,0x192
.cfg_ddr_training_delay_ps.read_dq_delay_t[62] = 0x0,//0,0x194
.cfg_ddr_training_delay_ps.read_dq_delay_t[63] = 0x0,//0,0x196
.cfg_ddr_training_delay_ps.read_dq_delay_t[64] = 0x0,//0,0x198
.cfg_ddr_training_delay_ps.read_dq_delay_t[65] = 0x0,//0,0x19a
.cfg_ddr_training_delay_ps.read_dq_delay_t[66] = 0x0,//0,0x19c
.cfg_ddr_training_delay_ps.read_dq_delay_t[67] = 0x0,//0,0x19e
.cfg_ddr_training_delay_ps.read_dq_delay_t[68] = 0x0,//0,0x1a0
.cfg_ddr_training_delay_ps.read_dq_delay_t[69] = 0x0,//0,0x1a2
.cfg_ddr_training_delay_ps.read_dq_delay_t[70] = 0x0,//0,0x1a4
.cfg_ddr_training_delay_ps.read_dq_delay_t[71] = 0x0,//0,0x1a6
.cfg_ddr_training_delay_ps.read_dq_delay_c[0] = 0x0,//0,0x1a8
.cfg_ddr_training_delay_ps.read_dq_delay_c[1] = 0x0,//0,0x1aa
.cfg_ddr_training_delay_ps.read_dq_delay_c[2] = 0x0,//0,0x1ac
.cfg_ddr_training_delay_ps.read_dq_delay_c[3] = 0x0,//0,0x1ae
.cfg_ddr_training_delay_ps.read_dq_delay_c[4] = 0x0,//0,0x1b0
.cfg_ddr_training_delay_ps.read_dq_delay_c[5] = 0x0,//0,0x1b2
.cfg_ddr_training_delay_ps.read_dq_delay_c[6] = 0x0,//0,0x1b4
.cfg_ddr_training_delay_ps.read_dq_delay_c[7] = 0x0,//0,0x1b6
.cfg_ddr_training_delay_ps.read_dq_delay_c[8] = 0x0,//0,0x1b8
.cfg_ddr_training_delay_ps.read_dq_delay_c[9] = 0x0,//0,0x1ba
.cfg_ddr_training_delay_ps.read_dq_delay_c[10] = 0x0,//0,0x1bc
.cfg_ddr_training_delay_ps.read_dq_delay_c[11] = 0x0,//0,0x1be
.cfg_ddr_training_delay_ps.read_dq_delay_c[12] = 0x0,//0,0x1c0
.cfg_ddr_training_delay_ps.read_dq_delay_c[13] = 0x0,//0,0x1c2
.cfg_ddr_training_delay_ps.read_dq_delay_c[14] = 0x0,//0,0x1c4
.cfg_ddr_training_delay_ps.read_dq_delay_c[15] = 0x0,//0,0x1c6
.cfg_ddr_training_delay_ps.read_dq_delay_c[16] = 0x0,//0,0x1c8
.cfg_ddr_training_delay_ps.read_dq_delay_c[17] = 0x0,//0,0x1ca
.cfg_ddr_training_delay_ps.read_dq_delay_c[18] = 0x0,//0,0x1cc
.cfg_ddr_training_delay_ps.read_dq_delay_c[19] = 0x0,//0,0x1ce
.cfg_ddr_training_delay_ps.read_dq_delay_c[20] = 0x0,//0,0x1d0
.cfg_ddr_training_delay_ps.read_dq_delay_c[21] = 0x0,//0,0x1d2
.cfg_ddr_training_delay_ps.read_dq_delay_c[22] = 0x0,//0,0x1d4
.cfg_ddr_training_delay_ps.read_dq_delay_c[23] = 0x0,//0,0x1d6
.cfg_ddr_training_delay_ps.read_dq_delay_c[24] = 0x0,//0,0x1d8
.cfg_ddr_training_delay_ps.read_dq_delay_c[25] = 0x0,//0,0x1da
.cfg_ddr_training_delay_ps.read_dq_delay_c[26] = 0x0,//0,0x1dc
.cfg_ddr_training_delay_ps.read_dq_delay_c[27] = 0x0,//0,0x1de
.cfg_ddr_training_delay_ps.read_dq_delay_c[28] = 0x0,//0,0x1e0
.cfg_ddr_training_delay_ps.read_dq_delay_c[29] = 0x0,//0,0x1e2
.cfg_ddr_training_delay_ps.read_dq_delay_c[30] = 0x0,//0,0x1e4
.cfg_ddr_training_delay_ps.read_dq_delay_c[31] = 0x0,//0,0x1e6
.cfg_ddr_training_delay_ps.read_dq_delay_c[32] = 0x0,//0,0x1e8
.cfg_ddr_training_delay_ps.read_dq_delay_c[33] = 0x0,//0,0x1ea
.cfg_ddr_training_delay_ps.read_dq_delay_c[34] = 0x0,//0,0x1ec
.cfg_ddr_training_delay_ps.read_dq_delay_c[35] = 0x0,//0,0x1ee
.cfg_ddr_training_delay_ps.read_dq_delay_c[36] = 0x0,//0,0x1f0
.cfg_ddr_training_delay_ps.read_dq_delay_c[37] = 0x0,//0,0x1f2
.cfg_ddr_training_delay_ps.read_dq_delay_c[38] = 0x0,//0,0x1f4
.cfg_ddr_training_delay_ps.read_dq_delay_c[39] = 0x0,//0,0x1f6
.cfg_ddr_training_delay_ps.read_dq_delay_c[40] = 0x0,//0,0x1f8
.cfg_ddr_training_delay_ps.read_dq_delay_c[41] = 0x0,//0,0x1fa
.cfg_ddr_training_delay_ps.read_dq_delay_c[42] = 0x0,//0,0x1fc
.cfg_ddr_training_delay_ps.read_dq_delay_c[43] = 0x0,//0,0x1fe
.cfg_ddr_training_delay_ps.read_dq_delay_c[44] = 0x0,//0,0x200
.cfg_ddr_training_delay_ps.read_dq_delay_c[45] = 0x0,//0,0x202
.cfg_ddr_training_delay_ps.read_dq_delay_c[46] = 0x0,//0,0x204
.cfg_ddr_training_delay_ps.read_dq_delay_c[47] = 0x0,//0,0x206
.cfg_ddr_training_delay_ps.read_dq_delay_c[48] = 0x0,//0,0x208
.cfg_ddr_training_delay_ps.read_dq_delay_c[49] = 0x0,//0,0x20a
.cfg_ddr_training_delay_ps.read_dq_delay_c[50] = 0x0,//0,0x20c
.cfg_ddr_training_delay_ps.read_dq_delay_c[51] = 0x0,//0,0x20e
.cfg_ddr_training_delay_ps.read_dq_delay_c[52] = 0x0,//0,0x210
.cfg_ddr_training_delay_ps.read_dq_delay_c[53] = 0x0,//0,0x212
.cfg_ddr_training_delay_ps.read_dq_delay_c[54] = 0x0,//0,0x214
.cfg_ddr_training_delay_ps.read_dq_delay_c[55] = 0x0,//0,0x216
.cfg_ddr_training_delay_ps.read_dq_delay_c[56] = 0x0,//0,0x218
.cfg_ddr_training_delay_ps.read_dq_delay_c[57] = 0x0,//0,0x21a
.cfg_ddr_training_delay_ps.read_dq_delay_c[58] = 0x0,//0,0x21c
.cfg_ddr_training_delay_ps.read_dq_delay_c[59] = 0x0,//0,0x21e
.cfg_ddr_training_delay_ps.read_dq_delay_c[60] = 0x0,//0,0x220
.cfg_ddr_training_delay_ps.read_dq_delay_c[61] = 0x0,//0,0x222
.cfg_ddr_training_delay_ps.read_dq_delay_c[62] = 0x0,//0,0x224
.cfg_ddr_training_delay_ps.read_dq_delay_c[63] = 0x0,//0,0x226
.cfg_ddr_training_delay_ps.read_dq_delay_c[64] = 0x0,//0,0x228
.cfg_ddr_training_delay_ps.read_dq_delay_c[65] = 0x0,//0,0x22a
.cfg_ddr_training_delay_ps.read_dq_delay_c[66] = 0x0,//0,0x22c
.cfg_ddr_training_delay_ps.read_dq_delay_c[67] = 0x0,//0,0x22e
.cfg_ddr_training_delay_ps.read_dq_delay_c[68] = 0x0,//0,0x230
.cfg_ddr_training_delay_ps.read_dq_delay_c[69] = 0x0,//0,0x232
.cfg_ddr_training_delay_ps.read_dq_delay_c[70] = 0x0,//0,0x234
.cfg_ddr_training_delay_ps.read_dq_delay_c[71] = 0x0,//0,0x236
.cfg_ddr_training_delay_ps.read_dqs_delay[0] = 0x6c,//108,0x238
.cfg_ddr_training_delay_ps.read_dqs_delay[1] = 0x6a,//106,0x23a
.cfg_ddr_training_delay_ps.read_dqs_delay[2] = 0x69,//105,0x23c
.cfg_ddr_training_delay_ps.read_dqs_delay[3] = 0x70,//112,0x23e
.cfg_ddr_training_delay_ps.read_dqs_delay[4] = 0x70,//112,0x240
.cfg_ddr_training_delay_ps.read_dqs_delay[5] = 0x70,//112,0x242
.cfg_ddr_training_delay_ps.read_dqs_delay[6] = 0x70,//112,0x244
.cfg_ddr_training_delay_ps.read_dqs_delay[7] = 0x70,//112,0x246
.cfg_ddr_training_delay_ps.write_dqs_delay[0] = 0x66,//102,0x248
.cfg_ddr_training_delay_ps.write_dqs_delay[1] = 0x6c,//108,0x24a
.cfg_ddr_training_delay_ps.write_dqs_delay[2] = 0x65,//101,0x24c
.cfg_ddr_training_delay_ps.write_dqs_delay[3] = 0x66,//102,0x24e
.cfg_ddr_training_delay_ps.write_dqs_delay[4] = 0x78,//120,0x250
.cfg_ddr_training_delay_ps.write_dqs_delay[5] = 0x78,//120,0x252
.cfg_ddr_training_delay_ps.write_dqs_delay[6] = 0x78,//120,0x254
.cfg_ddr_training_delay_ps.write_dqs_delay[7] = 0x78,//120,0x256
.cfg_ddr_training_delay_ps.write_wck_delay[0] = 0x0,//0,0x258
.cfg_ddr_training_delay_ps.write_wck_delay[1] = 0x0,//0,0x25a
.cfg_ddr_training_delay_ps.write_wck_delay[2] = 0x0,//0,0x25c
.cfg_ddr_training_delay_ps.write_wck_delay[3] = 0x0,//0,0x25e
.cfg_ddr_training_delay_ps.write_wck_delay[4] = 0x0,//0,0x260
.cfg_ddr_training_delay_ps.write_wck_delay[5] = 0x0,//0,0x262
.cfg_ddr_training_delay_ps.write_wck_delay[6] = 0x0,//0,0x264
.cfg_ddr_training_delay_ps.write_wck_delay[7] = 0x0,//0,0x266
.cfg_ddr_training_delay_ps.wdq_delay[0] = 0xd0,//208,0x268
.cfg_ddr_training_delay_ps.wdq_delay[1] = 0xd8,//216,0x26a
.cfg_ddr_training_delay_ps.wdq_delay[2] = 0xc6,//198,0x26c
.cfg_ddr_training_delay_ps.wdq_delay[3] = 0xd9,//217,0x26e
.cfg_ddr_training_delay_ps.wdq_delay[4] = 0xcc,//204,0x270
.cfg_ddr_training_delay_ps.wdq_delay[5] = 0xce,//206,0x272
.cfg_ddr_training_delay_ps.wdq_delay[6] = 0xd5,//213,0x274
.cfg_ddr_training_delay_ps.wdq_delay[7] = 0xc8,//200,0x276
.cfg_ddr_training_delay_ps.wdq_delay[8] = 0xd9,//217,0x278
.cfg_ddr_training_delay_ps.wdq_delay[9] = 0xd0,//208,0x27a
.cfg_ddr_training_delay_ps.wdq_delay[10] = 0xd6,//214,0x27c
.cfg_ddr_training_delay_ps.wdq_delay[11] = 0xc7,//199,0x27e
.cfg_ddr_training_delay_ps.wdq_delay[12] = 0xd5,//213,0x280
.cfg_ddr_training_delay_ps.wdq_delay[13] = 0xc7,//199,0x282
.cfg_ddr_training_delay_ps.wdq_delay[14] = 0xc8,//200,0x284
.cfg_ddr_training_delay_ps.wdq_delay[15] = 0xd6,//214,0x286
.cfg_ddr_training_delay_ps.wdq_delay[16] = 0xcb,//203,0x288
.cfg_ddr_training_delay_ps.wdq_delay[17] = 0xde,//222,0x28a
.cfg_ddr_training_delay_ps.wdq_delay[18] = 0xc2,//194,0x28c
.cfg_ddr_training_delay_ps.wdq_delay[19] = 0xcf,//207,0x28e
.cfg_ddr_training_delay_ps.wdq_delay[20] = 0xb7,//183,0x290
.cfg_ddr_training_delay_ps.wdq_delay[21] = 0xcd,//205,0x292
.cfg_ddr_training_delay_ps.wdq_delay[22] = 0xbf,//191,0x294
.cfg_ddr_training_delay_ps.wdq_delay[23] = 0xbd,//189,0x296
.cfg_ddr_training_delay_ps.wdq_delay[24] = 0xd0,//208,0x298
.cfg_ddr_training_delay_ps.wdq_delay[25] = 0xbf,//191,0x29a
.cfg_ddr_training_delay_ps.wdq_delay[26] = 0xc4,//196,0x29c
.cfg_ddr_training_delay_ps.wdq_delay[27] = 0xc0,//192,0x29e
.cfg_ddr_training_delay_ps.wdq_delay[28] = 0xd4,//212,0x2a0
.cfg_ddr_training_delay_ps.wdq_delay[29] = 0xc1,//193,0x2a2
.cfg_ddr_training_delay_ps.wdq_delay[30] = 0xd6,//214,0x2a4
.cfg_ddr_training_delay_ps.wdq_delay[31] = 0xbe,//190,0x2a6
.cfg_ddr_training_delay_ps.wdq_delay[32] = 0xbd,//189,0x2a8
.cfg_ddr_training_delay_ps.wdq_delay[33] = 0xd3,//211,0x2aa
.cfg_ddr_training_delay_ps.wdq_delay[34] = 0xb7,//183,0x2ac
.cfg_ddr_training_delay_ps.wdq_delay[35] = 0xc6,//198,0x2ae
.cfg_ddr_training_delay_ps.wdq_delay[36] = 0x0,//0,0x2b0
.cfg_ddr_training_delay_ps.wdq_delay[37] = 0x0,//0,0x2b2
.cfg_ddr_training_delay_ps.wdq_delay[38] = 0x0,//0,0x2b4
.cfg_ddr_training_delay_ps.wdq_delay[39] = 0x0,//0,0x2b6
.cfg_ddr_training_delay_ps.wdq_delay[40] = 0x0,//0,0x2b8
.cfg_ddr_training_delay_ps.wdq_delay[41] = 0x0,//0,0x2ba
.cfg_ddr_training_delay_ps.wdq_delay[42] = 0x0,//0,0x2bc
.cfg_ddr_training_delay_ps.wdq_delay[43] = 0x0,//0,0x2be
.cfg_ddr_training_delay_ps.wdq_delay[44] = 0x0,//0,0x2c0
.cfg_ddr_training_delay_ps.wdq_delay[45] = 0x0,//0,0x2c2
.cfg_ddr_training_delay_ps.wdq_delay[46] = 0x0,//0,0x2c4
.cfg_ddr_training_delay_ps.wdq_delay[47] = 0x0,//0,0x2c6
.cfg_ddr_training_delay_ps.wdq_delay[48] = 0x0,//0,0x2c8
.cfg_ddr_training_delay_ps.wdq_delay[49] = 0x0,//0,0x2ca
.cfg_ddr_training_delay_ps.wdq_delay[50] = 0x0,//0,0x2cc
.cfg_ddr_training_delay_ps.wdq_delay[51] = 0x0,//0,0x2ce
.cfg_ddr_training_delay_ps.wdq_delay[52] = 0x0,//0,0x2d0
.cfg_ddr_training_delay_ps.wdq_delay[53] = 0x0,//0,0x2d2
.cfg_ddr_training_delay_ps.wdq_delay[54] = 0x0,//0,0x2d4
.cfg_ddr_training_delay_ps.wdq_delay[55] = 0x0,//0,0x2d6
.cfg_ddr_training_delay_ps.wdq_delay[56] = 0x0,//0,0x2d8
.cfg_ddr_training_delay_ps.wdq_delay[57] = 0x0,//0,0x2da
.cfg_ddr_training_delay_ps.wdq_delay[58] = 0x0,//0,0x2dc
.cfg_ddr_training_delay_ps.wdq_delay[59] = 0x0,//0,0x2de
.cfg_ddr_training_delay_ps.wdq_delay[60] = 0x0,//0,0x2e0
.cfg_ddr_training_delay_ps.wdq_delay[61] = 0x0,//0,0x2e2
.cfg_ddr_training_delay_ps.wdq_delay[62] = 0x0,//0,0x2e4
.cfg_ddr_training_delay_ps.wdq_delay[63] = 0x0,//0,0x2e6
.cfg_ddr_training_delay_ps.wdq_delay[64] = 0x0,//0,0x2e8
.cfg_ddr_training_delay_ps.wdq_delay[65] = 0x0,//0,0x2ea
.cfg_ddr_training_delay_ps.wdq_delay[66] = 0x0,//0,0x2ec
.cfg_ddr_training_delay_ps.wdq_delay[67] = 0x0,//0,0x2ee
.cfg_ddr_training_delay_ps.wdq_delay[68] = 0x0,//0,0x2f0
.cfg_ddr_training_delay_ps.wdq_delay[69] = 0x0,//0,0x2f2
.cfg_ddr_training_delay_ps.wdq_delay[70] = 0x0,//0,0x2f4
.cfg_ddr_training_delay_ps.wdq_delay[71] = 0x0,//0,0x2f6
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[0] = 0x1e9,//489,0x2f8
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[1] = 0x1d5,//469,0x2fa
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[2] = 0x1d5,//469,0x2fc
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[3] = 0x1d8,//472,0x2fe
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[4] = 0x0,//0,0x300
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[5] = 0x0,//0,0x302
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[6] = 0x0,//0,0x304
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[7] = 0x0,//0,0x306
},
{
	.cfg_board_common_setting.timming_magic = 0,
	.cfg_board_common_setting.timming_max_valid_configs = 1,
		//sizeof(ddr_set_t_default) / sizeof(ddr_set_ps0_only_t),
	.cfg_board_common_setting.timming_struct_version = 0,
	.cfg_board_common_setting.timming_struct_org_size =
		sizeof(ddr_set_ps0_only_t),
	.cfg_board_common_setting.timming_struct_real_size = 0,
	.cfg_board_common_setting.fast_boot = {
		0, 0, 0, 0
	},
	.cfg_board_common_setting.ddr_func =
		DDR_FUNC_CONFIG_AUTO_DET_DQ_PINMUX_FUNCTION,
	//	DDR_FUNC_CONFIG_AUTO_DET_DQ_PINMUX_FUNCTION | DDR_FUNC_CONFIG_DFE_FUNCTION,
	.cfg_board_common_setting.board_id = CONFIG_BOARD_ID_MASK,
	.cfg_board_common_setting.DramType = CONFIG_DDR_TYPE_DDR4,
	.cfg_board_common_setting.enable_lpddr4x_mode = 0,
	.cfg_board_common_setting.dram_rank_config =
	//CONFIG_DDR0_16BIT_CH0,
	CONFIG_DDR0_32BIT_RANK0_CH0,
	//.cfg_board_common_setting.dram_cs0_base_add = 0,
	//.cfg_board_common_setting.dram_cs1_base_add = 0,
	.cfg_board_common_setting.dram_ch0_size_MB =
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_23_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_23_SIZE_256_ID_OFFSET),
	.cfg_board_common_setting.dram_ch1_size_MB =
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX4 << CONFIG_CS0_BYTE_23_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_01_SIZE_256_ID_OFFSET) +
		(DRAM_SIZE_ID_256MBX0 << CONFIG_CS1_BYTE_23_SIZE_256_ID_OFFSET),
	.cfg_board_common_setting.dram_ch0_size_MB = 0xffff,
	.cfg_board_common_setting.dram_ch1_size_MB = 0xffff,

	.cfg_board_common_setting.DisabledDbyte[0] = 0x00,
	//bit 0 -3 ch0 cs0 ,bit 4-7 ch0 cs1,
	.cfg_board_common_setting.DisabledDbyte[1] = 0x00,
	//bit 0 -3 ch1 cs0 ,bit 4-7 ch1
	.cfg_board_common_setting.dram_x4x8x16_mode = CONFIG_DRAM_MODE_X16,
	.cfg_board_common_setting.Is2Ttiming = CONFIG_USE_DDR_2T_MODE,
	.cfg_board_common_setting.log_level = 0xff,
	//.cfg_board_common_setting.log_level = 4,
	//4,//LOG_LEVEL_BASIC,
	.cfg_board_common_setting.dbi_enable = DDR_WRITE_READ_DBI_DISABLE,
	.cfg_board_common_setting.pll_ssc_mode =
		(1 << 20) | (1 << 8) | (2 << 4) | 0,
	//center_ssc_1000ppm,//SSC_DISABLE,(1 << 20) | (0 << 8) | (2 << 4) | 0,
	.cfg_board_common_setting.org_tdqs2dq = 0,
	.cfg_board_common_setting.reserve1_test = {
		0
	},
	.cfg_board_common_setting.ddr_dmc_remap =
	//DDR_DMC_REMAP_DDR4_16BIT_S1A,
	//DDR_DMC_REMAP_DDR4_16BIT_2,
	//DDR_DMC_REMAP_DDR4_32BIT,
	DDR_DMC_REMAP_PXP_DDR4_32BIT,
	//DDR_DMC_REMAP_DDR4_32BIT,
	//DDR_DMC_REMAP_DDR4_32BIT,
	// .cfg_board_common_setting.ac_pinmux = {
	//2, 3, 1, 0, 5, 4, 0, 0, 0, 0, 1, 3, 5, 2, 4, 0 },
	.cfg_board_common_setting.ddr_dqs_swap = 0,
	.cfg_board_common_setting.ddr_vddee_setting = {
		0
	},
	.cfg_board_SI_setting_ps.DRAMFreq = CACLU_CLK_D4,
	// .cfg_ddr_training_delay_ps.DRAMFreq = 600,
	// 2112,//1176,cfg_ddr_training_delay_ps
	//.cfg_ddr_training_delay_ps.PllBypassEn = 0,
	.cfg_board_SI_setting_ps.training_SequenceCtrl = 0,
	.cfg_board_SI_setting_ps.dfi_odt_config = DDR_DRAM_ODT_W_CS0_ODT0,
	.cfg_board_SI_setting_ps.dfi_odt_config = DDR_DRAM_ODT_DDR4_PARK_ENABLE,
	//DDR_DRAM_ODT_W_CS0_ODT0,
	//.cfg_ddr_training_delay_ps.dfe_offset = 0,
	.cfg_board_SI_setting_ps.vref_ac_permil = 0,
	.cfg_board_SI_setting_ps.vref_soc_data_permil = 0,
	.cfg_board_SI_setting_ps.vref_dram_data_permil = 800,
	.cfg_board_SI_setting_ps.max_core_timmming_frequency = 0,
	// 1 //real chip stk lp4
	.cfg_board_common_setting.dbi_enable = 0x00000000,
	.cfg_board_common_setting.ddr_rfc_type = DDR_RFC_TYPE_DDR4_2Gbx4,//DDR_RFC_TYPE_DDR4_2Gbx8,
	.cfg_board_common_setting.pll_ssc_mode =
		(6 << 4) | (1 << 8) | 0,
//.cfg_board_common_setting.max_core_timmming_frequency=0x00000e10,// 3600,0x0000006a

//.cfg_board_common_setting.lpddr4_x8_mode=0x00000000,// 0,0x00000087
//.cfg_board_common_setting.tdqs2dq=0x00000000,// 0,0x0000008a
//.cfg_board_common_setting.dfe_offset_value=0x00000000,// 0,0x0000008e
//.cfg_board_common_setting.training_offset=0x00000000,// 0,0x0000008f

#ifdef CONFIG_BOARD_TIMMING // skt lp4 board
#define DQ_PXP_OFFSET 0
#define TDQS2DQ_D4    ((0 * 128 * CACLU_CLK_D4) / 1000000)
// #define BOARD_DQS_DELAY   64+32
#define TDQSCK_ADD_DQS_D4 ((0 * 128 * CACLU_CLK_D4) / 1000000) // clk should use 64 steps
#define CLK_DELAY_D4	  (0)
#define PHY_TDQS2DQ_D4	  ((0 * 128 * CACLU_CLK_D4) / 1000000)
#define WL0		  ((0 * 128 * CACLU_CLK_D4 * 2) / 1000000)
// for 1584M 100 STEP 250ps //((480 * 128 * CACLU_CLK_D4 * 2) / 1000000)
#endif
	.cfg_board_SI_setting_ps.clk_drv_ohm = 60,
	.cfg_board_SI_setting_ps.cs_drv_ohm = 60,
	.cfg_board_SI_setting_ps.ac_drv_ohm = 60,
	.cfg_board_SI_setting_ps.soc_data_drv_ohm_p = 40,
	.cfg_board_SI_setting_ps.soc_data_drv_ohm_n = 40,
	.cfg_board_SI_setting_ps.soc_data_odt_ohm_p = 60,
	.cfg_board_SI_setting_ps.soc_data_odt_ohm_n = 0,
	.cfg_board_SI_setting_ps.dram_data_drv_ohm = 34,
	.cfg_board_SI_setting_ps.dram_data_odt_ohm = 60,
	.cfg_board_SI_setting_ps.dram_data_wr_odt_ohm = 0,
	.cfg_board_SI_setting_ps.dram_ac_odt_ohm = 120,
	.cfg_board_SI_setting_ps.dram_drv_pull_up_cal_ohm =
		DDR_DRAM_LPDDR4_ODT_40_OHM,
	.cfg_board_SI_setting_ps.lpddr4_dram_vout_range =
		DDR_DRAM_LPDDR4_OUTPUT_1_3_VDDQ,
	//.cfg_ddr_training_delay_ps.dfe_offset = 0,
	.cfg_board_common_setting.ac_pinmux = {
		14,	13,	0,	9,	8,	10,	3,	2,	4,	21,
	17,	11,	7,	6,	1,	22,	22,	26,	5,	19,	20,
	17,	18,	12,	16,	15,	24,	25,	28,	29,
	},
	.cfg_board_common_setting.ddr_dq_remap = {
		32,
		0,
		1,
		3,
		2,
		5,
		4,
		7,
		6,
		33,
		9,
		8,
		10,
		11,
		13,
		12,
		15,
		14,
		16,
		18,
		19,
		17,
		21,
		20,
		23,
		22,
		34,
		26,
		24,
		27,
		25,
		28,
		29,
		30,
		31,
		35,
	},
	.cfg_board_common_setting.ddr_dq_remap = { 0, 0, 0},
//.cfg_board_common_setting.ddr_dq_remap= {
// 3, 0, 2, 1, 4, 6, 5, 7,  14, 12, 13, 15,
//.cfg_ddr_training_delay_ps.rx_offset[0] = (0 << 7) | 0x0,
#ifdef CONFIG_BOARD_TIMMING
	.cfg_ddr_training_delay_ps.dac_offset[0] = (0 << 7) | 0x1,
	.cfg_ddr_training_delay_ps.tx_offset[0] = (0 << 7) | 0x0,
	.cfg_ddr_training_delay_ps.rx_offset[0] = (0 << 7) | 0x0,
	.cfg_ddr_training_delay_ps.reserve_para[0] = (1 << 7) | 8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[1] = (1 << 7) | 10,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[2] = (1 << 7) | 8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[3] = (1 << 7) | 8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[4] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[5] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[6] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[7] = (1 << 7) | 0x8,     //write dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 0] = (1 << 7) | 14,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 1] = (1 << 7) | 18,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 2] = (1 << 7) | 22,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 3] = (1 << 7) | 16,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 4] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 5] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 6] = (1 << 7) | 0x10,//read dqs
	.cfg_ddr_training_delay_ps.reserve_para[8 + 7] = (1 << 7) | 0x10,//read dqs
#endif

#define AC_OFFSET (128)
#define ac_off_1t (0)
#define CK_OFFSET (128)
	.cfg_ddr_training_delay_ps.ac_trace_delay[0] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[1] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[2] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[3] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[4] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[5] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[6] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[7] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[8] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[9] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[10] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[11] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[12] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[13] = 256 + AC_OFFSET + ac_off_1t - 64,
	.cfg_ddr_training_delay_ps.ac_trace_delay[14] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[15] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[16] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[17] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[18] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[19] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[20] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[21] = 256 + AC_OFFSET + ac_off_1t,
	//cke 128 only 1UI margin
	.cfg_ddr_training_delay_ps.ac_trace_delay[22] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[23] = 256 + AC_OFFSET + ac_off_1t + 40,
	.cfg_ddr_training_delay_ps.ac_trace_delay[24] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[25] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[26] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[27] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[28] = 256 + AC_OFFSET + ac_off_1t,
	.cfg_ddr_training_delay_ps.ac_trace_delay[29] = 256 + AC_OFFSET + ac_off_1t,

	.cfg_ddr_training_delay_ps.ac_trace_delay[15] = 128 + CK_OFFSET + 106,//copy ch1 cs0
	.cfg_ddr_training_delay_ps.ac_trace_delay[16] = 128 + CK_OFFSET + 120,//copy ch1 cs0
	.cfg_ddr_training_delay_ps.ac_trace_delay[28] = 128 + CK_OFFSET,    //ck
	.cfg_ddr_training_delay_ps.ac_trace_delay[29] = 128 + CK_OFFSET,    //ck
	.cfg_ddr_training_delay_ps.ac_trace_delay[27] = 128 + CK_OFFSET - 64,     //cke  real chip
	.cfg_ddr_training_delay_ps.ac_trace_delay[26] = 128 + CK_OFFSET - 64,     //cke1
	.cfg_ddr_training_delay_ps.ac_trace_delay[17] = 128 + CK_OFFSET,        //odt1
	//.cfg_ddr_training_delay_ps.ac_trace_delay[11] = 128 + CK_OFFSET,       //odt0
	.cfg_ddr_training_delay_ps.ac_trace_delay[8] = 256 + AC_OFFSET + ac_off_1t - 40,
	.cfg_ddr_training_delay_ps.ac_trace_delay[9] = 256 + AC_OFFSET + ac_off_1t - 60,
	.cfg_ddr_training_delay_ps.ac_trace_delay[10] = 256 + AC_OFFSET + ac_off_1t - 60,

	.cfg_ddr_training_delay_ps.read_dq_delay_t[0] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[1] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[2] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[3] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[4] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[5] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[6] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[7] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[8] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[9] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[10] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[11] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[12] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[13] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[14] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[15] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[16] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[17] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[18] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[19] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[20] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[21] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[22] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[23] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[24] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[25] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[26] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[27] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[28] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[29] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[30] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[31] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[32] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[33] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[34] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[35] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[36] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[37] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[38] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[39] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[40] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[41] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[42] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[43] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[44] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[45] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[46] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[47] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[48] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[49] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[50] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[51] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[52] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[53] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[54] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[55] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[56] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[57] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[58] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[59] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[60] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[61] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[62] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[63] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[64] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[65] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[66] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[67] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[68] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[69] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[70] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_t[71] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[0] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[1] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[2] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[3] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[4] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[5] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[6] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[7] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[8] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[9] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[10] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[11] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[12] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[13] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[14] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[15] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[16] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[17] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[18] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[19] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[20] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[21] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[22] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[23] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[24] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[25] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[26] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[27] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[28] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[29] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[30] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[31] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[32] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[33] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[34] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[35] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[36] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[37] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[38] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[39] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[40] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[41] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[42] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[43] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[44] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[45] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[46] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[47] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[48] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[49] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[50] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[51] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[52] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[53] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[54] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[55] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[56] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[57] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[58] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[59] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[60] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[61] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[62] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[63] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[64] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[65] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[66] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[67] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[68] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[69] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[70] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.read_dq_delay_c[71] = 64 + DQ_PXP_OFFSET,
	.cfg_ddr_training_delay_ps.write_dqs_delay[0] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[1] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[2] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[3] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[4] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[5] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[6] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_dqs_delay[7] = 160 + CK_OFFSET + WL0,
	.cfg_ddr_training_delay_ps.write_wck_delay[0] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[1] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[2] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[3] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[4] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[5] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[6] = 0x00000200,
	.cfg_ddr_training_delay_ps.write_wck_delay[7] = 0x00000200,
	.cfg_ddr_training_delay_ps.wdq_delay[0] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[1] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[2] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[3] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[4] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[5] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[6] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[7] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[8] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[9] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[10] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[11] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[12] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[13] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[14] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[15] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[16] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[17] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[18] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[19] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[20] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[21] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[22] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[23] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[24] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[25] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[26] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[27] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[28] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[29] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[30] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[31] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[32] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[33] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[34] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[35] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[36] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[37] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[38] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[39] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[40] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[41] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[42] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[43] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[44] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[45] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[46] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[47] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[48] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[49] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[50] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[51] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[52] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[53] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[54] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[55] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[56] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[57] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[58] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[59] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[60] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[61] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[62] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[63] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[64] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[65] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[66] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[67] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[68] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[69] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[70] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,
	.cfg_ddr_training_delay_ps.wdq_delay[71] = 64 + CK_OFFSET + TDQS2DQ_D4 + WL0,

	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[0] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[1] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[2] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[3] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[4] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[5] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[6] = 128 * 3 + AC_OFFSET,
	.cfg_ddr_training_delay_ps.read_dqs_gate_delay[7] = 128 * 3 + AC_OFFSET,

	.cfg_ddr_training_delay_ps.read_dqs_delay[0] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[1] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[2] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[3] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[4] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[5] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[6] = 128,
	.cfg_ddr_training_delay_ps.read_dqs_delay[7] = 128,
	.cfg_ddr_training_delay_ps.soc_bit_vref0[0] = 0x00000000,
	//0 for auto training
	.cfg_ddr_training_delay_ps.dram_vref[0] = 0x00000000,//1d
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_t[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_tx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_wck_rx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[0] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[1] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[2] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[3] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[4] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[5] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[6] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dqs_tx_c[7] = 0x00000266,
	.cfg_ddr_training_delay_ps.dca_dq_tx[0] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[1] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[2] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[3] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[4] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[5] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[6] = 0x00000002,
	.cfg_ddr_training_delay_ps.dca_dq_tx[7] = 0x00000002,
	.cfg_ddr_training_delay_ps.dfi_mrl[0] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[1] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[2] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_mrl[3] = 0x00000004,
	.cfg_ddr_training_delay_ps.dfi_hwtmrl = 0x00000004,
	.cfg_ddr_training_delay_ps.csr_hwtctrl = 0x00000004,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[0] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[1] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[2] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg0[3] = 0x00000026,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.pptdqscnttg1[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg0[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[0] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[1] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[2] = 0x00000000,
	.cfg_ddr_training_delay_ps.PptWck2DqoCntTg1[3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][0] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][1] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][2] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][3] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[0][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[1][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[2][4] = 0x00000000,
	.cfg_ddr_training_delay_ps.RxReplicaPhase[3][4] = 0x00000000,

.cfg_ddr_training_delay_ps.read_dq_delay_t[0] = 0x4d,//77,0x118
.cfg_ddr_training_delay_ps.read_dq_delay_t[1] = 0x4e,//78,0x11a
.cfg_ddr_training_delay_ps.read_dq_delay_t[2] = 0x51,//81,0x11c
.cfg_ddr_training_delay_ps.read_dq_delay_t[3] = 0x57,//87,0x11e
.cfg_ddr_training_delay_ps.read_dq_delay_t[4] = 0x42,//66,0x120
.cfg_ddr_training_delay_ps.read_dq_delay_t[5] = 0x44,//68,0x122
.cfg_ddr_training_delay_ps.read_dq_delay_t[6] = 0x53,//83,0x124
.cfg_ddr_training_delay_ps.read_dq_delay_t[7] = 0x40,//64,0x126
.cfg_ddr_training_delay_ps.read_dq_delay_t[8] = 0x5c,//92,0x128
.cfg_ddr_training_delay_ps.read_dq_delay_t[9] = 0x37,//55,0x12a
.cfg_ddr_training_delay_ps.read_dq_delay_t[10] = 0x5a,//90,0x12c
.cfg_ddr_training_delay_ps.read_dq_delay_t[11] = 0x46,//70,0x12e
.cfg_ddr_training_delay_ps.read_dq_delay_t[12] = 0x4a,//74,0x130
.cfg_ddr_training_delay_ps.read_dq_delay_t[13] = 0x39,//57,0x132
.cfg_ddr_training_delay_ps.read_dq_delay_t[14] = 0x47,//71,0x134
.cfg_ddr_training_delay_ps.read_dq_delay_t[15] = 0x4c,//76,0x136
.cfg_ddr_training_delay_ps.read_dq_delay_t[16] = 0x3c,//60,0x138
.cfg_ddr_training_delay_ps.read_dq_delay_t[17] = 0x51,//81,0x13a
.cfg_ddr_training_delay_ps.read_dq_delay_t[18] = 0x3c,//60,0x13c
.cfg_ddr_training_delay_ps.read_dq_delay_t[19] = 0x4e,//78,0x13e
.cfg_ddr_training_delay_ps.read_dq_delay_t[20] = 0x44,//68,0x140
.cfg_ddr_training_delay_ps.read_dq_delay_t[21] = 0x52,//82,0x142
.cfg_ddr_training_delay_ps.read_dq_delay_t[22] = 0x3a,//58,0x144
.cfg_ddr_training_delay_ps.read_dq_delay_t[23] = 0x36,//54,0x146
.cfg_ddr_training_delay_ps.read_dq_delay_t[24] = 0x51,//81,0x148
.cfg_ddr_training_delay_ps.read_dq_delay_t[25] = 0x39,//57,0x14a
.cfg_ddr_training_delay_ps.read_dq_delay_t[26] = 0x43,//67,0x14c
.cfg_ddr_training_delay_ps.read_dq_delay_t[27] = 0x54,//84,0x14e
.cfg_ddr_training_delay_ps.read_dq_delay_t[28] = 0x58,//88,0x150
.cfg_ddr_training_delay_ps.read_dq_delay_t[29] = 0x4e,//78,0x152
.cfg_ddr_training_delay_ps.read_dq_delay_t[30] = 0x55,//85,0x154
.cfg_ddr_training_delay_ps.read_dq_delay_t[31] = 0x52,//82,0x156
.cfg_ddr_training_delay_ps.read_dq_delay_t[32] = 0x43,//67,0x158
.cfg_ddr_training_delay_ps.read_dq_delay_t[33] = 0x4b,//75,0x15a
.cfg_ddr_training_delay_ps.read_dq_delay_t[34] = 0x4d,//77,0x15c
.cfg_ddr_training_delay_ps.read_dq_delay_t[35] = 0x47,//71,0x15e
.cfg_ddr_training_delay_ps.read_dq_delay_t[36] = 0x0,//0,0x160
.cfg_ddr_training_delay_ps.read_dq_delay_t[37] = 0x0,//0,0x162
.cfg_ddr_training_delay_ps.read_dq_delay_t[38] = 0x0,//0,0x164
.cfg_ddr_training_delay_ps.read_dq_delay_t[39] = 0x0,//0,0x166
.cfg_ddr_training_delay_ps.read_dq_delay_t[40] = 0x0,//0,0x168
.cfg_ddr_training_delay_ps.read_dq_delay_t[41] = 0x0,//0,0x16a
.cfg_ddr_training_delay_ps.read_dq_delay_t[42] = 0x0,//0,0x16c
.cfg_ddr_training_delay_ps.read_dq_delay_t[43] = 0x0,//0,0x16e
.cfg_ddr_training_delay_ps.read_dq_delay_t[44] = 0x0,//0,0x170
.cfg_ddr_training_delay_ps.read_dq_delay_t[45] = 0x0,//0,0x172
.cfg_ddr_training_delay_ps.read_dq_delay_t[46] = 0x0,//0,0x174
.cfg_ddr_training_delay_ps.read_dq_delay_t[47] = 0x0,//0,0x176
.cfg_ddr_training_delay_ps.read_dq_delay_t[48] = 0x0,//0,0x178
.cfg_ddr_training_delay_ps.read_dq_delay_t[49] = 0x0,//0,0x17a
.cfg_ddr_training_delay_ps.read_dq_delay_t[50] = 0x0,//0,0x17c
.cfg_ddr_training_delay_ps.read_dq_delay_t[51] = 0x0,//0,0x17e
.cfg_ddr_training_delay_ps.read_dq_delay_t[52] = 0x0,//0,0x180
.cfg_ddr_training_delay_ps.read_dq_delay_t[53] = 0x0,//0,0x182
.cfg_ddr_training_delay_ps.read_dq_delay_t[54] = 0x0,//0,0x184
.cfg_ddr_training_delay_ps.read_dq_delay_t[55] = 0x0,//0,0x186
.cfg_ddr_training_delay_ps.read_dq_delay_t[56] = 0x0,//0,0x188
.cfg_ddr_training_delay_ps.read_dq_delay_t[57] = 0x0,//0,0x18a
.cfg_ddr_training_delay_ps.read_dq_delay_t[58] = 0x0,//0,0x18c
.cfg_ddr_training_delay_ps.read_dq_delay_t[59] = 0x0,//0,0x18e
.cfg_ddr_training_delay_ps.read_dq_delay_t[60] = 0x0,//0,0x190
.cfg_ddr_training_delay_ps.read_dq_delay_t[61] = 0x0,//0,0x192
.cfg_ddr_training_delay_ps.read_dq_delay_t[62] = 0x0,//0,0x194
.cfg_ddr_training_delay_ps.read_dq_delay_t[63] = 0x0,//0,0x196
.cfg_ddr_training_delay_ps.read_dq_delay_t[64] = 0x0,//0,0x198
.cfg_ddr_training_delay_ps.read_dq_delay_t[65] = 0x0,//0,0x19a
.cfg_ddr_training_delay_ps.read_dq_delay_t[66] = 0x0,//0,0x19c
.cfg_ddr_training_delay_ps.read_dq_delay_t[67] = 0x0,//0,0x19e
.cfg_ddr_training_delay_ps.read_dq_delay_t[68] = 0x0,//0,0x1a0
.cfg_ddr_training_delay_ps.read_dq_delay_t[69] = 0x0,//0,0x1a2
.cfg_ddr_training_delay_ps.read_dq_delay_t[70] = 0x0,//0,0x1a4
.cfg_ddr_training_delay_ps.read_dq_delay_t[71] = 0x0,//0,0x1a6
.cfg_ddr_training_delay_ps.read_dqs_delay[0] = 0x72,//114,0x238
.cfg_ddr_training_delay_ps.read_dqs_delay[1] = 0x70,//112,0x23a
.cfg_ddr_training_delay_ps.read_dqs_delay[2] = 0x6e,//110,0x23c
.cfg_ddr_training_delay_ps.read_dqs_delay[3] = 0x70,//112,0x23e
.cfg_ddr_training_delay_ps.read_dqs_delay[4] = 0x70,//112,0x240
.cfg_ddr_training_delay_ps.read_dqs_delay[5] = 0x70,//112,0x242
.cfg_ddr_training_delay_ps.read_dqs_delay[6] = 0x70,//112,0x244
.cfg_ddr_training_delay_ps.read_dqs_delay[7] = 0x70,//112,0x246
.cfg_ddr_training_delay_ps.write_dqs_delay[0] = 0x156,//342,0x248
.cfg_ddr_training_delay_ps.write_dqs_delay[1] = 0x158,//344,0x24a
.cfg_ddr_training_delay_ps.write_dqs_delay[2] = 0x15a,//346,0x24c
.cfg_ddr_training_delay_ps.write_dqs_delay[3] = 0x153,//339,0x24e
.cfg_ddr_training_delay_ps.write_dqs_delay[4] = 0x118,//280,0x250
.cfg_ddr_training_delay_ps.write_dqs_delay[5] = 0x118,//280,0x252
.cfg_ddr_training_delay_ps.write_dqs_delay[6] = 0x118,//280,0x254
.cfg_ddr_training_delay_ps.write_dqs_delay[7] = 0x118,//280,0x256
.cfg_ddr_training_delay_ps.write_wck_delay[0] = 0x0,//0,0x258
.cfg_ddr_training_delay_ps.write_wck_delay[1] = 0x0,//0,0x25a
.cfg_ddr_training_delay_ps.write_wck_delay[2] = 0x0,//0,0x25c
.cfg_ddr_training_delay_ps.write_wck_delay[3] = 0x0,//0,0x25e
.cfg_ddr_training_delay_ps.write_wck_delay[4] = 0x0,//0,0x260
.cfg_ddr_training_delay_ps.write_wck_delay[5] = 0x0,//0,0x262
.cfg_ddr_training_delay_ps.write_wck_delay[6] = 0x0,//0,0x264
.cfg_ddr_training_delay_ps.write_wck_delay[7] = 0x0,//0,0x266
.cfg_ddr_training_delay_ps.wdq_delay[0] = 0x1b8,//440,0x268
.cfg_ddr_training_delay_ps.wdq_delay[1] = 0x1b8,//440,0x26a
.cfg_ddr_training_delay_ps.wdq_delay[2] = 0x1be,//446,0x26c
.cfg_ddr_training_delay_ps.wdq_delay[3] = 0x1c0,//448,0x26e
.cfg_ddr_training_delay_ps.wdq_delay[4] = 0x1b5,//437,0x270
.cfg_ddr_training_delay_ps.wdq_delay[5] = 0x1af,//431,0x272
.cfg_ddr_training_delay_ps.wdq_delay[6] = 0x1b8,//440,0x274
.cfg_ddr_training_delay_ps.wdq_delay[7] = 0x1af,//431,0x276
.cfg_ddr_training_delay_ps.wdq_delay[8] = 0x1c2,//450,0x278
.cfg_ddr_training_delay_ps.wdq_delay[9] = 0x1a0,//416,0x27a
.cfg_ddr_training_delay_ps.wdq_delay[10] = 0x1c4,//452,0x27c
.cfg_ddr_training_delay_ps.wdq_delay[11] = 0x1ad,//429,0x27e
.cfg_ddr_training_delay_ps.wdq_delay[12] = 0x1b0,//432,0x280
.cfg_ddr_training_delay_ps.wdq_delay[13] = 0x1a3,//419,0x282
.cfg_ddr_training_delay_ps.wdq_delay[14] = 0x1a9,//425,0x284
.cfg_ddr_training_delay_ps.wdq_delay[15] = 0x1b6,//438,0x286
.cfg_ddr_training_delay_ps.wdq_delay[16] = 0x1a2,//418,0x288
.cfg_ddr_training_delay_ps.wdq_delay[17] = 0x1b2,//434,0x28a
.cfg_ddr_training_delay_ps.wdq_delay[18] = 0x1a4,//420,0x28c
.cfg_ddr_training_delay_ps.wdq_delay[19] = 0x1b7,//439,0x28e
.cfg_ddr_training_delay_ps.wdq_delay[20] = 0x1ab,//427,0x290
.cfg_ddr_training_delay_ps.wdq_delay[21] = 0x1b9,//441,0x292
.cfg_ddr_training_delay_ps.wdq_delay[22] = 0x1a0,//416,0x294
.cfg_ddr_training_delay_ps.wdq_delay[23] = 0x1a8,//424,0x296
.cfg_ddr_training_delay_ps.wdq_delay[24] = 0x1b7,//439,0x298
.cfg_ddr_training_delay_ps.wdq_delay[25] = 0x1a4,//420,0x29a
.cfg_ddr_training_delay_ps.wdq_delay[26] = 0x1ac,//428,0x29c
.cfg_ddr_training_delay_ps.wdq_delay[27] = 0x1bf,//447,0x29e
.cfg_ddr_training_delay_ps.wdq_delay[28] = 0x1be,//446,0x2a0
.cfg_ddr_training_delay_ps.wdq_delay[29] = 0x1b8,//440,0x2a2
.cfg_ddr_training_delay_ps.wdq_delay[30] = 0x1bd,//445,0x2a4
.cfg_ddr_training_delay_ps.wdq_delay[31] = 0x1bf,//447,0x2a6
.cfg_ddr_training_delay_ps.wdq_delay[32] = 0x1b7,//439,0x2a8
.cfg_ddr_training_delay_ps.wdq_delay[33] = 0x1b5,//437,0x2aa
.cfg_ddr_training_delay_ps.wdq_delay[34] = 0x1ae,//430,0x2ac
.cfg_ddr_training_delay_ps.wdq_delay[35] = 0x1b4,//436,0x2ae
.cfg_ddr_training_delay_ps.wdq_delay[36] = 0x0,//0,0x2b0
.cfg_ddr_training_delay_ps.wdq_delay[37] = 0x0,//0,0x2b2
.cfg_ddr_training_delay_ps.wdq_delay[38] = 0x0,//0,0x2b4
.cfg_ddr_training_delay_ps.wdq_delay[39] = 0x0,//0,0x2b6
.cfg_ddr_training_delay_ps.wdq_delay[40] = 0x0,//0,0x2b8
.cfg_ddr_training_delay_ps.wdq_delay[41] = 0x0,//0,0x2ba
.cfg_ddr_training_delay_ps.wdq_delay[42] = 0x0,//0,0x2bc
.cfg_ddr_training_delay_ps.wdq_delay[43] = 0x0,//0,0x2be
.cfg_ddr_training_delay_ps.wdq_delay[44] = 0x0,//0,0x2c0
.cfg_ddr_training_delay_ps.wdq_delay[45] = 0x0,//0,0x2c2
.cfg_ddr_training_delay_ps.wdq_delay[46] = 0x0,//0,0x2c4
.cfg_ddr_training_delay_ps.wdq_delay[47] = 0x0,//0,0x2c6
.cfg_ddr_training_delay_ps.wdq_delay[48] = 0x0,//0,0x2c8
.cfg_ddr_training_delay_ps.wdq_delay[49] = 0x0,//0,0x2ca
.cfg_ddr_training_delay_ps.wdq_delay[50] = 0x0,//0,0x2cc
.cfg_ddr_training_delay_ps.wdq_delay[51] = 0x0,//0,0x2ce
.cfg_ddr_training_delay_ps.wdq_delay[52] = 0x0,//0,0x2d0
.cfg_ddr_training_delay_ps.wdq_delay[53] = 0x0,//0,0x2d2
.cfg_ddr_training_delay_ps.wdq_delay[54] = 0x0,//0,0x2d4
.cfg_ddr_training_delay_ps.wdq_delay[55] = 0x0,//0,0x2d6
.cfg_ddr_training_delay_ps.wdq_delay[56] = 0x0,//0,0x2d8
.cfg_ddr_training_delay_ps.wdq_delay[57] = 0x0,//0,0x2da
.cfg_ddr_training_delay_ps.wdq_delay[58] = 0x0,//0,0x2dc
.cfg_ddr_training_delay_ps.wdq_delay[59] = 0x0,//0,0x2de
.cfg_ddr_training_delay_ps.wdq_delay[60] = 0x0,//0,0x2e0
.cfg_ddr_training_delay_ps.wdq_delay[61] = 0x0,//0,0x2e2
.cfg_ddr_training_delay_ps.wdq_delay[62] = 0x0,//0,0x2e4
.cfg_ddr_training_delay_ps.wdq_delay[63] = 0x0,//0,0x2e6
.cfg_ddr_training_delay_ps.wdq_delay[64] = 0x0,//0,0x2e8
.cfg_ddr_training_delay_ps.wdq_delay[65] = 0x0,//0,0x2ea
.cfg_ddr_training_delay_ps.wdq_delay[66] = 0x0,//0,0x2ec
.cfg_ddr_training_delay_ps.wdq_delay[67] = 0x0,//0,0x2ee
.cfg_ddr_training_delay_ps.wdq_delay[68] = 0x0,//0,0x2f0
.cfg_ddr_training_delay_ps.wdq_delay[69] = 0x0,//0,0x2f2
.cfg_ddr_training_delay_ps.wdq_delay[70] = 0x0,//0,0x2f4
.cfg_ddr_training_delay_ps.wdq_delay[71] = 0x0,//0,0x2f6
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[0] = 0x3ac,//940,0x2f8
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[1] = 0x3a2,//930,0x2fa
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[2] = 0x3a1,//929,0x2fc
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[3] = 0x3a6,//934,0x2fe
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[4] = 0x0,//0,0x300
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[5] = 0x0,//0,0x302
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[6] = 0x0,//3021,0x304
.cfg_ddr_training_delay_ps.read_dqs_gate_delay[7] = 0x0,//0,0x306

},
//};
#endif

};
