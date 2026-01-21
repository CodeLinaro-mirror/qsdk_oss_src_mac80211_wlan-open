/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_PEER_WIFI8_H
#define ATH12K_PEER_WIFI8_H

#include "../dp_cmn.h"
#include "hw.h"
#include "dp_tx_flow_info.h"

struct peer_assoc_flowq_params;
struct ath12k_dp_peer_ext_ctx {
	struct ath12k_dp_tx_flow_info tx_flow_info;
	u16 ast_index;
	u16 ast_hash;
};

static inline struct ath12k_dp_tx_flow_info *
ath12k_dp_get_tx_flow_info_from_peer(struct ath12k_dp_peer *dp_peer)
{
	if (!dp_peer->peer_ext_ctx)
		return NULL;

	return &dp_peer->peer_ext_ctx->tx_flow_info;
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif);
void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id);
int ath12k_wifi8_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr);
int ath12k_wifi8_dp_link_peer_create(struct ath12k_base *ab, u32 vdev_id, u8 *addr);
void ath12k_wifi8_dp_link_peer_delete(struct ath12k_base *ab, u32 vdev_id, u8 *addr);
void ath12k_dp_peer_cleanup_indication(struct ath12k_dp *dp,
				       u16 peer_id,
				       u8 hw_link_id);
void ath12k_wifi8_dp_link_peer_assoc(struct ath12k_dp_hw *dp_hw, struct ath12k_dp *dp,
				     u8 *addr, u32 hw_link_id);
int ath12k_wifi8_get_mgmt_flowq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr, struct peer_assoc_flowq_params *flowq_params);
int ath12k_wifi8_get_holq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			  u8 *addr, struct peer_assoc_holq_params *holq_params);
#endif
