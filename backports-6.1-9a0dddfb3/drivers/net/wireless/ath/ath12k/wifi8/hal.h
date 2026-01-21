/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_WIFI8_H
#define ATH12K_HAL_WIFI8_H

#include "../hal.h"
#include "hal_desc.h"
#include "hal_tx.h"
#include "hal_rx.h"
#include "hal_rx_desc.h"

extern const struct ath12k_hw_regs qcn9625_regs;

extern const struct ath12k_hw_hal_params ath12k_wifi8_hw_hal_params_qcn9625;

extern const struct ath12k_hw_version_map ath12k_wifi8_hw_ver_map[];

#define PCIE_WINDOW_REG_ADDRESS			0x3278
#define WINDOW_VALUE_MASK			GENMASK(25, 19)
#define WINDOW_STATIC_MASK			GENMASK(31, 7)
#define WINDOW_DYNAMIC_MASK			GENMASK(6, 0)
#define CE_WINDOW_SHIFT				7
#define UMAC_WINDOW_SHIFT			14

#define HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX    128
#define HAL_DSCP_TID_TBL_SIZE                   24

/* calculate the register address from bar0 of shadow register x */
#define HAL_SHADOW_BASE_ADDR			0x000008fc
#define HAL_SHADOW_NUM_REGS			40
#define HAL_HP_OFFSET_IN_REG_START		1
#define HAL_OFFSET_FROM_HP_TO_TP		4

#define HAL_SHADOW_REG(x) (HAL_SHADOW_BASE_ADDR + (4 * (x)))
#define HAL_REO_QDESC_MAX_PEERID               8191

#define HAL_LINK_DESC_SIZE			(32 << 3)
#define HAL_NUM_MPDUS_PER_LINK_DESC		12
#define HAL_NUM_TX_MSDUS_PER_LINK_DESC		11
#define HAL_NUM_RX_MSDUS_PER_LINK_DESC		11
#define HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC	30

/* WCSS Relative address */
#define HAL_SEQ_WCSS_UMAC_OFFSET		0xf00000
#define HAL_SEQ_WCSS_UMAC_REO_REG		0xf22000
#define HAL_SEQ_WCSS_UMAC_TCL_REG		0xf13000
#define HAL_SEQ_WCSS_UMAC_TQM_REG		0xf08000
#define HAL_SEQ_WCSS_UMAC_WBM_REG		0xf04000

#define HAL_SEQ_WCSS_UMAC_CE0_SRC_REG	0x1B80000
#define HAL_SEQ_WCSS_UMAC_CE0_DST_REG	0x1B81000
#define HAL_SEQ_WCSS_UMAC_CE1_SRC_REG	0x1B82000
#define HAL_SEQ_WCSS_UMAC_CE1_DST_REG	0x1B83000

#define HAL_CE_WFSS_CE_REG_BASE                 0x1B80000
#define HAL_DP_REG_WINDOW_OFFSET                0x00180000

#define HAL_TCL_SW_CONFIG_BANK_ADDR		0x00F130E0

/*TODO: Revisit Default value */
#define HAL_TCL_SW_CONFIG_BANK_DEFAULT_VAL	0x3c43a
#define HAL_TCL_SW_CONFIG_BANK_DEFAULT		0x00F132E0

#define HAL_TCL_LINK_ID_TO_CHIP_ID_MAP		0x9c

/* SW2TCL(x) R0 ring configuration address */
#define HAL_TCL1_RING_CMN_CTRL_REG		0x30
#define HAL_TCL1_RING_DSCP_TID_MAP		0x77C
#define HAL_TCL1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_base_lsb)
#define HAL_TCL1_RING_BASE_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_base_msb)
#define HAL_TCL1_RING_ID(hal)			((hal)->regs->hal_tcl1_ring_id)
#define HAL_TCL1_RING_MISC(hal) \
	((hal)->regs->hal_tcl1_ring_misc)
#define HAL_TCL1_RING_TP_ADDR_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_tp_addr_lsb)
#define HAL_TCL1_RING_TP_ADDR_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_tp_addr_msb)
#define HAL_TCL1_RING_CONSUMER_INT_SETUP_IX0(hal) \
	((hal)->regs->hal_tcl1_ring_consumer_int_setup_ix0)
#define HAL_TCL1_RING_CONSUMER_INT_SETUP_IX1(hal) \
	((hal)->regs->hal_tcl1_ring_consumer_int_setup_ix1)
#define HAL_TCL1_RING_MSI1_BASE_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_base_lsb)
#define HAL_TCL1_RING_MSI1_BASE_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_base_msb)
#define HAL_TCL1_RING_MSI1_DATA(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_data)
#define HAL_TCL2_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl2_ring_base_lsb)
#define HAL_TCL_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_ring_base_lsb)

#define HAL_TCL1_RING_MSI1_BASE_LSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_BASE_LSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MSI1_BASE_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_BASE_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MSI1_DATA_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_DATA(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_BASE_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_BASE_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_ID_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_ID(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_CONSUMER_INT_SETUP_IX0(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_CONSUMER_INT_SETUP_IX1(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_TP_ADDR_LSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_TP_ADDR_LSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_TP_ADDR_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_TP_ADDR_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MISC_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MISC(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })

/* SW2TCL(x) R2 ring pointers (head/tail) address */
#define HAL_TCL1_RING_HP			0x4000
#define HAL_TCL1_RING_TP			0x4004
#define HAL_TCL2_RING_HP			0x4008

#define HAL_TCL1_RING_TP_OFFSET \
		(HAL_TCL1_RING_TP - HAL_TCL1_RING_HP)

/* TCL STATUS ring address */
#define HAL_TCL_STATUS_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_status_ring_base_lsb)
#define HAL_TCL_STATUS_RING_HP			0x4070

/* TCL2SW1_EXCEPTION_RING */
#define HAL_TX_EXCEPTION_RING_BASE_LSB		0x1dec
#define HAL_TX_EXCEPTION_RING_HP		0x4080

/* PPE2TCL1 Ring address */
#define HAL_TCL_PPE2TCL1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_ppe2tcl_ring_base_lsb)
#define HAL_TCL_PPE2TCL1_RING_HP		0x4040

/* PPE2TCL2 Ring address */
#define HAL_TCL_PPE2TCL2_RING_BASE_LSB		0x1a50
#define HAL_TCL_PPE2TCL2_RING_HP		0x4048

/* PPE2TCL3 Ring address */
#define HAL_TCL_PPE2TCL3_RING_BASE_LSB		0x1ae4
#define HAL_TCL_PPE2TCL3_RING_HP		0x4050

/*TQM2SW Ring address */
#define HAL_TQM2SW0_RELEASE_RING_HP		0x3078
#define HAL_TQM2SW1_RELEASE_RING_HP		0x3080

#define HAL_TCL_PPE_INDEX_MAPPING_OFFSET 0x214
#define HAL_TCL_PPE_INDEX_MAPPING_SLOT_SIZE 0x4
#define HAL_TCL_PPE_INDEX_MAPPING_TABLE_n_ADDR(base, n) ((base) + \
			HAL_TCL_PPE_INDEX_MAPPING_OFFSET + \
			(HAL_TCL_PPE_INDEX_MAPPING_SLOT_SIZE * (n)))

/* ASE configuration details */
#define HAL_HW_AST_ENTRY_ALIGN          8
#define HAL_HW_AST_ENTRY_SIZE           32

#define HAL_TCL_ASE_GST_BASE_ADDR_LOW	0xF150B4
#define HAL_TCL_ASE_GST_BASE_ADDR_HIGH	0xF150B8
#define HAL_TCL_ASE_GST_SIZE		0xF150BC
#define HAL_TCL_ASE_SEARCH_CTRL		0xF150C0
#define HAL_TCL_ASE_HASH_KEY_31_0	0xF143A8
#define HAL_TCL_ASE_HASH_KEY_63_32	0xF143AC
#define HAL_TCL_ASE_HASH_KEY_64		0xF143B0

#define HAL_TCL_ASE_GST_BASE_ADDR_LOW_MASK			GENMASK(31, 0)
#define HAL_TCL_ASE_GST_BASE_ADDR_HIGH_MASK			GENMASK(7, 0)
#define HAL_TCL_ASE_GST_SIZE_MASK				GENMASK(19, 0)
#define HAL_TCL_ASE_SEARCH_CTRL_MAX_SEARCH			GENMASK(7, 0)
#define HAL_TCL_ASE_SEARCH_CTRL_CACHE_DISABLE			BIT(9)
#define HAL_TCL_ASE_SEARCH_CTRL_CACHE_FAILURES_ENABLE		BIT(10)

/* WBM PPE Release Ring address */
#define HAL_TQM_PPE_RELEASE_RING_BASE_LSB(hal) \
	((hal)->regs->hal_ppe_rel_ring_base)
#define HAL_TQM_PPE_RELEASE_RING_HP	0xb88

#define HAL_TCL_ENTRANCE_FROM_PPE_RING_SIZE (8 * sizeof(u32))

/* TODO: CORE DP: cahnge REO error ring assignment as required */
#define HAL_REO_DEST_REL_ERR_RING_NUM 0

/* REO2SW(x) R0 ring configuration address */
#define HAL_REO1_GEN_ENABLE			0x00000000
#define HAL_REO1_MISC_CTRL_ADDR(hal) \
	((hal)->regs->hal_reo1_misc_ctrl_addr)
#define HAL_REO1_DEST_RING_CTRL_AP_IX_0		0x1be0
#define HAL_REO1_DEST_RING_CTRL_AP_IX_1		0x1be4
#define HAL_REO1_DEST_RING_CTRL_AP_IX_2		0x1be8
#define HAL_REO1_DEST_RING_CTRL_AP_IX_3		0x1bec
#define HAL_REO1_DEST_RING_CTRL_AP_IX_4		0x1bf0
#define HAL_REO1_DEST_RING_CTRL_AP_IX_5		0x1bf4

#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_0	0x1c28
#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_1	0x1c2c
#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_2	0x1c30

#define HAL_REO_IX_FIELD_WIDTH 5
#define HAL_REO_ERROR_CODE_RBM_OVERRIDE		0x1bd4
#define HAL_REO_RXDMA_ERROR_CODE_RBM_OVERRIDE	0x1c58

#define HAL_REO_RDI_CTRL_SEL_WITH_TID				0x00000048

#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_0	0x1bf8
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_1	0x1bfc
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_2	0x1c00
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_3	0x1c04
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_4	0x1c08
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_5	0x1c0c

#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_0	0x1c34
#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_1	0x1c38
#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_2	0x1c3c

#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_0	0x1c40
#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_1	0x1c44
#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_2	0x1c48
#define HAL_REO_MISC_CTL_AP		0x1c4c
#define HAL_REO_RXDMA_ERROR_CODE_REORDER	0x1c50
#define HAL_REO_RXDMA_ERROR_CODE_REO_DELINK	0x1c54
#define HAL_REO_ERROR_CODE_REO_DELINK		0x1bd0
#define HAL_BAR_REO_ERROR_CODE_DELINK		0x1df4

#define HAL_REO1_MISC_CFG_1				0x190c
#define HAL_REO1_MISC_CFG_1_REO_ERR_DELINK_ENABLE	BIT(3)
#define HAL_REO1_MISC_CFG_1_RXDMA_ERR_DELINK_ENABLE	BIT(4)
#define HAL_REO1_MISC_CFG_1_REO_MSDU_FETCH_OPTIMIZE	BIT(1)
#define HAL_REO1_MISC_CFG_1_REO_MSDU_LINK_SHARING_EN	BIT(10)

#define HAL_REO1_MISC_CFG_2				0x1c7c
#define HAL_REO1_MISC_CFG_2_BAR_REO_ERR_DELINK_ENABLE	BIT(12)

#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_IX_0	0x1c5c
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_IX_1	0x1c60
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_IX_2	0x1c64
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_IX_3	0x1c68
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_0	0x1c6c
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_1	0x1c70
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_2	0x1c74
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_3	0x1c78
#define HAL_REO_MISC_CFG_BN_2			0x1c7c
#define HAL_REO_PPE_DEST_OVERRIDE		0x1c80
#define HAL_REO_ERROR_RING_CFG_FOR_DEST_IX_0	0x1c84
#define HAL_REO_ERROR_RING_CFG_FOR_DEST_IX_1	0x1c88
#define HAL_REO_ERROR_RING_CFG_FOR_DEST_IX_2	0x1c8c
#define HAL_REO_FW_MGMT_ROUTING_CFG		0x1c90
#define HAL_REO_RX_SDWF_CFG			0x1c94
#define HAL_REO_BACKPRESSURE_DROP_EN		0x1c98
#define HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0	0x1c9c
#define HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1	0x1ca0
#define HAL_REO_AGING_FLUSH_LOW_LATENCY_OPTION	\
	HWIO_REO_R0_AGING_FLUSH_LOW_LATENCY_OPTION_OFFS

#define HAL_REO1_QDESC_ADDR(hal)        ((hal)->regs->hal_reo1_qdesc_addr)
#define HAL_REO1_QDESC_MAX_PEERID(hal)  ((hal)->regs->hal_reo1_qdesc_max_peerid)
#define HAL_REO1_SW_COOKIE_CFG0(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg0)
#define HAL_REO1_SW_COOKIE_CFG1(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg1)

#define REO2PPE_DST_RING_MAP 11
#define REO_DEST_CTRL_IX_0_RING6_MAP_MASK 0xF
#define REO_DEST_CTRL_IX_0_RING6_MAP_SHFT 24

#define HAL_REO1_QDESC_ADDR(hal)                ((hal)->regs->hal_reo1_qdesc_addr)
#define HAL_REO1_QDESC_MAX_PEERID(hal)  ((hal)->regs->hal_reo1_qdesc_max_peerid)
#define HAL_REO1_SW_COOKIE_CFG0(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg0)
#define HAL_REO1_SW_COOKIE_CFG1(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg1)
#define HAL_REO1_QDESC_LUT_BASE0(hal)	((hal)->regs->hal_reo1_qdesc_lut_base0)
#define HAL_REO1_QDESC_LUT_BASE1(hal)	((hal)->regs->hal_reo1_qdesc_lut_base1)
#define HAL_REO1_RING_BASE_LSB(hal)	((hal)->regs->hal_reo1_ring_base_lsb)
#define HAL_REO1_RING_BASE_MSB(hal)	((hal)->regs->hal_reo1_ring_base_msb)
#define HAL_REO1_RING_ID(hal)		((hal)->regs->hal_reo1_ring_id)
#define HAL_REO1_RING_MISC(hal)		((hal)->regs->hal_reo1_ring_misc)
#define HAL_REO1_RING_HP_ADDR_LSB(hal)	((hal)->regs->hal_reo1_ring_hp_addr_lsb)
#define HAL_REO1_RING_HP_ADDR_MSB(hal)	((hal)->regs->hal_reo1_ring_hp_addr_msb)
#define HAL_REO1_RING_PRODUCER_INT_SETUP(hal) \
	((hal)->regs->hal_reo1_ring_producer_int_setup)
#define HAL_REO1_RING_MSI1_BASE_LSB(hal)	\
	((hal)->regs->hal_reo1_ring_msi1_base_lsb)
#define HAL_REO1_RING_MSI1_BASE_MSB(hal)	\
	((hal)->regs->hal_reo1_ring_msi1_base_msb)
#define HAL_REO1_RING_MSI1_DATA(hal)	((hal)->regs->hal_reo1_ring_msi1_data)
#define HAL_REO2_RING_BASE_LSB(hal)	((hal)->regs->hal_reo2_ring_base)
#define HAL_REO1_AGING_THRESH_IX_0(hal)	((hal)->regs->hal_reo1_aging_thres_ix0)
#define HAL_REO1_AGING_THRESH_IX_1(hal)	((hal)->regs->hal_reo1_aging_thres_ix1)
#define HAL_REO1_AGING_THRESH_IX_2(hal)	((hal)->regs->hal_reo1_aging_thres_ix2)
#define HAL_REO1_AGING_THRESH_IX_3(hal)	((hal)->regs->hal_reo1_aging_thres_ix3)

#define HAL_REO1_RING_MISC_OFFSET \
		(HAL_REO1_RING_MISC(hal) - HAL_REO1_RING_BASE_LSB(hal))

#define HAL_REO1_REO2PPE_DST_VAL		0x2000
#define HAL_REO1_REO2PPE_DST_INFO		0x00000cf0

#define HAL_WIFI8_HASH_ROUTING_RING_SW0 0
#define HAL_WIFI8_HASH_ROUTING_RING_SW1 1
#define HAL_WIFI8_HASH_ROUTING_RING_SW2 2
#define HAL_WIFI8_HASH_ROUTING_RING_SW3 3
#define HAL_WIFI8_HASH_ROUTING_RING_SW4 4
#define HAL_WIFI8_HASH_ROUTING_RING_REL 5
#define HAL_WIFI8_HASH_ROUTING_RING_FW  6
#define HAL_WIFI8_HASH_ROUTING_RING_SW5 7
#define HAL_WIFI8_HASH_ROUTING_RING_SW6 8
#define HAL_WIFI8_HASH_ROUTING_RING_SW7 9
#define HAL_WIFI8_HASH_ROUTING_RING_SW8 10
#define HAL_WIFI8_HASH_ROUTING_RING_SW9 11
#define HAL_WIFI8_HASH_ROUTING_RING_SW10 16

/* REO2SW(x) R2 ring pointers (head/tail) address */
/* REO2SW(x) R2 ring pointers (head/tail) address */
#define HAL_REO1_RING_HP                       0x3078
#define HAL_REO1_RING_TP                       0x307c
#define HAL_REO2_RING_HP                       0x3080

#define HAL_REO1_RING_TP_OFFSET			(HAL_REO1_RING_TP - HAL_REO1_RING_HP)

/* REO2SW0 ring configuration address */
#define HAL_REO_SW0_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo2_sw0_ring_base)

/* REO2SW0 R2 ring pointer (head/tail) address */
#define HAL_REO_SW0_RING_HP			0x30d0

/* REO CMD R0 address */
#define HAL_REO_CMD_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo_cmd_ring_base)

/* REO CMD R2 address */
#define HAL_REO_CMD_HP				0x3020

/* SW2REO R0 address */
#define	HAL_SW2REO_RING_BASE_LSB(hal) \
	((hal)->regs->hal_sw2reo_ring_base)
#define HAL_SW2REO1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_sw2reo1_ring_base)

/* SW2REO R2 address */
#define HAL_SW2REO_RING_HP			0x3028
#define HAL_SW2REO1_RING_HP			0x3030

/* CE ring R0 address */
#define HAL_CE_SRC_RING_BASE_LSB	0x0
#define HAL_CE_DST_RING_BASE_LSB	0x0
#define HAL_CE_DST_STATUS_RING_BASE_LSB		0x58
#define HAL_CE_DST_RING_CTRL			0xb0

/* CE ring R2 address */
#define HAL_CE_DST_RING_HP		0x400
#define HAL_CE_DST_STATUS_RING_HP	0x408

/* REO status address */
#define HAL_REO_STATUS_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo_status_ring_base)
#define HAL_REO_STATUS_HP			0x312c

/* REO2PPE address */
#define HAL_REO2PPE_RING_BASE_LSB(hal) \
		((hal)->regs->hal_reo2ppe_ring_base)
#define HAL_REO2PPE_HP				0x30d8

/* WBM Idle R0 address */
#define HAL_WBM_IDLE_LINK_RING_BASE_LSB(hal) \
	((hal)->regs->hal_wbm_idle_ring_base_lsb)
#define HAL_WBM_IDLE_LINK_RING_MISC_ADDR(hal) \
	((hal)->regs->hal_wbm_idle_ring_misc_addr)
#define HAL_WBM_R0_IDLE_LIST_CONTROL_ADDR(hal) \
	((hal)->regs->hal_wbm_r0_idle_list_cntl_addr)
#define HAL_WBM_R0_IDLE_LIST_SIZE_ADDR(hal) \
	((hal)->regs->hal_wbm_r0_idle_list_size_addr)
#define HAL_WBM_SCATTERED_RING_BASE_LSB(hal) \
	((hal)->regs->hal_wbm_scattered_ring_base_lsb)
#define HAL_WBM_SCATTERED_RING_BASE_MSB(hal) \
	((hal)->regs->hal_wbm_scattered_ring_base_msb)
#define HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0(hal) \
	((hal)->regs->hal_wbm_scattered_desc_head_info_ix0)
#define HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX1(hal) \
	((hal)->regs->hal_wbm_scattered_desc_head_info_ix1)
#define HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX0(hal) \
	((hal)->regs->hal_wbm_scattered_desc_tail_info_ix0)
#define HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX1(hal) \
	((hal)->regs->hal_wbm_scattered_desc_tail_info_ix1)
#define HAL_WBM_SCATTERED_DESC_PTR_HP_ADDR(hal) \
	((hal)->regs->hal_wbm_scattered_desc_ptr_hp_addr)

/* WBM Idle R2 address */
#define HAL_WBM_IDLE_LINK_RING_HP		0x3040

/* SW2WBM R0 release address */
#define HAL_WBM_SW_RELEASE_RING_BASE_LSB	0x24c
#define HAL_WBM_FW_RELEASE_RING_BASE_LSB	0x2c4

/* SW2WBM R2 release address */
#define HAL_WBM_SW_RELEASE_RING_HP		0x3010
#define HAL_WBM_FW_RELEASE_RING_HP		0x3018

/* WBM2SW R0 release address */
#define HAL_TQM2SW0_RELEASE_RING_BASE_LSB	0x73c
#define HAL_TQM2SW1_RELEASE_RING_BASE_LSB	0x7b4

/* TQM cookie config address and mask */
#define HAL_TQM_TX_COMPLETION_MISC_CFG	0x1310
#define HAL_TQM_TX_COMPLETION_MISC_CFG_RELEASE_PATH_EN	0x1
#define HAL_TQM_TX_COMPLETION_MISC_CFG_ERR_PATH_EN	0x2
#define HAL_TQM_TX_COMPLETION_MISC_CFG_CONV_IND_EN	0x8

#define HAL_TQM_SW_COOKIE_CFG0		0x1314
#define HAL_TQM_SW_COOKIE_CFG0_CMEM_LUT_BASE_ADDR_31_0	0xffffffff

#define HAL_TQM_SW_COOKIE_CFG1		0x1318
#define HAL_TQM_SW_COOKIE_CFG1_CMEM_BASE_ADDR_MSB	0xff
#define HAL_TQM_SW_COOKIE_CFG1_COOKIE_PPT_MSB		0x1f00
#define HAL_TQM_SW_COOKIE_CFG1_COOKIE_SPT_MSB		0x3e000
#define HAL_TQM_SW_COOKIE_CFG1_ALIGN	0x40000

#define HAL_TQM_SW_COOKIE_CONVERT_CFG	0x1220
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW0_EN	0x2
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW1_EN	0x4
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW2_EN	0x8
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW3_EN	0x10
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW4_EN	0x20
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW5_EN	0x40
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW6_EN	0x80
#define HAL_TQM_SW_COOKIE_CONV_CFG_GLOBAL_EN	0x100

#define HAL_TQM_SW_COOKIE_CONVERT_CFG2  0x1224
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW7_EN	BIT(0x0)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW8_EN	BIT(0x1)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN	BIT(0x2)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW_ROAMING0_EN	BIT(0x3)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW_ROAMING1_EN	BIT(0x4)

/* SW2WBM buf ring */
#define HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_BASE_LSB     0x00001c38
#define HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_BASE_LSB     0x00001cb0
#define HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_HP           0x00003188
#define HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_HP           0x00003190

/* WBM idle buf pool */
#define HAL_WBM_SW_IDLE_BUF_RING_LSB                   0x000020e8
#define HAL_WBM_SW_IDLE_BUF_RING_HP                    0x000031d8

/* TCL ring field mask and offset */
#define HAL_TCL1_RING_BASE_MSB_RING_SIZE	0xfffff00
#define HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB	0xff
#define HAL_TCL1_RING_ID_ENTRY_SIZE	0xff
#define HAL_TCL1_RING_MISC_MSI_RING_ID_DISABLE		0x1
#define HAL_TCL1_RING_MISC_MSI_LOOPCNT_DISABLE		0x2
#define HAL_TCL1_RING_MISC_MSI_SWAP	0x8
#define HAL_TCL1_RING_MISC_HOST_FW_SWAP		0x10
#define HAL_TCL1_RING_MISC_DATA_TLV_SWAP	0x20
#define HAL_TCL1_RING_MISC_SRNG_ENABLE	0x40
#define HAL_TCL1_RING_MISC_TRANSACTION_TYPE		0x400000
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_INTR_TMR_THOLD	0xffff0000
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_BATCH_COUNTER_THOLD	0x7fff
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD		0xffff
#define HAL_TCL1_RING_MSI1_BASE_MSB_MSI1_ENABLE		0x100
#define HAL_TCL1_RING_MSI1_BASE_MSB_ADDR		0xff
#define HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN		0x80000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP		GENMASK(31, 0)
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP0		0x7
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP1		0x38
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP2		0x1c0
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP3		0xe00
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP4		0x7000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP5		0x38000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP6		0x1c0000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP7		0xe00000

/* TODO: CORE DP TX needs any update from Host */
#define HAL_TCL1_CMN_CONFIG	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x38)
#define HAL_TCL1_CMN_CONFIG_PPE	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x44)
#define HAL_TCL1_RBM_MAPPING0	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0xd8)
#define HAL_TCL1_RBM_MAPPING1	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0xdc)
#define HAL_TCL1_LINK_ID_TO_CHIP_ID_MAP	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x9c)

/* REO ring field mask and offset */
#define HAL_REO1_RING_BASE_MSB_RING_SIZE		0xfffff00
#define HAL_REO1_RING_BASE_MSB_RING_BASE_ADDR_MSB	0xff
#define HAL_REO1_RING_ID_RING_ID	0xff00
#define HAL_REO1_RING_ID_ENTRY_SIZE	0xff
#define HAL_REO1_RING_MISC_MSI_SWAP	0x8
#define HAL_REO1_RING_MISC_HOST_FW_SWAP		0x10
#define HAL_REO1_RING_MISC_DATA_TLV_SWAP	0x20
#define HAL_REO1_RING_MISC_SRNG_ENABLE		0x40
#define HAL_REO1_RING_PRDR_INT_SETUP_INTR_TMR_THOLD	0xffff0000
#define HAL_REO1_RING_PRDR_INT_SETUP_BATCH_COUNTER_THOLD	0x7fff
#define HAL_REO1_RING_MSI1_BASE_MSB_MSI1_ENABLE		0x100
#define HAL_REO1_RING_MSI1_BASE_MSB_ADDR		0xff
#define HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING		BIT(5)

#define HAL_REO1_MISC_CTL_NULL_BUFFER_ERROR_DST_RING	0xf8000
#define HAL_REO1_MISC_CTL_SOFT_REORDER_DST_RING		0x7c00
#define HAL_REO1_MISC_CTL_BAR_DST_RING		0x3e0
#define HAL_REO1_MISC_CTL_FRAG_DST_RING		0x1f

#define HAL_REO1_GEN_ENABLE_AGING_LIST_ENABLE		0x4
#define HAL_REO1_GEN_ENABLE_AGING_FLUSH_ENABLE		0x8
#define HAL_REO1_SW_COOKIE_CFG_CMEM_BASE_ADDR_MSB	0xff
#define HAL_REO1_SW_COOKIE_CFG_COOKIE_PPT_MSB		0x1f00
#define HAL_REO1_SW_COOKIE_CFG_COOKIE_SPT_MSB		0x3e000
#define HAL_REO1_SW_COOKIE_CFG_ALIGN	0x40000
#define HAL_REO1_SW_COOKIE_CFG_ENABLE		0x80000
#define HAL_REO1_SW_COOKIE_CFG_GLOBAL_ENABLE	0x100000
#define HAL_REO_QDESC_ADDR_READ_LUT_ENABLE		0x80
#define HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY	0x40
#define HAL_REO_QLUT_REG_BASE_ADDR			GENMASK(39, 8)

/* CE ring bit field mask and shift */
#define HAL_CE_DST_R0_DEST_CTRL_MAX_LEN			GENMASK(15, 0)

#define HAL_ADDR_LSB_REG_MASK				0xffffffff

#define HAL_ADDR_MSB_REG_SHIFT				32

/* WBM ring bit field mask and shift */
#define HAL_WBM_LINK_DESC_IDLE_LIST_MODE	0x1
#define HAL_WBM_SCATTER_BUFFER_SIZE		0x7fc
#define HAL_WBM_SCATTER_RING_SIZE_OF_IDLE_LINK_DESC_LIST	0xffff0000
#define HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32		0xff
#define HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG		0xffffff00

#define HAL_WBM_SCATTERED_DESC_HEAD_P_OFFSET_IX1		0x1fff00
#define HAL_WBM_SCATTERED_DESC_TAIL_P_OFFSET_IX1	0x1fff00

#define HAL_WBM_IDLE_LINK_RING_MISC_SRNG_ENABLE		BIT(6)
#define HAL_WBM_IDLE_LINK_RING_MISC_RIND_ID_DISABLE	BIT(0)

#define BASE_ADDR_MATCH_TAG_VAL 0x5

#define HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO_CMD_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO_STATUS_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO2PPE_RING_BASE_MSB_RING_SIZE		0xffffffff
#define HAL_PPE2TCL_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_SW2TCL1_CMD_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_TX_EXCEPTION_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_CE_SRC_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_CE_DST_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_TQM2SW_RELEASE_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_RXDMA_RING_MAX_SIZE				0x000fffff
#define HAL_RXDMA_RING_MAX_SIZE_BE			0x000fffff
#define HAL_WBM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE	0x0000ffff

#define HAL_WBM2SW_REL_ERR_RING_NUM 5
#define HAL_WBM2SW_PPEDS_TX_CMPLN_MAP_ID 11
#define HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM 6

#define HAL_RX_MAX_BA_WINDOW	256

#define MIN_VI_REORDER_TIMEOUT_MS 100
#define MAX_VI_REORDER_TIMEOUT_MS 600
#define HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC	(100 * 1000)
#define HAL_DEFAULT_VO_REO_TIMEOUT_USEC		(40 * 1000)

#define HAL_SRNG_TLV_HDR_TAG		GENMASK(9, 1)
#define HAL_SRNG_TLV_HDR_LEN		GENMASK(25, 10)

#define HAL_SRNG_DESC_LOOP_CNT		0xf0000000

#define HAL_REO_CMD_FLG_NEED_STATUS		BIT(0)
#define HAL_REO_CMD_FLG_STATS_CLEAR		BIT(1)
#define HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER	BIT(2)
#define HAL_REO_CMD_FLG_FLUSH_RELEASE_BLOCKING	BIT(3)
#define HAL_REO_CMD_FLG_FLUSH_NO_INVAL		BIT(4)
#define HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS	BIT(5)
#define HAL_REO_CMD_FLG_FLUSH_ALL		BIT(6)
#define HAL_REO_CMD_FLG_UNBLK_RESOURCE		BIT(7)
#define HAL_REO_CMD_FLG_UNBLK_CACHE		BIT(8)
#define HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC	BIT(9)

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO0_UPD_* fields */
#define HAL_REO_CMD_UPD0_RX_QUEUE_NUM		BIT(8)
#define HAL_REO_CMD_UPD0_VLD			BIT(9)
#define HAL_REO_CMD_UPD0_ALDC			BIT(10)
#define HAL_REO_CMD_UPD0_DIS_DUP_DETECTION	BIT(11)
#define HAL_REO_CMD_UPD0_SOFT_REORDER_EN	BIT(12)
#define HAL_REO_CMD_UPD0_AC			BIT(13)
#define HAL_REO_CMD_UPD0_BAR			BIT(14)
#define HAL_REO_CMD_UPD0_RETRY			BIT(15)
#define HAL_REO_CMD_UPD0_CHECK_2K_MODE		BIT(16)
#define HAL_REO_CMD_UPD0_OOR_MODE		BIT(17)
#define HAL_REO_CMD_UPD0_BA_WINDOW_SIZE		BIT(18)
#define HAL_REO_CMD_UPD0_PN_CHECK		BIT(19)
#define HAL_REO_CMD_UPD0_EVEN_PN		BIT(20)
#define HAL_REO_CMD_UPD0_UNEVEN_PN		BIT(21)
#define HAL_REO_CMD_UPD0_PN_HANDLE_ENABLE	BIT(22)
#define HAL_REO_CMD_UPD0_PN_SIZE		BIT(23)
#define HAL_REO_CMD_UPD0_IGNORE_AMPDU_FLG	BIT(24)
#define HAL_REO_CMD_UPD0_SVLD			BIT(25)
#define HAL_REO_CMD_UPD0_SSN			BIT(26)
#define HAL_REO_CMD_UPD0_SEQ_2K_ERR		BIT(27)
#define HAL_REO_CMD_UPD0_PN_ERR			BIT(28)
#define HAL_REO_CMD_UPD0_PN_VALID		BIT(29)
#define HAL_REO_CMD_UPD0_PN			BIT(30)

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO1_* fields */
#define HAL_REO_CMD_UPD1_VLD			BIT(16)
#define HAL_REO_CMD_UPD1_ALDC			GENMASK(18, 17)
#define HAL_REO_CMD_UPD1_DIS_DUP_DETECTION	BIT(19)
#define HAL_REO_CMD_UPD1_SOFT_REORDER_EN	BIT(20)
#define HAL_REO_CMD_UPD1_AC			GENMASK(22, 21)
#define HAL_REO_CMD_UPD1_BAR			BIT(23)
#define HAL_REO_CMD_UPD1_RETRY			BIT(24)
#define HAL_REO_CMD_UPD1_CHECK_2K_MODE		BIT(25)
#define HAL_REO_CMD_UPD1_OOR_MODE		BIT(26)
#define HAL_REO_CMD_UPD1_PN_CHECK		BIT(27)
#define HAL_REO_CMD_UPD1_EVEN_PN		BIT(28)
#define HAL_REO_CMD_UPD1_UNEVEN_PN		BIT(29)
#define HAL_REO_CMD_UPD1_PN_HANDLE_ENABLE	BIT(30)
#define HAL_REO_CMD_UPD1_IGNORE_AMPDU_FLG	BIT(31)

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO2_* fields */
#define HAL_REO_CMD_UPD2_SVLD			BIT(10)
#define HAL_REO_CMD_UPD2_SSN			GENMASK(22, 11)
#define HAL_REO_CMD_UPD2_SEQ_2K_ERR		BIT(23)
#define HAL_REO_CMD_UPD2_PN_ERR			BIT(24)

/**
 * enum hal_wifi8_rx_buf_return_buf_manager - manager for returned rx buffers
 *
 * @HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST: Buffer returned to WBM idle buffer list
 * @HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 0 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_FW_CHIP1_BM: Buffer returned to chip 1 FW
 * @HAL_RX_BUF_RBM_FW_CHIP2_BM: Buffer returned to chip 2 FW
 * @HAL_RX_BUF_RBM_FW_CHIP0_BM: Buffer returned to chip 0 FW
 * @HAL_RX_BUF_RBM_SW0_BM: Buffer returned to SW ring 0
 * @HAL_RX_BUF_RBM_SW1_BM: Buffer returned to SW ring 1
 * @HAL_RX_BUF_RBM_SW2_BM: Buffer returned to SW ring 2
 * @HAL_RX_BUF_RBM_SW3_BM: Buffer returned to SW ring 3
 * @HAL_RX_BUF_RBM_SW4_BM: Buffer returned to SW ring 4
 * @HAL_RX_BUF_RBM_SW5_BM: Buffer returned to SW ring 5
 * @HAL_RX_BUF_RBM_SW6_BM: Buffer returned to SW ring 6
 * @HAL_RX_BUF_RBM_FW_CHIP3_BM: Buffer returned to chip 3 FW
 * @HAL_RX_BUF_RBM_SW7_BM: Buffer returned to SW ring 7
 * @HAL_RX_BUF_RBM_FW_CHIP4_BM: Buffer returned to chip 4 FW
 *
 * Description:
 *	Consumer: WBM
 *	Producer: SW/FW
 *
 *	In case of 'NULL' pointer, this field is set to 0.
 *
 *	Indicates to which buffer manager the buffer, MSDU_EXTENSION descriptor,
 *	or link descriptor being pointed to shall be returned after the frame
 *	has been processed. It is used by WBM for routing purposes.
 *
 *	<legal 0-14>
 */

enum hal_wifi8_rx_buf_return_buf_manager {
	HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST,
	HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_FW_CHIP1_BM,
	HAL_RX_BUF_RBM_FW_CHIP2_BM,
	HAL_RX_BUF_RBM_FW_CHIP0_BM,
	HAL_RX_BUF_RBM_SW0_BM,
	HAL_RX_BUF_RBM_SW1_BM,
	HAL_RX_BUF_RBM_SW2_BM,
	HAL_RX_BUF_RBM_SW3_BM,
	HAL_RX_BUF_RBM_SW4_BM,
	HAL_RX_BUF_RBM_SW5_BM,
	HAL_RX_BUF_RBM_SW6_BM,
	HAL_RX_BUF_RBM_FW_CHIP3_BM,
	HAL_RX_BUF_RBM_SW7_BM,
	HAL_RX_BUF_RBM_FW_CHIP4_BM,
};

/* Add any other errors here and return them in
 * ath12k_hal_rx_desc_get_err().
 */

struct hal_wbm_idle_scatter_list {
	dma_addr_t paddr;
	struct hal_wbm_link_desc *vaddr;
};

/* TODO: confirm src_info relacement from HW DESC */
struct rx_mpdu_desc_info {
	union {
		struct {
			u32 msdu_count				:  8,
			    fragment_flag			:  1,
			    mpdu_retry_bit			:  1,
			    ampdu_flag				:  1,
			    bar_frame				:  1,
			    pn_fields_contain_valid_info	:  1,
			    raw_mpdu				:  1,
			    more_fragment_flag			:  1,
			    vdev_id                             :  8,
			    reserved				:  2,
			    flow_idx_invalid                    :  1,
			    flow_idx_timeout                    :  1,
			    mpdu_qos_control_valid		:  1,
			    tid					:  4;
		};
		u32 info0;
	};
	union {
		u32 peer_meta_data;
		struct {
			u32 peer_id		: 16,
			flow_metadata		: 16;
		} flow_info;
	};
	union {
		struct {
			u32 mgmt_pkt				:  1,
			    rxdma_push_reason			:  2,
			    rxdma_error_code			:  5,
			    reo_dest_buffer_type		:  1,
			    release_source_module		:  3,
			    msdu_link_desc_index		:  4,
			    ll_pkt				:  1,
			    high_priority_pkt			:  1,
			    src_link_id				:  3,
			    reo_push_reason			:  2,
			    reo_error_code			:  5,
			    groupcast_mpdu			:  1;
		};
		u32 info1;
	};
};

struct hal_rx_msdu_desc_info {
	u32 first_msdu		        :  1,
	    last_msdu			:  1,
	    msdu_continuation           :  1,
	    msdu_length                 : 14,
	    msdu_drop                   :  1,
	    sa_is_valid                 :  1,
	    da_is_valid                 :  1,
	    da_is_mcbc                  :  1,
	    l3_header_padding_msb       :  1,
	    tcp_udp_chksum_fail         :  1,
	    ip_chksum_fail              :  1,
	    fr_ds                       :  1,
	    to_ds                       :  1,
	    intra_bss                   :  1,
	    dest_chip_id                :  2,
	    ra_is_mcbc			:  1,
	    reserved			:  2;
};

struct rx_tlv_info_1 {
	u32 freq;
	u32 decap			: 2,
	    rate_mcs			: 4,
	    nss				: 7,
	    sgi				: 2,
	    is_decrypted		: 1,
	    mesh_ctrl_present		: 1,
	    pkt_type			: 4,
	    bw				: 3;
};

struct hal_rx_spd_data {
	union {
		u64 info4;
		u8 *vaddr;
	};
	union {
		u64 info3;
		struct sk_buff *msdu;
	};

	struct rx_mpdu_desc_info rx_mpdu_info;

	union {
		u32 info2;
		struct hal_rx_msdu_desc_info rx_msdu_info;
	};

	struct rx_tlv_info_1 tlv_info;

	union {
		u32 info0;
		struct {
			u32 cookie_conversion_status	:  1,
			    reo_delink_error		:  1,
			    sw_buffer_cookie		:  20,
			    reserved_6a			:  4,
			    c_tdma_lut_ptr		:  6;
		};
	};
	union {
		u32 info1;
		struct {
			u32 phy_lmac_latency		:  8,
			    umac_latency		:  8,
			    sw_exception		:  1,
			    backpressure_drop		:  1,
			    flow_idx_valid		:  1,
			    rx_sawf_msdu_dropped	:  1,
			    ring_id			:  8,
			    looping_count		:  4;
		};
	};
	u64 rsvd1;
	u64 rsvd2;
} __packed;

static_assert(sizeof(struct hal_rx_spd_data) == 64,
	      "size of struct hal_rx_spd_data is not 64 bytes!");

struct hal_reo_status_queue_stats {
	u16 ssn;
	u16 curr_idx;
	u32 pn[4];
	u32 last_rx_queue_ts;
	u32 last_rx_dequeue_ts;
	u32 rx_bitmap[8]; /* Bitmap from 0-255 */
	u32 curr_mpdu_cnt;
	u32 curr_msdu_cnt;
	u16 fwd_due_to_bar_cnt;
	u16 dup_cnt;
	u32 frames_in_order_cnt;
	u32 num_mpdu_processed_cnt;
	u32 num_msdu_processed_cnt;
	u32 total_num_processed_byte_cnt;
	u32 late_rx_mpdu_cnt;
	u32 reorder_hole_cnt;
	u8 timeout_cnt;
	u8 bar_rx_cnt;
	u8 num_window_2k_jump_cnt;
};

struct hal_reo_status_flush_queue {
	bool err_detected;
};

enum hal_reo_status_flush_cache_err_code {
	HAL_REO_STATUS_FLUSH_CACHE_ERR_CODE_SUCCESS,
	HAL_REO_STATUS_FLUSH_CACHE_ERR_CODE_IN_USE,
	HAL_REO_STATUS_FLUSH_CACHE_ERR_CODE_NOT_FOUND,
};

struct hal_reo_status_flush_cache {
	bool err_detected;
	enum hal_reo_status_flush_cache_err_code err_code;
	bool cache_controller_flush_status_hit;
	u8 cache_controller_flush_status_desc_type;
	u8 cache_controller_flush_status_client_id;
	u8 cache_controller_flush_status_err;
	u8 cache_controller_flush_status_cnt;
};

enum hal_reo_status_unblock_cache_type {
	HAL_REO_STATUS_UNBLOCK_BLOCKING_RESOURCE,
	HAL_REO_STATUS_UNBLOCK_ENTIRE_CACHE_USAGE,
};

struct hal_reo_status_unblock_cache {
	bool err_detected;
	enum hal_reo_status_unblock_cache_type unblock_type;
};

struct hal_reo_status_flush_timeout_list {
	bool err_detected;
	bool list_empty;
	u16 release_desc_cnt;
	u16 fwd_buf_cnt;
};

enum hal_reo_threshold_idx {
	HAL_REO_THRESHOLD_IDX_DESC_COUNTER0,
	HAL_REO_THRESHOLD_IDX_DESC_COUNTER1,
	HAL_REO_THRESHOLD_IDX_DESC_COUNTER2,
	HAL_REO_THRESHOLD_IDX_DESC_COUNTER_SUM,
};

struct hal_reo_status_desc_thresh_reached {
	enum hal_reo_threshold_idx threshold_idx;
	u32 link_desc_counter0;
	u32 link_desc_counter1;
	u32 link_desc_counter2;
	u32 link_desc_counter_sum;
};

struct hal_reo_status {
	struct hal_reo_status_header uniform_hdr;
	u8 loop_cnt;
	union {
		struct hal_reo_status_queue_stats queue_stats;
		struct hal_reo_status_flush_queue flush_queue;
		struct hal_reo_status_flush_cache flush_cache;
		struct hal_reo_status_unblock_cache unblock_cache;
		struct hal_reo_status_flush_timeout_list timeout_list;
		struct hal_reo_status_desc_thresh_reached desc_thresh_reached;
	} u;
};

#define HAL_AST_ENTRY_INFO0_MAC_ADDR_31_0		GENMASK(31, 0)
#define HAL_AST_ENTRY_INFO1_MAC_ADDR_47_32		GENMASK(15, 0)
#define HAL_AST_ENTRY_INFO1_MEC				BIT(17)
#define HAL_AST_ENTRY_INFO1_LINK_ID			GENMASK(21, 19)
#define HAL_AST_ENTRY_INFO1_ENTRY_VALID			BIT(22)
#define HAL_AST_ENTRY_INFO1_USE_ADDRX_SEARCH		BIT(27)
#define HAL_AST_ENTRY_INFO1_NUM_WHO_CLASSIFY_INFO	GENMASK(31, 30)
#define HAL_AST_ENTRY_INFO2_WHO_CLASSIFY_INFO_31_0	GENMASK(31, 0)
#define HAL_AST_ENTRY_INFO3_WHO_CLASSIFY_INFO_39_32	GENMASK(7, 0)
#define HAL_AST_ENTRY_INFO5_SW_PEER_ID			GENMASK(31, 16)
#define HAL_AST_ENTRY_INFO6_VDEV_ID			GENMASK(7, 0)

struct hal_ast_entry {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
} __packed;

struct ath12k_hal_ast_param {
	u32 skid_len:8,
	    ast_cache_en:1,
	    ast_cache_failure_en:1,
	    reserved:22;
	u32 ase_hash_key1;
	u32 ase_hash_key2;
	u32 ase_hash_key3;
	dma_addr_t paddr;
	u16 num_ast_entries;
};

extern const struct hal_ops hal_qcn9625_ops;

void ath12k_wifi8_hal_ce_dst_setup(struct ath12k_base *ab,
				   struct hal_srng *srng, int ring_num);
void ath12k_wifi8_hal_srng_dst_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng, u32 restore_idx);
void ath12k_wifi8_hal_srng_src_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng, u32 restore_idx);
void ath12k_wifi8_hal_set_umac_srng_ptr_addr(struct ath12k_base *ab,
					     struct hal_srng *srng,
					     enum hal_ring_type type, int ring_num);
int ath12k_wifi8_hal_srng_update_shadow_config(struct ath12k_base *ab,
					       enum hal_ring_type ring_type,
					       int ring_num);
int ath12k_wifi8_hal_srng_get_ring_id(struct ath12k_hal *hal,
				      enum hal_ring_type type,
				      int ring_num, int mac_id);
u32 ath12k_wifi8_hal_ce_get_desc_size(enum hal_ce_desc type);
void ath12k_wifi8_hal_cc_config(struct ath12k_base *ab);
u8
ath12k_wifi8_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id);
void ath12k_wifi8_get_tlv_tag_params(__le32 tl, uint16_t *tag, uint32_t *id,
				     uint16_t *length);
void ath12k_wifi8_hal_get_hw_hptp(struct ath12k_base *ab, enum hal_ring_type type,
				  struct hal_srng *srng, uint32_t *hp, uint32_t *tp);
void ath12k_wifi8_hal_ce_src_set_desc(struct hal_ce_srng_src_desc *desc,
				      dma_addr_t paddr,
				      u32 len, u32 id, u8 byte_swap_data);
void ath12k_wifi8_hal_ce_dst_set_desc(struct hal_ce_srng_dest_desc *desc,
				      dma_addr_t paddr);
void
ath12k_wifi8_hal_set_link_desc_addr(struct hal_wbm_link_desc *desc,
				    u32 cookie, dma_addr_t paddr,
				    u8 rbm);
u32
ath12k_wifi8_hal_ce_dst_status_get_length(struct hal_ce_srng_dst_status_desc *desc);
void
ath12k_wifi8_hal_setup_link_idle_list(struct ath12k_base *ab,
				      struct hal_wbm_idle_scatter_list *sbuf,
				      u32 nsbufs, u32 tot_link_desc,
				      u32 end_offset);
void ath12k_wifi8_hal_reoq_lut_addr_read_enable(struct ath12k_base *ab);
void ath12k_wifi8_hal_reoq_lut_set_max_peerid(struct ath12k_base *ab);
void ath12k_wifi8_hal_write_reoq_lut_addr(struct ath12k_base *ab,
					  dma_addr_t paddr);
void ath12k_wifi8_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab,
					     dma_addr_t paddr);
void ath12k_wifi8_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_wifi8_hal_reo_hw_setup(struct ath12k_base *ab);
void
ath12k_wifi8_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_wifi8_rx_buf_return_buf_manager *rbm);
u32 ath12k_wifi8_hal_reo_qdesc_size(u32 ba_window_size, u8 tid);
void ath12k_wifi8_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type);

void ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
						     u32 ppeds_idx_map_val);
void ath12k_wifi8_hal_srng_hw_disable(struct ath12k_base *ab,
				      struct hal_srng *srng);
void ath12k_wifi8_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid);
void ath12k_wifi8_hal_tx_configure_bank_register_default(struct ath12k_base *ab);
bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_get(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_set(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_reset(struct ath12k_base *ab);
bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_done(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
					    u8 rbm_id, int ring_type);
void ath12k_wifi8_hal_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					  struct ath12k_dp_ppe_vp_profile *ppe_vp_profile,
					  u32 ppe_vp_idx, u32 vdev_id,
					  u32 bank_id, u32 lmac_id);
void ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
						     u32 ppeds_idx_map_val);
void ath12k_wifi8_hal_reo_config_reo2ppe_dest_info(struct ath12k_base *ab);
bool ath12k_wifi8_hal_tx_completion_process(struct hal_tqm2sw_completion_ring *desc,
					    struct ath12k_dp_tx_comp_status *tx_comp_status);
void ath12k_wifi8_hal_hw_ase_init(struct ath12k_base *ab,
				  struct ath12k_hal_ast_param *ast_param);

static inline
void *ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(struct hal_srng *srng)
{
	void *desc;
	u32 next_hp;

	/* TODO: Using % is expensive, but we have to do this since size of some
	 * SRNG rings is not power of 2 (due to descriptor sizes). Need to see
	 * if separate function is defined for rings having power of 2 ring size
	 * (TCL2SW, REO2SW, SW2RXDMA and CE rings) so that we can avoid the
	 * overhead of % by using mask (with &).
	 */
	next_hp = (srng->u.src_ring.hp + srng->entry_size) % srng->ring_size;

	if (next_hp == srng->u.src_ring.cached_tp) {
		srng->u.src_ring.cached_tp = *(volatile u32 *)srng->u.src_ring.tp_addr;
		if (next_hp == srng->u.src_ring.cached_tp)
			return NULL;
	}

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = next_hp;

	/* TODO: Reap functionality is not used by all rings. If particular
	 * ring does not use reap functionality, we need not update reap_hp
	 * with next_hp pointer. Need to make sure a separate function is used
	 * before doing any optimization by removing below code updating
	 * reap_hp.
	 */
	srng->u.src_ring.reap_hp = next_hp;

	return desc;
}

#endif
