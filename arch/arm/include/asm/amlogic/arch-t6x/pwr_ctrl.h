/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PWRC_H_
#define _PWRC_H_

#define PWR_ON    1
#define PWR_OFF   0

#define PDID_MDC_VPU    0
#define PDID_MDC_SYS    1
#define PDID_MDC_CPU    2
#define PDID_DOS_HCODEC 3
#define PDID_DOS_HEVC   4
#define PDID_GE2D       5
#define PDID_HDMIRX     6
#define PDID_VPU_HDMI   7
#define PDID_AMFC       8
#define PDID_DMC0       9
#define PDID_DMC1       10
#define PDID_DEMOD      11

#define PM_MAX          12

#define PM_ETH		PM_MAX

unsigned long viu_init_psci_smc(unsigned long flag);
unsigned long pwr_ctrl_psci_smc(unsigned int power_domain, bool power_control);
unsigned long pwr_ctrl_status_psci_smc(unsigned int power_domain);
#endif
