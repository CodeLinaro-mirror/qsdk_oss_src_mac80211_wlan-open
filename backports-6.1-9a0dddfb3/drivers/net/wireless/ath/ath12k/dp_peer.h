/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_PEER_H
#define ATH12K_DP_PEER_H

#include "event.h"
#include "dp_rx.h"
#include "dp_stats.h"

#define ATH12K_DP_HW_LINK_ID_INVALID           0xFF
#define ATH12K_DP_LOGICAL_LINK_ID_INVALID      0xFF

#define ATH12K_DP_PEER_ID_INVALID              0xFFFF
#define ATH12K_3LINK_MLO_MAX_STA_LINKS         3

/* 17 tids for DP, 2 for mgmt, and 1 shared between DP and mgmt */
#define ATH12K_MAX_TIDS 20

struct ath12k_dp_link_vif;
struct ath12k_dp_peer_ext_ctx;

enum ath12k_dp_peer_state {
	ATH12K_DP_PEER_CREATED,
	ATH12K_DP_PEER_LOGICALLY_DELETED,
	ATH12K_DP_PEER_DELETED,
};

struct ppdu_user_delayba {
	u16 sw_peer_id;
	u32 info0;
	u16 ru_end;
	u16 ru_start;
	u32 info1;
	u32 rate_flags;
	u32 resp_rate_flags;
};

struct ath12k_atf_peer_airtime {
	struct peer_airtime_consumption tx_airtime_consumption[WME_NUM_AC];
	struct peer_airtime_consumption rx_airtime_consumption[WME_NUM_AC];
	u64 last_update_time;
};

struct ath12k_mscs_ctxt {
	u8 user_priority_bitmap;
	u8 user_priority_limit;
	u8 tclas_mask;
};

/**
 * struct ath12k_peer_event - Peer-specific event (optimized)
 * @common: Base event structure (includes flags)
 * @peer_id: Peer ID for safe lookup
 * @state: State flags for queue management (e.g. ATH12K_EVENT_QUEUED)
 *
 * Optimized event structure containing only fields needed for
 * safe and efficient event processing. The link_id is NOT needed
 * as it's available from peer->link_id after successful lookup.
 */
#define ATH12K_EVENT_QUEUED 0

struct ath12k_peer_event {
	struct ath12k_event common;
	unsigned long state;
	u16 peer_id;
};

struct ath12k_dp_link_peer {
	struct list_head list;
	struct ath12k_dp_peer *dp_peer;
	int vdev_id;
	u8 addr[ETH_ALEN];
	int peer_id;
	u16 ast_hash;
	u8 pdev_idx;
	u16 hw_peer_id;

	struct ppdu_user_delayba ppdu_stats_delayba;
	bool delayba_flag;
	bool is_authorized;
	bool mlo;
	u32 last_delayed_ba_ppduid;
	/* protected by ab->data_lock */

	u16 ml_id;

	/* any other ML info common for all partners can be added
	 * here and would be same for all partner peers.
	 */
	u8 ml_addr[ETH_ALEN];

	/* To ensure only certain work related to dp is done once */
	bool primary_link;

	/* for reference to ath12k_link_sta */
	u8 link_id;

	/* peer addr based rhashtable list pointer */
	struct rhash_head rhash_addr;
	bool rhash_done;

	bool is_bridge_peer;
	u8 hw_link_id;

	/* link stats */
	struct rate_info txrate;
	struct rate_info rxrate;
	struct rate_info last_txrate;
	u64 rx_duration;
	u64 tx_duration;
	u8 rssi_comb;
	u16 tx_retry_failed;
	u16 tx_retry_count;
	struct ewma_avg_rssi avg_rssi;
	struct ath12k_dp_link_peer_stats peer_stats;

	u16 tcl_metadata;
	bool assoc_success; /* information on peer assoc status from firmware */
	u32 flow_cnt[ATH12K_DATA_TID_MAX];
	u8 tid_weight[ATH12K_DATA_TID_MAX];

	struct ath12k_atf_peer_airtime atf_peer_airtime;
	u32 atf_peer_conf_airtime;
	u32 atf_actual_airtime;
	u8 atf_group_index;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;

	bool is_assigned;

	struct ath12k_dp_link_peer_rx_signal_stats signal_stats;
	/* Generic Event Mechanism */
	struct ath12k_peer_event event;

	/* RSSI-based deauthentication monitoring */
	struct {
		s8 last_rssi;				/* Last measured RSSI in dBm */
		u32 low_rssi_count;			/* Consecutive low RSSI samples */
		unsigned long first_low_jiffies;	/* Timestamp of first low RSSI */
		struct ath12k_rssi_deauth_config *cfg;	/* Cached config pointer */
	} rssi_mon;
	s8 min_rssi;
	s8 max_rssi;

	u16 link_band_id;
	u16 tid_band_id[ATH12K_DATA_TID_MAX];
};

#define ATH12K_PEER_EVENT_RSSI_LOW      BIT(0)

struct ath12k_dp_peer {
	struct list_head list;
	struct ieee80211_sta *sta;
	struct ieee80211_vif *vif;
	struct rcu_head rcu_head;
	enum ath12k_dp_peer_state dp_peer_state;
	u16 tcl_metadata;
	u16 peer_id;
	u16 sta_id;
	u8 addr[ETH_ALEN];
	bool dms_disable;       /* Peer DMS capability (use ME6) */
	bool is_mlo;
	bool is_vdev_peer;
	bool is_sta_bss_peer;
	/* hw_link_id of the radio, valid only for self bss peer */
	u8 hw_link_id;

	u8 primary_link_id;
	u8 assoc_hw_link_id;

	/* Lock for protection of link_peers*/
	spinlock_t link_peers_lock;
	struct ath12k_dp_link_peer __rcu *link_peers[ATH12K_DP_PEER_MAX_MLO_LINKS];

	u32 peer_links_map;
	bool primary_link_frag_setup;

	enum hal_pn_type pn_type;

	/* Lock for protection of keys */
	spinlock_t keys_lock;
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
	struct ath12k_dp_rx_tid rx_tid[ATH12K_MAX_TIDS];

	struct ath12k_dp_peer_qos *qos;
	/* Info used in MMIC verification of * RX fragments */
	struct crypto_shash *tfm_mmic;
	u8 mcast_keyidx;
	u8 ucast_keyidx;
	u16 sec_type;
	u16 sec_type_grp;
	u8 vdev_type_4addr;
	bool is_reset_mcbc;
	struct ath12k_mld_qos_stats mld_qos_stats[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	struct ath12k_dp_mld_peer_stats mld_stats;
	bool qos_stats_lvl;

	u8 l2h_link_map[ATH12K_NUM_MAX_LINKS];
	u8 hw_links[ATH12K_DP_PEER_MAX_MLO_LINKS];
	struct ath12k_dp_peer_stats stats[ATH12K_DP_PEER_MAX_MLO_LINKS];
	struct ath12k_mscs_ctxt mscs_ctxt;
	struct ath12k_dp_preserved_stats link_peer_delete_stats;
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx;
	u8 is_sta_bss_peer_4addr :1,
	   is_11s_mesh_peer      :1,
	   is_mmesh_peer         :1,
	   is_authorized         :1,
	   mscs_session_exists	 :1,
	   use_4addr		 :1;
#if defined(CPTCFG_MAC80211_PPE_SUPPORT) || defined(CPTCFG_ATH12K_PPE_DS_SUPPORT)
	int ppe_vp_num;
#endif
	struct net_device *dev;
	u16 stats_id;
	u16 tid_stats_id[ATH12K_MAX_TIDS];
	u8 tx_encap_type;
	u8 rx_decap_type;

	/* Hash table node for MAC address lookup */
	struct hlist_node hash_node;
};

#define QOS_MSDUQ_MAX ((QOS_TID_MDSUQ_MAX * QOS_TID_MAX) + MSDUQ_MAX_DEF)

#define QOS_MAX_SCS_ID 128

#define QOS_TAG_MASK	GENMASK(7, 0)
#define QOS_QOS_ID_MASK	GENMASK(15, 8)

#define QOS_SCS_TAG	0xB9
#define QOS_MSCS_TAG	0x58

#define QOS_INVALID_MSDUQ  0x3F

#define SCS_MSDUQ_MASK		GENMASK(5, 0)
#define SCS_QOS_ID_MASK		GENMASK(15, 6)

#define MSDUQ_MAX_DEF		16
#define MSDUQ_TID_MASK		GENMASK(2, 0)
#define MSDUQ_MASK		GENMASK(5, 3)

#define MSDUQ_TID		GENMASK(2, 0)
#define MSDUQ_FLOW_OVERRIDE	BIT(3)
#define MSDUQ_WHO_CL_INFO	GENMASK(5, 4)

#define QOS_NW_DELAY_MAX	0x3FFFF
#define QOS_NW_DELAY		GENMASK(23, 6)
#define QOS_NW_TAG_SHIFT	GENMASK(23, 16)
#define QOS_TAG_ID		GENMASK(31, 24)
#define QOS_NW_DELAY_SHIFT	0x6
#define QOS_VALID_TAG		BIT(30)
#define DP_RETRY_COUNT		7

struct ath12k_msduq {
	bool reserved;
	u8 qos_id;
	u32 tgt_opaque_id;
	u16 msduq;
};

struct ath12k_dl_scs {
	u16 qos_id_msduq;
};

struct ath12k_dp_peer_qos {
	struct ath12k_dl_scs scs_map[QOS_MAX_SCS_ID];
	struct ath12k_msduq msduq_map[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	void *telemetry_peer_ctx;
};

void ath12k_peer_unmap_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			     u8 *mac_addr, bool is_wds);
void ath12k_peer_map_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id, bool is_wds);
struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr(struct ath12k_dp_hw *dp_hw,
						   const u8 *addr);
struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr_and_sta(struct ath12k_dp_hw *dp_hw,
							   u8 *addr, struct ieee80211_sta *sta);
struct ath12k_dp_peer *ath12k_dp_peer_create_find(struct ath12k_dp_hw *dp_hw, u8 *addr,
						  struct ieee80211_sta *sta,
						  bool mlo_peer);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_vdev_id_and_addr(struct ath12k_dp *dp,
					     int vdev_id, const u8 *addr);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_addr(struct ath12k_dp *dp, const u8 *addr);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_id(struct ath12k_dp *dp, int peer_id);
bool ath12k_dp_link_peer_exist_by_vdev_id(struct ath12k_dp *dp, int vdev_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_pdev_idx(struct ath12k_dp *dp, u8 pdev_idx,
				     const u8 *addr);
int ath12k_dp_link_peer_rhash_tbl_init(struct ath12k_dp *dp);
void ath12k_dp_link_peer_rhash_tbl_destroy(struct ath12k_dp *dp);
int ath12k_dp_link_peer_rhash_add(struct ath12k_dp *dp,
				  struct ath12k_dp_link_peer *peer);
int ath12k_dp_link_peer_rhash_delete(struct ath12k_dp *dp,
				     struct ath12k_dp_link_peer *peer);
struct ath12k_dp_peer *ath12k_dp_peer_find_by_peerid_index(struct ath12k_dp *dp,
							   struct ath12k_pdev_dp *dp_pdev,
							   u16 peer_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_peerid_index(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev, u16 peer_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ml_peer_vdev_id(struct ath12k_dp *dp,
					    int peer_id,
					    int vdev_id);
static inline struct ieee80211_sta *
ath12k_dp_peer_get_sta(const struct ath12k_dp_peer *peer)
{
	return peer ? peer->sta : NULL;
}

static inline struct ieee80211_sta *
ath12k_dp_link_peer_get_sta(const struct ath12k_dp_link_peer *link_peer)
{
	return link_peer->dp_peer ? link_peer->dp_peer->sta : NULL;
}

static inline struct ieee80211_vif *
ath12k_dp_peer_get_vif(struct ath12k_dp_peer *peer)
{
	return peer ? peer->vif : NULL;
}

static inline struct ieee80211_vif *
ath12k_dp_link_peer_get_vif(struct ath12k_dp_link_peer *link_peer)
{
	return ath12k_dp_peer_get_vif(link_peer->dp_peer);
}

static inline
enum nl80211_iftype ath12k_dp_peer_get_vif_type(struct ath12k_dp_peer *dp_peer)
{
	struct ieee80211_vif *temp_vif = ath12k_dp_peer_get_vif(dp_peer);

	return temp_vif ? temp_vif->type : NL80211_IFTYPE_UNSPECIFIED;
}

static inline enum nl80211_iftype
ath12k_dp_link_peer_get_vif_type(struct ath12k_dp_link_peer *link_peer)
{
	struct ieee80211_vif *temp_vif = ath12k_dp_link_peer_get_vif(link_peer);

	return temp_vif ? temp_vif->type : NL80211_IFTYPE_UNSPECIFIED;
}

struct ath12k_dp_vif *ath12k_dp_peer_get_dp_vif(struct ath12k_dp_peer *dp_peer);

struct ath12k_dp_vif *
ath12k_dp_link_peer_get_dp_vif(struct ath12k_dp_link_peer *link_peer);

struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_get(struct ath12k_dp *dp,
		       struct ath12k_dp_peer *peer);
struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_alloc(struct ath12k_dp *dp,
			 struct ath12k_dp_peer *peer);
void ath12k_dp_peer_qos_free(struct ath12k_dp *dp,
			     struct ath12k_dp_peer *peer);
bool ath12k_dp_qos_stats_alloc(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct ath12k_dp_link_peer *peer);
int ath12k_dp_peer_scs_add(struct ath12k_pdev_dp *dp_pdev, u16 peer_id, u8 qm_id,
			   u16 qos_id);
int ath12k_dp_peer_scs_del(struct ath12k_pdev_dp *dp_pdev, u16 peer_id, u8 qm_id,
			   u16 *qos_id);
int ath12k_dp_peer_scs_data(struct ath12k_dp *dp,
			    struct ath12k_dp_peer_qos *qos, u8 scs_id,
			    struct ath12k_dp_link_peer *link_peer,
			    struct ath12k *ar,
			    u16 *msduq, u16 *qos_id);
u16 ath12k_dp_peer_scs_get_qos_id(struct ath12k_base *ab,
				  struct ath12k_dp_peer_qos *qos, u8 scs_id);
u16 ath12k_dp_peer_qos_msduq(struct ath12k_base *ab,
			     struct ath12k_dp_peer_qos *qos,
			     struct ath12k_dp_link_peer *link_peer,
			     struct ath12k *ar,
			     u16 qos_id, u8 svc_id);
u16 dp_peer_msduq_qos_id(struct ath12k_base *ab,
			 struct ath12k_dp_peer_qos *qos,
			 u16 msduq);
void ath12k_peer_qos_queue_ind_handler(struct ath12k_base *ab,
				       struct sk_buff *skb);
void ath12k_link_peer_free(struct ath12k_dp_link_peer *peer);
void ath12k_link_sta_hlist_delete(struct ath12k *ar, struct ath12k_link_sta *arsta);
struct ath12k_dp_peer *ath12k_dp_vdev_peer_find(struct ath12k_dp_hw *dp_hw,
						const u8 *addr, u8 hw_link_id);
struct ath12k_dp_peer *ath12k_dp_vdev_peer_check(struct ath12k_dp_hw *dp_hw,
						 u8 *addr, u8 hw_link_id);
u8 ath12k_dp_validate_hw_link_id(u8 hw_link_id);

/*
 * Peer Walk API - Walks across DP peers and performs the desired action.
 */
int ath12k_dp_peer_walk_action(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			       struct ath12k_dp_link_vif *dp_link_vif,
			       int (*action)(struct ath12k_dp *dp,
				       struct ath12k_dp_vif *,
				       struct ath12k_dp_link_vif *,
				       struct ath12k_dp_peer *, void *,
				       struct ath12k_dp_tx_msdu_info *),
			       void *app_data,
			       struct ath12k_dp_tx_msdu_info *msdu_info);
void ath12k_dp_iterate_vdev_link_peer(struct ath12k_dp *dp, int vdev_id,
				      void (*callback)(struct ath12k_dp *,
						       struct ath12k_dp_link_peer *));
void ath12k_dp_iterate_pdev_link_peer(struct ath12k_dp *dp, int pdev_idx,
				      void (*callback)(struct ath12k_dp *,
						       struct ath12k_dp_link_peer *));

int ath12k_dp_peer_stats_alloc(struct ath12k_dp_peer *dp_peer,
			       struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_peer_stats_free(struct ath12k_dp_peer *dp_peer);
int ath12k_dp_peer_link_stats_alloc(struct ath12k_dp_link_peer *link_peer,
				    struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_peer_link_stats_free(struct ath12k_dp_link_peer *link_peer);

bool ath12k_dp_hw_peer_stats_enabled(struct ath12k_pdev_dp *dp_pdev);

static inline void ath12k_peer_event_set_and_queue(struct ath12k_dp_link_peer *peer,
						   struct ath12k_event_queue *queue,
						   u32 event_flag)
{
	atomic_or(event_flag, &peer->event.common.flags);

	/* Queue the node once */
	if (!test_and_set_bit(ATH12K_EVENT_QUEUED, &peer->event.state))
		ath12k_event_enqueue(queue, &peer->event.common);
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_hw_link_id(struct ath12k_dp_peer *dp_peer, u8 hw_link_id);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_logical_link_id(struct ath12k_dp_peer *dp_peer, u8 link_id);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_mac_addr(struct ath12k_dp_peer *dp_peer,
				     const u8 *addr);
u16 ath12k_dp_get_peer_based_tcl_metadata(struct ath12k_dp *dp, u16 peer_id,
					  u8 valid_htt_ext);
#ifndef CPTCFG_EXT_IPA_OFFLOAD
static inline
void ath12k_dp_ipa_peer_unmap_event_wds(struct ath12k_base *ab, u8 vdev_id,
					u16 peer_id, u8 *mac_addr)
{
}

static inline
void ath12k_dp_ipa_peer_map_event_wds(struct ath12k_base *ab, u8 vdev_id,
				      u16 peer_id, u8 *mac_addr)
{
}
#endif /* !CPTCFG_EXT_IPA_OFFLOAD */

static inline
u8 ath12k_dp_peer_convert_logical_to_hw_link_id(struct ath12k_dp_peer *dp_peer,
						const u8 link_id)
{
	if (link_id >= ATH12K_NUM_MAX_LINKS)
		return ATH12K_DP_HW_LINK_ID_INVALID;

	return dp_peer->l2h_link_map[link_id];
}

static inline
u8 ath12k_dp_peer_convert_hw_to_logical_link_id(struct ath12k_dp_peer *dp_peer,
						const u8 hw_link_id)
{
	if (hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS)
		return 0;

	return dp_peer->hw_links[hw_link_id];
}

void
ath12k_dp_link_peer_iterate_by_dp_pdev(struct ath12k_pdev_dp *dp_pdev,
				       void (*iter_fn)(struct ath12k_pdev_dp *dp_pdev,
						       struct ath12k_dp_link_peer *peer,
						       void *context),
				       void *data);

void ath12k_dp_peer_hash_table_add(struct ath12k_dp_hw *dp_hw,
				   struct ath12k_dp_peer *dp_peer);
void ath12k_dp_peer_hash_table_delete(struct ath12k_dp_hw *dp_hw,
				      struct ath12k_dp_peer *dp_peer);
#endif
