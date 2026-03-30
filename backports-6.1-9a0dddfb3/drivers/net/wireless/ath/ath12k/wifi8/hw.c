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
#include "dp.h"
#include "hal_qcn9625.h"
#include "mgmt_rx.h"
#include "dp_peer.h"

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

static bool ath12k_wifi8_hw_link_id_required_in_mgmt_send_qcn9625(struct ath12k_base *ab)
{
	return true;
}

static void ath12k_wifi8_hw_rx_peer_ba_config_qcn9625(struct ath12k_base *ab, u8 tid,
						      u32 *ba_win_size, u16 *ssn)
{
	u32 ba_win_size_val = 1;
	u16 ssn_val = 0;

	if (ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid)) {
		ba_win_size_val = ab->ag->num_devices;
		/* SSN=0 is used for association request before queue setup */
		ssn_val = 1;
	}

	if (ba_win_size)
		*ba_win_size = ba_win_size_val;

	if (ssn)
		*ssn = ssn_val;
}

static bool
ath12k_wifi8_rx_peer_tid_skip_pn_replay_qcn9625(struct ath12k_base *ab, u8 tid)
{
	return (tid == HAL_MGMT_BCAST_TID || tid == HAL_MGMT_SENSING_TID);
}

/* Note: called under rcu_read_lock() */
static void
ath12k_dp_peer_migration_qcn9625(struct ath12k_link_vif *arvif,
				 struct ath12k_mac_pri_link_migr_peer_node *peer_node)
{
	struct ath12k_link_sta *arsta, *old_arsta;
	struct ath12k_dp_link_peer *old_peer, *new_peer;
	struct ath12k_link_vif *old_arvif;
	struct ath12k_hw *ah = arvif->ar->ah;
	struct ath12k_dp_peer *ml_peer;
	struct ath12k_sta *ahsta;
	u8 old_link_id;

	/* The primary_link_id needs to be updated here based on the link_id sent
	 * in the migration command.
	 */
	ml_peer = rcu_dereference(ah->dp_hw.dp_peer_list[peer_node->ml_peer_id]);
	if (ml_peer && ml_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED) {
		ahsta = ath12k_sta_to_ahsta(ml_peer->sta);
		old_link_id = ahsta->primary_link_id;

		/* Clear old primary link flags */
		if (old_link_id != peer_node->pri_link_id) {
			old_arsta = rcu_dereference(ahsta->link[old_link_id]);
			if (old_arsta && old_arsta->arvif) {
				old_arvif = old_arsta->arvif;
				old_arvif->primary_sta_link = false;
			}

			old_peer = rcu_dereference(ml_peer->link_peers[old_link_id]);
			if (old_peer)
				old_peer->primary_link = false;
		}

		/* Update to new primary link */
		ahsta->primary_link_id = peer_node->pri_link_id;

		/* Set new primary link flags */
		arsta = rcu_dereference(ahsta->link[peer_node->pri_link_id]);
		arsta->arvif->primary_sta_link = true;

		new_peer = rcu_dereference(ml_peer->link_peers[peer_node->pri_link_id]);
		if (new_peer)
			new_peer->primary_link = true;

		ahsta->is_migration_in_progress = false;

		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_WMI,
			   "Updated primary_link_id from %u to %u for sta %pM\n",
			   old_link_id, peer_node->pri_link_id, ml_peer->sta->addr);
	}
}

static const struct ath12k_hw_ops qcn9625_ops = {
	.get_hw_mac_from_pdev_id = ath12k_wifi8_hw_qcn9625_mac_from_pdev_id,
	.mac_id_to_pdev_id = ath12k_wifi8_hw_mac_id_to_pdev_id_qcn9625,
	.mac_id_to_srng_id = ath12k_wifi8_hw_mac_id_to_srng_id_qcn9625,
	.rxdma_ring_sel_config = ath12k_wifi8_dp_rxdma_ring_sel_config_qcn9625,
	.get_ring_selector = ath12k_wifi8_hw_get_ring_selector_qcn9625,
	.dp_srng_is_tx_comp_ring = ath12k_wifi8_dp_srng_is_comp_ring_qcn9625,
	.hw_link_id_required_in_mgmt_send =
		ath12k_wifi8_hw_link_id_required_in_mgmt_send_qcn9625,
	.mgmt_rxdma_ring_sel_config = ath12k_wifi8_mgmt_wbm_ring_sel_config_qcn9625,
	.rx_peer_ba_config = ath12k_wifi8_hw_rx_peer_ba_config_qcn9625,
	.rx_peer_tid_skip_pn_replay = ath12k_wifi8_rx_peer_tid_skip_pn_replay_qcn9625,
	.dp_peer_migration = ath12k_dp_peer_migration_qcn9625,
};

/* Interrupt Grouping is as follows
 * Group 0-3: Tx completion
 * Group 4-7: Rx ring
 * Group 8: Tx exception ring
 * Group 9: Rx error, Reo Status, TCL status, TQM status
 * Group 10,11 : Monitor destination(TX,RX)
 * Group 12: Monitor buffer(TX,RX)
 * Group 19-21: PPE interrupts
 * Group 22: UMAC reset
 */
static struct ath12k_hw_ring_mask ath12k_wifi8_hw_ring_mask_qcn9625 = {
	/* Group 0-3, 5th ring uses group 10 */
	.tx  = {
		ATH12K_TX_RING_MASK_0,
		ATH12K_TX_RING_MASK_1,
		ATH12K_TX_RING_MASK_2,
		ATH12K_TX_RING_MASK_3,
		0, 0, 0, 0,
		0, 0,
		ATH12K_TX_RING_MASK_4,
	},
	/* Group 4-7, 5th ring uses group 11 */
	.rx = {
		0, 0, 0, 0,
		ATH12K_RX_RING_MASK_0,
		ATH12K_RX_RING_MASK_1,
		ATH12K_RX_RING_MASK_2,
		ATH12K_RX_RING_MASK_3,
		0, 0, 0,
		ATH12K_RX_RING_MASK_4,
	},
	/* Group 8 */
	.tx_exception = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_TX_EXCEPTION_RING_MASK_0,
	},
	/* Group 9 */
	.rx_err = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_RX_ERR_RING_MASK_0,
	},
	/* Group 9 */
	.reo_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_REO_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.tcl_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TCL_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.tqm_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TQM_STATUS_RING_MASK_0,
	},
	.sam_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_SAM_STATUS_RING_MASK_0,
	},
	/* Group 9 */
	.ase_status = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_ASE_STATUS_RING_MASK_0,
	},
	/* Group 9*/
	.reo_flush = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_REO_FLUSH_RING_MASK_0,
	},
	/* Group 10, 11 */
	.rx_mon_dest = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_RX_MON_RING_MASK_0,
		ATH12K_RX_MON_RING_MASK_1,
	},
	/* Group 12 */
	.host2rxmon = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_HOST2RXMON_RING_MASK_0,
	},
	/* Group 13 */
	.tx_peer_telemetry = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_TX_PEER_TELEMETRY_RING_MASK
	},
	.rx_peer_telemetry = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
		ATH12K_RX_PEER_TELEMETRY_RING_MASK
	},

	/* Group 19-21 */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppe2tcl = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
		ATH12K_PPE2TCL_RING_MASK_0,
	},
	.reo2ppe = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0,
		ATH12K_REO2PPE_RING_MASK_0,
	},
	.ppeds_tx_cmpln = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		ATH12K_PPE_WBM2SW_RELEASE_RING_MASK_0
	},
#endif
	/* Group 22 */
	.umac_dp_reset = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
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
			.std_elf_img = true,
			.unified_fw_image = true,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.host_ce_config = ath12k_wifi8_host_ce_config_qcn9625,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi8_target_ce_config_wlan_qcn9625,
		.target_ce_count = 13,
		/* TODO: CP: update CE maps and definitions later when available */
		.svc_to_ce_map =
			ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625,
		.svc_to_ce_map_len = 21,
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

		.num_tcl_banks = 128,
		.max_tx_ring = MIN(NR_CPUS, 5),

		.mhi_config = &ath12k_wifi8_mhi_config_qcn9625,

		.wmi_init = ath12k_wifi8_wmi_init_qcn9625,

		.hal_ops = &hal_qcn9625_ops,
		.cp_arch_ops = &ath12k_wifi8_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_AUX_UC_SUPPORT_V01),

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

		.supports_aspm = true,

		.current_cc_support = false,

		.ce_ie_addr = NULL,
		.ce_remap = NULL,
		.handle_beacon_miss = true,
		.en_qdsslog = true,
		.support_fse = true,
		.cumac_support = false,
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
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = false,
#endif
		.mlo_3_link_tx_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.num_rx_spt_pages = ATH12K_NUM_RX_SPT_PAGES,
		.peer_del_all_support = true,
	},
	{
		.name = "qcn9589 hw1.0",
		.hw_rev = ATH12K_HW_QCN9589_HW10,
		.fw = {
			.dir = "QCN9589/hw1.0",
			.board_size = 256 * 1024,
			.cal_offset = 128 * 1024,
			.m3_loader = ath12k_m3_fw_loader_driver,
			.std_elf_img = true,
			.unified_fw_image = true,
		},
		.max_radios = 2,
		.single_pdev_only = false,
		.qmi_service_ins_id = ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9625,
		.internal_sleep_clock = false,

		.hw_ops = &qcn9625_ops,
		.ring_mask = &ath12k_wifi8_hw_ring_mask_qcn9625,

		.host_ce_config = ath12k_wifi8_host_ce_config_qcn9625,
		.ce_count = 16,
		.target_ce_config = ath12k_wifi8_target_ce_config_wlan_qcn9625,
		.target_ce_count = 13,
		.svc_to_ce_map =
			ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625,
		.svc_to_ce_map_len = 21,
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

		.num_tcl_banks = 128,
		.max_tx_ring = 4,

		.mhi_config = &ath12k_wifi8_mhi_config_qcn9625,

		.wmi_init = ath12k_wifi8_wmi_init_qcn9625,

		.hal_ops = &hal_qcn9625_ops,
		.cp_arch_ops = &ath12k_wifi8_cp_ops,

		.qmi_cnss_feature_bitmap = BIT(CNSS_QDSS_CFG_MISS_V01) |
					   BIT(CNSS_AUX_UC_SUPPORT_V01),

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
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		.ds_support = false,
#endif
		.mlo_3_link_tx_support = true,
		.board_magic = "QCA-ATH12K-BOARD",
		.ext_irq_grp_num_max = ATH12K_EXT_IRQ_GRP_NUM_MAX,
		.num_rx_spt_pages = ATH12K_NUM_RX_SPT_PAGES,
	},
};

static bool ath12k_wifi8_mac_is_mgmt_action_link_agnostic(struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
	const u8 *buf = (u8 *)&mgmt->u.action;
	u8 category, action_code, iv_len;

	/* Offset by iv_len if it is a protected frame */
	if (ieee80211_has_protected(mgmt->frame_control)) {
		switch (ATH12K_SKB_CB(skb)->cipher) {
		/* Other cipher types than CCMP  will be sanitized in
		 * ath12k_mac_mgmt_action_frame_fill_elem.
		 */
		case WLAN_CIPHER_SUITE_CCMP:
			iv_len = IEEE80211_CCMP_HDR_LEN;
			break;
		default:
			iv_len = 0;
			break;
		}

		buf += iv_len;
	}

	category = *buf++;
	action_code = *buf++;

	switch (category) {
	case WLAN_CATEGORY_PROTECTED_EHT:
		switch (action_code) {
		case WLAN_PROTECTED_EHT_ACTION_ML_OP_UPDATE_REQ:
		case WLAN_PROTECTED_EHT_ACTION_ML_OP_UPDATE_RESP:
			fallthrough;
		case WLAN_PROTECTED_EHT_ACTION_LINK_RECONFIG_REQ:
		case WLAN_PROTECTED_EHT_ACTION_LINK_RECONFIG_RESP:
			/* Exempt Link Reconfig Req/Resp frames from tx-ed as
			 * link-agnostic since both frames should be exchanged on
			 * the same link.
			 *
			 * IEEE P802.11be/D7.0, 35.3.6.4 - Link reconfiguration to
			 * the ML setup
			 */
			return false;
		}
		break;
	case WLAN_CATEGORY_RADIO_MEASUREMENT:
		return false;
	case WLAN_CATEGORY_WNM:
		switch (action_code) {
		case WLAN_WNM_ACTION_EVENT_REQ:
		case WLAN_WNM_ACTION_EVENT_RESP:
		case WLAN_WNM_ACTION_DIAGNOSTIC_REQ:
		case WLAN_WNM_ACTION_DIAGNOSTIC_RESP:
		case WLAN_WNM_ACTION_LOCATION_CFG_REQ:
		case WLAN_WNM_ACTION_LOCATION_CFG_RESP:
		case WLAN_WNM_ACTION_BSS_TM_QUERY:
			return false;
		}
		break;
	default:
		/* Extend as per feature addition */
		break;
	}

	return true;
}

/* This function should be called only for mgmt frames to a Multi-Link device,
 * after meeting master link eligibility. Hence, such sanity checks are skipped.
 */
static bool ath12k_wifi8_mac_is_mgmt_link_agnostic(struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)skb->data;
	u16 fc = le16_to_cpu(mgmt->frame_control);

	switch (fc & IEEE80211_FCTL_STYPE) {
	case IEEE80211_STYPE_DEAUTH:
	case IEEE80211_STYPE_DISASSOC:
		return true;
	case IEEE80211_STYPE_ACTION:
		return ath12k_wifi8_mac_is_mgmt_action_link_agnostic(skb);
	default:
		break;
	}

	return false;
}

/* Note: called under rcu_read_lock() */
static u8
ath12k_wifi8_mac_get_tx_link(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
			     u8 link, struct sk_buff *skb, u32 info_flags)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_bss_conf *bss_conf;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab = NULL;
	u8 user_link = link, link_id;
	struct ath12k_sta *ahsta;
	struct ath12k *ar = NULL;
	unsigned long links;

	/* Use the link id passed or the first available link */
	if (!sta) {
		if (link != IEEE80211_LINK_UNSPECIFIED)
			return link;

		return ffs(ahvif->links_map) - 1;
	}

	ahsta = ath12k_sta_to_ahsta(sta);

	/* Below translation ensures we pass proper A2 & A3 for non ML clients.
	 * Also it assumes for now support only for MLO AP in this path
	 */
	if (!sta->mlo) {
		link = ahsta->deflink.link_id;

		if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
			return link;

		bss_conf = rcu_dereference(vif->link_conf[link]);
		if (bss_conf) {
			ether_addr_copy(hdr->addr2, bss_conf->addr);
			if (!ieee80211_has_tods(hdr->frame_control) &&
			    !ieee80211_has_fromds(hdr->frame_control))
				ether_addr_copy(hdr->addr3, bss_conf->addr);
		}

		return link;
	}

	/* enqueue eth enacap & data frames on primary link, FW does link
	 * selection and address translation.
	 */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP ||
	    ieee80211_is_data(hdr->frame_control))
		return ahsta->primary_link_id;

	/* 802.11 frame cases */
	if (link == IEEE80211_LINK_UNSPECIFIED)
		link = ahsta->deflink.link_id;

	if (!ieee80211_is_mgmt(hdr->frame_control))
		return link;

	if (ahsta->deflink.arvif->ar) {
		ar = ahsta->deflink.arvif->ar;
		ab = ar->ab;
	}

	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    ieee80211_has_protected(hdr->frame_control))
		goto skip_link_agnostic_tx;

	if (ab && test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
	    ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
		/* If disassoc frame comes in crash link, need to
		 * change the link which is active at that instance.
		 */
		if (ieee80211_is_disassoc(hdr->frame_control)) {
			link = ahsta->deflink.link_id;
			links = ahsta->links_map;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta = rcu_dereference(ahsta->link[link_id]);
				if (!arsta)
					continue;
				ab = arsta->arvif->ar->ab;
				if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
					link = arsta->link_id;
					break;
				}
			}
			goto skip_link_agnostic_tx;
		}
	}

	/* Check if this mgmt frame can be queued at MLD level, in which case,
	 * the frame will be transmitted on master (primary) link. An individually
	 * addressed mgmt frame can be transmitted on master link after peer assoc.
	 */
	if (user_link != IEEE80211_LINK_UNSPECIFIED ||
	    ahsta->state <= IEEE80211_STA_ASSOC ||
	    !ath12k_wifi8_mac_is_mgmt_link_agnostic(skb))
		goto skip_link_agnostic_tx;

	ATH12K_SKB_CB(skb)->flags |= ATH12K_SKB_MGMT_LINK_AGNOSTIC;
	link = ahsta->primary_link_id;

skip_link_agnostic_tx:
	/* Perform address conversion for ML STA Tx */
	bss_conf = rcu_dereference(vif->link_conf[link]);
	link_sta = rcu_dereference(sta->link[link]);

	if (bss_conf && link_sta) {
		ether_addr_copy(hdr->addr1, link_sta->addr);
		ether_addr_copy(hdr->addr2, bss_conf->addr);

		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);

		return link;
	}

	if (bss_conf) {
		/* In certain cases where a ML sta associated and added subset of
		 * links on which the ML AP is active, but now sends some frame
		 * (ex. Probe request) on a different link which is active in our
		 * MLD but was not added during previous association, we can
		 * still honor the Tx to that ML STA via the requested link.
		 * The control would reach here in such case only when that link
		 * address is same as the MLD address or in worst case clients
		 * used MLD address at TA wrongly which would have helped
		 * identify the ML sta object and pass it here.
		 * If the link address of that STA is different from MLD address,
		 * then the sta object would be NULL and control won't reach
		 * here but return at the start of the function itself with !sta
		 * check. Also this would not need any translation at hdr->addr1
		 * from MLD to link address since the RA is the MLD address
		 * (same as that link address ideally) already.
		 */
		ether_addr_copy(hdr->addr2, bss_conf->addr);

		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);
	}

	return link;
}

/* Note: called under rcu_read_lock() */
static void ath12k_wifi8_mac_op_tx(struct ieee80211_hw *hw,
				   struct ieee80211_tx_control *control,
				   struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ieee80211_vif *vlan_vif = control->vlan_vif;
	struct ath12k_vif *vlan_ahvif = vlan_vif ? ath12k_vif_to_ahvif(vlan_vif) : NULL;
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
	struct ath12k_pdev_dp *dp_pdev = NULL, *tmp_dp_pdev = NULL;
	struct ath12k_dp_link_peer *peer = NULL;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp *dp = NULL;
	unsigned long links_map;
	bool is_mcast = false, is_eth = false;
	bool is_dvlan = false;
	struct ethhdr *eth;
	bool is_prb_rsp;
	u32 qos_nw_delay = info->sawf.nw_delay;
	u16 frm_type = 0;
	u16 mcbc_gsn;
	u8 link_id;
#ifdef CPTCFG_MAC80211_SFE_SUPPORT
	u8 tid;
#endif
	int ret;
	u8 qos_tag;
	enum ath12k_dp_tx_enq_error err;
	u8 ring_id = 0, ring_selector = 0;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
						sta, dp_vif,
						DP_TX_ENQ_DROP_VIF_TYPE_MON,
						ring_id, false);
		return;
	}

	link_id = u32_get_bits(info->control.flags, IEEE80211_TX_CTRL_MLO_LINK);
	if (unlikely(!(skb->fast_xmit &&
		       ((skb->mark & ATH12K_MLO_METADATA_MLO_ASSIST_TAG_MASK) ==
		       ATH12K_MLO_METADATA_MLO_ASSIST_TAG)) || !hw->perf_mode)) {
		memset(skb_cb, 0, sizeof(*skb_cb));
		skb_cb->vif = vif;

		if (key) {
			skb_cb->cipher = key->cipher;
			skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
		}
	}

	/* handle only for MLO case, use deflink for non MLO case */
#ifdef CPTCFG_MAC80211_SFE_SUPPORT
	if (likely(skb->fast_xmit &&
		   ((skb->mark & ATH12K_MLO_METADATA_MLO_ASSIST_TAG_MASK) ==
		    ATH12K_MLO_METADATA_MLO_ASSIST_TAG))) {
		link_id =  u32_get_bits(skb->mark, ATH12K_MLO_METADATA_LINKID_MASK);
		skb_cb->link_id = link_id;

		arvif = rcu_dereference(ahvif->link[link_id]);

		if (unlikely(!arvif || !arvif->ar)) {
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_INV_ARVIF_FAST,
							ring_id, false);

			return;
		}

		ar = arvif->ar;
		skb_cb->u.ar = ar;

		dp_pdev = ath12k_dp_to_dp_pdev(ar->ab->dp, ar->pdev_idx);
		if (unlikely(!dp_pdev)) {
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_INV_PDEV_FAST,
							ring_id, false);

			return;
		}

		ret = ath12k_mac_tx_check_max_limit(dp_pdev, skb);
		if (unlikely(ret)) {
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					 "failed due to limit check pdev idx %d\n",
					 ar->pdev_idx);
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_MAX_TX_LIMIT_FAST,
							ring_id, false);

			return;
		}

		switch (ahvif->dp_vif.tx_encap_type) {
		case ATH12K_HW_TXRX_ETHERNET:
			skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
			err = ath12k_wifi8_dp_tx_fast(dp_pdev, arvif,
						      vlan_ahvif,
						      skb,
						      qos_nw_delay);
			break;
		case ATH12K_HW_TXRX_NATIVE_WIFI:
			ath12k_dp_tx_encap_nwifi(skb);
			err = ath12k_wifi8_dp_tx_fast(dp_pdev, arvif,
						      vlan_ahvif,
						      skb,
						      qos_nw_delay);
			break;
		case ATH12K_HW_TXRX_RAW:
		default:
			err = DP_TX_ENQ_DROP_INV_ENCAP_FAST;
		}
		if (unlikely(err)) {
			ring_selector =
				dp_pdev->dp->hw_params->hw_ops->get_ring_selector(skb);
			ring_id = ring_selector % dp_pdev->dp->hw_params->max_tx_ring;

			if (ath12k_mac_check_err_code_debug_logging(err))
				ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						 "failed to transmit frame %d\n", err);
			else
				ath12k_warn(ar->ab, "failed to transmit frame %d\n", err);

			ath12k_mac_ieee80211_free_txskb(ar->ah->hw, skb, dp_pdev,
							sta, dp_vif,
							err, ring_id, false);

			return;
		}
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev) &&
			     ath12k_tid_stats_enabled(dp_pdev))) {
			tid = skb->priority &
			      IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, skb->len,
					    ATH_TX_SFE_PKTS);
		}

		return;
	} else if (ieee80211_vif_is_mld(vif)) {
#else
	if (ieee80211_vif_is_mld(vif)) {
#endif
		link_id = ath12k_wifi8_mac_get_tx_link(sta, vif, link_id, skb,
						       info_flags);
		if (link_id >= ATH12K_NUM_MAX_LINKS ||
		    (ATH12K_SCAN_LINKS_MASK & BIT(link_id))) {
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_INV_LINK,
							ring_id, false);
			return;
		}
	} else {
		link_id = 0;
	}

	/* MLO params should always be added to class-3 frames queued
	 * for peers on Wi-Fi 8.
	 */
	if (ieee80211_is_mgmt(hdr->frame_control) && sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		if (ahsta->state == IEEE80211_STA_AUTHORIZED)
			ATH12K_SKB_CB(skb)->flags |= ATH12K_SKB_MGMT_MLO_PARAMS;
	}

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (!arvif || !arvif->ar) {
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
						sta, dp_vif,
						DP_TX_ENQ_DROP_INV_ARVIF,
						ring_id, false);
		return;
	}

	ar = arvif->ar;
	skb_cb->link_id = link_id;

	if (unlikely(test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))) {
		ieee80211_free_txskb(hw, skb);
		return;
	}

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		qos_tag = u32_get_bits(skb->mark, QOS_TAG_MASK);
		if (ahsta->use_4addr_set || qos_tag)
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
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_BRIDGE_VDEV,
							ring_id, false);
			return;
		}

		frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
		ret = ath12k_mac_mgmt_tx(ar, skb, is_prb_rsp);
		if (ret) {
			if (ret != -EBUSY)
				ath12k_warn(ar->ab, "failed to queue mgmt stype 0x%x frame %d\n",
					    frm_type, ret);
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_MGMT_FRAME,
							ring_id, false);
			spin_lock_bh(&ar->data_lock);
			mgmt_stats->tx_fail_cnt[frm_type]++;
			mgmt_stats->aggr_tx_mgmt_fail_cnt++;
			spin_unlock_bh(&ar->data_lock);
		} else {
			spin_lock_bh(&ar->data_lock);
			mgmt_stats->tx_succ_cnt[frm_type]++;
			mgmt_stats->aggr_tx_mgmt_success_cnt++;
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
		ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
						sta, dp_vif,
						DP_TX_ENQ_DROP_INV_PDEV,
						ring_id, false);
		return;
	}

	dp = dp_pdev->dp;
	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);
	ring_id = ring_selector % dp->hw_params->max_tx_ring;

	/* Checking if it is a DVLAN frame */
	if (!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags) &&
	    !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	    ieee80211_has_protected(hdr->frame_control))
		is_dvlan = true;

	if (!vif->valid_links || !is_mcast || is_dvlan ||
	    (is_eth && (!is_mcast || sta)) ||
	    test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ar->ab->ag->flags)) {
		ret = ath12k_mac_tx_check_max_limit(dp_pdev, skb);
		if (ret) {
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
					 "failed due to limit check pdev idx %d\n",
					 ar->pdev_idx);
			ath12k_mac_ieee80211_free_txskb(hw, skb, dp_pdev,
							sta, dp_vif,
							DP_TX_ENQ_DROP_MAX_TX_LIMIT,
							ring_id, false);
			return;
		}

		err = ath12k_wifi8_dp_tx(dp_pdev, arvif, skb, false, 0, is_mcast,
					 arsta, ring_id, qos_nw_delay);
		if (unlikely(err)) {
			if (ath12k_mac_check_err_code_debug_logging(err))
				ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						 "failed to transmit frame %d\n", err);
			else
				ath12k_warn(ar->ab, "failed to transmit frame %d\n", err);

			ath12k_mac_ieee80211_free_txskb(ar->ah->hw, skb, dp_pdev,
							sta, dp_vif,
							err, ring_id, false);
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
			if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH,
					      &tmp_ar->ab->dev_flags)))
				continue;

			tmp_dp_pdev = ath12k_dp_to_dp_pdev(tmp_ar->ab->dp,
							   tmp_ar->pdev_idx);
			if (!tmp_dp_pdev)
				continue;

			ret = ath12k_mac_tx_check_max_limit(tmp_dp_pdev, skb);
			if (ret) {
				ath12k_dbg_level(tmp_ar->ab, ATH12K_DBG_MAC,
						 ATH12K_DBG_L2,
						 "failed mcast tx due to limit check pdev idx %d\n",
						 tmp_ar->pdev_idx);
				continue;
			}

			if (is_eth) {
				msdu_copied = skb_clone(skb, GFP_ATOMIC);
				if (!msdu_copied) {
					ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC,
							 ATH12K_DBG_L2,
							 "skb clone failure link_id 0x%X vdevid 0x%X\n",
							 link_id, tmp_arvif->vdev_id);
					continue;
				}
				skb_cb = ATH12K_SKB_CB(msdu_copied);
				goto skip_nwifi;
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
skip_nwifi:
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
				ath12k_mac_ieee80211_free_txskb(hw, msdu_copied,
								tmp_dp_pdev,
								sta, dp_vif,
								DP_TX_ENQ_DROP_INV_PEER,
								ring_id, true);
				continue;
			}

			key = peer->dp_peer->keys[peer->dp_peer->mcast_keyidx];
			if (key) {
				skb_cb->cipher = key->cipher;
				skb_cb->flags |= ATH12K_SKB_CIPHER_SET;

				if (!is_eth) {
					hdr = (struct ieee80211_hdr *)msdu_copied->data;
					if (!ieee80211_has_protected(hdr->frame_control))
						hdr->frame_control |=
						cpu_to_le16(IEEE80211_FCTL_PROTECTED);
				}
			}
			spin_unlock_bh(&tmp_ar->ab->dp->dp_lock);

skip_peer_find:
			err = ath12k_wifi8_dp_tx(tmp_dp_pdev, tmp_arvif,
						 msdu_copied, true, mcbc_gsn,
						 is_mcast, arsta, ring_id,
						 qos_nw_delay);
			if (unlikely(err)) {
				if (ath12k_mac_check_err_code_debug_logging(err))
					ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC,
							 ATH12K_DBG_L2,
							 "failed to transmit frame %d\n",
							 err);
				else
					ath12k_warn(ar->ab, "failed to transmit frame %d\n",
						    err);

				ath12k_mac_ieee80211_free_txskb(hw, msdu_copied,
								tmp_dp_pdev, sta,
								dp_vif, err,
								ring_id, true);
			}
		}
		ieee80211_free_txskb(ar->ah->hw, skb);
	}
}

static void ath12k_wifi8_mac_op_sta_set_4addr(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_sta *sta,
					      bool enabled)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_vlan_iface *vlan_iface = ahvif->vlan_iface;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);

	if (enabled && !ahsta->use_4addr_set) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		ahsta->vlan_iface = ahvif->vlan_iface;
#endif
		wiphy_work_queue(hw->wiphy, &ahsta->set_4addr_wk);
		ahsta->use_4addr_set = true;
		if (vif->type == NL80211_IFTYPE_AP_VLAN) {
			if (vlan_iface)
				vlan_iface->is_wds_4addr = true;

			ath12k_wifi8_dp_vif_update_4addr(&ah->dp_hw, &ahvif->dp_vif,
							 sta->addr);
		}
	}
}

static const struct ieee80211_ops ath12k_ops_wifi8 = {
	.tx				= ath12k_wifi8_mac_op_tx,
	.wake_tx_queue			= ieee80211_handle_wake_tx_queue,
	.start                          = ath12k_mac_op_start,
	.stop                           = ath12k_mac_op_stop,
	.reconfig_complete              = ath12k_mac_op_reconfig_complete,
	.add_interface                  = ath12k_mac_op_add_interface,
	.remove_interface		= ath12k_mac_op_remove_interface,
	.update_vif_offload		= ath12k_mac_op_update_vif_offload,
	.config                         = ath12k_mac_op_config,
	.sta_set_4addr			= ath12k_wifi8_mac_op_sta_set_4addr,
	.link_info_changed              = ath12k_mac_op_link_info_changed,
	.start_ap                       = ath12k_mac_op_start_ap,
	.link_going_down                = ath12k_mac_op_link_going_down,
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
	.set_dscp_tid                   = ath12k_mac_op_set_dscp_tid,
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
	.sta_add_debugfs                = ath12k_debugfs_sta_op_add,
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
	.set_radar_background           = ath12k_mac_op_set_radar_background,
	.erp                            = ath12k_mac_op_erp,
	.qos_mgmt_cfg                   = ath12k_mac_op_qos_mgmt_cfg,
	.get_afc_eirp_pwr               = ath12k_mac_op_get_afc_eirp_pwr,
	.get_6ghz_dev_deployment_type	= ath12k_mac_op_get_6ghz_dev_deployment_type,
	.ap_power_save                  = ath12k_mac_op_ap_power_save,
};

int ath12k_wifi8_hw_init(struct ath12k_base *ab)
{
	struct ath12k_hw_params *hw_params = NULL;
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

	ab->hw_params = hw_params;
	ab->ath12k_ops = &ath12k_ops_wifi8;
	ab->map_event_required = false;

	ath12k_info(ab, "WiFi8 Hardware name: %s\n", ab->hw_params->name);

	return 0;
}
