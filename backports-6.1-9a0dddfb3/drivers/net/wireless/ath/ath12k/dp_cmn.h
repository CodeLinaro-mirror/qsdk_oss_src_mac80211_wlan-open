/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_CMN_H
#define ATH12K_DP_CMN_H

#include "cmn_defs.h"
#include "hw.h"

/* Max number of links for MLO connection */
#define ATH12K_DP_MAX_MLO_LINKS 4

struct ath12k_hw_group;
struct ath12k_sta;
struct ath12k;

struct dp_srng {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
};

struct ath12k_dp_hw_link {
	u8 device_id;
	u8 pdev_idx;
};

#define MAX_DP_PEER_LIST_SIZE  16384
#define ATH12K_MAX_PEER_ID	2048
#define ATH12K_MAX_STA_ID	1536
#define DP_TCL_NUM_RING_MAX  4
#define DP_REO_DST_RING_MAX  4
#define DP_TCL_DESC_TYPE_MAX 2

struct ath12k_dp_hw {
	struct ath12k_dp_peer __rcu *dp_peer_list[MAX_DP_PEER_LIST_SIZE];
	DECLARE_BITMAP(free_peer_id_map, ATH12K_MAX_PEER_ID);
	DECLARE_BITMAP(free_sta_id_map, ATH12K_MAX_STA_ID);
	u16 last_peer_id;
	u16 last_sta_id;

	/* Lock for protection of dp_peer_list and peers */
	spinlock_t peer_lock;
	struct list_head peers;
};

struct ath12k_dp_hw_group {
	struct ath12k_dp_hw_link hw_links[ATH12K_GROUP_MAX_RADIO];
	struct ath12k_dp *dp[ATH12K_MAX_SOCS];
	struct dp_rx_fst *fst;
	u8 *tx_status_buf[ATH12K_HW_MAX_QUEUES];
	u8 *rx_status_buf[DP_REO_DST_RING_MAX];

	/* Keep Last */
	u8 arch_data[] __aligned(sizeof(void *));
};

/* TODO: Move this to a seperate dp_stats file */
struct ath12k_per_peer_tx_stats {
	u32 succ_bytes;
	u32 retry_bytes;
	u32 failed_bytes;
	u32 duration;
	u16 succ_pkts;
	u16 retry_pkts;
	u16 failed_pkts;
	u16 ru_start;
	u16 ru_tones;
	u16 succ_mpdu_pkts;
	u16 retried_mpdu_pkts;
	u16 mpdu_tried;
	u8 ba_fails;
	u8 mu_grpid;
	u8 mu_pos;
	u8 rate;
	u8 bw;
	u8 flags;
	u8 mcs;
	u8 tid;
	u8 nss;
	bool is_ampdu;
	bool stbc;
	bool ldpc;
};

struct ath12k_dp_peer_create_params {
	struct ieee80211_sta *sta;
	bool is_mlo;
	bool is_vdev_peer;
	u8 hw_link_id;
	bool is_sta_bss_peer;
	u16 peer_id;
	u16 sta_id;
};

struct ath12k_dp_link_peer_rate_info {
	struct rate_info txrate;
	u64 rx_duration;
	u64 tx_duration;
	u8 rssi_comb;
	s8 signal_avg;
	u16 tx_retry_count;
	u16 tx_retry_failed;
	u32 rx_retries;
};

enum wme_ac {
	WME_AC_BE,
	WME_AC_BK,
	WME_AC_VI,
	WME_AC_VO,
	WME_NUM_AC
};

struct ath12k_dp_link_peer;
struct ath12k_dp_link_peer_rate_info;
void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp);
int ath12k_dp_cmn_device_init(struct ath12k_dp *dp);
void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag);
void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag);
void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar);
int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       struct ieee80211_sta *sta, u8 *addr, u8 link_id,
			       u32 hw_link_id, struct ieee80211_vif *vif,
			       u8 vp_type, int vp_num);
void ath12k_dp_link_peer_unassign(struct ath12k *ar, u8 vdev_id, u8 *addr);
unsigned long ath12k_link_peer_last_active(struct ath12k_dp_link_peer *link_peer);
void ath12k_link_peer_get_sta_rate_info_stats(struct ath12k_dp_link_peer *link_peer,
					      struct ath12k_dp_link_peer_rate_info *rate_info);
bool ath12k_dp_link_peer_reset_rx_stats(struct ath12k_dp *dp, const u8 *addr);
bool ath12k_dp_link_peer_reset_tx_stats(struct ath12k_dp *dp, const u8 *addr);
int ath12k_dp_mon_init(struct ath12k_dp *dp);
void ath12k_dp_mon_deinit(struct ath12k_dp *dp);
void ath12k_dp_cp_link_peer_unassign(struct ath12k *ar, struct ath12k_link_vif *arvif,
				     struct ath12k_sta *ahsta, u8 link_id, u8 *addr);
struct ath12k_dp_peer *ath12k_dp_peer_find(struct ath12k_dp_hw *dp_hw, u8 *addr);
u16 ath12k_dp_peer_get_peer_id(struct ath12k_dp_hw *dp_hw, u8 *addr);
u16 ath12k_dp_peer_get_sta_id(struct ath12k_dp_hw *dp_hw, u8 *addr);
#endif
