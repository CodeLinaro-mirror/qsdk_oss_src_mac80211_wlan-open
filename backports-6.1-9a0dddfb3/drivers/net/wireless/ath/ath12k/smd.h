/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_SMD_H
#define ATH12K_SMD_H

#include <linux/ieee80211.h>
#include <net/mac80211.h>

struct ath12k_sta;
struct ath12k_vif;
struct ath12k_base;
struct ath12k_smd_info;
struct hal_reo_status;
struct ath12k_dp;
struct ath12k;

/**
 * struct ath12k_smd_peer_assoc_ctx - SMD BSS transition peer assoc context
 *
 * Groups the cross-STA context needed for Phase B PEER_ASSOC commands.
 * Passed to ath12k_smd_bss_assoc(), ath12k_smd_current_bss_assoc(),
 * ath12k_mac_peer_assoc_prepare_smd(), and ath12k_mac_peer_assoc_h_mlo_smd().
 *
 * @current_ahsta:   Current AP STA (ath12k_sta) — DL drain STA
 * @target_ahsta:    Target AP STA (ath12k_sta)  — being added
 * @ahvif:           VIF (used to access link vifs)
 * @primary_link_id: DL drain link of current AP STA (e.g. 0 for 2.4GHz)
 */
struct ath12k_smd_peer_assoc_ctx {
	struct ath12k_sta *current_ahsta;
	struct ath12k_sta *target_ahsta;
	struct ath12k_vif *ahvif;
	u8                 primary_link_id;
};


/* SMD driver op implementations (called from mac.c thin dispatchers) */
int ath12k_smd_uhr_link_reconfig(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *current_sta,
				 struct ieee80211_sta *target_sta,
				 enum ieee80211_uhr_link_reconfig_action action,
				 struct ieee80211_uhr_link_reconfig_info *info);
int ath12k_smd_uhr_smd_update(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *peer,
			      u32 role, u32 type, u32 status,
			      u32 dl_sn, u32 ul_sn, u32 dl_drain_time);
int ath12k_smd_remap_links_op(struct ath12k_vif *ahvif,
			      struct ath12k_sta *ahsta_target,
			      const struct ieee80211_uhr_link_reconfig_info *info);

/* Threshold (in us) to reuse last received SMD context instead of
 * pulling a fresh one.
 */
#define ATH12K_SMD_CTX_REUSE_THRESHOLD 500

/* copy of ATH12K_DP_MAX_POSSIBLE_BA_WIN */
#define ATH12K_SMD_BA_WIN_SIZE_MAX 0x400

#define ATH12K_SMD_CTX_NUM_VALID_CTX    8
#define ATH12K_SMD_CTX_VALID_DL_SN      0
#define ATH12K_SMD_CTX_VALID_UL_SN      1
#define ATH12K_SMD_CTX_VALID_PN         2
#define ATH12K_SMD_CTX_VALID_BA_PARAMS  3
#define ATH12K_SMD_CTX_VALID_QOS        4

struct ath12k_smd_ctx_ba {
	u16 amsdu_supported:1,
	    ba_policy:1,
	    buffer_size:10;
	u16 timeout;
	u16 ext_no_frag:1,
	    extfrag_level:1,
	    ext_buffer_size:10;
};

/* Vendor Context and TLVs */

struct ath12k_smd_reo_bitmap {
	struct_group(bitmap_287_0_grp,
		u32 bitmap_31_0;
		u32 bitmap_63_32;
		u32 bitmap_95_64;
		u32 bitmap_127_96;
		u32 bitmap_159_128;
		u32 bitmap_191_160;
		u32 bitmap_223_192;
		u32 bitmap_255_224;
		u32 bitmap_287_256;
	);
	struct_group(bitmap_1023_288_grp,
		u32 bitmap_319_288;
		u32 bitmap_351_320;
		u32 bitmap_383_352;
		u32 bitmap_415_384;
		u32 bitmap_447_416;
		u32 bitmap_479_448;
		u32 bitmap_511_480;
		u32 bitmap_543_512;
		u32 bitmap_575_544;
		u32 bitmap_607_576;
		u32 bitmap_639_608;
		u32 bitmap_671_640;
		u32 bitmap_703_672;
		u32 bitmap_735_704;
		u32 bitmap_767_736;
		u32 bitmap_799_768;
		u32 bitmap_831_800;
		u32 bitmap_863_832;
		u32 bitmap_895_864;
		u32 bitmap_927_896;
		u32 bitmap_959_928;
		u32 bitmap_991_960;
		u32 bitmap_1023_992;
	);
} __packed;

/* Any modification should be part of a new version */
struct ath12k_smd_ctx_vendor_v1 {
	u16 dl_mgmt_sn;
	u8 dl_mgmt_pn[IEEE80211_MAX_PN_LEN];

	u16 ul_mgmt_sn;
	u8 ul_mgmt_pn[IEEE80211_MAX_PN_LEN];

	u16 dl_data_lsn_offset[IEEE80211_MAX_NUM_TIDS];

	struct ath12k_smd_reo_bitmap ul_reo_bmap[IEEE80211_MAX_NUM_TIDS];
};

struct ath12k_smd_ctx {
	DECLARE_BITMAP(valid_ctx_bmap, ATH12K_SMD_CTX_NUM_VALID_CTX);
	u8 pn_len; /* Length of PN which varies based on the cipher type */

	struct {
		DECLARE_BITMAP(valid_tid_bmap, IEEE80211_MAX_NUM_TIDS);
		DECLARE_BITMAP(completed_tid_bmap, IEEE80211_MAX_NUM_TIDS);
		u16 sn[IEEE80211_MAX_NUM_TIDS];
		u8 pn[IEEE80211_MAX_PN_LEN];
		struct ath12k_smd_ctx_ba ba[IEEE80211_MAX_NUM_TIDS];
	} dl;
	struct {
		DECLARE_BITMAP(valid_tid_bmap, IEEE80211_MAX_NUM_TIDS);
		DECLARE_BITMAP(completed_tid_bmap, IEEE80211_MAX_NUM_TIDS);
		u16 sn[IEEE80211_MAX_NUM_TIDS];
		u8 pn[IEEE80211_MAX_NUM_TIDS][IEEE80211_MAX_PN_LEN];
		struct ath12k_smd_ctx_ba ba[IEEE80211_MAX_NUM_TIDS];
	} ul;

	/* Vendor context */
	struct {
		u8 version;
		union {
			struct ath12k_smd_ctx_vendor_v1 ctx_v1;
		};
	} vendor_ctx;
};

struct ath12k_smd_ctx_req {
	enum ieee80211_uhr_link_reconf_resp_type type;
	DECLARE_BITMAP(wait_for_1k_status_ctx, IEEE80211_MAX_NUM_TIDS);
	bool tx_done; /* mark Tx HW block completion */
	bool rx_done; /* mark Rx HW block completion */
	struct ath12k_smd_ctx ctx;
	struct sk_buff *mmpdu;
	u8 sta_addr[ETH_ALEN];
	ktime_t enqueued_ts;
	void (*handler)(struct ath12k_smd_info *smd_info, struct ath12k_smd_ctx_req *req);
};

struct ath12k_smd_ctx_tx_cb_per_tid {
	u16 sn;
	u16 lsn_offset;
	u8 pn_len;
	u8 pn[IEEE80211_CCMP_256_MIC_LEN];
};

int ath12k_smd_global_init(struct ath12k_base *ab);
void ath12k_smd_global_deinit(void);

static inline
const char *ath12k_uhr_reconf_type_str(enum ieee80211_uhr_link_reconf_resp_type type)
{
	return type == IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP ? "Prep" : "Exec";
}

int ath12k_smd_collect_sta_session_ctx(struct ath12k *ar, struct sk_buff *skb);
void ath12k_smd_update_ctx_to_stack(struct ath12k_smd_info *smd_info,
				    struct ath12k_smd_ctx_req *req);

#endif /* ATH12K_SMD_H */
