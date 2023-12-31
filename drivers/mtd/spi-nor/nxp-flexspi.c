/*
 * NXP FSPI(FlexSPI controller) driver.
 *
 * Copyright 2018 NXP
 * Author: Yogesh Gaur <yogeshnarayan.gaur@nxp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/completion.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/pci.h>

/* The registers */
#define FSPI_MCR0			0x00
#define FSPI_MCR0_AHB_TIMEOUT_SHIFT	24
#define FSPI_MCR0_AHB_TIMEOUT_MASK	(0xFF << FSPI_MCR0_AHB_TIMEOUT_SHIFT)
#define FSPI_MCR0_IP_TIMEOUT_SHIFT	16
#define FSPI_MCR0_IP_TIMEOUT_MASK	(0xFF << FSPI_MCR0_IP_TIMEOUT_SHIFT)
#define FSPI_MCR0_LEARN_EN_SHIFT	15
#define FSPI_MCR0_LEARN_EN_MASK		(1 << FSPI_MCR0_LEARN_EN_SHIFT)
#define FSPI_MCR0_SCRFRUN_EN_SHIFT	14
#define FSPI_MCR0_SCRFRUN_EN_MASK	(1 << FSPI_MCR0_SCRFRUN_EN_SHIFT)
#define FSPI_MCR0_OCTCOMB_EN_SHIFT	13
#define FSPI_MCR0_OCTCOMB_EN_MASK	(1 << FSPI_MCR0_OCTCOMB_EN_SHIFT)
#define FSPI_MCR0_DOZE_EN_SHIFT		12
#define FSPI_MCR0_DOZE_EN_MASK		(1 << FSPI_MCR0_DOZE_EN_SHIFT)
#define FSPI_MCR0_HSEN_SHIFT		11
#define FSPI_MCR0_HSEN_MASK		(1 << FSPI_MCR0_HSEN_SHIFT)
#define FSPI_MCR0_SERCLKDIV_SHIFT	8
#define FSPI_MCR0_SERCLKDIV_MASK	(7 << FSPI_MCR0_SERCLKDIV_SHIFT)
#define FSPI_MCR0_ATDF_EN_SHIFT		7
#define FSPI_MCR0_ATDF_EN_MASK		(1 << FSPI_MCR0_ATDF_EN_SHIFT)
#define FSPI_MCR0_ARDF_EN_SHIFT		6
#define FSPI_MCR0_ARDF_EN_MASK		(1 << FSPI_MCR0_ARDF_EN_SHIFT)
#define FSPI_MCR0_RXCLKSRC_SHIFT	4
#define FSPI_MCR0_RXCLKSRC_MASK		(3 << FSPI_MCR0_RXCLKSRC_SHIFT)
#define FSPI_MCR0_END_CFG_SHIFT		2
#define FSPI_MCR0_END_CFG_MASK		(3 << FSPI_MCR0_END_CFG_SHIFT)
#define FSPI_MCR0_MDIS_SHIFT		1
#define FSPI_MCR0_MDIS_MASK		(1 << FSPI_MCR0_MDIS_SHIFT)
#define FSPI_MCR0_SWRST_SHIFT		0
#define FSPI_MCR0_SWRST_MASK		(1 << FSPI_MCR0_SWRST_SHIFT)

#define FSPI_MCR1			0x04
#define FSPI_MCR1_SEQ_TIMEOUT_SHIFT	16
#define FSPI_MCR1_SEQ_TIMEOUT_MASK	\
	(0xFFFF << FSPI_MCR1_SEQ_TIMEOUT_SHIFT)
#define FSPI_MCR1_AHB_TIMEOUT_SHIFT	0
#define FSPI_MCR1_AHB_TIMEOUT_MASK	\
	(0xFFFF << FSPI_MCR1_AHB_TIMEOUT_SHIFT)

#define FSPI_MCR2			0x08
#define FSPI_MCR2_IDLE_WAIT_SHIFT	24
#define FSPI_MCR2_IDLE_WAIT_MASK	(0xFF << FSPI_MCR2_IDLE_WAIT_SHIFT)
#define FSPI_MCR2_SAMEFLASH_SHIFT	15
#define FSPI_MCR2_SAMEFLASH_MASK	(1 << FSPI_MCR2_SAMEFLASH_SHIFT)
#define FSPI_MCR2_CLRLRPHS_SHIFT	14
#define FSPI_MCR2_CLRLRPHS_MASK		(1 << FSPI_MCR2_CLRLRPHS_SHIFT)
#define FSPI_MCR2_ABRDATSZ_SHIFT	8
#define FSPI_MCR2_ABRDATSZ_MASK		(1 << FSPI_MCR2_ABRDATSZ_SHIFT)
#define FSPI_MCR2_ABRLEARN_SHIFT	7
#define FSPI_MCR2_ABRLEARN_MASK		(1 << FSPI_MCR2_ABRLEARN_SHIFT)
#define FSPI_MCR2_ABR_READ_SHIFT	6
#define FSPI_MCR2_ABR_READ_MASK		(1 << FSPI_MCR2_ABR_READ_SHIFT)
#define FSPI_MCR2_ABRWRITE_SHIFT	5
#define FSPI_MCR2_ABRWRITE_MASK		(1 << FSPI_MCR2_ABRWRITE_SHIFT)
#define FSPI_MCR2_ABRDUMMY_SHIFT	4
#define FSPI_MCR2_ABRDUMMY_MASK		(1 << FSPI_MCR2_ABRDUMMY_SHIFT)
#define FSPI_MCR2_ABR_MODE_SHIFT	3
#define FSPI_MCR2_ABR_MODE_MASK		(1 << FSPI_MCR2_ABR_MODE_SHIFT)
#define FSPI_MCR2_ABRCADDR_SHIFT	2
#define FSPI_MCR2_ABRCADDR_MASK		(1 << FSPI_MCR2_ABRCADDR_SHIFT)
#define FSPI_MCR2_ABRRADDR_SHIFT	1
#define FSPI_MCR2_ABRRADDR_MASK		(1 << FSPI_MCR2_ABRRADDR_SHIFT)
#define FSPI_MCR2_ABR_CMD_SHIFT		0
#define FSPI_MCR2_ABR_CMD_MASK		(1 << FSPI_MCR2_ABR_CMD_SHIFT)

#define FSPI_AHBCR			0x0c
#define FSPI_AHBCR_RDADDROPT_SHIFT	6
#define FSPI_AHBCR_RDADDROPT_MASK	(1 << FSPI_AHBCR_RDADDROPT_SHIFT)
#define FSPI_AHBCR_PREF_EN_SHIFT	5
#define FSPI_AHBCR_PREF_EN_MASK		(1 << FSPI_AHBCR_PREF_EN_SHIFT)
#define FSPI_AHBCR_BUFF_EN_SHIFT	4
#define FSPI_AHBCR_BUFF_EN_MASK		(1 << FSPI_AHBCR_BUFF_EN_SHIFT)
#define FSPI_AHBCR_CACH_EN_SHIFT	3
#define FSPI_AHBCR_CACH_EN_MASK		(1 << FSPI_AHBCR_CACH_EN_SHIFT)
#define FSPI_AHBCR_CLRTXBUF_SHIFT	2
#define FSPI_AHBCR_CLRTXBUF_MASK	(1 << FSPI_AHBCR_CLRTXBUF_SHIFT)
#define FSPI_AHBCR_CLRRXBUF_SHIFT	1
#define FSPI_AHBCR_CLRRXBUF_MASK	(1 << FSPI_AHBCR_CLRRXBUF_SHIFT)
#define FSPI_AHBCR_PAR_EN_SHIFT		0
#define FSPI_AHBCR_PAR_EN_MASK		(1 << FSPI_AHBCR_PAR_EN_SHIFT)

#define FSPI_INTEN			0x10
#define FSPI_INTEN_SCLKSBWR_SHIFT	9
#define FSPI_INTEN_SCLKSBWR_MASK	(1 << FSPI_INTEN_SCLKSBWR_SHIFT)
#define FSPI_INTEN_SCLKSBRD_SHIFT	8
#define FSPI_INTEN_SCLKSBRD_MASK	(1 << FSPI_INTEN_SCLKSBRD_SHIFT)
#define FSPI_INTEN_DATALRNFL_SHIFT	7
#define FSPI_INTEN_DATALRNFL_MASK	(1 << FSPI_INTEN_DATALRNFL_SHIFT)
#define FSPI_INTEN_IPTXWE_SHIFT		6
#define FSPI_INTEN_IPTXWE_MASK		(1 << FSPI_INTEN_IPTXWE_SHIFT)
#define FSPI_INTEN_IPRXWA_SHIFT		5
#define FSPI_INTEN_IPRXWA_MASK		(1 << FSPI_INTEN_IPRXWA_SHIFT)
#define FSPI_INTEN_AHBCMDERR_SHIFT	4
#define FSPI_INTEN_AHBCMDERR_MASK	(1 << FSPI_INTEN_AHBCMDERR_SHIFT)
#define FSPI_INTEN_IPCMDERR_SHIFT	3
#define FSPI_INTEN_IPCMDERR_MASK	(1 << FSPI_INTEN_IPCMDERR_SHIFT)
#define FSPI_INTEN_AHBCMDGE_SHIFT	2
#define FSPI_INTEN_AHBCMDGE_MASK	(1 << FSPI_INTEN_AHBCMDGE_SHIFT)
#define FSPI_INTEN_IPCMDGE_SHIFT	1
#define FSPI_INTEN_IPCMDGE_MASK		(1 << FSPI_INTEN_IPCMDGE_SHIFT)
#define FSPI_INTEN_IPCMDDONE_SHIFT	0
#define FSPI_INTEN_IPCMDDONE_MASK	(1 << FSPI_INTEN_IPCMDDONE_SHIFT)

#define FSPI_INTR			0x14
#define FSPI_INTR_SCLKSBWR_SHIFT	9
#define FSPI_INTR_SCLKSBWR_MASK		(1 << FSPI_INTR_SCLKSBWR_SHIFT)
#define FSPI_INTR_SCLKSBRD_SHIFT	8
#define FSPI_INTR_SCLKSBRD_MASK		(1 << FSPI_INTR_SCLKSBRD_SHIFT)
#define FSPI_INTR_DATALRNFL_SHIFT	7
#define FSPI_INTR_DATALRNFL_MASK	(1 << FSPI_INTR_DATALRNFL_SHIFT)
#define FSPI_INTR_IPTXWE_SHIFT		6
#define FSPI_INTR_IPTXWE_MASK		(1 << FSPI_INTR_IPTXWE_SHIFT)
#define FSPI_INTR_IPRXWA_SHIFT		5
#define FSPI_INTR_IPRXWA_MASK		(1 << FSPI_INTR_IPRXWA_SHIFT)
#define FSPI_INTR_AHBCMDERR_SHIFT	4
#define FSPI_INTR_AHBCMDERR_MASK	(1 << FSPI_INTR_AHBCMDERR_SHIFT)
#define FSPI_INTR_IPCMDERR_SHIFT	3
#define FSPI_INTR_IPCMDERR_MASK		(1 << FSPI_INTR_IPCMDERR_SHIFT)
#define FSPI_INTR_AHBCMDGE_SHIFT	2
#define FSPI_INTR_AHBCMDGE_MASK		(1 << FSPI_INTR_AHBCMDGE_SHIFT)
#define FSPI_INTR_IPCMDGE_SHIFT		1
#define FSPI_INTR_IPCMDGE_MASK		(1 << FSPI_INTR_IPCMDGE_SHIFT)
#define FSPI_INTR_IPCMDDONE_SHIFT	0
#define FSPI_INTR_IPCMDDONE_MASK	(1 << FSPI_INTR_IPCMDDONE_SHIFT)

#define FSPI_LUTKEY			0x18
#define FSPI_LUTKEY_VALUE		0x5AF05AF0

#define FSPI_LCKCR			0x1C
#define FSPI_LCKER_LOCK			0x1
#define FSPI_LCKER_UNLOCK		0x2

#define FSPI_BUFXCR_INVALID_MSTRID	0xe
#define FSPI_AHBRX_BUF0CR0		0x20
#define FSPI_AHBRX_BUF1CR0		0x24
#define FSPI_AHBRX_BUF2CR0		0x28
#define FSPI_AHBRX_BUF3CR0		0x2C
#define FSPI_AHBRX_BUF4CR0		0x30
#define FSPI_AHBRX_BUF5CR0		0x34
#define FSPI_AHBRX_BUF6CR0		0x38
#define FSPI_AHBRX_BUF7CR0		0x3C
#define FSPI_AHBRXBUF0CR7_PREF_SHIFT	31
#define FSPI_AHBRXBUF0CR7_PREF_MASK	(1 << FSPI_AHBRXBUF0CR7_PREF_SHIFT)

#define FSPI_AHBRX_BUF0CR1		0x40
#define FSPI_AHBRX_BUF1CR1		0x44
#define FSPI_AHBRX_BUF2CR1		0x48
#define FSPI_AHBRX_BUF3CR1		0x4C
#define FSPI_AHBRX_BUF4CR1		0x50
#define FSPI_AHBRX_BUF5CR1		0x54
#define FSPI_AHBRX_BUF6CR1		0x58
#define FSPI_AHBRX_BUF7CR1		0x5C
#define FSPI_BUFXCR1_MSID_SHIFT		0
#define FSPI_BUFXCR1_MSID_MASK		(0xF << FSPI_BUFXCR1_MSID_SHIFT)
#define FSPI_BUFXCR1_PRIO_SHIFT		8
#define FSPI_BUFXCR1_PRIO_MASK		(0x7 << FSPI_BUFXCR1_PRIO_SHIFT)

#define FSPI_FLSHA1CR0			0x60
#define FSPI_FLSHA2CR0			0x64
#define FSPI_FLSHB1CR0			0x68
#define FSPI_FLSHB2CR0			0x6C
#define FSPI_FLSHXCR0_SZ_SHIFT		10
#define FSPI_FLSHXCR0_SZ_MASK		(0x3FFFFF << FSPI_FLSHXCR0_SZ_SHIFT)

#define FSPI_FLSHA1CR1			0x70
#define FSPI_FLSHA2CR1			0x74
#define FSPI_FLSHB1CR1			0x78
#define FSPI_FLSHB2CR1			0x7C
#define FSPI_FLSHXCR1_CSINTR_SHIFT	16
#define FSPI_FLSHXCR1_CSINTR_MASK	\
		(0xFFFF << FSPI_FLSHXCR1_CSINTR_SHIFT)
#define FSPI_FLSHXCR1_CAS_SHIFT		11
#define FSPI_FLSHXCR1_CAS_MASK		(0xF << FSPI_FLSHXCR1_CAS_SHIFT)
#define FSPI_FLSHXCR1_WA_SHIFT		10
#define FSPI_FLSHXCR1_WA_MASK		(1 << FSPI_FLSHXCR1_WA_SHIFT)
#define FSPI_FLSHXCR1_TCSH_SHIFT	5
#define FSPI_FLSHXCR1_TCSH_MASK		(0x1F << FSPI_FLSHXCR1_TCSH_SHIFT)
#define FSPI_FLSHXCR1_TCSS_SHIFT	0
#define FSPI_FLSHXCR1_TCSS_MASK		(0x1F << FSPI_FLSHXCR1_TCSS_SHIFT)

#define FSPI_FLSHA1CR2			0x80
#define FSPI_FLSHA2CR2			0x84
#define FSPI_FLSHB1CR2			0x88
#define FSPI_FLSHB2CR2			0x8C
#define FSPI_FLSHXCR2_CLRINSP_SHIFT	24
#define FSPI_FLSHXCR2_CLRINSP_MASK	(1 << FSPI_FLSHXCR2_CLRINSP_SHIFT)
#define FSPI_FLSHXCR2_AWRWAIT_SHIFT	16
#define FSPI_FLSHXCR2_AWRWAIT_MASK	(0xFF << FSPI_FLSHXCR2_AWRWAIT_SHIFT)
#define FSPI_FLSHXCR2_AWRSEQN_SHIFT	13
#define FSPI_FLSHXCR2_AWRSEQN_MASK	(0x7 << FSPI_FLSHXCR2_AWRSEQN_SHIFT)
#define FSPI_FLSHXCR2_AWRSEQI_SHIFT	8
#define FSPI_FLSHXCR2_AWRSEQI_MASK	(0xF << FSPI_FLSHXCR2_AWRSEQI_SHIFT)
#define FSPI_FLSHXCR2_ARDSEQN_SHIFT	5
#define FSPI_FLSHXCR2_ARDSEQN_MASK	(0x7 << FSPI_FLSHXCR2_ARDSEQN_SHIFT)
#define FSPI_FLSHXCR2_ARDSEQI_SHIFT	0
#define FSPI_FLSHXCR2_ARDSEQI_MASK	(0xF << FSPI_FLSHXCR2_ARDSEQI_SHIFT)

#define FSPI_IPCR0			0xA0

#define FSPI_IPCR1			0xA4
#define FSPI_IPCR1_IPAREN_SHIFT		31
#define FSPI_IPCR1_IPAREN_MASK		(1 << FSPI_IPCR1_IPAREN_SHIFT)
#define FSPI_IPCR1_SEQNUM_SHIFT		24
#define FSPI_IPCR1_SEQNUM_MASK		(0xF << FSPI_IPCR1_SEQNUM_SHIFT)
#define FSPI_IPCR1_SEQID_SHIFT		16
#define FSPI_IPCR1_SEQID_MASK		(0xF << FSPI_IPCR1_SEQID_SHIFT)
#define FSPI_IPCR1_IDATSZ_SHIFT		0
#define FSPI_IPCR1_IDATSZ_MASK		(0xFFFF << FSPI_IPCR1_IDATSZ_SHIFT)

#define FSPI_IPCMD			0xB0
#define FSPI_IPCMD_TRG_SHIFT		0
#define FSPI_IPCMD_TRG_MASK		(1 << FSPI_IPCMD_TRG_SHIFT)

#define FSPI_DLPR			0xB4

#define FSPI_IPRXFCR			0xB8
#define FSPI_IPRXFCR_CLR_SHIFT		0
#define FSPI_IPRXFCR_CLR_MASK		(1 << FSPI_IPRXFCR_CLR_SHIFT)
#define FSPI_IPRXFCR_DMA_EN_SHIFT	1
#define FSPI_IPRXFCR_DMA_EN_MASK	(1 << FSPI_IPRXFCR_DMA_EN_SHIFT)
#define FSPI_IPRXFCR_WMRK_SHIFT		2
#define FSPI_IPRXFCR_WMRK_MASK		(0x1F << FSPI_IPRXFCR_WMRK_SHIFT)

#define FSPI_IPTXFCR			0xBC
#define FSPI_IPTXFCR_CLR_SHIFT		0
#define FSPI_IPTXFCR_CLR_MASK		(1 << FSPI_IPTXFCR_CLR_SHIFT)
#define FSPI_IPTXFCR_DMA_EN_SHIFT	1
#define FSPI_IPTXFCR_DMA_EN_MASK	(1 << FSPI_IPTXFCR_DMA_EN_SHIFT)
#define FSPI_IPTXFCR_WMRK_SHIFT		2
#define FSPI_IPTXFCR_WMRK_MASK		(0x1F << FSPI_IPTXFCR_WMRK_SHIFT)

#define	FSPI_DLLACR			0xC0
#define	FSPI_DLLACR_OVRDEN_SHIFT	8
#define	FSPI_DLLACR_OVRDEN_MASK		(1 << FSPI_DLLACR_OVRDEN_SHIFT)

#define	FSPI_DLLBCR			0xC4
#define	FSPI_DLLBCR_OVRDEN_SHIFT	8
#define	FSPI_DLLBCR_OVRDEN_MASK		(1 << FSPI_DLLBCR_OVRDEN_SHIFT)

#define FSPI_STS0			0xE0
#define FSPI_STS0_DLPHA_SHIFT		9
#define FSPI_STS0_DLPHA_MASK		(0x1F << FSPI_STS0_DLPHA_SHIFT)
#define FSPI_STS0_DLPHB_SHIFT		4
#define FSPI_STS0_DLPHB_MASK		(0x1F << FSPI_STS0_DLPHB_SHIFT)
#define FSPI_STS0_CMD_SRC_SHIFT		2
#define FSPI_STS0_CMD_SRC_MASK		(3 << FSPI_STS0_CMD_SRC_SHIFT)
#define FSPI_STS0_ARB_IDLE_SHIFT	1
#define FSPI_STS0_ARB_IDLE_MASK		(1 << FSPI_STS0_ARB_IDLE_SHIFT)
#define FSPI_STS0_SEQ_IDLE_SHIFT	0
#define FSPI_STS0_SEQ_IDLE_MASK		(1 << FSPI_STS0_SEQ_IDLE_SHIFT)

#define FSPI_STS1			0xE4
#define FSPI_STS1_IP_ERRCD_SHIFT	24
#define FSPI_STS1_IP_ERRCD_MASK		(0xF << FSPI_STS1_IP_ERRCD_SHIFT)
#define FSPI_STS1_IP_ERRID_SHIFT	16
#define FSPI_STS1_IP_ERRID_MASK		(0xF << FSPI_STS1_IP_ERRID_SHIFT)
#define FSPI_STS1_AHB_ERRCD_SHIFT	8
#define FSPI_STS1_AHB_ERRCD_MASK	(0xF << FSPI_STS1_AHB_ERRCD_SHIFT)
#define FSPI_STS1_AHB_ERRID_SHIFT	0
#define FSPI_STS1_AHB_ERRID_MASK	(0xF << FSPI_STS1_AHB_ERRID_SHIFT)

#define FSPI_AHBSPNST			0xEC
#define FSPI_AHBSPNST_DATLFT_SHIFT	16
#define FSPI_AHBSPNST_DATLFT_MASK	\
		(0xFFFF << FSPI_AHBSPNST_DATLFT_SHIFT)
#define FSPI_AHBSPNST_BUFID_SHIFT	1
#define FSPI_AHBSPNST_BUFID_MASK	(7 << FSPI_AHBSPNST_BUFID_SHIFT)
#define FSPI_AHBSPNST_ACTIVE_SHIFT	0
#define FSPI_AHBSPNST_ACTIVE_MASK	(1 << FSPI_AHBSPNST_ACTIVE_SHIFT)

#define FSPI_IPRXFSTS			0xF0
#define FSPI_IPRXFSTS_RDCNTR_SHIFT	16
#define FSPI_IPRXFSTS_RDCNTR_MASK	\
		(0xFFFF << FSPI_IPRXFSTS_RDCNTR_SHIFT)
#define FSPI_IPRXFSTS_FILL_SHIFT	0
#define FSPI_IPRXFSTS_FILL_MASK		(0xFF << FSPI_IPRXFSTS_FILL_SHIFT)

#define FSPI_IPTXFSTS			0xF4
#define FSPI_IPTXFSTS_WRCNTR_SHIFT	16
#define FSPI_IPTXFSTS_WRCNTR_MASK	\
		(0xFFFF << FSPI_IPTXFSTS_WRCNTR_SHIFT)
#define FSPI_IPTXFSTS_FILL_SHIFT	0
#define FSPI_IPTXFSTS_FILL_MASK		(0xFF << FSPI_IPTXFSTS_FILL_SHIFT)

#define FSPI_RFDR			0x100
#define FSPI_TFDR			0x180

#define FSPI_LUT_BASE			0x200

/* register map end */

/*
 * The definition of the LUT register shows below:
 *
 *  ---------------------------------------------------
 *  | INSTR1 | PAD1 | OPRND1 | INSTR0 | PAD0 | OPRND0 |
 *  ---------------------------------------------------
 */
#define OPRND0_SHIFT		0
#define PAD0_SHIFT		8
#define INSTR0_SHIFT		10
#define OPRND1_SHIFT		16

/* Instruction set for the LUT register. */

#define LUT_STOP		0x00
#define LUT_CMD			0x01
#define LUT_ADDR		0x02
#define LUT_CADDR_SDR		0x03
#define LUT_MODE		0x04
#define LUT_MODE2		0x05
#define LUT_MODE4		0x06
#define LUT_MODE8		0x07
#define LUT_NXP_WRITE		0x08
#define LUT_NXP_READ		0x09
#define LUT_LEARN_SDR		0x0A
#define LUT_DATSZ_SDR		0x0B
#define LUT_DUMMY		0x0C
#define LUT_DUMMY_RWDS_SDR	0x0D
#define LUT_JMP_ON_CS		0x1F
#define LUT_CMD_DDR		0x21
#define LUT_ADDR_DDR		0x22
#define LUT_CADDR_DDR		0x23
#define LUT_MODE_DDR		0x24
#define LUT_MODE2_DDR		0x25
#define LUT_MODE4_DDR		0x26
#define LUT_MODE8_DDR		0x27
#define LUT_WRITE_DDR		0x28
#define LUT_READ_DDR		0x29
#define LUT_LEARN_DDR		0x2A
#define LUT_DATSZ_DDR		0x2B
#define LUT_DUMMY_DDR		0x2C
#define LUT_DUMMY_RWDS_DDR	0x2D


/*
 * The PAD definitions for LUT register.
 *
 * The pad stands for the lines number of IO[0:3].
 * For example, the Quad read need four IO lines, so you should
 * set LUT_PAD4 which means we use four IO lines.
 */
#define LUT_PAD1		0
#define LUT_PAD2		1
#define LUT_PAD4		2
#define LUT_PAD8		3

/* Oprands for the LUT register. */
#define ADDR24BIT		0x18
#define ADDR32BIT		0x20

/* Macros for constructing the LUT register. */
#define LUT0(ins, pad, opr)						\
		(((opr) << OPRND0_SHIFT) | ((LUT_##pad) << PAD0_SHIFT) | \
		((LUT_##ins) << INSTR0_SHIFT))

#define LUT1(ins, pad, opr)	(LUT0(ins, pad, opr) << OPRND1_SHIFT)

/* other macros for LUT register. */
#define FSPI_LUT(x)		(FSPI_LUT_BASE + (x) * 4)
#define FSPI_LUT_NUM		128

/* SEQID -- we can have 32 seqids at most. */
#define SEQID_READ		0
#define SEQID_WREN		1
#define SEQID_WRDI		2
#define SEQID_RDSR		3
#define SEQID_SE		4
#define SEQID_CHIP_ERASE	5
#define SEQID_PP		6
#define SEQID_RDID		7
#define SEQID_WRSR		8
#define SEQID_RDCR		9
#define SEQID_EN4B		10
#define SEQID_BRWR		11
#define SEQID_RD_EVCR		12
#define SEQID_WD_EVCR		13
#define SEQID_RDFSR		14

#define FSPI_MIN_IOMAP		SZ_4M

#define FSPI_RX_MAX_IPBUF_SIZE		0x200 /* 64 * 64bits  */
#define FSPI_TX_MAX_IPBUF_SIZE		0x400 /* 128 * 64bits */
#define FSPI_RX_MAX_AHBBUF_SIZE		0x800 /* 256 * 64bits */
#define FSPI_TX_MAX_AHBBUF_SIZE		0x40  /* 8 * 64bits   */

#define TX_IPBUF_SIZE		FSPI_TX_MAX_IPBUF_SIZE
#define RX_IPBUF_SIZE		FSPI_RX_MAX_IPBUF_SIZE
#define RX_AHBBUF_SIZE		FSPI_RX_MAX_AHBBUF_SIZE
#define TX_AHBBUF_SIZE		FSPI_TX_MAX_AHBBUF_SIZE

#define FSPI_SINGLE_MODE	1
#define FSPI_OCTAL_MODE		8

#define FSPINOR_OP_READ_1_1_8_4B	0x7c

enum nxp_fspi_devtype {
	NXP_FSPI_LX2160A,
};

struct nxp_fspi_devtype_data {
	enum nxp_fspi_devtype devtype;
	int rxfifo;
	int txfifo;
	int ahb_buf_size;
	int driver_data;
};

static struct nxp_fspi_devtype_data lx2160a_data = {
	.devtype = NXP_FSPI_LX2160A,
	.rxfifo = RX_IPBUF_SIZE,
	.txfifo = TX_IPBUF_SIZE,
	.ahb_buf_size = RX_AHBBUF_SIZE,
	.driver_data = 0,
};

#define NXP_FSPI_MAX_CHIP	4
struct nxp_fspi {
	struct mtd_info mtd[NXP_FSPI_MAX_CHIP];
	struct spi_nor nor[NXP_FSPI_MAX_CHIP];
	void __iomem *iobase;
	void __iomem *ahb_addr;
	u32 memmap_phy;
	u32 memmap_offs;
	u32 memmap_len;
	struct clk *clk, *clk_en;
	struct device *dev;
	struct completion c;
	struct nxp_fspi_devtype_data *devtype_data;
	u32 nor_size;
	u32 nor_num;
	u32 clk_rate;
	u32 spi_rx_bus_width;
	u32 spi_tx_bus_width;
	unsigned int chip_base_addr; /* We may support two chips. */
	bool has_second_chip;
	struct mutex lock;
	struct pm_qos_request pm_qos_req;
};

static inline void nxp_fspi_unlock_lut(struct nxp_fspi *fspi)
{
	writel(FSPI_LUTKEY_VALUE, fspi->iobase + FSPI_LUTKEY);
	writel(FSPI_LCKER_UNLOCK, fspi->iobase + FSPI_LCKCR);
}

static inline void nxp_fspi_lock_lut(struct nxp_fspi *fspi)
{
	writel(FSPI_LUTKEY_VALUE, fspi->iobase + FSPI_LUTKEY);
	writel(FSPI_LCKER_LOCK, fspi->iobase + FSPI_LCKCR);
}

static irqreturn_t nxp_fspi_irq_handler(int irq, void *dev_id)
{
	struct nxp_fspi *fspi = dev_id;
	u32 reg;

	reg = readl(fspi->iobase + FSPI_INTR);
	writel(FSPI_INTR_IPCMDDONE_MASK, fspi->iobase + FSPI_INTR);
	if (reg & FSPI_INTR_IPCMDDONE_MASK)
		complete(&fspi->c);

	return IRQ_HANDLED;
}

static void nxp_fspi_init_lut(struct nxp_fspi *fspi)
{
	void __iomem *base = fspi->iobase;
	struct spi_nor *nor = &fspi->nor[0];
	u8 addrlen = (nor->addr_width == 3) ? ADDR24BIT : ADDR32BIT;
	u32 lut_base;
	u8 op, dm;
	int i;

	nxp_fspi_unlock_lut(fspi);

	/* Clear all the LUT table */
	for (i = 0; i < FSPI_LUT_NUM; i++)
		writel(0, base + FSPI_LUT_BASE + i * 4);

	/* Read */
	lut_base = SEQID_READ * 4;
	op = nor->read_opcode;
	dm = nor->read_dummy;

	if (fspi->spi_rx_bus_width == FSPI_OCTAL_MODE) {
		dm = 8;
		op = FSPINOR_OP_READ_1_1_8_4B;
		writel(LUT0(CMD, PAD1, op) | LUT1(ADDR, PAD1, addrlen),
				base + FSPI_LUT(lut_base));
		writel(LUT0(DUMMY, PAD8, dm) | LUT1(NXP_READ, PAD8, 0),
				base + FSPI_LUT(lut_base + 1));
	} else {
		if ((op == SPINOR_OP_READ_FAST_4B) ||
		     (op == SPINOR_OP_READ_FAST) ||
		     (op == SPINOR_OP_READ) ||
		     (op == SPINOR_OP_READ_4B)) {
			dm = 8;
			writel(LUT0(CMD, PAD1, op) | LUT1(ADDR, PAD1, addrlen),
					base + FSPI_LUT(lut_base));
			writel(LUT0(DUMMY, PAD1, dm) | LUT1(NXP_READ, PAD1, 0),
					base + FSPI_LUT(lut_base + 1));
		} else if (nor->read_proto == SNOR_PROTO_1_4_4) {
			dev_dbg(nor->dev, "Unsupported opcode : 0x%.2x\n", op);
			/* TODO Add support for other Read ops. */
		} else {
			dev_dbg(nor->dev, "Unsupported opcode : 0x%.2x\n", op);
		}
	}

	/* Write enable */
	lut_base = SEQID_WREN * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_WREN), base + FSPI_LUT(lut_base));

	/* Page Program */
	lut_base = SEQID_PP * 4;
	writel(LUT0(CMD, PAD1, nor->program_opcode) | LUT1(ADDR, PAD1, addrlen),
			base + FSPI_LUT(lut_base));
	writel(LUT0(NXP_WRITE, PAD1, 0), base + FSPI_LUT(lut_base + 1));

	/* Read Status */
	lut_base = SEQID_RDSR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_RDSR) | LUT1(NXP_READ, PAD1, 0x1),
			base + FSPI_LUT(lut_base));

	/* Erase a sector */
	lut_base = SEQID_SE * 4;
	writel(LUT0(CMD, PAD1, nor->erase_opcode) | LUT1(ADDR, PAD1, addrlen),
			base + FSPI_LUT(lut_base));

	/* Erase the whole chip */
	lut_base = SEQID_CHIP_ERASE * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_CHIP_ERASE),
			base + FSPI_LUT(lut_base));

	/* READ ID */
	lut_base = SEQID_RDID * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_RDID) | LUT1(NXP_READ, PAD1, 0x8),
			base + FSPI_LUT(lut_base));

	/* Write Register */
	lut_base = SEQID_WRSR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_WRSR) | LUT1(NXP_WRITE, PAD1, 0x2),
			base + FSPI_LUT(lut_base));

	/* Read Configuration Register */
	lut_base = SEQID_RDCR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_RDCR) | LUT1(NXP_READ, PAD1, 0x1),
			base + FSPI_LUT(lut_base));

	/* Write disable */
	lut_base = SEQID_WRDI * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_WRDI), base + FSPI_LUT(lut_base));

	/* Enter 4 Byte Mode (Micron) */
	lut_base = SEQID_EN4B * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_EN4B), base + FSPI_LUT(lut_base));

	/* Enter 4 Byte Mode (Spansion) */
	lut_base = SEQID_BRWR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_BRWR), base + FSPI_LUT(lut_base));

	/* Read EVCR register */
	lut_base = SEQID_RD_EVCR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_RD_EVCR),
	       base + FSPI_LUT(lut_base));

	/* Write EVCR register */
	lut_base = SEQID_WD_EVCR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_WD_EVCR),
	       base + FSPI_LUT(lut_base));

	/* Read Flag Status */
	lut_base = SEQID_RDFSR * 4;
	writel(LUT0(CMD, PAD1, SPINOR_OP_RDFSR) | LUT1(NXP_READ, PAD1, 0x1),
			base + FSPI_LUT(lut_base));

	nxp_fspi_lock_lut(fspi);
}

/* Get the SEQID for the command */
static int nxp_fspi_get_seqid(struct nxp_fspi *fspi, u8 cmd)
{

	switch (cmd) {
	case SPINOR_OP_READ_1_1_4_4B:
	case SPINOR_OP_READ_1_1_4:
	case SPINOR_OP_READ:
	case SPINOR_OP_READ_4B:
	case SPINOR_OP_READ_FAST:
	case SPINOR_OP_READ_FAST_4B:
		return SEQID_READ;
	case SPINOR_OP_WREN:
		return SEQID_WREN;
	case SPINOR_OP_WRDI:
		return SEQID_WRDI;
	case SPINOR_OP_RDSR:
		return SEQID_RDSR;
	case SPINOR_OP_RDFSR:
		return SEQID_RDFSR;
	case SPINOR_OP_BE_4K:
	case SPINOR_OP_SE:
	case SPINOR_OP_SE_4B:
	case SPINOR_OP_BE_4K_4B:
		return SEQID_SE;
	case SPINOR_OP_CHIP_ERASE:
		return SEQID_CHIP_ERASE;
	case SPINOR_OP_PP:
	case SPINOR_OP_PP_4B:
		return SEQID_PP;
	case SPINOR_OP_RDID:
		return SEQID_RDID;
	case SPINOR_OP_WRSR:
		return SEQID_WRSR;
	case SPINOR_OP_RDCR:
		return SEQID_RDCR;
	case SPINOR_OP_EN4B:
		return SEQID_EN4B;
	case SPINOR_OP_BRWR:
		return SEQID_BRWR;
	case SPINOR_OP_RD_EVCR:
		return SEQID_RD_EVCR;
	case SPINOR_OP_WD_EVCR:
		return SEQID_WD_EVCR;
	default:
		dev_err(fspi->dev, "Unsupported cmd 0x%.2x\n", cmd);
		break;
	}
	return -EINVAL;
}

static int
nxp_fspi_runcmd(struct nxp_fspi *fspi, u8 cmd, unsigned int addr, int len)
{
	void __iomem *base = fspi->iobase;
	int seqid;
	int seqnum = 0;
	u32 reg;
	int err;
	int iprxfcr = 0;

	iprxfcr = readl(fspi->iobase + FSPI_IPRXFCR);
	/* invalid RXFIFO first */
	iprxfcr &= ~FSPI_IPRXFCR_DMA_EN_MASK;
	iprxfcr = iprxfcr | FSPI_IPRXFCR_CLR_MASK;
	writel(iprxfcr, fspi->iobase + FSPI_IPRXFCR);

	init_completion(&fspi->c);
	dev_dbg(fspi->dev, "to 0x%.8x:0x%.8x, len:%d, cmd:%.2x\n",
			fspi->chip_base_addr, addr, len, cmd);

	/* write address */
	writel(fspi->chip_base_addr + addr, base + FSPI_IPCR0);

	seqid = nxp_fspi_get_seqid(fspi, cmd);

	writel((seqnum << FSPI_IPCR1_SEQNUM_SHIFT) |
			(seqid << FSPI_IPCR1_SEQID_SHIFT) | len,
			base + FSPI_IPCR1);

	/* wait till controller is idle */
	do {
		reg = readl(base + FSPI_STS0);
		if ((reg & FSPI_STS0_ARB_IDLE_MASK) &&
		    (reg & FSPI_STS0_SEQ_IDLE_MASK))
			break;
		udelay(1);
		dev_dbg(fspi->dev, "The controller is busy, 0x%x\n", reg);
	} while (1);

	/* trigger the LUT now */
	writel(1, base + FSPI_IPCMD);

	/* Wait for the interrupt. */
	if (!wait_for_completion_timeout(&fspi->c, msecs_to_jiffies(1000))) {
		dev_err(fspi->dev,
			"cmd 0x%.2x timeout, addr@%.8x, Status0:0x%.8x, Status1:0x%.8x\n",
			cmd, addr, readl(base + FSPI_STS0),
			readl(base + FSPI_STS1));
		err = -ETIMEDOUT;
	} else {
		err = 0;
		dev_dbg(fspi->dev, "FSPI Intr done,INTR:<0x%.8x>\n",
			 readl(base + FSPI_INTR));
	}

	return err;
}

/* Read out the data from the FSPI_RBDR buffer registers. */
static void nxp_fspi_read_data(struct nxp_fspi *fspi, int len, u8 *rxbuf)
{
	int i = 0, j = 0, tmp_size = 0;
	int size;
	u32 tmp = 0;

	while (len > 0) {

		size = len / 8;

		for (i = 0; i < size; ++i) {
			/* Wait for RXFIFO available*/
			while (!(readl(fspi->iobase + FSPI_INTR)
				 & FSPI_INTR_IPRXWA_MASK))
				;

			j = 0;
			tmp_size = 8;
			while (tmp_size > 0) {
				tmp = 0;
				tmp = readl(fspi->iobase + FSPI_RFDR + j * 4);
				memcpy(rxbuf, &tmp, 4);
				tmp_size -= 4;
				j++;
				rxbuf += 4;
			}

			/* move the FIFO pointer */
			writel(FSPI_INTR_IPRXWA_MASK,
			       fspi->iobase + FSPI_INTR);
			len -= 8;
		}

		size = len % 8;

		j = 0;
		if (size) {
			/* Wait for RXFIFO available*/
			while (!(readl(fspi->iobase + FSPI_INTR)
				 & FSPI_INTR_IPRXWA_MASK))
				;

			while (len > 0) {
				tmp = 0;
				size = (len < 4) ? len : 4;
				tmp = readl(fspi->iobase + FSPI_RFDR + j * 4);
				memcpy(rxbuf, &tmp, size);
				len -= size;
				j++;
				rxbuf += size;
			}
		}

		/* invalid the RXFIFO */
		writel(FSPI_IPRXFCR_CLR_MASK,
		       fspi->iobase + FSPI_IPRXFCR);

		writel(FSPI_INTR_IPRXWA_MASK,
		       fspi->iobase + FSPI_INTR);
	}
}

static inline void nxp_fspi_invalid(struct nxp_fspi *fspi)
{
	u32 reg;

	reg = readl(fspi->iobase + FSPI_MCR0);
	writel(reg | FSPI_MCR0_SWRST_MASK, fspi->iobase + FSPI_MCR0);

	/*
	 * The minimum delay : 1 AHB + 2 SFCK clocks.
	 * Delay 1 us is enough.
	 */
	while (readl(fspi->iobase + FSPI_MCR0) & FSPI_MCR0_SWRST_MASK)
		;
}

static ssize_t nxp_fspi_nor_write(struct nxp_fspi *fspi,
				     struct spi_nor *nor, u8 opcode,
				     unsigned int to, u32 *txbuf,
				     unsigned int count)
{
	int ret, i, j;
	int size, tmp_size;
	u32 data = 0;

	dev_dbg(fspi->dev, "nor write to 0x%.8x:0x%.8x, len : %d\n",
		fspi->chip_base_addr, to, count);

	/* clear the TX FIFO. */
	writel(FSPI_IPTXFCR_CLR_MASK, fspi->iobase + FSPI_IPTXFCR);

	size = count / 8;
	for (i = 0; i < size; i++) {
		/* Wait for TXFIFO empty*/
		while (!(readl(fspi->iobase + FSPI_INTR)
			 & FSPI_INTR_IPTXWE_MASK))
			;
		j = 0;
		tmp_size = 8;
		while (tmp_size > 0) {
			data = 0;
			memcpy(&data, txbuf, 4);
			writel(data, fspi->iobase + FSPI_TFDR + j * 4);
			tmp_size -= 4;
			j++;
			txbuf += 1;
		}

		writel(FSPI_INTR_IPTXWE_MASK, fspi->iobase + FSPI_INTR);
	}

	size = count % 8;
	if (size) {
		/* Wait for TXFIFO empty*/
		while (!(readl(fspi->iobase + FSPI_INTR)
			 & FSPI_INTR_IPTXWE_MASK))
			;

		j = 0;
		tmp_size = 0;
		while (size > 0) {
			data = 0;
			tmp_size = (size < 4) ? size : 4;
			memcpy(&data, txbuf, tmp_size);
			writel(data, fspi->iobase + FSPI_TFDR + j * 4);
			size -= tmp_size;
			j++;
			txbuf += 1;
		}

		writel(FSPI_INTR_IPTXWE_MASK, fspi->iobase + FSPI_INTR);
	}

	/* Trigger it */
	ret = nxp_fspi_runcmd(fspi, opcode, to, count);

	if (ret == 0)
		return count;

	return ret;
}

static void nxp_fspi_set_map_addr(struct nxp_fspi *fspi)
{
	int nor_size = fspi->nor_size >> 10;
	void __iomem *base = fspi->iobase;

	/*
	 * Supporting same flash device as slaves on different chip-select.
	 * As SAMEDEVICEEN bit set, by default, in mcr2 reg then need not to
	 * configure FLSHA2CRx/FLSHB1CRx/FLSHB2CRx register as setting for
	 * these would be ignored.
	 * Need to Reset SAMEDEVICEEN bit in mcr2 reg, when require to add
	 * support for different flashes.
	 */
	writel(nor_size, base + FSPI_FLSHA1CR0);
	writel(0, base + FSPI_FLSHA2CR0);
	writel(0, base + FSPI_FLSHB1CR0);
	writel(0, base + FSPI_FLSHB2CR0);
}

static void nxp_fspi_init_ahb_read(struct nxp_fspi *fspi)
{
	void __iomem *base = fspi->iobase;
	struct spi_nor *nor = &fspi->nor[0];
	int i = 0;
	int seqid;

	/* AHB configuration for access buffer 0~7. */
	for (i = 0; i < 7; i++)
		writel(0, base + FSPI_AHBRX_BUF0CR0 + 4 * i);

	/*
	 * Set ADATSZ with the maximum AHB buffer size to improve the read
	 * performance.
	 */
	writel((fspi->devtype_data->ahb_buf_size / 8 |
		FSPI_AHBRXBUF0CR7_PREF_MASK), base + FSPI_AHBRX_BUF7CR0);

	/* prefetch and no start address alignment limitation */
	writel(FSPI_AHBCR_PREF_EN_MASK | FSPI_AHBCR_RDADDROPT_MASK,
		    base + FSPI_AHBCR);


	/* Set the default lut sequence for AHB Read. */
	seqid = nxp_fspi_get_seqid(fspi, nor->read_opcode);
	writel(seqid, base + FSPI_FLSHA1CR2);
}

/* This function was used to prepare and enable FSPI clock */
static int nxp_fspi_clk_prep_enable(struct nxp_fspi *fspi)
{
	int ret;

	ret = clk_prepare_enable(fspi->clk_en);
	if (ret)
		return ret;

	ret = clk_prepare_enable(fspi->clk);
	if (ret) {
		clk_disable_unprepare(fspi->clk_en);
		return ret;
	}

	return 0;
}

/* This function was used to disable and unprepare FSPI clock */
static void nxp_fspi_clk_disable_unprep(struct nxp_fspi *fspi)
{
	clk_disable_unprepare(fspi->clk);
	clk_disable_unprepare(fspi->clk_en);
}

/* We use this function to do some basic init for spi_nor_scan(). */
static int nxp_fspi_nor_setup(struct nxp_fspi *fspi)
{
	void __iomem *base = fspi->iobase;
	u32 reg;

	/* Reset the module */
	writel(FSPI_MCR0_SWRST_MASK, base + FSPI_MCR0);
	do {
		udelay(1);
	} while (0x1 & readl(base + FSPI_MCR0));

	/* Disable the module */
	writel(FSPI_MCR0_MDIS_MASK, base + FSPI_MCR0);

	/* Reset the DLL register to default value */
	writel(FSPI_DLLACR_OVRDEN_MASK, base + FSPI_DLLACR);
	writel(FSPI_DLLBCR_OVRDEN_MASK, base + FSPI_DLLBCR);

	/* enable module */
	writel(FSPI_MCR0_AHB_TIMEOUT_MASK | FSPI_MCR0_IP_TIMEOUT_MASK,
	       base + FSPI_MCR0);

	/* Read the register value */
	reg = readl(base + FSPI_MCR0);

	/* Init the LUT table. */
	nxp_fspi_init_lut(fspi);

	/* enable the interrupt */
	writel(FSPI_INTEN_IPCMDDONE_MASK, fspi->iobase + FSPI_INTEN);
	return 0;
}

static int nxp_fspi_nor_setup_last(struct nxp_fspi *fspi)
{
	unsigned long rate = fspi->clk_rate;
	int ret;

	/* disable and unprepare clock to avoid glitch pass to controller */
	nxp_fspi_clk_disable_unprep(fspi);

	ret = clk_set_rate(fspi->clk, rate);
	if (ret)
		return ret;

	ret = nxp_fspi_clk_prep_enable(fspi);
	if (ret)
		return ret;

	/* Init the LUT table again. */
	nxp_fspi_init_lut(fspi);

	/* Init for AHB read */
	nxp_fspi_init_ahb_read(fspi);

	return 0;
}

static void nxp_fspi_set_base_addr(struct nxp_fspi *fspi,
				      struct spi_nor *nor)
{
	fspi->chip_base_addr = fspi->nor_size * (nor - fspi->nor);
}

static int nxp_fspi_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf,
				int len)
{
	int ret;
	struct nxp_fspi *fspi = nor->priv;

	ret = nxp_fspi_runcmd(fspi, opcode, 0, len);
	if (ret)
		return ret;

	nxp_fspi_read_data(fspi, len, buf);
	return 0;
}

static int nxp_fspi_write_reg(struct spi_nor *nor, u8 opcode, u8 *buf,
				 int len)
{
	struct nxp_fspi *fspi = nor->priv;
	int ret;

	if (!buf) {
		ret = nxp_fspi_runcmd(fspi, opcode, 0, 1);
		if (ret)
			return ret;

		if (opcode == SPINOR_OP_CHIP_ERASE)
			nxp_fspi_invalid(fspi);

	} else if (len > 0) {
		ret = nxp_fspi_nor_write(fspi, nor, opcode, 0,
					(u32 *)buf, len);
	} else {
		dev_err(fspi->dev, "invalid cmd %d\n", opcode);
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t nxp_fspi_write(struct spi_nor *nor, loff_t to,
		size_t len, const u_char *buf)
{
	struct nxp_fspi *fspi = nor->priv;
	ssize_t tx_size = 0, act_wrt = 0, ret = 0;

	while (len > 0) {
		tx_size = (len > TX_IPBUF_SIZE) ? TX_IPBUF_SIZE : len;

		act_wrt = nxp_fspi_nor_write(fspi, nor, nor->program_opcode, to,
					(u32 *)buf, tx_size);
		len -= tx_size;
		to  += tx_size;
		ret += act_wrt;
	}

	/* invalid the data in the AHB buffer. */
	nxp_fspi_invalid(fspi);
	return ret;
}

static ssize_t nxp_fspi_read(struct spi_nor *nor, loff_t from,
		size_t len, u_char *buf)
{
	struct nxp_fspi *fspi = nor->priv;

	/* if necessary, ioremap buffer before AHB read, */
	if (!fspi->ahb_addr) {
		fspi->memmap_offs = fspi->chip_base_addr + from;
		fspi->memmap_len = len > FSPI_MIN_IOMAP ?
				   len : FSPI_MIN_IOMAP;

		fspi->ahb_addr = ioremap_nocache(
					fspi->memmap_phy + fspi->memmap_offs,
					fspi->memmap_len);
		if (!fspi->ahb_addr) {
			dev_err(fspi->dev, "ioremap failed\n");
			return -ENOMEM;
		}
	/* ioremap if the data requested is out of range */
	} else if (fspi->chip_base_addr + from < fspi->memmap_offs
			|| fspi->chip_base_addr + from + len >
			fspi->memmap_offs + fspi->memmap_len) {
		iounmap(fspi->ahb_addr);

		fspi->memmap_offs = fspi->chip_base_addr + from;
		fspi->memmap_len = len > FSPI_MIN_IOMAP ?
				   len : FSPI_MIN_IOMAP;
		fspi->ahb_addr = ioremap_nocache(
					fspi->memmap_phy + fspi->memmap_offs,
					fspi->memmap_len);
		if (!fspi->ahb_addr) {
			dev_err(fspi->dev, "ioremap failed\n");
			return -ENOMEM;
		}
	}

	dev_dbg(fspi->dev, "cmd [%x],read from %p, len:%zd\n",
		nor->read_opcode, fspi->ahb_addr + fspi->chip_base_addr
		+ from - fspi->memmap_offs, len);

	/* Read out the data directly from the AHB buffer.*/
	memcpy_toio(buf, fspi->ahb_addr + fspi->chip_base_addr
		+ from - fspi->memmap_offs, len);

	return len;
}

static int nxp_fspi_erase(struct spi_nor *nor, loff_t offs)
{
	struct nxp_fspi *fspi = nor->priv;
	int ret;

	dev_dbg(nor->dev, "%dKiB at 0x%08x:0x%08x\n",
		nor->mtd.erasesize / 1024, fspi->chip_base_addr, (u32)offs);

	ret = nxp_fspi_runcmd(fspi, nor->erase_opcode, offs, 0);
	if (ret)
		return ret;

	nxp_fspi_invalid(fspi);
	return 0;
}

static int nxp_fspi_prep(struct spi_nor *nor, enum spi_nor_ops ops)
{
	struct nxp_fspi *fspi = nor->priv;
	int ret;

	mutex_lock(&fspi->lock);

	ret = nxp_fspi_clk_prep_enable(fspi);
	if (ret)
		goto err_mutex;

	nxp_fspi_set_base_addr(fspi, nor);
	return 0;

err_mutex:
	mutex_unlock(&fspi->lock);
	return ret;
}

static void nxp_fspi_unprep(struct spi_nor *nor, enum spi_nor_ops ops)
{
	struct nxp_fspi *fspi = nor->priv;

	nxp_fspi_clk_disable_unprep(fspi);
	mutex_unlock(&fspi->lock);
}

static const struct of_device_id nxp_fspi_dt_ids[] = {
	{ .compatible = "nxp,lx2160a-fspi", .data = (void *)&lx2160a_data, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nxp_fspi_dt_ids);

static int nxp_fspi_probe(struct platform_device *pdev)
{
	struct spi_nor_hwcaps hwcaps = {
		.mask = SPINOR_OP_READ_FAST_4B |
			SPINOR_OP_READ_4B |
			SNOR_HWCAPS_PP
	};
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct nxp_fspi *fspi;
	struct resource *res;
	struct spi_nor *nor;
	struct mtd_info *mtd;
	int ret, i = 0;
	int find_node = 0;

	const struct of_device_id *of_id =
			of_match_device(nxp_fspi_dt_ids, &pdev->dev);

	fspi = devm_kzalloc(dev, sizeof(*fspi), GFP_KERNEL);
	if (!fspi)
		return -ENOMEM;

	fspi->nor_num = of_get_child_count(dev->of_node);
	if (!fspi->nor_num || fspi->nor_num > 4)
		return -ENODEV;

	fspi->dev = dev;
	fspi->devtype_data = (struct nxp_fspi_devtype_data *)of_id->data;
	platform_set_drvdata(pdev, fspi);

	/* find the resources */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fspi_base");
	if (!res) {
		dev_err(dev, "fspi_base get resource IORESOURCE_MEM failed\n");
		return -ENODEV;
	}

	fspi->iobase = devm_ioremap_resource(dev, res);
	if (IS_ERR(fspi->iobase))
		return PTR_ERR(fspi->iobase);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "fspi_mmap");
	if (!res) {
		dev_err(dev,
			"fspi_mmap get resource IORESOURCE_MEM failed\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start, resource_size(res),
				     res->name)) {
		dev_err(dev, "can't request region for resource %pR\n", res);
		return -EBUSY;
	}

	fspi->memmap_phy = res->start;

	/* find the clocks */
	fspi->clk_en = devm_clk_get(dev, "fspi_en");
	if (IS_ERR(fspi->clk_en))
		return PTR_ERR(fspi->clk_en);

	fspi->clk = devm_clk_get(dev, "fspi");
	if (IS_ERR(fspi->clk))
		return PTR_ERR(fspi->clk);

	ret = nxp_fspi_clk_prep_enable(fspi);
	if (ret) {
		dev_err(dev, "can not enable the clock\n");
		goto clk_failed;
	}

	/* find the irq */
	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "failed to get the irq: %d\n", ret);
		goto irq_failed;
	}

	ret = devm_request_irq(dev, ret,
			nxp_fspi_irq_handler, 0, pdev->name, fspi);
	if (ret) {
		dev_err(dev, "failed to request irq: %d\n", ret);
		goto irq_failed;
	}

	ret = nxp_fspi_nor_setup(fspi);
	if (ret)
		goto irq_failed;

	if (of_get_property(np, "nxp,fspi-has-second-chip", NULL))
		fspi->has_second_chip = true;

	mutex_init(&fspi->lock);

	find_node = 0;
	/* iterate the subnodes. */
	for_each_available_child_of_node(dev->of_node, np) {
		/* skip the holes */
		if (!fspi->has_second_chip)
			i *= 2;

		nor = &fspi->nor[i];
		mtd = &nor->mtd;

		nor->dev = dev;
		spi_nor_set_flash_node(nor, np);
		nor->priv = fspi;

		/* fill the hooks */
		nor->read_reg = nxp_fspi_read_reg;
		nor->write_reg = nxp_fspi_write_reg;
		nor->read = nxp_fspi_read;
		nor->write = nxp_fspi_write;
		nor->erase = nxp_fspi_erase;

		nor->prepare = nxp_fspi_prep;
		nor->unprepare = nxp_fspi_unprep;

		ret = of_property_read_u32(np, "spi-max-frequency",
				&fspi->clk_rate);
		if (ret < 0)
			goto next_node;

		/* set the chip address for READID */
		nxp_fspi_set_base_addr(fspi, nor);

		ret = of_property_read_u32(np, "spi-rx-bus-width",
				&fspi->spi_rx_bus_width);
		if (ret < 0)
			fspi->spi_rx_bus_width = FSPI_SINGLE_MODE;

		ret = of_property_read_u32(np, "spi-tx-bus-width",
				&fspi->spi_tx_bus_width);
		if (ret < 0)
			fspi->spi_tx_bus_width = FSPI_SINGLE_MODE;

		ret = spi_nor_scan(nor, NULL, &hwcaps);
		if (ret)
			goto next_node;

		ret = mtd_device_register(mtd, NULL, 0);
		if (ret)
			goto next_node;

		/* Set the correct NOR size now. */
		if (fspi->nor_size == 0) {
			fspi->nor_size = mtd->size;

			/* Map the SPI NOR to accessiable address */
			nxp_fspi_set_map_addr(fspi);
		}

		/*
		 * The write is working in the  unit of the TX FIFO,
		 * not in the unit of the SPI NOR's page size.
		 *
		 * So shrink the spi_nor->page_size if it is larger then the
		 * TX FIFO.
		 */
		if (nor->page_size > fspi->devtype_data->txfifo)
			nor->page_size = fspi->devtype_data->txfifo;

		find_node++;
next_node:
		i++;
	}

	if (find_node == 0)
		goto mutex_failed;

	/* finish the rest init. */
	ret = nxp_fspi_nor_setup_last(fspi);
	if (ret)
		goto last_init_failed;

	nxp_fspi_clk_disable_unprep(fspi);
	return 0;

last_init_failed:
	for (i = 0; i < fspi->nor_num; i++) {
		/* skip the holes */
		if (!fspi->has_second_chip)
			i *= 2;
		mtd_device_unregister(&fspi->mtd[i]);
	}
mutex_failed:
	mutex_destroy(&fspi->lock);
irq_failed:
	nxp_fspi_clk_disable_unprep(fspi);
clk_failed:
	dev_err(dev, "NXP FSPI probe failed\n");
	return ret;
}

static int nxp_fspi_remove(struct platform_device *pdev)
{
	struct nxp_fspi *fspi = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < fspi->nor_num; i++) {
		/* skip the holes */
		if (!fspi->has_second_chip)
			i *= 2;
		mtd_device_unregister(&fspi->nor[i].mtd);
	}

	/* disable the hardware */
	writel(FSPI_MCR0_MDIS_MASK, fspi->iobase + FSPI_MCR0);

	mutex_destroy(&fspi->lock);

	if (fspi->ahb_addr)
		iounmap(fspi->ahb_addr);

	return 0;
}

static int nxp_fspi_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int nxp_fspi_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver nxp_fspi_driver = {
	.driver = {
		.name	= "nxp-fspi",
		.bus	= &platform_bus_type,
		.of_match_table = nxp_fspi_dt_ids,
	},
	.probe          = nxp_fspi_probe,
	.remove		= nxp_fspi_remove,
	.suspend	= nxp_fspi_suspend,
	.resume		= nxp_fspi_resume,
};
module_platform_driver(nxp_fspi_driver);

MODULE_DESCRIPTION("NXP FSPI Controller Driver");
MODULE_AUTHOR("NXP Semiconductor");
MODULE_LICENSE("GPL v2");
