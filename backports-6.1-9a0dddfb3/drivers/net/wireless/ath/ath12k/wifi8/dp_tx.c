// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_tx.h"
#include "../peer.h"
#include "dp_tx.h"
#include "dp.h"
#include "hal_rx.h"
#include "hal_desc.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../sdwf.h"
#include "../dp_stats.h"
#include "../dp_peer.h"
#include "../telemetry.h"
#include "../telemetry_agent_if.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"

struct ath12k_tx_sw_metadata {
	struct sk_buff *skb;
	struct sk_buff *skb_ext_desc;
	u64 paddr      : 40,
	    len	       : 16,
	    hw_link_id : 5,
	    flags      : 3;
	u64 paddr_ext_desc : 40,
	    ext_desc_len   : 16,
	    rsvd2          : 8;
} __packed __aligned(32);

static_assert(sizeof(struct ath12k_tx_sw_metadata) == 32,
	      "ath12k_tx_sw_metadata size != 32");

struct ath12k_wifi8_tx_status_entry {
	struct hal_tqm2sw_completion_ring tx_status;
	union {
		struct ath12k_tx_sw_metadata sw_metadata;
		void *tx_desc;
	};
} __packed;

static_assert(sizeof(struct ath12k_wifi8_tx_status_entry) == 64,
	      "ath12k_wifi8_tx_status_entry size != 64");

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_pkt_stats[reason]++;
	tstats->tid_stats[tid].tx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}

void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_drop_stats[reason]++;
	tstats->tid_stats[tid].tx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}

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

int ath12k_wifi8_dp_tqm_cmd_send(struct ath12k_base *ab,
				 enum hal_tlv_tag_be type,
				 struct ath12k_hal_tqm_cmd *cmd,
				 struct ath12k_dp_tx_queue *data,
				 void (*callback_fn)(struct ath12k_dp *dp,
				 void *ctx, struct hal_tqm_status *tqm_status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_tqm_cmd *dp_cmd;
	struct hal_srng *cmd_ring;
	int cmd_num;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	cmd_ring = &ab->hal.srng_list[dp_wifi8->tqm_cmd_ring.ring_id];
	cmd_num = ath12k_wifi8_hal_tqm_cmd_send(ab, cmd_ring, type, cmd);

	//error, hence return error code
	if (cmd_num < 0) {
		ath12k_warn(ab, "Failed to send TQM command: %d", cmd_num);
		return cmd_num;
	}
	//cmd_num starts from 1
	if (cmd_num == 0) {
		ath12k_warn(ab, "TQM command returned zero cmd_num");
		return -EINVAL;
	}
	if (!callback_fn)
		return 0;
	dp_cmd = kzalloc(sizeof(*dp_cmd), GFP_ATOMIC);

	if (!dp_cmd)
		return -ENOMEM;

	dp_cmd->cmd_num = cmd_num;
	dp_cmd->cmd_type = type;
	dp_cmd->handler = callback_fn;
	memcpy(&dp_cmd->data, data, sizeof(*data));
	spin_lock_bh(&dp->tqm_cmd_lock);
	list_add_tail(&dp_cmd->list, &dp->tqm_cmd_list);
	spin_unlock_bh(&dp->tqm_cmd_lock);

	return 0;
}

//to be called from dp->hw_params->ring_mask->tqm_status
int ath12k_wifi8_dp_tx_process_tqm_status(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	struct ath12k_dp_tqm_cmd *cmd, *tmp;
	struct hal_tqm_status tqm_status;
	bool found = false;
	int quota = budget;
	u16 tag;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	srng = &ab->hal.srng_list[dp_wifi8->tqm_status_ring.ring_id];

	memset(&tqm_status, 0, sizeof(tqm_status));

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- && (hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		switch (tag) {
		case HAL_TQM_REMOVE_MSDU_STATUS_BO:
			ath12k_wifi8_hal_tqm_remove_msdu_status(ab, hdr,
								&tqm_status);
			break;
		case HAL_TQM_REMOVE_MPDU_STATUS_BO:
			ath12k_wifi8_hal_tqm_remove_mpdu_status(ab, hdr,
								&tqm_status);
			break;
		case HAL_TQM_SYNC_CMD_STATUS_BO:
			ath12k_wifi8_hal_tqm_sync_cmd_status(ab, hdr,
							     &tqm_status);
			break;
		case HAL_TQM_GET_MPDUQ_STATS_STATUS_BO:
			ath12k_wifi8_hal_tqm_get_mpduq_stats_cmd_status(ab,
									hdr,
									&tqm_status);
			break;
		case HAL_TQM_UPDATE_MPDUQ_STATUS_BO:
			ath12k_wifi8_hal_tqm_update_mpduq_cmd_status(ab,
								     hdr,
								     &tqm_status);
			break;
		default:
			ath12k_warn(ab, "unknown tqm status type %d", tag);
			continue;
		}

		spin_lock_bh(&dp->tqm_cmd_lock);
		list_for_each_entry_safe(cmd, tmp, &dp->tqm_cmd_list, list) {
			if (tqm_status.status_hdr.status_num == cmd->cmd_num) {
				found = true;
				list_del(&cmd->list);
				break;
			}
		}
		spin_unlock_bh(&dp->tqm_cmd_lock);

		if (found) {
			cmd->handler(dp, (void *)&cmd->data, &tqm_status);
			kfree(cmd);
		}
		found = false;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

void ath12k_dp_peer_cleanup_tqm_sync(struct ath12k_dp *dp, void *ctx,
				     struct hal_tqm_status *tqm_status)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_tx_queue *data = ctx;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	u8 pdev_id;
	u8 hw_link_id = data->hw_link_id;
	u16 peer_id = data->peer_id;
	dma_addr_t tx_classify_info_paddr;
	void *tx_classify_info_vaddr;

	if (!tqm_status) {
		ath12k_err(ab, "Error: TQM STATUS is not valid");
		return;
	}
	if (tqm_status->status_hdr.cmd_execution_status != HAL_TQM_SUCCESSFUL_EXECUTION) {
		ath12k_err(ab, "Error: TQM STATUS FAILED with reason %d",
			   tqm_status->status_hdr.cmd_execution_status);
		return;
	}

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

	if (dp_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED)
		ath12k_dp_ast_entry_delete(dp->dp_hw_grp,
					   dp_peer->peer_ext_ctx->ast_index);

	tx_classify_info_paddr =
		dp_peer->peer_ext_ctx->tx_flow_info.hw_who_classify_info_paddr;
	tx_classify_info_vaddr =
		dp_peer->peer_ext_ctx->tx_flow_info.hw_who_classify_info_vaddr;
	ath12k_dp_peer_free_queues(dp_hw_grp, dp_peer); //generic free API
	ath12k_dp_tx_classify_info_free(dp_hw_grp, tx_classify_info_paddr,
					tx_classify_info_vaddr);

	kfree(dp_peer->peer_ext_ctx);
	dp_peer->peer_ext_ctx = NULL;

	if (dp_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED) {
		dp_peer->dp_peer_state = ATH12K_DP_PEER_LOGICALLY_DELETED;
		spin_unlock_bh(&dp_hw->peer_lock);
		rcu_read_unlock();
	} else {
		ath12k_wifi8_dp_peer_cleanup(dp_hw, dp_peer);
		spin_unlock_bh(&dp_hw->peer_lock);
		rcu_read_unlock();
		kfree_rcu(dp_peer, rcu_head);
	}
}

static void
ath12k_wifi8_hal_tx_cmd_ext_desc_setup(struct ath12k_base *ab,
				       struct hal_tx_msdu_extension *tcl_ext_cmd,
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

/* TODO: SAWF Team to review */
static inline
void ath12k_wifi_qos_desc(struct hal_tcl_data_cmd *desc,
			  u32 msduq, u16 qos_id)
{
	u32 tid, flow_override, who_classify_info_sel;
	u32 meta_data_flags;

	tid = u32_get_bits(msduq, MSDUQ_TID);
	flow_override = u32_get_bits(msduq, MSDUQ_FLOW_OVERRIDE);
	who_classify_info_sel = u32_get_bits(msduq, MSDUQ_WHO_CL_INFO);

	desc->info1 = u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		      u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE);

	desc->info2 = u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		      u32_encode_bits(who_classify_info_sel,
				      HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL);

	meta_data_flags = ath12k_qos_get_metadata(qos_id);
	desc->info3 = u32_encode_bits(flow_override,
				      HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT);
	desc->tcl_cmd_number = cpu_to_le32(meta_data_flags);
}

static inline
void ath12k_wifi_qos_hlos_tid(struct hal_tcl_data_cmd *desc,
			      u8 tid)
{
	desc->info1 |= u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		       u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE);
}

static inline u8 ath12k_get_qos_tag(u32 mark)
{
	u8 qos_tag = u32_get_bits(mark, QOS_TAG_MASK);

	if (qos_tag == QOS_SCS_TAG || qos_tag == QOS_MSCS_TAG)
		return qos_tag;

	return 0;
}

static inline void
ath12k_dp_qos_update(struct ath12k_dp *dp, struct ath12k_pdev_dp *dp_pdev,
		     u32 mark, struct hal_tcl_data_cmd *desc, u8 qos_tag,
		     u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer *dp_peer;
	u8 scs_id;
	u16 msduq, peer_id;
	u16 qos_id = QOS_ID_MAX;
	int ret;

	if (mark & SDWF_VALID_MASK) {
		rcu_read_lock();
		peer_id = u32_get_bits(mark, SDWF_PEER_ID);
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return;
		}
		msduq = u32_get_bits(mark, SDWF_MSDUQ_ID);
		if (msduq >= MSDUQ_MAX_DEF)
			qos_id = dp_peer_msduq_qos_id(dp->ab, dp_peer->qos,
						      msduq);
		rcu_read_unlock();
	} else if (qos_tag == QOS_SCS_TAG) {
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
	} else if (msduq < QOS_MSDUQ_MAX) {
		/* Update Desc for User Defined QoS MSDUQ */
		ath12k_wifi_qos_desc(desc, msduq, qos_id);
	}
}

static void ath12k_qos_tx_enqueue_peer_stats(struct ath12k_dp_link_peer_stats *peer_stats,
					     u16 msduq_id,
					     unsigned int len)
{
	struct tx_stats *qos_tx;
	u8 tid, q_id;

	if (!peer_stats->qos_stats) {
		ath12k_err(NULL, "Qos stats not initialized\n");
		return;
	}

	if (unlikely(msduq_id >= QOS_MSDUQ_MAX &&
		     msduq_id < MSDUQ_MAX_DEF))
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);

	qos_tx = &peer_stats->qos_stats->qos_tx[tid][q_id];

	qos_tx->queue_depth++;
	qos_tx->tx_ingress.num++;
	qos_tx->tx_ingress.bytes += len;
}

static void
ath12k_dp_sdwftx_ingress_stats_update(struct ath12k_link_vif *arvif,
				      u32 *skb_mark, u32 qos_nw_delay,
				      unsigned int skb_len)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_dp *dp;
	struct ath12k_dp_link_peer *pri_peer;
	u16 msduq, peer_id, qos_id;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (!ar)
		return;

	if (!(*skb_mark & SDWF_VALID_MASK))
		return;

	msduq = u32_get_bits(*skb_mark, SDWF_MSDUQ_ID);

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    (ath12k_debugfs_is_qos_stats_enabled(ar) &
	    ATH12K_QOS_STATS_BASIC)) {
		peer_id = u32_get_bits(*skb_mark, SDWF_PEER_ID);

		if (!ar->dp.dp)
			return;

		dp = ar->dp.dp;

		rcu_read_lock();
		spin_lock_bh(&dp->dp_lock);

		pri_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								    peer_id);
		if (!pri_peer || !pri_peer->dp_peer || !pri_peer->dp_peer->qos) {
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		qos_id = dp_peer_msduq_qos_id(ar->ab, pri_peer->dp_peer->qos,
					      msduq);
		if (qos_id == QOS_ID_INVALID) {
			ath12k_err(ar->ab, "msduq_id: %u not yet reserved\n",
				   msduq);
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		ath12k_qos_tx_enqueue_peer_stats(&pri_peer->peer_stats,
						 msduq, skb_len);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
	}

	/* Store the NWDELAY to skb->mark which can be fetched
	 * during tx completion
	 */
	if (qos_nw_delay > QOS_NW_DELAY_MAX)
		qos_nw_delay = QOS_NW_DELAY_MAX;

	*skb_mark = u32_encode_bits(u32_get_bits(*skb_mark, QOS_NW_TAG_SHIFT),
				    QOS_TAG_ID) | (qos_nw_delay << QOS_NW_DELAY_SHIFT) |
				    msduq;
}

void ath12k_sdwf_update_peer_mcs_stats(struct tx_stats *qos_tx,
				       struct hal_tx_status *ts)
{
	u8 mcs = MAX_MCS, pkt_type;

	mcs = ts->mcs;
	pkt_type = ts->pkt_type;

	if (pkt_type > HAL_TX_RATE_STATS_PKT_TYPE_11BE ||
	    pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BA) {
		return;
	}

	if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE)
		pkt_type = DOT11_BE;

	switch (pkt_type) {
	case DOT11_A:
		mcs = (mcs >= MAX_MCS_11A) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_B:
		mcs = (mcs >= MAX_MCS_11B) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_N:
		mcs = (mcs >= MAX_MCS_11N) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AC:
		mcs = (mcs >= MAX_MCS_11AC) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AX:
		mcs = (mcs >= MAX_MCS_11AX) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_BE:
		mcs = (mcs >= MAX_MCS_11BE) ? (MAX_MCS - 1) : mcs;
		break;
	default:
		break;
	}

	if (mcs != MAX_MCS)
		qos_tx->pkt_type[pkt_type].mcs_count[mcs]++;
}

bool ath12k_get_qos_params_delay_bound(struct ath12k_base *ab, u8 qos_id,
				       u32 *delay_bound)
{
	struct ath12k_qos_ctx *qos_ctx;

	if (qos_id >= QOS_PROFILES_MAX) {
		ath12k_err(NULL, "Invalid qos id :%u\n", qos_id);
		return false;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(NULL, "QoS Context is NULL\n");
		return false;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	if (!qos_ctx->profiles[qos_id].ref_count) {
		ath12k_err(NULL, "Qos ctx : %u profiles not present\n",
			   qos_id);
		spin_unlock_bh(&qos_ctx->profile_lock);
		return false;
	}

	*delay_bound = qos_ctx->profiles[qos_id].params.msdu_delivery_info;
	spin_unlock_bh(&qos_ctx->profile_lock);

	return true;
}

#define HW_TX_DELAY_MAX				0x1000000

#define TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US	10
#define HW_TX_DELAY_MASK			0x1FFFFFFF
#define TX_COMPL_BUFFER_TSTAMP_US(TSTAMP) \
	(((TSTAMP) << TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US) & \
	 HW_TX_DELAY_MASK)

#define ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER 1000
#define ATH12K_MOV_AVG_PKT_WIN	10

#define ATH12K_HIST_AVG_DIV	2

const u32 ath12k_hist_hw_tx_comp_bucket[] = {
	250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000, U32_MAX
};

void ath12k_hist_fill_buckets(struct hist_bucket *hist, u32 value)
{
	int idx;

	hist->hist_type = HIST_TYPE_HW_TX_COMP_DELAY;

	for (idx = HIST_BUCKET_0; idx < ARRAY_SIZE(ath12k_hist_hw_tx_comp_bucket); idx++) {
		if (value <= ath12k_hist_hw_tx_comp_bucket[idx]) {
			hist->freq[idx]++;
			return;
		}
	}
}

void ath12k_update_hist_stats(struct hist_stats *hist_stats, u32 value)
{
	if (!hist_stats)
		return;

	ath12k_hist_fill_buckets(&hist_stats->hist, value);

	if (!hist_stats->min || value < hist_stats->min)
		hist_stats->min = value;

	if (value > hist_stats->max)
		hist_stats->max = value;

	if (unlikely(!hist_stats->avg))
		hist_stats->avg = value;
	else
		hist_stats->avg = (hist_stats->avg + value) / ATH12K_HIST_AVG_DIV;
}

void ath12k_sdwf_compute_hw_delay(struct ath12k *ar, struct hal_tx_status *ts,
				  u32 *hw_delay)
{
	/* low 32 alone will be filled for TSF2 from FW and the value can be
	 * negative for both TSF2 and TQM delta
	 */
	int tmp_delta_tsf2 = ar->delta_tsf2, tmp_delta_tqm = ar->delta_tqm;
	u32 msdu_tqm_enqueue_tstamp_us, final_msdu_tqm_enqueue_tstamp_us;
	u32 msdu_compl_tsf_tstamp_us, final_msdu_compl_tsf_tstamp_us;
	struct ath12k_hw_group *ag = ar->ab->ag;
	/* MLO TSTAMP OFFSET can be negative
	 */
	int mlo_offset = ag->mlo_tstamp_offset;
	int delta_tsf2, delta_tqm;

	msdu_tqm_enqueue_tstamp_us =
		TX_COMPL_BUFFER_TSTAMP_US(ts->buffer_timestamp);
	msdu_compl_tsf_tstamp_us = ts->tsf;
	delta_tsf2 = mlo_offset - tmp_delta_tsf2;
	delta_tqm = mlo_offset - tmp_delta_tqm;

	final_msdu_tqm_enqueue_tstamp_us =
		(msdu_tqm_enqueue_tstamp_us + delta_tqm) & HW_TX_DELAY_MASK;
	final_msdu_compl_tsf_tstamp_us =
		(msdu_compl_tsf_tstamp_us + delta_tsf2) & HW_TX_DELAY_MASK;

	*hw_delay = (final_msdu_compl_tsf_tstamp_us -
			final_msdu_tqm_enqueue_tstamp_us) & HW_TX_DELAY_MASK;
}

/* reinject_pkt stats - needs to be implemented */
void ath12k_qos_stats_update(struct ath12k *ar, struct sk_buff *skb,
			     struct hal_tx_status *ts,
			     struct ath12k_pdev_dp *dp_pdev,
			     ktime_t timestamp)
{
	struct ath12k_dp_link_peer *link_peer = NULL, *pri_peer = NULL;
	struct ath12k_dp_peer *mld_peer;
	struct ath12k_dp *dp = NULL;
	struct ath12k_dp_hw *dp_hw = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_mld_qos_stats *mld_qos;
	struct tx_stats *qos_tx;
	struct delay_stats *qos_delay;
	void *telemetry_peer_ctx = NULL;
	u64 enqueue_timestamp, total_delay_pkts, tmp_div;
	u32 len, q_id, tid, hw_delay, nw_delay, sw_delay, delay_bound;
	u32 pkt_win, num_pkts, dropped_age_out = 0;
	u16 msduq_id;
	u8 link_id, pri_link_id, qos_id;
	bool update_pri_peer = false;

	if (!ts || !dp_pdev)
		return;

	if (!(skb->mark & QOS_VALID_TAG))
		return;

	msduq_id = u32_get_bits(skb->mark, SDWF_MSDUQ_ID);
	if (msduq_id >= QOS_MSDUQ_MAX &&
	    msduq_id < MSDUQ_MAX_DEF)
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);
	len = skb->len;

	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) & ATH12K_QOS_STATS_BASIC))
		return;

	mld_peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp,
						       dp_pdev,
						       ts->peer_id);
	if (!mld_peer) {
		ath12k_err(ar->ab, "MLD peer NA with peer_id: %u\n",
			   ts->peer_id);
		return;
	}

	if (mld_peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS) {
		/* primary link only */
		link_id = mld_peer->hw_links[dp_pdev->hw_link_id];
	} else {
		link_id = ath12k_dp_get_link_id(dp_pdev, ts->hw_link_id,
						mld_peer);
	}

	if (link_id < ATH12K_NUM_MAX_LINKS) {
		link_peer = rcu_dereference(mld_peer->link_peers[link_id]);
		if (!link_peer) {
			ath12k_err(ar->ab, "link peer not present with link_id: %u\n",
				   link_id);
			return;
		}
		if (ath12k_dp_peer_get_vif(mld_peer)) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(mld_peer));
		} else {
			ath12k_err(ar->ab, "vif not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (ahvif) {
			arvif = rcu_dereference(ahvif->link[link_id]);
		} else {
			ath12k_err(ar->ab, "ath12k vif not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (arvif && arvif->ar) {
			dp = arvif->ar->ab->dp;
			dp_hw = arvif->ar->dp.dp_hw;
		} else {
			ath12k_err(ar->ab, "link vif or link vif radio not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (!dp || !dp_hw) {
			ath12k_err(ar->ab, "dp or dp_hw not present %u\n",
				   link_id);
			return;
		}
	} else {
		ath12k_err(ar->ab, "link peer NA with link_id: %u\n",
			   link_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	spin_lock_bh(&dp_hw->peer_lock);

	mld_qos = &mld_peer->mld_qos_stats[tid][q_id];

	if (!link_peer->peer_stats.qos_stats) {
		spin_unlock_bh(&dp_hw->peer_lock);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	qos_tx = &link_peer->peer_stats.qos_stats->qos_tx[tid][q_id];

	switch (ts->status) {
	case HAL_WBM_TQM_REL_REASON_FRAME_ACKED:
		mld_qos->tx_success_pkts++;
		qos_tx->tx_success.num++;
		qos_tx->tx_success.bytes += len;
		if (ts->transmit_cnt > 1) {
			qos_tx->total_retries_count += (ts->transmit_cnt - 1);
			qos_tx->retry_count++;
			if (ts->transmit_cnt > 2)
				qos_tx->multiple_retry_count++;
		}
		ath12k_sdwf_update_peer_mcs_stats(qos_tx, ts);
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
		qos_tx->dropped.fw_rem.num++;
		qos_tx->dropped.fw_rem.bytes += len;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
		qos_tx->dropped.fw_rem_tx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX:
		qos_tx->dropped.fw_rem_notx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
		qos_tx->dropped.age_out++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1:
		qos_tx->dropped.fw_reason1++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2:
		qos_tx->dropped.fw_reason2++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3:
		qos_tx->dropped.fw_reason3++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE:
		qos_tx->dropped.fw_rem_queue_disable++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING:
		qos_tx->dropped.fw_rem_no_match++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
		qos_tx->dropped.drop_threshold++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL:
		qos_tx->dropped.drop_link_desc_na++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU:
		qos_tx->dropped.invalid_drop++;
		break;
	case HAL_WBM_TQM_REL_REASON_MULTICAST_DROP:
		qos_tx->dropped.mcast_vdev_drop++;
		break;
	default:
		qos_tx->dropped.invalid_rr++;
		break;
	}

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		qos_tx->tx_failed.num++;
		qos_tx->tx_failed.bytes += len;
		mld_qos->tx_failed_pkts++;
		if (ts->transmit_cnt > DP_RETRY_COUNT)
			qos_tx->failed_retry_count++;
	}

	pri_link_id = mld_peer->hw_links[dp_pdev->hw_link_id];

	if (link_id == pri_link_id)
		qos_tx->queue_depth--;
	else
		update_pri_peer = true;

	ath12k_telemetry_get_sla_num_pkts(&num_pkts);
	if (mld_peer->qos)
		telemetry_peer_ctx = mld_peer->qos->telemetry_peer_ctx;

	tmp_div = mld_qos->tx_success_pkts + mld_qos->tx_failed_pkts;
	if ((!(do_div(tmp_div, num_pkts))) &&
	    telemetry_peer_ctx) {
		if (mld_peer->qos_stats_lvl ==
		    ATH12K_QOS_SINGLE_LINK_STATS) {
			dropped_age_out = qos_tx->dropped.age_out;
		} else {
			struct ath12k_dp_link_peer *tmp_peer = NULL;
			struct tx_stats *tmp_qos_tx = NULL;
			unsigned long peer_links_map, scan_links_map;
			u8 tmp_link_id;

			peer_links_map = mld_peer->peer_links_map;
			scan_links_map = ATH12K_SCAN_LINKS_MASK;

			for_each_andnot_bit(tmp_link_id, &peer_links_map,
					    &scan_links_map,
					    ATH12K_NUM_MAX_LINKS) {
				tmp_peer = rcu_dereference(mld_peer->link_peers[tmp_link_id]);
				if (!tmp_peer ||
				    !tmp_peer->peer_stats.qos_stats) {
					continue;
				}

				tmp_qos_tx = &tmp_peer->peer_stats.qos_stats->qos_tx[tid][q_id];
				dropped_age_out += tmp_qos_tx->dropped.age_out;
				tmp_peer = NULL;
			}
		}
		ath12k_telemetry_update_msdu_drop(telemetry_peer_ctx, tid, msduq_id,
						  mld_qos->tx_success_pkts,
						  mld_qos->tx_failed_pkts,
						  dropped_age_out);
	}

	qos_delay = &link_peer->peer_stats.qos_stats->qos_delay[tid][q_id];

	ath12k_sdwf_compute_hw_delay(ar, ts, &hw_delay);
	if (hw_delay > HW_TX_DELAY_MAX) {
		mld_qos->tx_invalid_delay_pkts++;
		qos_delay->invalid_delay_pkts++;
		goto out;
	}

	mld_qos->hwdelay_win_total += hw_delay;
	ath12k_update_hist_stats(&qos_delay->delay_hist, hw_delay);

	nw_delay = u32_get_bits(skb->mark, QOS_NW_DELAY);
	mld_qos->nwdelay_win_total += nw_delay;

	enqueue_timestamp = ktime_to_us(timestamp);

	if (!enqueue_timestamp)
		sw_delay = 0;
	else
		sw_delay = (u32)(enqueue_timestamp);

	mld_qos->swdelay_win_total += sw_delay;

	ath12k_telemetry_get_sla_mov_avg_num_pkt(&pkt_win);
	if (!pkt_win)
		pkt_win = ATH12K_MOV_AVG_PKT_WIN;

	total_delay_pkts = mld_qos->tx_success_pkts +
			   mld_qos->tx_failed_pkts -
			   mld_qos->tx_invalid_delay_pkts;
	tmp_div = total_delay_pkts;

	if (telemetry_peer_ctx && !(do_div(tmp_div, pkt_win))) {
		u32 nwdelay_avg, hwdelay_avg, swdelay_avg;

		nwdelay_avg = div_u64(mld_qos->nwdelay_win_total,
				      pkt_win);
		swdelay_avg = div_u64(mld_qos->swdelay_win_total,
				      pkt_win);
		hwdelay_avg = div_u64(mld_qos->hwdelay_win_total,
				      pkt_win);
		mld_qos->nwdelay_win_total = 0;
		mld_qos->swdelay_win_total = 0;
		mld_qos->hwdelay_win_total = 0;

		ath12k_telemetry_update_delay_mvng(telemetry_peer_ctx,
						   tid, msduq_id,
						   nwdelay_avg,
						   swdelay_avg,
						   hwdelay_avg);
	}

	if (!mld_peer->qos) {
		ath12k_err(ar->ab, "link peer's qos not present\n");
		goto out;
	}

	qos_id = mld_peer->qos->msduq_map[tid][q_id].qos_id;

	if (ath12k_get_qos_params_delay_bound(arvif->ar->ab, qos_id,
					      &delay_bound)) {
		if (hw_delay > (delay_bound *
				ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER))
			qos_delay->delay_failure++;
		else
			qos_delay->delay_success++;

		tmp_div = total_delay_pkts;
		if (!(do_div(tmp_div, num_pkts)) && telemetry_peer_ctx) {
			u64 delay_success = 0, delay_failure = 0;

			if (mld_peer->qos_stats_lvl ==
			    ATH12K_QOS_SINGLE_LINK_STATS) {
				delay_success = qos_delay->delay_success;
				delay_failure = qos_delay->delay_failure;
			} else {
				struct ath12k_dp_link_peer *tmp_peer = NULL;
				struct delay_stats *tmp_qos_delay = NULL;
				unsigned long peer_links_map, scan_links_map;
				u8 tmp_link_id;

				peer_links_map = mld_peer->peer_links_map;
				scan_links_map = ATH12K_SCAN_LINKS_MASK;

				for_each_andnot_bit(tmp_link_id, &peer_links_map,
						    &scan_links_map,
						    ATH12K_NUM_MAX_LINKS) {
					tmp_peer = rcu_dereference(mld_peer->link_peers[tmp_link_id]);
					if (!tmp_peer ||
					    !tmp_peer->peer_stats.qos_stats) {
						continue;
					}

					tmp_qos_delay = &tmp_peer->peer_stats.qos_stats->qos_delay[tid][q_id];
					delay_success += tmp_qos_delay->delay_success;
					delay_failure += tmp_qos_delay->delay_failure;
					tmp_peer = NULL;
				}
			}
			ath12k_telemetry_update_delay(telemetry_peer_ctx,
						      tid, msduq_id,
						      delay_success,
						      delay_failure);
		}
	}

out:
	spin_unlock_bh(&dp_hw->peer_lock);
	spin_unlock_bh(&dp->dp_lock);

	if (update_pri_peer) {
		pri_peer = rcu_dereference(mld_peer->link_peers[pri_link_id]);
		if (pri_peer) {
			spin_lock_bh(&dp_pdev->dp->dp_lock);
			if (pri_peer->peer_stats.qos_stats)
				pri_peer->peer_stats.qos_stats->qos_tx[tid][q_id].queue_depth--;
			spin_unlock_bh(&dp_pdev->dp->dp_lock);
		}
	}
}

#define HTT_META_DATA_ALIGNMENT 0x8

/* Preparing HTT Metadata when utilized with ext MSDU */
static int ath12k_wifi8_dp_prepare_htt_metadata(struct sk_buff *skb)
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

static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end) {
#ifndef CONFIG_IO_COHERENCY
#ifndef PLATFORM_SDX85
	dmac_clean_range_no_dsb(start, end);
#endif
#endif
}

#ifdef CONFIG_IO_COHERENCY
static inline void
ath12k_wifi8_dp_tx_populate_tcl_desc(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_dp_link_vif *dp_link_vif,
				     struct ath12k_vif *vlan_ahvif,
				     struct sk_buff *skb,
				     struct hal_tcl_data_cmd *hal_tcl_desc,
				     struct ath12k_tx_desc_info *tx_desc,
				     u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	u16 ast_idx;
	u16 ast_hash;
	bool ast_overwrite = false;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		ast_idx = dp_vlan_vif->ast_idx;
		ast_hash = dp_vlan_vif->ast_hash;
		ast_overwrite = true;
	} else {
		ast_idx = dp_vif->ast_idx;
		ast_hash = dp_vif->ast_hash;
	}

	hal_tcl_desc->buf_addr_info.info0 = (u32)virt_to_phys(skb->data);
	hal_tcl_desc->buf_addr_info.info1 =
				(((u64)virt_to_phys(skb->data) >> 32) |
				(tx_desc->desc_id << 12));
	hal_tcl_desc->info0 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
					 dp_vif->bank_id) |
			      FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_VDEV_ID,
					 dp_vif->dp_vif_id) |
			      FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH, skb->len);

	hal_tcl_desc->info1 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM, ast_hash) |
			      FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE,
					 ast_overwrite);
	hal_tcl_desc->search_index = ast_idx;
	hal_tcl_desc->tcl_cmd_number = dp_link_vif->tcl_metadata;
	hal_tcl_desc->info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_LINK_ID,
					 HAL_TX_WILD_CARD_LINK_ID);
	hal_tcl_desc->info5 = 0;

	/**
	 * Check if the vif supports mscs hlos tid override, which
	 * will be true if there is an active MSCS session
	 * In this case, check for skb->priority which would have
	 * the correct tid value and program it to the TCL metadata
	 * For accelerated packets with MSCS, skb->mark will not be
	 * set
	 */
	if (unlikely(skb->priority &&
		     dp_vif->mscs_hlos_tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, skb->priority);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arvif, &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}
}
#else
static inline void
ath12k_wifi8_dp_tx_populate_tcl_desc(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_dp_link_vif *dp_link_vif,
				     struct ath12k_vif *vlan_ahvif,
				     struct sk_buff *skb,
				     struct hal_tcl_data_cmd *hal_tcl_desc,
				     struct ath12k_tx_desc_info *tx_desc,
				     u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct hal_tcl_data_cmd tcl_desc = {0};
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	u16 ast_idx;
	u16 ast_hash;
	bool ast_overwrite = false;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		ast_idx = dp_vlan_vif->ast_idx;
		ast_hash = dp_vlan_vif->ast_hash;
		ast_overwrite = true;
	} else {
		ast_idx = dp_vif->ast_idx;
		ast_hash = dp_vif->ast_hash;
	}


	tcl_desc.buf_addr_info.info0 = (u32)virt_to_phys(skb->data);
	tcl_desc.buf_addr_info.info1 = (((u64)virt_to_phys(skb->data) >> 32) |
				       (tx_desc->desc_id << 12));
	tcl_desc.info0 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
				    dp_vif->bank_id) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_VDEV_ID,
				    dp_vif->dp_vif_id) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH, skb->len);

	tcl_desc.info1 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM, ast_hash) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE,
				    ast_overwrite);
	tcl_desc.search_index = ast_idx;
	tcl_desc.tcl_cmd_number = dp_link_vif->tcl_metadata;
	tcl_desc.info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_LINK_ID,
				    HAL_TX_WILD_CARD_LINK_ID);
	tcl_desc.info5 = 0;

	/**
	 * Check if the vif supports mscs hlos tid override, which
	 * will be true if there is an active MSCS session
	 * In this case, check for skb->priority which would have
	 * the correct tid value and program it to the TCL metadata
	 * For accelerated packets with MSCS, skb->mark will not be
	 * set
	 */
	if (unlikely(skb->priority &&
		     dp_vif->mscs_hlos_tid_override))
		ath12k_wifi_qos_hlos_tid(&tcl_desc, skb->priority);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, &tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arvif, &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}
	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
}
#endif

enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx_fast(struct ath12k_pdev_dp *dp_pdev,
			struct ath12k_link_vif *arvif,
			struct ath12k_vif *vlan_ahvif,
			struct sk_buff *skb,
			u32 qos_nw_delay)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_srng *tcl_ring;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[arvif->link_id];
	struct dp_tx_ring *tx_ring;
	u8 pool_id;
	u8 hal_ring_id;
	u8 tid;
	bool is_from_recycler;
	bool stats_disable = ab->stats_disable;
	u8 ring_id = smp_processor_id();

	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return DP_TX_ENQ_DROP_CRASH_FLUSH;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags)) {
		kfree_skb(skb);
		return DP_TX_ENQ_SUCCESS;
	}

	pool_id = skb_get_queue_mapping(skb) & (ATH12K_HW_MAX_QUEUES - 1);

	tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
					     dp->dp_hw_grp->tx_desc_free_list,
					     ring_id);
	if (unlikely(!tx_desc)) {
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_BUF_ERR);
		}
		dp->device_stats.tx_err.txbuf_na[ring_id]++;
		return DP_TX_ENQ_DROP_SW_DESC_NA;
	}

	ath12k_core_dma_clean_range_no_dsb(skb->data, skb->data + DP_TX_SFE_BUFFER_SIZE);

	/* the edma driver uses this flags to optimize the cache invalidation */
	is_from_recycler = (skb->fast_recycled = !!skb->is_from_recycler);
	if (likely(is_from_recycler))
		tx_desc->flags = (DP_TX_DESC_FLAG_FAST & stats_disable);
	else
		tx_desc->flags = 0;

	tx_desc->skb = skb;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;

	tx_ring = &dp->tx_ring[ring_id];
	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	hal_tcl_desc =
	(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);
	if (unlikely(!hal_tcl_desc)) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		dp->device_stats.tx_err.desc_na[ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_DESC_ERR);
		}
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);
		return DP_TX_ENQ_DROP_TCL_DESC_NA;
	}

	ath12k_wifi8_dp_tx_populate_tcl_desc(dp_pdev, arvif,
					     dp_link_vif,
					     vlan_ahvif,
					     skb, hal_tcl_desc,
					     tx_desc, qos_nw_delay);
	dmb(oshst);
	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	if (unlikely(ath12k_dp_stats_enabled(dp_pdev) &&
		     ath12k_tid_stats_enabled(dp_pdev))) {
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
		ath12k_tid_tx_stats(ahvif, tid, skb->len,
				    ATH_TX_FAST_UNICAST);
	}
	dp->device_stats.tx_fast_unicast[ring_id]++;

	DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, skb->len, ring_id);
	atomic_inc(&dp_pdev->num_tx_pending);

	return DP_TX_ENQ_SUCCESS;
}

/* TODO: Remove the export once this file is built with wifi8 ko */
enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx(struct ath12k_pdev_dp *dp_pdev,
		   struct ath12k_link_vif *arvif,
		   struct sk_buff *skb, bool gsn_valid, int mcbc_gsn,
		   bool is_mcast, struct ath12k_link_sta *arsta, u8 ring_id,
		   u32 qos_nw_delay)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct ath12k_base *ab = dp->ab;
	struct hal_tx_info ti = {0};
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tx_msdu_extension *msg;
	struct sk_buff *skb_ext_desc = NULL;
	struct ethhdr *eth = NULL;
	struct hal_srng *tcl_ring;
	struct ieee80211_hdr *hdr = NULL;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[arvif->link_id];
	struct dp_tx_ring *tx_ring;
	u8 pool_id;
	u8 hal_ring_id;
	int ret;
	u8 reason, tid;
	u8 ring_selector, subtype;
	bool msdu_ext_desc = false;
	size_t hdrlen;
	bool add_htt_metadata = false;
	u32 iova_mask = dp->hw_params->iova_mask;
	bool is_diff_encap = false, is_null = false;
	u8 qos_tag;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp_peer *dp_peer = NULL;
	enum ath12k_dp_tx_enq_error err = DP_TX_ENQ_SUCCESS;

	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return DP_TX_ENQ_DROP_CRASH_FLUSH;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags)) {
		kfree_skb(skb);
		return err;
	}

	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP)
		eth = (struct ethhdr *)skb->data;
	else
		hdr = (void *)skb->data;

	if (!(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !ieee80211_is_data(hdr->frame_control))
		return DP_TX_ENQ_DROP_NON_DATA_FRAME;

	ti.meta_data_flags = dp_link_vif->tcl_metadata;
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		ti.bss_ast_hash = dp_vif->ast_hash;
		ti.bss_ast_idx = dp_vif->ast_idx;
	} else if (arsta) {
		ahsta = arsta->ahsta;
		if (ahsta->use_4addr_set) {
			rcu_read_lock();
			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
								      ahsta->dp_peer_id);
			if (!dp_peer) {
				rcu_read_unlock();
				return DP_TX_ENQ_DROP_INV_PEER;
			}

			ti.bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
			ti.bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
			ti.lookup_override = true;
			ti.meta_data_flags =
			u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
					u32_encode_bits(dp_peer->peer_id,
							HTT_TCL_META_DATA_PEER_ID);
			rcu_read_unlock();
		}
	} else if (is_mcast) {
		ti.bss_ast_hash = dp_link_vif->ast_hash;
		ti.bss_ast_idx = dp_link_vif->ast_idx;
		ti.lookup_override = true;
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

	ti.ring_id = ring_selector % dp->hw_params->max_tx_ring;

	ti.rbm_id = hal->tcl_to_cmp_rbm_map[ti.ring_id].rbm_id;

	tx_ring = &dp->tx_ring[ti.ring_id];

	if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
		tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
						     dp->dp_hw_grp->tx_spl_desc_free_list,
						     ti.ring_id);
	else
		tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
						     dp->dp_hw_grp->tx_desc_free_list,
						     ti.ring_id);

	if (!tx_desc) {
		dp->device_stats.tx_err.txbuf_na[ti.ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_BUF_ERR);
		}
		return DP_TX_ENQ_DROP_SW_DESC_NA;
	}

	ti.bank_id = dp_vif->bank_id;

	if (gsn_valid && !arsta) {
		/* Reset and Initialize meta_data_flags with Global Sequence
		 * Number (GSN) info.
		 */
		ti.meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					GENMASK(15, 14)) |
			u32_encode_bits(mcbc_gsn, GENMASK(11, 0));

		ti.tx_notify_frame = 6;
		if (arvif->nawds_support)
			ti.meta_data_flags |=
				u32_encode_bits(1, BIT(12));
	}

	ti.encap_type = ath12k_dp_tx_get_encap_type(ab, skb);
	ti.addr_search_flags = dp_vif->hal_addr_search_flags;
	ti.search_type = dp_vif->search_type;
	ti.type = HAL_TCL_DESC_TYPE_BUFFER;
	ti.pkt_offset = 0;
	ti.link_id = HAL_TX_WILD_CARD_LINK_ID;

	ti.vdev_id = dp_vif->dp_vif_id;
	if (gsn_valid)
		ti.vdev_id += HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
	else if (arvif->nawds_support && is_mcast && !ti.lookup_override)
		ti.meta_data_flags |=
			u32_encode_bits(1, HTT_TCL_META_DATA_HOST_INSPECTED_MISSION);

	ti.dscp_tid_tbl_idx = 0;

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
			err = DP_TX_ENQ_DROP_ENCAP_RAW;
			goto fail_remove_tx_buf;
		}
		break;
	case HAL_TCL_ENCAP_TYPE_ETHERNET:
		/* no need to encap */
		break;
	case HAL_TCL_ENCAP_TYPE_802_3:
	default:
		/* TODO: Take care of other encap modes as well */
		err = DP_TX_ENQ_DROP_ENCAP_802_3;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, 0, ATH_TX_MISC_FAIL);
		}
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
		err = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail_remove_tx_buf;
	}
#else
	ti.paddr = virt_to_phys(skb->data);
	if (!ti.paddr) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		ath12k_warn(ab, "failed to DMA map data Tx buffer\n");
		err = DP_TX_ENQ_DROP_DMA_ERR;
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
		ti.flags0 |= u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_TO_FW_SW);
		ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	tx_desc->skb = skb;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;
	ti.desc_id = tx_desc->desc_id;
	ti.data_len = skb->len;

	tx_desc->paddr = ti.paddr;
	tx_desc->len = ti.data_len;

	tx_desc->paddr_ext_desc = 0;

	if (msdu_ext_desc) {
		skb_ext_desc = dev_alloc_skb(sizeof(struct hal_tx_msdu_extension));
		if (!skb_ext_desc) {
			err = DP_TX_ENQ_DROP_EXT_DESC_NA;
			goto fail_unmap_dma;
		}

		skb_put(skb_ext_desc, sizeof(struct hal_tx_msdu_extension));
		memset(skb_ext_desc->data, 0, skb_ext_desc->len);

		msg = (struct hal_tx_msdu_extension *)skb_ext_desc->data;
		ath12k_wifi8_hal_tx_cmd_ext_desc_setup(ab, msg, &ti);

		if (add_htt_metadata) {
			ret = ath12k_wifi8_dp_prepare_htt_metadata(skb_ext_desc);
			if (ret < 0) {
				ath12k_dbg(ab, ATH12K_DBG_DP_TX,
					   "Failed to add HTT meta data, dropping packet\n");
				err = DP_TX_ENQ_DROP_HTT_MDATA_ERR;
				goto fail_free_ext_skb;
			}
		}
#ifndef CONFIG_IO_COHERENCY
		ti.paddr = dma_map_single(dp->dev, skb_ext_desc->data,
					  skb_ext_desc->len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dp->dev, ti.paddr);
		if (ret) {
			err = DP_TX_ENQ_DROP_DMA_ERR;
			goto fail_free_ext_skb;
		}
#else
		ti.paddr = virt_to_phys(skb_ext_desc->data);
		if (!ti.paddr) {
			err = DP_TX_ENQ_DROP_DMA_ERR;
			goto fail_free_ext_skb;
		}
#endif
		ti.data_len = skb_ext_desc->len;
		ti.type = HAL_TCL_DESC_TYPE_EXT_DESC;

		tx_desc->paddr_ext_desc = ti.paddr;
		tx_desc->ext_desc_len = ti.data_len;
		tx_desc->skb_ext_desc = skb_ext_desc;
	}

	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	ath12k_hal_srng_access_begin_no_lock(tcl_ring);
	hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(ab, tcl_ring);
	if (!hal_tcl_desc) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath12k_hal_srng_access_end_no_lock(ab, tcl_ring);
		dp->device_stats.tx_err.desc_na[ti.ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, 0,
						 ATH_TX_DESC_NA_ERR);
		}
		err = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail_unmap_dma_ext;
	}

	spin_lock_bh(&arvif->link_stats_lock);
	if (is_mcast) {
		reason = ATH_TX_MCAST_PKTS;
		ab->dp->device_stats.tx_mcast[ti.ring_id]++;
	} else if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
		ab->dp->device_stats.tx_eapol[ti.ring_id]++;
		reason = ATH_TX_EAPOL_PKTS;
		if (ti.encap_type == HAL_TCL_ENCAP_TYPE_NATIVE_WIFI) {
			hdr = (struct ieee80211_hdr *)skb->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(skb);
			subtype = ath12k_dp_get_eapol_subtype(skb->data + hdrlen +
							      LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				ab->dp->device_stats.tx_eapol_type[subtype - 1][ti.ring_id]++;
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Transmit %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 hdr->addr1);
			}
		} else {
			eth = (struct ethhdr *)skb->data;
			subtype = ath12k_dp_get_eapol_subtype(skb->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				ab->dp->device_stats.tx_eapol_type[subtype - 1][ti.ring_id]++;
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Transmit %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 eth->h_dest);
			}
		}

	} else if (is_null) {
		ab->dp->device_stats.tx_null_frame[ti.ring_id]++;
		reason = ATH_TX_NULL_PKTS;
	} else {
		ab->dp->device_stats.tx_unicast[ti.ring_id]++;
		reason = ATH_TX_UNICAST_PKTS;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
		ath12k_tid_tx_stats(ahvif, tid, skb->len, reason);
	}

	arvif->link_stats.tx_encap_type[ti.encap_type]++;
	arvif->link_stats.tx_encrypt_type[ti.encrypt_type]++;
	arvif->link_stats.tx_desc_type[ti.type]++;

	if (is_mcast)
		arvif->link_stats.tx_bcast_mcast++;
	else
		arvif->link_stats.tx_enqueued++;
	spin_unlock_bh(&arvif->link_stats_lock);

	if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			if (is_mcast) {
				eth = (struct ethhdr *)skb->data;
				if (eth && is_broadcast_ether_addr(eth->h_dest))
					tx_desc->flags |= DP_TX_DESC_FLAG_BCAST;
				else
					tx_desc->flags |= DP_TX_DESC_FLAG_MCAST;
				DP_STATS_INC_PKT(dp_vif, tx_i.mcast, 1, skb->len,
						 ti.ring_id);
			}
			DP_STATS_INC(dp_vif, tx_i.encap_type[ti.encap_type], 1,
				     ti.ring_id);
			DP_STATS_INC(dp_vif, tx_i.encrypt_type[ti.encrypt_type], 1,
				     ti.ring_id);
			DP_STATS_INC(dp_vif, tx_i.desc_type[ti.type], 1, ti.ring_id);
		}
	}

	ath12k_wifi8_hal_tx_cmd_desc_setup(ab, hal_tcl_desc, &ti);

	/* For SDWF DS support, either the slow packet or the
	 * reinject packets would not have skb->fast_xmit set
	 * and the msduq information should be updated if the
	 * SDWF is valid in skb->mark
	 */
	if (unlikely(skb->mark & SDWF_VALID_MASK))
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);

	if (unlikely(arsta)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_dp_qos_update(dp, dp_pdev, skb->mark,
					     hal_tcl_desc,
					     qos_tag, arsta->addr);
	}

	ath12k_hal_srng_access_end_no_lock(ab, tcl_ring);

	DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw, 1, ti.data_len, ti.ring_id);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_TX, NULL, "dp tx msdu: ",
			skb->data, skb->len);

	atomic_inc(&dp_pdev->num_tx_pending);

	return DP_TX_ENQ_SUCCESS;

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
	return err;
}

static inline void
ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(struct hal_tx_status *ts,
					       struct ath12k_dp *dp)
{
	ts->hw_link_id = (DP_GET_HW_LINK_ID_FRM_PPDU_ID(ts->ppdu_id,
							dp->link_id_offset,
							dp->link_id_bits));
}

static void ath12k_wifi8_dp_tx_free_txbuf(struct ath12k_dp *dp,
					  struct sk_buff *msdu,
					  struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr,
				     sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_htt_tx_complete_buf(struct ath12k_dp *dp,
				       struct sk_buff *msdu,
				       struct dp_tx_ring *tx_ring,
				       struct hal_tx_status *ts,
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
	struct ethhdr *eth;
	struct ieee80211_hdr *hdr;
	size_t hdrlen;
	enum ath12k_dp_eapol_key_type subtype;

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr,
				     sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

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
		if (dp_pdev->wmm_stats.tx_type) {
			ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
			if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
				ahvif->wmm_stats.total_wmm_tx_drop[ahvif->wmm_stats.tx_type]++;
		}

		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (arvif) {
			spin_lock_bh(&arvif->link_stats_lock);
			arvif->link_stats.tx_completed++;
			spin_unlock_bh(&arvif->link_stats_lock);
		}
	}

	if (msdu->protocol == cpu_to_be16(ETH_P_PAE)) {
		if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
			eth = (struct ethhdr *)msdu->data;
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 eth->h_dest);
		} else {
			hdr = (struct ieee80211_hdr *)msdu->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(msdu);
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + hdrlen +
							      LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 hdr->addr1);
		}
	}

	memset(&info->status, 0, sizeof(info->status));

	if (ts->acked) {
		if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		} else {
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
		}
	}

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer))
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n", peer_id);
	else
		status.sta = ath12k_dp_link_peer_get_sta(peer);

	if ((unlikely(ath12k_dp_stats_enabled(dp_pdev))) &&
	    (unlikely(ath12k_debugfs_is_qos_stats_enabled(dp_pdev->ar)))) {
		ath12k_qos_stats_update(dp_pdev->ar, msdu, ts, dp_pdev,
					msdu->tstamp);
	}

	status.info = info;
	status.skb = msdu;
	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_process_htt_tx_complete(struct ath12k_dp *dp,
					   void *desc, struct sk_buff *msdu,
					   struct dp_tx_ring *tx_ring,
					   struct ath12k_tx_sw_metadata *sw_metadata,
					   struct hal_tx_status *ts,
					   int ring_id, u32 htt_status)
{
	struct htt_tx_completion *status_desc;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	status_desc = desc;

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	ts->ppdu_id = le32_get_bits(status_desc->info2,
				    HTT_TX_WBM_COMP_INFO2_PPDU_ID);
	ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(ts, dp);

	if (le32_get_bits(status_desc->info3, HTT_TX_WBM_COMP_INFO3_VALID)) {
		ts->peer_id = le32_get_bits(status_desc->info3,
					    HTT_TX_WBM_COMP_INFO3_SW_PEER_ID);
	} else {
		ts->peer_id = HAL_INVALID_PEERID;
	}

	switch (htt_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
		ts->acked = (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts->ack_rssi = le32_get_bits(status_desc->info2,
					     HTT_TX_WBM_COMP_INFO2_ACK_RSSI);

		ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
		ath12k_wifi8_dp_tx_htt_tx_complete_buf(dp, msdu, tx_ring, ts,
						       sw_metadata, ts->peer_id);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
		switch (htt_status) {
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
			break;
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
			fallthrough;
		default:
			break;
		}
		fallthrough;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH:
		ath12k_wifi8_dp_tx_free_txbuf(dp, msdu, sw_metadata);
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

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer) {
		link_id = ath12k_dp_peer_get_stats_link_id(dp->ab, peer,
							   ts->hw_link_id);
		ath12k_dp_tx_update_peer_basic_stats(peer, msdu_len,
						     htt_status,
						     link_id, ring_id);
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_dp_tx_comp_update_peer_stats(peer, ts,
								    ring_id,
								    tx_desc_flags,
								    link_id,
								    msdu_len);
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring_id], 1);
	}
	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_cache_peer_stats(struct ath12k *ar,
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
ath12k_wifi8_dp_tx_update_txcompl(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_link_sta *arsta;
	struct rate_info txrate = {0};
	u16 rate, ru_tones;
	u8 rate_idx = 0;
	int ret;
	u8 link_id = 0;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (!dp_peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "MLD peer NA with peer_id: %u\n", ts->peer_id);
		return;
	}

	link_id = ath12k_dp_get_link_id(dp_pdev, ts->hw_link_id, dp_peer);

	spin_lock_bh(&dp->dp_lock);
	peer = rcu_dereference(dp_peer->link_peers[link_id]);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	sta = ath12k_dp_link_peer_get_sta(peer);
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

		if (txrate.nss < 1 ||
		    (dp_pdev->ar->pdev->cap.max_tx_nss &&
		     (txrate.nss > dp_pdev->ar->pdev->cap.max_tx_nss)) ||
		      (txrate.nss > hweight32(dp_pdev->ar->pdev->cap.tx_chain_mask)))
			ath12k_warn(ab, "Invalid nss value: %d", txrate.nss);
		else
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
	case HAL_TX_RATE_STATS_PKT_TYPE_11BN:
		if (ts->mcs > ATH12K_UHR_MCS_MAX) {
			ath12k_warn(ab, "Invalid UHR mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		/* TODO: This has to be changed to UHR mcs */
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
	peer = rcu_dereference(dp_peer->link_peers[link_id]);
	if (peer)
		peer->txrate = txrate;
	else
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_wifi8_dp_tx_complete_msdu(struct ath12k_pdev_dp *dp_pdev,
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
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_dp_link_peer *link_peer;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	struct ath12k *ar;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u8 reason = 0;
	u8 tid = 0;
	enum ath12k_dp_tx_comp_error drop_reason = DP_TX_COMP_ERR_MISC;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_TQM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	if (sw_metadata->skb)
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr,
					     sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		return;

	dp_pdev->wmm_stats.tx_type =
		ath12k_tid_to_ac(ts->tid > ATH12K_DSCP_PRIORITY ? 0 : ts->tid);
	if (dp_pdev->wmm_stats.tx_type) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
			dp_pdev->wmm_stats.total_wmm_tx_drop[dp_pdev->wmm_stats.tx_type]++;
	}

	skb_cb = ATH12K_SKB_CB(msdu);

	rcu_read_lock();

	if (!rcu_dereference(ab->pdevs_active[dp_pdev->mac_id])) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_PDEV;
		goto exit;
	}

	if (!skb_cb->vif) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_VIF;
		goto exit;
	}

	vif = skb_cb->vif;
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, msdu->len,
					    ATH_TX_COMPLETED_PKTS);
		}
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

	ar = dp_pdev->ar;

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer) {
		link_id = ath12k_dp_peer_get_stats_link_id(dp->ab, peer,
							   ts->hw_link_id);
		ath12k_dp_tx_update_peer_basic_stats(peer, msdu_len, ts->status,
						     link_id, ring);

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_dp_tx_comp_update_peer_stats(peer, ts,
								    ring,
								    tx_desc_flags,
								    link_id,
								    msdu_len);
			if (unlikely(ath12k_debugfs_is_qos_stats_enabled(ar)))
				ath12k_qos_stats_update(ar, msdu, ts,
							dp_pdev,
							msdu->tstamp);
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring], 1);
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !(info->flags & IEEE80211_TX_CTL_NO_ACK))	{
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ts->ack_rssi;

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
			reason = ATH_TX_TQM_REMOVE_MPDU;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_TQM_THRESHOLD;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_TQM_REMOVE_AGED;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_TQM_REMOVE_TX;
			dev_kfree_skb_any(msdu);
			goto exit;
		default:
			/* TODO: Remove this print and add as a stats */
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts->status);
		}
		if (ahvif && ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev))
			ath12k_tid_tx_drop_stats(ahvif, ts->tid, msdu->len, reason);
	}

	/* NOTE: Tx rate status reporting. Tx completion status does not have
	 * necessary information (for example nss) to build the tx rate.
	 * Might end up reporting it out-of-band from HTT stats.
	 */

	if (ath12k_extd_tx_stats_enabled(ar)) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath12k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath12k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath12k_wifi8_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

	ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);

	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
							     ts->peer_id);
	if (!link_peer || !ath12k_dp_link_peer_get_sta(link_peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   ts->peer_id);
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_LINK_PEER;
		goto exit;
	}

	status.sta = ath12k_dp_link_peer_get_sta(link_peer);
	status.info = info;
	status.skb = msdu;
	rate = link_peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;

	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
	return;

exit:
	DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[drop_reason][ring], 1);
	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_status_parse(struct ath12k_base *ab,
				struct hal_tqm2sw_completion_ring *desc,
				struct hal_tx_status *ts)
{
	u32 info0 = le32_to_cpu(desc->rate_stats.info0);

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
	    ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)
		return;

	ts->ppdu_id = le32_get_bits(desc->info1,
				    HAL_TQM2SW_COMPLETION_RING_INFO1_TQM_STATUS_NUMBER);

	ts->ack_rssi = FIELD_GET(HAL_TQM2SW_COMPLETION_RING_INFO2_ACK_FRAME_RSSI,
				 desc->info2);

	ts->peer_id = le16_to_cpu(desc->sw_peer_id);

	if (info0 & HAL_TX_RATE_TID_INFO_INFO0_TX_RATE_STATS_INFO_VALID) {
		ts->pkt_type = u32_get_bits(info0,
					    HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_PKT_TYPE);
		ts->mcs = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_MCS);
		ts->sgi = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_SGI);
		ts->bw = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_BW);
		ts->tones = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TONES_IN_RU);
		ts->ofdma = u32_get_bits(info0,
					 HAL_TX_RATE_TID_INFO_INFO0_OFDMA_TRANSMISSION);
		ts->tid = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TID);
	}

	ts->transmit_cnt = le32_get_bits(desc->info1,
					 HAL_TQM2SW_COMPLETION_RING_INFO1_TRANSMIT_COUNT);
	ts->first_msdu = le32_get_bits(desc->info2,
				       HAL_TQM2SW_COMPLETION_RING_INFO2_FIRST_MSDU);
	ts->last_msdu = le32_get_bits(desc->info2,
				      HAL_TQM2SW_COMPLETION_RING_INFO2_LAST_MSDU);
	ts->msdu_part_of_amsdu =
			(ts->first_msdu && ts->last_msdu) ? false : true;

	ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(ts, ab->dp);
}

int ath12k_wifi8_dp_tx_mec_handler(struct ath12k_dp *dp,
				   struct ath12k_tx_desc_info *tx_desc,
				   int status)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	u8 *sa_addr = NULL;
	struct ethhdr *eth = NULL;
	struct sk_buff *skb = tx_desc->skb;
	u8 pdev_id = tx_desc->hw_link_id;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	int ret = 0;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	u8 ring_selector = 0, ring_id = 0;

	/* get the src address from the packet */
	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		sa_addr = eth->h_source;
	} else {
		sa_addr = ieee80211_get_SA((struct ieee80211_hdr *)(skb->data));
	}

	if (status == HAL_WBM_TQM_REL_REASON_TCL_MEC_SEARCH_FAIL_FOR_VDEV) {
		ath12k_core_dma_unmap_single(dp->dev, tx_desc->paddr,
					     tx_desc->len, DMA_TO_DEVICE);
		if (tx_desc->paddr_ext_desc) {
			ath12k_core_dma_unmap_single(dp->dev,
						     tx_desc->paddr_ext_desc,
						     tx_desc->ext_desc_len,
						     DMA_TO_DEVICE);
			dev_kfree_skb_any(tx_desc->skb_ext_desc);
		}
		ath12k_dp_tx_release_txbuf(dp, tx_desc, tx_desc->pool_id);

		rcu_read_lock();
		dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, pdev_id);
		if (!dp_pdev) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: dp_pdev is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}
		if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
			wake_up(&dp_pdev->tx_empty_waitq);

		memcpy(ast_param.mac_addr, sa_addr, ETH_ALEN);
		ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_MEC;
		ret = ath12k_dp_ast_entry_create(dp->dp_hw_grp, &ast_param);
		if (ret && ret != -EALREADY) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "unable to create the mec entry %pM", sa_addr);
			rcu_read_unlock();
			return ret;
		}

		/* reinject the packet back */
		vif = skb_cb->vif;
		if (!vif) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: vif is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}
		ahvif = ath12k_vif_to_ahvif(vif);
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (!arvif) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: arvif is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}
		ring_selector = smp_processor_id();
		ring_id = ring_selector % dp_pdev->dp->hw_params->max_tx_ring;
		ret = ath12k_wifi8_dp_tx(dp_pdev, arvif, skb,
					 false, 0, false, NULL, ring_id, 0);
		if (ret) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: enqueue failure %d", ret);
		}

		rcu_read_unlock();
	} else if (status == HAL_WBM_TQM_REL_REASON_TCL_MEC_KEEP_ALIVE_FOR_VDEV) {
		ret = ath12k_mec_entry_keep_alive_update(dp->dp_hw_grp, sa_addr);
	}

	return ret;
}

static inline bool
ath12k_wifi8_dp_tx_mec_filter(struct ath12k_dp *dp,
			      struct ath12k_tx_desc_info *tx_desc,
			      int status)
{
	int ret;

	if (unlikely(status == HAL_WBM_TQM_REL_REASON_TCL_MEC_SEARCH_FAIL_FOR_VDEV ||
		     status == HAL_WBM_TQM_REL_REASON_TCL_MEC_KEEP_ALIVE_FOR_VDEV)) {
		ret = ath12k_wifi8_dp_tx_mec_handler(dp, tx_desc, status);
		if (ret)
			ath12k_err(dp->ab, "MEC: pkt drop in MEC handling ret = %d", ret);

		return true;
	}

	return false;
}

int ath12k_wifi8_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	struct ath12k_dp_tx_comp_status tx_comp_status;
	int i;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int orig_budget = budget;
	struct ath12k_skb_cb *skb_cb;
	struct sk_buff *msdu = NULL;
	struct ath12k_vif *ahvif = NULL;
	bool fast_flag;
	int pdev_tx_comp_cnt[ATH12K_GROUP_MAX_RADIO] = {0};
	u8 hw_link_id = 0;

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_hal_srng_dst_invalidate_entry(dp, status_ring, valid_entries);
#endif

	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_wifi8_tx_status_entry *tx_status_entry;
	struct ath12k_tx_sw_metadata *sw_metadata;
	u8 n_entry = 0, idx = 0;
	struct list_head desc_free_list;
	struct hal_tqm2sw_completion_ring *tx_status;
	struct sk_buff_head free_list_head;
	int tx_status_idx = smp_processor_id();
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] = {0};
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON] = {0};
	u32 fw_tx_status[MAX_FW_TX_STATUS] = {0};
	u32 htt_status = 0, tx_completed = 0;
	u8 tid = 0;

	INIT_LIST_HEAD(&desc_free_list);
	skb_queue_head_init(&free_list_head);

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)
				dp_hw_grp->tx_status_buf[tx_status_idx];
	while (budget-- &&
	       (tx_status = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL))) {
		ath12k_wifi8_hal_tx_completion_process(tx_status, &tx_comp_status);
		tx_desc = (struct ath12k_tx_desc_info *)((unsigned long)tx_comp_status.tx_desc);
		if (unlikely(!tx_desc)) {
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_DESC][ring_id], 1);
			ath12k_warn(ab, "unable to retrieve tx_desc!");
			continue;
		}

		if (ath12k_wifi8_dp_tx_mec_filter(dp, tx_desc,
						  tx_comp_status.u.htt_status))
			continue;

		tx_status_entry->tx_desc = tx_desc;

		n_entry++;
		memcpy(&tx_status_entry->tx_status, tx_status, sizeof(*tx_status));
		tx_status_entry++;
	}

	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	if (!n_entry)
		return orig_budget - budget;

	spin_lock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)
				dp_hw_grp->tx_status_buf[tx_status_idx];
	for (i = 0; i < n_entry; i++) {
		struct ath12k_wifi8_tx_status_entry *tx_status_entry_next;

		sw_metadata = &tx_status_entry->sw_metadata;
		tx_desc = tx_status_entry->tx_desc;
		tx_status_entry++;

		if ((i + 10) < n_entry) {
			tx_status_entry_next = tx_status_entry + 8;

			prefetch(tx_status_entry_next->tx_desc);
			prefetch((tx_status_entry_next + 1));
		}

		if (unlikely(!tx_desc->in_use)) {
			sw_metadata->skb = NULL;
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_DESC_INUSE][ring_id], 1);
			continue;
		}

		if (likely(!tx_desc->spl_desc))
			list_add_tail(&tx_desc->list, &desc_free_list);
		else
			list_add_tail(&tx_desc->list,
				      &dp->dp_hw_grp->tx_spl_desc_free_list[ring_id]);

		sw_metadata->skb = tx_desc->skb;
		sw_metadata->paddr = tx_desc->paddr;
		sw_metadata->len = tx_desc->len;
		sw_metadata->flags = tx_desc->flags;

		if (unlikely(!(sw_metadata->flags & DP_TX_DESC_FLAG_FAST))) {
			sw_metadata->skb_ext_desc = tx_desc->skb_ext_desc;
			sw_metadata->paddr_ext_desc = tx_desc->paddr_ext_desc;
			tx_desc->skb_ext_desc = NULL;
			tx_desc->paddr_ext_desc = 0;
			sw_metadata->ext_desc_len = tx_desc->ext_desc_len;
		}

		sw_metadata->hw_link_id = tx_desc->hw_link_id;
		pdev_tx_comp_cnt[sw_metadata->hw_link_id]++;

		tx_desc->skb = NULL;
		tx_desc->in_use = false;
		tx_desc->flags = 0;
	}

	list_splice(&desc_free_list, &dp->dp_hw_grp->tx_desc_free_list[ring_id]);

	spin_unlock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	for (hw_link_id = 0; hw_link_id < ATH12K_GROUP_MAX_RADIO; hw_link_id++) {
		if (likely(pdev_tx_comp_cnt[hw_link_id])) {
			rcu_read_lock();
			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
			rcu_read_unlock();

			if (unlikely(!dp_pdev))
				continue;

			if (atomic_sub_and_test(pdev_tx_comp_cnt[hw_link_id],
						&dp_pdev->num_tx_pending))
				wake_up(&dp_pdev->tx_empty_waitq);
		}
	}

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	while (n_entry--) {
		fast_flag = false;
		tx_status = &tx_status_entry->tx_status;
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		if (!sw_metadata->skb)
			continue;

		tx_completed++;
		ts.buf_rel_source =
		    le32_get_bits(tx_status->info0,
				  HAL_TQM2SW_COMPLETION_RING_INFO0_RELEASE_SOURCE_MODULE);

		tx_wbm_rel_source[ts.buf_rel_source]++;

		if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			msdu = sw_metadata->skb;
			skb_cb = ATH12K_SKB_CB(msdu);
			ahvif = ath12k_vif_to_ahvif(skb_cb->vif);
			tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, msdu->len,
					    ATH_TX_WBM_REL_SRC);
		}

		if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
			ts.status = le32_get_bits(tx_status->info0,
				     HAL_TQM2SW_COMPLETION_RING_INFO0_TQM_RELEASE_REASON);
			tqm_rel_reason[ts.status]++;
		} else if (ts.buf_rel_source == HAL_TQM_REL_SRC_MODULE_FW) {
			htt_status = le32_get_bits(tx_status->info0,
						   HTT_TX_WBM_COMP_INFO0_STATUS);
			fw_tx_status[htt_status]++;
			if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_FW_STATUS);
			}

			ath12k_wifi8_dp_tx_process_htt_tx_complete(dp,
								   (void *)tx_status,
								   sw_metadata->skb,
								   tx_ring, sw_metadata,
								   &ts, ring_id,
								   htt_status);
			sw_metadata->skb = NULL;
			continue;
		}

		if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
			__skb_queue_head(&free_list_head, sw_metadata->skb);
			sw_metadata->skb = NULL;
			fast_flag = true;
		}

		if (n_entry == 1)
			prefetch(&dp->device_stats);

		if (unlikely(!fast_flag && sw_metadata->skb)) {
			rcu_read_lock();

			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp,
							      sw_metadata->hw_link_id);
			if (!dp_pdev) {
				DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
				rcu_read_unlock();
				continue;
			}

			ath12k_wifi8_dp_tx_status_parse(ab, tx_status, &ts);
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_COMPLETED_PKTS);
			}
			ath12k_wifi8_dp_tx_complete_msdu(dp_pdev, sw_metadata->skb, &ts,
							 sw_metadata,
							 sw_metadata->hw_link_id,
							 ring_id);
			sw_metadata->skb = NULL;

			rcu_read_unlock();
		}
	}

	dp->device_stats.tx_comp_stats[ring_id].tx_completed += tx_completed;

	for (idx = 0; idx < HAL_WBM_REL_SRC_MODULE_MAX; idx++)
		dp->device_stats.tx_comp_stats[ring_id].tx_wbm_rel_source[idx] +=
			tx_wbm_rel_source[idx];

	for (idx = 0; idx < MAX_TQM_RELEASE_REASON; idx++)
		dp->device_stats.tx_comp_stats[ring_id].tqm_rel_reason[idx] +=
			tqm_rel_reason[idx];

	for (idx = 0; idx < MAX_FW_TX_STATUS; idx++)
		dp->device_stats.tx_comp_stats[ring_id].fw_tx_status[idx] +=
			fw_tx_status[idx];

	dev_kfree_skb_list_fast(&free_list_head);

	return orig_budget - budget;
}

u32 ath12k_wifi8_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable)
{
	u32 bank_config = 0;
	enum hal_encrypt_type encrypt_type = 0;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];
	u32 key_cipher = ahvif->deflink.key_cipher;
	bool vdev_id_check_en;

	if (force_vdev_id_check_disable)
		vdev_id_check_en = false;
	else
		vdev_id_check_en = dp_vif->vdev_id_check_en;

	/* Only valid for raw frames with HW crypto enabled.
	 * With SW crypto, mac80211 sets key per packet
	 */
	if (dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW &&
	    test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    key_cipher != INVALID_CIPHER)
		bank_config |=
			u32_encode_bits(ath12k_dp_tx_get_encrypt_type(key_cipher),
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

	bank_config |= u32_encode_bits(dp_vif->hal_addr_search_flags &
				       HAL_TX_ADDRX_EN,	HAL_TX_BANK_CONFIG_ADDRX_EN) |
			u32_encode_bits(!!(dp_vif->hal_addr_search_flags &
					HAL_TX_ADDRY_EN),
					HAL_TX_BANK_CONFIG_ADDRY_EN);

	bank_config |= u32_encode_bits(ieee80211_vif_is_mesh(ahvif->vif) ? 3 : 0,
					HAL_TX_BANK_CONFIG_MESH_EN) |
			u32_encode_bits(vdev_id_check_en,
					HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN);

	/*TODO need to revist with qos implementation */
	bank_config |= u32_encode_bits(dp_link_vif->map_id,
				       HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID);

	return bank_config;
}

int ath12k_wifi8_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb, struct ath12k_link_sta *arsta)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath12k_dp_vif *dp_vif = &arvif->ahvif->dp_vif;
	u32 info_flags = info->flags;
	bool is_mcast = false, is_eth = false;
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};

	/* Native WiFi format */
	hdr = (struct ieee80211_hdr *)skb->data;

	/* Check if HW encapsulation */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		is_eth = true;
		is_mcast = is_multicast_ether_addr(eth->h_dest);
	} else {
		is_mcast = is_multicast_ether_addr(hdr->addr1);
	}

	if (is_mcast)
		ath12k_wifi8_mcbc_handler(dp_vif, arvif->link_id, arsta, skb,
					  is_eth, false, false, &skb_ctrl,
					  0, false, NULL);
	else
		ath12k_wifi8_ucast_handler(dp_vif, arvif->link_id,
					   arsta, skb, &skb_ctrl, 0, NULL);
	return 0;
}

void ath12k_wifi8_dp_tx_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i;

	ath12k_wifi8_hal_sam_cmd_staging_free(ab);
	ath12k_wifi8_hal_tqm_cmd_staging_free(ab);

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_comp_ring);
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_data_ring);
	}
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tx_exception);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tcl_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tcl_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tqm_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tqm_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->sam_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->sam_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_status_ring);
}

int ath12k_wifi8_dp_tx_ring_setup(struct ath12k_base *ab)
{
	int i, tx_comp_ring_num;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	int ret;
	u8 rbm_id;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;
		rbm_id = map[i].rbm_id;

		ret = ath12k_dp_srng_setup(ab, &dp->tx_ring[i].tcl_data_ring,
					   HAL_TCL_DATA, i, 0,
					   DP_TCL_DATA_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_data ring (%d) :%d\n",
				    i, ret);
			goto err;
		}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_hal_tx_config_rbm_mapping(ab, i, rbm_id, HAL_TCL_DATA);
#endif

		ret = ath12k_dp_srng_setup(ab, &dp->tx_ring[i].tcl_comp_ring,
					   HAL_TX_COMPLETION, tx_comp_ring_num, 0,
					   DP_TX_COMP_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up tx_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			goto err;
		}
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tx_exception, HAL_TX_EXCEPTION, 0, 0,
				   DP_TX_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up wbm2sw_release ring :%d\n",
				ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tcl_status_ring, HAL_TCL_STATUS, 0, 0,
				   DP_TCL_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tcl_status ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tcl_cmd_ring, HAL_TCL_CMD, 0, 0,
				   DP_TCL_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tcl_cmd ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tqm_cmd_ring, HAL_TQM_CMD, 0, 0,
				   DP_TQM_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tqm command ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_wifi8_hal_tqm_cmd_staging_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to allocate tqm staging buffer :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tqm_status_ring, HAL_TQM_STATUS, 0, 0,
				   DP_TQM_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tqm_status ring :%d\n", ret);
		goto err;
	}
	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->rx_ase_cmd_ring, HAL_ASE_CMD_RING, 0, 0,
				   DP_RX_ASE_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up ase_cmd ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->sam_cmd_ring, HAL_SAM_CMD,
				   0, 0, DP_SAM_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup sam cmd ring: %d\n", ret);
		goto err;
	}

	ret = ath12k_wifi8_hal_sam_cmd_staging_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to allocate sam staging buffer :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->sam_status_ring, HAL_SAM_STATUS,
				   0, 0, DP_SAM_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup sam status ring: %d\n", ret);
		goto err;
	}

	/* Send clear command to reset SAM related structures.*/
	ath12k_wifi8_hal_tx_sam_program_clear(ab);

	return 0;

err:
	ath12k_wifi8_dp_tx_ring_cleanup(ab);
	return ret;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_ppeds_tx_update_stats(struct ath12k *ar, int skb_len,
				  struct hal_tqm2sw_completion_ring *tx_status)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct hal_tx_status ts = { 0 };
	bool tx_drop = false;
	bool tx_status_default = false;
	struct ieee80211_tx_info info;
	u8 reason;

	memset(&info, 0, sizeof(info));
	info.status.rates[0].idx = -1;

	dp = ath12k_ab_to_dp(ab);
	ath12k_wifi8_dp_tx_status_parse(ab, tx_status, &ts);
	info.status.ack_signal = ATH12K_DEFAULT_NOISE_FLOOR + ts.ack_rssi;
	info.status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	dp->ppe.ppeds_stats.tqm_rel_reason[ts.status]++;

	if (ts.status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
		info.flags |= IEEE80211_TX_STAT_ACK;
	else if (ts.status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX)
		info.flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts.status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_DS_TQM_REMOVE_MPDU;
			break;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_DS_TQM_DROP_THRESHOLD;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_DS_TQM_REMOVE_TX;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_DS_TQM_REMOVE_AGED;
			break;
		default:
			reason = ATH_TX_DS_TQM_REMOVE_DEF;
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts.status);
			tx_status_default = true;
		}
		tx_drop = true;
	}

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, ts.peer_id);
	if (unlikely(!peer || !ath12k_dp_link_peer_get_sta(peer) ||
		     !ath12k_dp_link_peer_get_vif(peer))) {
		rcu_read_unlock();
		return;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		ahvif = ath12k_vif_to_ahvif(ath12k_dp_link_peer_get_vif(peer));
		if (tx_drop) {
			ath12k_tid_tx_drop_stats(ahvif, ts.tid, skb_len,
						 reason);
		} else {
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_PPEDS_PKTS);
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_COMPLETED_PKTS);
		}
	}

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !tx_status_default) {
		rcu_read_unlock();
		return;
	}

#ifdef CPTCFG_MAC80211_DS_SUPPORT
	ieee80211_ppeds_tx_update_stats(ar->ah->hw, ath12k_dp_link_peer_get_sta(peer),
					&info, peer->txrate, peer->link_id, 0);
#endif
	rcu_read_unlock();
}

int ath12k_wifi8_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k *ar;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_ppeds_tx_comp_ring *tx_ring = &dp->ppe.ppeds_comp_ring;
	int hal_ring_id = tx_ring->ppeds_txcmpl_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_ppeds_tx_desc_info *tx_desc = NULL;
	int valid_entries, count = 0;
	int list_no_skb_count = 0;
	struct htt_tx_completion *status_desc;
	int htt_status;
	struct list_head local_list;
	struct list_head local_list_no_skb;
	size_t stat_size;
	struct hal_tqm2sw_completion_ring *desc;
	struct hal_tqm2sw_completion_ring *tx_status;
	struct ath12k_dp_tx_comp_status tx_comp_status;

	if (WARN_ON_ONCE(budget > DP_PPEDS_SERVICE_BUDGET))
		return count;

	if (likely(ab->stats_disable))
		/* only need buf_addr_info and info0 */
		stat_size = 3 * sizeof(u32);
	else
		stat_size = sizeof(struct hal_tqm2sw_completion_ring);
	INIT_LIST_HEAD(&local_list);
	INIT_LIST_HEAD(&local_list_no_skb);

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return count;
	}

	if (valid_entries >= budget)
		valid_entries = budget;

	ath12k_hal_srng_ppeds_dst_inv_entry(ab, status_ring, valid_entries);

	while (likely(valid_entries--)) {
		desc = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL);
		if (!desc)
			continue;

		ath12k_wifi8_hal_tx_completion_process(desc, &tx_comp_status);
		tx_status = (struct hal_tqm2sw_completion_ring *)desc;
		if (likely(!ab->stats_disable))
			memcpy(((void *)tx_ring->tx_status) +
			       (count * status_ring->entry_size),
			       desc, stat_size);

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS W0:%u W1:%u W2:%u rate:%u peer_id:%d W3:%u W4:%u",
				tx_status->info0, tx_status->info1,
				tx_status->info2, tx_status->rate_stats.info0,
				tx_status->sw_peer_id, tx_status->info3,
				tx_status->info4);

		/* HW done cookie conversion */
		tx_desc = (struct ath12k_ppeds_tx_desc_info *)
				((unsigned long)tx_comp_status.tx_desc);
		if (unlikely(!tx_desc)) {
			ath12k_warn(ab, "unable to retrieve ppe ds tx_desc!");
			continue;
		}

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS completion txdesc:%p\n", tx_desc);

		tx_ring->macid[count] = tx_desc->mac_id;

		if (unlikely(tx_comp_status.buf_rel_source ==
			HAL_WBM_REL_SRC_MODULE_FW)) {
			status_desc = (void *)tx_status;
			htt_status = le32_get_bits(status_desc->info0,
					HAL_TX_COMP_TQM_RELEASE_REASON_MASK);

			if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ)
				ath12k_ppeds_reinject_handler(ab,
						tx_desc,
						(struct htt_tx_completion *)status_desc);

			if (htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_OK &&
				htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ) {
				ab->dp->ppe.ppeds_stats.fw2wbm_pkt_drops++;
				ath12k_dbg(ab, ATH12K_DBG_PPE,
					"Frame rcvd from unexpected src %d status %d!\n",
					tx_comp_status.buf_rel_source, htt_status);
			}
			tx_ring->macid[count] = 0xF;
		}
		/* add descriptor to local list to process in bulk */
		tx_desc->in_use = false;
		if (likely(tx_desc->skb)) {
			list_add_tail(&tx_desc->list, &local_list);
			if (tx_ring->macid[count] != 0xF) {
				ar = ab->pdevs[tx_ring->macid[count]].ar;
				dp_pdev = &ar->dp;
				if (ath12k_dp_stats_enabled(dp_pdev))
					ath12k_ppeds_tx_update_stats(ar,
							tx_desc->skb->len,
							desc);
			}
			count++;
		} else {
			list_add_tail(&tx_desc->list, &local_list_no_skb);
			list_no_skb_count++;
		}
	}
	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	ath12k_dp_ppeds_tx_release_desc_list_bulk(dp, &local_list, count,
			&local_list_no_skb, list_no_skb_count);
	return (count + list_no_skb_count);
}

#define INDEX_LOOKUP_OVERRIDE_ENABLED 1
#define HLOS_TID_OVERWRITE_ENABLED 1
#define FLOW_OVERRIDE_ENABLED 1
static int ath12k_wifi8_dp_tx_reinject(struct ath12k_dp *dp,
				       struct ath12k_dp_vif *dp_vif,
				       struct hal_tcl_exit_base *tx_exception_desc,
				       struct ath12k_tx_desc_info *tx_desc)
{
	struct hal_tcl_data_cmd tcl_desc = {0};
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct dp_tx_ring *tx_ring;
	u8 ring_selector = 0, ring_id = 0;
	struct ath12k_hal *hal = dp->hal;
	struct hal_srng *tcl_ring;
	u8 hal_ring_id;
	u32 data_len;

	ring_selector = dp->hw_params->hw_ops->get_ring_selector(tx_desc->skb);
	ring_id = ring_selector % dp->hw_params->max_tx_ring;

	memcpy(&tcl_desc.buf_addr_info, &tx_exception_desc->buf_addr_info,
	       sizeof(struct ath12k_buffer_addr));
	tcl_desc.info0 =
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO0_BUF_OR_EXT_DESC_TYPE,
					     le32_to_cpu(tx_exception_desc->info0))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO12_BANK_ID,
					      le32_to_cpu(tx_exception_desc->info12))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_VDEV_ID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO11_VDEV_ID,
					     le32_to_cpu(tx_exception_desc->info11))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO9_DATA_LENGTH,
					     le32_to_cpu(tx_exception_desc->info9)));
	if (le32_get_bits(tx_exception_desc->info5,
			  HAL_TCL_EXIT_BASE_INFO5_INDEX_LOOKUP_OVERRIDE)) {
		tcl_desc.search_index = tx_exception_desc->addrx_ast_hash_idx;
		tcl_desc.info1 =
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO11_CACHE_SET_NUM,
					     le32_to_cpu(tx_exception_desc->info11))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE,
				   INDEX_LOOKUP_OVERRIDE_ENABLED);
	}
	tcl_desc.info2 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TO_FW_SW,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO11_TO_FW_SW,
					      le32_to_cpu(tx_exception_desc->info11))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_IPV4_CHECKSUM_EN,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO9_IPV4_CHECKSUM_EN,
					      le32_to_cpu(tx_exception_desc->info9))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_UDP_OVER_IPV4_CHECKSUM_EN,
				    FIELD_GET(
					HAL_TCL_EXIT_BASE_INFO9_UDP_OVER_IPV4_CHECKSUM_EN,
					le32_to_cpu(tx_exception_desc->info9))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_L4_CHECKSUM_EN,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO9_L4_CHECKSUM_EN,
					      le32_to_cpu(tx_exception_desc->info9)));
	tcl_desc.info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO5_FLOW_SELECT,
					      le32_to_cpu(tx_exception_desc->info5))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_LINK_ID,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO5_CMD_LINK_ID,
					      le32_to_cpu(tx_exception_desc->info5)));
	tcl_desc.tcl_cmd_number = tx_exception_desc->tcl_status_number;

	if (le32_get_bits(tx_exception_desc->info9,
			  HAL_TCL_EXIT_BASE_INFO9_HLOS_TID_OVERWRITE))
		tcl_desc.info1 |=
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_HLOS_TID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO0_TID,
					     le32_to_cpu(tx_exception_desc->info0))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE,
				   HLOS_TID_OVERWRITE_ENABLED);
	if (le32_get_bits(tx_exception_desc->info5,
			  HAL_TCL_EXIT_BASE_INFO5_FLOW_OVERRIDE_ENABLE))
		tcl_desc.info2 |=
		FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE,
			   FLOW_OVERRIDE_ENABLED) |
		FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL,
			   FIELD_GET(HAL_TCL_EXIT_BASE_INFO11_WHO_CLASSIFY_INFOSEL,
				     le32_to_cpu(tx_exception_desc->info11)));

	tx_ring = &dp->tx_ring[ring_id];
	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	ath12k_hal_srng_access_begin_no_lock(tcl_ring);
	hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(dp->ab, tcl_ring);
	if (unlikely(!hal_tcl_desc)) {
		ath12k_hal_srng_access_end_no_lock(dp->ab, tcl_ring);
		dp->device_stats.tx_err.desc_na[ring_id]++;
		return DP_TX_ENQ_DROP_TCL_DESC_NA;
	}

	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));

	data_len = FIELD_GET(HAL_TCL_EXIT_BASE_INFO9_DATA_LENGTH,
			     le32_to_cpu(tx_exception_desc->info9));
	ath12k_hal_srng_access_end_no_lock(dp->ab, tcl_ring);
	return DP_TX_ENQ_SUCCESS;
}

static int ath12k_wifi8_dp_tx_null_flowq_handler(
			struct ath12k_dp *dp,
			struct hal_tcl_exit_base *tx_exception_desc,
			struct ath12k_tx_desc_info *tx_desc)
{
	enum hal_tcl_encap_type encap_type;
	struct ath12k_dp_tx_queue_metadata tx_q_params = {0};
	struct ath12k_dp_peer *peer;
	struct ath12k_pdev_dp *dp_pdev;
	u8 tidno, flow_type;
	u16 peer_id;
	u8 hw_link_id, link_id;
	bool non_qos, is_tcp, is_udp, mcast;
	u8 bank_id;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_vif *dp_vif;
	int ret;

	tidno = le32_get_bits(tx_exception_desc->info0,
			      HAL_TCL_EXIT_BASE_INFO0_TID);

	if (le32_get_bits(tx_exception_desc->info6,
			  HAL_TCL_EXIT_BASE_INFO6_ADDRX_IDX_INVALID)) {
		ath12k_err(dp->ab, "Addr X index is invalid for desc_id %d",
			   tx_desc->desc_id);
		return -EINVAL;
	}
	peer_id = le16_to_cpu(tx_exception_desc->meta_data_ase);
	hw_link_id = le32_get_bits(tx_exception_desc->info5,
				   HAL_TCL_EXIT_BASE_INFO12_FW_LINK_ID);

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
	if (!dp_pdev) {
		ath12k_err(dp->ab, "link id is invalid for desc_id %d",
			   tx_desc->desc_id);
		ret = -EINVAL;
		goto end;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_err(dp->ab, "peer_id is invalid for desc_id %d",
			   tx_desc->desc_id);
		ret = -EINVAL;
		goto end;
	}
	if (peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS)
		link_id = peer->hw_links[dp_pdev->hw_link_id];
	else
		link_id = ath12k_dp_get_link_id(dp_pdev, hw_link_id, peer);

	if (link_id < ATH12K_NUM_MAX_LINKS) {
		if (ath12k_dp_peer_get_vif(peer)) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
		} else {
			ath12k_err(dp->ab, "vif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
		if (ahvif) {
			dp_vif = &ahvif->dp_vif;
		} else {
			ath12k_err(dp->ab, "ahvif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
		if (dp_vif) {
			encap_type = dp_vif->tx_encap_type;
		} else {
			ath12k_err(dp->ab, "dp_vif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
	} else {
		ath12k_err(dp->ab, "link_id %d exceeding MAX_LINKS",
			   link_id);
		ret = -EINVAL;
		goto end;
	}

	mcast = le32_get_bits(tx_exception_desc->info5,
			      HAL_TCL_EXIT_BASE_INFO5_DA_IS_BCAST_MCAST);
	is_udp = le32_get_bits(tx_exception_desc->info3,
			       HAL_TCL_EXIT_BASE_INFO3_UDP_PROTO);
	is_tcp = le32_get_bits(tx_exception_desc->info3,
			       HAL_TCL_EXIT_BASE_INFO3_TCP_PROTO);
	non_qos = le32_get_bits(tx_exception_desc->info1,
				HAL_TCL_EXIT_BASE_INFO1_NON_QOS);

	if (peer->is_vdev_peer || non_qos) {
		ath12k_err(dp->ab,
			   "Frame is either mcast %d or non_qos %d for desc_id %d",
			   mcast, non_qos, tx_desc->desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}

	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_BANK_ID_EXCEEDED)) {
		ath12k_err(dp->ab, "Bank ID exceeded for desc_id %d",
			   tx_desc->desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_BANK_NOT_CONFIGURED)) {
		ath12k_err(dp->ab, "Bank registers not configured for desc_id %d",
			   tx_desc->desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_WHO_CLASSIFY_INFO_SEL_EXCEEDED)) {
		ath12k_err(dp->ab, "Number of who_classify_info exceeded for desc_id %d",
			   tx_desc->desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	bank_id = le32_get_bits(tx_exception_desc->info11,
				HAL_TCL_EXIT_BASE_INFO11_WHO_CLASSIFY_INFOSEL);

	flow_type = bank_id * ATH12K_NUM_MSDU_Q_PER_TID + is_udp;
	tx_q_params.encap_type = encap_type;
	tx_q_params.tidno = tidno;
	tx_q_params.flow_type = flow_type;
	ret = ath12k_peer_alloc_dynamic_queue(dp->dp_hw_grp, peer,
					      &tx_q_params);
end:
	rcu_read_unlock();

	if (ret)
		return ret;
	return ath12k_wifi8_dp_tx_reinject(dp, dp_vif, tx_exception_desc,
					   tx_desc);
}

static bool
ath12k_wifi8_dp_validate_tx_exception_error(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tcl_exit_base *tx_exception_desc)
{
	struct ath12k_wifi8_tx_exc_stats *stats = &dp_wifi8->stats.tx_exc_stats;

	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_VDEV_ID_CHECK_EN) &&
	    le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_VDEV_ID_CHECK_FAILURE)) {
		stats->vdev_id_check_fail++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info6,
			  HAL_TCL_EXIT_BASE_INFO6_ADDRX_IDX_INVALID)) {
		stats->addrx_invalid++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info6,
			  HAL_TCL_EXIT_BASE_INFO6_ADDRX_IDX_TIMEOUT)) {
		stats->addrx_timeout++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info0,
			  HAL_TCL_EXIT_BASE_INFO0_MSDU_DROP)) {
		stats->msdu_drop++;
		return true;
	}

	if (le32_get_bits(tx_exception_desc->info1,
			  HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_FRAME)) {
		stats->illegal_pkts++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info1,
			  HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_ETH_HEADER)) {
		stats->illegal_pkt_hdr++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_PEER_POINTER_NULL_EXCEPTION)) {
		stats->peer_ptr_null++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_BANK_NOT_CONFIGURED)) {
		stats->bank_not_configured++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info9,
			  HAL_TCL_EXIT_BASE_INFO9_MSDU_LENGTH_ERROR)) {
		stats->msdu_len_err++;
		return true;
	}
	/* In case tcl cmd to SW */
	if ((le32_get_bits(tx_exception_desc->info11,
			   HAL_TCL_EXIT_BASE_INFO11_TO_FW_SW)) == 2) {
		stats->to_sw_pkts++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_PARSER_OP_TLV_SEQUENCE_ERR)) {
		stats->parse_err++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_WHO_CLASSIFY_INFO_SEL_EXCEEDED)) {
		stats->classify_info_sel_exceed++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_BANK_ID_EXCEEDED)) {
		stats->bank_id_exceed++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info11,
			  HAL_TCL_EXIT_BASE_INFO11_BUFFER_LENGTH_ERROR)) {
		stats->buf_len_err++;
		return true;
	}

	return false;
}

int ath12k_wifi8_dp_tx_exception_handler(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_wifi8_tx_exc_stats *stats = &dp_wifi8->stats.tx_exc_stats;
	struct ath12k_base *ab = dp->ab;
	struct hal_tcl_exit_base *tx_exception_desc = NULL;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_srng *srng;
	int quota = budget;
	u32 desc_id;
	dma_addr_t paddr;
	struct ath12k_tx_sw_metadata sw_metadata = {0};
	int pdev_tx_comp_cnt[ATH12K_GROUP_MAX_RADIO] = {0};
	u8 hw_link_id = 0;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int ret;
	bool tx_exception_error = false;

	srng = &ab->hal.srng_list[dp_wifi8->tx_exception.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	while (budget-- &&
	       (tx_exception_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		stats->tx_exceptions++;
		paddr = ((u64)(le32_get_bits(tx_exception_desc->buf_addr_info.info1,
					     BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(tx_exception_desc->buf_addr_info.info0,
				      BUFFER_ADDR_INFO0_ADDR);
		if (!paddr) {
			ath12k_warn(dp->ab, "invalid  paddr in TX exception path\n");
			print_hex_dump(KERN_ERR, "tx exception desc: ",
				       DUMP_PREFIX_ADDRESS, 16, 1, tx_exception_desc,
				       sizeof(*tx_exception_desc), false);
			continue;
		}

		tx_exception_error =
			ath12k_wifi8_dp_validate_tx_exception_error(dp_wifi8,
								    tx_exception_desc);
		desc_id = le32_get_bits(tx_exception_desc->buf_addr_info.info1,
					BUFFER_ADDR_INFO1_SW_COOKIE);
		tx_desc = ath12k_dp_get_tx_desc(dp, desc_id);
		if (!tx_desc) {
			stats->invalid_desc++;
			ath12k_warn(dp->ab,
				    "unable to get txdesc exception path %d\n", desc_id);
			continue;
		}

		if (tx_exception_error)
			goto tx_buf_release;

		if (le32_get_bits(tx_exception_desc->info11,
				  HAL_TCL_EXIT_BASE_INFO11_FLOW_POINTER_NULL)) {
			stats->null_flowq_pkts++;
			ret = ath12k_wifi8_dp_tx_null_flowq_handler(dp,
								    tx_exception_desc,
								    tx_desc);
			if (ret == 0) {
				stats->reinject_pkts++;
				continue;
			}
		}
tx_buf_release:
		sw_metadata.skb = tx_desc->skb;
		sw_metadata.paddr = tx_desc->paddr;
		sw_metadata.len = tx_desc->len;
		sw_metadata.skb_ext_desc = tx_desc->skb_ext_desc;
		sw_metadata.paddr_ext_desc = tx_desc->paddr_ext_desc;
		sw_metadata.ext_desc_len = tx_desc->ext_desc_len;
		sw_metadata.flags = tx_desc->flags;
		sw_metadata.hw_link_id = tx_desc->hw_link_id;

		tx_desc->skb = NULL;
		tx_desc->skb_ext_desc = NULL;
		tx_desc->in_use = false;
		tx_desc->flags = 0;
		tx_desc->paddr_ext_desc = 0;

		pdev_tx_comp_cnt[sw_metadata.hw_link_id]++;
		ath12k_wifi8_dp_tx_free_txbuf(dp, sw_metadata.skb, &sw_metadata);
		ath12k_dp_tx_release_txbuf(dp, tx_desc, tx_desc->pool_id);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	for (hw_link_id = 0; hw_link_id < ATH12K_GROUP_MAX_RADIO; hw_link_id++) {
		if (likely(pdev_tx_comp_cnt[hw_link_id])) {
			rcu_read_lock();
			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
			if (unlikely(!dp_pdev)) {
				rcu_read_unlock();
				continue;
			}

			if (atomic_sub_and_test(pdev_tx_comp_cnt[hw_link_id],
						&dp_pdev->num_tx_pending))
				wake_up(&dp_pdev->tx_empty_waitq);

			rcu_read_unlock();
		}
	}

	return quota - budget;
}
#endif

int ath12k_wifi8_dp_tx_process_sam_status(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	int quota = budget;
	u16 tag;

	srng = &ab->hal.srng_list[dp_wifi8->sam_status_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- && (hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		ath12k_wifi8_hal_tx_sam_status(ab, hdr);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

static inline
void ath12k_wifi8_dp_tx_stats_update_pre_enqueue(struct ath12k_pdev_dp *dp_pdev,
						 struct ath12k_dp_vif *dp_vif,
						 struct sk_buff *skb,
						 u8 ring_id, u32 len)
{
	/* TODO: implement statistics updates */
}

void ath12k_wifi8_dp_tx_stats_post_enqueue(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_vif *dp_vif,
					   struct sk_buff *skb,
					   struct ath12k_dp_tx_msdu_info *msdu_info,
					   u8 ring_id, u32 len, bool is_mcast)
{
	/* TODO: implement statistics updates */
}

/**
 * ath12k_wifi8_mcbc_get_gsn() - Atomically increment and return the multicast GSN
 * @dp_vif: DP virtual interface whose mcbc_gsn counter is incremented
 *
 * Atomically increments the per-vif multicast/broadcast Global Sequence Number
 * counter and returns the lower 12 bits. The 12-bit wrap-around ensures the
 * value fits in the HTT TCL metadata GSN field.
 *
 * Returns: 12-bit GSN value (0..4095)
 */
static u16 ath12k_wifi8_mcbc_get_gsn(struct ath12k_dp_vif *dp_vif)
{
	return atomic_inc_return(&dp_vif->mcbc_gsn) & 0xfff;
}

/**
 * ath12k_wifi8_tx_validate_recovery() - Check recovery state
 * @ab: ath12k_base
 *
 * Returns: true if in recovery, false otherwise
 */
static bool ath12k_wifi8_tx_validate_recovery(struct ath12k_base *ab)
{
	return unlikely(test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS,
			&ab->dev_flags) ||
			test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags));
}

/**
 * ath12k_wifi8_dp_get_ring_id() - Select TX ring based on SKB
 * @dp: DP structure containing hardware parameters and ring selector callback
 * @ring_id: Output ring ID, set to the selected ring index
 * @skb: Socket buffer used as input to the hardware-specific ring selector
 *
 * Calls the hardware-specific get_ring_selector() callback (e.g. CPU ID for
 * QCN9625) and wraps the result modulo the
 * maximum number of TX rings configured for this hardware.
 */
static inline
void ath12k_wifi8_dp_get_ring_id(struct ath12k_dp *dp, u8 *ring_id,
				 struct sk_buff *skb)
{
	u8 ring_selector;

	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);
	*ring_id = ring_selector % dp->hw_params->max_tx_ring;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	if (unlikely(*ring_id == ATH12K_IPA_TCL_RING))
		*ring_id = ATH12K_IPA_TCL_SW_RING;
#endif
}

/**
 * ath12k_wifi8_ucast_setup_msdu_info() - Setup MSDU info for unicast transmission
 * @arvif: ath12k link virtual interface
 * @arsta: Link station pointer
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to fill with link vif parameters
 * @skb: Socket buffer (unused, reserved for future use)
 * @dp_pdev: DP pdev structure for the transmitting radio
 *
 * Populates the MSDU info structure with unicast-specific fields from the
 * DP virtual interface: bank ID, TCL metadata, vdev ID,
 * BSS AST index/hash, and descriptor type (HAL_TCL_DESC_TYPE_BUFFER).
 */
static void ath12k_wifi8_ucast_setup_msdu_info(struct ath12k_link_vif *arvif,
					       struct ath12k_link_sta *arsta,
					       struct ath12k_dp *dp,
					       struct ath12k_dp_tx_msdu_info *msdu_info,
					       struct sk_buff *skb,
					       struct ath12k_pdev_dp *dp_pdev,
					       struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_sta *ahsta = NULL;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		msdu_info->bss_ast_idx = dp_vlan_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vlan_vif->ast_hash;
		msdu_info->lookup_override = true;
	} else {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	} else if (arsta) {
		ahsta = arsta->ahsta;
		if (ahsta->use_4addr_set) {
			rcu_read_lock();
			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
								      ahsta->dp_peer_id);
			if (!dp_peer) {
				rcu_read_unlock();
				return;
			}

			msdu_info->bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
			msdu_info->bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
			msdu_info->lookup_override = true;
			msdu_info->meta_data_flags =
				u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
				u32_encode_bits(dp_peer->peer_id,
						HTT_TCL_META_DATA_PEER_ID);

			rcu_read_unlock();
		}
	}

	msdu_info->bank_id = dp_vif->bank_id;
	msdu_info->vdev_id = dp_vif->dp_vif_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->pkt_offset = 0;
}

/**
 * ath12k_wifi8_mcbc_setup_msdu_info() - Setup MSDU info for multicast/broadcast
 * @arvif: ath12k link virtual interface
 * @dp_link_vif: DP link virtual interface with bank, lmac, vdev, and AST info
 * @arsta: Link station pointer (optional; used for 4-addr/WDS frames)
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to populate
 * @gsn: Global Sequence Number for MLO multicast reinjection
 * @gsn_valid: Whether the GSN should be embedded in TCL metadata
 * @skb: Socket buffer; header parsed for 4-addr detection in non-Ethernet mode
 * @is_eth: True if the frame is in Ethernet encapsulation format
 * @dp_pdev: DP pdev structure for the transmitting radio
 *
 * Populates the MSDU info for multicast/broadcast transmission. For 4-addr
 * (WDS) Ethernet frames, uses the station's AST info and sets lookup_override.
 * For 4-addr native WiFi frames, redirects to firmware. Otherwise uses the
 * BSS AST info from the link vif. If GSN is valid and lookup_override is not
 * set, embeds the GSN into the TCL metadata for MLO multicast reinjection.
 *
 * Returns: 0 always
 */
static int ath12k_wifi8_mcbc_setup_msdu_info(struct ath12k_link_vif *arvif,
					     struct ath12k_dp_link_vif *dp_link_vif,
					     struct ath12k_link_sta *arsta,
					     struct ath12k_dp *dp,
					     struct ath12k_dp_tx_msdu_info *msdu_info,
					     u16 gsn, bool gsn_valid,
					     struct sk_buff *skb,
					     bool is_eth, struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_sta *ahsta = NULL;

	msdu_info->meta_data_flags = dp_link_vif->tcl_metadata;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		msdu_info->bss_ast_idx = dp_vlan_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vlan_vif->ast_hash;
		msdu_info->lookup_override = true;
	} else {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	} else if (arsta) {
		ahsta = arsta->ahsta;
		if (ahsta->use_4addr_set) {
			rcu_read_lock();
			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
								      ahsta->dp_peer_id);
			if (!dp_peer) {
				rcu_read_unlock();
				return -EINVAL;
			}

			msdu_info->bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
			msdu_info->bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
			msdu_info->lookup_override = true;
			msdu_info->meta_data_flags =
				u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
				u32_encode_bits(dp_peer->peer_id,
						HTT_TCL_META_DATA_PEER_ID);

			rcu_read_unlock();
		}
	} else {
		msdu_info->bss_ast_hash = dp_link_vif->ast_hash;
		msdu_info->bss_ast_idx = dp_link_vif->ast_idx;
		msdu_info->lookup_override = true;
	}

	if (gsn_valid && !arsta) {
		/* Reset and Initialize meta_data_flags with Global Sequence
		 * Number (GSN) info.
		 */
		msdu_info->meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					GENMASK(15, 14)) |
			u32_encode_bits(gsn, GENMASK(11, 0));
		msdu_info->tx_notify_frame = 6;

		if (arvif->nawds_support)
			msdu_info->meta_data_flags |= u32_encode_bits(1, BIT(12));
	}

	msdu_info->bank_id = dp_vif->bank_id;
	msdu_info->vdev_id = dp_vif->dp_vif_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->pkt_offset = 0;

	if (gsn_valid)
		msdu_info->vdev_id += HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
	else if (arvif->nawds_support && !msdu_info->lookup_override)
		msdu_info->meta_data_flags |= u32_encode_bits(1,
					HTT_TCL_META_DATA_HOST_INSPECTED_MISSION);

	return 0;
}

/**
 * ath12k_wifi8_mcbc_setup_encryption() - Setup encryption context for multicast
 * @dp_vif: DP virtual interface
 * @dp_pdev: DP pdev for the transmitting radio (provides the ath12k pointer)
 * @link_id: MLO link identifier
 * @skb: Socket buffer whose skb_cb will be populated with cipher/link info
 * @is_sta: True if the transmitting vdev is in STA mode (skips group slot lookup)
 *
 * Resolves the ath12k link vif for the given link_id, sets the skb_cb fields
 * (ar, link_id, vif), and looks up the BSS peer to find the current multicast
 * key. If a key is found, sets the cipher and ATH12K_SKB_CIPHER_SET flag.
 * For non-STA vdevs, also resolves the VLAN group key slot.
 *
 * Returns: 0 on success, -ENOENT if ar, arvif, or BSS peer is not found
 */
static int ath12k_wifi8_mcbc_setup_encryption(struct ath12k_dp_vif *dp_vif,
					      struct ath12k_pdev_dp *dp_pdev,
					      u8 link_id,
					      struct sk_buff *skb,
					      bool is_sta, bool is_eth)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_hdr *hdr = NULL;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_key_conf *key;
	struct ath12k *ar;

	if (!is_eth)
		ath12k_mlo_mcast_update_tx_link_address(ahvif->vif, link_id,
							skb, info->flags);
	/* Get AR from link */
	ar = dp_pdev->ar;
	if (!ar)
		return -ENOENT;

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (unlikely(!arvif))
		return -ENOENT;

	skb_cb->u.ar = ar;
	skb_cb->link_id = link_id;
	skb_cb->vif = ahvif->vif;

	/* Skip for open mode */
	if (unlikely(arvif->key_cipher == WMI_CIPHER_NONE))
		return 0;

	/* Find peer */
	spin_lock_bh(&ar->ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_addr(ar->ab->dp, arvif->bssid);
	if (!peer) {
		spin_unlock_bh(&ar->ab->dp->dp_lock);
		return -ENOENT;
	}

	/* Get multicast key */
	key = peer->dp_peer->keys[peer->dp_peer->mcast_keyidx];
	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
		if (!is_eth) {
			hdr = (struct ieee80211_hdr *)skb->data;
			if (!ieee80211_has_protected(hdr->frame_control))
				hdr->frame_control |=
						cpu_to_le16(IEEE80211_FCTL_PROTECTED);
		}
	}

	spin_unlock_bh(&ar->ab->dp->dp_lock);

	return 0;
}

/**
 * ath12k_wifi8_dp_mac_encrypt_handler() - Dynamic VLAN (DVLAN) feature handler
 * @msdu_info: MSDU information structure to update with DVLAN settings
 *
 * Handles dynamic-VLAN tagged frames by setting the encapsulation type to RAW,
 * disabling encryption (OPEN), and directing the frame to firmware for
 * processing. Sets the HTT metadata flag and marks the frame for kernel
 * memory extension (ext_kmem) so an extended descriptor is allocated.
 *
 * Returns: DP_TX_FEATURE_SUCCESS always
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_mac_encrypt_handler(struct ath12k_dp_tx_msdu_info *msdu_info)
{
	msdu_info->ext_kmem = true;
	msdu_info->to_fw = true;
	msdu_info->ext_desc.add_htt_metadata = true;
	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;

	return  DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_encap_mismatch_handler() - Encapsulation mismatch handler
 * @dp_vif: DP virtual interface with the configured TX encapsulation type
 * @dp: DP structure for accessing hardware parameters
 * @msdu_info: MSDU information structure to update with corrected encap settings
 * @skb: Socket buffer being transmitted
 *
 * Handles frames where the encapsulation type does not match the expected
 * type for the vdev. For Ethernet-mode vdevs: EAPOL frames are redirected
 * to firmware with HTT metadata; null-function frames are sent to FW with
 * RAW encapsulation.
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on invalid encap type
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_encap_mismatch_handler(struct ath12k_dp_vif *dp_vif,
				       struct ath12k_dp *dp,
				       struct ath12k_dp_tx_msdu_info *msdu_info,
				       struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	msdu_info->ext_desc.encap_type = ath12k_dp_tx_get_encap_type(dp->ab, skb);
	if (msdu_info->ext_desc.encap_type >= HAL_TCL_ENCAP_TYPE_802_3)
		return DP_TX_ERROR;

	msdu_info->is_null = ieee80211_is_nullfunc(hdr->frame_control);
	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET)) {
		msdu_info->ext_kmem = true;
		msdu_info->to_fw = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

		if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			msdu_info->ext_desc.add_htt_metadata = true;
		} else if (msdu_info->is_null) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		}
	}
	return ret;
}

/**
 * ath12k_wifi8_dp_nwifi_handler() - Native WiFi feature handler
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_nwifi_handler(struct sk_buff *skb)
{
	ath12k_dp_tx_encap_nwifi(skb);
	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_raw_mode_handler() - RAW mode feature handler
 * @dp_vif: DP virtual interface
 * @dp: DP structure
 * @msdu_info: MSDU information
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_raw_mode_handler(struct ath12k_dp_vif *dp_vif,
				 struct ath12k_dp *dp,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 u32 *len,
				 struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	if (!test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &dp->ab->ag->flags))
		return DP_TX_ERROR;

	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	if (skb->protocol == cpu_to_be16(ETH_P_ARP)) {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
	}

	/* Handle encryption in RAW mode */
	if (skb_cb->flags & ATH12K_SKB_CIPHER_SET) {
		msdu_info->ext_desc.encrypt_type =
			ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

		/* Add MIC for encrypted frames */
		if (ieee80211_has_protected(hdr->frame_control)) {
			skb_put(skb, IEEE80211_CCMP_MIC_LEN);
			*len = skb->len;
		}
	} else {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_tx_process_features() - Process feature bitmap
 * @dp_vif: DP virtual interface
 * @dp_pdev: DP pdev structure for the transmitting radio
 * @skb: Socket buffer
 * @msdu_info: MSDU info
 * @skb_ctrl: Per-SKB feature bitmap
 *
 * Returns: DP_TX_FEATURE_SUCCESS to continue, DP_TX_DROP to drop
 */
static enum ath12k_dp_feature_result
ath12k_wifi8_dp_tx_process_features(struct ath12k_dp_vif *dp_vif,
				    struct ath12k_pdev_dp *dp_pdev,
				    struct sk_buff *skb,
				    u32 *len,
				    struct ath12k_dp_tx_msdu_info *msdu_info,
				    struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	/* Fast path: no features enabled */
	if (likely(ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_NONE)))
		return DP_TX_FEATURE_SUCCESS;

	/* Process SW encrpt feature */
	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features, DP_FEATURE_SW_ENCRPT)) {
		ret = ath12k_wifi8_dp_mac_encrypt_handler(msdu_info);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features,
				   DP_FEATURE_ENCAP_MISMATCH_HANDLE)) {
		ret = ath12k_wifi8_dp_encap_mismatch_handler(dp_vif, dp_pdev->dp,
							     msdu_info, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process Native WiFi feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_NATIVE_WIFI)) {
		ret = ath12k_wifi8_dp_nwifi_handler(skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process RAW mode feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_RAW_MODE)) {
		ret = ath12k_wifi8_dp_raw_mode_handler(dp_vif, dp_pdev->dp,
						       msdu_info, len, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* HLOS TID override */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_HLOS)) {
		if (unlikely(skb->priority)) {
			msdu_info->tid = skb->priority;
			msdu_info->tid_override = true;
		}
	}

	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_ME)) {
		if (!ath12k_dp_me_tx(dp_vif, skb, msdu_info))
			return DP_TX_RETURN;
	}

	return ret;
}

/**
 * ath12k_wifi8_dp_dma_align_handler() - DMA alignment feature handler
 * @dp_vif: DP virtual interface
 * @skb: Socket buffer
 *
 * Returns: None
 */
static inline
void ath12k_wifi8_dp_dma_align_handler(struct ath12k_dp *dp,
				       struct sk_buff *skb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	int ret;

	/* Check if alignment is needed */
	if (!iova_mask || !((unsigned long)skb->data & iova_mask))
		return;

	/* Align buffer */
	ret = ath12k_dp_tx_align_payload(dp, &skb);
	if (ret) {
		/* Continue with unaligned buffer */
		return;
	}
}

/**
 * ath12k_wifi8_dp_tx_hal_tcl_desc_update() - Populate HAL TCL data descriptor
 * @hal_tcl_desc: Pointer to the HAL TCL data command descriptor to fill
 * @msdu_info: MSDU information containing all descriptor field values
 * @skb: Socket buffer (unused, reserved for future use)
 *
 * Fills all fields of the HAL TCL data command descriptor from the
 * ath12k_dp_tx_msdu_info structure, including buffer address info (physical
 * address, RBM ID, SW cookie), descriptor type, bank ID, TCL metadata flags,
 * data length, TID, LMAC ID, vdev ID, AST index, and cache set number.
 */
static inline
void ath12k_wifi8_dp_tx_hal_tcl_desc_update(struct hal_tcl_data_cmd *hal_tcl_desc,
					    struct ath12k_dp_tx_msdu_info *msdu_info,
					    struct sk_buff *skb)
{
	hal_tcl_desc->buf_addr_info.info0 =
		le32_encode_bits(msdu_info->paddr, BUFFER_ADDR_INFO0_ADDR);
	hal_tcl_desc->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)msdu_info->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	hal_tcl_desc->buf_addr_info.info1 |=
		le32_encode_bits((msdu_info->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(msdu_info->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	hal_tcl_desc->info0 =
		le32_encode_bits(msdu_info->type,
				 HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE) |
		le32_encode_bits(msdu_info->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID) |
		le32_encode_bits(msdu_info->vdev_id, HAL_TCL_DATA_CMD_INFO0_VDEV_ID) |
		le32_encode_bits(msdu_info->data_len, HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH);

	hal_tcl_desc->search_index = cpu_to_le32(msdu_info->bss_ast_idx);

	hal_tcl_desc->info1 =
		le32_encode_bits(msdu_info->bss_ast_hash,
				 HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM) |
		le32_encode_bits(msdu_info->lookup_override,
				 HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE) |
		le32_encode_bits(msdu_info->tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HEADER_LENGTH_READ_SEL);

	hal_tcl_desc->info2 = cpu_to_le32(msdu_info->flags0) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_MSDU_COLOR) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_FORMAT) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_VALID);

	hal_tcl_desc->info3 = cpu_to_le32(msdu_info->flags1) |
		le32_encode_bits(msdu_info->tx_notify_frame,
				 HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT) |
		le32_encode_bits(msdu_info->pkt_offset,
				 HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH) |
		le32_encode_bits(HAL_TX_WILD_CARD_LINK_ID,
				 HAL_TCL_DATA_CMD_INFO3_LINK_ID);

	hal_tcl_desc->tcl_cmd_number = cpu_to_le32(msdu_info->meta_data_flags);

	hal_tcl_desc->info4 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID);

	hal_tcl_desc->insert_vlan_tci_override_val = 0;

	hal_tcl_desc->info5 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_INSERT_VLAN_TCI_OVERRIDE_EN) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_RING_ID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_LOOPING_COUNT);
}

/**
 * ath12k_wifi8_dp_tx_hw_enqueue() - Enqueue a frame to the TCL hardware ring
 * @dp_link_vif: DP link virtual interface
 * @dp_pdev: DP pdev structure for the transmitting radio
 * @msdu_info: MSDU information with all descriptor fields populated
 * @ring_id: TX ring index to enqueue to
 * @arsta: Link station pointer (optional, for QoS tag updates)
 * @skb: Socket buffer to enqueue
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Acquires the TCL ring, writes the TCL data descriptor, applies QoS
 * descriptor updates for SDWF/SCS-tagged frames, and releases the ring.
 * CONFIG_IO_COHERENCY variant writes directly to the ring entry; the
 * non-coherent variant uses a local copy and memcpy for cache safety.
 *
 * Returns: 0 on success, -ENOENT if no TCL descriptor is available
 */
#ifdef CONFIG_IO_COHERENCY
static int ath12k_wifi8_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_tx_msdu_info *msdu_info,
					 u8 ring_id,
					 struct ath12k_link_sta *arsta,
					 struct sk_buff *skb,
					 u32 qos_nw_delay)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		return -ENOENT;
	}

	ath12k_wifi8_dp_tx_hal_tcl_desc_update(hal_tcl_desc, msdu_info, skb);
	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arsta->arvif,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}

	if (unlikely(arsta)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_dp_qos_update(dp, dp_pdev, skb->mark,
					     hal_tcl_desc,
					     qos_tag, arsta->addr);
	}

	ath12k_dmb();
	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#else
static int ath12k_wifi8_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_tx_msdu_info *msdu_info,
					 u8 ring_id,
					 struct ath12k_link_sta *arsta,
					 struct sk_buff *skb,
					 u32 qos_nw_delay)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tcl_data_cmd tcl_desc = {0};
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		return -ENOENT;
	}

	ath12k_wifi8_dp_tx_hal_tcl_desc_update(&tcl_desc, msdu_info, skb);

	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arsta->arvif,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}

	if (unlikely(arsta)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_dp_qos_update(dp, dp_pdev, skb->mark,
					     &tcl_desc,
					     qos_tag, arsta->addr);
	}

	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
	ath12k_dmb();
	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#endif

/**
 * ath12k_wifi8_dp_ext_desc_populate() - Allocate and populate extended TX descriptor
 * @dp: DP structure for slab cache access and DMA mapping
 * @dp_link_vif: DP link virtual interface (for NAWDS and GSN metadata)
 * @msdu_info: MSDU information with ext_feature type and buffer addresses
 * @tx_desc: SW TX descriptor to update with ext_desc pointer and physical address
 * @gsn_valid: Whether the Global Sequence Number is valid for MLO multicast
 * @gsn: Global Sequence Number value to embed in metadata
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on allocation failure
 */
static int
ath12k_wifi8_dp_ext_desc_populate(struct ath12k_dp *dp,
				  struct ath12k_dp_link_vif *dp_link_vif,
				  struct ath12k_dp_tx_msdu_info *msdu_info,
				  struct ath12k_tx_desc_info *tx_desc,
				  bool gsn_valid, int gsn)
{
	struct ath12k_dp_ext_desc *ext_desc = NULL;
	struct ath12k_dp_ext_desc_msdu_info *ext_msdu_info =
		&msdu_info->ext_desc;
	struct hal_tx_msdu_metadata *htt_desc_ext = NULL;
	u8 htt_desc_size;

	/* Allocate extended descriptor */
	ext_desc = kmem_cache_alloc(dp->ext_cache, GFP_DMA | __GFP_ZERO);
	if (!ext_desc) {
		ath12k_warn(dp->ab, "Ext Descriptor not allocated\n");
		return -ENOMEM;
	}
	memset(ext_desc, 0, ATH12K_DP_EXT_DESC_SZ);

	switch (msdu_info->ext_desc.ext_feature) {
	case DP_EXT_ENCAP_OVERRIDE:
		ath12k_dp_ext_desc_set_buf0(ext_desc, msdu_info->paddr,
					    msdu_info->data_len);
		ath12k_dp_ext_desc_override_set(&ext_desc->desc,
						ext_msdu_info);
		msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ;
		break;
	case DP_EXT_TSO:
	case DP_EXT_SG:
		break;
	default:
		break;
	}

	if (msdu_info->ext_desc.add_htt_metadata) {
		msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
		htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
		htt_desc_ext = (struct hal_tx_msdu_metadata *)
				ath12k_dp_ext_desc_get_rsvd0(ext_desc);

		if (!htt_desc_ext)
			goto fail_free_ext_desc;

		htt_desc_ext->info0 |=
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE) |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);

		msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;
	}

	msdu_info->type = HAL_TCL_DESC_TYPE_EXT_DESC;
	msdu_info->paddr = ath12k_dp_ext_desc_map(dp, ext_desc);

	tx_desc->ext_kmem = msdu_info->ext_kmem;
	tx_desc->paddr_ext_desc = msdu_info->paddr;
	tx_desc->ext_desc = ext_desc;
	tx_desc->ext_desc_len = ATH12K_DP_EXT_DESC_SZ;

	return 0;

fail_free_ext_desc:
	if (ext_desc)
		kmem_cache_free(dp->ext_cache, ext_desc);

	return -ENOMEM;
}

/**
 * ath12k_wifi8_dp_tx_desc_populate() - Populate SW TX descriptor fields
 * @dp: DP structure (passed to ath12k_wifi8_dp_ext_desc_populate if needed)
 * @skb: Socket buffer
 * @dp_link_vif: DP link virtual interface (passed to ext_desc_populate)
 * @msdu_info: MSDU information with DMA address, length, and ext_kmem flag
 * @tx_desc: SW TX descriptor to populate
 * @gsn_valid: Whether the Global Sequence Number is valid
 * @gsn: Global Sequence Number value
 *
 * Stores the DMA physical address, data length, and FW-redirect flag into
 * the SW TX descriptor. If extended kernel memory is required (ext_kmem is
 * set in msdu_info), allocates and populates an extended descriptor via
 * ath12k_wifi8_dp_ext_desc_populate().
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on ext_desc failure
 */
static enum ath12k_dp_feature_result
ath12k_wifi8_dp_tx_desc_populate(struct ath12k_dp *dp, struct sk_buff *skb,
				 struct ath12k_dp_link_vif *dp_link_vif,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 struct ath12k_tx_desc_info *tx_desc,
				 bool gsn_valid, int gsn)
{
	int ret = 0;

	tx_desc->len = msdu_info->data_len;
	tx_desc->skb = skb;

	if (msdu_info->ext_kmem)
		ret = ath12k_wifi8_dp_ext_desc_populate(dp, dp_link_vif, msdu_info,
							tx_desc, gsn_valid, gsn);

	if (msdu_info->to_fw) {
		msdu_info->flags0 |= u32_encode_bits(1,
				     HAL_TCL_DATA_CMD_INFO2_TO_FW_SW);
		tx_desc->to_fw = msdu_info->to_fw;
	}

	return ret;
}

enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_vif *ahvif,
			      struct ath12k_dp_link_vif *dp_link_vif,
			      u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			      bool gsn_valid, u16 gsn, int group_slot,
			      struct sk_buff *skb, struct ath12k_link_sta *arsta,
			      struct ath12k_dp_skb_ctrl *skb_ctrl, bool htt_mesh)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_tx_desc_info *tx_desc = NULL;
	enum ath12k_dp_tx_enq_error drop_reason;
	u32 qos_nw_delay = msdu_info->qos_nw_delay;
	bool dma_map;
	int ret;

	if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_spl_desc_free_list,
					   ring_id);
	else
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_desc_free_list,
					   ring_id);

	if (!tx_desc) {
		drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
		goto fail;
	}

	ath12k_wifi8_dp_dma_align_handler(central_dp, skb);
	dma_map = ath12k_dp_tx_dma_map(central_dp, skb, msdu_info->data_len, tx_desc,
				       msdu_info, skb_ctrl);

	if (unlikely(!dma_map)) {
		drop_reason = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail;
	}

	msdu_info->desc_id = tx_desc->desc_id;
	ret = ath12k_wifi8_dp_tx_desc_populate(central_dp, skb, dp_link_vif,
					       msdu_info, tx_desc,
					       gsn_valid, gsn);

	if (ret < 0) {
		drop_reason = DP_TX_ENQ_DROP_EXT_DESC_NA;
		goto fail;
	}

	/* Enqueue to hardware */
	ret = ath12k_wifi8_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, msdu_info,
					    ring_id, arsta, skb, qos_nw_delay);
	if (ret < 0) {
		drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail;
	}

	return DP_TX_ENQ_SUCCESS;

fail:
	if (tx_desc->ext_kmem) {
		ath12k_core_dma_unmap_single(central_dp->dev,
					     tx_desc->paddr_ext_desc,
					     tx_desc->ext_desc_len,
					     DMA_TO_DEVICE);
		kmem_cache_free(central_dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(central_dp, tx_desc, ring_id);
	return drop_reason;
}

/**
 * ath12k_wifi8_ucast_handler() - Unicast packet transmission handler
 * @dp_vif: DP virtual interface
 * @link_id: MLO link identifier for the target link
 * @arsta: Link station pointer (optional; used for QoS tag updates)
 * @skb: Socket buffer to transmit
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Main unicast TX handler. Resolves the DP pdev, checks the TX pending limit,
 * selects a TX ring, populates MSDU info, processes the TX feature bitmap,
 * assigns a TX descriptor, DMA-maps the buffer, populates the SW TX descriptor,
 * and enqueues the frame to the TCL hardware ring.
 *
 */
void ath12k_wifi8_ucast_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
				struct ath12k_link_sta *arsta, struct sk_buff *skb,
				struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
				struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = rcu_dereference(ahvif->link[link_id]);
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_dp *central_dp = NULL;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	enum ath12k_dp_tx_enq_error drop_reason = DP_TX_ENQ_DROP_MISC;
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	enum ath12k_dp_feature_result ret;
	bool dma_map;
	u8 ring_id;
	u32 len = skb->len;

	if (unlikely(!arvif || !arvif->is_created))
		goto fail;

	/* Get DP pdev */
	dp_pdev = ath12k_dp_to_dp_pdev(arvif->ar->ab->dp, dp_link_vif->pdev_idx);
	if (!dp_pdev)
		goto fail;

	prefetch(dp_pdev);
	central_dp = ath12k_get_central_dp(dp_pdev->dp);

	/* Check recovery state */
	if (ath12k_wifi8_tx_validate_recovery(central_dp->ab)) {
		ieee80211_free_txskb(dp_pdev->ar->ah->hw, skb);
		return;
	}

	/* Get ring ID  */
	if (likely(skb_ctrl->flags & DP_SKB_FAST_TX))
		ring_id = smp_processor_id();
	else
		ath12k_wifi8_dp_get_ring_id(central_dp, &ring_id, skb);

	/* Update receive statistics */
	ath12k_wifi8_dp_tx_stats_update_pre_enqueue(dp_pdev, dp_vif,
						    skb, ring_id, len);

	/* Setup MSDU info */
	ath12k_wifi8_ucast_setup_msdu_info(arvif, arsta, central_dp, &msdu_info, skb,
					   dp_pdev, vlan_ahvif);
	msdu_info.meta_data_flags = dp_link_vif->tcl_metadata;

	/* Fast path: no features enabled */
	if (unlikely((DP_FEATURE_IS_ANY(dp_vif) || skb_ctrl->features))) {
		/* Process features based on bitmap */
		ret = ath12k_wifi8_dp_tx_process_features(dp_vif, dp_pdev, skb, &len,
							&msdu_info, skb_ctrl);

		if (ret != DP_TX_FEATURE_SUCCESS && ret != DP_TX_RETURN)
			goto fail;
	}

	prefetch(central_dp->device_stats.tx_fast_unicast);

	/* Assign TX descriptor */
	if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_spl_desc_free_list,
					   ring_id);
	else
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_desc_free_list,
					   ring_id);

	if (unlikely(!tx_desc))
		goto fail;

	dma_map = ath12k_dp_tx_dma_map(central_dp, skb, len, tx_desc, &msdu_info,
				       skb_ctrl);
	if (unlikely(!dma_map)) {
		ath12k_warn(central_dp->ab, "failed to DMA map data Tx buffer\n");
		goto fail;
	}

	msdu_info.desc_id = tx_desc->desc_id;
	msdu_info.data_len = len;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;

	ret = ath12k_wifi8_dp_tx_desc_populate(central_dp, skb, dp_link_vif, &msdu_info,
					       tx_desc, false, 0);
	if (ret != DP_TX_FEATURE_SUCCESS)
		goto fail;

	/* Enqueue to hardware */
	ret = ath12k_wifi8_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, &msdu_info, ring_id,
					    arsta, skb, qos_nw_delay);

	if (ret)
		goto fail;

	/* Update success statistics */
	if (likely(skb_ctrl->flags & DP_SKB_FAST_TX)) {
		DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, len, ring_id);
		central_dp->device_stats.tx_unicast[ring_id]++;
	} else {
		ath12k_wifi8_dp_tx_stats_post_enqueue(dp_pdev, dp_vif, skb,
						      &msdu_info, ring_id,
						      len, false);
	}
	atomic_inc(&dp_pdev->num_tx_pending);

	return;

fail:
	if (tx_desc->ext_kmem) {
		ath12k_core_dma_unmap_single(central_dp->dev,
					     tx_desc->paddr_ext_desc,
					     tx_desc->ext_desc_len,
					     DMA_TO_DEVICE);
		kmem_cache_free(central_dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(central_dp, tx_desc, ring_id);

	ath12k_mac_ieee80211_free_txskb(dp_pdev->ar->ah->hw, skb, dp_pdev,
					arsta ? ath12k_ahsta_to_sta(arsta->ahsta) : NULL,
					dp_vif, drop_reason, ring_id, false);
}

/**
 * ath12k_wifi8_mcbc_handler() - Multicast/Broadcast packet transmission handler
 * @dp_vif: DP virtual interface
 * @link_id: MLO link identifier for the target link
 * @arsta: ath12k_link station pointer (optional; used for 4-addr/WDS frames)
 * @skb: Socket buffer to transmit (will be cloned/copied per active link)
 * @is_eth: True if the frame is in Ethernet encapsulation format
 * @gsn_valid: True if a Global Sequence Number should be assigned (MLO mcast)
 * @is_sta: True if the transmitting vdev is in STA mode
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Iterates over all active MLO links of the vif. For each link, clones or
 * copies the SKB, sets up encryption context, populates MSDU info, processes
 * the TX feature bitmap, assigns a TX descriptor, DMA-maps the buffer, and
 * enqueues the frame to the TCL hardware ring. Frames are dropped per-link
 * on any error without affecting other links.
 */
void ath12k_wifi8_mcbc_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
			       struct ath12k_link_sta *arsta, struct sk_buff *skb,
			       bool is_eth, bool gsn_valid, bool is_sta,
			       struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
			       bool htt_mesh, struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	enum ath12k_dp_feature_result feature_ret;
	enum ath12k_dp_tx_enq_error err;
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *skb_new;
	struct ath12k_dp *central_dp = NULL;
	unsigned long links_map = 0;
	int group_slot = -1;
	u32 len;
	u16 gsn;
	u8 ring_id;
	int ret;

	/* Get active links.*/
	if (gsn_valid) {
		links_map = ahvif->links_map;
		gsn = ath12k_wifi8_mcbc_get_gsn(dp_vif);
	} else {
		set_bit(link_id, &links_map);
	}

	/* Iterate through all active links.*/
	for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ath12k_link_vif *arvif = rcu_dereference(ahvif->link[link_id]);

		if (!arvif || !arvif->is_up)
			continue;

		dp_link_vif = &dp_vif->dp_link_vif[link_id];

		/* Check if link is up */
		if (!dp_link_vif)
			continue;

		/* Get DP pdev */
		dp_pdev = ath12k_dp_to_dp_pdev(arvif->ar->ab->dp,
					       dp_link_vif->pdev_idx);

		if (!dp_pdev)
			continue;

		central_dp = ath12k_get_central_dp(dp_pdev->dp);
		ath12k_wifi8_dp_get_ring_id(central_dp, &ring_id, skb);

		/* Check recovery state */
		if (ath12k_wifi8_tx_validate_recovery(central_dp->ab))
			return;

		/* Copy SKB for this link */
		if (is_eth)
			skb_new = skb_clone(skb, GFP_ATOMIC);
		else
			skb_new = skb_copy(skb, GFP_ATOMIC);

		if (!skb_new)
			continue;

		len = skb_new->len;

		/* Setup encryption.*/
		ret = ath12k_wifi8_mcbc_setup_encryption(dp_vif, dp_pdev,
							 link_id, skb_new,
							 is_sta, is_eth);
		if (ret) {
			dev_kfree_skb_any(skb_new);
			continue;
		}

		/* Setup MSDU info.*/
		msdu_info.qos_nw_delay = qos_nw_delay;
		ret = ath12k_wifi8_mcbc_setup_msdu_info(arvif, dp_link_vif, arsta,
							central_dp, &msdu_info, gsn,
							gsn_valid, skb_new, is_eth,
							dp_pdev, vlan_ahvif);
		if (ret < 0) {
			dev_kfree_skb_any(skb_new);
			continue;
		}

		/* Process features based on bitmap.*/
		feature_ret = ath12k_wifi8_dp_tx_process_features(dp_vif, dp_pdev,
								  skb_new, &len,
								  &msdu_info,
								  skb_ctrl);

		if (feature_ret != DP_TX_FEATURE_SUCCESS) {
			if (feature_ret == DP_TX_RETURN)
				break;

			DP_STATS_INC(dp_vif, tx_i.drop[DP_TX_ENQ_DROP_FEAT_ERR],
				     1, ring_id);
			dev_kfree_skb_any(skb_new);
			continue;
		}

		msdu_info.data_len = len;

		err = ath12k_wifi8_dp_tx_mcast_send(dp_pdev, ahvif, dp_link_vif, ring_id,
						    &msdu_info, gsn_valid, gsn,
						    group_slot, skb_new, arsta, skb_ctrl,
						    htt_mesh);

		if (unlikely(err != DP_TX_ENQ_SUCCESS)) {
			DP_STATS_INC(dp_vif, tx_i.drop[err], 1, ring_id);
			dev_kfree_skb_any(skb_new);
			continue;
		}

		atomic_inc(&dp_pdev->num_tx_pending);
	}
}
