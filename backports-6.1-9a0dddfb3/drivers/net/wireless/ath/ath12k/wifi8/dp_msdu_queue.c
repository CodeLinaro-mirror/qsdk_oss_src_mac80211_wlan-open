// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_msdu_queue.h"
#include "dp_pool.h"
#include "dp.h"
#include "dp_tx_tid.h"

struct ath12k_dp_msdu_q_info
*ath12k_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			    struct ath12k_dp_peer *peer)
{
	u32 sw_msduq_idx;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	sw_msduq_ptr = alloc_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt);
	if (!sw_msduq_ptr) {
		ath12k_warn(ab, "no memory available for SW MSDUQ\n");
		return NULL;
	}
	sw_msduq_idx = pool_node_to_id(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
	if (sw_msduq_idx >= NUM_TOTAL_MSDU_QUEUES) {
		ath12k_warn(ab, "sw_msduq_idx %d out of allocated msduq indexes\n",
			    sw_msduq_idx);
		free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
		return NULL;
	}

	memset(sw_msduq_ptr, 0, sizeof(struct ath12k_dp_msdu_q_info));

	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_INVALID;
	sw_msduq_ptr->msduq_idx = sw_msduq_idx;
	sw_msduq_ptr->mlo = peer->is_mlo;
	sw_msduq_ptr->flow_info.peer_id = peer->peer_id;
	sw_msduq_ptr->allocated = 1;

	return sw_msduq_ptr;
}

int ath12k_init_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      struct ath12k_dp_msdu_q_info *sw_msduq_ptr)
{
	struct hal_tx_msdu_flow_info ti = {0};
	u8 tid_num = sw_msduq_ptr->flow_info.tid_num;

	ti.paddr = sw_msduq_ptr->msdu_q_paddr;
	ti.queue_number = (sw_msduq_ptr->queue_number & 0xFFFFFF);
	ti.peer_id = peer->peer_id;
	ti.bitmap = sw_msduq_ptr->bitmap;

	if (tid_num == MLO_MGMT_TID) {
		ti.is_mgmtq = true;
		ti.tid = TQM_NON_DATA_TID;
	} else {
		/*
		 *  NON_QOS_TID is enum 16 but MSDU HW struct has
		 *   only 4 bits tid; hence setting to 15 (TQM_NON_DATA_TID)
		 */
		ti.tid = (tid_num < NON_QOS_TID) ? tid_num : TQM_NON_DATA_TID;
	}
	if (peer->is_mlo)
		ti.mlo = 1;

	return ath12k_wifi8_hal_tx_msdu_queue_setup(dp_hw_grp,
						    sw_msduq_ptr->msduq_idx,
						    &ti);
}

static inline
u8 ath12k_dp_get_chipid_bitmap(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_peer *dp_peer)
{
	int i;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id;
	struct ath12k_dp *partner_dp;
	u8 bitmap = 0;

	rcu_read_lock();
	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
		link_peer = rcu_dereference(dp_peer->link_peers[i]);
		if (!link_peer)
			continue;

		hw_link_id = link_peer->hw_link_id;
		partner_dp =
		ath12k_dp_hw_grp_to_dp(dp_hw_grp,
				       dp_hw_grp->hw_links[hw_link_id].device_id);
		if (!partner_dp)
			continue;
		bitmap |= BIT(partner_dp->device_id);
	}
	rcu_read_unlock();
	return bitmap;
}

struct ath12k_dp_msdu_q_info
*ath12k_init_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer,
				 u8 tid_num, u8 msduq_type,
				 u8 mgmt_msduq_type)
{
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u8 is_qos = 0;
	int ret;

	if (msduq_type == MGMT_TID_MSDUQ_TYPE && mgmt_msduq_type >= MGMT_MSDUQ_TYPE_MAX)
		return NULL;

	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_msduq_ptr = ath12k_alloc_tx_msdu_flowq(dp_hw_grp, peer);
	if (!sw_msduq_ptr) {
		ath12k_warn(ab, "SW MSDUQ allocation failed\n");
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		return NULL;
	}

	is_qos = (tid_num == NON_QOS_TID) ? 0 : 1;

	sw_msduq_ptr->qos = is_qos;
	sw_msduq_ptr->flow_info.tid_num = tid_num;
	sw_msduq_ptr->flow_info.flow_type = msduq_type;
	sw_msduq_ptr->bitmap = ath12k_dp_get_chipid_bitmap(dp_hw_grp, peer);
	pool_addr_from_id(dp_hw_grp_wifi8->msduq_ctxt, sw_msduq_ptr->msduq_idx,
			  &sw_msduq_ptr->msdu_q_vaddr,
			  &sw_msduq_ptr->msdu_q_paddr);

	ret = ath12k_init_tx_msdu_flowq(dp_hw_grp, peer, sw_msduq_ptr);
	if (ret) {
		ath12k_warn(ab, "HAL MSDUQ write failed\n");
		sw_msduq_ptr->msduq_state = ATH12K_TX_Q_DELETED;
		free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		return NULL;
	}

	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_CREATED;

	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);

	return sw_msduq_ptr;
}

void ath12k_free_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_msdu_q_info *sw_msduq_ptr)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
				ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct hal_tx_msdu_flow *msduq;

	if (!sw_msduq_ptr)
		return;

	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_msduq_ptr->allocated = 0;
	msduq = pool_node_from_id(dp_hw_grp_wifi8->msduq_ctxt, sw_msduq_ptr->msduq_idx);
	ath12k_wifi8_hal_msduq_set_invalid(dp_hw_grp, msduq, sw_msduq_ptr->msdu_q_paddr);
	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_DELETED;
	free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
}
