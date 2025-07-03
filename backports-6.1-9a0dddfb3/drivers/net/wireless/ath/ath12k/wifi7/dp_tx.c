// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_tx.h"
#include "../peer.h"
#include "dp_tx.h"
#include "hal_rx.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"

struct ath12k_tx_sw_metadata {
	struct sk_buff *skb;
	struct sk_buff *skb_ext_desc;
	u64 paddr : 40,
	    len   : 16,
	    mac_id: 5,
	    flags : 1,
	    rsvd  : 2;
	u64 paddr_ext_desc : 40,
	    ext_desc_len   : 16,
	    rsvd2          : 8;
} __packed __aligned(32);

static_assert(sizeof(struct ath12k_tx_sw_metadata) == 32, "size of struct ath12k_tx_sw_metadata is not 32 bytes!");

struct ath12k_wifi7_tx_status_entry {
	struct hal_wbm_completion_ring_tx tx_status;
	struct ath12k_tx_sw_metadata sw_metadata;
} __packed;

static_assert(sizeof(struct ath12k_wifi7_tx_status_entry) == 64, "size of struct ath12k_wifi7_tx_status_entry is not 64 bytes!");

static enum hal_tcl_encap_type
ath12k_dp_tx_get_encap_type(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		return HAL_TCL_ENCAP_TYPE_RAW;

	if (tx_info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
		return HAL_TCL_ENCAP_TYPE_ETHERNET;

	return HAL_TCL_ENCAP_TYPE_NATIVE_WIFI;
}

static void
ath12k_wifi7_hal_tx_cmd_ext_desc_setup(struct ath12k_base *ab,
				       struct hal_tx_msdu_ext_desc *tcl_ext_cmd,
				       struct hal_tx_info *ti)
{
	tcl_ext_cmd->info0 = le32_encode_bits(ti->paddr,
					      HAL_TX_MSDU_EXT_INFO0_BUF_PTR_LO);
	tcl_ext_cmd->info1 = le32_encode_bits(0x0,
					      HAL_TX_MSDU_EXT_INFO1_BUF_PTR_HI) |
			       le32_encode_bits(ti->data_len,
						HAL_TX_MSDU_EXT_INFO1_BUF_LEN);

	tcl_ext_cmd->info1 |= le32_encode_bits(1, HAL_TX_MSDU_EXT_INFO1_EXTN_OVERRIDE) |
				le32_encode_bits(ti->encap_type,
						 HAL_TX_MSDU_EXT_INFO1_ENCAP_TYPE) |
				le32_encode_bits(ti->encrypt_type,
						 HAL_TX_MSDU_EXT_INFO1_ENCRYPT_TYPE);
}

static inline u32 ath12k_qos_get_metadata(u16 qos_id)
{
	u32 tcl_metadata = 0;

	tcl_metadata = u32_encode_bits(HTT_TCL_META_DATA_TYPE_SVC_ID_BASED,
				       HTT_TCL_META_DATA_TYPE_MISSION) |
			u32_encode_bits(1, HTT_TCL_META_DATA_SAWF_TID_OVERRIDE) |
			u32_encode_bits(qos_id, HTT_TCL_META_DATA_SAWF_SVC_ID);
	return tcl_metadata;
}

static inline u32 ath12k_qos_get_tcl_cmd(u32 msduq)
{
	u32 tid, flow_override, who_classify_info_sel, update = 0;

	tid = u32_get_bits(msduq, MSDUQ_TID);
	flow_override = u32_get_bits(msduq, MSDUQ_FLOW_OVERRIDE);
	who_classify_info_sel = u32_get_bits(msduq, MSDUQ_WHO_CL_INFO);

	update = u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE_EN) |
		 u32_encode_bits(who_classify_info_sel,
				 HAL_TCL_DATA_CMD_INFO3_CLASSIFY_INFO_SEL) |
		 u32_encode_bits(flow_override,
				 HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE);
	return update;
}

static inline
void ath12k_wifi_qos_desc(struct hal_tcl_data_cmd *desc,
			  u32 msduq, u16 qos_id)
{
	u32 meta_data_flags;

	desc->info3 |= ath12k_qos_get_tcl_cmd(msduq);
	meta_data_flags = ath12k_qos_get_metadata(qos_id);
	desc->info1 = u32_encode_bits(meta_data_flags,
				      HAL_TCL_DATA_CMD_INFO1_CMD_NUM);
}

static inline
void ath12k_wifi_qos_hlos_tid(struct hal_tcl_data_cmd *desc,
			      u8 tid)
{
	desc->info3 |= u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE);
}

static inline u8 ath12k_get_qos_tag(u32 mark)
{
	u8 qos_tag = u32_get_bits(mark, QOS_TAG_MASK);

	if (qos_tag == QOS_SCS_TAG || qos_tag == QOS_MSCS_TAG)
		return qos_tag;

	return 0;
}

static inline void
ath12k_dp_qos_update(struct ath12k_dp *dp, u32 mark,
		     struct hal_tcl_data_cmd *desc, u8 qos_tag,
		     u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	u8 scs_id;
	u16 msduq, qos_id;
	int ret;

	if (qos_tag == QOS_SCS_TAG) {
		scs_id = u32_get_bits(mark, QOS_QOS_ID_MASK);

		spin_lock_bh(&dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
		if (!peer) {
			spin_unlock_bh(&dp->dp_lock);
			return;
		}
		ret = ath12k_dp_peer_scs_data(dp, peer->dp_peer->qos,
					      scs_id, &msduq, &qos_id);
		spin_unlock_bh(&dp->dp_lock);

		if (ret != 0) {
			ath12k_err(dp->ab, "SCS Peer Data is NULL");
			return;
		}
	} else {
		msduq = u32_get_bits(mark, QOS_QOS_ID_MASK);
	}
	/* Update Desc for HLOS TID Override */
	if (msduq < MSDUQ_MAX_DEF) {
		ath12k_wifi_qos_hlos_tid(desc, msduq);
	} else if (msduq < QOS_MSDUQ_MAX){
	/* Update Desc for User Defined QoS MSDUQ */
			ath12k_wifi_qos_desc(desc, msduq, qos_id);
	}
}

#define HTT_META_DATA_ALIGNMENT 0x8

/* Preparing HTT Metadata when utilized with ext MSDU */
static int ath12k_wifi7_dp_prepare_htt_metadata(struct sk_buff *skb)
{
	struct hal_tx_msdu_metadata *desc_ext;
	u8 htt_desc_size;
	/* Size rounded of multiple of 8 bytes */
	u8 htt_desc_size_aligned;

	htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
	htt_desc_size_aligned = ALIGN(htt_desc_size, HTT_META_DATA_ALIGNMENT);

	desc_ext = ath12k_dp_metadata_align_skb(skb, htt_desc_size_aligned);
	if (!desc_ext)
		return -ENOMEM;

	desc_ext->info0 = le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			  le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE) |
			  le32_encode_bits(1,
					   HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);

	return 0;
}

bool ath12k_mac_tx_check_max_limit(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *skb)
{
	if (atomic_read(&dp_pdev->num_tx_pending) > ATH12K_DP_PDEV_TX_LIMIT) {
		/* Allow EAPOL */
		if (!(skb->protocol == cpu_to_be16(ETH_P_PAE))) {
			dp_pdev->dp->device_stats.tx_err.threshold_limit++;
			return true;
		}
	}

	return false;
}

static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end) {
#ifndef CONFIG_IO_COHERENCY
        dmac_clean_range_no_dsb(start, end);
#endif
}

/* TODO: Remoe the export once this file is built with wifi7 ko */
int ath12k_wifi7_dp_tx(struct ath12k_pdev_dp *dp_pdev,
		       struct ath12k_link_vif *arvif,
		       struct sk_buff *skb, bool gsn_valid, int mcbc_gsn,
		       bool is_mcast, struct ath12k_link_sta *arsta)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct ath12k_base *ab = dp->ab;
	struct hal_tx_info ti = {0};
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tx_msdu_ext_desc *msg;
	struct sk_buff *skb_ext_desc = NULL;
	struct ethhdr *eth = NULL;
	struct hal_srng *tcl_ring;
	struct ieee80211_hdr *hdr = (void *)skb->data;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[arvif->link_id];
	struct dp_tx_ring *tx_ring;
	struct ath12k_sta *ahsta;
	u8 pool_id;
	u8 hal_ring_id;
	int ret;
	u16 peer_id;
	u8 ring_selector, ring_map = 0;
	bool tcl_ring_retry;
	bool msdu_ext_desc = false;
	bool add_htt_metadata = false;
	u32 iova_mask = dp->hw_params->iova_mask;
	bool is_diff_encap = false, is_null = false;
	bool is_from_recycler;
	u8 qos_tag;
	bool stats_disable = ab->stats_disable;
	struct hal_tcl_data_cmd tcl_desc = {0};
	u8 ring_id;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return -ESHUTDOWN;

	if (likely(skb->fast_xmit)) {
		pool_id = skb_get_queue_mapping(skb) & (ATH12K_HW_MAX_QUEUES - 1);
		ring_selector = smp_processor_id();
		ring_id = ring_selector % dp->hw_params->max_tx_ring;

		tx_desc = ath12k_dp_tx_assign_buffer(dp, ring_id);
		if (unlikely(!tx_desc)) {
			dp->device_stats.tx_err.txbuf_na[ring_id]++;
			return -ENOSPC;
		}

		ath12k_core_dma_clean_range_no_dsb(skb->data, skb->data + DP_TX_SFE_BUFFER_SIZE);

		/* the edma driver uses this flags to optimize the cache invalidation */
		is_from_recycler = (skb->fast_recycled = !!skb->is_from_recycler);
		if (likely(is_from_recycler))
			tx_desc->flags = (DP_TX_DESC_FLAG_FAST & stats_disable);
		else
			tx_desc->flags = 0;

		tx_desc->skb = skb;
		tx_desc->mac_id = dp_link_vif->pdev_idx;

		tcl_desc.buf_addr_info.info0 = (u32)virt_to_phys(skb->data);
		tcl_desc.buf_addr_info.info1 =
			(((u64)virt_to_phys(skb->data) >> 32) | (tx_desc->desc_id << 12));
		tcl_desc.info0 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
					    dp_link_vif->bank_id);
		tcl_desc.info1 =  FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CMD_NUM,
					     dp_link_vif->tcl_metadata);
		tcl_desc.info2 =  skb->len;

		tcl_desc.info2 |= TX_IP_CHECKSUM;
		tcl_desc.info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_PMAC_ID, dp_link_vif->lmac_id) |
				 FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_VDEV_ID, dp_link_vif->vdev_id);
		tcl_desc.info4 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX, dp_link_vif->ast_idx) |
				 FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM, dp_link_vif->ast_hash);
		tcl_desc.info5 = 0;

		tx_ring = &dp->tx_ring[ring_id];
		hal_ring_id = tx_ring->tcl_data_ring.ring_id;
		tcl_ring = &hal->srng_list[hal_ring_id];

		spin_lock_bh(&tcl_ring->lock);

		ath12k_hal_srng_access_begin(ab, tcl_ring);
		hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(ab, tcl_ring);
		if (unlikely(!hal_tcl_desc)) {
			/* NOTE: It is highly unlikely we'll be running out of tcl_ring
			 * desc because the desc is directly enqueued onto hw queue.
			 */
			ath12k_hal_srng_access_end(ab, tcl_ring);
			dp->device_stats.tx_err.desc_na[ring_id]++;
			spin_unlock_bh(&tcl_ring->lock);
			ret = -ENOMEM;
			goto fail_remove_tx_buf;
		}

		memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
#ifndef CONFIG_IO_COHERENCY
		dmb(oshst);
#endif
		ath12k_hal_srng_access_end(ab, tcl_ring);

		dp->device_stats.tx_fast_unicast[ring_id]++;
		spin_unlock_bh(&tcl_ring->lock);

		atomic_inc(&dp_pdev->num_tx_pending);

		return 0;
	}

	if (!(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !ieee80211_is_data(hdr->frame_control))
		return -EOPNOTSUPP;

	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP)
		eth = (struct ethhdr *)skb->data;

	if (eth && is_multicast_ether_addr(eth->h_dest) && arsta) {
		ti.meta_data_flags = arsta->tcl_metadata;
		peer_id = u16_get_bits(ti.meta_data_flags, HTT_TCL_META_DATA_PEER_ID_MISSION);
		ti.bss_ast_hash = arsta->ast_hash;
		ti.bss_ast_idx = peer_id;
		ti.lookup_override = true;
	} else if (ieee80211_has_a4(hdr->frame_control) &&
	    is_multicast_ether_addr(hdr->addr3) && arsta) {
		ahsta = arsta->ahsta;
		if (unlikely(!ahsta->link[ahsta->primary_link_id])) {
			ath12k_err(ab, "arsta not found on primary link");
			ret = -EINVAL;
			goto fail_remove_tx_buf;
		}
		ti.meta_data_flags = ahsta->link[ahsta->primary_link_id]->tcl_metadata;
		ti.flags0 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TO_FW, 1);
	} else {
		ti.meta_data_flags = dp_link_vif->tcl_metadata;
	}
	pool_id = skb_get_queue_mapping(skb) & (ATH12K_HW_MAX_QUEUES - 1);

	/* Let the default ring selection be based on current processor
	 * number, where one of the 3 tcl rings are selected based on
	 * the smp_processor_id(). In case that ring
	 * is full/busy, we resort to other available rings.
	 * If all rings are full, we drop the packet.
	 * TODO: Add throttling logic when all rings are full
	 */
	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);

tcl_ring_sel:
	tcl_ring_retry = false;
	ti.ring_id = ring_selector % dp->hw_params->max_tx_ring;

	ring_map |= BIT(ti.ring_id);
	ti.rbm_id = hal->tcl_to_wbm_rbm_map[ti.ring_id].rbm_id;

	tx_ring = &dp->tx_ring[ti.ring_id];

	tx_desc = ath12k_dp_tx_assign_buffer(dp, ti.ring_id);
	if (!tx_desc) {
		dp->device_stats.tx_err.txbuf_na[ti.ring_id]++;
		return -ENOMEM;
	}

	ti.bank_id = dp_link_vif->bank_id;

	if (gsn_valid && !(ti.lookup_override)) {
		/* Reset and Initialize meta_data_flags with Global Sequence
		 * Number (GSN) info.
		 */
		ti.meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					HTT_TCL_META_DATA_TYPE) |
			u32_encode_bits(mcbc_gsn, HTT_TCL_META_DATA_GLOBAL_SEQ_NUM);

		if (arvif->nawds_support)
			ti.meta_data_flags |= u32_encode_bits(1, HTT_TCL_META_DATA_GLOBAL_SEQ_HOST_INSPECTED);
	}

	ti.encap_type = ath12k_dp_tx_get_encap_type(ab, skb);
	ti.addr_search_flags = dp_link_vif->hal_addr_search_flags;
	ti.search_type = dp_link_vif->search_type;
	ti.type = HAL_TCL_DESC_TYPE_BUFFER;
	ti.pkt_offset = 0;
	ti.lmac_id = dp_link_vif->lmac_id;

	ti.vdev_id = dp_link_vif->vdev_id;
	if (gsn_valid)
		ti.vdev_id += HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
	else if (arvif->nawds_support && is_mcast && !ti.lookup_override)
		ti.meta_data_flags |= u32_encode_bits(1, HTT_TCL_META_DATA_HOST_INSPECTED_MISSION);

	if (!(ti.lookup_override)) {
		ti.bss_ast_hash = dp_link_vif->ast_hash;
		ti.bss_ast_idx = dp_link_vif->ast_idx;
	}
	ti.dscp_tid_tbl_idx = 0;

	if (skb->ip_summed == CHECKSUM_PARTIAL &&
	    ti.encap_type != HAL_TCL_ENCAP_TYPE_RAW) {
		ti.flags0 |= u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_IP4_CKSUM_EN) |
			     u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_UDP4_CKSUM_EN) |
			     u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_UDP6_CKSUM_EN) |
			     u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_TCP4_CKSUM_EN) |
			     u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_TCP6_CKSUM_EN);
	}

	switch (ti.encap_type) {
	case HAL_TCL_ENCAP_TYPE_NATIVE_WIFI:
		is_null = ieee80211_is_nullfunc(hdr->frame_control);
		if ((ahvif->vif->offload_flags & IEEE80211_OFFLOAD_ENCAP_ENABLED) &&
		    (skb->protocol == cpu_to_be16(ETH_P_PAE) || is_null))
			is_diff_encap = true;
		else
			ath12k_dp_tx_encap_nwifi(skb);
		break;
	case HAL_TCL_ENCAP_TYPE_RAW:
		if (!test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags)) {
			ret = -EINVAL;
			goto fail_remove_tx_buf;
		}
		break;
	case HAL_TCL_ENCAP_TYPE_ETHERNET:
		/* no need to encap */
		break;
	case HAL_TCL_ENCAP_TYPE_802_3:
	default:
		/* TODO: Take care of other encap modes as well */
		ret = -EINVAL;
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		goto fail_remove_tx_buf;
	}

	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET &&
		     !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP))) {
		msdu_ext_desc = true;
		if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
			ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		}
	}

	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW)) {
		if (skb->protocol == cpu_to_be16(ETH_P_ARP)) {
			ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
			msdu_ext_desc = true;
		}

		if (skb_cb->flags & ATH12K_SKB_CIPHER_SET) {
			ti.encrypt_type =
				ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

			if (ieee80211_has_protected(hdr->frame_control))
				skb_put(skb, IEEE80211_CCMP_MIC_LEN);
		} else {
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		}
	}

	if (iova_mask &&
	    (unsigned long)skb->data & iova_mask) {
		ret = ath12k_dp_tx_align_payload(dp, &skb);
		if (ret) {
			ath12k_warn(ab, "failed to align TX buffer %d\n", ret);
			/* don't bail out, give original buffer
			 * a chance even unaligned.
			 */
			goto map;
		}

		/* hdr is pointing to a wrong place after alignment,
		 * so refresh it for later use.
		 */
		hdr = (void *)skb->data;
	}
map:
#ifndef CONFIG_IO_COHERENCY
	ti.paddr = dma_map_single(dp->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(dp->dev, ti.paddr)) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		ath12k_warn(ab, "failed to DMA map data Tx buffer\n");
		ret = -ENOMEM;
		goto fail_remove_tx_buf;
	}
#else
	ti.paddr = virt_to_phys(skb->data);
	if (!ti.paddr) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		ath12k_warn(ab, "failed to DMA map data Tx buffer\n");
		ret = -ENOMEM;
		goto fail_remove_tx_buf;
	}
#endif

	if ((!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	     !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	     !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	     ieee80211_has_protected(hdr->frame_control)) ||
	     is_diff_encap) {
		if (is_null && msdu_ext_desc)
			goto skip_htt_metadata;
		/* Add metadata for sw encrypted vlan group traffic */
		add_htt_metadata = true;
		msdu_ext_desc = true;
		ti.meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
skip_htt_metadata:
		ti.flags0 |= u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_TO_FW);
		ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	tx_desc->skb = skb;
	tx_desc->mac_id = dp_link_vif->pdev_idx;
	ti.desc_id = tx_desc->desc_id;
	ti.data_len = skb->len;

	tx_desc->paddr = ti.paddr;
	tx_desc->len = ti.data_len;

	tx_desc->paddr_ext_desc = 0;

	if (msdu_ext_desc) {
		skb_ext_desc = dev_alloc_skb(sizeof(struct hal_tx_msdu_ext_desc));
		if (!skb_ext_desc) {
			ret = -ENOMEM;
			goto fail_unmap_dma;
		}

		skb_put(skb_ext_desc, sizeof(struct hal_tx_msdu_ext_desc));
		memset(skb_ext_desc->data, 0, skb_ext_desc->len);

		msg = (struct hal_tx_msdu_ext_desc *)skb_ext_desc->data;
		ath12k_wifi7_hal_tx_cmd_ext_desc_setup(ab, msg, &ti);

		if (add_htt_metadata) {
			ret = ath12k_wifi7_dp_prepare_htt_metadata(skb_ext_desc);
			if (ret < 0) {
				ath12k_dbg(ab, ATH12K_DBG_DP_TX,
					   "Failed to add HTT meta data, dropping packet\n");
				goto fail_free_ext_skb;
			}
		}
#ifndef CONFIG_IO_COHERENCY
		ti.paddr = dma_map_single(dp->dev, skb_ext_desc->data,
					  skb_ext_desc->len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dp->dev, ti.paddr);
		if (ret)
			goto fail_free_ext_skb;
#else
		ti.paddr = virt_to_phys(skb_ext_desc->data);
		if (!ti.paddr)
			goto fail_free_ext_skb;
#endif
		ti.data_len = skb_ext_desc->len;
		ti.type = HAL_TCL_DESC_TYPE_EXT_DESC;

		tx_desc->paddr_ext_desc = ti.paddr;
		tx_desc->ext_desc_len = ti.data_len;
		tx_desc->skb_ext_desc = skb_ext_desc;
	}

	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	spin_lock_bh(&tcl_ring->lock);

	ath12k_hal_srng_access_begin(ab, tcl_ring);

	hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(ab, tcl_ring);
	if (!hal_tcl_desc) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath12k_hal_srng_access_end(ab, tcl_ring);
		dp->device_stats.tx_err.desc_na[ti.ring_id]++;
		spin_unlock_bh(&tcl_ring->lock);
		ret = -ENOMEM;

		/* Checking for available tcl descriptors in another ring in
		 * case of failure due to full tcl ring now, is better than
		 * checking this ring earlier for each pkt tx.
		 * Restart ring selection if some rings are not checked yet.
		 */
		if (ring_map != (BIT(dp->hw_params->max_tx_ring) - 1) &&
		    dp->hw_params->tcl_ring_retry) {
			tcl_ring_retry = true;
			ring_selector++;
		}

		goto fail_unmap_dma_ext;
	}

	spin_lock_bh(&arvif->link_stats_lock);
	if (is_mcast)
		ab->dp->device_stats.tx_mcast[ti.ring_id]++;
	else if (skb->protocol == cpu_to_be16(ETH_P_PAE))
		ab->dp->device_stats.tx_eapol[ti.ring_id]++;
	else if (is_null)
		ab->dp->device_stats.tx_null_frame[ti.ring_id]++;
	else
		ab->dp->device_stats.tx_unicast[ti.ring_id]++;

	arvif->link_stats.tx_encap_type[ti.encap_type]++;
	arvif->link_stats.tx_encrypt_type[ti.encrypt_type]++;
	arvif->link_stats.tx_desc_type[ti.type]++;

	if (is_mcast)
		arvif->link_stats.tx_bcast_mcast++;
	else
		arvif->link_stats.tx_enqueued++;
	spin_unlock_bh(&arvif->link_stats_lock);

	ath12k_wifi7_hal_tx_cmd_desc_setup(ab, hal_tcl_desc, &ti);

	if (unlikely(arsta)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_dp_qos_update(dp, skb->mark, hal_tcl_desc,
					     qos_tag, arsta->addr);
	}

	ath12k_hal_srng_access_end(ab, tcl_ring);

	spin_unlock_bh(&tcl_ring->lock);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_TX, NULL, "dp tx msdu: ",
			skb->data, skb->len);

	atomic_inc(&dp_pdev->num_tx_pending);

	return 0;

fail_unmap_dma_ext:
	if (tx_desc->paddr_ext_desc)
		ath12k_core_dma_unmap_single(dp->dev, tx_desc->paddr_ext_desc,
					     tx_desc->ext_desc_len,
					     DMA_TO_DEVICE);
fail_free_ext_skb:
	if (skb_ext_desc)
		kfree_skb(skb_ext_desc);

fail_unmap_dma:
	ath12k_core_dma_unmap_single(dp->dev, ti.paddr, ti.data_len, DMA_TO_DEVICE);

fail_remove_tx_buf:
	if (tx_desc)
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);

	spin_lock_bh(&arvif->link_stats_lock);
	arvif->link_stats.tx_dropped++;
	spin_unlock_bh(&arvif->link_stats_lock);

	if (tcl_ring_retry)
		goto tcl_ring_sel;

	return ret;
}

static void ath12k_wifi7_dp_tx_free_txbuf(struct ath12k_dp *dp,
					  struct sk_buff *msdu,
					  struct dp_tx_ring *tx_ring,
					  struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	u8 pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
		wake_up(&dp_pdev->tx_empty_waitq);

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_htt_tx_complete_buf(struct ath12k_dp *dp,
				       struct sk_buff *msdu,
				       struct dp_tx_ring *tx_ring,
				       struct ath12k_dp_htt_wbm_tx_status *ts,
				       struct ath12k_tx_sw_metadata *sw_metadata,
				       u16 peer_id)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_tx_info *info;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	struct ath12k_pdev_dp *dp_pdev;
	u8 pdev_id;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
		wake_up(&dp_pdev->tx_empty_waitq);

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	skb_cb = ATH12K_SKB_CB(msdu);
	info = IEEE80211_SKB_CB(msdu);

	vif = skb_cb->vif;
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (arvif) {
			spin_lock_bh(&arvif->link_stats_lock);
			arvif->link_stats.tx_completed++;
			spin_unlock_bh(&arvif->link_stats_lock);
		}
	}

	memset(&info->status, 0, sizeof(info->status));

	if (ts->acked) {
		if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = dp_pdev->ar->rssi_offsets.rssi_offset +
						  ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		} else {
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
		}
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer || !peer->sta)
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n", peer_id);
	else
		status.sta = peer->sta;
	spin_unlock_bh(&dp->dp_lock);

	status.info = info;
	status.skb = msdu;
	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_process_htt_tx_complete(struct ath12k_dp *dp,
					   void *desc, struct sk_buff *msdu,
					   struct dp_tx_ring *tx_ring,
					   struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct htt_tx_wbm_completion *status_desc;
	struct ath12k_dp_htt_wbm_tx_status ts = {0};
	int htt_status;
	u16 peer_id;

	status_desc = desc;

	htt_status = le32_get_bits(status_desc->info0,
				   HTT_TX_WBM_COMP_INFO0_STATUS);
	dp->device_stats.fw_tx_status[htt_status]++;

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		struct ath12k_pdev_dp *dp_pdev;
		u8 pdev_id;

		pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

		rcu_read_lock();
		dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
		if (!dp_pdev) {
			rcu_read_unlock();
			return;
		}

		if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
			wake_up(&dp_pdev->tx_empty_waitq);

		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	switch (htt_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
		ts.acked = (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts.ack_rssi = le32_get_bits(status_desc->info2,
					    HTT_TX_WBM_COMP_INFO2_ACK_RSSI);
		peer_id = le32_get_bits(((struct hal_wbm_completion_ring_tx *)desc)->
				info3, HAL_WBM_COMPL_TX_INFO3_PEER_ID);

		ath12k_wifi7_dp_tx_htt_tx_complete_buf(dp, msdu, tx_ring, &ts,
						       sw_metadata, peer_id);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH:
		ath12k_wifi7_dp_tx_free_txbuf(dp, msdu, tx_ring, sw_metadata);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY:
		/* This event is to be handled only when the driver decides to
		 * use WDS offload functionality.
		 */
		break;
	default:
		ath12k_warn(dp->ab, "Unknown htt tx status %d\n", htt_status);
		break;
	}

}

static void 
ath12k_wifi7_dp_tx_cache_peer_stats(struct ath12k *ar,
					  struct sk_buff *msdu,
					  struct hal_tx_status *ts)
{
	struct ath12k_per_peer_tx_stats *peer_stats = &ar->cached_stats;

	if (ts->try_cnt > 1) {
		peer_stats->retry_pkts += ts->try_cnt - 1;
		peer_stats->retry_bytes += (ts->try_cnt - 1) * msdu->len;

		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
			peer_stats->failed_pkts += 1;
			peer_stats->failed_bytes += msdu->len;
		}
	}
}

static void
ath12k_wifi7_dp_tx_update_txcompl(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	struct rate_info txrate = {0};
	u16 rate, ru_tones;
	u8 rate_idx = 0;
	int ret;

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, ts->peer_id);
	if (!peer || !peer->sta) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	sta = peer->sta;
	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = &ahsta->deflink;

	/* This is to prefer choose the real NSS value arsta->last_txrate.nss,
	 * if it is invalid, then choose the NSS value while assoc.
	 */
	if (peer->last_txrate.nss)
		txrate.nss = peer->last_txrate.nss;
	else
		txrate.nss = arsta->peer_nss;
	spin_unlock_bh(&dp->dp_lock);

	WARN_ON_ONCE(txrate.nss < 1 ||
             (txrate.nss > hweight32(dp_pdev->ar->pdev->cap.tx_chain_mask)));

	switch (ts->pkt_type) {
	case HAL_TX_RATE_STATS_PKT_TYPE_11A:
	case HAL_TX_RATE_STATS_PKT_TYPE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(ts->mcs,
							    ts->pkt_type,
							    &rate_idx,
							    &rate);
		if (ret < 0) {
			ath12k_warn(ab, "Invalid tx legacy rate %d\n", ret);
			return;
		}

		txrate.legacy = rate;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11N:
		if (ts->mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ab, "Invalid HT mcs index %d\n", ts->mcs);
			return;
		}

		if (txrate.nss != 0)
			txrate.mcs = ts->mcs + 8 * (txrate.nss - 1);

		txrate.flags = RATE_INFO_FLAGS_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AC:
		if (ts->mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid VHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_VHT_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AX:
		if (ts->mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ab, "Invalid HE mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		txrate.he_gi = ath12k_he_gi_to_nl80211_he_gi(ts->sgi);
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11BE:
		if (ts->mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid EHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		txrate.eht_gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(ts->sgi);
		break;
	default:
		ath12k_warn(ab, "Invalid tx pkt type: %d\n", ts->pkt_type);
		return;
	}

	txrate.bw = ath12k_mac_bw_to_mac80211_bw(ts->bw);

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		txrate.bw = RATE_INFO_BW_HE_RU;
		ru_tones = ath12k_mac_he_convert_tones_to_ru_tones(ts->tones);
		txrate.he_ru_alloc =
			ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	}

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE) {
		txrate.bw = RATE_INFO_BW_EHT_RU;
		txrate.eht_ru_alloc =
			ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(ts->tones);
	}

	spin_lock_bh(&dp->dp_lock);
	peer->txrate = txrate;
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_wifi7_dp_tx_complete_msdu(struct ath12k_pdev_dp *dp_pdev,
					     struct sk_buff *msdu,
					     struct hal_tx_status *ts,
					     struct ath12k_tx_sw_metadata *sw_metadata,
					     u8 mac_id, int ring)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_rate_status status_rate = { 0 };
	struct rate_info rate;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_tx_info *info;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *peer;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	struct ath12k *ar;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	if (sw_metadata->skb)
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		return;

	dp_pdev->wmm_stats.tx_type = ath12k_tid_to_ac(ts->tid > ATH12K_DSCP_PRIORITY ? 0:ts->tid);
	if (dp_pdev->wmm_stats.tx_type) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
			dp_pdev->wmm_stats.total_wmm_tx_drop[dp_pdev->wmm_stats.tx_type]++;
	}

	skb_cb = ATH12K_SKB_CB(msdu);

	rcu_read_lock();

	if (!rcu_dereference(ab->pdevs_active[dp_pdev->mac_id])) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		goto exit;
	}

	if (!skb_cb->vif) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		goto exit;
	}

	vif = skb_cb->vif;
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (arvif) {
			spin_lock_bh(&arvif->link_stats_lock);
			arvif->link_stats.tx_completed++;
			spin_unlock_bh(&arvif->link_stats_lock);
		}
	}

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	/* skip tx rate update from ieee80211_status*/
	info->status.rates[0].idx = -1;

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
			!(info->flags & IEEE80211_TX_CTL_NO_ACK))	{
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = dp_pdev->ar->rssi_offsets.rssi_offset +
					  ts->ack_rssi;

		if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
			      ab->wmi_ab.svc_map))
			info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

		info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
			(info->flags & IEEE80211_TX_CTL_NO_ACK))
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts->status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			dev_kfree_skb_any(msdu);
			goto exit;
		default:
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX, "tx frame is not acked status %d\n", ts->status);
 		}
	}

	/* NOTE: Tx rate status reporting. Tx completion status does not have
	 * necessary information (for example nss) to build the tx rate.
	 * Might end up reporting it out-of-band from HTT stats.
	 */

	ar = dp_pdev->ar;
	if (ath12k_debugfs_is_extd_tx_stats_enabled(ar)) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
						sizeof(struct ath12k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0, 
						sizeof(struct ath12k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath12k_wifi7_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

       	ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, ts->peer_id);
	if (!peer || !peer->sta) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		goto exit;
	}

	status.sta = peer->sta;
	status.info = info;
	status.skb = msdu;
	rate = peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;
	spin_unlock_bh(&dp->dp_lock);

	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);

exit:
	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_status_parse(struct ath12k_base *ab,
				struct hal_wbm_completion_ring_tx *desc,
				struct hal_tx_status *ts)
{
	u32 info0 = le32_to_cpu(desc->rate_stats.info0);

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
	    ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)
		return;

	ts->ppdu_id = le32_get_bits(desc->info1,
				    HAL_WBM_COMPL_TX_INFO1_TQM_STATUS_NUMBER);

	ts->ack_rssi = FIELD_GET(HAL_WBM_COMPL_TX_INFO2_ACK_FRAME_RSSI,
				 desc->info2);

	ts->peer_id = le32_get_bits(desc->info3, HAL_WBM_COMPL_TX_INFO3_PEER_ID);

	if (info0 & HAL_TX_RATE_STATS_INFO0_VALID) {
		ts->pkt_type = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_PKT_TYPE);
		ts->mcs = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_MCS);
		ts->sgi = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_SGI);
		ts->bw = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_BW);
		ts->tones = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_TONES_IN_RU);
		ts->ofdma = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_OFDMA_TX);
	}

	ts->tid = FIELD_GET(HAL_WBM_RELEASE_TX_INFO3_TID, desc->info3);
}

int ath12k_wifi7_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	struct hal_wbm_release_ring *desc;
	u8 pdev_id;
	u64 desc_va;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int orig_budget = budget;
	bool fast_flag;

	ath12k_hal_srng_access_begin(ab, status_ring);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = ath12k_hal_srng_dst_num_free(ab, status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_end(ab, status_ring);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_hal_srng_dst_invalidate_entry(dp, status_ring, valid_entries);
#endif

	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_wifi7_tx_status_entry *tx_status_entry;
	struct ath12k_tx_sw_metadata *sw_metadata;
	u8 n_entry = 0;
	struct list_head desc_free_list, *cur;
	struct hal_wbm_completion_ring_tx *tx_status;
	struct sk_buff_head free_list_head;

	INIT_LIST_HEAD(&desc_free_list);
	skb_queue_head_init(&free_list_head);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[ring_id];
	while (budget-- && (desc = ath12k_hal_srng_dst_get_next_cached_entry(ab, status_ring, NULL))) {
		tx_status = (struct hal_wbm_completion_ring_tx *)desc;

		if (le32_get_bits(tx_status->info0, HAL_WBM_COMPL_TX_INFO0_CC_DONE)) {
			/* HW done cookie conversion */
			desc_va = ((u64)le32_to_cpu(tx_status->buf_va_hi) << 32 |
				   le32_to_cpu(tx_status->buf_va_lo));
			tx_desc = (struct ath12k_tx_desc_info *)((unsigned long)desc_va);
		} else {
			u32 desc_id;

			/* SW does cookie conversion to VA */
			desc_id = le32_get_bits(tx_status->buf_va_hi,
						BUFFER_ADDR_INFO1_SW_COOKIE);

			tx_desc = ath12k_dp_get_tx_desc(dp, desc_id);
		}
		if (!tx_desc) {
			ath12k_warn(ab, "unable to retrieve tx_desc!");
			continue;
		}

		if (!tx_desc->in_use)
			continue;

		memcpy(&tx_status_entry->tx_status, tx_status, sizeof(*tx_status));

		list_add_tail(&tx_desc->list, &desc_free_list);

		n_entry++;
		tx_status_entry++;
	}

	ath12k_hal_srng_access_end(ab, status_ring);

	if (!n_entry)
		return orig_budget - budget;

	spin_lock_bh(&dp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[ring_id];
	list_for_each(cur, &desc_free_list) {
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		tx_desc = list_entry(cur, struct ath12k_tx_desc_info, list);

		sw_metadata->skb = tx_desc->skb;
		sw_metadata->paddr = tx_desc->paddr;
		sw_metadata->len = tx_desc->len;
		sw_metadata->skb_ext_desc = tx_desc->skb_ext_desc;
		sw_metadata->paddr_ext_desc = tx_desc->paddr_ext_desc;
		sw_metadata->ext_desc_len = tx_desc->ext_desc_len;
		sw_metadata->flags = tx_desc->flags;
		sw_metadata->mac_id = tx_desc->mac_id;

		tx_desc->skb = NULL;
		tx_desc->skb_ext_desc = NULL;
		tx_desc->in_use = false;
		tx_desc->flags = 0;
		tx_desc->paddr_ext_desc = 0;
	}

	list_splice(&desc_free_list, &dp->tx_desc_free_list[ring_id]);

	spin_unlock_bh(&dp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[ring_id];
	while (n_entry--) {
		fast_flag = false;
		tx_status = &tx_status_entry->tx_status;
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		if (!sw_metadata->skb)
			continue;

		ts.buf_rel_source =
			le32_get_bits(tx_status->info0, HAL_WBM_COMPL_TX_INFO0_REL_SRC_MODULE);

		dp->device_stats.tx_completed[ring_id]++;

		dp->device_stats.tx_wbm_rel_source[ts.buf_rel_source]++;

		if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW) {
			ath12k_wifi7_dp_tx_process_htt_tx_complete(dp, (void *)tx_status,
								   sw_metadata->skb,
								   tx_ring, sw_metadata);
			continue;
		}

		ts.status = le32_get_bits(tx_status->info0,
					  HAL_WBM_COMPL_TX_INFO0_TQM_RELEASE_REASON);

		dp->device_stats.tqm_rel_reason[ts.status]++;

		if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
			__skb_queue_head(&free_list_head, sw_metadata->skb);
			sw_metadata->skb = NULL;
			fast_flag = true;
		}

		pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

		rcu_read_lock();

		dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
		if (!dp_pdev) {
			rcu_read_unlock();
			continue;
		}

		if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
			wake_up(&dp_pdev->tx_empty_waitq);

		if (!fast_flag && sw_metadata->skb) {
			ath12k_wifi7_dp_tx_status_parse(ab, tx_status, &ts);

			ath12k_wifi7_dp_tx_complete_msdu(dp_pdev, sw_metadata->skb, &ts,
							 sw_metadata, sw_metadata->mac_id,
							 tx_ring->tcl_data_ring_id);
		}

		rcu_read_unlock();
	}

	dev_kfree_skb_list_fast(&free_list_head);

	return orig_budget - budget;
}

u32 ath12k_wifi7_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_link_vif *arvif,
					    bool vdev_id_check_en)
{
	u32 bank_config = 0;
	u8 link_id = arvif->link_id;
	enum hal_encrypt_type encrypt_type = 0;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];

	/* Only valid for raw frames with HW crypto enabled.
	 * With SW crypto, mac80211 sets key per packet
	 */
	if (dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW &&
	    test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    arvif->key_cipher != INVALID_CIPHER)
		bank_config |=
			u32_encode_bits(ath12k_dp_tx_get_encrypt_type(arvif->key_cipher),
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);
	else
		encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

	bank_config |= u32_encode_bits(dp_vif->tx_encap_type,
					HAL_TX_BANK_CONFIG_ENCAP_TYPE) |
					u32_encode_bits(encrypt_type,
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);

	bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_SRC_BUFFER_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_LINK_META_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_EPD);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		bank_config |= u32_encode_bits(1, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);
	else
		bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);

	bank_config |= u32_encode_bits(dp_link_vif->hal_addr_search_flags & HAL_TX_ADDRX_EN,
					HAL_TX_BANK_CONFIG_ADDRX_EN) |
			u32_encode_bits(!!(dp_link_vif->hal_addr_search_flags &
					HAL_TX_ADDRY_EN),
					HAL_TX_BANK_CONFIG_ADDRY_EN);

	bank_config |= u32_encode_bits(ieee80211_vif_is_mesh(ahvif->vif) ? 3 : 0,
					HAL_TX_BANK_CONFIG_MESH_EN) |
			u32_encode_bits(vdev_id_check_en,
					HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN);

	bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID);

	return bank_config;
}
