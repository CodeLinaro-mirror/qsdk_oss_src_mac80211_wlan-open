// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_tid.h"
#include "dp_pool.h"
#include "../dp_cmn.h"
#include "dp.h"

struct ath12k_dp_mpdu_q_info
*ath12k_alloc_peer_tid_mpduq(struct ath12k_dp_hw_group *dp_hw_grp,
			     struct ath12k_dp_peer *peer,
			     u8 tidno, u8 flow_type)
{
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	sw_mpduq_ptr = alloc_memory_pool(dp_hw_grp_wifi8->sw_mpduq_ctxt);
	if (!sw_mpduq_ptr)
		return NULL;

	memset(sw_mpduq_ptr, 0, sizeof(struct ath12k_dp_mpdu_q_info));
	sw_mpduq_ptr->mpduq_id = pool_node_to_id(dp_hw_grp_wifi8->sw_mpduq_ctxt,
						 sw_mpduq_ptr);
	sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_INVALID;
	sw_mpduq_ptr->flow_info.tid_num = tidno;
	sw_mpduq_ptr->flow_info.flow_type = HTT_TID_MPDUQ_TYPE;
	sw_mpduq_ptr->flow_info.peer_id = peer->peer_id;
	sw_mpduq_ptr->pn_addr = ath12k_dp_get_page_paddr(dp_hw_grp, peer->peer_id);

	pool_addr_from_id(dp_hw_grp_wifi8->mpduq_ctxt, sw_mpduq_ptr->mpduq_id,
			  &sw_mpduq_ptr->mpdu_q_vaddr, &sw_mpduq_ptr->mpdu_q_paddr);

	return sw_mpduq_ptr;
}

int ath12k_tx_send_mpduq_init(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      struct ath12k_dp_vif *dp_vif,
			      struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr)
{
	u8 tid_num = sw_mpduq_ptr->flow_info.tid_num;
	struct hal_tx_mpdu_queue_head_info ti = {0};
	struct ath12k_sta *ahsta;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id;
	int i;

	ti.paddr = sw_mpduq_ptr->mpdu_q_paddr;
	ti.queue_number = (sw_mpduq_ptr->queue_number & 0xFFFFFF);
	ti.peer_id = peer->peer_id;
	if (tid_num ==  MLO_MGMT_TID) {
		ti.tid = TQM_NON_DATA_TID;
	} else {
		/*
		 *  NON_QOS_TID is enum 16 but MPDU HW struct has
		 *   only 4 bits tid; hence setting to 15 (TQM_NON_DATA_TID)
		 */
		ti.tid = (tid_num < NON_QOS_TID) ? tid_num : TQM_NON_DATA_TID;
	}
	ti.encap_type = dp_vif->tx_encap_type;
	//TBD: ti.wapi
	ti.assoc_link_id = ATH12K_INVALID_LINK_ID;
	ti.link_id1 = ATH12K_INVALID_LINK_ID;
	ti.link_id2 = ATH12K_INVALID_LINK_ID;
	if (peer->sta) {
		rcu_read_lock();
		ahsta = ath12k_sta_to_ahsta(peer->sta);
		ti.assoc_link_id = ahsta->assoc_link_id;
		for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
			link_peer = rcu_dereference(peer->link_peers[i]);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (hw_link_id == ti.assoc_link_id)
				continue;
			if (ti.link_id1 == ATH12K_INVALID_LINK_ID) {
				ti.link_id1 = hw_link_id;
				continue;
			}
			if (ti.link_id2 == ATH12K_INVALID_LINK_ID) {
				ti.link_id2 = hw_link_id;
				break;
			}
		}
		rcu_read_unlock();
	}
	ti.mlo = peer->is_mlo;
	ti.pn_dma_addr = sw_mpduq_ptr->pn_addr;

	return ath12k_wifi8_hal_tx_mpdu_queue_setup(dp_hw_grp,
						    sw_mpduq_ptr->mpduq_id,
						    &ti);
}

struct ath12k_dp_tx_tid_info *ath12k_dp_get_tid(struct ath12k_dp_peer *peer, u8 tidno)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	struct ath12k_dp_tx_tid_info *tid = NULL;

	rcu_read_lock();
	if (tidno < ATH12K_MAX_NUM_DATA_TIDS)
		tid = &tx_flow_info->tid_info[tidno];
	else if (tidno == NON_QOS_TID)
		tid = &tx_flow_info->tid_info[DEFAULT_TID];
	rcu_read_unlock();
	return tid;
}

struct ath12k_dp_mpdu_q_info *ath12k_peer_alloc_tid(struct ath12k_dp_hw_group *dp_hw_grp,
						    struct ath12k_dp_peer *peer,
						    struct ath12k_dp_vif *dp_vif,
						    u8 tidno,
						    struct ath12k_dp_tx_tid_info **ptid,
						    u8 flow_type)
{
	struct ath12k_dp_tx_tid_info *tid = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	int ret;

	if ((tidno != NON_QOS_TID && tidno >= ATH12K_MAX_NUM_DATA_TIDS) ||
	    flow_type == HTT_TID_MSDUQ_MCAST) {
		*ptid = NULL;
		goto create_mpdu;
	}

	*ptid = tid = ath12k_dp_get_tid(peer, tidno);
	if (tid && tid->tid_num == tidno && tid->mpduq)
		return tid->mpduq;

	memset(tid, 0, sizeof(struct ath12k_dp_tx_tid_info));
	tid->tid_num = tidno;
	tid->peer_id = peer->peer_id;

create_mpdu:
	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_mpduq_ptr = ath12k_alloc_peer_tid_mpduq(dp_hw_grp, peer, tidno, flow_type);
	if (!sw_mpduq_ptr) {
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		goto error;
	}
	ret = ath12k_tx_send_mpduq_init(dp_hw_grp, peer, dp_vif, sw_mpduq_ptr);
	if (ret) {
		ath12k_err(ab, "HAL MPDUQ INIT FAILED\n");
		sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_DELETED;
		free_memory_pool(dp_hw_grp_wifi8->sw_mpduq_ctxt, sw_mpduq_ptr);
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		goto error;
	}
	sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_CREATED;
	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);

	return sw_mpduq_ptr;

error:
	*ptid = NULL;
	return NULL;
}
