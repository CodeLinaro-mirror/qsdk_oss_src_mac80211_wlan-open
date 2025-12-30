/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH12K_DEBUGFS_STA_H_
#define _ATH12K_DEBUGFS_STA_H_

#include <net/mac80211.h>

#include "core.h"
#include "dp_rx.h"

#define ATH12K_STA_RX_STATS_BUF_SIZE		(1024 * 16)
#define STATS_OP_FMT(name) tx_stats->stats[ATH12K_STATS_TYPE_##name]

#ifdef CPTCFG_ATH12K_DEBUGFS
void ath12k_debugfs_sta_op_add(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta, struct dentry *dir);
void ath12k_debugfs_link_sta_op_add(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_link_sta *link_sta,
				    struct dentry *dir);
void ath12k_debugfs_sta_update_success(struct ath12k_dp_link_peer *peer,
				       struct ath12k_per_peer_tx_stats *peer_stats);
void ath12k_debugfs_sta_update_failure(struct ath12k_dp_link_peer *peer,
				       u16 failed_msdu);
void ath12k_debugfs_sta_update_retry(struct ath12k_dp_link_peer *peer,
				     struct ath12k_per_peer_tx_stats *peer_stats);
void ath12k_debugfs_sta_update_misc(struct ath12k_dp_link_peer *peer,
				    struct ath12k_per_peer_tx_stats *peer_stats);

#else /* CPTCFG_ATH12K_DEBUGFS */

#define ath12k_debugfs_sta_op_add NULL

static inline void
ath12k_debugfs_sta_update_success(struct ath12k_dp_link_peer *peer,
				  struct ath12k_per_peer_tx_stats *peer_stats)
{
}
static inline void
ath12k_debugfs_sta_update_failure(struct ath12k_dp_link_peer *peer,
				  u16 failed_msdu)
{
}
static inline void
ath12k_debugfs_sta_update_retry(struct ath12k_dp_link_peer *peer,
				struct ath12k_per_peer_tx_stats *peer_stats)
{
}
static inline void
ath12k_debugfs_sta_update_misc(struct ath12k_dp_link_peer *peer,
			       struct ath12k_per_peer_tx_stats *peer_stats)
{
}
#endif /* CPTCFG_ATH12K_DEBUGFS */

#endif /* _ATH12K_DEBUGFS_STA_H_ */
