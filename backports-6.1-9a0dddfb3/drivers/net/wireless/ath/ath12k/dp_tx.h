/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_TX_H
#define ATH12K_DP_TX_H

#include "core.h"

struct ath12k_ppeds_desc_params {
	unsigned int num_ppeds_desc;
	unsigned int ppeds_hotlist_len;
};

struct ath12k_dp_htt_wbm_tx_status {
	bool acked;
	s8 ack_rssi;
};

void ath12k_dp_tx_put_bank_profile(struct ath12k_dp *dp, u8 bank_id);

void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb);
void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len);
int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb);
void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id);
struct ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp *dp,
						       u8 pool_id);
int ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_ppeds_tx_completion_handler(struct ath12k_base *ab, int ring_id);
struct ath12k_ppeds_tx_desc_info *
ath12k_dp_ppeds_tx_assign_desc_nolock(struct ath12k_dp *dp);
#endif
#endif
