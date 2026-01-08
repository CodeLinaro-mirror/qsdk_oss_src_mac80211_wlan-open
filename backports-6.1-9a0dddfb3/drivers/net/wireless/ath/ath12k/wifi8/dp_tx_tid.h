/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_TID_H
#define ATH12K_DP_TX_TID_H

#include "dp_msdu_queue.h"

#define TQM_NON_DATA_TID		15
#define ATH12K_HOL_TID			6

#define MGMT_TID_MSDUQ_TYPE		31

enum ath12k_tx_tid {
	DEFAULT_TID       = 0,
	MAX_VALID_DATA_TID = 7,
	NON_QOS_TID       = 16,
	MGMT_TID          = 17,
	BEACON_TID        = 18,
	NONPAUSE_TID      = 19,
	NONBUFFERED_TID   = 20,
	NAN_BEACON_TID    = 21,
	NAN_MGMT_TID      = 22,
	QOS_NULL_TID      = 23,
	EXP_NON_QOS_TID   = 24,
	UL_TRIG_START_TID = 25,
	UL_TRIG_BK_TID    = UL_TRIG_START_TID,
	UL_TRIG_BE_TID    = 26,
	UL_TRIG_VI_TID    = 27,
	UL_TRIG_VO_TID    = 28,
	UL_BSR_TID        = 29,
	OFF_CHANNEL_TID   = 30,
	MLO_MGMT_TID      = 31,
	MAX_TID,
};

struct ath12k_dp_mpdu_q_info
*ath12k_alloc_peer_tid_mpduq(struct ath12k_dp_hw_group *dp_hw_grp,
			     struct ath12k_dp_peer *peer, u8 tidno,
			     u8 flow_type);
int ath12k_tx_send_mpduq_init(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      struct ath12k_dp_vif *dp_vif,
			      struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr);
struct ath12k_dp_tx_tid_info *ath12k_dp_get_tid(struct ath12k_dp_peer *peer,
						u8 tidno);
struct ath12k_dp_mpdu_q_info
*ath12k_peer_alloc_tid(struct ath12k_dp_hw_group *dp_hw_grp,
		       struct ath12k_dp_peer *peer,
		       struct ath12k_dp_vif *dp_vif, u8 tidno,
		       struct ath12k_dp_tx_tid_info **ptid, u8 flow_type);
void ath12k_peer_free_tid(struct ath12k_dp_hw_group *dp_hw_grp,
			  struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
			  struct ath12k_dp_tx_tid_info *ptid);
#endif
