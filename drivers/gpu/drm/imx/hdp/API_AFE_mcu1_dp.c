/******************************************************************************
 *
 * Copyright (C) 2016-2017 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 *
 * Copyright 2018-2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ******************************************************************************
 *
 * API_AFE_mcu1_dp.c
 *
 ******************************************************************************
 */

#include <linux/delay.h>
#include "API_AFE_mcu1_dp.h"
#include "../../../../mxc/hdp/all.h"

static void AFE_WriteReg(state_struct *state, ENUM_AFE_LINK_RATE link_rate,
			 unsigned int addr,
			 unsigned int val1_6,
			 unsigned int val2_1,
			 unsigned int val2_4,
			 unsigned int val2_7,
			 unsigned int val3_2,
			 unsigned int val4_3,
			 unsigned int val5_4)
{
	switch (link_rate) {
	case AFE_LINK_RATE_1_6:
		Afe_write(state, addr, val1_6);
		break;
	case AFE_LINK_RATE_2_1:
		Afe_write(state, addr, val2_1);
		break;
	case AFE_LINK_RATE_2_4:
		Afe_write(state, addr, val2_4);
		break;
	case AFE_LINK_RATE_2_7:
		Afe_write(state, addr, val2_7);
		break;
	case AFE_LINK_RATE_3_2:
		Afe_write(state, addr, val3_2);
		break;
	case AFE_LINK_RATE_4_3:
		Afe_write(state, addr, val4_3);
		break;
	case AFE_LINK_RATE_5_4:
		Afe_write(state, addr, val5_4);
		break;
	case AFE_LINK_RATE_8_1:	/* Not used in MCU1 */
	default:
		pr_err("Warning. Unsupported Link Rate!\n");
		break;
	}
}

void phy_cfg_24mhz(state_struct *state, int num_lanes)
{
	int k;

	for (k = 0; k < num_lanes; k++) {
		Afe_write(state, XCVR_DIAG_LANE_FCM_EN_MGN_TMR | (k << 9), 0x0090);
		Afe_write(state, TX_RCVDET_EN_TMR | (k << 9), 0x0960);
		Afe_write(state, TX_RCVDET_ST_TMR | (k << 9), 0x0030);
	}
}

/* Valid for 24 MHz only */
void phy_cfg_dp_pll0(state_struct *state, int num_lanes, ENUM_AFE_LINK_RATE link_rate)
{
	int k;
	volatile u16 rdata;

	rdata = Afe_read(state, PHY_HDP_CLK_CTL);
	rdata = rdata & 0x00FF;

	switch (link_rate) {
	case AFE_LINK_RATE_1_6:
	case AFE_LINK_RATE_2_1:
	case AFE_LINK_RATE_2_4:
	case AFE_LINK_RATE_2_7:
		rdata = rdata | 0x2400;
		break;
	case AFE_LINK_RATE_3_2:
	case AFE_LINK_RATE_4_3:
	case AFE_LINK_RATE_5_4:
		rdata = rdata | 0x1200;
		break;
	case AFE_LINK_RATE_8_1:	/* nNot used in MCU1 */
	default:
		pr_err("Warning. Unsupported Link Rate!\n");
		break;
	}

	Afe_write(state, PHY_HDP_CLK_CTL, rdata);
	rdata = Afe_read(state, CMN_DIAG_HSCLK_SEL);
	rdata = rdata & 0xFFCC;
	switch (link_rate) {
	case AFE_LINK_RATE_1_6:
	case AFE_LINK_RATE_2_1:
	case AFE_LINK_RATE_2_4:
	case AFE_LINK_RATE_2_7:
		rdata = rdata | 0x0011;
		break;
	case AFE_LINK_RATE_3_2:
	case AFE_LINK_RATE_4_3:
	case AFE_LINK_RATE_5_4:
		rdata = rdata | 0x0000;
		break;
	case AFE_LINK_RATE_8_1:	/* Not used in MCU1 */
	default:
		pr_err("Warning. Unsupported Link Rate!\n");
		break;
	}
	Afe_write(state, CMN_DIAG_HSCLK_SEL, rdata);
	for (k = 0; k < num_lanes; k = k + 1) {
		rdata = Afe_read(state, (XCVR_DIAG_HSCLK_SEL | (k << 9)));
		rdata = rdata & 0xCFFF;
		switch (link_rate) {
		case AFE_LINK_RATE_1_6:
		case AFE_LINK_RATE_2_1:
		case AFE_LINK_RATE_2_4:
		case AFE_LINK_RATE_2_7:
			rdata = rdata | 0x1000;
			break;
		case AFE_LINK_RATE_3_2:
		case AFE_LINK_RATE_4_3:
		case AFE_LINK_RATE_5_4:
			rdata = rdata | 0x0000;
			break;
		case AFE_LINK_RATE_8_1:	/* Not used in MCU1 */
		default:
			pr_err("Warning. Unsupported Link Rate!\n");
			break;
		}
		Afe_write(state, (XCVR_DIAG_HSCLK_SEL | (k << 9)), rdata);
	}
	Afe_write(state, CMN_PLL0_VCOCAL_INIT_TMR, 0x00F0);
	Afe_write(state, CMN_PLL0_VCOCAL_ITER_TMR, 0x0018);
	AFE_WriteReg(state, link_rate, CMN_PLL0_VCOCAL_START, 0x3061, 0x3092, 0x30B3, 0x30D0, 0x3061, 0x3092, 0x30D0);
	AFE_WriteReg(state, link_rate, CMN_PLL0_INTDIV, 0x0086, 0x00B3, 0x00CA, 0x00E0, 0x0086, 0x00B3, 0x00E0);
	AFE_WriteReg(state, link_rate, CMN_PLL0_FRACDIV, 0xF917, 0xF6C7, 0x75A1, 0xF479, 0xF917, 0xF6C7, 0xF479);
	AFE_WriteReg(state, link_rate, CMN_PLL0_HIGH_THR, 0x0022, 0x002D, 0x0033, 0x0038, 0x0022, 0x002D, 0x0038);
#ifdef SSC_ON_INIT
	/* Following register writes enable SSC on PHY's initialization */
	AFE_WriteReg(state, link_rate, CMN_PLL0_SS_CTRL1, 0x0140, 0x01AB, 0x01E0, 0x0204, 0x0140, 0x01AB, 0x0204);
	Afe_write(state, CMN_PLL0_SS_CTRL2, 0x7F03);
#endif
	Afe_write(state, CMN_PLL0_DSM_DIAG, 0x0020);
	AFE_WriteReg(state, link_rate, CMN_PLLSM0_USER_DEF_CTRL, 0x0000, 0x1000, 0x1000, 0x1000, 0x0000, 0x1000, 0x1000);
	Afe_write(state, CMN_DIAG_PLL0_OVRD, 0x0000);
	Afe_write(state, CMN_DIAG_PLL0_FBH_OVRD, 0x0000);
	Afe_write(state, CMN_DIAG_PLL0_FBL_OVRD, 0x0000);
	AFE_WriteReg(state, link_rate, CMN_DIAG_PLL0_V2I_TUNE, 0x0006, 0x0007, 0x0007, 0x0007, 0x0006, 0x0007, 0x0007);
	AFE_WriteReg(state, link_rate, CMN_DIAG_PLL0_CP_TUNE, 0x0026, 0x0029, 0x0029, 0x0029, 0x0026, 0x0029, 0x0029);
	Afe_write(state, CMN_DIAG_PLL0_LF_PROG, 0x0008);
	Afe_write(state, CMN_DIAG_PLL0_PTATIS_TUNE1, 0x008C);
	Afe_write(state, CMN_DIAG_PLL0_PTATIS_TUNE2, 0x002E);

	for (k = 0; k < num_lanes; k = k + 1) {
		rdata = Afe_read(state, (XCVR_DIAG_PLLDRC_CTRL | (k << 9)));
		rdata = rdata & 0x8FFF;
		switch (link_rate) {
		case AFE_LINK_RATE_1_6:
		case AFE_LINK_RATE_2_1:
		case AFE_LINK_RATE_2_4:
		case AFE_LINK_RATE_2_7:
			rdata = rdata | 0x2000;
			break;
		case AFE_LINK_RATE_3_2:
		case AFE_LINK_RATE_4_3:
		case AFE_LINK_RATE_5_4:
			rdata = rdata | 0x1000;
			break;
		case AFE_LINK_RATE_8_1:	/* Not used in MCU1 */
		default:
			pr_err("Warning. Unsupported Link Rate!\n");
			break;
		}
		Afe_write(state, (XCVR_DIAG_PLLDRC_CTRL | (k << 9)), rdata);
	}
}

void phy_cfg_dp_ln(state_struct *state, int num_lanes)
{
	int k;
	u16 rdata;

	for (k = 0; k < num_lanes; k = k + 1) {
		Afe_write(state, (XCVR_PSM_RCTRL | (k << 9)), 0xBEFC);
		if (state->edp == 0) {
			Afe_write(state, (TX_PSC_A0 | (k << 9)), 0x6799);
			Afe_write(state, (TX_PSC_A1 | (k << 9)), 0x6798);
			Afe_write(state, (TX_PSC_A2 | (k << 9)), 0x0098);
			Afe_write(state, (TX_PSC_A3 | (k << 9)), 0x0098);
		} else {
			Afe_write(state, (TX_PSC_A0 | (k << 9)), 0x279B);
			Afe_write(state, (TX_PSC_A1 | (k << 9)), 0x2798);
			Afe_write(state, (TX_PSC_A2 | (k << 9)), 0x0098);
			Afe_write(state, (TX_PSC_A3 | (k << 9)), 0x0098);
			rdata = Afe_read(state, (TX_DIAG_TX_DRV | (k << 9)));
			rdata &= 0x0600;	/* keep bits related to programmable boost */
			rdata |= 0x00C0;
			Afe_write(state, (TX_DIAG_TX_DRV | (k << 9)), rdata);
		}
	}
}

u16 aux_cal_cfg(state_struct *state, u16 prev_calib_code)
{
	u16 txpu_calib_code;
	u16 txpd_calib_code;
	u16 txpu_adj_calib_code;
	u16 txpd_adj_calib_code;
	u16 new_calib_code;
	u16 rdata;

	txpu_calib_code = Afe_read(state, CMN_TXPUCAL_CTRL);
	txpd_calib_code = Afe_read(state, CMN_TXPDCAL_CTRL);
	txpu_adj_calib_code = Afe_read(state, CMN_TXPU_ADJ_CTRL);
	txpd_adj_calib_code = Afe_read(state, CMN_TXPD_ADJ_CTRL);

	new_calib_code = ((txpu_calib_code + txpd_calib_code) / 2)
		+ txpu_adj_calib_code + txpd_adj_calib_code;

	if (new_calib_code != prev_calib_code) {
		rdata = Afe_read(state, TX_ANA_CTRL_REG_1);
		rdata &= 0xDFFF;
		Afe_write(state, TX_ANA_CTRL_REG_1, rdata);
		Afe_write(state, TX_DIG_CTRL_REG_2, new_calib_code);
		mdelay(10);
		rdata |= 0x2000;
		Afe_write(state, TX_ANA_CTRL_REG_1, rdata);
		udelay(150);
	}

	return new_calib_code;
}

void aux_cfg(state_struct *state)
{
	volatile u16 rdata;

	Afe_write(state, TX_DIG_CTRL_REG_2, 36);

	Afe_write(state, TX_ANA_CTRL_REG_2, 0x0100);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_2, 0x0300);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_3, 0x0000);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_1, 0x2008);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_1, 0x2018);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_1, 0xA018);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_2, 0x030C);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_5, 0x0000);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_4, 0x1001);
	udelay(150);
	Afe_write(state, TX_ANA_CTRL_REG_1, 0xA098);
	mdelay(5);
	Afe_write(state, TX_ANA_CTRL_REG_1, 0xA198);
	mdelay(5);
	Afe_write(state, TX_ANA_CTRL_REG_2, 0x030D);
	mdelay(5);
	Afe_write(state, TX_ANA_CTRL_REG_2, 0x030F);
	mdelay(5);

	rdata = Afe_read(state, TX_ANA_CTRL_REG_1);
	pr_info("TX_ANA_CTRL_REG_1 %x)\n", rdata);
	rdata = Afe_read(state, TX_ANA_CTRL_REG_2);
	pr_info("TX_ANA_CTRL_REG_2 %x)\n", rdata);
	rdata = Afe_read(state, TX_ANA_CTRL_REG_3);
	pr_info("TX_ANA_CTRL_REG_3 %x)\n", rdata);
	rdata = Afe_read(state, TX_ANA_CTRL_REG_4);
	pr_info("TX_ANA_CTRL_REG_4 %x)\n", rdata);
	rdata = Afe_read(state, TX_ANA_CTRL_REG_5);
	pr_info("TX_ANA_CTRL_REG_5 %x)\n", rdata);
}

void AFE_init(state_struct *state, int num_lanes, ENUM_AFE_LINK_RATE link_rate)
{
	volatile u16 val;

	if (AFE_check_rate_supported(link_rate) == 0) {
		pr_info("AFE_init() *E: Selected link rate not supported: 0x%x\n", link_rate);
		return;
	}

#ifdef CONFIG_EMU_PXP
	volatile unsigned short temp;

	/* 1.    Assert
	   a.    Iddq_Enable_pad  = 1’b0       // reg12[27] */

	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<27));
	cdn_apb_write(0xc000c<<2, temp);

	/* b.    Pll_ats_eanble = 1’b0 //reg12[29] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<29));
	cdn_apb_write(0xc000c<<2, temp);

	/* 2.    Assert all Reset pads to Low
	   a.    Tap_trst_n = 1’b0 //reg12[3] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<3));
	cdn_apb_write(0xc000c<<2, temp);

	/* b.    Apb_preset_n = 1’b0 //reg12[2] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<2));
	cdn_apb_write(0xc000c<<2, temp);

	/* c.    Chip_rst_b = 1’b0 reg12[0] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<0));
	cdn_apb_write(0xc000c<<2, temp);

	/* d.    Phy_reset_n = 1’b0 //reg12[1] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp & (~(1<<1));
	cdn_apb_write(0xc000c<<2, temp);

	/* 3.    Assert Chipmode[2:0] = 3’b000 reg11[2:0] */
	cdn_apb_read(0xc000b<<2, &temp);
	temp = temp & (~(7<<0));
	cdn_apb_write(0xc000b<<2, temp);

	/* 4.    Assert Sel_Tap[1:0]= 2’b00  //reg11[4:3];*/
	cdn_apb_read(0xc000b<<2, &temp);
	temp = temp & (~(3<<3));
	cdn_apb_write(0xc000b<<2, temp);

	/* 5.    Assert
	   a.    After 100ns, Apb_preset_n = 1’b1 //reg12[2] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp | ((1<<2));
	cdn_apb_write(0xc000c<<2, temp);


	/* b.    After 100ns, Chip_reset_b = 1’b1 // Reg12[0] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp | ((1<<0));
	cdn_apb_write(0xc000c<<2, temp);


	/* c.    After 100ns, Tap_trst_n = 1’b1 //reg12[3] */
	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp | ((1<<3));
	cdn_apb_write(0xc000c<<2, temp);
#endif

	val = Afe_read(state, PHY_PMA_CMN_CTRL1);
	val = val & 0xFFF7;
	val = val | 0x0008;
	Afe_write(state, PHY_PMA_CMN_CTRL1, val);

	Afe_write(state, CMN_DIAG_PLL0_TEST_MODE, 0x0020);
	Afe_write(state, CMN_PSM_CLK_CTRL, 0x0016);

	phy_cfg_24mhz(state, num_lanes);

	phy_cfg_dp_pll0(state, num_lanes, link_rate);

	val = Afe_read(state, PHY_PMA_CMN_CTRL1);
	val = val & 0xFF8F;
	val = val | 0x0030;
	Afe_write(state, PHY_PMA_CMN_CTRL1, val);

	if (state->edp != 0)
		Afe_write(state, CMN_DIAG_CAL_CTRL, 0x0001);

	phy_cfg_dp_ln(state, num_lanes);

	/* Configure PHY in A2 Mode */
	Afe_write(state, PHY_HDP_MODE_CTRL, 0x0004);

#ifdef CONFIG_EMU_PXP
	/* Assert  Phy_reset_n = 1’b1 */

	cdn_apb_read(0xc000c<<2, &temp);
	temp = temp | ((1<<1));
	cdn_apb_write(0xc000c<<2, temp);
#endif
}

void AFE_power(state_struct *state, int num_lanes,
	       ENUM_AFE_LINK_RATE link_rate)
{
	static u16 prev_calib_code;
	volatile u16 val;

	if (AFE_check_rate_supported(link_rate) == 0) {
		pr_info("%s *E: Selected link rate not supported: 0x%x\n", __func__, link_rate);
		return;
	}

	Afe_write(state, TX_DIAG_ACYA_0, 1);
	Afe_write(state, TX_DIAG_ACYA_1, 1);
	Afe_write(state, TX_DIAG_ACYA_2, 1);
	Afe_write(state, TX_DIAG_ACYA_3, 1);

	Afe_write(state, TXDA_CYA_AUXDA_CYA, 1);

	/* Wait for A2 ACK (PHY_HDP_MODE_CTL [6] = 1’b1) */
	do {
		val = Afe_read(state, PHY_HDP_MODE_CTRL);
		val = val >> 6;
	} while ((val & 1) == 0);

	/* to check if PLL has locked (bit 6) */
	val = Afe_read(state, PHY_PMA_CMN_CTRL2);
	val = val >> 6;

	if ((val & 1) == 0)
		pr_err("ERROR: PLL is not locked\n");
	else
		pr_info("PHY_PMA_CMN_CTRL2 = %x\n", val);

	/* to check if cmn_ready is asserted (bit 0) */
	val = Afe_read(state, PHY_PMA_CMN_CTRL1);

	if ((val & 1) == 0)
		pr_err("ERROR: cmn_ready is not asserted\n");
	else
		pr_info("PHY_PMA_CMN_CTRL1 = %x\n", val);

	/* Configure PHY in A0 mode (PHY must be in the A0 power state in order to transmit data) */
	Afe_write(state, PHY_HDP_MODE_CTRL, 0x0101);

	/* Wait for A2 ACK (PHY_HDP_MODE_CTL [4] = 1’b1) */
	do {
		val = Afe_read(state, PHY_HDP_MODE_CTRL);
		val = val >> 4;
	} while ((val & 1) == 0);

	prev_calib_code = aux_cal_cfg(state, prev_calib_code);

	aux_cfg(state);

}
