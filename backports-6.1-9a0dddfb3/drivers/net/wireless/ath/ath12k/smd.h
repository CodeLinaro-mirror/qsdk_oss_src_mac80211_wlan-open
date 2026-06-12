/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_SMD_H
#define ATH12K_SMD_H

struct ath12k_sta;
struct ath12k_vif;

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
#endif /* ATH12K_SMD_H */
