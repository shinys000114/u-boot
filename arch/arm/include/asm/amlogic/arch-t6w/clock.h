/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __T6W_H
#define __T6W_H

// REG_BASE:  REGISTER_BASE_ADDR = 0xfe000000
// -----------------------------------------------
#define T6W_CLKCTRL_NAND_CLK_CTRL                      (0x005a  << 2)
#define T6W_CLKCTRL_SPICC_CLK_CTRL                     (0x005d  << 2)
#define T6W_CLKCTRL_SAR_CLK_CTRL                       (0x005f  << 2)
#define T6W_CLKCTRL_VPU_CLK_CTRL                       (0x003a  << 2)
#define T6W_CLKCTRL_VAPBCLK_CTRL                       (0x003f  << 2)
//========================================================================

// REG_BASE:  REGISTER_BASE_ADDR = 0xfe008000
// -----------------------------------------------
#define T6W_ANACTRL_SYS0PLL_CTRL0                       ((0x00 << 2) + 0x8000)
#define T6W_ANACTRL_SYS0PLL_CTRL1                       ((0x01 << 2) + 0x8000)
#define T6W_ANACTRL_SYS0PLL_STS                         ((0x04 << 2) + 0x8000)
#define T6W_ANACTRL_FIXPLL_CTRL0                        ((0x10 << 2) + 0x8000)
#define T6W_ANACTRL_FIXPLL_CTRL1                        ((0x11 << 2) + 0x8000)
#define T6W_ANACTRL_FIXPLL_STS                          ((0x17 << 2) + 0x8000)
#define T6W_ANACTRL_GP0PLL_CTRL0                        ((0x20 << 2) + 0x8000)
#define T6W_ANACTRL_GP0PLL_CTRL1                        ((0x21 << 2) + 0x8000)
#define T6W_ANACTRL_GP0PLL_CTRL2                        ((0x22 << 2) + 0x8000)
#define T6W_ANACTRL_GP0PLL_CTRL3                        ((0x23 << 2) + 0x8000)
#define T6W_ANACTRL_GP0PLL_STS                          ((0x27 << 2) + 0x8000)
#define T6W_ANACTRL_GP1PLL_CTRL0                        ((0x30 << 2) + 0x8000)
#define T6W_ANACTRL_GP1PLL_CTRL1                        ((0x31 << 2) + 0x8000)
#define T6W_ANACTRL_GP1PLL_CTRL2                        ((0x32 << 2) + 0x8000)
#define T6W_ANACTRL_GP1PLL_CTRL3                        ((0x33 << 2) + 0x8000)
#define T6W_ANACTRL_GP1PLL_STS                          ((0x37 << 2) + 0x8000)
#define T6W_ANACTRL_GP2PLL_CTRL0                        ((0x40 << 2) + 0x8000)
#define T6W_ANACTRL_GP2PLL_CTRL1                        ((0x41 << 2) + 0x8000)
#define T6W_ANACTRL_GP2PLL_CTRL2                        ((0x42 << 2) + 0x8000)
#define T6W_ANACTRL_GP2PLL_CTRL3                        ((0x43 << 2) + 0x8000)
#define T6W_ANACTRL_GP2PLL_STS                          ((0x47 << 2) + 0x8000)

#include <dt-bindings/amlogic/clock/t6w-clkc.h>
#define NR_CLKS				(CLKID_END_BASE)

#endif /* __T6W_H */
