// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"
#include "dp.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"
#include "dp_tx_flow_info.h"
#include "../telemetry_agent_if.h"

static u16 ath12k_wifi8_peer_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 peer_id;
	int i;

	spin_lock_bh(&dp_hw->peer_lock);
	peer_id = dp_hw->last_peer_id;
	for (i = 0; i < ATH12K_MAX_PEER_ID; i++) {
		peer_id = (peer_id + 1) % ATH12K_MAX_PEER_ID;

		if (!peer_id)
			continue;

		if (test_bit(peer_id, dp_hw->free_peer_id_map))
			continue;

		set_bit(peer_id, dp_hw->free_peer_id_map);
		break;
	}

	dp_hw->last_peer_id = peer_id;
	if (i >= ATH12K_MAX_PEER_ID)
		peer_id = ATH12K_MLO_PEER_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated peer_id:%d", peer_id);

	return peer_id;
}

static u16 ath12k_wifi8_sta_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 sta_id;
	int i;

	spin_lock_bh(&dp_hw->peer_lock);
	sta_id = dp_hw->last_sta_id;
	for (i = 0; i < ATH12K_MAX_STA_ID; i++) {
		sta_id = (sta_id + 1) % ATH12K_MAX_STA_ID;

		if (test_bit(sta_id, dp_hw->free_sta_id_map))
			continue;

		set_bit(sta_id, dp_hw->free_sta_id_map);
		break;
	}

	dp_hw->last_sta_id = sta_id;
	if (i >= ATH12K_MAX_STA_ID)
		sta_id = ATH12K_STA_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated sta_id:%d", sta_id);

	return sta_id;
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct wireless_dev *wdev;
	struct ath12k_sta *ahsta = NULL;

	ahsta = ath12k_sta_to_ahsta(params->sta);

	spin_lock_bh(&dp_hw->peer_lock);
	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_lock);
	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer)
		return -ENOMEM;

	dp_peer->sta_id = ATH12K_STA_ID_INVALID;
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = ath12k_wifi8_peer_id_alloc(dp_hw);
	if (dp_peer->peer_id == ATH12K_MLO_PEER_ID_INVALID) {
		kfree(dp_peer);
		return -ENOMEM;
	}

	if (!params->is_vdev_peer) {
		dp_peer->sta_id = ath12k_wifi8_sta_id_alloc(dp_hw);
		if (dp_peer->sta_id == ATH12K_STA_ID_INVALID) {
			spin_lock_bh(&dp_hw->peer_lock);
			clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
			spin_unlock_bh(&dp_hw->peer_lock);
			kfree(dp_peer);
			return -ENOMEM;
		}
	}

	dp_peer->is_vdev_peer = params->is_vdev_peer;
	dp_peer->is_sta_bss_peer = params->is_sta_bss_peer;
	dp_peer->link_peer_delete_stats = ath12k_dp_alloc_preserved_stats();
	if (!dp_peer->link_peer_delete_stats) {
		spin_lock_bh(&dp_hw->peer_lock);
		clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
		clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);
		spin_unlock_bh(&dp_hw->peer_lock);
		ath12k_err(NULL, "Failed to allocate link peer delete stats");
		kfree(dp_peer);
		return -ENOMEM;
	}

	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer)
		dp_peer->hw_link_id = params->hw_link_id;
	else
		ahsta->dp_peer_id = dp_peer->peer_id;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev) {
		dp_peer->dev = wdev->netdev;
		if (params->is_sta_bss_peer)
			dp_peer->is_sta_bss_peer_4addr = wdev->use_4addr;
	}

	spin_lock_bh(&dp_hw->peer_lock);

	list_add(&dp_peer->list, &dp_hw->peers);

	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	params->peer_id = dp_peer->peer_id;
	params->sta_id = dp_peer->sta_id;
	return 0;
}

void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	u16 peerid_index;

	spin_lock_bh(&dp_hw->peer_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, hw_link_id);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return;
	}

	list_del(&dp_peer->list);

	clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);
	if (!dp_peer->peer_ext_ctx) {
		clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
		peerid_index = dp_peer->peer_id;
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
		if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);
		spin_unlock_bh(&dp_hw->peer_lock);
		synchronize_rcu();
		kfree(dp_peer->qos);
		ath12k_dp_free_preserved_stats(dp_peer->link_peer_delete_stats);
		kfree(dp_peer);
		return;
	}

	ath12k_dp_ast_entry_delete(dp->dp_hw_grp,
				   dp_peer->peer_ext_ctx->ast_index);

	spin_unlock_bh(&dp_hw->peer_lock);
}

int ath12k_wifi8_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	dma_addr_t pn_counter_paddr = 0;
	struct ath12k_dp_peer *dp_peer;
	dma_addr_t tx_classify_paddr;
	void *tx_classify_vaddr;
	bool is_qos = true;
	int ret, i;
	int vdev_peer_link_id;
	struct ath12k_dp_link_vif *dp_link_vif;

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = ath12k_dp_peer_find(dp_hw, addr);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -ENOENT;
	}

	peer_ext_ctx = kzalloc(sizeof(*peer_ext_ctx), GFP_ATOMIC);
	if (!peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -ENOMEM;
	}

	dp_peer->peer_ext_ctx = peer_ext_ctx;
	spin_lock_init(&peer_ext_ctx->tx_flow_info.tx_q_lock);
	rcu_read_lock();
	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
		link_peer = rcu_dereference(dp_peer->link_peers[i]);
		if (!link_peer)
			continue;

		set_bit(link_peer->hw_link_id,
			&peer_ext_ctx->tx_flow_info.assoc_hw_links_bitmap);

		if (dp_peer->is_vdev_peer) {
			vdev_peer_link_id = i;
			break;
		}
	}
	rcu_read_unlock();
	ret = ath12k_dp_tx_classify_info_alloc(dp->dp_hw_grp,
					       &tx_classify_paddr,
					       &tx_classify_vaddr);
	if (ret)
		goto free_peer_ext_ctx;

	peer_ext_ctx->tx_flow_info.hw_who_classify_info_vaddr = tx_classify_vaddr;
	peer_ext_ctx->tx_flow_info.hw_who_classify_info_paddr = tx_classify_paddr;
	pn_counter_paddr = ath12k_dp_get_page_paddr(dp->dp_hw_grp, dp_peer->peer_id);
	if (!pn_counter_paddr) {
		ret = -ENOMEM;
		goto free_tx_classify_info;
	}

	if (dp_peer->is_vdev_peer) {
		ret = ath12k_peer_alloc_mcast_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	} else {
		if (dp_peer->sta->wme)
			is_qos = true;
		else
			is_qos = false;

		ret = ath12k_peer_alloc_default_queues(dp->dp_hw_grp, dp_peer,
						       dp_vif, is_qos);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_hol_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_mgmt_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	}

	memcpy(ast_param.mac_addr, addr, ETH_ALEN);
	ast_param.peer_id = dp_peer->peer_id;
	ast_param.tx_classify_info_paddr = tx_classify_paddr;
	ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_USE_ADDRX;
	ret = ath12k_dp_ast_entry_create(dp->dp_hw_grp, &ast_param);
	if (ret)
		goto free_queues_info;

	peer_ext_ctx->ast_index = ast_param.ast_index;
	peer_ext_ctx->ast_hash = ast_param.ast_hash;

	if (dp_peer->is_sta_bss_peer) {
		dp_vif->ast_idx = ast_param.ast_index;
		dp_vif->ast_hash = ast_param.ast_hash;
	} else if (dp_peer->is_vdev_peer) {
		dp_link_vif = &dp_vif->dp_link_vif[vdev_peer_link_id];
		dp_link_vif->ast_idx = ast_param.ast_index;
		dp_link_vif->ast_hash =	ast_param.ast_hash;
	}

	spin_unlock_bh(&dp_hw->peer_lock);
	return 0;

free_queues_info:
	ath12k_peer_free_static_queues(dp->dp_hw_grp, dp_peer);
free_tx_classify_info:
	ath12k_dp_tx_classify_info_free(dp->dp_hw_grp,
					tx_classify_paddr,
					tx_classify_vaddr);
free_peer_ext_ctx:
	kfree(peer_ext_ctx);
	dp_peer->peer_ext_ctx = NULL;
	spin_unlock_bh(&dp_hw->peer_lock);
	return ret;
}

int ath12k_wifi8_dp_link_peer_create(struct ath12k_base *ab, u32 vdev_id, u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);
	peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		return -ENOMEM;
	}

	peer->vdev_id = vdev_id;
	peer->peer_id = ATH12K_MLO_PEER_ID_INVALID;
	ether_addr_copy(peer->addr, addr);
	list_add(&peer->list, &dp->peers);
	ewma_avg_rssi_init(&peer->avg_rssi);

	spin_unlock_bh(&dp->dp_lock);

	return 0;
}

void ath12k_wifi8_dp_link_peer_delete(struct ath12k_base *ab, u32 vdev_id, u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, vdev_id, addr);
	if (!peer)
		goto exit;

	ath12k_link_peer_free(peer);
exit:
	spin_unlock_bh(&dp->dp_lock);
}

void ath12k_dp_peer_cleanup_indication(struct ath12k_dp *dp,
				       u16 peer_id,
				       u8 hw_link_id)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_dp_peer *dp_peer;
	u8 pdev_id;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
					      dp_hw_grp->hw_links[hw_link_id].pdev_idx);
	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	dp_hw = dp_pdev->dp_hw;
	if (!dp_hw) {
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = rcu_dereference(dp_pdev->dp_hw->dp_peer_list[peer_id]);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		rcu_read_unlock();
		return;
	}

	tx_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_info) {
		rcu_read_unlock();
		spin_unlock_bh(&dp_hw->peer_lock);
		return;
	}

	clear_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);
	/* Check whether event is for last link or not */
	if (tx_info->txq_hw_links_bitmap) {
		spin_unlock_bh(&dp_hw->peer_lock);
		rcu_read_unlock();
		return;
	}

	if (ath12k_dp_peer_find(dp_pdev->dp_hw, dp_peer->addr)) {
		list_del(&dp_peer->list);
		clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);
		ath12k_dp_ast_entry_delete(dp->dp_hw_grp,
					   dp_peer->peer_ext_ctx->ast_index);
	}

	/*
	 * 1. Free the MSDUQ Queues
	 * 2. Free the MPDU Queues
	 * 3. Free the PN Address
	 * 4. Free who classify info
	 */

	clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
	rcu_assign_pointer(dp_hw->dp_peer_list[peer_id], NULL);
	kfree(dp_peer->peer_ext_ctx);
	dp_peer->peer_ext_ctx = NULL;

	spin_unlock_bh(&dp_hw->peer_lock);
	rcu_read_unlock();

	kfree(dp_peer);
}

void ath12k_wifi8_dp_link_peer_assoc(struct ath12k_dp_hw *dp_hw,
				     struct ath12k_dp *dp,
				     u8 *addr, u32 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = ath12k_dp_peer_find(dp_hw, addr);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return;
	}

	ath12k_dp_tx_peer_msduq_mpduq_setup(dp->dp_hw_grp, dp_peer, hw_link_id);
	spin_unlock_bh(&dp_hw->peer_lock);
}

int ath12k_wifi8_get_mgmt_flowq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr,
				struct peer_assoc_flowq_params *flowq_params)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct peer_assoc_msduq_params *msduq_params;
	struct peer_assoc_mpduq_params *mpduq_params;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	u64 dma_addr = 0;
	u8 hw_link_id;
	int ret = 0;
	u8 idx;
	int i;

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = ath12k_dp_peer_find(dp_hw, addr);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	flowq_params->num_links = 0;
	rcu_read_lock();
	if (dp_peer->is_mlo) {
		for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
			link_peer = rcu_dereference(dp_peer->link_peers[i]);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (!tx_info->mgmt_msduq[ATH12K_LINK_TO_MGMT_TYPE(hw_link_id)])
				continue;
			idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
			dma_addr = (u64)tx_info->mgmt_msduq[idx]->msdu_q_paddr;
			msduq_params =
				&flowq_params->msduq_params[flowq_params->num_links];
			msduq_params->mgmt_msduq_address =
					(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
			msduq_params->flow_type = WMI_MGMT_TID_MSDUQ_LINK_SPECIFIC;
			msduq_params->link_id = hw_link_id;
			flowq_params->num_links++;
		}

		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]) {
			ret = -ENOENT;
			goto exit;
		}
		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = WMI_MGMT_TID_MSDUQ_LINK_AGNOSTIC;
		flowq_params->num_links++;
	} else {
		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]) {
			ret = -ENOENT;
			goto exit;
		}

		for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
			link_peer = rcu_dereference(dp_peer->link_peers[i]);
			if (link_peer)
				break;
		}

		if (!link_peer) {
			ret = -ENOENT;
			goto exit;
		}

		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = WMI_MGMT_TID_MSDUQ_LINK_SPECIFIC;
		msduq_params->link_id = link_peer->hw_link_id;
		flowq_params->num_links++;
	}

	if (tx_info->mgmt_mpduq) {
		dma_addr = (u64)tx_info->mgmt_mpduq->mpdu_q_paddr;
		mpduq_params = &flowq_params->mpduq_params;
		mpduq_params->mgmt_mpduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		dma_addr = (u64)tx_info->mgmt_mpduq->pn_addr;
		mpduq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
		mpduq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	}
	flowq_params->enabled = 1;

exit:
	rcu_read_unlock();
	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_lock);
	return ret;
}

int ath12k_wifi8_get_holq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			  u8 *addr, struct peer_assoc_holq_params *holq_params)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_dp_peer *dp_peer;
	u64 dma_addr = 0;
	int ret = 0;

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = ath12k_dp_peer_find(dp_hw, addr);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	if (!tx_info->hol_msduq || !tx_info->tid_info[ATH12K_HOL_TID].mpduq) {
		spin_unlock_bh(&tx_info->tx_q_lock);
		spin_unlock_bh(&dp_hw->peer_lock);
		return -ENOENT;
	}

	holq_params->peer_id = tx_info->hol_msduq->flow_info.peer_id;
	holq_params->tid = tx_info->hol_msduq->flow_info.tid_num;
	holq_params->mpdu_type =
		tx_info->tid_info[ATH12K_HOL_TID].mpduq->flow_info.flow_type;
	holq_params->msdu_type = tx_info->hol_msduq->flow_info.flow_type;

	dma_addr = (u64)tx_info->tid_info[ATH12K_HOL_TID].mpduq->mpdu_q_paddr;
	holq_params->mpduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->hol_msduq->msdu_q_paddr;
	holq_params->msduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->tid_info[ATH12K_HOL_TID].mpduq->pn_addr;
	holq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
	holq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	holq_params->enabled = 1;

	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_lock);
	return ret;
}

int ath12k_wifi8_dp_get_peer_init_status(struct ath12k_dp *dp,
					 struct ath12k_dp_hw *dp_hw,
					 u8 *addr)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp))
		return 0;

	if (!wait_for_completion_timeout(&dp_hw_grp_wifi8->peer_init_done, 1 * HZ)) {
		ath12k_wifi8_invalidate_peer_ase_cache_table(dp_hw_grp);
		ath12k_warn(dp->ab, "peer init is not completed for %pM", addr);
		return -ETIMEDOUT;
	}

	return 0;
}
