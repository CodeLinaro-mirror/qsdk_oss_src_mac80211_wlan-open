// SPDX-License-Identifier: GPL-2.0-only
/*
 * SMD (Seamless Mobility Domain) BSS Transition MLME state machine
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "ieee80211_i.h"
#include "driver-ops.h"
#include "rate.h"
#include "key.h"
#include "sta_info.h"
#include "debugfs_netdev.h"
#include "smd.h"

bool ieee80211_smd_move_sta_state(struct ieee80211_sub_if_data *sdata,
				  const u8 *ap_addr,
				  enum ieee80211_sta_state new_state)
{
	struct sta_info *sta;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	sta = sta_info_get(sdata, ap_addr);
	if (!sta) {
		WARN_ONCE(1, "%s: SMD STA %pM not found", sdata->name, ap_addr);
		return false;
	}

	if (sta_info_move_state(sta, new_state)) {
		sdata_info(sdata, "smd: failed moving %pM to state %d\n",
			   ap_addr, new_state);
		return false;
	}

	if (new_state == IEEE80211_STA_AUTH)
		sdata_info(sdata, "smd: %pM authenticated\n", ap_addr);
	else if (new_state == IEEE80211_STA_ASSOC)
		sdata_info(sdata, "smd: %pM associated\n", ap_addr);

	return true;
}

static void ieee80211_smd_exec_timeout_work(struct wiphy *wiphy,
					    struct wiphy_work *work);

void ieee80211_smd_prep_timeout_work(struct wiphy *wiphy,
				     struct wiphy_work *work)
{
	struct ieee80211_smd_prep_target *target =
		container_of(work, struct ieee80211_smd_prep_target,
			     prep_timeout_work.work);
	struct ieee80211_sub_if_data *sdata;

	lockdep_assert_wiphy(wiphy);

	if (WARN_ON(!target))
		return;

	sdata = target->sdata;
	if (WARN_ON(!sdata))
		return;

	sdata_info(sdata, "smd: prep timeout expired for target %pM\n",
		   target->target_mld_addr);

	ieee80211_smd_prep_reset_target(sdata, target,
					WLAN_STATUS_UNSPECIFIED_FAILURE, 0);
}

void ieee80211_smd_prep_init(struct ieee80211_sub_if_data *sdata)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	int i;

	ifmgd->max_prepared_targets = IEEE80211_SMD_MAX_PREP_TARGETS;
	ifmgd->prep_targets = kcalloc(ifmgd->max_prepared_targets,
				      sizeof(struct ieee80211_smd_prep_target),
				      GFP_KERNEL);
	if (!ifmgd->prep_targets) {
		ifmgd->max_prepared_targets = 0;
		return;
	}

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		wiphy_delayed_work_init(&ifmgd->prep_targets[i].prep_timeout_work,
					ieee80211_smd_prep_timeout_work);
		wiphy_delayed_work_init(&ifmgd->prep_targets[i].exec_timeout_work,
					ieee80211_smd_exec_timeout_work);
	}
}

void ieee80211_smd_prep_deinit(struct ieee80211_sub_if_data *sdata)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	int i;

	if (!ifmgd->prep_targets)
		return;

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		if (ifmgd->prep_targets[i].prep_timeout_started) {
			wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					&ifmgd->prep_targets[i].prep_timeout_work);
			ifmgd->prep_targets[i].prep_timeout_started = false;
		}
		if (ifmgd->prep_targets[i].exec_timeout_started) {
			wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					&ifmgd->prep_targets[i].exec_timeout_work);
			ifmgd->prep_targets[i].exec_timeout_started = false;
		}
	}

	kfree(ifmgd->prep_targets);
	ifmgd->prep_targets = NULL;
	ifmgd->max_prepared_targets = 0;
	ifmgd->num_prepared_targets = 0;
}

/**
 * ieee80211_smd_dl_drain_work_fn - wiphy_work handler for DL drain completion
 *
 * Runs with wiphy mutex held (guaranteed by wiphy_work infrastructure),
 * satisfying the lockdep_assert_wiphy() requirement of
 * ieee80211_smd_dl_drain_complete().
 */
void ieee80211_smd_dl_drain_work(struct wiphy *wiphy,
					struct wiphy_work *work)
{
	struct ieee80211_if_managed *ifmgd =
		container_of(work, struct ieee80211_if_managed, smd_dl_drain_work);
	struct ieee80211_sub_if_data *sdata =
		container_of(ifmgd, struct ieee80211_sub_if_data, u.mgd);

	ieee80211_smd_dl_drain_complete(&sdata->vif,
					ifmgd->smd_dl_drain_target_mld_addr);
}

/**
 * ieee80211_smd_dl_drain_complete_irqsafe - IRQ-safe DL drain completion notify
 * @vif: virtual interface
 * @target_mld_addr: target AP MLD address
 *
 * IRQ-safe version of ieee80211_smd_dl_drain_complete(). Can be called from
 * any context including softirq/tasklet
 *
 * Internally schedules a wiphy_work that runs with the
 * wiphy mutex held, then calls ieee80211_smd_dl_drain_complete().
 *
 * Context: Any context (IRQ-safe).
 */
void ieee80211_smd_dl_drain_complete_irqsafe(struct ieee80211_vif *vif,
					     const u8 *target_mld_addr)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

	ether_addr_copy(ifmgd->smd_dl_drain_target_mld_addr, target_mld_addr);
	wiphy_work_queue(sdata->local->hw.wiphy, &ifmgd->smd_dl_drain_work);
}
EXPORT_SYMBOL(ieee80211_smd_dl_drain_complete_irqsafe);

int ieee80211_smd_find_free_target_slot(struct ieee80211_sub_if_data *sdata)
{
	int i;

	for (i = 0; i < sdata->u.mgd.max_prepared_targets; i++) {
		if (!sdata->u.mgd.prep_targets[i].valid)
			return i;
	}
	return -1;
}

int ieee80211_smd_find_target_by_addr(struct ieee80211_sub_if_data *sdata,
				      const u8 *target_addr)
{
	int i;

	for (i = 0; i < sdata->u.mgd.max_prepared_targets; i++) {
		if (sdata->u.mgd.prep_targets[i].valid &&
		    ether_addr_equal(sdata->u.mgd.prep_targets[i].target_mld_addr,
				     target_addr))
			return i;
	}

	return -1;
}

int ieee80211_smd_find_target_by_dialog_token(struct ieee80211_sub_if_data *sdata,
					      u8 dialog_token)
{
	int i;

	for (i = 0; i < sdata->u.mgd.max_prepared_targets; i++) {
		if (sdata->u.mgd.prep_targets[i].valid &&
		    sdata->u.mgd.prep_targets[i].dialog_token == dialog_token)
			return i;
	}
	return -1;
}

int ieee80211_smd_find_sap_lid_for_band(struct ieee80211_sub_if_data *sdata,
					       enum nl80211_band band)
{
	int sap_link_id;

	for (sap_link_id = 0; sap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; sap_link_id++) {
		struct ieee80211_link_data *link =
			sdata_dereference(sdata->link[sap_link_id], sdata);
		if (link && link->conf->chanreq.oper.chan &&
		    link->conf->chanreq.oper.chan->band == band)
			return sap_link_id;
	}
	return -1;
}

/* Build the tap_to_sap_link[] and sap_to_tap_link[] maps for a prep target
 * by matching each TAP link's BSS band against current SAP link operating bands.
 * Initializes to identity if already correct (same-links map); sets link_id_remap=true
 * only when the mapping is non-trivial.
 */
void
ieee80211_smd_build_link_id_remap(struct ieee80211_sub_if_data *sdata,
				  struct ieee80211_smd_prep_target *target)
{
	int tap_link_id, sap_link_id;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	for (tap_link_id = 0; tap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; tap_link_id++) {
		enum nl80211_band tap_band;

		if (!target->assoc_data->link[tap_link_id].bss)
			continue;

		tap_band = target->assoc_data->link[tap_link_id].bss->channel->band;
		sap_link_id = ieee80211_smd_find_sap_lid_for_band(sdata, tap_band);
		if (sap_link_id < 0)
			continue;

		target->tap_to_sap_link[tap_link_id] = sap_link_id;
		target->sap_to_tap_link[sap_link_id] = tap_link_id;
		if (tap_link_id != sap_link_id)
			target->link_id_remap = true;
	}
}

int ieee80211_smd_add_prep_target(struct ieee80211_sub_if_data *sdata,
				  const u8 *target_addr,
				  struct ieee80211_mgd_assoc_data *assoc_data)
{
	int slot;
	int i;

	slot = ieee80211_smd_find_free_target_slot(sdata);

	if (slot < 0)
		return -ENOSPC;

	memcpy(sdata->u.mgd.prep_targets[slot].target_mld_addr,
	       target_addr, ETH_ALEN);
	sdata->u.mgd.prep_targets[slot].valid = true;
	sdata->u.mgd.prep_targets[slot].sdata = sdata;
	sdata->u.mgd.prep_targets[slot].assoc_data = assoc_data;
	sdata->u.mgd.prep_targets[slot].prep_time = jiffies;

	/* Re-initialize both timeout work items after the memset from the
	 * previous use of this slot in prep_invalidate_target().
	 */
	wiphy_delayed_work_init(&sdata->u.mgd.prep_targets[slot].prep_timeout_work,
				ieee80211_smd_prep_timeout_work);
	wiphy_delayed_work_init(&sdata->u.mgd.prep_targets[slot].exec_timeout_work,
				ieee80211_smd_exec_timeout_work);

	sdata->u.mgd.smd_dialog_token_alloc++;
	if (!sdata->u.mgd.smd_dialog_token_alloc)
		sdata->u.mgd.smd_dialog_token_alloc = 1;
	sdata->u.mgd.prep_targets[slot].dialog_token =
		sdata->u.mgd.smd_dialog_token_alloc;

	sdata->u.mgd.num_prepared_targets++;

	/* Initialize link ID remap to identity (same-links map default).
	 * The actual remap is built in ieee80211_process_smd_prep_resp after
	 * parse_ml_persta, using fresh TAP BSSIDs from the PREP RESPONSE frame
	 * to refresh assoc_data->link[].bss before band matching.
	 */
	for (i = 0; i < IEEE80211_MLD_MAX_NUM_LINKS; i++) {
		sdata->u.mgd.prep_targets[slot].tap_to_sap_link[i] = i;
		sdata->u.mgd.prep_targets[slot].sap_to_tap_link[i] = i;
	}
	sdata->u.mgd.prep_targets[slot].link_id_remap = false;

	return slot;
}

/* Remove SMD preparation target */
void ieee80211_smd_remove_prep_target(struct ieee80211_sub_if_data *sdata,
				      int slot)
{
	if (slot < 0 || slot >= sdata->u.mgd.max_prepared_targets)
		return;

	if (!sdata->u.mgd.prep_targets[slot].valid)
		return;

	if (sdata->u.mgd.prep_targets[slot].prep_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
				&sdata->u.mgd.prep_targets[slot].prep_timeout_work);
		sdata->u.mgd.prep_targets[slot].prep_timeout_started = false;
	}
	if (sdata->u.mgd.prep_targets[slot].exec_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
				&sdata->u.mgd.prep_targets[slot].exec_timeout_work);
		sdata->u.mgd.prep_targets[slot].exec_timeout_started = false;
	}

	kfree(sdata->u.mgd.prep_targets[slot].assoc_data);
	sdata->u.mgd.prep_targets[slot].assoc_data = NULL;

	kfree(sdata->u.mgd.prep_targets[slot].drv_info);
	sdata->u.mgd.prep_targets[slot].drv_info = NULL;

	memset(&sdata->u.mgd.prep_targets[slot], 0,
	       sizeof(sdata->u.mgd.prep_targets[slot]));

	sdata->u.mgd.num_prepared_targets--;
}
int ieee80211_smd_execute_transition(struct ieee80211_sub_if_data *sdata,
					    struct ieee80211_smd_prep_target *target,
					    u16 dl_drain_time_tu)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_uhr_link_reconfig_info *info;
	struct sta_info *current_sta, *target_sta;
	u16 transitioning_links;
	int ret;

	lockdep_assert_wiphy(local->hw.wiphy);

	if (!target || !target->target_sta) {
		sdata_info(sdata, "smd: exec target not properly prepared\n");
		return -EINVAL;
	}

	current_sta = sta_info_get(sdata, sdata->vif.cfg.ap_addr);
	if (!current_sta) {
		sdata_info(sdata, "smd: exec current AP sta not found\n");
		return -ENOENT;
	}

	/*
	 * partner links were already transitioned in PREP resp.
	 * target_sta was inserted (sta_inserted=true) during PREP.
	 */
	target_sta = target->target_sta;
	if (!target_sta) {
		sdata_info(sdata, "smd: exec no target_sta (PREP incomplete)\n");
		return -EINVAL;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/*
	 * Compute transitioning links: all prepared links EXCEPT primary.
	 * Primary link switches after DL drain.
	 */
	transitioning_links = target->prepared_links_mask;
	if (target->primary_link_id >= 0)
		transitioning_links &= ~BIT(target->primary_link_id);

	target->transitioning_links = transitioning_links;
	target->execution_in_progress = true;

	info->transitioning_links = 0;

	info->primary_link_id = (u8)(target->primary_link_id >= 0
		? target->tap_to_sap_link[target->primary_link_id]
		: 0);
	ether_addr_copy(info->target_ap_mld_addr, target->target_mld_addr);
	info->target_aid = target->target_aid;
	info->dl_drain_time_tu = dl_drain_time_tu;
	info->request_dl_sn_not_transferred = target->no_dl_sn;
	info->request_ul_sn_not_transferred = target->no_ul_sn;

	ret = drv_uhr_link_reconfig(local, sdata, current_sta, target_sta,
				    IEEE80211_UHR_LINK_RECONFIG_EXECUTE_RESP, info);
	if (ret) {
		sdata_info(sdata,
			   "smd: exec drv_uhr_link_reconfig(EXECUTE) failed: %d\n",
			   ret);
		target->execution_in_progress = false;
		kfree(info);
		return ret;
	}

	kfree(info);
	return 0;
}

void ieee80211_smd_stop_old_link(struct ieee80211_vif *vif,
				 struct ieee80211_link_data *old_link,
				 unsigned int link_id)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);
	struct ieee80211_local *local = sdata->local;

	lockdep_assert_wiphy(local->hw.wiphy);

	if (!old_link)
		return;

	ieee80211_mgd_stop_link(old_link);

	wiphy_work_cancel(local->hw.wiphy,
			  &old_link->advertised_ttlm_evt_notify_work);
	wiphy_delayed_work_cancel(local->hw.wiphy,
				  &old_link->color_collision_detect_work);
	wiphy_work_cancel(local->hw.wiphy,
			  &old_link->color_change_finalize_work);
	wiphy_work_cancel(local->hw.wiphy,
			  &old_link->csa.finalize_work);

	if (sdata->wdev.links[link_id].cac_started) {
		ieee80211_handle_cac_stop(old_link->sdata->local->hw.wiphy,
					  old_link->sdata, old_link, old_link->conf,
					  NULL);
	}

	/* Remove the VIF-level link-N debugfs directory so the target link
	 * can create a fresh one for the same link_id without conflict.
	 */
	ieee80211_link_debugfs_remove(old_link);

	/* Release channel context with skip_idle_recalc=true
	 * because we're doing atomic multi-link operations
	 */
	__ieee80211_link_release_channel(old_link, true);
}

void ieee80211_smd_free_old_links(struct ieee80211_sub_if_data *sdata,
				  struct ieee80211_smd_prep_target *target)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_link_data *link;
	unsigned int link_id;
	LIST_HEAD(old_keys);

	lockdep_assert_wiphy(local->hw.wiphy);

	synchronize_rcu();

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		struct ieee80211_link_data *old_link = target->old_links[link_id];

		if (!old_link || old_link == &sdata->deflink)
			continue;

		ieee80211_remove_link_keys(old_link, &old_keys);
	}

	wiphy_delayed_work_cancel(local->hw.wiphy, &sdata->dec_tailroom_needed_wk);
	ieee80211_delayed_tailroom_dec(local->hw.wiphy,
				       &sdata->dec_tailroom_needed_wk.work);
	ieee80211_free_key_list(local, &old_keys);

	/*
	 * For diff-links maps, PREP-era group keys carry stale SAP link_ids
	 * that block post-transition TAP key installation (key.c:506).
	 * Remove them now, before cfg80211_notify(COMPLETE) fires.
	 */
	if (target->link_id_remap)
		ieee80211_smd_remap_link_keys(sdata, target->sap_to_tap_link);

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		if (!target->old_links[link_id] ||
		    target->old_links[link_id] == &sdata->deflink)
			continue;

		link = target->old_links[link_id];
		ieee80211_link_debugfs_remove(link);
		kfree(target->old_links[link_id]);
		target->old_links[link_id] = NULL;
	}
}

static void
ieee80211_smd_remove_orphan_sta_links(struct ieee80211_sub_if_data *sdata,
				      u16 orphan_links)
{
	struct ieee80211_local *local = sdata->local;
	struct sta_info *sta;
	unsigned int lid;

	if (!orphan_links)
		return;

	sta = sta_info_get(sdata, sdata->vif.cfg.ap_addr);
	if (!sta)
		return;

	for_each_set_bit(lid, (unsigned long *)&orphan_links,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		struct link_sta_info *lsi;

		if (!(sta->sta.valid_links & BIT(lid)))
			continue;

		lsi = rcu_dereference_protected(sta->link[lid],
						lockdep_is_held(&local->hw.wiphy->mtx));
		ieee80211_sta_remove_link(sta, lid, true);
	}
}

static int __smd_dl_drain_no_remap(struct ieee80211_sub_if_data *sdata,
				   struct ieee80211_smd_prep_target *target,
				   int primary_id)
{
	int ret;

	/*
	 * Downgrade (same-links map, fewer TAP links than SAP):
	 * remove orphan STA links from the current AP peer before
	 * transitioning the primary link.  This produces
	 *   drv_change_sta_links(old=0x3, new=0x1)  [e.g. 5 GHz gone]
	 * instead of
	 *   drv_change_sta_links(old=0x3, new=0x2)  [5 GHz re-asserted]
	 * followed by a stale peer still on the vdev when vdev-delete
	 * arrives in assoc_success_finalize.
	 */
	ieee80211_smd_remove_orphan_sta_links(sdata,
					      sdata->vif.valid_links &
					      ~target->tap_prepared_mask);

	if (!target->transition_done_in_prep) {
		ret = ieee80211_smd_assoc_success(sdata, target, primary_id,
						  target->tap_to_sap_link[primary_id]);
		if (ret) {
			sdata_info(sdata, "smd: dl_drain assoc_success failed for primary %d: %d\n",
				   primary_id, ret);
			return ret;
		}
	}

	sdata->vif.active_links |= BIT(primary_id);
	sdata->vif.dormant_links &= ~BIT(primary_id);

	return 0;
}

static int __smd_dl_drain_remap(struct ieee80211_sub_if_data *sdata,
				struct ieee80211_smd_prep_target *target,
				int primary_id)
{
	struct ieee80211_local *local = sdata->local;
	int primary_sap_link_id = target->tap_to_sap_link[primary_id];
	unsigned int prepared_mask = target->tap_prepared_mask;
	struct ieee80211_uhr_link_reconfig_info *remap_info;
	struct ieee80211_link_data *old_primary_link;
	struct ieee80211_smd_target_link *nl;
	struct sta_info *current_sta;
	unsigned int tap_link_id;
	u16 mapped_sap_links = 0;
	u16 orphan_sap_links;
	int ret;

	if (WARN_ON_ONCE(primary_sap_link_id < 0 ||
			 primary_sap_link_id >= IEEE80211_MLD_MAX_NUM_LINKS)) {
		sdata_info(sdata, "smd: dl_drain sap=%d tap=%d invalid\n",
			   primary_sap_link_id, primary_id);
		return -EINVAL;
	}

	/*
	 * Orphan SAP slots must be freed before REMAP_LINKS while
	 * ahvif->link[] is still SAP-indexed; after remap those slots
	 * hold live TAP arvifs and a remove call would free them.
	 */
	for_each_set_bit(tap_link_id, (unsigned long *)&prepared_mask,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		int sap_link = target->tap_to_sap_link[tap_link_id];

		if (sap_link >= 0 && sap_link < IEEE80211_MLD_MAX_NUM_LINKS)
			mapped_sap_links |= BIT(sap_link);
	}
	orphan_sap_links = sdata->vif.valid_links & ~mapped_sap_links;

	if (orphan_sap_links) {
		u16 kept = sdata->vif.valid_links & mapped_sap_links;

		ieee80211_smd_remove_orphan_sta_links(sdata, orphan_sap_links);
		ret = ieee80211_vif_set_links(sdata, kept, 0);
		if (ret) {
			sdata_info(sdata, "smd: vif_set_links(0x%x) failed: %d\n",
				   kept, ret);
			return ret;
		}
	}

	/*
	 * Skipped when transition_done_in_prep=true: these steps were
	 * already run during PREP; current_sta will be destroyed by
	 * assoc_success_finalize at the end of this function.
	 */
	if (!target->transition_done_in_prep) {
		old_primary_link = sdata->link[primary_sap_link_id];
		current_sta = sta_info_get(sdata, sdata->vif.cfg.ap_addr);
		if (current_sta &&
		    (current_sta->sta.valid_links & BIT(primary_sap_link_id)))
			ieee80211_sta_remove_link(current_sta, primary_sap_link_id,
						  true);

		target->old_links[primary_sap_link_id] = old_primary_link;

		if (current_sta) {
			WARN_ON(__sta_info_destroy(current_sta));
			current_sta = NULL;
		}

		ieee80211_smd_stop_old_link(&sdata->vif, old_primary_link,
					    primary_sap_link_id);
	}

	remap_info = kzalloc(sizeof(*remap_info), GFP_KERNEL);
	if (!remap_info)
		return -ENOMEM;

	memcpy(remap_info->tap_to_sap_link, target->tap_to_sap_link,
	       sizeof(remap_info->tap_to_sap_link));
	remap_info->primary_link_id = (u8)primary_sap_link_id;
	ret = drv_uhr_link_reconfig(local, sdata, target->target_sta, NULL,
				    IEEE80211_UHR_LINK_RECONFIG_REMAP_LINKS,
				    remap_info);
	kfree(remap_info);
	if (ret) {
		sdata_info(sdata, "smd: dl_drain remap_links failed: %d\n", ret);
		return ret;
	}

	for_each_set_bit(tap_link_id, (unsigned long *)&prepared_mask,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		int sap_link_id_local = target->tap_to_sap_link[tap_link_id];

		if (target->new_links[tap_link_id]) {
			nl = target->new_links[tap_link_id];
			rcu_assign_pointer(sdata->vif.link_conf[tap_link_id],
					   &nl->conf);
		}
		if (sdata->link[sap_link_id_local])
			__ieee80211_link_unassign(sdata, sap_link_id_local);
	}

	if (!target->transition_done_in_prep) {
		if (target->new_links[primary_id]) {
			rcu_assign_pointer(
				target->new_links[primary_id]->conf.chanctx_conf,
				NULL);
			if (target->new_links[primary_id]->data.reserved_chanctx)
				ieee80211_link_unreserve_chanctx(
						&target->new_links[primary_id]->data);
		}
		for_each_set_bit(tap_link_id, (unsigned long *)&prepared_mask,
				 IEEE80211_MLD_MAX_NUM_LINKS) {
			if (tap_link_id == (unsigned int)primary_id)
				continue;
			if (target->new_links[tap_link_id]) {
				nl = target->new_links[tap_link_id];
				__ieee80211_link_assign(sdata, tap_link_id,
							&nl->data, &nl->conf);
			}
		}
	} else {
		for_each_set_bit(tap_link_id, (unsigned long *)&prepared_mask,
				 IEEE80211_MLD_MAX_NUM_LINKS) {
			if (target->new_links[tap_link_id]) {
				nl = target->new_links[tap_link_id];
				__ieee80211_link_assign(sdata, tap_link_id,
							&nl->data, &nl->conf);
			}
		}
	}

	synchronize_rcu();
	for (tap_link_id = 0; tap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; tap_link_id++) {
		if (!target->new_links[tap_link_id])
			continue;
		target->new_links[tap_link_id]->conf.link_id = tap_link_id;
		target->new_links[tap_link_id]->data.link_id = tap_link_id;
	}
	synchronize_rcu();

	for (tap_link_id = 0; tap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; tap_link_id++) {
		nl = target->new_links[tap_link_id];

		if (!nl || target->tap_to_sap_link[tap_link_id] == (int)tap_link_id)
			continue;

		ieee80211_link_debugfs_remove(&nl->data);
	}

	for (tap_link_id = 0; tap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; tap_link_id++) {
		nl = target->new_links[tap_link_id];

		if (!nl || target->tap_to_sap_link[tap_link_id] == (int)tap_link_id)
			continue;

		ieee80211_link_debugfs_add(&nl->data);
	}

	ieee80211_smd_remap_sta_links(target->target_sta, target->tap_to_sap_link);

	if (!target->transition_done_in_prep) {
		sdata->vif.active_links |= BIT(primary_id);

		ret = ieee80211_smd_assoc_success(sdata, target, primary_id, primary_id);
		if (ret) {
			sdata_info(sdata, "smd: dl_drain assoc_success failed: %d\n",
				   ret);
			return ret;
		}
	}

	sdata->vif.active_links  = target->tap_prepared_mask;
	sdata->vif.dormant_links = 0;
	sdata->vif.valid_links   = target->tap_prepared_mask;

	return 0;
}

void __ieee80211_smd_dl_drain_complete(struct ieee80211_sub_if_data *sdata,
					      struct ieee80211_smd_prep_target *target,
					      bool defer_complete)
{
	struct ieee80211_local *local = sdata->local;
	int primary_id;

	lockdep_assert_wiphy(local->hw.wiphy);

	if (!target || !target->execution_in_progress)
		return;

	if (!target->target_sta)
		return;

	primary_id = target->primary_link_id;

	if (primary_id >= 0) {
		struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

		ifmgd->smd_transitioning_links |= BIT(primary_id);

		if (!target->link_id_remap)
			__smd_dl_drain_no_remap(sdata, target, primary_id);
		else
			__smd_dl_drain_remap(sdata, target, primary_id);
	}

	ieee80211_smd_assoc_success_finalize(sdata, target, target->current_sta_addr,
					     defer_complete);
}

void ieee80211_smd_dl_drain_complete(struct ieee80211_vif *vif,
				     const u8 *target_mld_addr)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	struct ieee80211_smd_prep_target *target = NULL;
	int i;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!ifmgd->prep_targets) {
		sdata_err(sdata, "smd: dl_drain no prep targets\n");
		return;
	}

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		if (ifmgd->prep_targets[i].valid &&
		    ifmgd->prep_targets[i].execution_in_progress) {
			target = &ifmgd->prep_targets[i];
			break;
		}
	}

	/* Stale FW notification after transition completed — benign. */
	if (!target)
		return;

	__ieee80211_smd_dl_drain_complete(sdata, target, false);
}
EXPORT_SYMBOL(ieee80211_smd_dl_drain_complete);

void ieee80211_smd_free_target_links(struct ieee80211_smd_prep_target *target)
{
	struct ieee80211_smd_target_link *tgt_link;
	unsigned int link_id;

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		tgt_link = target->new_links[link_id];
		if (!tgt_link)
			continue;

		__ieee80211_link_release_channel(&tgt_link->data, false);
		kfree(target->new_links[link_id]);
		target->new_links[link_id] = NULL;
	}
}

static void ieee80211_smd_exec_timeout_work(struct wiphy *wiphy,
					    struct wiphy_work *work)
{
	struct ieee80211_smd_prep_target *target =
		container_of(work, struct ieee80211_smd_prep_target,
			     exec_timeout_work.work);
	struct ieee80211_sub_if_data *sdata = target->sdata;

	lockdep_assert_wiphy(wiphy);

	/*
	 * The target slot was already cleaned up (transition completed or
	 * aborted) before this work item got a chance to run.  The memset
	 * in ieee80211_smd_prep_invalidate_target() zeroes target->valid,
	 * so use that as the sentinel: if the slot is no longer valid, the
	 * timeout fired after a successful transition and must be ignored.
	 */
	if (!target->valid || !sdata) {
		pr_warn("SMD: Timeout work with invalid sdata or target invalid\n");
		return;
	}

	sdata_info(sdata, "smd: exec timeout expired for target %pM\n",
		   target->target_mld_addr);

	/*
	 * Execution didn't happen in time. Abort the preparation by:
	 * 1. Notifying driver to abort via uhr_link_reconfig(ABORT)
	 * 2. Cleaning up target sta_info
	 * 3. Resetting the target state
	 */
	ieee80211_smd_prep_reset_target(sdata, target,
					WLAN_STATUS_UNSPECIFIED_FAILURE, 1);
}

#define IEEE80211_SMD_PREP_TIMEOUT_TU	64
void ieee80211_smd_start_exec_timeout(struct ieee80211_sub_if_data *sdata,
					     struct ieee80211_smd_prep_target *target,
					     u16 timeout_tu)
{
	unsigned long timeout_jiffies;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!target)
		return;

	if (!timeout_tu)
		return;

	timeout_tu *= IEEE80211_SMD_PREP_TIMEOUT_TU;
	timeout_jiffies = msecs_to_jiffies(timeout_tu);
	wiphy_delayed_work_queue(sdata->local->hw.wiphy,
				 &target->exec_timeout_work,
				 timeout_jiffies);

	target->exec_timeout_started = true;

}

void ieee80211_smd_start_prep_timeout(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_smd_prep_target *target,
				      u16 timeout_tu)
{
	unsigned long timeout_jiffies;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!target || !timeout_tu)
		return;

	timeout_tu *= IEEE80211_SMD_PREP_TIMEOUT_TU;
	timeout_jiffies = msecs_to_jiffies(timeout_tu);
	wiphy_delayed_work_queue(sdata->local->hw.wiphy,
				 &target->prep_timeout_work,
				 timeout_jiffies);
	target->prep_timeout_started = true;
}

static void ieee80211_smd_prep_invalidate_target(struct ieee80211_if_managed *ifmgd,
						 struct ieee80211_smd_prep_target *target)
{
	struct ieee80211_sub_if_data *sdata =
		container_of(ifmgd, struct ieee80211_sub_if_data, u.mgd);

	if (target->prep_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->prep_timeout_work);
		target->prep_timeout_started = false;
	}
	if (target->exec_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->exec_timeout_work);
		target->exec_timeout_started = false;
	}

	memset(target, 0, sizeof(*target));
	target->primary_link_id = -1;

	if (ifmgd->num_prepared_targets > 0)
		ifmgd->num_prepared_targets--;
}

void ieee80211_smd_prep_reset_target(struct ieee80211_sub_if_data *sdata,
				     struct ieee80211_smd_prep_target *target,
				     u16 status, u16 type)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

	lockdep_assert_wiphy(local->hw.wiphy);

	if (!target || !target->valid)
		return;

	cfg80211_notify_smd_bss_transition(sdata->dev,
					   target->target_mld_addr,
					   NL80211_SMD_TRANSITION_ABORT,
					   status);

	/* Only clear the transition state bit when the last prepared target
	 * is being reset — in multi-prep, other targets may still be active.
	 * prep_invalidate_target() decrements num_prepared_targets, so check
	 * before calling it: if only this one target is left, clear the bit.
	 */
	if (ifmgd->num_prepared_targets <= 1)
		clear_bit(SDATA_STATE_SMD_BSS_TRANSITION, &sdata->state);

	if (target->prep_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->prep_timeout_work);
		target->prep_timeout_started = false;
	}
	if (target->exec_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->exec_timeout_work);
		target->exec_timeout_started = false;
	}

	if (target->target_sta && status != WLAN_STATUS_SUCCESS) {
		struct sta_info *current_sta =
			sta_info_get(sdata, sdata->vif.cfg.ap_addr);
		struct ieee80211_uhr_link_reconfig_info *info;
		int ret = 0;

		if (current_sta) {
			info = kzalloc(sizeof(*info), GFP_KERNEL);
			if (info) {
				info->transitioning_links = target->prepared_links_mask;
				ether_addr_copy(info->target_ap_mld_addr,
						target->target_mld_addr);

				ret = drv_uhr_link_reconfig(local, sdata,
						    current_sta,
						    target->target_sta,
						    IEEE80211_UHR_LINK_RECONFIG_ABORT,
						    info);
				if (ret)
					sdata_info(sdata,
						   "smd: drv_uhr_link_reconfig(ABORT) failed: %d\n",
						   ret);
				kfree(info);
			}
		}

		/*
		 * Use the correct destruction path based on whether the STA
		 * was inserted into the hash table. __sta_info_destroy() requires
		 * the STA to be in the hash; sta_info_free() handles the
		 * pre-insertion case (e.g. exec timeout before STA fully setup).
		 */
		if (target->sta_inserted)
			WARN_ON(__sta_info_destroy(target->target_sta));
		else
			sta_info_free(local, target->target_sta);
		target->target_sta = NULL;
	}

	kfree(target->assoc_data);
	target->assoc_data = NULL;

	kfree(target->elems);
	target->elems = NULL;

	kfree(target->drv_info);
	target->drv_info = NULL;

	ieee80211_smd_free_target_links(target);

	ieee80211_smd_prep_invalidate_target(ifmgd, target);
}

void ieee80211_smd_prep_complete_target(struct ieee80211_sub_if_data *sdata,
					       struct ieee80211_smd_prep_target *target)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	unsigned int link_id;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!target || !target->valid)
		return;

	/* Cancel exec timeout only if it was actually started. */
	if (target->prep_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->prep_timeout_work);
		target->prep_timeout_started = false;
	}
	if (target->exec_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->exec_timeout_work);
		target->exec_timeout_started = false;
	}

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++)
		target->new_links[link_id] = NULL;

	kfree(target->assoc_data);
	target->assoc_data = NULL;

	cfg80211_notify_smd_bss_transition(sdata->dev,
					   target->target_mld_addr,
					   NL80211_SMD_TRANSITION_COMPLETE,
					   WLAN_STATUS_SUCCESS);

	/* Clear the SMD BSS transition state bit only when the last target
	 * completes — other targets may still be in flight in multi-prep.
	 */
	if (ifmgd->num_prepared_targets <= 1)
		clear_bit(SDATA_STATE_SMD_BSS_TRANSITION, &sdata->state);

	ieee80211_smd_prep_invalidate_target(ifmgd, target);
}

void ieee80211_smd_prep_reset(struct ieee80211_sub_if_data *sdata,
			      u16 status_code, u16 type)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	int i;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!ieee80211_vif_is_mld(&sdata->vif))
		return;

	if (!ifmgd->prep_targets)
		return;

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		if (ifmgd->prep_targets[i].valid)
			ieee80211_smd_prep_reset_target(sdata,
							&ifmgd->prep_targets[i],
							status_code, type);
	}

	/* Reset global SMD state after all per-target cleanup is done */
	ifmgd->smd_dialog_token_alloc = 0;
}

int ieee80211_smd_parse_trans_params(struct ieee80211_sub_if_data *sdata,
					    const u8 *data, size_t len,
					    struct ieee80211_smd_prep_target *target)
{
	size_t remaining = len;
	const u8 *pos = data;

	/* Minimum: Presence Bitmap (1) = 4 */
	if (remaining < 1)
		return -EINVAL;

	target->smd_bss_trans_flags = *pos++;
	remaining--;

	if (target->smd_bss_trans_flags & SMD_BSS_TRANS_FLAG_AID_PRESENT) {
		if (remaining < 2)
			return -EINVAL;

		target->target_aid = get_unaligned_le16(pos);
		pos += 2;
		remaining -= 2;
	} else {
		target->target_aid = 0;
	}

	if (target->smd_bss_trans_flags & SMD_BSS_TRANS_FLAG_DL_BA_INFO_PRESENT) {
		int num_tids;

		if (remaining < 1)
			return -EINVAL;

		num_tids = hweight8(*pos++);
		remaining--;

		/* Extended BA Parameters Info (3 bytes per TID) */
		if (remaining < num_tids * 3)
			return -EINVAL;

		pos += num_tids * 3;
		remaining -= num_tids * 3;
	}

	if (target->smd_bss_trans_flags & SMD_BSS_TRANS_FLAG_UL_BA_INFO_PRESENT) {
		int num_tids;

		/* UL TID Bitmap (1 byte) */
		if (remaining < 1)
			return -EINVAL;

		num_tids = hweight8(*pos++);
		remaining--;

		/* Extended BA Parameters Info (3 bytes per TID) */
		if (remaining < num_tids * 3)
			return -EINVAL;

		pos += num_tids * 3;
		remaining -= num_tids * 3;
	}

	if (target->smd_bss_trans_flags & SMD_BSS_TRANS_FLAG_SCS_LIST_PRESENT) {
		u8 num_scs;

		/* Number of SCS IDs (1 byte) */
		if (remaining < 1)
			return -EINVAL;

		num_scs = *pos++;
		remaining--;

		/* SCS ID List (1 byte per ID) */
		if (remaining < num_scs)
			return -EINVAL;

		pos += num_scs;
		remaining -= num_scs;
	}

	return 0;
}

int ieee80211_smd_parse_exec_trans_params(struct ieee80211_sub_if_data *sdata,
					  const u8 *data, size_t len,
					  u32 *dl_drain_time)
{
	size_t remaining = len;
	const u8 *pos = data;
	u8 st_control;
	u16 dl_drain_tu;

	*dl_drain_time = 0;

	/* Minimum size: Presence Bitmap (1) = 4 octets */
	if (remaining < 1)
		return -EINVAL;

	st_control = *pos++;

	remaining--;

/* B0: Nominal Maximum DL Draining Period Duration Present */
#define SMD_EXEC_PRESENCE_DL_DRAIN_PRESENT	BIT(0)
/* B1: Latest UL SN Present */
#define SMD_EXEC_PRESENCE_LATEST_UL_SN_PRESENT	BIT(1)

	if (st_control & SMD_EXEC_PRESENCE_DL_DRAIN_PRESENT) {
		if (remaining < 2)
			return -EINVAL;

		dl_drain_tu = get_unaligned_le16(pos);
		pos += 2;
		remaining -= 2;

		if (dl_drain_tu == 0)
			return -EINVAL;

		*dl_drain_time = dl_drain_tu;
	}

	if (st_control & SMD_EXEC_PRESENCE_LATEST_UL_SN_PRESENT) {
		u8 tid_bitmap;
		int tid, num_tids = 0;

		if (remaining < 1)
			return -EINVAL;

		tid_bitmap = *pos++;
		remaining--;

		for (tid = 0; tid < 8; tid++) {
			if (tid_bitmap & BIT(tid))
				num_tids++;
		}

		if (num_tids > 0) {
			size_t sn_bytes = (num_tids * 12 + 7) / 8;

			if (remaining < sn_bytes)
				return -EINVAL;

			pos += sn_bytes;
			remaining -= sn_bytes;
		}
	}

	return 0;
}

int ieee80211_smd_parse_ml_persta(struct ieee80211_sub_if_data *sdata,
				  const u8 *data, size_t len,
				  struct ieee80211_smd_prep_target *target)
{
	const struct ieee80211_multi_link_elem *ml = (const void *)data;
	const struct ieee80211_mle_basic_common_info *common;
	const struct element *sub;
	int link_count = 0;
	u16 ml_control;

	if (!ieee80211_mle_size_ok(data, len))
		return -EINVAL;

	ml_control = le16_to_cpu(ml->control);

	if ((ml_control & IEEE80211_ML_CONTROL_TYPE) !=
	     IEEE80211_ML_CONTROL_TYPE_BASIC) {
		return 0; /* Not an error, just skip */
	}

	if (len >= sizeof(*ml) + sizeof(*common)) {
		common = (const void *)ml->variable;

		if (!ether_addr_equal(common->mld_mac_addr, target->target_mld_addr)) {
			sdata_info(sdata, "smd: ML MLD addr mismatch got %pM expected %pM\n",
				   common->mld_mac_addr, target->target_mld_addr);
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	for_each_mle_subelement(sub, data, len) {
		const struct ieee80211_mle_per_sta_profile *prof;
		const u8 *sta_info;
		u16 sta_control;
		u8 link_id;

		if (sub->id != IEEE80211_MLE_SUBELEM_PER_STA_PROFILE)
			continue;

		if (!ieee80211_mle_basic_sta_prof_size_ok(sub->data, sub->datalen))
			continue;

		prof = (const void *)sub->data;
		sta_control = le16_to_cpu(prof->control);
		link_id = u16_get_bits(sta_control, IEEE80211_MLE_STA_CONTROL_LINK_ID);

		if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
			continue;

		if (!(target->prepared_links_mask & BIT(link_id)))
			continue;

		if (!target->assoc_data || !target->assoc_data->link[link_id].bss)
			continue;

		if (sta_control & IEEE80211_MLE_STA_CONTROL_STA_MAC_ADDR_PRESENT) {
			sta_info = prof->variable;

			ether_addr_copy(target->assoc_data->link[link_id].addr,
					sta_info);

			if (prof->sta_info_len >= 7) {
				target->assoc_data->link[link_id].elems =
					(u8 *)prof->variable + 6;
				target->assoc_data->link[link_id].elems_len =
					prof->sta_info_len - 7;
			}
		}

		link_count++;
	}

	return 0;
}

int ieee80211_smd_alloc_target_sta(struct ieee80211_sub_if_data *sdata,
				   struct ieee80211_smd_prep_target *target)
{
	struct ieee80211_local *local = sdata->local;
	struct sta_info *sta = NULL;
	struct link_sta_info *link_sta;
	int first_link_id = -1;
	int first_sap_link_id = -1;
	int link_id, err;

	lockdep_assert_wiphy(local->hw.wiphy);

	u16 trans_links = target->prepared_links_mask;

	if (target->primary_link_id >= 0)
		trans_links &= ~BIT(target->primary_link_id);

	if (trans_links) {
		/* MLO: use first partner (transitioning) link as first_link_id */
		first_link_id = ffs(trans_links) - 1;
	} else {
		/* SLO: no partner links, use primary link */
		for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
			if (target->assoc_data && target->assoc_data->link[link_id].bss) {
				first_link_id = link_id;
				break;
			}
		}
	}

	if (first_link_id < 0) {
		sdata_info(sdata, "smd: no valid target links found\n");
		return -EINVAL;
	}

	/* For different-links map, sta_info is SAP-indexed (vdev routing).
	 * first_sap_link_id is the SAP slot that the firmware vdev will use.
	 * For same-links map tap_to_sap_link[] is identity so
	 * first_sap_link_id == first_link_id.
	 */
	first_sap_link_id = target->tap_to_sap_link[first_link_id];

	if (WARN_ON_ONCE(first_sap_link_id < 0 ||
			 first_sap_link_id >= IEEE80211_MLD_MAX_NUM_LINKS)) {
		sdata_info(sdata, "smd: invalid sap_link_id %d for tap_link_id %d\n",
			   first_sap_link_id, first_link_id);
		return -EINVAL;
	}

	/*
	 * Step 1: Allocate sta_info with first link as deflink.
	 *
	 * sta_info_alloc_with_link() sets valid_links = BIT(first_sap_link_id).
	 * KEEP valid_links = BIT(first_sap_link_id) — do NOT reset to 0.
	 *
	 * This is critical: when sta_info_insert() calls drv_sta_state(NOTEXIST→NONE),
	 * ath12k derives the vdev_id from valid_links:
	 *   link_id = ffs(sta->sta.valid_links) - 1 = first_sap_link_id
	 *   → WMI_PEER_CREATE(vdev_id=first_sap_link_id, peer=bssid[first_link_id])
	 *
	 * Since first_sap_link_id is the SAP slot for the first PARTNER link, and
	 * vdev[first_sap_link_id] has already been started for AP-B
	 * (via ieee80211_link_use_channel in Step 7),
	 * WMI_PEER_CREATE goes to the correct vdev.
	 *
	 * For activate_link(L1) in assoc_success: BIT(first_sap_link_id) is already in
	 * valid_links → activate_link is skipped
	 * (no WARN_ON in link_sta_info_hash_add).
	 * For activate_link(L2): BIT(2) NOT in valid_links → drv_change_sta_links(...)
	 * For activate_link(L0): BIT(0) NOT in valid_links → drv_change_sta_links(...)
	 */
	sta = sta_info_alloc_with_link(sdata, target->target_mld_addr,
				       first_sap_link_id,
				       target->assoc_data->link[first_link_id].bss->bssid,
				       GFP_KERNEL);
	if (!sta) {
		sdata_info(sdata, "smd: failed to allocate target sta_info\n");
		return -ENOMEM;
	}

	sta->sta.mlo = true;

	/* Step 3: Copy first link_sta address from assoc_data BSS.
	 * sta->link[] is SAP-indexed; look up using first_sap_link_id.
	 * BSS bssid is TAP-indexed; look up using first_link_id.
	 */
	rcu_read_lock();
	link_sta = rcu_dereference(sta->link[first_sap_link_id]);
	if (link_sta) {
		struct cfg80211_bss *bss = target->assoc_data->link[first_link_id].bss;

		memcpy(link_sta->addr, bss->bssid, ETH_ALEN);
		memcpy(link_sta->pub->addr, bss->bssid, ETH_ALEN);
	}
	rcu_read_unlock();

	if (!link_sta) {
		sdata_info(sdata, "smd: first link_sta not found\n");
		err = -EINVAL;
		goto out_free_sta;
	}

	/*
	 * Allocate ALL other prepared links BEFORE sta_info_insert().
	 *
	 * We need sta->link[link_id] != NULL for ALL prepared links so that
	 * ieee80211_sta_activate_link() can find them in assoc_success().
	 *
	 * sta_info_insert() is NOT called here. It is deferred to
	 * ieee80211_smd_sta_insert_and_auth() which runs from assoc_success()
	 * AFTER ieee80211_link_use_channel() (WMI_VDEV_START for AP-B L1).
	 * FW requires PEER_CREATE to come after VDEV_START for SMD target links.
	 */
	for_each_set_bit(link_id, (unsigned long *)&target->prepared_links_mask,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		struct link_sta_info *partner_link_sta;
		struct cfg80211_bss *partner_bss;
		int sap_link_id = target->tap_to_sap_link[link_id];

		if ((int)link_id == first_link_id || sap_link_id == first_sap_link_id)
			continue; /* already allocated by sta_info_alloc_with_link */

		if (WARN_ON_ONCE(sap_link_id < 0 ||
				 sap_link_id >= IEEE80211_MLD_MAX_NUM_LINKS))
			goto out_free_sta;

		if (!target->assoc_data || !target->assoc_data->link[link_id].bss) {
			sdata_info(sdata,
				   "smd: prepared link %d has no BSS in assoc_data, skipping\n",
				   link_id);
			continue;
		}

		/* Allocate link_sta at SAP slot (vdev routing).
		 * BSS bssid is TAP-indexed.
		 */
		err = ieee80211_sta_allocate_link(sta, sap_link_id);
		if (err) {
			sdata_info(sdata,
				   "smd: failed to allocate prepared link tap=%d sap=%d: %d\n",
				   link_id, sap_link_id, err);
			goto out_free_sta;
		}

		/* Copy partner link_sta address from assoc_data BSS.
		 * sta->link[] is SAP-indexed; BSS bssid is TAP-indexed.
		 */
		rcu_read_lock();
		partner_link_sta = rcu_dereference(sta->link[sap_link_id]);
		if (partner_link_sta) {
			partner_bss = target->assoc_data->link[link_id].bss;
			memcpy(partner_link_sta->addr, partner_bss->bssid, ETH_ALEN);
			memcpy(partner_link_sta->pub->addr, partner_bss->bssid, ETH_ALEN);
		}
		rcu_read_unlock();

	}

	/* Store — NO insert, NO state transitions yet */
	target->target_sta = sta;
	target->sta_inserted = false;

	return 0;

out_free_sta:
	/* Free sta that was never inserted */
	if (sta)
		sta_info_free(local, sta);

	return err;
}

int
ieee80211_smd_compute_prep_bitmaps(struct ieee80211_sub_if_data *sdata,
				   struct ieee80211_smd_prep_target *target,
				   struct ieee80211_mgd_assoc_data *assoc_data)
{
	u16 all_requested = 0;
	int lid;

	/* primary_link_id = TAP lid corresponding to the original SAP assoc band.
	 * Diagnostic: log smd_assoc_link_id and the remap to catch intermittent
	 * cases where primary_link_id != smd_assoc_link_id on same-links map.
	 * smd_assoc_link_id is saved from assoc_data->assoc_link_id at initial
	 * 802.11 association time (may be link 0 or 1 depending on which link
	 * the AP used for the (Re)Association frame exchange).
	 */

	target->primary_link_id =
		target->sap_to_tap_link[sdata->u.mgd.smd_assoc_link_id];
	target->dl_drain_link_mask = BIT(target->primary_link_id);
	target->tap_prepared_mask  = target->prepared_links_mask;

	if (!target->link_id_remap &&
	    target->primary_link_id != sdata->u.mgd.smd_assoc_link_id) {
		sdata_info(sdata,
			   "smd: same-links remap bug: primary_link_id=%d != smd_assoc_link_id=%d\n",
			   target->primary_link_id, sdata->u.mgd.smd_assoc_link_id);
		for (lid = 0; lid < IEEE80211_MLD_MAX_NUM_LINKS; lid++) {
			struct ieee80211_link_data *link =
				sdata_dereference(sdata->link[lid], sdata);
			if (!link)
				continue;
			sdata_info(sdata,
				   "smd: SAP link %d: chan=%pS band=%d sap_to_tap=%d\n",
				   lid,
				   link->conf->chanreq.oper.chan,
				   link->conf->chanreq.oper.chan
					? link->conf->chanreq.oper.chan->band : -1,
				   target->sap_to_tap_link[lid]);
		}
	}

	for (lid = 0; lid < IEEE80211_MLD_MAX_NUM_LINKS; lid++) {
		if (target->assoc_data->link[lid].bss)
			all_requested |= BIT(lid);
	}
	target->rejected_links_mask = all_requested & ~target->prepared_links_mask;

	if (!(target->prepared_links_mask & target->dl_drain_link_mask)) {
		sdata_info(sdata,
			   "smd: TAP rejected DL drain link (primary_link_id=%d) — aborting\n",
			   target->primary_link_id);
		return -EINVAL;
	}

	target->transitioning_links =
		target->prepared_links_mask & ~target->dl_drain_link_mask;

	WARN_ON(target->dl_drain_link_mask & ~target->prepared_links_mask);
	WARN_ON(target->primary_link_id < 0 ||
		target->primary_link_id >= IEEE80211_MLD_MAX_NUM_LINKS);

	return 0;
}

struct ieee80211_smd_target_link *
ieee80211_smd_alloc_target_link(struct ieee80211_sub_if_data *sdata,
				struct ieee80211_smd_prep_target *target,
				unsigned int link_id)
{
	struct ieee80211_smd_target_link *tgt_link;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		sdata_info(sdata, "smd: invalid link_id %u for target allocation\n",
			   link_id);
		return NULL;
	}

	if (target->new_links[link_id]) {
		sdata_info(sdata, "smd: target link %u already allocated\n", link_id);
		return target->new_links[link_id];
	}

	tgt_link = kzalloc(sizeof(*tgt_link), GFP_KERNEL);
	if (!tgt_link)
		return NULL;

	tgt_link->allocated = true;
	tgt_link->link_id = link_id;
	target->new_links[link_id] = tgt_link;

	return tgt_link;
}
