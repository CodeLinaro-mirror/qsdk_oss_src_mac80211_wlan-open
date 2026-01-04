// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "hal_rx_desc.h"
#include "hal_qcn9625.h"
#include "hw.h"
#include "hal.h"
#include <linux/cacheflush.h>

static const struct hal_srng_config hw_srng_config_template[] = {
	/* TODO: max_rings can populated by querying HW capabilities */
	/* REO2SW Rings */
	[HAL_REO_EXCEPTION] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception",
	},
	[HAL_REO_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW1,
		.max_rings = 6,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst",
	},
	[HAL_REO_EXCEPTION_DS] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW7,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception_ds",
	},
	[HAL_REO_EXCEPTION_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW8,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception_mgmt",
	},
	[HAL_REO_DST_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW9,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_mgmt",
	},
	[HAL_REO_DST_CTDMA] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW10,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_ctdma",
	},
	[HAL_REO_DST_HIGH_PRIO] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW11,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_high_prio",
	},
	[HAL_REO_REINJECT] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2REO,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_reinject",
	},
	[HAL_REO_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_CMD,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_reo_get_queue_stats)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_cmd",
	},
	[HAL_REO_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_reo_get_queue_stats_status)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_status",
	},
	[HAL_REO2PPE] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2PPE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO2PPE_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_TCL_DATA] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2TCL1,
		.max_rings = 6,
		.entry_size = sizeof(struct hal_tcl_data_cmd) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE,
		.name = "Tcl_data",
	},
	[HAL_TCL_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_TCL_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			     sizeof(struct hal_tcl_status_ring)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "Tcl_status",
	},
	/* TCL2SW Exception Ring */
	[HAL_TX_EXCEPTION] = {
		.start_ring_id = HAL_SRNG_RING_ID_TX_EXCEPTION,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_tcl_regular_exit_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TX_EXCEPTION_RING_BASE_MSB_RING_SIZE,
		.name = "Tx_exception",
	},
	[HAL_CE_SRC] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_SRC,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_src_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_SRC_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_DST_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST_STATUS,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_dst_status_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_WBM_IDLE_LINK] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_IDLE_LINK,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_link_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE,
		.name = "WBM_hw_idle_link",
	},
	[HAL_SW2WBM_RELEASE] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_SW0_RELEASE,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "sw2wbm_release",
	},
	/* TX completions */
	[HAL_TX_COMPLETION] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM2SW0_RELEASE,
		.max_rings = 9,
		.entry_size = sizeof(struct hal_tqm2sw_completion_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TQM2SW_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "tx_completion",
	},
	[HAL_RXDMA_BUF] = {
		.start_ring_id = HAL_SRNG_SW2RXDMA_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_DMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_buf",
	},
	[HAL_RXDMA_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
		.max_rings = 0,
		.entry_size = 0,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_dst",
	},
	[HAL_RXDMA_MONITOR_BUF] = {
		.start_ring_id = HAL_SRNG_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_buf_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_monitor_buf",
	},
	[HAL_RXDMA_MONITOR_STATUS] = { 0, },
	[HAL_RXDMA_MONITOR_DESC] = { 0, },
	[HAL_RXDMA_DIR_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
		.max_rings = 2,
		.entry_size = 8 >> 2, /* TODO: Define the struct */
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
	},
	[HAL_PPE2TCL] = {
		.start_ring_id = HAL_SRNG_RING_ID_PPE2TCL1,
		.max_rings = 3,
		.entry_size = sizeof(struct hal_tcl_entrance_from_ppe_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_PPE2TCL_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_PPE_RELEASE] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_PPE_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_TX_MONITOR_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2TXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_buf_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
	},
	[HAL_RXDMA_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_monitor_dst",
	},
	[HAL_TX_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_TXMON2SW0_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
	}
};

const struct ath12k_hw_regs qcn9625_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_id = HWIO_TCL_R0_SW2TCL1_RING_ID_OFFS,
	.hal_tcl1_ring_misc = HWIO_TCL_R0_SW2TCL1_RING_MISC_OFFS,
	.hal_tcl1_ring_tp_addr_lsb = HWIO_TCL_R0_SW2TCL1_RING_TP_ADDR_LSB_OFFS,
	.hal_tcl1_ring_tp_addr_msb = HWIO_TCL_R0_SW2TCL1_RING_TP_ADDR_MSB_OFFS,
	.hal_tcl1_ring_consumer_int_setup_ix0 =
				HWIO_TCL_R0_SW2TCL1_RING_CONSUMER_INT_SETUP_IX0_OFFS,
	.hal_tcl1_ring_consumer_int_setup_ix1 =
				HWIO_TCL_R0_SW2TCL1_RING_CONSUMER_INT_SETUP_IX1_OFFS,
	.hal_tcl1_ring_msi1_base_lsb = HWIO_TCL_R0_SW2TCL1_RING_MSI1_BASE_LSB_OFFS,
	.hal_tcl1_ring_msi1_base_msb = HWIO_TCL_R0_SW2TCL1_RING_MSI1_BASE_MSB_OFFS,
	.hal_tcl1_ring_msi1_data = HWIO_TCL_R0_SW2TCL1_RING_MSI1_DATA_OFFS,
	.hal_tcl1_ring_base_lsb = HWIO_TCL_R0_SW2TCL1_RING_BASE_LSB_OFFS,
	.hal_tcl1_ring_base_msb = HWIO_TCL_R0_SW2TCL1_RING_BASE_MSB_OFFS,
	.hal_tcl2_ring_base_lsb = HWIO_TCL_R0_SW2TCL2_RING_BASE_LSB_OFFS,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = HWIO_TCL_R0_TCL2SW_STATUS1_RING_BASE_LSB_OFFS,

	/* WBM idle link ring address */
	.hal_wbm_idle_ring_base_lsb = HWIO_WBM_R0_WBM_IDLE_LINK_RING_BASE_LSB_OFFS,
	.hal_wbm_idle_ring_misc_addr = HWIO_WBM_R0_WBM_IDLE_LINK_RING_MISC_OFFS,
	.hal_wbm_r0_idle_list_cntl_addr = HWIO_WBM_R0_IDLE_LIST_CONTROL_OFFS,
	.hal_wbm_r0_idle_list_size_addr = HWIO_WBM_R0_IDLE_LIST_SIZE_OFFS,
	.hal_wbm_scattered_ring_base_lsb =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_LSB_OFFS,
	.hal_wbm_scattered_ring_base_msb =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_LIST_BASE_MSB_OFFS,
	.hal_wbm_scattered_desc_head_info_ix0 =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX0_OFFS,
	.hal_wbm_scattered_desc_head_info_ix1 =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HEAD_INFO_IX1_OFFS,
	.hal_wbm_scattered_desc_tail_info_ix0 =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX0_OFFS,
	.hal_wbm_scattered_desc_tail_info_ix1 =
				HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_TAIL_INFO_IX1_OFFS,
	.hal_wbm_scattered_desc_ptr_hp_addr = HWIO_WBM_R0_SCATTERED_LINK_DESC_PTR_HP_OFFS,

	/* SW2WBM release ring address */
	.hal_wbm_sw_release_ring_base_lsb = HWIO_WBM_R0_SW_RELEASE_RING_BASE_LSB_OFFS,
	.hal_wbm_sw1_release_ring_base_lsb = HWIO_WBM_R0_FW_RELEASE_RING_BASE_MSB_OFFS,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01E0C094,
	.pcie_pcs_osc_dtct_config_base = 0x01E0CE58,

	/*PCIe qrtr node id reg*/
	.pcie_pcie_local_qrtr_ins_reg = 0x1E03300,

	/*PCIe hot reset reg*/
	.pcie_gcc_gcc_pcie_hot_rst = 0x1E38438,

	/* REO DEST ring address */
	.hal_reo2_ring_base = HWIO_REO_R0_REO2SW2_RING_BASE_LSB_OFFS,
	.hal_reo1_misc_ctrl_addr = HWIO_REO_R0_MISC_CTL_OFFS,
	.hal_reo1_sw_cookie_cfg0 = HWIO_REO_R0_SW_COOKIE_CFG0_OFFS,
	.hal_reo1_sw_cookie_cfg1 = HWIO_REO_R0_SW_COOKIE_CFG1_OFFS,
	.hal_reo1_qdesc_lut_base0 = HWIO_REO_R0_QDESC_LUT_BASE0_ADDR_OFFS,
	.hal_reo1_qdesc_lut_base1 = HWIO_REO_R0_QDESC_LUT_BASE1_ADDR_OFFS,
	.hal_reo1_qdesc_addr = HWIO_REO_R0_QDESC_ADDR_READ_OFFS,
	.hal_reo1_qdesc_max_peerid = HWIO_REO_R0_QDESC_MAX_SW_PEER_ID_OFFS,
	.hal_reo1_ring_base_lsb = HWIO_REO_R0_REO2SW1_RING_BASE_LSB_OFFS,
	.hal_reo1_ring_base_msb = HWIO_REO_R0_REO2SW1_RING_BASE_MSB_OFFS,
	.hal_reo1_ring_id = HWIO_REO_R0_REO2SW1_RING_ID_OFFS,
	.hal_reo1_ring_misc = HWIO_REO_R0_REO2SW1_RING_MISC_OFFS,
	.hal_reo1_ring_hp_addr_lsb = HWIO_REO_R0_REO2SW1_RING_HP_ADDR_LSB_OFFS,
	.hal_reo1_ring_hp_addr_msb = HWIO_REO_R0_REO2SW1_RING_HP_ADDR_MSB_OFFS,
	.hal_reo1_ring_producer_int_setup =
					HWIO_REO_R0_REO2SW1_RING_PRODUCER_INT_SETUP_OFFS,
	.hal_reo1_ring_msi1_base_lsb = HWIO_REO_R0_REO2SW1_RING_MSI1_BASE_LSB_OFFS,
	.hal_reo1_ring_msi1_base_msb = HWIO_REO_R0_REO2SW1_RING_MSI1_BASE_MSB_OFFS,
	.hal_reo1_ring_msi1_data = HWIO_REO_R0_REO2SW1_RING_MSI1_DATA_OFFS,
	.hal_reo1_aging_thres_ix0 = HWIO_REO_R0_AGING_THRESHOLD_IX_0_OFFS,
	.hal_reo1_aging_thres_ix1 = HWIO_REO_R0_AGING_THRESHOLD_IX_1_OFFS,
	.hal_reo1_aging_thres_ix2 = HWIO_REO_R0_AGING_THRESHOLD_IX_2_OFFS,
	.hal_reo1_aging_thres_ix3 = HWIO_REO_R0_AGING_THRESHOLD_IX_3_OFFS,

	/* REO Exception ring address */
	.hal_reo2_sw0_ring_base = HWIO_REO_R0_REO2SW0_RING_BASE_LSB_OFFS,

	/* REO Reinject ring address */
	.hal_sw2reo_ring_base = HWIO_REO_R0_SW2REO_RING_BASE_LSB_OFFS,
	.hal_sw2reo1_ring_base = HWIO_REO_R0_SW2REO1_RING_BASE_LSB_OFFS,

	/* REO cmd ring address */
	.hal_reo_cmd_ring_base = HWIO_REO_R0_REO_CMD_RING_BASE_LSB_OFFS,

	/* REO status ring address */
	.hal_reo_status_ring_base = HWIO_REO_R0_REO_STATUS_RING_BASE_LSB_OFFS,

	/* CE base address */
	.hal_umac_ce0_src_reg_base =
			SEQ_WCSS_UMAC_WFSS_CE_0_REG_WFSS_CE_0_CHANNEL_SRC_REG_OFFSET,
	.hal_umac_ce0_dest_reg_base =
			SEQ_WCSS_UMAC_WFSS_CE_0_REG_WFSS_CE_0_CHANNEL_DST_REG_OFFSET,
	.hal_umac_ce1_src_reg_base =
			SEQ_WCSS_UMAC_WFSS_CE_0_REG_WFSS_CE_1_CHANNEL_SRC_REG_OFFSET,
	.hal_umac_ce1_dest_reg_base =
			SEQ_WCSS_UMAC_WFSS_CE_0_REG_WFSS_CE_1_CHANNEL_DST_REG_OFFSET,
	.hal_ppe_rel_ring_base = HWIO_TQM_R0_TQM2PPE_RELEASE_RING_BASE_LSB_OFFS,
	.hal_reo2ppe_ring_base = HWIO_REO_R0_REO2PPE_RING_BASE_LSB_OFFS,
	.hal_tcl_ppe2tcl_ring_base_lsb = HWIO_TCL_R0_PPE2TCL1_RING_BASE_LSB_OFFS,
	/* PMM register base address */
	.hal_pmm_reg_base = 0x010D40FC,
};

const struct ath12k_hw_hal_params ath12k_wifi8_hw_hal_params_qcn9625 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW0_BM,
	.tqm2sw_cc_enable1 = HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW0_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW1_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW2_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW3_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW4_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW5_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW6_EN,
	.tqm2sw_cc_enable2 = HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW7_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW8_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN,
	.link_desc_size = HAL_LINK_DESC_SIZE,
	.num_mpdus_per_link_desc = HAL_NUM_MPDUS_PER_LINK_DESC,
	.num_tx_msdus_per_link_desc = HAL_NUM_TX_MSDUS_PER_LINK_DESC,
	.num_rx_msdus_per_link_desc = HAL_NUM_RX_MSDUS_PER_LINK_DESC,
	.num_mpdu_links_per_queue_desc = HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC,
	.dscp_tid_map_tbl_max_entries = HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX,
};

u32 ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625(struct hal_rx_desc *desc)
{
	u32 info = __le32_to_cpu(desc->u.qcn9625_compact.msdu_end.info14);
	u32 errmap = 0;

	if (info & RX_MSDU_END_INFO14_FCS_ERR)
		errmap |= HAL_RX_MPDU_ERR_FCS;

	if (info & RX_MSDU_END_INFO14_DECRYPT_ERR)
		errmap |= HAL_RX_MPDU_ERR_DECRYPT;

	if (info & RX_MSDU_END_INFO14_TKIP_MIC_ERR)
		errmap |= HAL_RX_MPDU_ERR_TKIP_MIC;

	if (info & RX_MSDU_END_INFO14_A_MSDU_ERROR)
		errmap |= HAL_RX_MPDU_ERR_AMSDU_ERR;

	if (info & RX_MSDU_END_INFO14_OVERFLOW_ERR)
		errmap |= HAL_RX_MPDU_ERR_OVERFLOW;

	if (info & RX_MSDU_END_INFO14_MSDU_LENGTH_ERR)
		errmap |= HAL_RX_MPDU_ERR_MSDU_LEN;

	if (info & RX_MSDU_END_INFO14_MPDU_LENGTH_ERR)
		errmap |= HAL_RX_MPDU_ERR_MPDU_LEN;

	return errmap;
}

void
ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625(struct hal_rx_desc *desc,
						   u8 *crypto_hdr,
						   enum hal_encrypt_type encype)
{
	unsigned int key_id;

	switch (encype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[1] = 0;
		crypto_hdr[2] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		break;
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[1] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[2] = 0;
		break;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		return;
	}
	key_id = le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info4,
			       RX_MPDU_INFO_INFO4_KEY_ID_OCTET);
	crypto_hdr[3] = 0x20 | (key_id << 6);
	crypto_hdr[4] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE3(desc->u.qcn9625_compact.mpdu_start.pn[0]);
	crypto_hdr[5] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE4(desc->u.qcn9625_compact.mpdu_start.pn[0]);
	crypto_hdr[6] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[1]);
	crypto_hdr[7] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[1]);
}

void
ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625(struct hal_rx_desc *desc,
					       struct ieee80211_hdr *hdr)
{
	hdr->frame_control = desc->u.qcn9625_compact.mpdu_start.frame_ctrl;
	hdr->duration_id = desc->u.qcn9625_compact.mpdu_start.duration;
	ether_addr_copy(hdr->addr1, desc->u.qcn9625_compact.mpdu_start.addr1);
	ether_addr_copy(hdr->addr2, desc->u.qcn9625_compact.mpdu_start.addr2);
	ether_addr_copy(hdr->addr3, desc->u.qcn9625_compact.mpdu_start.addr3);
	if (__le32_to_cpu(desc->u.qcn9625_compact.mpdu_start.info2) &
	    RX_MPDU_INFO_INFO2_MAC_ADDR_AD4_VALID) {
		ether_addr_copy(hdr->addr4, desc->u.qcn9625_compact.mpdu_start.addr4);
	}
	hdr->seq_ctrl = desc->u.qcn9625_compact.mpdu_start.seq_ctrl;
}

void ath12k_wifi8_hal_extract_rx_desc_data_qcn9625(struct hal_rx_desc_data *rx_desc_data,
						   struct hal_rx_desc *rx_desc,
						   struct hal_rx_desc *ldesc)
{
	rx_desc_data->msdu_done = ath12k_wifi8_hal_rx_h_msdu_done_qcn9625(ldesc);
	rx_desc_data->msdu_len = ath12k_wifi8_hal_rx_h_msdu_len_qcn9625(ldesc);
	rx_desc_data->l3_pad_bytes = ath12k_wifi8_hal_rx_h_l3pad_qcn9625(ldesc);
	rx_desc_data->is_first_msdu =
		ath12k_wifi8_hal_rx_h_first_msdu_qcn9625(ldesc);
	rx_desc_data->is_last_msdu =
		ath12k_wifi8_hal_rx_h_last_msdu_qcn9625(ldesc);
	rx_desc_data->freq = ath12k_wifi8_hal_rx_h_freq_qcn9625(rx_desc);
	rx_desc_data->pkt_type = ath12k_wifi8_hal_rx_h_pkt_type_qcn9625(rx_desc);
	rx_desc_data->bw = ath12k_wifi8_hal_rx_h_rx_bw_qcn9625(rx_desc);
	rx_desc_data->rate_mcs = ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625(rx_desc);
	rx_desc_data->nss = hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625(rx_desc));
	rx_desc_data->sgi = ath12k_wifi8_hal_rx_h_sgi_qcn9625(rx_desc);
	rx_desc_data->is_mcbc =
		ath12k_wifi8_hal_rx_h_is_da_mcbc_qcn9625(rx_desc);
	rx_desc_data->seq_no =
		ath12k_wifi8_hal_rx_h_seq_no_qcn9625(rx_desc);
	rx_desc_data->peer_id = ath12k_wifi8_hal_rx_h_peer_id_qcn9625(rx_desc);
	rx_desc_data->err_bitmap =
		ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625(rx_desc);
	rx_desc_data->is_decrypted =
		ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625(rx_desc);
	rx_desc_data->decap = ath12k_wifi8_hal_rx_h_decap_type_qcn9625(rx_desc);
	rx_desc_data->ip_csum_fail =
		ath12k_wifi8_hal_rx_h_ip_cksum_fail_qcn9625(rx_desc);
	rx_desc_data->l4_csum_fail =
		ath12k_wifi8_hal_rx_h_l4_cksum_fail_qcn9625(rx_desc);
	rx_desc_data->tid = ath12k_wifi8_hal_rx_h_tid_qcn9625(rx_desc);
	rx_desc_data->mesh_ctrl_present =
		ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625(rx_desc);
	rx_desc_data->seq_ctl_valid =
			ath12k_wifi8_hal_rx_h_seq_ctrl_valid_qcn9625(rx_desc);
	rx_desc_data->fc_valid = ath12k_wifi8_hal_rx_h_fc_valid_qcn9625(rx_desc);
	rx_desc_data->is_ip_valid = ath12k_wifi8_hal_rx_h_is_ip_valid_qcn9625(rx_desc);
	rx_desc_data->is_from_ds = ath12k_wifi8_hal_rx_h_from_ds_qcn9625(rx_desc);
	rx_desc_data->is_to_ds = ath12k_wifi8_hal_rx_h_to_ds_qcn9625(rx_desc);
}

void ath12k_wifi8_hal_extract_rx_spd_data_qcn9625(struct hal_rx_spd_data *rx_info,
						  struct hal_rx_desc *rx_desc, int set)
{
	if (set == 0) {
		rx_info->tlv_info.decap =
			ath12k_wifi8_hal_rx_h_decap_type_qcn9625(rx_desc);
		rx_info->tlv_info.mesh_ctrl_present =
			ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625(rx_desc);
	} else if (set == 1) {
		rx_info->tlv_info.freq = ath12k_wifi8_hal_rx_h_freq_qcn9625(rx_desc);
		rx_info->tlv_info.pkt_type =
			ath12k_wifi8_hal_rx_h_pkt_type_qcn9625(rx_desc);
		rx_info->tlv_info.bw = ath12k_wifi8_hal_rx_h_rx_bw_qcn9625(rx_desc);
		rx_info->tlv_info.rate_mcs =
			ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625(rx_desc);
		rx_info->tlv_info.nss =
			hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625(rx_desc));
		rx_info->tlv_info.sgi = ath12k_wifi8_hal_rx_h_sgi_qcn9625(rx_desc);
	}
}

static int ath12k_wifi8_hal_srng_create_config_qcn9625(struct ath12k_hal *hal)
{
	struct hal_srng_config *s;

	hal->srng_config = kmemdup(hw_srng_config_template,
				   sizeof(hw_srng_config_template),
				   GFP_KERNEL);
	if (!hal->srng_config)
		return -ENOMEM;

	s = &hal->srng_config[HAL_REO_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_HP;
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_REO2_RING_HP - HAL_REO1_RING_HP;

	s = &hal->srng_config[HAL_REO_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_HP;

	s = &hal->srng_config[HAL_REO_REINJECT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_HP;
	s->reg_size[0] = HAL_SW2REO1_RING_BASE_LSB(hal) - HAL_SW2REO_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_SW2REO1_RING_HP - HAL_SW2REO_RING_HP;

	s = &hal->srng_config[HAL_REO_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_HP;

	s = &hal->srng_config[HAL_REO_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_HP;

	s = &hal->srng_config[HAL_REO2PPE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2PPE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2PPE_HP;
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_REO2_RING_HP - HAL_REO1_RING_HP;

	s = &hal->srng_config[HAL_TCL_DATA];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_HP;
	s->reg_size[0] = HAL_TCL2_RING_BASE_LSB(hal) - HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_TCL2_RING_HP - HAL_TCL1_RING_HP;

	s = &hal->srng_config[HAL_CE_SRC];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG - HAL_SEQ_WCSS_UMAC_CE0_SRC_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG - HAL_SEQ_WCSS_UMAC_CE0_SRC_REG;

	s = &hal->srng_config[HAL_CE_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;

	s = &hal->srng_config[HAL_CE_DST_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG +
		HAL_CE_DST_STATUS_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_STATUS_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;

	s = &hal->srng_config[HAL_WBM_IDLE_LINK];
	s->reg_start[0] =
		HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_HP;

	s = &hal->srng_config[HAL_SW2WBM_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
		HAL_WBM_SW_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_RELEASE_RING_HP;
	s->reg_size[0] = HAL_WBM_FW_RELEASE_RING_BASE_LSB -
			 HAL_WBM_SW_RELEASE_RING_BASE_LSB;
	s->reg_size[1] = HAL_WBM_FW_RELEASE_RING_HP - HAL_WBM_SW_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_TX_COMPLETION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW0_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW0_RELEASE_RING_HP;
	s->reg_size[0] = HAL_TQM2SW1_RELEASE_RING_BASE_LSB -
			 HAL_TQM2SW0_RELEASE_RING_BASE_LSB;
	s->reg_size[1] = HAL_TQM2SW1_RELEASE_RING_HP - HAL_TQM2SW0_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_TCL_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_HP;

	/* TCL2SW1_EXCEPTION */
	s = &hal->srng_config[HAL_TX_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TX_EXCEPTION_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TX_EXCEPTION_RING_HP;

	/* Some LMAC rings are not accessed from the host:
	 * RXDMA_BUG, RXDMA_DST, RXDMA_MONITOR_BUF, RXDMA_MONITOR_STATUS,
	 * RXDMA_MONITOR_DST, RXDMA_MONITOR_DESC, RXDMA_DIR_BUF_SRC,
	 * RXDMA_RX_MONITOR_BUF, TX_MONITOR_BUF, TX_MONITOR_DST, SW2RXDMA
	 */
	s = &hal->srng_config[HAL_PPE2TCL];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_PPE2TCL1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_PPE2TCL1_RING_HP;

	s = &hal->srng_config[HAL_PPE_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG +
				HAL_TQM_PPE_RELEASE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_PPE_RELEASE_RING_HP;

	return 0;
}

static const struct ath12k_hal_tcl_to_cmp_rbm_map
ath12k_wifi8_hal_tcl_to_cmp_rbm_map_qcn9625[DP_TCL_NUM_RING_MAX] = {
	{
		.cmp_ring_num = 0,
		.rbm_id = HAL_RX_BUF_RBM_SW0_BM,
	},
	{
		.cmp_ring_num = 1,
		.rbm_id = HAL_RX_BUF_RBM_SW1_BM,
	},
	{
		.cmp_ring_num = 2,
		.rbm_id = HAL_RX_BUF_RBM_SW2_BM,
	},
	{
		.cmp_ring_num = 3,
		.rbm_id = HAL_RX_BUF_RBM_SW3_BM,
	}
};

static int ath12k_wifi8_hal_init_qcn9625(struct ath12k_hal *hal, u8 hw_version)
{
	hal->regs = ath12k_wifi8_hw_ver_map[hw_version].hw_regs;
	hal->tcl_to_cmp_rbm_map = ath12k_wifi8_hal_tcl_to_cmp_rbm_map_qcn9625;
	hal->hal_ops = &hal_qcn9625_ops;
	hal->hal_desc_sz = ath12k_wifi8_hal_get_rx_desc_size_qcn9625();
	hal->hal_params = ath12k_wifi8_hw_ver_map[hw_version].hal_params;

	return 0;
}

const struct hal_ops hal_qcn9625_ops = {
	.hal_init = ath12k_wifi8_hal_init_qcn9625,
	.create_srng_config = ath12k_wifi8_hal_srng_create_config_qcn9625,
	.rx_desc_set_msdu_len = ath12k_wifi8_hal_rxdesc_set_msdu_len_qcn9625,
	.rx_desc_get_dot11_hdr = ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625,
	.rx_desc_get_mpdu_frame_ctl =
		ath12k_wifi8_hal_rxdesc_get_mpdu_frame_ctrl_qcn9625,
	.rx_desc_get_crypto_header =
			ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625,
	.rx_desc_copy_end_tlv = ath12k_wifi8_hal_rx_desc_end_tlv_copy_qcn9625,
	.rx_desc_get_msdu_src_link_id =
			ath12k_wifi8_hal_rx_get_msdu_src_link_qcn9625,
	.extract_rx_desc_data =
			ath12k_wifi8_hal_extract_rx_desc_data_qcn9625,
	.extract_rx_spd_data =
		ath12k_wifi8_hal_extract_rx_spd_data_qcn9625,
	.ce_dst_setup = ath12k_wifi8_hal_ce_dst_setup,
	.srng_src_hw_init = ath12k_wifi8_hal_srng_src_hw_init,
	.srng_dst_hw_init = ath12k_wifi8_hal_srng_dst_hw_init,
	.set_umac_srng_ptr_addr = ath12k_wifi8_hal_set_umac_srng_ptr_addr,
	.srng_update_shadow_config = ath12k_wifi8_hal_srng_update_shadow_config,
	.srng_get_ring_id = ath12k_wifi8_hal_srng_get_ring_id,
	.ce_get_desc_size = ath12k_wifi8_hal_ce_get_desc_size,
	.ce_src_set_desc = ath12k_wifi8_hal_ce_src_set_desc,
	.ce_dst_set_desc = ath12k_wifi8_hal_ce_dst_set_desc,
	.ce_dst_status_get_length = ath12k_wifi8_hal_ce_dst_status_get_length,
	.set_link_desc_addr = ath12k_wifi8_hal_set_link_desc_addr,
	.tx_set_dscp_tid_map = ath12k_wifi8_hal_tx_set_dscp_tid_map,
	.tx_update_dscp_tid_map = ath12k_wifi8_hal_tx_update_dscp_tid_map,
	.tx_configure_bank_register =
				ath12k_wifi8_hal_tx_configure_bank_register,
	.write_reoq_lut_addr = ath12k_wifi8_hal_write_reoq_lut_addr,
	.write_ml_reoq_lut_addr = ath12k_wifi8_hal_write_ml_reoq_lut_addr,
	.reoq_lut_addr_read_enable = ath12k_wifi8_hal_reoq_lut_addr_read_enable,
	.reoq_lut_set_max_peerid = ath12k_wifi8_hal_reoq_lut_set_max_peerid,
	.setup_link_idle_list = ath12k_wifi8_hal_setup_link_idle_list,
	.reo_qdesc_setup = ath12k_wifi8_hal_reo_qdesc_setup,
	.reo_init_cmd_ring = ath12k_wifi8_hal_reo_init_cmd_ring,
	.reo_hw_setup = ath12k_wifi8_hal_reo_hw_setup,
	.cc_config = ath12k_wifi8_hal_cc_config,
	.srng_hw_disable = ath12k_wifi8_hal_srng_hw_disable,
	.reset_rx_reo_tid_q = ath12k_wifi8_hal_reset_rx_reo_tid_q,
	.get_idle_link_rbm = ath12k_wifi8_hal_get_idle_link_rbm,
	.reo_shared_qaddr_cache_clear = ath12k_wifi8_hal_reo_shared_qaddr_cache_clear,
	.hal_get_tsf2_scratch_reg = ath12k_hal_qcn9625_get_tsf2_scratch_reg,
	.hal_get_tqm_scratch_reg = ath12k_hal_qcn9625_get_tqm_scratch_reg,
	.rxdesc_get_mpdu_start_addr2 =
			ath12k_wifi8_hal_rxdesc_get_mpdu_start_addr2_qcn9625,
	.rx_h_is_decrypted = ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625,
	.get_hw_hptp = ath12k_wifi8_hal_get_hw_hptp,
	.rx_desc_get_fse_info = ath12k_wifi8_hal_rx_desc_get_fse_info_qcn9625,
	.hal_tx_ppe2tcl_ring_halt_get = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_get,
	.hal_tx_ppe2tcl_ring_halt_set = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_set,
	.hal_tx_ppe2tcl_ring_halt_reset = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_reset,
	.hal_tx_ppe2tcl_ring_halt_done = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_done,
	.hal_tx_config_rbm_mapping = ath12k_wifi8_hal_tx_config_rbm_mapping,
	.hal_ppeds_cfg_ast_override_map_reg =
			ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg,
	.hal_reo_config_reo2ppe_dest_info = ath12k_wifi8_hal_reo_config_reo2ppe_dest_info,
	.hal_tx_set_ppe_vp_entry = ath12k_wifi8_hal_tx_set_ppe_vp_entry,
	.hal_get_tlv_tag_params = ath12k_wifi8_get_tlv_tag_params,
};
