// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "../debug.h"
#include "../core.h"
#include "../hw.h"
#include "hw.h"
#include "../wow.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../testmode.h"
#include "../dp_peer.h"
#include "../dp_tx.h"

static u8 ath12k_wifi8_hw_qcn9625_mac_from_pdev_id(int pdev_idx)
{
	return pdev_idx;
}

static int
ath12k_wifi8_hw_mac_id_to_pdev_id_qcn9625(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static int
ath12k_wifi8_hw_mac_id_to_srng_id_qcn9625(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static u8 ath12k_wifi8_hw_get_ring_selector_qcn9625(struct sk_buff *skb)
{
	return smp_processor_id();
}

static bool ath12k_wifi8_dp_srng_is_comp_ring_qcn9625(int ring_num)
{
	return false;
}

static const struct ath12k_hw_ops qcn9625_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi8_hw_qcn9625_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi8_hw_mac_id_to_pdev_id_qcn9625,
	.mac_id_to_srng_id = ath12k_wifi8_hw_mac_id_to_srng_id_qcn9625,
	.get_ring_selector = ath12k_wifi8_hw_get_ring_selector_qcn9625,
	.dp_srng_is_tx_comp_ring = ath12k_wifi8_dp_srng_is_comp_ring_qcn9625,
};

/* To support 8 MSI DP grouping */
static struct ath12k_hw_ring_mask ath12k_wifi8_hw_ring_mask_qcn9625_msi8 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2 | ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0, 0
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_dest = {
		0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0, 0
	},
	.rx = {
		0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2 | ATH12K_RX_RING_MASK_3,
		0, 0
	},
	.rx_err = {
		0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0
	},
	.rx_wbm_rel = {
		0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0
	},
	.reo_status = {
		0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0
	},
	.host2rxdma = {
		0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
		0, 0, 0, 0
	},
	.tx_mon_dest = {
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
		0, 0, 0, 0, 0, 0
	},
	.umac_dp_reset = {
		0, 0, 0, 0, 0, 0, 0,
		ATH12K_UMAC_RESET_INTR_MASK_0
	},
};

#define ATH12K_PPE2TCL_RING_MASK_0 0x1
#define ATH12K_REO2PPE_RING_MASK_0 0x1
#define ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0 0x1

static struct ath12k_hw_ring_mask ath12k_wifi8_hw_ring_mask_qcn9625 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
	.host2rxmon = {
		0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	.rx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0,
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
		0, 0, 0, 0, 0,
	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	.tx_mon_dest = {
		0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.wbm2sw6_ppeds_tx_cmpln = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
	.umac_dp_reset = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_UMAC_RESET_INTR_MASK_0
	},
};

static struct ath12k_hw_params ath12k_wifi8_hw_params[] = {
	{
		.name = "qcn9625 hw1.0",
		.hw_rev = ATH12K_HW_QCN9625_HW10,
		.fw = {
			.dir = "QCN9625/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
#ifndef CONFIG_ATH12K_MEM_PROFILE_512M
		.supports_monitor = true,
		.max_clients_supported = 512,
#endif

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0x600000,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = QCN9274_QFPROM_RAW_RFA_PDET_ROW13_LSB,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = true,

		.iova_mask = 0,

		.supports_aspm = false,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 512,
			.fragment_160mhz = true,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
		.ds_support = true,
		.mlo_3_link_tx_support = true,
	},
};

int ath12k_wifi8_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
	struct ath12k_hw_params *hw_params_msi8 = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi8_hw_params); i++) {
		hw_params = &ath12k_wifi8_hw_params[i];

		if (hw_params->hw_rev == ab->hw_rev)
			break;
	}

	if (i == ARRAY_SIZE(ath12k_wifi8_hw_params)) {
		ath12k_err(ab, "Unsupported WiFi8 hardware version: 0x%x\n",
			   ab->hw_rev);
		return -EINVAL;
	}

	if (ab->hif.bus == ATH12K_BUS_PCI &&
	    ab->msi.config->total_vectors == ATH12K_MSI_16) {
		hw_params_msi8 = kzalloc(sizeof(*hw_params_msi8), GFP_KERNEL);
		if (!hw_params_msi8)
			return -ENOMEM;
		memcpy(hw_params_msi8, hw_params, sizeof(struct ath12k_hw_params));
		hw_params_msi8->ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625_msi8;
		ab->hw_params = hw_params_msi8;
	} else {
		ab->hw_params = hw_params;
	}

	ath12k_info(ab, "WiFi8 Hardware name: %s\n", ab->hw_params->name);

	return 0;
}
