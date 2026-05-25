// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>

#include "mac.h"
#include "smd.h"
#include "core.h"
#include "wmi.h"
#include "dp_cmn.h"
#include "debug.h"
#include "peer.h"

static int ath12k_smd_ctx_vendor_version = 1;

static void ath12k_smd_ctx_hw_tx_tid_cb(struct ath12k_dp *dp, void *cb_ctx,
					u8 *addr, u8 tid);
static void ath12k_smd_ctx_hw_rx_tid_cb(struct ath12k_dp *dp, void *cb_ctx,
					struct hal_reo_status *reo_status);

static int ath12k_uhr_smd_transfer_ext_ctx(struct ath12k_vif *ahvif,
					   struct ieee80211_sta *current_sta,
					   struct ieee80211_sta *target_sta,
					   struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_sta *current_ahsta = ath12k_sta_to_ahsta(current_sta);
	struct ath12k_link_sta *current_arsta;
	void *current_dp_peer = NULL;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;

	current_arsta = wiphy_dereference(ahvif->ah->hw->wiphy,
					  current_ahsta->link[info->primary_link_id]);

	if (!current_arsta || !current_arsta->arvif) {
		ath12k_hw_warn(ahvif->ah, "smd prep: current arsta not found\n");
		return -ENOENT;
	}

	ab = current_arsta->arvif->ar->ab;
	dp = ath12k_ab_to_dp(ab);
	dp_hw = &current_arsta->arvif->ar->ah->dp_hw;

	if (ath12k_dp_arch_smd_prep_rx_tid(dp, dp_hw, current_sta->addr))
		ath12k_warn(ab,
			    "smd prep: rx_tid park failed for %pM (non-fatal)\n",
			    current_sta->addr);

	current_dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ahvif->ah->hw->wiphy,
							      current_ahsta);
	if (!current_dp_peer) {
		ath12k_warn(ab,
			    "smd prep: failed to find current dp_peer (%pM)\n",
			    current_sta->addr);
		return -ENOENT;
	}

	return ath12k_dp_arch_smd_prep_transfer_ext_ctx(dp,
			current_dp_peer, info->target_ap_mld_addr,
			(u16)info->transitioning_links);
}

static int ath12k_smd_bss_assoc(struct ath12k *ar,
				struct ath12k_link_vif *arvif,
				struct ath12k_link_sta *arsta,
				struct ieee80211_uhr_link_transfer_info *link_info,
				u16 target_aid,
				const struct ath12k_smd_peer_assoc_ctx *ctx)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_wmi_vdev_up_params params = {};
	struct ath12k_wmi_peer_assoc_arg *peer_arg;
	struct ieee80211_link_sta *link_sta;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	void *dp_peer;
	struct ieee80211_sta *target_ap_sta;
	u8 *target_mld_addr;
	struct ieee80211_sta_he_cap he_cap;
	struct ieee80211_sta_ht_cap ht_cap;
	struct ieee80211_he_6ghz_capa he_6ghz_cap;
	union ath12k_config_param val;
	u32 hemode = 0, bandwidth;

	bool is_auth = false;
	int ret;

	if (WARN_ON(!arvif->is_started))
		return -EINVAL;

	target_ap_sta  = ath12k_ahsta_to_sta(arsta->ahsta);
	target_mld_addr = target_ap_sta->addr;

	peer_arg = kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	rcu_read_lock();
	link_sta = ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		rcu_read_unlock();
		kfree(peer_arg);
		ath12k_warn(ar->ab, "smd bss_assoc: link_sta not found for vdev %u\n",
			    arvif->vdev_id);
		return -ENOENT;
	}

	he_cap      = link_sta->he_cap;
	ht_cap      = link_sta->ht_cap;
	he_6ghz_cap = link_sta->he_6ghz_capa;
	bandwidth   = ath12k_mac_ieee80211_sta_bw_to_wmi(ar, link_sta);

	if (ctx)
		ath12k_mac_peer_assoc_prepare_smd(ar, arvif, arsta, peer_arg,
					      false, link_sta, ctx);
	else
		ath12k_peer_assoc_prepare(ar, arvif, arsta, peer_arg,
					  false, link_sta);

	ret = ath12k_mac_vif_recalc_sta_he_txbf(ar, arvif, &he_cap, &hemode);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: failed to recalc he txbf for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		rcu_read_unlock();
		kfree(peer_arg);
		return ret;
	}

	spin_lock_bh(&ar->data_lock);
	arsta->bw = bandwidth;
	spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_SET_HEMU_MODE, hemode);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: failed to set hemu mode for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		kfree(peer_arg);
		return ret;
	}

	peer_arg->is_assoc = true;
	/* UHR ML Reconfig: when ctx is provided, ath12k_mac_peer_assoc_h_mlo_smd()
	 * already set ml_reconfig=true and mlo_link_add=true in the peer_arg.
	 * When ctx is NULL (legacy path), set them manually.
	 */
	if (!ctx) {
		peer_arg->ml.ml_reconfig = true;
		peer_arg->ml.mlo_link_add = true;
	}

	ret = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
	kfree(peer_arg);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: peer_assoc failed for %pM vdev %u: %d\n",
			    link_info->target_bssid, arvif->vdev_id, ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: peer_assoc timeout for %pM vdev %u\n",
			    link_info->target_bssid, arvif->vdev_id);
		return -ETIMEDOUT;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ath12k_ar_to_hw(ar)->wiphy,
						      arsta->ahsta);
	if (!dp_peer)
		return -EINVAL;

	ret = ath12k_dp_link_peer_get_param_by_dp_peer_and_link_mac(dp_peer, arsta->addr,
							ATH12K_DP_LINK_PEER_ASSOC_PARAM,
							&val);
	if (!ret && !val.assoc_success) {
		ath12k_warn(ar->ab, "peer assoc failure from firmware %pM\n",
			    arsta->addr);
		return -EINVAL;
	}

	if (val.is_authorized)
		is_auth = true;

	ath12k_dp_arch_link_peer_assoc(dp, &ar->ah->dp_hw,
				       target_mld_addr, ar->hw_link_id);

	ret = ath12k_setup_peer_smps(ar, arvif, link_info->target_bssid,
				     &ht_cap, &he_6ghz_cap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: failed to setup peer SMPS for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	ether_addr_copy(arvif->bssid, link_info->target_bssid);
	ahvif->aid = target_aid;

	params.vdev_id = arvif->vdev_id;
	params.aid     = target_aid;
	params.bssid   = link_info->target_bssid;

	ret = ath12k_wmi_vdev_up(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd bss_assoc: vdev_up failed for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	arvif->is_up = true;
	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac smd bss  link %u vdev %d up (associated) with bssid %pM (dormant) aid %d\n",
		   arvif->link_id, arvif->vdev_id, link_info->target_bssid,
		   target_aid);

	if (!is_zero_ether_addr(ahvif->smd.target_mld_addr) &&
	    ahvif->smd.exec_in_progress) {
		struct ath12k_base *ab = arvif->ar->ab;
		struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
		struct ath12k_dp_hw *dp_hw = &arvif->ar->ah->dp_hw;
		u8 primary_link_id = arvif->link_id;
		const u8 *target_mld_addr = ahvif->smd.target_mld_addr;

		ath12k_dp_arch_smd_exec_activate_links(dp, dp_hw,
						       &ahvif->dp_vif,
						       target_mld_addr,
						       BIT(arvif->ar->hw_link_id));

		if (ath12k_dp_arch_smd_exec_rx_tid(dp, dp_hw, target_mld_addr))
			ath12k_warn(ab,
				    "smd: rx_tid restore skipped %pM (no parked info)\n",
				    target_mld_addr);

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "smd bss_assoc: activated partner link %u hw_link_id %u\n",
			   primary_link_id, arvif->ar->hw_link_id);
	}

	if (is_auth) {
		ret = ath12k_wmi_set_peer_param(ar, arvif->bssid,
						arvif->vdev_id,
						WMI_PEER_AUTHORIZE,
						1);
		if (ret)
			ath12k_warn(ar->ab, "Unable to authorize BSS peer: %d\n", ret);
	}

	if (test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ar->ab->wmi_ab.svc_map) &&
	    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE)
		ath12k_mac_11d_scan_stop_all(ar->ab);

	return 0;
}

static int ath12k_smd_current_bss_assoc(const struct ath12k_smd_peer_assoc_ctx *ctx)
{
	struct ath12k_vif *ahvif         = ctx->ahvif;
	struct ath12k_sta *current_ahsta = ctx->current_ahsta;
	u8 primary_link_id               = ctx->primary_link_id;
	struct ieee80211_hw *hw = ahvif->ah->hw;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	struct ath12k_wmi_peer_assoc_arg *peer_arg;
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_sta_he_cap he_cap;
	u32 hemode = 0;
	struct ath12k *ar;
	int ret;

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[primary_link_id]);
	arsta = wiphy_dereference(hw->wiphy, current_ahsta->link[primary_link_id]);

	if (arvif && arvif->ar)
		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_SMD,
			   "smd: current_bss_assoc primary=%u arvif=%s arsta=%s\n",
			   primary_link_id,
			   arvif->is_started ? "started" : "stopped",
			   arsta ? "found" : "NULL");

	if (!arvif || !arsta || !arvif->ar) {
		ath12k_hw_warn(ahvif->ah,
			       "smd current_bss_assoc: primary link %u not found\n",
			       primary_link_id);
		return -ENOENT;
	}

	if (!arvif->is_started) {
		ath12k_hw_warn(ahvif->ah,
			       "smd current_bss_assoc: primary vdev %u not started\n",
			       arvif->vdev_id);
		return -EINVAL;
	}

	ar = arvif->ar;

	peer_arg = kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	rcu_read_lock();
	link_sta = ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		rcu_read_unlock();
		kfree(peer_arg);
		ath12k_warn(ar->ab,
			    "smd current_bss_assoc: link_sta not found for primary link %u\n",
			    primary_link_id);
		return -ENOENT;
	}

	he_cap = link_sta->he_cap;

	ath12k_mac_peer_assoc_prepare_smd(ar, arvif, arsta, peer_arg,
				      true,  /* reassoc */
				      link_sta, ctx);

	ret = ath12k_mac_vif_recalc_sta_he_txbf(ar, arvif, &he_cap, &hemode);
	if (ret) {
		rcu_read_unlock();
		kfree(peer_arg);
		ath12k_warn(ar->ab,
			    "smd current_bss_assoc: failed to recalc he txbf for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	rcu_read_unlock();

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_SET_HEMU_MODE, hemode);
	if (ret) {
		kfree(peer_arg);
		ath12k_warn(ar->ab,
			    "smd current_bss_assoc: failed to set hemu mode for vdev %u: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	peer_arg->is_assoc = true;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "smd current_bss_assoc: PEER_ASSOC for current AP STA primary link %u vdev %u\n",
		   primary_link_id, arvif->vdev_id);

	ret = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
	kfree(peer_arg);
	if (ret) {
		ath12k_warn(ar->ab,
			    "smd current_bss_assoc: peer_assoc failed for primary link %u: %d\n",
			    primary_link_id, ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath12k_warn(ar->ab,
			    "smd current_bss_assoc: peer_assoc timeout for primary link %u\n",
			    primary_link_id);
		return -ETIMEDOUT;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "smd current_bss_assoc: done for primary link %u\n",
		   primary_link_id);
	return 0;
}

static int ath12k_uhr_prepare_links(struct ath12k_vif *ahvif,
				    struct ieee80211_sta *current_sta,
				    struct ieee80211_sta *target_sta,
				    struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ieee80211_hw *hw = ahvif->ah->hw;
	struct ath12k_sta *target_ahsta;
	struct ath12k_sta *current_ahsta = NULL;
	struct ath12k_smd_peer_assoc_ctx ctx = {};
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	unsigned long links;
	struct ath12k *ar;
	u8 link_id;
	int ret;

	links = info->transitioning_links;

	if (!target_sta)
		return 0;

	target_ahsta = ath12k_sta_to_ahsta(target_sta);

	target_ahsta->primary_link_id = info->primary_link_id;

	if (current_sta) {
		current_ahsta = ath12k_sta_to_ahsta(current_sta);
		ctx.current_ahsta  = current_ahsta;
		ctx.target_ahsta   = target_ahsta;
		ctx.ahvif          = ahvif;
		ctx.primary_link_id = info->primary_link_id;
	}

	{
		u8 new_assoc_link_id = info->primary_link_id; /* fallback */
		unsigned long tl = info->transitioning_links;
		unsigned long alinks;
		u8 lid;

		if (current_ahsta &&
		    test_bit(current_ahsta->assoc_link_id, &tl))
			new_assoc_link_id = current_ahsta->assoc_link_id;

		target_ahsta->assoc_link_id = new_assoc_link_id;

		alinks = target_ahsta->links_map;
		for_each_set_bit(lid, &alinks, IEEE80211_MLD_MAX_NUM_LINKS) {
			struct ath12k_link_sta *arsta_tmp =
				wiphy_dereference(hw->wiphy,
						  target_ahsta->link[lid]);
			if (arsta_tmp)
				arsta_tmp->is_assoc_link =
					(lid == new_assoc_link_id);
		}

		if (current_ahsta) {
			alinks = current_ahsta->links_map;
			for_each_set_bit(lid, &alinks,
					 IEEE80211_MLD_MAX_NUM_LINKS) {
				struct ath12k_link_sta *arsta_cur =
					wiphy_dereference(hw->wiphy,
							  current_ahsta->link[lid]);
				if (arsta_cur)
					arsta_cur->is_assoc_link =
						(lid == new_assoc_link_id);
			}
		}
	}

	if (target_sta) {
		ret = ath12k_uhr_smd_transfer_ext_ctx(ahvif, current_sta,
						      target_sta, info);
		if (ret) {
			ath12k_hw_warn(ahvif->ah,
				       "smd prep: ext_ctx transfer failed: %d\n", ret);
			/* Non-fatal: continue — flowq params will be missing */
		} else {
			struct ath12k *first_ar = NULL;
			struct ath12k_link_vif *primary_arvif =
				wiphy_dereference(hw->wiphy,
						  ahvif->link[info->primary_link_id]);
			if (primary_arvif && primary_arvif->ar)
				first_ar = primary_arvif->ar;

			if (first_ar && info->transitioning_links) {
				ret = ath12k_dp_arch_peer_assoc(first_ar->ab->dp,
								&first_ar->ah->dp_hw,
								&ahvif->dp_vif,
								info->target_ap_mld_addr);
				if (ret)
					ath12k_hw_warn(ahvif->ah,
						       "smd prep: dp_peer_assoc failed for %pM: %d\n",
						       info->target_ap_mld_addr,
						       ret);
			} else if (first_ar && !info->transitioning_links) {
				ath12k_dbg(first_ar->ab, ATH12K_DBG_MAC,
					   "smd prep: skipping dp_peer_assoc for %pM (transitioning_links=0, peer setup deferred to EXEC)\n",
					   info->target_ap_mld_addr);
			}
		}
	}

	if (current_sta && target_sta && ctx.current_ahsta) {
		ret = ath12k_smd_current_bss_assoc(&ctx);
		if (ret)
			ath12k_hw_warn(ahvif->ah,
				       "uhr prepare: current_bss_assoc failed: %d\n",
				       ret);
	}

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ieee80211_uhr_link_transfer_info *link_info;

		if (link_id == info->primary_link_id)
			continue; /* Skip primary - preserved with Current AP */

		link_info = &info->links[link_id];
		if (!link_info->valid)
			continue;

		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(hw->wiphy, target_ahsta->link[link_id]);
		if (!arvif || !arsta) {
			ath12k_dbg(NULL, ATH12K_DBG_MAC,
				   "uhr prepare: link %u arvif or arsta not found\n",
				   link_id);
			continue;
		}

		ar = arvif->ar;
		if (!ar)
			continue;

		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "uhr prepare link %u: vdev %u target_bssid=%pM\n",
			   link_id, arvif->vdev_id, link_info->target_bssid);
		if (!arvif->is_started) {
			ath12k_warn(ar->ab,
				    "uhr prepare: vdev %u not started\n",
				    arvif->vdev_id);
			continue;
		}

		ret = ath12k_smd_bss_assoc(ar, arvif, arsta, link_info,
					   info->target_aid,
					   ctx.current_ahsta ? &ctx : NULL);
		if (ret) {
			ath12k_warn(ar->ab,
				    "uhr prepare: ath12k_smd_bss_assoc failed for link %u vdev %u: %d\n",
				    link_id, arvif->vdev_id, ret);
			continue;
		}
	}

	return 0;
}

static void ath12k_uhr_smd_activate_ext_ctx(struct ath12k_vif *ahvif,
					    struct ieee80211_sta *current_sta,
					    struct ieee80211_sta *target_sta,
					    struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_sta *target_ahsta = ath12k_sta_to_ahsta(target_sta);
	unsigned long mac_links = info->transitioning_links;
	struct ath12k_link_sta *target_arsta;
	struct ath12k_dp_hw *dp_hw;
	u16 active_hw_links = 0;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	unsigned int link_id;

	/* DP activation requires at least one link to be set up on the target.
	 * For SLO exec_path=0, this is deferred to DL drain completion (Phase B)
	 * when the primary link is set up. BA params are captured separately in
	 * ath12k_uhr_execute_transition before any link teardown begins.
	 */
	if (!target_ahsta->links_map) {
		ath12k_hw_warn(ahvif->ah,
			       "smd exec: target ahsta has no links (SLO - DP deferred)\n");
		return;
	}

	target_arsta = wiphy_dereference(ahvif->ah->hw->wiphy,
				target_ahsta->link[__ffs(target_ahsta->links_map)]);

	if (!target_arsta || !target_arsta->arvif || !target_arsta->arvif->ar) {
		ath12k_hw_warn(ahvif->ah, "smd exec: target arsta not found\n");
		return;
	}

	ab = target_arsta->arvif->ar->ab;
	dp = ath12k_ab_to_dp(ab);
	dp_hw = &target_arsta->arvif->ar->ah->dp_hw;

	for_each_set_bit(link_id, &mac_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ath12k_link_sta *arsta =
			wiphy_dereference(ahvif->ah->hw->wiphy,
					  target_ahsta->link[link_id]);
		if (arsta && arsta->arvif && arsta->arvif->ar)
			active_hw_links |= BIT(arsta->arvif->ar->hw_link_id);
	}

	ath12k_dp_arch_smd_exec_activate_links(dp, dp_hw,
					       &ahvif->dp_vif,
					       target_sta->addr,
					       active_hw_links);

	if (ath12k_dp_arch_smd_exec_rx_tid(dp, dp_hw, target_sta->addr))
		ath12k_warn(ab,
			    "smd: rx_tid restore skipped %pM (no parked info)\n",
			    target_sta->addr);
}

static int ath12k_uhr_execute_transition(struct ath12k_vif *ahvif,
					 struct ieee80211_sta *current_sta,
					 struct ieee80211_sta *target_sta,
					 struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_sta *target_ahsta = ath12k_sta_to_ahsta(target_sta);
	struct ath12k_sta *current_ahsta = ath12k_sta_to_ahsta(current_sta);

	ath12k_dbg(NULL, ATH12K_DBG_SMD,
		   "smd: execute_transition links=0x%x primary=%u\n",
		   info->transitioning_links, info->primary_link_id);

	ahvif->smd.exec_in_progress = true;
	if (ahvif->ah->ag && ahvif->ah->ag->dp_hw_grp)
		ahvif->ah->ag->dp_hw_grp->smd_exec_in_progress = true;

	if (current_sta && target_sta) {
		target_ahsta->assoc_link_id = current_ahsta->assoc_link_id;
		ath12k_dbg(NULL, ATH12K_DBG_SMD,
			   "smd: execute_transition sap=%pM tap=%pM assoc_link=%u\n",
			   current_sta->addr, target_sta->addr,
			   target_ahsta->assoc_link_id);
		spin_lock_bh(&current_ahsta->ba_lock);
		spin_lock_bh(&target_ahsta->ba_lock);
		memcpy(target_ahsta->rx_ba_params, current_ahsta->rx_ba_params,
		       sizeof(target_ahsta->rx_ba_params));
		memcpy(target_ahsta->tx_ba_params, current_ahsta->tx_ba_params,
		       sizeof(target_ahsta->tx_ba_params));
		spin_unlock_bh(&target_ahsta->ba_lock);
		spin_unlock_bh(&current_ahsta->ba_lock);
	}

	if (target_sta)
		ath12k_uhr_smd_activate_ext_ctx(ahvif, current_sta, target_sta, info);

	return 0;
}

static int ath12k_uhr_abort_transition(struct ath12k_vif *ahvif,
				       struct ieee80211_sta *current_sta,
				       struct ieee80211_sta *target_sta,
				       struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_hw *ah = ahvif->ah;

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "uhr abort transition: links=0x%x\n",
		   info->transitioning_links);

	ahvif->smd.exec_in_progress = false;
	if (ahvif->ah->ag && ahvif->ah->ag->dp_hw_grp)
		ahvif->ah->ag->dp_hw_grp->smd_exec_in_progress = false;
	eth_zero_addr(ahvif->smd.target_mld_addr);
	ahvif->smd.transitioning_links = 0;

	ath12k_hw_warn(ah, "uhr_abort_transition: WMI sequence not implemented - requires FW support\n");
	return 0;
}

static inline enum smd_roam_config_cmd_type
uhr_action_to_smd_cmd(enum ieee80211_uhr_link_reconfig_action action)
{
	switch (action) {
	case IEEE80211_UHR_LINK_RECONFIG_PREPARE_REQ:
		return SMD_ROAM_CONFIG_CMD_PREP_REQ;
	case IEEE80211_UHR_LINK_RECONFIG_PREPARE_RESP:
		return SMD_ROAM_CONFIG_CMD_PREP_RESP;
	case IEEE80211_UHR_LINK_RECONFIG_EXECUTE_REQ:
		return SMD_ROAM_CONFIG_CMD_EXEC_REQ;
	case IEEE80211_UHR_LINK_RECONFIG_EXECUTE_RESP:
		return SMD_ROAM_CONFIG_CMD_EXEC_RESP;
	case IEEE80211_UHR_LINK_RECONFIG_DYNAMIC_CONTEXT:
		return SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT;
	case IEEE80211_UHR_LINK_RECONFIG_ABORT:
		return SMD_ROAM_CONFIG_CMD_TERMINATION;
	default:
		WARN_ON(1);
		return SMD_ROAM_CONFIG_CMD_PREP_REQ; /* fallback */
	}
}

static void ath12k_mac_smd_roam_config(struct ieee80211_hw *hw,
				      struct ath12k_vif *ahvif,
				      struct ath12k_sta *ahsta,
				      struct ath12k_sta *target_ahsta,
				      enum ieee80211_uhr_link_reconfig_action action,
				      struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_wmi_smd_roam_config_arg arg;
	struct ath12k_link_vif *arvif;
	unsigned long valid_links;
	int ret;
	u32 sn_flags = 0;
	u32 link_id;
	u32 role;

	lockdep_assert_wiphy(hw->wiphy);
	role = (ahvif->vif->type == NL80211_IFTYPE_STATION) ?
		SMD_ROAM_CONFIG_ROLE_STA : SMD_ROAM_CONFIG_ROLE_SERVING_AP;

	if (info->request_dl_sn_not_transferred)
		sn_flags |= BIT(0);
	if (info->request_ul_sn_not_transferred)
		sn_flags |= BIT(1);
	ath12k_dbg(NULL, ATH12K_DBG_SMD, "smd roam_config sn_flags:0x%x\n",
		   sn_flags);
	valid_links = ahsta->links_map;
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif || !arvif->is_created)
			continue;
		if (WARN_ON(!arvif->ar))
			continue;
		memset(&arg, 0, sizeof(arg));

		arg.vdev_id       = arvif->vdev_id;
		arg.role          = role;
		arg.status        = SMD_ROAM_CONFIG_STATUS_SUCCESS;
		arg.cmd_type      = uhr_action_to_smd_cmd(action);
		arg.flags         = sn_flags;
		arg.dl_drain_time = info->dl_drain_time_tu;

		if (role == SMD_ROAM_CONFIG_ROLE_STA &&
		    (info->transitioning_links & BIT(link_id)))
			arg.flags |= BIT(2);

		if (info->transitioning_links & BIT(link_id))
			arg.flags |= SMD_ROAM_CONFIG_FLAG_DISABLE_LINK;

		if (ahsta) {
			struct ath12k_link_sta *arsta =
				wiphy_dereference(hw->wiphy,
						  ahsta->link[link_id]);
			u8 tid;

			if (WARN_ON(!arsta))
				continue;
			ether_addr_copy(arg.peer_mac, arsta->addr);
			spin_lock_bh(&ahsta->ba_lock);
			for (tid = 0; tid < IEEE80211_MAX_NUM_TIDS; tid++) {
				arg.peer_tid_info[tid].tx_buf_size =
					ahsta->tx_ba_params[tid].buf_size;
				arg.peer_tid_info[tid].rx_buf_size =
					ahsta->rx_ba_params[tid].buf_size;
			}
			spin_unlock_bh(&ahsta->ba_lock);
		}
		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_SMD,
			   "smd: roam_config vdev=%u peer=%pM flags=0x%x dl_drain=%u\n",
			   arg.vdev_id, arg.peer_mac, arg.flags,
			   arg.dl_drain_time);
		ret = ath12k_wmi_send_smd_roam_config(arvif->ar, &arg);
		if (ret) {
			ath12k_warn(arvif->ar->ab,
				    "failed SMD roam config on link %u (ret=%d)\n",
				    link_id, ret);
		}
	}

	valid_links = target_ahsta ? target_ahsta->links_map : 0;
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif || !arvif->is_created)
			continue;
		if (WARN_ON(!arvif->ar))
			continue;
		memset(&arg, 0, sizeof(arg));

		arg.vdev_id       = arvif->vdev_id;
		arg.role          = role;
		arg.status        = SMD_ROAM_CONFIG_STATUS_SUCCESS;
		arg.cmd_type      = uhr_action_to_smd_cmd(action);
		arg.flags         = sn_flags;
		arg.dl_drain_time = info->dl_drain_time_tu;

		if (role == SMD_ROAM_CONFIG_ROLE_STA &&
		    (info->transitioning_links & BIT(link_id)))
			arg.flags |= BIT(2);


		if (target_ahsta) {
			struct ath12k_link_sta *arsta =
				wiphy_dereference(hw->wiphy,
						  target_ahsta->link[link_id]);

			if (WARN_ON(!arsta))
				continue;
			ether_addr_copy(arg.peer_mac, arsta->addr);
		}
		ret = ath12k_wmi_send_smd_roam_config(arvif->ar, &arg);
		if (ret) {
			ath12k_warn(arvif->ar->ab,
				    "failed SMD roam config on link %u (ret=%d)\n",
				    link_id, ret);
		}
	}
}

/* Must be called with wiphy lock held */
/**
 * update_smd_forall_links_locked - Send SMD roam config WMI command on all
 *                                   active links of an MLD vif.
 *
 * @hw:    ieee80211_hw (wiphy lock assertion + link dereferencing)
 * @ahvif: ath12k per-vif state; ahvif->vif used to derive STA vs AP role
 * @ahsta: ath12k per-sta state; ahsta->sta->addr used as peer_mac.
 *         Pass NULL when no peer address is needed.
 * @info:  UHR link reconfiguration parameters:
 *           - info->action:   REQ vs RESP (WLAN_PROTECTED_UHR_ACTION_*)
 *           - info->type:     ST_PREP vs ST_EXEC
 *           - info->status:   status code (response frames)
 *           - info->transitioning_links: bitmap of links being transitioned
 *           - info->dl_drain_time_tu:    DL drain period in TUs
 *           - info->request_dl/ul_sn_not_transferred: SN flags
 *
 * Must be called with wiphy mutex held.
 */
static int update_smd_forall_links_locked(struct ieee80211_hw *hw,
					  struct ath12k_vif *ahvif, /* Self */
					  struct ath12k_sta *ahsta, /* Peer */
					  u16 link_bitmap,
					  u32 role, u32 type, u32 status,
					  u32 dl_sn_not_transferred,
					  u32 ul_sn_not_transferred,
					  u32 dl_drain_time)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_wmi_smd_roam_config_arg arg;
	int ret;
	u32 link_id;

	lockdep_assert_wiphy(hw->wiphy);

	/* Iterate through each arvif and find the corresponding */

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif || !arvif->is_created)
			continue;

		if (WARN_ON(!arvif->ar))
			continue;

		memset(&arg, 0, sizeof(arg));

		arg.vdev_id = arvif->vdev_id;
		arg.role = role;
		arg.status = status;

		switch (type) {
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_PREP_REQ:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_PREP_REQ;
			break;
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_PREP_RESP:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_PREP_RESP;
			break;
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_EXEC_REQ:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_EXEC_REQ;
			break;
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_EXEC_RESP:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_EXEC_RESP;
			break;
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_DYNAMIC_CONTEXT:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT;
			break;
		case IEEE80211_SMD_ROAM_CONFIG_TYPE_TERMINATION:
			arg.cmd_type = SMD_ROAM_CONFIG_CMD_TERMINATION;
			break;
		default:
			WARN_ON_ONCE(1);
			return -EINVAL;
		}

		if (dl_sn_not_transferred)
			arg.flags |= BIT(0);

		if (ul_sn_not_transferred)
			arg.flags |= BIT(1);

		arg.dl_drain_time = dl_drain_time;

		if (role == SMD_ROAM_CONFIG_ROLE_STA &&
		    (link_bitmap & BIT(link_id)))
			arg.flags |= 0x4;

		if (ahsta) {
			struct ath12k_link_sta *arsta =
				wiphy_dereference(hw->wiphy,
						  ahsta->link[link_id]);
			int i;

			if (WARN_ON(!arsta))
				continue;
			ether_addr_copy(arg.peer_mac, arsta->addr);
			for (i = 0; i < IEEE80211_MAX_NUM_TIDS; i++) {
				arg.peer_tid_info[i].tx_buf_size = 0;
				arg.peer_tid_info[i].rx_buf_size = 0;
			}
		}

		ret = ath12k_wmi_send_smd_roam_config(arvif->ar, &arg);
		if (ret) {
			ath12k_warn(arvif->ar->ab,
				    "Failed SMD roam config on link %u (ret=%d)\n",
				    link_id, ret);
		}
	}

	return 0;
}

static void ath12k_uhr_smd_update_workfn(struct work_struct *work)
{
	struct ath12k_smd_update_work *ctx =
		container_of(work, struct ath12k_smd_update_work, work);
	struct ieee80211_sta *peer;
	struct ath12k_sta *ahsta = NULL;

	/* Re-derive ahsta under wiphy lock.  The pointer stored at enqueue
	 * time may have become stale if mac80211 freed the STA before the
	 * work item ran.  ieee80211_find_sta() is safe to call with the
	 * wiphy lock held, and update_smd_forall_links_locked() requires
	 * it, so do both under a single lock acquisition.
	 */
	wiphy_lock(ctx->hw->wiphy);

	peer = ieee80211_find_sta(ctx->ahvif->vif, ctx->peer_addr);
	if (peer)
		ahsta = ath12k_sta_to_ahsta(peer);

	(void)update_smd_forall_links_locked(ctx->hw, ctx->ahvif,
					     ahsta,
					     ctx->link_bitmap,
					     ctx->role, ctx->type, ctx->status,
					     ctx->dl_sn, ctx->ul_sn,
					     ctx->dl_drain_time);

	wiphy_unlock(ctx->hw->wiphy);

	kfree(ctx);
}

int ath12k_smd_uhr_smd_update(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *peer,
			      u32 role,
			      u32 type,
			      u32 status,
			      u32 dl_sn,
			      u32 ul_sn,
			      u32 dl_drain_time)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_smd_update_work *ctx;

	/* Called from tasklet => must be GFP_ATOMIC */
	ctx = kzalloc(sizeof(*ctx), GFP_ATOMIC);
	if (!ctx)
		return -ENOMEM;

	INIT_WORK(&ctx->work, ath12k_uhr_smd_update_workfn);

	ctx->hw = hw;
	ctx->ahvif = ahvif; /* Self ahvif */
	ctx->link_bitmap = 0xffff;
	ctx->role = role;
	ctx->type = type;
	ctx->status = status;
	ctx->dl_sn = dl_sn;
	ctx->ul_sn = ul_sn;
	ctx->dl_drain_time = dl_drain_time;
	/* Store the MLD address; ahsta is re-derived safely in the workfn */
	ether_addr_copy(ctx->peer_addr, peer->addr);

	/* Queue onto mac80211 workqueue (sleepable context) */
	ieee80211_queue_work(hw, &ctx->work);
	return 0;
}

static void
ath12k_smd_remap_vif_links(struct ath12k_vif *ahvif,
			   const struct ieee80211_uhr_link_reconfig_info *info,
			   struct ath12k_link_vif *const saved[])
{
	struct ieee80211_hw *hw = ahvif->ah->hw;
	u8 old_primary = ahvif->primary_link_id;
	int tap_lid, sap_lid;

	lockdep_assert_wiphy(hw->wiphy);

	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		sap_lid = info->tap_to_sap_link[tap_lid];
		if (sap_lid < 0 || sap_lid == tap_lid || !saved[sap_lid])
			continue;

		rcu_assign_pointer(ahvif->link[tap_lid], saved[sap_lid]);
		saved[sap_lid]->link_id = tap_lid;
		ath12k_dbg(saved[sap_lid]->ar->ab, ATH12K_DBG_MAC,
			   "smd: vif link[%d] <- link[%d] (vdev %u)\n",
			   tap_lid, sap_lid, saved[sap_lid]->vdev_id);
	}

	synchronize_rcu();

	for (sap_lid = 0; sap_lid < IEEE80211_MLD_MAX_NUM_LINKS; sap_lid++) {
		int t;

		if (info->tap_to_sap_link[sap_lid] != sap_lid)
			continue;

		for (t = 0; t < IEEE80211_MLD_MAX_NUM_LINKS; t++) {
			if (t != sap_lid && info->tap_to_sap_link[t] == sap_lid) {
				rcu_assign_pointer(ahvif->link[sap_lid], NULL);
				break;
			}
		}
	}

	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		if (info->tap_to_sap_link[tap_lid] == (int)old_primary) {
			ahvif->primary_link_id = (u8)tap_lid;
			break;
		}
	}

	ahvif->links_map = 0;
	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		if (wiphy_dereference(hw->wiphy, ahvif->link[tap_lid]))
			ahvif->links_map |= BIT(tap_lid);
	}
}

static void
ath12k_smd_remap_sta_links(struct ieee80211_hw *hw,
			   struct ath12k_sta *ahsta,
			   const struct ieee80211_uhr_link_reconfig_info *info,
			   struct ath12k_link_sta *const saved[],
			   struct ath12k_base *ab)
{
	u32 old_lmap = ahsta->links_map;
	u32 new_lmap = 0;
	int primary_tap_lid = -1;
	int tap_lid, sap_lid;

	lockdep_assert_wiphy(hw->wiphy);

	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		if (info->tap_to_sap_link[tap_lid] == (int)info->primary_link_id) {
			primary_tap_lid = tap_lid;
			break;
		}
	}

	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		struct ath12k_link_sta *arsta;

		sap_lid = info->tap_to_sap_link[tap_lid];
		if (sap_lid < 0 || sap_lid >= IEEE80211_MLD_MAX_NUM_LINKS)
			continue;

		if (old_lmap & BIT(sap_lid))
			new_lmap |= BIT(tap_lid);

		if (sap_lid == tap_lid)
			continue;

		rcu_assign_pointer(ahsta->link[tap_lid], saved[sap_lid]);
		if (saved[sap_lid])
			saved[sap_lid]->link_id = tap_lid;

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "smd: sta link[%d] <- link[%d] (%s)\n",
			   tap_lid, sap_lid, saved[sap_lid] ? "set" : "NULL");

		if (primary_tap_lid < 0)
			continue;

		arsta = wiphy_dereference(hw->wiphy, ahsta->link[tap_lid]);
		if (arsta)
			arsta->is_assoc_link = (tap_lid == primary_tap_lid);
	}

	if (primary_tap_lid >= 0)
		ahsta->assoc_link_id = (u8)primary_tap_lid;

	ahsta->links_map = new_lmap;
	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "smd: sta links_map 0x%x -> 0x%x assoc_link=%u\n",
		   old_lmap, new_lmap, ahsta->assoc_link_id);
}

int ath12k_smd_remap_links_op(struct ath12k_vif *ahvif,
			      struct ath12k_sta *ahsta_target,
			      const struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ieee80211_hw *hw = ahvif->ah->hw;
	struct ath12k_link_vif *saved_arvif[IEEE80211_MLD_MAX_NUM_LINKS] = {};
	struct ath12k_link_sta *saved_arsta[IEEE80211_MLD_MAX_NUM_LINKS] = {};
	struct ath12k_link_vif *primary_arvif;
	int tap_lid;

	lockdep_assert_wiphy(hw->wiphy);

	for (tap_lid = 0; tap_lid < IEEE80211_MLD_MAX_NUM_LINKS; tap_lid++) {
		saved_arvif[tap_lid] = wiphy_dereference(hw->wiphy, ahvif->link[tap_lid]);
		if (ahsta_target) {
			struct ath12k_link_sta *lk =
				wiphy_dereference(hw->wiphy,
						  ahsta_target->link[tap_lid]);

			saved_arsta[tap_lid] = lk;
		}
	}

	ath12k_smd_remap_vif_links(ahvif, info, saved_arvif);

	if (ahsta_target) {
		primary_arvif = wiphy_dereference(hw->wiphy,
						  ahvif->link[info->primary_link_id]);
		ath12k_smd_remap_sta_links(hw, ahsta_target, info, saved_arsta,
					   primary_arvif ? primary_arvif->ar->ab : NULL);
	}

	return 0;
}

int ath12k_smd_uhr_link_reconfig(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_sta *current_sta,
				    struct ieee80211_sta *target_sta,
				    enum ieee80211_uhr_link_reconfig_action action,
				    struct ieee80211_uhr_link_reconfig_info *info)
{
	struct ath12k_sta *target_ahsta =
		target_sta ? ath12k_sta_to_ahsta(target_sta) : NULL;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(current_sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[info->primary_link_id]);
	if (!arvif || !arvif->ar) {
		ath12k_generic_dbg(ATH12K_DBG_MAC, ATH12K_DBG_L1,
				   "uhr link reconfig: no arvif for primary link %u\n",
				   info->primary_link_id);
		return -EINVAL;
	}

	ar = arvif->ar;

	switch (action) {
	case IEEE80211_UHR_LINK_RECONFIG_PREPARE_REQ:
		break;

	case IEEE80211_UHR_LINK_RECONFIG_PREPARE_RESP:
		ether_addr_copy(ahvif->smd.target_mld_addr, info->target_ap_mld_addr);
		ath12k_mac_smd_roam_config(hw, ahvif, ahsta, target_ahsta,
						 action, info);
		ret = ath12k_uhr_prepare_links(ahvif, current_sta, target_sta, info);
		if (ret)
			return ret;
		break;

	case IEEE80211_UHR_LINK_RECONFIG_EXECUTE_REQ:
		ath12k_mac_smd_roam_config(hw, ahvif, ahsta, target_ahsta,
						 action, info);
		break;

	case IEEE80211_UHR_LINK_RECONFIG_EXECUTE_RESP:
		ath12k_mac_smd_roam_config(hw, ahvif, ahsta, target_ahsta,
						 action, info);

		return ath12k_uhr_execute_transition(ahvif, current_sta,
						     target_sta, info);

	case IEEE80211_UHR_LINK_RECONFIG_DYNAMIC_CONTEXT:
		if (info->request_ul_sn_not_transferred) {
			ret = ath12k_dp_arch_peer_tx_tid_sn_reset(ar->ab->dp,
								  &ahvif->ah->dp_hw,
								  current_sta->addr);
			if (ret)
				ath12k_warn(ar->ab,
					    "smd: dynamic_context tx_sn reset failed %pM (%d)\n",
					    current_sta->addr, ret);
		}

		if (info->request_dl_sn_not_transferred) {
			ret = ath12k_dp_arch_peer_rx_tid_svld_reset(ar->ab->dp,
								    &ahvif->ah->dp_hw,
								    current_sta->addr);
			if (ret)
				ath12k_warn(ar->ab,
					    "smd: dynamic_context rx_svld reset failed %pM (%d)\n",
					    current_sta->addr, ret);
		}

		ath12k_mac_smd_roam_config(hw, ahvif, ahsta, NULL,
						 action, info);

		ahvif->smd.exec_in_progress = false;
		if (ahvif->ah->ag && ahvif->ah->ag->dp_hw_grp)
			ahvif->ah->ag->dp_hw_grp->smd_exec_in_progress = false;
		eth_zero_addr(ahvif->smd.target_mld_addr);
		ath12k_dbg(ar->ab, ATH12K_DBG_SMD,
			   "smd: dynamic_context complete %pM\n",
			   current_sta->addr);
		return ret;

	case IEEE80211_UHR_LINK_RECONFIG_ABORT:
		return ath12k_uhr_abort_transition(ahvif, current_sta, target_sta, info);

	case IEEE80211_UHR_LINK_RECONFIG_REMAP_LINKS:
		return ath12k_smd_remap_links_op(ahvif, ahsta, info);

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

int ath12k_smd_global_init(struct ath12k_base *ab)
{
	return 0;
}

void ath12k_smd_global_deinit(void)
{
}

static int ath12k_smd_post_sta_session_ctx_req(struct ath12k_vif *ahvif,
					       struct ath12k_smd_ctx_req *req,
					       struct ath12k_smd_info *smd_info)
{
	struct ath12k_dp_hw *dp_hw = &ahvif->ah->dp_hw;
	struct ath12k_ba_session_params *dl_ba;
	struct ath12k_dp_smd_ctx dp_ctx = {};
	struct ath12k_link_vif *arvif;
	struct ath12k_sta *ahsta;
	struct ath12k_dp *dp;
	bool dl_sn_transfer;
	u8 tid;

	arvif = &ahvif->deflink;
	dp = arvif->ar->ab->dp;
	ahsta = container_of(smd_info, struct ath12k_sta, smd_info);

	smd_info->ctx_inflight = true;
	smd_info->latest_ctx_valid = false;
	smd_info->current_req = req;

	memcpy(dp_ctx.peer_addr, req->sta_addr, ETH_ALEN);
	dp_ctx.in.tx_tid_bitmap = bitmap_read(req->ctx.dl.valid_tid_bmap,
					      0, IEEE80211_MAX_NUM_TIDS);
	dp_ctx.in.tx_tid_bitmap |= BIT(ATH12K_SMD_TX_MGMT_TID);
	dp_ctx.in.rx_tid_bitmap = bitmap_read(req->ctx.ul.valid_tid_bmap,
					      0, IEEE80211_MAX_NUM_TIDS);
	dp_ctx.in.rx_tid_bitmap |= BIT(ATH12K_SMD_RX_MGMT_TID);

	if (test_bit(ATH12K_SMD_CTX_VALID_BA_PARAMS, req->ctx.valid_ctx_bmap)) {
		dl_sn_transfer =
			test_bit(ATH12K_SMD_CTX_VALID_DL_SN, req->ctx.valid_ctx_bmap);

		spin_lock_bh(&ahsta->ba_lock);

		for_each_set_bit(tid, req->ctx.dl.valid_tid_bmap,
				 IEEE80211_MAX_NUM_TIDS) {
			dl_ba = &ahsta->tx_ba_params[tid];

			/* Maximum Last SN (MLSN) not to be set if DL SN is not requested
			 * or no BlockAck is set up.
			 */
			if (!dl_sn_transfer || !dl_ba->valid) {
				dp_ctx.in.tx_tid_ba_size[tid] =
					ATH12K_SMD_BA_WIN_SIZE_MAX + 1;
			} else {
				dp_ctx.in.tx_tid_ba_size[tid] = dl_ba->buf_size;
			}
		}

		spin_unlock_bh(&ahsta->ba_lock);
	}

	return ath12k_dp_arch_dp_peer_fetch_smd_ctx(dp, dp_hw, &dp_ctx,
						    ath12k_smd_ctx_hw_rx_tid_cb,
						    ath12k_smd_ctx_hw_tx_tid_cb);
}

int ath12k_smd_collect_sta_session_ctx(struct ath12k *ar, struct sk_buff *mmpdu)
{
	const struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)mmpdu->data;
	const struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(mmpdu);
	unsigned long valid_ctx_bmap, valid_dl_tid_bmap, valid_ul_tid_bmap;
	struct ath12k_smd_info *smd_info;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_smd_ctx_req *req;
	u8 uhr_reconf_type, st_control;
	struct ath12k_link_sta *arsta;
	const u8 *buf, *st_params_ie;
	bool security_hdr_stripped;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	int ret;

	buf = (const u8 *)&mgmt->u.action;
	security_hdr_stripped = (status->flag & RX_FLAG_IV_STRIPPED);

	/* If the frame rx module did not strip CCMP header, adjust the buffer */
	if (ieee80211_has_protected(mgmt->frame_control) &&
	    !security_hdr_stripped)
		buf += IEEE80211_CCMP_HDR_LEN;

	buf += 3; /* skip Category, Action, and Dialog Token */
	uhr_reconf_type = *buf;

	spin_lock_bh(&ar->arsta_lock);

	arsta = ath12k_link_sta_find_by_addr(ar, mgmt->sa);
	if (!arsta) {
		ath12k_err(ab, "Failed to fetch peer for link %pM to collect SMD ctx",
			   mgmt->sa);
		spin_unlock_bh(&ar->arsta_lock);
		return -EINVAL;
	}

	ahsta = arsta->ahsta;
	sta = ath12k_ahsta_to_sta(ahsta);

	if (!sta->smd_params.smd_enabled) {
		ath12k_err(ab, "ST %s frame from non-SMD STA %pM",
			   ath12k_uhr_reconf_type_str(uhr_reconf_type), sta->addr);
		spin_unlock_bh(&ar->arsta_lock);
		return -EINVAL;
	}

	if (ahsta->state != IEEE80211_STA_AUTHORIZED) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "SMD STA %pM not in AUTHORIZED state, skip context collection",
			   sta->addr);
		spin_unlock_bh(&ar->arsta_lock);
		return -EOPNOTSUPP;
	}

	smd_info = &ahsta->smd_info;

	if (uhr_reconf_type == IEEE80211_UHR_LINK_RECONF_TYPE_ST_EXEC) {
		st_control = smd_info->st_control;
	} else {
		const u8 *ies = mgmt->u.action.u.uhr_link_reconf_req.variable;
		size_t ies_len =
			mmpdu->len - offsetof(struct ieee80211_mgmt,
					      u.action.u.uhr_link_reconf_req.variable);

		if (!security_hdr_stripped)
			ies_len -= IEEE80211_CCMP_HDR_LEN;

		st_params_ie = cfg80211_find_ext_ie(WLAN_EID_EXT_BSS_SMD_TRANS_PARAMS,
						    ies, ies_len);
		if (!st_params_ie || st_params_ie[1] < 2) {
			ath12k_err(ab,
				   "Failed to find ST %s Parameters element for %pM",
				   ath12k_uhr_reconf_type_str(uhr_reconf_type),
				   sta->addr);
			spin_unlock_bh(&ar->arsta_lock);
			return -EINVAL;
		}

		st_control = st_params_ie[3];
	}
	smd_info->st_control = st_control;

	if (uhr_reconf_type == IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP &&
	    smd_info->latest_ctx_valid &&
	    ktime_us_delta(ktime_get(), smd_info->latest_ctx_ts) <=
	    ATH12K_SMD_CTX_REUSE_THRESHOLD) {
		struct ath12k_smd_ctx_req *_req __free(kfree) =
			kzalloc(sizeof(*_req), GFP_ATOMIC);

		if (!_req) {
			spin_unlock_bh(&ar->arsta_lock);
			return -ENOMEM;
		}

		ath12k_dbg(ab, ATH12K_DBG_SMD, "Using cached ST Prep context for %pM",
			   sta->addr);
		memcpy(&_req->ctx, &smd_info->latest_ctx, sizeof(_req->ctx));
		_req->mmpdu = mmpdu;
		ath12k_smd_update_ctx_to_stack(smd_info, _req);
		spin_unlock_bh(&ar->arsta_lock);
		return 0;
	}

	valid_ctx_bmap = BIT(ATH12K_SMD_CTX_VALID_PN);
	valid_ctx_bmap |= BIT(ATH12K_SMD_CTX_VALID_BA_PARAMS);

	/* if Request DL SN Not Transferred is not set */
	if (!(st_control & SMD_PREP_REQ_DL_SN_NOT_TRANSFERRED))
		valid_ctx_bmap |= BIT(ATH12K_SMD_CTX_VALID_DL_SN);

	/* if Request UL SN Not Transferred is not set */
	if (!(st_control & SMD_PREP_REQ_UL_SN_NOT_TRANSFERRED))
		valid_ctx_bmap |= BIT(ATH12K_SMD_CTX_VALID_UL_SN);

	/* Prep phase collects only vendor context whereas Exec phase collects
	 * data context for TIDs 0-7 and vendor context.
	 */
	if (uhr_reconf_type == IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP) {
		valid_dl_tid_bmap = 0;
		valid_ul_tid_bmap = 0;
	} else {
		valid_dl_tid_bmap = 0xff;
		valid_ul_tid_bmap = 0xff;
	}

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		spin_unlock_bh(&ar->arsta_lock);
		return -ENOMEM;
	}

	spin_lock_init(&req->lock);
	req->mmpdu = mmpdu;
	req->type = uhr_reconf_type;
	req->handler = ath12k_smd_update_ctx_to_stack;
	req->enqueued_ts = ktime_get();
	memcpy(req->sta_addr, sta->addr, ETH_ALEN);
	bitmap_write(req->ctx.dl.valid_tid_bmap, valid_dl_tid_bmap,
		     0, IEEE80211_MAX_NUM_TIDS);
	bitmap_write(req->ctx.ul.valid_tid_bmap, valid_ul_tid_bmap,
		     0, IEEE80211_MAX_NUM_TIDS);
	bitmap_write(req->ctx.valid_ctx_bmap, valid_ctx_bmap,
		     0, ATH12K_SMD_CTX_NUM_VALID_CTX);

	ret = ath12k_smd_post_sta_session_ctx_req(ahsta->ahvif, req, smd_info);

	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "Posted context request (0x%*pb) for %pM (%d), data dl_tids=0x%*pb ul_tids=0x%*pb",
		   ATH12K_SMD_CTX_NUM_VALID_CTX, req->ctx.valid_ctx_bmap,
		   sta->addr, ret,
		   IEEE80211_MAX_NUM_TIDS, req->ctx.dl.valid_tid_bmap,
		   IEEE80211_MAX_NUM_TIDS, req->ctx.ul.valid_tid_bmap);

	if (ret) {
		smd_info->current_req = NULL;
		kfree(req);
	}

	spin_unlock_bh(&ar->arsta_lock);
	return ret;
}
EXPORT_SYMBOL(ath12k_smd_collect_sta_session_ctx);

void ath12k_smd_update_ctx_to_stack(struct ath12k_smd_info *smd_info,
				    struct ath12k_smd_ctx_req *req)
{}

u16 ath12k_smd_ctx_get_rx_ba_bufsize(struct ath12k_base *ab, struct ath12k_hw *ah,
				     const u8 *peer_addr, u8 tid, u16 orig_ba_win_sz)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp *dp = ab->dp;
	u32 ba_win_sz = 0;
	u16 _ssn;

	if (orig_ba_win_sz)
		return orig_ba_win_sz;

	/* fetch current active buffer size from dp */
	spin_lock_bh(&ah->dp_hw.peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(&ah->dp_hw, (u8 *)peer_addr);
	if (dp_peer && dp_peer->rx_tid[tid].active)
		ba_win_sz = dp_peer->rx_tid[tid].ba_win_sz;

	spin_unlock_bh(&ah->dp_hw.peer_hash_lock);

	if (ba_win_sz)
		return ba_win_sz;

	/* get default buffer size as a last resort */
	ath12k_dp_rx_peer_tid_ba_config(dp, tid, &ba_win_sz, &_ssn);

	return ba_win_sz;
}

static int
ath12k_smd_ctx_highest_sn_from_bitmap(u16 ssn, const u32 *bitmap_words,
				      int num_words)
{
	int word;
	u16 offset;

	for (word = num_words - 1; word >= 0; word--) {
		if (bitmap_words[word])
			break;
	}

	if (word < 0)
		return -ENOENT;

	offset = (word * 32) + __fls(bitmap_words[word]);

	return (ssn + offset) & IEEE80211_SN_MASK;
}

static void
ath12k_smd_ctx_hw_tid_update_rx_sn_from_bitmap(struct ath12k_smd_ctx_req *req,
					       u8 tid, u16 ssn, int num_words)
{
	struct ath12k_smd_reo_bitmap *reo_bitmap;
	int sn;

	if (!test_bit(ATH12K_SMD_CTX_VALID_UL_SN, req->ctx.valid_ctx_bmap))
		return;

	reo_bitmap = &req->ctx.vendor_ctx.ctx_v1.ul_reo_bmap[tid];
	sn = ath12k_smd_ctx_highest_sn_from_bitmap(ssn,
						   &reo_bitmap->bitmap_31_0,
						   num_words);
	req->ctx.ul.sn[tid] = (sn >= 0) ? sn : ssn;
}

static void
ath12k_smd_ctx_hw_tid_cb_vendor_1k_status(struct hal_reo_status *reo_status,
					  struct ath12k_smd_ctx_req *req, u8 tid)
{
	struct hal_reo_status_queue_1k_stats *q_1k_stats;
	struct hal_rx_reo_bitmap_1023_288 *bitmap;
	struct ath12k_smd_reo_bitmap *reo_bitmap;

	reo_bitmap = &req->ctx.vendor_ctx.ctx_v1.ul_reo_bmap[tid];
	q_1k_stats = &reo_status->u.queue_1k_stats;
	bitmap = &q_1k_stats->bitmap;

	/* copy bitmaps of bits 1023..288 */
	memcpy((void *)&reo_bitmap->bitmap_1023_288_grp, (void *)bitmap,
	       sizeof(*bitmap));

	if (test_bit(ATH12K_SMD_CTX_VALID_UL_SN, req->ctx.valid_ctx_bmap)) {
		int num_words = sizeof(*reo_bitmap) / sizeof(u32);
		u16 ssn = req->ctx.ul.sn[tid];

		ath12k_smd_ctx_hw_tid_update_rx_sn_from_bitmap(req, tid, ssn, num_words);
	}
}

static void
ath12k_smd_ctx_hw_tid_cb_vendor_v1(void *cb_data, struct ath12k_smd_ctx_req *req, bool tx,
				   u8 tid)
{
	struct ath12k_smd_ctx_tx_cb_per_tid *tx_cb_data;
	struct hal_reo_status_queue_stats *q_stats;
	struct ath12k_smd_reo_bitmap *reo_bitmap;
	struct hal_rx_reo_bitmap_287_0 *bitmap;
	u8 *pn;

	/* Tx data */
	if (tx) {
		tx_cb_data = (struct ath12k_smd_ctx_tx_cb_per_tid *)cb_data;

		if (tid != ATH12K_SMD_TX_MGMT_TID) {
			req->ctx.vendor_ctx.ctx_v1.dl_data_lsn_offset[tid] =
				tx_cb_data->lsn_offset;
			return;
		}

		req->ctx.vendor_ctx.ctx_v1.dl_mgmt_sn = tx_cb_data->sn;
		req->ctx.pn_len = tx_cb_data->pn_len;
		memcpy(req->ctx.vendor_ctx.ctx_v1.dl_mgmt_pn, tx_cb_data->pn,
		       tx_cb_data->pn_len);

		ath12k_dbg_level(NULL, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "%s cb tid: %u sn: %u, pn_len: %u, pn: %*ph",
				 tx ? "DL" : "UL", tid,
				 req->ctx.vendor_ctx.ctx_v1.dl_mgmt_sn,
				 req->ctx.pn_len,
				 req->ctx.pn_len, req->ctx.vendor_ctx.ctx_v1.dl_mgmt_pn);

		return;
	}

	/* Rx data */
	q_stats = &((struct hal_reo_status *)cb_data)->u.queue_stats;
	if (tid == ATH12K_SMD_RX_MGMT_TID) {
		req->ctx.vendor_ctx.ctx_v1.ul_mgmt_sn = q_stats->ssn;

		req->ctx.pn_len = q_stats->pn_len;
		pn = req->ctx.vendor_ctx.ctx_v1.ul_mgmt_pn;
		memcpy(&pn[0], &q_stats->pn_31_0, sizeof(q_stats->pn_31_0));
		memcpy(&pn[4], &q_stats->pn_47_32, sizeof(q_stats->pn_47_32));

		ath12k_dbg_level(NULL, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "%s cb tid: %u sn: %u, pn_len: %u, pn: %*ph",
				 tx ? "DL" : "UL", tid,
				 req->ctx.vendor_ctx.ctx_v1.ul_mgmt_sn,
				 req->ctx.pn_len,
				 req->ctx.pn_len, req->ctx.vendor_ctx.ctx_v1.ul_mgmt_pn);

		return;
	}

	reo_bitmap = &req->ctx.vendor_ctx.ctx_v1.ul_reo_bmap[tid];
	bitmap = &q_stats->bitmap;

	/* copy bitmaps of bits 287..0 */
	memcpy((void *)&reo_bitmap->bitmap_287_0_grp, (void *)bitmap,
	       sizeof(*bitmap));

	if (q_stats->to_follow_1k && test_bit(tid, req->wait_for_1k_status_ctx)) {
		if (test_bit(ATH12K_SMD_CTX_VALID_UL_SN, req->ctx.valid_ctx_bmap))
			req->ctx.ul.sn[tid] = q_stats->ssn;
		return;
	}

	clear_bit(tid, req->wait_for_1k_status_ctx);
	ath12k_smd_ctx_hw_tid_update_rx_sn_from_bitmap(req, tid, q_stats->ssn, 9);
}

static void ath12k_smd_ctx_hw_tid_cb_vendor(void *cb_data, struct ath12k_smd_ctx_req *req,
					    bool tx, u8 tid)
{
	switch (ath12k_smd_ctx_vendor_version) {
	case 1:
		ath12k_smd_ctx_hw_tid_cb_vendor_v1(cb_data, req, tx, tid);
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_SMD,
			   "Unsupported SMD Vendor ctx HW cb version=%d",
			   ath12k_smd_ctx_vendor_version);
	}
}

static void ath12k_smd_ctx_hw_tx_tid_cb(struct ath12k_dp *dp, void *cb_ctx,
					u8 *addr, u8 tid)
{
	struct ath12k_smd_ctx_tx_cb_per_tid *cb_data = cb_ctx;
	struct ath12k_smd_info *smd_info;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_smd_ctx_req *req;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_hw *ah;

	ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L2,
			 "%s for %pM tid: %u", __func__, addr, tid);

	guard(rcu)();

	ah = ab->ag->ah[0]; /* fetch default hw */
	sta = ieee80211_find_sta_by_ifaddr(ah->hw, addr, NULL);
	if (!sta) {
		ath12k_err(ab, "No STA found for %pM to process SMD ctx", addr);
		return;
	}

	ahsta = ath12k_sta_to_ahsta(sta);
	smd_info = &ahsta->smd_info;

	req = smd_info->current_req;
	if (!req) {
		ath12k_err(ab, "Current request not found for %pM to process SMD ctx",
			   sta->addr);
		return;
	}

	spin_lock_bh(&req->lock);
	if (!test_bit(tid, req->ctx.dl.valid_tid_bmap) &&
	    tid != ATH12K_SMD_TX_MGMT_TID) {
		spin_unlock_bh(&req->lock);
		return;
	}

	/* Data TIDs */
	if (test_bit(tid, req->ctx.dl.valid_tid_bmap)) {
		bool ba_setup = false;
		u16 ba_buf_size = 0;

		set_bit(tid, req->ctx.dl.completed_tid_bmap);

		if (test_bit(ATH12K_SMD_CTX_VALID_DL_SN, req->ctx.valid_ctx_bmap))
			req->ctx.dl.sn[tid] = cb_data->sn;

		if (test_bit(ATH12K_SMD_CTX_VALID_PN, req->ctx.valid_ctx_bmap) &&
		    cb_data->pn_len) {
			req->ctx.pn_len = cb_data->pn_len;
			memcpy(req->ctx.dl.pn, cb_data->pn, cb_data->pn_len);
		}

		if (test_bit(ATH12K_SMD_CTX_VALID_BA_PARAMS, req->ctx.valid_ctx_bmap)) {
			spin_lock_bh(&ahsta->ba_lock);
			if (ahsta->tx_ba_params[tid].valid) {
				struct ath12k_smd_ctx_ba *dl_ba = &req->ctx.dl.ba[tid];
				u16 buf_size_base = 0, buf_size_ext = 0;

				ba_setup = true;
				ba_buf_size = ahsta->tx_ba_params[tid].buf_size;

				dl_ba->amsdu_supported = ahsta->tx_ba_params[tid].amsdu;
				dl_ba->ba_policy = ahsta->tx_ba_params[tid].policy;
				dl_ba->timeout = ahsta->tx_ba_params[tid].timeout;
				ath12k_smd_ctx_encode_ba_buf_size(ba_buf_size,
								  &buf_size_base,
								  &buf_size_ext);
				dl_ba->buffer_size = buf_size_base;
				dl_ba->ext_buffer_size = buf_size_ext;
			}
			spin_unlock_bh(&ahsta->ba_lock);
		}

		/* LSN Offset */
		ath12k_smd_ctx_hw_tid_cb_vendor(cb_data, req, true, tid);

		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "DL cb tid: %u sn: %u, lsn_offset: %u, pn_len: %u, pn: %*ph, ba_setup: %s, ba_amsdu: %d, ba_policy: %d, ba_buffer_size: %u, ba_timeout: %u",
				 tid,
				 req->ctx.dl.sn[tid],
				 cb_data->lsn_offset,
				 req->ctx.pn_len,
				 req->ctx.pn_len, req->ctx.dl.pn,
				 ba_setup ? "valid" : "invalid",
				 req->ctx.dl.ba[tid].amsdu_supported,
				 req->ctx.dl.ba[tid].ba_policy,
				 ba_buf_size,
				 req->ctx.dl.ba[tid].timeout);

		spin_unlock_bh(&req->lock);
		return;
	}

	/* Mgmt TID */
	ath12k_smd_ctx_hw_tid_cb_vendor(cb_data, req, true, tid);
	spin_unlock_bh(&req->lock);
}

static void ath12k_smd_ctx_hw_rx_tid_cb(struct ath12k_dp *dp, void *cb_ctx,
					struct hal_reo_status *reo_status)
{
	struct ath12k_dp_smd_ctx *smd_data = cb_ctx;
	struct hal_reo_status_queue_stats *q_stats;
	struct ath12k_smd_info *smd_info;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_smd_ctx_req *req;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	u8 tid = smd_data->out.tid;
	struct ath12k_hw *ah;

	ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L2,
			 "%s for %pM tid: %u", __func__, smd_data->peer_addr, tid);

	guard(rcu)();

	ah = ab->ag->ah[0]; /* fetch default hw */
	sta = ieee80211_find_sta_by_ifaddr(ah->hw, smd_data->peer_addr, NULL);
	if (!sta) {
		ath12k_err(ab, "No STA found for %pM to process SMD ctx",
			   smd_data->peer_addr);
		return;
	}

	ahsta = ath12k_sta_to_ahsta(sta);
	smd_info = &ahsta->smd_info;

	req = smd_info->current_req;
	if (!req) {
		ath12k_err(ab, "Current request not found for %pM to process SMD ctx",
			   sta->addr);
		return;
	}

	spin_lock_bh(&req->lock);
	if (!test_bit(tid, req->ctx.ul.valid_tid_bmap) &&
	    tid != ATH12K_SMD_RX_MGMT_TID) {
		spin_unlock_bh(&req->lock);
		return;
	}

	/* If the STA is using 1k BlockAck buffer size, REO posts two status responses
	 * for the queue stats request to copy the complete Rx bitmap:
	 *     queue stats followed by queue_1k_stats
	 */
	if (ath12k_hal_rx_reo_1k_status(ab, reo_status)) {
		if (!test_bit(tid, req->ctx.ul.valid_tid_bmap)) {
			spin_unlock_bh(&req->lock);
			return;
		}

		if (test_bit(tid, req->wait_for_1k_status_ctx)) {
			ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
					 "REO 1k status cb with ext bitmaps for tid: %u",
					 tid);
			ath12k_smd_ctx_hw_tid_cb_vendor_1k_status(reo_status, req, tid);
			clear_bit(tid, req->wait_for_1k_status_ctx);
			set_bit(tid, req->ctx.ul.completed_tid_bmap);
		}
		spin_unlock_bh(&req->lock);
		return;
	}

	q_stats = &reo_status->u.queue_stats;

	if (ath12k_hal_rx_reo_status(ab, reo_status) && q_stats->to_follow_1k) {
		set_bit(tid, req->wait_for_1k_status_ctx);
		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "REO 1k status to follow for tid: %u", tid);
	}

	/* Data TIDs */
	if (test_bit(tid, req->ctx.ul.valid_tid_bmap)) {
		bool ba_setup = false;
		u16 ba_buf_size = 0;

		if (!test_bit(tid, req->wait_for_1k_status_ctx))
			set_bit(tid, req->ctx.ul.completed_tid_bmap);

		if (test_bit(ATH12K_SMD_CTX_VALID_PN, req->ctx.valid_ctx_bmap) &&
		    q_stats->pn_len) {
			req->ctx.pn_len = q_stats->pn_len;
			memcpy(&req->ctx.ul.pn[tid][0], &q_stats->pn_31_0,
			       sizeof(q_stats->pn_31_0));
			memcpy(&req->ctx.ul.pn[tid][4], &q_stats->pn_47_32,
			       sizeof(q_stats->pn_47_32));
		}
		if (test_bit(ATH12K_SMD_CTX_VALID_BA_PARAMS, req->ctx.valid_ctx_bmap)) {
			spin_lock_bh(&ahsta->ba_lock);
			if (ahsta->rx_ba_params[tid].valid) {
				struct ath12k_smd_ctx_ba *ul_ba = &req->ctx.ul.ba[tid];
				u16 orig_buf_size = ahsta->rx_ba_params[tid].buf_size;
				u16 buf_size_base = 0, buf_size_ext = 0;

				ba_setup = true;
				ba_buf_size =
					ath12k_smd_ctx_get_rx_ba_bufsize(ab, ah,
									 sta->addr,
									 tid,
									 orig_buf_size);

				ul_ba->amsdu_supported = ahsta->rx_ba_params[tid].amsdu;
				ul_ba->ba_policy = ahsta->rx_ba_params[tid].policy;
				ul_ba->timeout = ahsta->rx_ba_params[tid].timeout;
				ath12k_smd_ctx_encode_ba_buf_size(ba_buf_size,
								  &buf_size_base,
								  &buf_size_ext);
				ul_ba->buffer_size = buf_size_base;
				ul_ba->ext_buffer_size = buf_size_ext;
			}

			spin_unlock_bh(&ahsta->ba_lock);
		}

		/* Rx SN computation and REO Bitmap */
		ath12k_smd_ctx_hw_tid_cb_vendor((void *)reo_status, req, false, tid);

		ath12k_dbg_level(ab, ATH12K_DBG_SMD, ATH12K_DBG_L3,
				 "UL cb tid: %u sn: %u, pn_len: %u, pn: %*ph, ba_setup: %s, ba_amsdu: %d, ba_policy: %d, ba_buffer_size: %u, ba_timeout: %u",
				 tid,
				 req->ctx.ul.sn[tid],
				 q_stats->pn_len,
				 q_stats->pn_len, req->ctx.ul.pn[tid],
				 ba_setup ? "valid" : "invalid",
				 req->ctx.ul.ba[tid].amsdu_supported,
				 req->ctx.ul.ba[tid].ba_policy,
				 ba_buf_size,
				 req->ctx.ul.ba[tid].timeout);

		spin_unlock_bh(&req->lock);
		return;
	}

	/* Mgmt TID */
	ath12k_smd_ctx_hw_tid_cb_vendor((void *)reo_status, req, false, tid);
	spin_unlock_bh(&req->lock);
}
