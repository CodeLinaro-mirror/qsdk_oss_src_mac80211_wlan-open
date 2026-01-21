// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../peer.h"
#include "../htc.h"
#include "../dp_htt.h"
#include "dp_htt.h"
#include "dp_tx_flow_info.h"
#include "dp_peer.h"

static int ath12k_dp_tx_htt_msduq_mpduq_setup(struct ath12k_base *ab,
					      struct list_head *mpduq_list_head,
					      struct list_head *msduq_list_head,
					      u8 num_mpduq,
					      u8 num_msduq,
					      u8 hw_link_id)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_mpduq_and_msduq_info_hdr *cmd;
	struct htt_mpduq_or_msduq_info *txq_cmd;
	struct ath12k_dp_mpdu_q_info *mpduq, *mpduq_temp;
	struct ath12k_dp_msdu_q_info *msduq, *msduq_temp;
	int hdr_size = sizeof(struct htt_mpduq_and_msduq_info_hdr);
	int q_info_size = sizeof(struct htt_mpduq_or_msduq_info);
	dma_addr_t pn_addr;
	struct sk_buff *skb = NULL;
	int total_len;
	int ret;

	if (!num_mpduq && !num_msduq)
		return -EINVAL;

	total_len = hdr_size +
		((num_mpduq + num_msduq) * q_info_size);
	if (total_len > ATH12K_MAX_DP_HTT_MSG_LEN)
		return -EINVAL;

	skb = ath12k_htc_alloc_skb(ab, total_len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, total_len);

	memset(skb->data, 0x0, total_len);
	/* encode the header */
	cmd = (struct htt_mpduq_and_msduq_info_hdr *)skb->data;
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO_HDR,
				      HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_MSG_TYPE) |
		     le32_encode_bits(total_len - hdr_size,
				      HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_PAYLOAD_SIZE);

	/* encode mpduq info */
	txq_cmd = (struct htt_mpduq_or_msduq_info *)((u8 *)cmd + hdr_size);
	list_for_each_entry_safe(mpduq, mpduq_temp, mpduq_list_head, list) {
		pn_addr = ath12k_dp_get_page_paddr(dp->dp_hw_grp,
						   mpduq->flow_info.peer_id);

		txq_cmd->info0 =
			le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_MSG_TYPE) |
			le32_encode_bits(mpduq->flow_info.flow_type,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_Q_TYPE) |
			le32_encode_bits(hw_link_id,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_HW_LINK_ID);
		txq_cmd->mpduq.info1 =
			le32_encode_bits((mpduq->mpdu_q_paddr) >> 8,
					 HTT_MPDUQ_INFO_CMD_INFO1_MPDUQ_ADDR);
		txq_cmd->mpduq.info2 =
			le32_encode_bits(mpduq->queue_number,
					 HTT_MPDUQ_INFO_CMD_INFO2_MPDUQ_NUM) |
			le32_encode_bits(((u64)pn_addr) >> HAL_ADDR_MSB_REG_SHIFT,
					 HTT_MPDUQ_INFO_CMD_INFO2_PN_ADDR_39_32);
		txq_cmd->mpduq.info3 =
			le32_encode_bits(pn_addr, HTT_MPDUQ_INFO_CMD_INFO3_PN_ADDR_31_0);
		txq_cmd++;
	}

	/* encode msduq info */
	list_for_each_entry_safe(msduq, msduq_temp, msduq_list_head, list) {
		txq_cmd->info0 =
			le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_MSG_TYPE) |
			le32_encode_bits(msduq->flow_info.flow_type,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_Q_TYPE) |
			le32_encode_bits(hw_link_id,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_HW_LINK_ID);
		txq_cmd->msduq.info1 =
			le32_encode_bits(msduq->queue_number,
					 HTT_MSDUQ_INFO_CMD_INFO1_MSDUQ_NUM);
		txq_cmd->msduq.info2 =
			le32_encode_bits((msduq->msdu_q_paddr) >> 8,
					 HTT_MSDUQ_INFO_CMD_INFO2_MSDUQ_ADDR);
		txq_cmd++;
	}

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_dp_tx_htt_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					    struct ath12k_dp_peer *dp_peer,
					    struct list_head *mpduq_pending_list_head,
					    struct list_head *msduq_pending_list_head,
					    u8 link_id,
					    bool is_mcast_queues)
{
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_dp_mpdu_q_info *mpduq, *mpduq_temp;
	struct ath12k_dp_msdu_q_info *msduq, *msduq_temp;
	struct list_head mpduq_list_head;
	struct list_head msduq_list_head;
	int q_info_size = sizeof(struct htt_mpduq_or_msduq_info);
	int total_len;
	u8 num_mpduq = 0;
	u8 num_msduq = 0;
	int i;
	int ret;
	u8 hw_link_id;
	struct ath12k_dp *dp;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_list_head);
	INIT_LIST_HEAD(&msduq_list_head);

init_tx_queues:
	num_mpduq = 0;
	num_msduq = 0;
	total_len = 0;

	list_for_each_entry_safe(mpduq, mpduq_temp, mpduq_pending_list_head, list) {
		/* truncate the list once we reach the limit */
		if ((total_len + q_info_size) >
		    (ATH12K_MAX_DP_HTT_MSG_LEN - sizeof(struct ath12k_htc_hdr))) {
			break;
		}
		list_move_tail(&mpduq->list, &mpduq_list_head);
		num_mpduq++;
		total_len += q_info_size;
	}

	list_for_each_entry_safe(msduq, msduq_temp, msduq_pending_list_head, list) {
		/* truncate the list once we reach the limit */
		if ((total_len + q_info_size) >
		    (ATH12K_MAX_DP_HTT_MSG_LEN - sizeof(struct ath12k_htc_hdr))) {
			break;
		}
		list_move_tail(&msduq->list, &msduq_list_head);
		num_msduq++;
		total_len += q_info_size;
	}

	if (!num_mpduq && !num_msduq)
		return 0;

	rcu_read_lock();

	if (link_id >= ATH12K_GROUP_MAX_RADIO) {
		/* send the message to all the required socs */
		for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
			link_peer = rcu_dereference(dp_peer->link_peers[i]);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			dp =
			ath12k_dp_hw_grp_to_dp(dp_hw_grp,
					       dp_hw_grp->hw_links[hw_link_id].device_id);
			if (!dp)
				continue;

			if (!is_mcast_queues) {
				/* only after assoc done we are allowed send the htt */
				if (!link_peer->assoc_success)
					continue;
			}
			set_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);
			ret = ath12k_dp_tx_htt_msduq_mpduq_setup(dp->ab,
								 &mpduq_list_head,
								 &msduq_list_head,
								 num_mpduq,
								 num_msduq,
								 hw_link_id);
			/* TODO check for any possible error handling */
			if (ret) {
				rcu_read_unlock();
				goto err_release;
			}
		}
	} else {
		/* send the message to specific soc */
		for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
			link_peer = rcu_dereference(dp_peer->link_peers[i]);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (hw_link_id != link_id)
				continue;

			dp =
			ath12k_dp_hw_grp_to_dp(dp_hw_grp,
					       dp_hw_grp->hw_links[hw_link_id].device_id);
			if (!dp)
				continue;

			if (!is_mcast_queues) {
				/* only after assoc done we are allowed send the htt */
				if (!link_peer->assoc_success)
					continue;
			}

			set_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);
			ret = ath12k_dp_tx_htt_msduq_mpduq_setup(dp->ab,
								 &mpduq_list_head,
								 &msduq_list_head,
								 num_mpduq,
								 num_msduq,
								 hw_link_id);
			/* TODO check for any possible error handling */
			if (ret) {
				rcu_read_unlock();
				goto err_release;
			}
			break;
		}
	}
	rcu_read_unlock();

	if (tx_info->txq_hw_links_bitmap == tx_info->assoc_hw_links_bitmap) {
		/* change the state to init done */
		list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list) {
			mpduq->mpduq_state = ATH12K_TX_Q_INIT_DONE;
			list_del(&mpduq->list);
		}

		list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list) {
			msduq->msduq_state = ATH12K_TX_Q_INIT_DONE;
			list_del(&msduq->list);
		}
	} else {
		list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list)
			list_del(&mpduq->list);

		list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list)
			list_del(&msduq->list);
	}

	/* init for pending queues */
	if (!list_empty(mpduq_pending_list_head) ||
	    !list_empty(msduq_pending_list_head)) {
		goto init_tx_queues;
	}

	return 0;
err_release:
	list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list)
		list_del(&mpduq->list);

	list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list)
		list_del(&msduq->list);

	return ret;
}

int ath12k_dp_rx_htt_ast_info_setup(struct ath12k_base *ab,
				    struct ath12k_hal_ast_param *ast_param)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ast_info_t *cmd;
	int len = sizeof(*cmd);
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_ast_info_t *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_AST_INFO,
				      HTT_AST_INFO0_MSG_TYPE) |
		     le32_encode_bits(ast_param->num_ast_entries,
				      HTT_AST_INFO0_TABLE_SIZE) |
		     le32_encode_bits(ast_param->skid_len,
				      HTT_AST_INFO0_MAX_SEARCH);
	cmd->info1 = le32_encode_bits(ast_param->paddr,
				      HTT_AST_INFO1_BASE_ADDR_31_0);
	cmd->info2 = le32_encode_bits(ast_param->ase_hash_key1,
				      HTT_AST_INFO2_HASH_KEY_1);
	cmd->info3 = le32_encode_bits(ast_param->ase_hash_key2,
				      HTT_AST_INFO3_HASH_KEY_2);
	cmd->info4 = le32_encode_bits(ast_param->ase_hash_key3,
				      HTT_AST_INFO4_HASH_KEY_3);
	cmd->info5 = le32_encode_bits(((u64)ast_param->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				      HTT_AST_INFO5_BASE_ADDR_39_32);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_err(ab, "failed to send ast info htt: %d", ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

void ath12k_dp_htt_peer_cleanup_indication(struct ath12k_dp *dp,
					   struct sk_buff *skb)
{
	struct htt_t2h_global_peer_id_unmap *msg =
				(struct htt_t2h_global_peer_id_unmap *)skb->data;
	u16 peer_id;
	u8 hw_link_id;

	peer_id = le32_get_bits(msg->info, HTT_T2H_GLOBAL_PEER_ID_UNMAP_PEER_ID);
	hw_link_id = le32_get_bits(msg->info, HTT_T2H_GLOBAL_PEER_ID_UNMAP_HW_LINK_ID);

	ath12k_dp_peer_cleanup_indication(dp, peer_id, hw_link_id);
}
