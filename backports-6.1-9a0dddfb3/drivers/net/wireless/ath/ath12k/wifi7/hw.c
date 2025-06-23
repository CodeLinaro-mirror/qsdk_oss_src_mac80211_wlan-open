// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "../debug.h"
#include "../core.h"
#include "../ce.h"
#include "ce.h"
#include "../hw.h"
#include "hw.h"
#include "../mhi.h"
#include "mhi.h"
#include "dp_rx.h"
#include "wmi.h"
#include "../wow.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../testmode.h"
#include "../dp_peer.h"
#include "../dp_tx.h"
#include "dp_tx.h"
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../cfr.h"

static const guid_t wcn7850_uuid = GUID_INIT(0xf634f534, 0x6147, 0x11ec,
					     0x90, 0xd6, 0x02, 0x42,
					     0xac, 0x12, 0x00, 0x03);

static u8 ath12k_wifi7_hw_qcn9274_mac_from_pdev_id(int pdev_idx)
{
	return pdev_idx;
}

static int
ath12k_wifi7_hw_mac_id_to_pdev_id_qcn9274(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static int
ath12k_wifi7_hw_mac_id_to_srng_id_qcn9274(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static u8 ath12k_wifi7_hw_get_ring_selector_qcn9274(struct sk_buff *skb)
{
	return smp_processor_id();
}

static bool ath12k_wifi7_dp_srng_is_comp_ring_qcn9274(int ring_num)
{
	if (ring_num < 3 || ring_num == 4)
		return true;

	return false;
}

static int
ath12k_wifi7_hw_mac_id_to_pdev_id_wcn7850(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return 0;
}

static int
ath12k_wifi7_hw_mac_id_to_srng_id_wcn7850(const struct ath12k_hw_params *hw,
					  int mac_id)
{
	return mac_id;
}

static u8 ath12k_wifi7_hw_get_ring_selector_wcn7850(struct sk_buff *skb)
{
	return skb_get_queue_mapping(skb);
}

static bool ath12k_wifi7_dp_srng_is_comp_ring_wcn7850(int ring_num)
{
	if (ring_num == 0 || ring_num == 2 || ring_num == 4)
		return true;

	return false;
}

void ath12k_hw_qcn9274_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->pltform_type = PLATFORM_TYPE_ARM;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->cfr_data_version = ATH12K_CFR_DATA_VERSION_1;
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_9;
	if(ar->ab->hw_rev == ATH12K_HW_QCN6432_HW10)
		header->chip_type = ATH12K_CFR_RADIO_QCN6432;
	else if (ar->ab->hw_rev == ATH12K_HW_IPQ5424_HW10)
		header->chip_type = ATH12K_CFR_RADIO_IPQ5424;
	else if (ar->ab->hw_rev == ATH12K_HW_IPQ5332_HW10)
		header->chip_type = ATH12K_CFR_RADIO_IPQ5332;
	else
		header->chip_type = ATH12K_CFR_RADIO_QCN9274;

	header->u.meta_enh.status = FIELD_GET(WMI_CFR_PEER_CAPTURE_STATUS,
					      params->status);
	header->u.meta_enh.capture_bw = params->bandwidth;
	header->u.meta_enh.phy_mode = params->phy_mode;
	header->u.meta_enh.prim20_chan = params->primary_20mhz_chan;
	header->u.meta_enh.center_freq1 = params->band_center_freq1;
	header->u.meta_enh.center_freq2 = params->band_center_freq2;
	header->u.meta_enh.capture_mode = params->bandwidth ?
		ATH12K_CFR_CAPTURE_DUP_LEGACY_ACK : ATH12K_CFR_CAPTURE_LEGACY_ACK;
	header->u.meta_enh.capture_type = params->capture_method;
	header->u.meta_enh.num_rx_chain = ar->cfg_rx_chainmask;
	header->u.meta_enh.sts_count = params->spatial_streams;
	header->u.meta_enh.timestamp = params->timestamp_us;
	header->u.meta_enh.rx_start_ts = params->rx_start_ts;
	header->u.meta_enh.cfo_measurement = params->cfo_measurement;
	header->u.meta_enh.mcs_rate = params->mcs_rate;
	header->u.meta_enh.gi_type = params->gi_type;

	memcpy(header->u.meta_enh.peer_addr.su_peer_addr,
	       params->peer_mac_addr, ETH_ALEN);
	memcpy(header->u.meta_enh.chain_rssi, params->chain_rssi,
	       sizeof(params->chain_rssi));
	memcpy(header->u.meta_enh.chain_phase, params->chain_phase,
	       sizeof(params->chain_phase));
	memcpy(header->u.meta_enh.agc_gain, params->agc_gain,
	       sizeof(params->agc_gain));
	memcpy(header->u.meta_enh.agc_gain_tbl_index, params->agc_gain_tbl_index,
	       sizeof(params->agc_gain_tbl_index));
}

void ath12k_hw_wcn7850_fill_cfr_hdr_info(struct ath12k *ar,
					 struct ath12k_csi_cfr_header *header,
					 struct ath12k_cfr_peer_tx_param *params)
{
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->pltform_type = PLATFORM_TYPE_ARM;
	header->cfr_metadata_len = sizeof(struct cfr_enh_metadata);
	header->cfr_data_version = ATH12K_CFR_DATA_VERSION_1;
	header->host_real_ts = ktime_to_ns(ktime_get_real());

	header->cfr_metadata_version = ATH12K_CFR_META_VERSION_9;
	header->chip_type = ATH12K_CFR_RADIO_WCN7850;

	header->u.meta_enh.status = FIELD_GET(WMI_CFR_PEER_CAPTURE_STATUS,
					      params->status);
	header->u.meta_enh.capture_bw = params->bandwidth;
	header->u.meta_enh.phy_mode = params->phy_mode;
	header->u.meta_enh.prim20_chan = params->primary_20mhz_chan;
	header->u.meta_enh.center_freq1 = params->band_center_freq1;
	header->u.meta_enh.center_freq2 = params->band_center_freq2;
	header->u.meta_enh.capture_mode = params->bandwidth ?
		ATH12K_CFR_CAPTURE_DUP_LEGACY_ACK : ATH12K_CFR_CAPTURE_LEGACY_ACK;
	header->u.meta_enh.capture_type = params->capture_method;
	header->u.meta_enh.num_rx_chain = ar->cfg_rx_chainmask;
	header->u.meta_enh.sts_count = params->spatial_streams;
	header->u.meta_enh.timestamp = params->timestamp_us;
	header->u.meta_enh.rx_start_ts = params->rx_start_ts;
	header->u.meta_enh.cfo_measurement = params->cfo_measurement;
	header->u.meta_enh.mcs_rate = params->mcs_rate;
	header->u.meta_enh.gi_type = params->gi_type;

	memcpy(header->u.meta_enh.peer_addr.su_peer_addr,
	       params->peer_mac_addr, ETH_ALEN);
	memcpy(header->u.meta_enh.chain_rssi, params->chain_rssi,
	       sizeof(params->chain_rssi));
	memcpy(header->u.meta_enh.chain_phase, params->chain_phase,
	       sizeof(params->chain_phase));
	memcpy(header->u.meta_enh.agc_gain, params->agc_gain,
	       sizeof(params->agc_gain));
	memcpy(header->u.meta_enh.agc_gain_tbl_index, params->agc_gain_tbl_index,
	       sizeof(params->agc_gain_tbl_index));
}

static const struct ath12k_hw_ops qcn9274_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi7_hw_qcn9274_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi7_hw_mac_id_to_pdev_id_qcn9274,
	.mac_id_to_srng_id = ath12k_wifi7_hw_mac_id_to_srng_id_qcn9274,
	.rxdma_ring_sel_config = ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274,
	.get_ring_selector = ath12k_wifi7_hw_get_ring_selector_qcn9274,
	.dp_srng_is_tx_comp_ring = ath12k_wifi7_dp_srng_is_comp_ring_qcn9274,
	.fill_cfr_hdr_info = ath12k_hw_qcn9274_fill_cfr_hdr_info,
};

static const struct ath12k_hw_ops wcn7850_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi7_hw_qcn9274_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi7_hw_mac_id_to_pdev_id_wcn7850,
	.mac_id_to_srng_id = ath12k_wifi7_hw_mac_id_to_srng_id_wcn7850,
	.rxdma_ring_sel_config = ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850,
	.get_ring_selector = ath12k_wifi7_hw_get_ring_selector_wcn7850,
	.dp_srng_is_tx_comp_ring = ath12k_wifi7_dp_srng_is_comp_ring_wcn7850,
};

#define ATH12K_TX_RING_MASK_0 0x1
#define ATH12K_TX_RING_MASK_1 0x2
#define ATH12K_TX_RING_MASK_2 0x4
#define ATH12K_TX_RING_MASK_3 0x8
#define ATH12K_TX_RING_MASK_4 0x10

#define ATH12K_RX_RING_MASK_0 0x1
#define ATH12K_RX_RING_MASK_1 0x2
#define ATH12K_RX_RING_MASK_2 0x4
#define ATH12K_RX_RING_MASK_3 0x8

#define ATH12K_RX_ERR_RING_MASK_0 0x1

#define ATH12K_RX_WBM_REL_RING_MASK_0 0x1

#define ATH12K_REO_STATUS_RING_MASK_0 0x1

#define ATH12K_HOST2RXDMA_RING_MASK_0 0x1
#define ATH12K_HOST2RXDMA_RING_MASK_1 0x2
#define ATH12K_HOST2RXDMA_RING_MASK_2 0x4

#define ATH12K_RX_MON_RING_MASK_0 0x1
#define ATH12K_RX_MON_RING_MASK_1 0x2
#define ATH12K_RX_MON_RING_MASK_2 0x4

#define ATH12K_TX_MON_RING_MASK_0 0x1
#define ATH12K_TX_MON_RING_MASK_1 0x2
#define ATH12K_UMAC_RESET_INTR_MASK_0   0x1

/* To support 8 MSI DP grouping */
static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn9274_msi8 = {
        .tx  = {
                ATH12K_TX_RING_MASK_0,
                ATH12K_TX_RING_MASK_1,
                ATH12K_TX_RING_MASK_2 | ATH12K_TX_RING_MASK_3,
                0, 0, 0, 0, 0
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
/*This will be rebased only when ppeds patches are rebased
 */
#if 0
        .ppe2tcl = {
                0, 0, 0, 0, 0, 0,
                ATH12K_PPE2TCL_RING_MASK_0,
		0
        },
        .reo2ppe = {
                0, 0, 0, 0, 0,
                ATH12K_REO2PPE_RING_MASK_0,
		0, 0
        },
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
        .wbm2sw6_ppeds_tx_cmpln = {
		ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0
        },
#endif
#endif
        .umac_dp_reset = {
                0, 0, 0, 0, 0, 0, 0,
		ATH12K_UMAC_RESET_INTR_MASK_0
        },
};

#define ATH12K_PPE2TCL_RING_MASK_0 0x1
#define ATH12K_REO2PPE_RING_MASK_0 0x1
#define ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0 0x1

#define ATH12K_RX_MON_STATUS_RING_MASK_0 0x1
#define ATH12K_RX_MON_STATUS_RING_MASK_1 0x2
#define ATH12K_RX_MON_STATUS_RING_MASK_2 0x4

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn9274 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0,
		0, 0, 0, 0, 0,
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
	.host2rxdma = {
		0, 0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
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

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_ipq5332 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
	},
	.rx_mon_dest = {
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	.host2rxdma = {
		0, 0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
	},
	.tx_mon_dest = {
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.wbm2sw6_ppeds_tx_cmpln = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
};

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_wcn7850 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
	},
	.rx_mon_dest = {
	},
	.rx_mon_status = {
		0, 0, 0, 0,
		ATH12K_RX_MON_STATUS_RING_MASK_0,
		ATH12K_RX_MON_STATUS_RING_MASK_1,
		ATH12K_RX_MON_STATUS_RING_MASK_2,
	},
	.rx = {
		0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
	},
	.rx_err = {
		ATH12K_RX_ERR_RING_MASK_0,
	},
	.rx_wbm_rel = {
		ATH12K_RX_WBM_REL_RING_MASK_0,
	},
	.reo_status = {
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	.host2rxdma = {
	},
	.tx_mon_dest = {
	},
};

static struct ath12k_hw_ring_mask ath12k_wifi7_hw_ring_mask_qcn6432 = {
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0
	},
	.rx_mon_dest = {
		0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
		ATH12K_RX_MON_RING_MASK_2,
		0, 0, 0, 0
	},
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
 		ATH12K_RX_RING_MASK_3,
                0, 0, 0, 0,
                0, 0, 0

	},
	.rx_err = {
		0, 0, 0,
		ATH12K_RX_ERR_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.rx_wbm_rel = {
		0, 0, 0,
		ATH12K_RX_WBM_REL_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.reo_status = {
		0, 0, 0,
		ATH12K_REO_STATUS_RING_MASK_0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
	.host2rxdma = {
		0, 0,
		ATH12K_HOST2RXDMA_RING_MASK_0,
		ATH12K_HOST2RXDMA_RING_MASK_1,
		ATH12K_HOST2RXDMA_RING_MASK_2,
		0, 0, 0, 0, 0, 0,
	},
	.tx_mon_dest = {
		ATH12K_TX_MON_RING_MASK_0,
		ATH12K_TX_MON_RING_MASK_1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0
	},
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		ATH12K_PPE2TCL_RING_MASK_0, 0, 0
	},
	.reo2ppe = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, ATH12K_REO2PPE_RING_MASK_0, 0
	},
	.wbm2sw6_ppeds_tx_cmpln = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
};

static const struct ce_ie_addr ath12k_wifi7_ce_ie_addr_ipq5332 = {
	.ie1_reg_addr = CE_HOST_IPQ5332_IE_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
	.ie2_reg_addr = CE_HOST_IPQ5332_IE_2_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
	.ie3_reg_addr = CE_HOST_IPQ5332_IE_3_ADDRESS - HAL_IPQ5332_CE_WFSS_REG_BASE,
};

static const struct ce_remap ath12k_wifi7_ce_remap_ipq5332 = {
	.base = HAL_IPQ5332_CE_WFSS_REG_BASE,
	.size = HAL_IPQ5332_CE_SIZE,
	.cmem_offset = HAL_SEQ_WCSS_CMEM_OFFSET,
};

static const struct ce_ie_addr ath12k_wifi7_ce_ie_addr_ipq5424 = {
	.ie1_reg_addr = CE_HOST_IPQ5424_IE_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
	.ie2_reg_addr = CE_HOST_IPQ5424_IE_2_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
	.ie3_reg_addr = CE_HOST_IPQ5424_IE_3_ADDRESS - HAL_IPQ5424_CE_WFSS_REG_BASE,
};

static const struct ce_remap ath12k_wifi7_ce_remap_ipq5424 = {
	.base = HAL_IPQ5424_CE_WFSS_REG_BASE,
	.size = HAL_IPQ5424_CE_SIZE,
	.cmem_offset = HAL_SEQ_WCSS_CMEM_OFFSET,
};

static struct ath12k_hw_params ath12k_wifi7_hw_params[] = {
	{
		.name = "qcn9274 hw1.0",
		.hw_rev = ATH12K_HW_QCN9274_HW10,
		.fw = {
			.dir = "QCN9274/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9274,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274,

		.host_ce_config = ath12k_wifi7_host_ce_config_qcn9274,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_qcn9274,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_qcn9274,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = false,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT) |
					BIT(NL80211_IFTYPE_AP_VLAN),
		.supports_monitor = false,

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.tcl_ring_retry = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi7_mhi_config_qcn9274,

		.wmi_init = ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,

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
		.support_fse = false,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 0,
			.fft_pad_sz = 0,
			.summary_pad_sz = 0,
			.fft_hdr_len = 0,
			.max_fft_bins = 0,
			.fragment_160mhz = false,
		},
		.supports_ap_ps = true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = false,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
		.ds_support = false,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 127,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
				(CFR_HDR_MAX_LEN_WORDS_QCN9274 *4) +
				CFR_DATA_MAX_LEN_QCN9274,
	},
	{
		.name = "wcn7850 hw2.0",
		.hw_rev = ATH12K_HW_WCN7850_HW20,

		.fw = {
			.dir = "WCN7850/hw2.0",
			.board_size = 256 * 1024,
			.cal_offset = 256 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
		},

		.max_radios = 1,
		.single_pdev_only = true,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_WCN7850,
		.internal_sleep_clock = true,

		.hw_ops = &wcn7850_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_wcn7850,

		.host_ce_config = ath12k_wifi7_host_ce_config_wcn7850,
		.ce_count = 9,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_wcn7850,
		.target_ce_count = 9,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_wcn7850,
		.svc_to_ce_map_len = 14,

		.rxdma1_enable = false,
		.num_rxdma_per_pdev = 2,
		.num_rxdma_dst_ring = 1,
		.rx_mac_buf_ring = true,
		.vdev_start_delay = true,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
				   BIT(NL80211_IFTYPE_AP) |
				   BIT(NL80211_IFTYPE_P2P_DEVICE) |
				   BIT(NL80211_IFTYPE_P2P_CLIENT) |
				   BIT(NL80211_IFTYPE_P2P_GO),
		.supports_monitor = false,

		.idle_ps = true,
		.cold_boot_calib = true,
		.download_calib = false,
		.supports_suspend = true,
		.tcl_ring_retry = false,
		.reoq_lut_support = true,
		.supports_shadow_regs = true,

		.num_tcl_banks = 7,
		.max_tx_ring = 3,

		.mhi_config = &ath12k_wifi7_mhi_config_wcn7850,

		.wmi_init = ath12k_wifi7_wmi_init_wcn7850,

		.hal_ops = &hal_wcn7850_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_PCIE_PERST_NO_PULL_V01),

		.rfkill_pin = 48,
		.rfkill_cfg = 0,
		.rfkill_on_level = 1,

		.rddm_size = 0x780000,

		.def_num_link = 2,
		.max_mlo_peer = 32,

		.otp_board_id_register = 0,

		.supports_sta_ps = true,

		.acpi_guid = &wcn7850_uuid,
		.supports_dynamic_smps_6ghz = false,

		.iova_mask = ATH12K_PCIE_MAX_PAYLOAD_SIZE - 1,

		.supports_aspm = true,

		.current_cc_support = true,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = false,
		.en_qdsslog = true,
		.support_fse = false,
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = false,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
		.ds_support = false,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 255,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_WCN7850 *4) +
					CFR_DATA_MAX_LEN_WCN7850,
	},
	{
		.name = "qcn9274 hw2.0",
		.hw_rev = ATH12K_HW_QCN9274_HW20,
		.fw = {
			.dir = "QCN92XX/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9274,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274,

		.host_ce_config = ath12k_wifi7_host_ce_config_qcn9274,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_qcn9274,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_qcn9274,
		.svc_to_ce_map_len = 18,

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
		.tcl_ring_retry = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi7_mhi_config_qcn9274,

		.wmi_init = ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,

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
		.support_ce_manual_poll=true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = 0,
		.ds_support = true,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 127,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN9274 *4) +
					CFR_DATA_MAX_LEN_QCN9274,
	},
	{
		.name = "ipq5332 hw1.0",
		.hw_rev = ATH12K_HW_IPQ5332_HW10,
		.fw = {
			.dir = "IPQ5332/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ5332,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_ipq5332,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
				   BIT(NL80211_IFTYPE_AP) |
				   BIT(NL80211_IFTYPE_MESH_POINT),
#ifndef CONFIG_ATH12K_MEM_PROFILE_512M
		.supports_monitor = true,
		.max_clients_supported = 256,
#endif

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.tcl_ring_retry = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),
		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = 0,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,
		.iova_mask = 0,
		.supports_aspm = false,

		.ce_ie_addr = &ath12k_wifi7_ce_ie_addr_ipq5332,
		.ce_remap = &ath12k_wifi7_ce_remap_ipq5332,
		.en_qdsslog = true,
		.support_fse = true,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 512,
			.fragment_160mhz = false,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
		.credit_flow = false,
		.is_plink_preferable = false,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_IPQ5332,
		.ds_support = false,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 255,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_IPQ5332 *4) +
					CFR_DATA_MAX_LEN_IPQ5332,
	},
	{
		.name = "qcn6432 hw1.0",
		.hw_rev = ATH12K_HW_QCN6432_HW10,
		.fw = {
			.dir = "QCN6432/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN6432,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_qcn6432,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map =
			ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
		.svc_to_ce_map_len = 19,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
#ifndef CONFIG_ATH12K_MEM_PROFILE_512M
		.supports_monitor = true,
		.max_clients_supported = 256,
#endif

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.tcl_ring_retry = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,
		.hal_ops = &hal_qcn9274_ops,

		.supports_aspm = true,
		.send_platform_model = true,
		.en_qdsslog = true,
		.support_fse = true,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 512,
			.fragment_160mhz = false,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = true,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = true,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_QCN6432,
		.ds_support = true,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 128,
		/* sizeof (ath12k_csi_cfr_header) + max cfr header(200 bytes) +
		 * max cfr payload(16384 bytes)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_QCN6432 *4) +
					CFR_DATA_MAX_LEN_QCN6432,
	},
	{
		.name = "ipq5424 hw1.0",
		.hw_rev = ATH12K_HW_IPQ5424_HW10,
		.fw = {
			.dir = "IPQ5424/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_remoteproc,
		},
		.max_radios = 1,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ5332,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9274_ops,
		.ring_mask = &ath12k_wifi7_hw_ring_mask_ipq5332,

		.host_ce_config = ath12k_wifi7_host_ce_config_ipq5332,
		.ce_count = 12,
		.target_ce_config = ath12k_wifi7_target_ce_config_wlan_ipq5332,
		.target_ce_count = 12,
		.svc_to_ce_map = ath12k_wifi7_target_service_to_ce_map_wlan_ipq5332,
		.svc_to_ce_map_len = 18,

		.rxdma1_enable = true,
		.num_rxdma_per_pdev = 1,
		.num_rxdma_dst_ring = 0,
		.rx_mac_buf_ring = false,
		.vdev_start_delay = false,

		.interface_modes = BIT(NL80211_IFTYPE_STATION) |
					BIT(NL80211_IFTYPE_AP) |
					BIT(NL80211_IFTYPE_MESH_POINT),
#ifndef CONFIG_ATH12K_MEM_PROFILE_512M
		.supports_monitor = true,
		.max_clients_supported = 256,
#endif

		.idle_ps = false,
		.cold_boot_calib = true,
		.download_calib = true,
		.supports_suspend = false,
		.tcl_ring_retry = true,
		.reoq_lut_support = true,
		.supports_shadow_regs = false,

		.num_tcl_banks = 48,
		.max_tx_ring = 4,

		.wmi_init = &ath12k_wifi7_wmi_init_qcn9274,

		.hal_ops = &hal_qcn9274_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01),

		.rfkill_pin = 0,
		.rfkill_cfg = 0,
		.rfkill_on_level = 0,

		.rddm_size = 0,

		.def_num_link = 0,
		.max_mlo_peer = 256,

		.otp_board_id_register = 0,

		.supports_sta_ps = false,

		.acpi_guid = NULL,
		.supports_dynamic_smps_6ghz = false,
		.iova_mask = 0,
		.supports_aspm = false,

		.ce_ie_addr = &ath12k_wifi7_ce_ie_addr_ipq5424,
		.ce_remap = &ath12k_wifi7_ce_remap_ipq5424,
		.en_qdsslog = true,
		.support_fse = true,
		.alloc_cacheable_memory = true,
		.spectral = {
			.fft_sz = 7,
			.fft_pad_sz = 0,
			.summary_pad_sz = 16,
			.fft_hdr_len = 24,
			.max_fft_bins = 512,
			.fragment_160mhz = false,
		},
		.supports_ap_ps = true,
		.support_ce_manual_poll=true,
		.ftm_responder = false,
		.credit_flow = false,
		.is_plink_preferable = true,
		.support_umac_reset = true,
		.umac_irq_line_reset = false,
		.umac_reset_ipc = ATH12K_UMAC_RESET_IPC_IPQ5332,
		.ds_support = true,
		.cfr_support = true,
		.cfr_dma_hdr_size = sizeof(struct ath12k_cfir_enh_dma_hdr),
		.cfr_num_stream_bufs = 128,
		/* Max size :
		 * sizeof(ath12k_csi_cfr_header) + 96 bytes(cfr header) +
		 *                              15744 bytes(cfr payload)
		 * where cfr_header = rtt upload header len +
		 *                    freeze_tlv len + uplink user setup info
		 *                  = 32bytes + 32bytes + (8bytes * 4users)
		 */
		.cfr_stream_buf_size = sizeof(struct ath12k_csi_cfr_header) +
					(CFR_HDR_MAX_LEN_WORDS_IPQ5424 * 4) +
					CFR_DATA_MAX_LEN_IPQ5424,
	},
};

static int ath12k_mac_op_create_datapath_offload_if(struct ieee80211_hw *hw,
						    struct ieee80211_vif *vif,
						    struct net_device *dev)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	int ppe_vp_num;

	/* Allocate a PASSIVE VP at VAP init.
	 * Later update the VP type at the time of add interface
	 */
	ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
	if (ppe_vp_num <= 0) {
		ahvif->dp_vif.ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
		WARN_ON(ath12k_vif_alloc_vp(ahvif, PPE_VP_USER_TYPE_PASSIVE, NULL, dev));
	}

	return 0;
}

static int ath12k_mac_op_destroy_datapath_offload_if(struct ieee80211_hw *hw,
						     struct ieee80211_vif *vif,
						     struct net_device *dev)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	int ppe_vp_num;

	/* No init method is registered for monitor VAP
	 * and hence no VP will be created for the same.
	 */
	if (vif->type == NL80211_IFTYPE_MONITOR)
		return 0;

	/* Free the VP is not yet done in deinit
	 */
	ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
	if (ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM)
		ath12k_vif_free_vp(ahvif, dev);

	return 0;
}

static int ath12k_mac_op_set_mtu(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 int mtu)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);
	int ret = 0;

	if (!wdev)
		return -ENODEV;

	wiphy_lock(ahvif->ah->hw->wiphy);
	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR || !wdev->netdev) {
		wiphy_unlock(ahvif->ah->hw->wiphy);
		return 0;
	}

	if (ahvif->dp_vif.ppe_vp_type != ATH12K_INVALID_PPE_VP_TYPE &&
	    ahvif->dp_vif.ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM) {
		ret = ath12k_vif_set_mtu(ahvif, mtu);
	}

	wiphy_unlock(ahvif->ah->hw->wiphy);

	return ret;
}

/* Note: called under rcu_read_lock() */
static void ath12k_wifi7_mac_op_tx(struct ieee80211_hw *hw,
				   struct ieee80211_tx_control *control,
				   struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif = &ahvif->deflink;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct ath12k_mgmt_frame_stats *mgmt_stats = &ahvif->mgmt_stats;
	struct ieee80211_sta *sta = control->sta;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_link_vif *tmp_arvif;
	struct ath12k_sta *ahsta = NULL;
	u32 info_flags = info->flags;
	struct ieee80211_mgmt *mgmt;
	struct sk_buff *msdu_copied;
	struct ath12k *ar, *tmp_ar;
	struct ath12k_pdev_dp *dp_pdev, *tmp_dp_pdev;
	struct ath12k_dp_link_peer *peer = NULL;
	unsigned long links_map;
	bool is_mcast = false, is_eth = false;
	bool is_dvlan = false;
	struct ethhdr *eth;
	bool is_prb_rsp;
	u16 frm_type = 0;
	u16 mcbc_gsn;
	u8 link_id;
	int ret;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ieee80211_free_txskb(hw, skb);
		return;
	}

	link_id = u32_get_bits(info->control.flags, IEEE80211_TX_CTRL_MLO_LINK);
	memset(skb_cb, 0, sizeof(*skb_cb));
	skb_cb->vif = vif;

	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	/* handle only for MLO case, use deflink for non MLO case */
#ifdef CPTCFG_MAC80211_SFE_SUPPORT
	if (likely(skb->fast_xmit &&
		   ((skb->mark & ATH12K_MLO_METADATA_MLO_ASSIST_TAG_MASK) ==
		    ATH12K_MLO_METADATA_MLO_ASSIST_TAG))) {
		link_id =  u32_get_bits(skb->mark, ATH12K_MLO_METADATA_LINKID_MASK);
		skb_cb->link_id = link_id;

		arvif = rcu_dereference(ahvif->link[link_id]);

		if (!arvif || !arvif->ar) {
			ieee80211_free_txskb(hw, skb);
			return;
		}

		ar = arvif->ar;

		dp_pdev = ath12k_dp_to_dp_pdev(ar->ab->dp, ar->pdev_idx);
		if (!dp_pdev) {
			ieee80211_free_txskb(hw, skb);
			return;
		}

		ret = ath12k_mac_tx_check_max_limit(dp_pdev, skb);
		if (ret) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "failed due to limit check pdev idx %d\n",
				   ar->pdev_idx);
			ieee80211_free_txskb(hw, skb);
			return;
		}

		switch (ahvif->dp_vif.tx_encap_type) {
			case ATH12K_HW_TXRX_ETHERNET:
				skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
				ret = ath12k_wifi7_dp_tx(dp_pdev, arvif, skb, false, 0, is_mcast, arsta);
				break;
			case ATH12K_HW_TXRX_NATIVE_WIFI:
				ath12k_dp_tx_encap_nwifi(skb);
				ret = ath12k_wifi7_dp_tx(dp_pdev, arvif, skb, false, 0, is_mcast, arsta);
				break;
			case ATH12K_HW_TXRX_RAW:
			default:
				ret = -EINVAL;
		}
		if (unlikely(ret)) {
			ath12k_warn(ar->ab, "failed to transmit frame %d\n", ret);
			ieee80211_free_txskb(ar->ah->hw, skb);
			return;
		}

		return;
	} else if (ieee80211_vif_is_mld(vif)) {
#else
	if (ieee80211_vif_is_mld(vif)) {
#endif
		link_id = ath12k_mac_get_tx_link(sta, vif, link_id, skb, info_flags);
		if (link_id >= ATH12K_NUM_MAX_LINKS ||
		    (ATH12K_SCAN_LINKS_MASK & BIT(link_id))) {
			ieee80211_free_txskb(hw, skb);
			return;
		}
	} else {
		link_id = 0;
	}

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (!arvif || !arvif->ar) {
		ath12k_warn(ahvif->ah, "failed to find arvif link id %u for frame transmission",
			    link_id);
		ieee80211_free_txskb(hw, skb);
		return;
	}

	ar = arvif->ar;
	skb_cb->link_id = link_id;

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		if (ahsta->use_4addr_set)
			arsta = rcu_dereference(ahsta->link[link_id]);
	}


	/* as skb_cb is common currently for dp and mgmt tx processing
	 * set this in the common mac op tx function.
	 */
	skb_cb->u.ar = ar;
	is_prb_rsp = ieee80211_is_probe_resp(hdr->frame_control);

	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		is_mcast = is_multicast_ether_addr(eth->h_dest);
		is_eth = true;
		skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
	} else if (ieee80211_is_mgmt(hdr->frame_control)) {
		if (is_prb_rsp && arvif->tbtt_offset) {
			u64 adjusted_tsf;

			mgmt = (struct ieee80211_mgmt *)skb->data;
			adjusted_tsf = cpu_to_le64(0ULL - arvif->tbtt_offset);
			memcpy(&mgmt->u.probe_resp.timestamp, &adjusted_tsf,
			       sizeof(adjusted_tsf));
		}

		if (ath12k_mac_is_bridge_vdev(arvif)) {
			ieee80211_free_txskb(hw, skb);
			return;
		}

		frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
		ret = ath12k_mac_mgmt_tx(ar, skb, is_prb_rsp);
		if (ret) {
			if (ret != -EBUSY)
				ath12k_warn(ar->ab, "failed to queue mgmt stype 0x%x frame %d\n", frm_type, ret);
			ieee80211_free_txskb(hw, skb);
			spin_lock_bh(&ar->data_lock);
			mgmt_stats->tx_fail_cnt[frm_type]++;
			spin_unlock_bh(&ar->data_lock);
		} else {
			spin_lock_bh(&ar->data_lock);
			mgmt_stats->tx_succ_cnt[frm_type]++;
			spin_unlock_bh(&ar->data_lock);
		}
		return;
	}

	if (!(info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP))
		is_mcast = is_multicast_ether_addr(hdr->addr1);

	/* This is case only for P2P_GO */
	if (vif->type == NL80211_IFTYPE_AP && vif->p2p)
		ath12k_mac_add_p2p_noa_ie(ar, vif, skb, is_prb_rsp);

	dp_pdev = ath12k_dp_to_dp_pdev(ar->ab->dp, ar->pdev_idx);
	if (!dp_pdev) {
		ieee80211_free_txskb(hw, skb);
		return;
	}

	/* Checking if it is a DVLAN frame */
	if (!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags) &&
	    !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	    ieee80211_has_protected(hdr->frame_control))
		is_dvlan = true;

	if (!vif->valid_links || !is_mcast || is_dvlan || is_eth ||
	    test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ar->ab->ag->flags)) {
		ret = ath12k_mac_tx_check_max_limit(dp_pdev, skb);
		if (ret) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "failed due to limit check pdev idx %d\n",
				   ar->pdev_idx);
			ieee80211_free_txskb(hw, skb);
			return;
		}

		ret = ath12k_wifi7_dp_tx(dp_pdev, arvif, skb, false, 0, is_mcast, arsta);
		if (unlikely(ret)) {
			ath12k_warn(ar->ab, "failed to transmit frame %d\n", ret);
			ieee80211_free_txskb(ar->ah->hw, skb);
			return;
		}
	} else {
		mcbc_gsn = atomic_inc_return(&ahvif->dp_vif.mcbc_gsn) & 0xfff;

		links_map = ahvif->links_map;
		for_each_set_bit(link_id, &links_map,
				 IEEE80211_MLD_MAX_NUM_LINKS) {
			tmp_arvif = rcu_dereference(ahvif->link[link_id]);
			if (!tmp_arvif || !tmp_arvif->is_up)
				continue;

			tmp_ar = tmp_arvif->ar;
			tmp_dp_pdev = ath12k_dp_to_dp_pdev(tmp_ar->ab->dp, tmp_ar->pdev_idx);
			if (!tmp_dp_pdev)
				continue;

			ret = ath12k_mac_tx_check_max_limit(tmp_dp_pdev, skb);
			if (ret) {
				ath12k_dbg(tmp_ar->ab, ATH12K_DBG_MAC,
					   "failed mcast tx due to limit check pdev idx %d\n",
					    tmp_ar->pdev_idx);
				continue;
			}

			msdu_copied = skb_copy(skb, GFP_ATOMIC);
			if (!msdu_copied) {
				ath12k_err(ar->ab,
					   "skb copy failure link_id 0x%X vdevid 0x%X\n",
					   link_id, tmp_arvif->vdev_id);
				continue;
			}

			ath12k_mlo_mcast_update_tx_link_address(vif, link_id,
								msdu_copied,
								info_flags);

			skb_cb = ATH12K_SKB_CB(msdu_copied);
			skb_cb->link_id = link_id;
			skb_cb->vif = vif;
			skb_cb->u.ar = tmp_ar;

			if (ahsta && ahsta->use_4addr_set)
				arsta = rcu_dereference(ahsta->link[link_id]);

			/* For open mode, skip peer find logic */
			if (unlikely(arvif->key_cipher == WMI_CIPHER_NONE))
				goto skip_peer_find;

			spin_lock_bh(&tmp_ar->ab->dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_addr(tmp_ar->ab->dp,
								tmp_arvif->bssid);
			if (!peer) {
				spin_unlock_bh(&tmp_ar->ab->dp->dp_lock);
				ath12k_warn(tmp_ar->ab,
					    "failed to find peer for vdev_id 0x%X addr %pM link_map 0x%X\n",
					    tmp_arvif->vdev_id, tmp_arvif->bssid,
					    ahvif->links_map);
				dev_kfree_skb_any(msdu_copied);
				continue;
			}

			key = peer->dp_peer->keys[peer->dp_peer->mcast_keyidx];
			if (key) {
				skb_cb->cipher = key->cipher;
				skb_cb->flags |= ATH12K_SKB_CIPHER_SET;

				hdr = (struct ieee80211_hdr *)msdu_copied->data;
				if (!ieee80211_has_protected(hdr->frame_control))
					hdr->frame_control |=
						cpu_to_le16(IEEE80211_FCTL_PROTECTED);
			}
			spin_unlock_bh(&tmp_ar->ab->dp->dp_lock);

skip_peer_find:
			ret = ath12k_wifi7_dp_tx(tmp_dp_pdev, tmp_arvif,
						 msdu_copied, true, mcbc_gsn, is_mcast, arsta);
			if (unlikely(ret)) {
				if (ret == -ENOMEM) {
					/* Drops are expected during heavy multicast
					 * frame flood. Print with debug log
					 * level to avoid lot of console prints
					 */
					ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
						   "failed to transmit frame %d\n",
						   ret);
				} else {
					ath12k_warn(ar->ab,
						    "failed to transmit frame %d\n",
						    ret);
				}

				dev_kfree_skb_any(msdu_copied);
			}
		}
		ieee80211_free_txskb(ar->ah->hw, skb);
	}
}

static const struct ieee80211_ops ath12k_ops_wifi7 = {
	.tx				= ath12k_wifi7_mac_op_tx,
	.wake_tx_queue			= ieee80211_handle_wake_tx_queue,
	.start                          = ath12k_mac_op_start,
	.stop                           = ath12k_mac_op_stop,
	.reconfig_complete              = ath12k_mac_op_reconfig_complete,
	.add_interface                  = ath12k_mac_op_add_interface,
	.remove_interface		= ath12k_mac_op_remove_interface,
	.update_vif_offload		= ath12k_mac_op_update_vif_offload,
	.config                         = ath12k_mac_op_config,
	.sta_set_4addr			= ath12k_mac_op_sta_set_4addr,
	.link_info_changed              = ath12k_mac_op_link_info_changed,
	.vif_cfg_changed		= ath12k_mac_op_vif_cfg_changed,
	.change_vif_links               = ath12k_mac_op_change_vif_links,
	.configure_filter		= ath12k_mac_op_configure_filter,
	.hw_scan                        = ath12k_mac_op_hw_scan,
	.cancel_hw_scan                 = ath12k_mac_op_cancel_hw_scan,
	.set_key                        = ath12k_mac_op_set_key,
	.set_rekey_data	                = ath12k_mac_op_set_rekey_data,
	.sta_state                      = ath12k_mac_op_sta_state,
	.sta_set_txpwr			= ath12k_mac_op_sta_set_txpwr,
	.link_sta_rc_update		= ath12k_mac_op_link_sta_rc_update,
	.conf_tx                        = ath12k_mac_op_conf_tx,
	.set_antenna			= ath12k_mac_op_set_antenna,
	.get_antenna			= ath12k_mac_op_get_antenna,
	.ampdu_action			= ath12k_mac_op_ampdu_action,
	.add_chanctx			= ath12k_mac_op_add_chanctx,
	.remove_chanctx			= ath12k_mac_op_remove_chanctx,
	.change_chanctx			= ath12k_mac_op_change_chanctx,
	.assign_vif_chanctx		= ath12k_mac_op_assign_vif_chanctx,
	.unassign_vif_chanctx		= ath12k_mac_op_unassign_vif_chanctx,
	.switch_vif_chanctx		= ath12k_mac_op_switch_vif_chanctx,
	.get_txpower			= ath12k_mac_op_get_txpower,
	.set_rts_threshold		= ath12k_mac_op_set_rts_threshold,
	.set_frag_threshold		= ath12k_mac_op_set_frag_threshold,
	.set_bitrate_mask		= ath12k_mac_op_set_bitrate_mask,
	.get_survey			= ath12k_mac_op_get_survey,
	.flush				= ath12k_mac_op_flush,
	.sta_statistics			= ath12k_mac_op_sta_statistics,
	.link_sta_statistics		= ath12k_mac_op_link_sta_statistics,
	.remain_on_channel              = ath12k_mac_op_remain_on_channel,
	.cancel_remain_on_channel       = ath12k_mac_op_cancel_remain_on_channel,
	.change_sta_links               = ath12k_mac_op_change_sta_links,
	.can_activate_links             = ath12k_mac_op_can_activate_links,
#ifdef CONFIG_PM
	.suspend			= ath12k_wow_op_suspend,
	.resume				= ath12k_wow_op_resume,
	.set_wakeup			= ath12k_wow_op_set_wakeup,
#endif
#ifdef CPTCFG_ATH12K_DEBUGFS
	.vif_add_debugfs                = ath12k_debugfs_op_vif_add,
#endif
	CFG80211_TESTMODE_CMD(ath12k_tm_cmd)
#ifdef CPTCFG_ATH12K_DEBUGFS
	.link_sta_add_debugfs           = ath12k_debugfs_link_sta_op_add,
#endif
	.link_reconfig_remove           = ath12k_mac_op_link_reconfig_remove,
	.removed_link_is_primary        = ath12k_mac_op_removed_link_is_primary,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.change_mtu			= ath12k_mac_op_set_mtu,
	.init_interface			= ath12k_mac_op_create_datapath_offload_if,
	.deinit_interface		= ath12k_mac_op_destroy_datapath_offload_if,
#endif
	.can_neg_ttlm			= ath12k_mac_op_can_neg_ttlm,
	.apply_neg_ttlm_per_client	= ath12k_mac_op_apply_neg_ttlm_per_client,
};

int ath12k_wifi7_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
	struct ath12k_hw_params *hw_params_msi8 = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi7_hw_params); i++) {
		hw_params = &ath12k_wifi7_hw_params[i];

		if (hw_params->hw_rev == ab->hw_rev)
			break;
	}

	if (i == ARRAY_SIZE(ath12k_wifi7_hw_params)) {
		ath12k_err(ab, "Unsupported WiFi7 hardware version: 0x%x\n",
			   ab->hw_rev);
		return -EINVAL;
	}

	if (ab->hif.bus == ATH12K_BUS_PCI &&
	    ab->msi.config->total_vectors == ATH12K_MSI_16) {
		hw_params_msi8 = kzalloc(sizeof(struct ath12k_hw_params), GFP_KERNEL);
		if (!hw_params_msi8)
			return -ENOMEM;
		memcpy(hw_params_msi8, hw_params, sizeof(struct ath12k_hw_params));
		/* Include it when PPEDS patch are rebased
		hw_params_msi8->ext_irq_grp_num_max = 6;
		 */
		hw_params_msi8->ring_mask = &ath12k_wifi7_hw_ring_mask_qcn9274_msi8;
		ab->hw_params = hw_params_msi8;
	} else {
		ab->hw_params = hw_params;
	}
	ab->ath12k_ops = &ath12k_ops_wifi7;

	ath12k_info(ab, "WiFi7 Hardware name: %s\n", ab->hw_params->name);

	return 0;
}
