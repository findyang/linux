# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Freescale DPAA2 Ethernet controller
#

obj-$(CONFIG_FSL_DPAA2_ETH) += fsl-dpaa2-eth.o

fsl-dpaa2-eth-objs    := dpaa2-eth.o dpaa2-ethtool.o dpni.o
fsl-dpaa2-eth-${CONFIG_DEBUG_FS} += dpaa2-eth-debugfs.o
fsl-dpaa2-eth-${CONFIG_FSL_DPAA2_ETH_CEETM} += dpaa2-eth-ceetm.o

# Needed by the tracing framework
CFLAGS_dpaa2-eth.o := -I$(src)
