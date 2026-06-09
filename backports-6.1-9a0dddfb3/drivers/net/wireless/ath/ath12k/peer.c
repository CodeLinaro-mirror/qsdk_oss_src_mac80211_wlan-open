// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "peer.h"
#include "dp_peer.h"
#include "debug.h"
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include "ppe.h"
#endif
#include "telemetry_agent_if.h"
#include "mac.h"

/* Timer callback for peer deletion timeout */
static void ath12k_peer_del_timeout(struct timer_list *t)
{
	struct ath12k_peer_del_entry *entry = from_timer(entry, t, timer);
	struct ath12k_pdev *pdev = entry->pdev;
	struct ath12k_peer_del_tracker *tracker;
	struct ath12k_base *ab;
	bool removed = false;
	u8 addr[ETH_ALEN];
	u32 vdev_id;

	if (!pdev)
		return;

	tracker = pdev->peer_del_tracker;
	if (!tracker)
		return;

	ab = ath12k_pdev_to_ab(pdev);

	ether_addr_copy(addr, entry->addr);
	vdev_id = entry->vdev_id;

	/* Remove the peer from hash table */
	spin_lock_bh(&tracker->lock);
	/* Verify entry is still in the hash before removing */
	if (rhashtable_lookup_fast(&tracker->peer_del_hash, entry->addr,
				   tracker->hash_params) == entry) {
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		if (!is_zero_ether_addr(entry->mld_addr))
			rhashtable_remove_fast(&tracker->mld_del_hash,
					       &entry->mld_rhash_node,
					       tracker->mld_hash_params);
		removed = true;
	}
	spin_unlock_bh(&tracker->lock);

	if (removed) {
		ath12k_warn(ab, "peer delete timeout for %pM vdev %d, removing from tracker\n",
			    addr, vdev_id);

		/* Wake up any waiters on hash_delete_queue */
		wake_up_all(&tracker->hash_delete_queue);

		/* The WARN_ON check is skipped because the firmware does not send a peer
		 * delete response to the host while a recovery is in progress.
		 */
		if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
			WARN_ON(1);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	}
}

/* Initialize peer deletion tracker for a pdev */
int ath12k_peer_del_tracker_init(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker;
	int ret;

	tracker = kzalloc(sizeof(*tracker), GFP_KERNEL);
	if (!tracker)
		return -ENOMEM;

	spin_lock_init(&tracker->lock);
	init_waitqueue_head(&tracker->hash_delete_queue);

	/* Initialize link-addr hash table parameters */
	tracker->hash_params.key_offset = offsetof(struct ath12k_peer_del_entry, addr);
	tracker->hash_params.head_offset = offsetof(struct ath12k_peer_del_entry,
						    rhash_node);
	tracker->hash_params.key_len = ETH_ALEN;
	tracker->hash_params.automatic_shrinking = true;

	ret = rhashtable_init(&tracker->peer_del_hash, &tracker->hash_params);
	if (ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "failed to init peer_del hash table for pdev %d: %d\n",
			    pdev->pdev_id, ret);
		kfree(tracker);
		return ret;
	}

	/* Initialize MLD-addr hash table parameters */
	tracker->mld_hash_params.key_offset = offsetof(struct ath12k_peer_del_entry,
						       mld_addr);
	tracker->mld_hash_params.head_offset = offsetof(struct ath12k_peer_del_entry,
							mld_rhash_node);
	tracker->mld_hash_params.key_len = ETH_ALEN;
	tracker->mld_hash_params.automatic_shrinking = true;

	ret = rhashtable_init(&tracker->mld_del_hash, &tracker->mld_hash_params);
	if (ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "failed to init mld_del hash table for pdev %d: %d\n",
			    pdev->pdev_id, ret);
		rhashtable_destroy(&tracker->peer_del_hash);
		kfree(tracker);
		return ret;
	}

	pdev->peer_del_tracker = tracker;
	atomic_set(&pdev->peer_del_tracker_entries, 0);
	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer deletion tracker initialized for pdev %d\n", pdev->pdev_id);

	return 0;
}

/* Destroy peer deletion tracker */
void ath12k_peer_del_tracker_destroy(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;
	struct rhashtable_iter iter;

	if (!tracker)
		return;

	/* Prevent new entries from being added */
	spin_lock_bh(&tracker->lock);
	pdev->peer_del_tracker = NULL;
	spin_unlock_bh(&tracker->lock);

	/* Clean up all remaining entries */
	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		/* Cancel the timer before removing entry */
		del_timer_sync(&entry->timer);

		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		if (!is_zero_ether_addr(entry->mld_addr))
			rhashtable_remove_fast(&tracker->mld_del_hash,
					       &entry->mld_rhash_node,
					       tracker->mld_hash_params);
		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	rhashtable_destroy(&tracker->peer_del_hash);
	rhashtable_destroy(&tracker->mld_del_hash);
	kfree(tracker);
	atomic_set(&pdev->peer_del_tracker_entries, 0);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer deletion tracker destroyed for pdev %d\n", pdev->pdev_id);
}

/* Add a peer to the deletion tracking hash */
int ath12k_peer_del_tracker_add(struct ath12k_pdev *pdev, u32 vdev_id,
				const u8 *addr, const u8 *mld_addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;
	int ret;

	if (!tracker)
		return -EINVAL;

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	entry->vdev_id = vdev_id;
	ether_addr_copy(entry->addr, addr);
	if (mld_addr && !is_zero_ether_addr(mld_addr))
		ether_addr_copy(entry->mld_addr, mld_addr);
	entry->pdev = pdev;

	timer_setup(&entry->timer, ath12k_peer_del_timeout, 0);

	spin_lock_bh(&tracker->lock);
	ret = rhashtable_insert_fast(&tracker->peer_del_hash,
				     &entry->rhash_node,
				     tracker->hash_params);
	if (ret) {
		spin_unlock_bh(&tracker->lock);
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "failed to add peer %pM vdev %d to del tracker: %d\n",
			    addr, vdev_id, ret);
		kfree(entry);
		return ret;
	}

	if (!is_zero_ether_addr(entry->mld_addr)) {
		ret = rhashtable_insert_fast(&tracker->mld_del_hash,
					     &entry->mld_rhash_node,
					     tracker->mld_hash_params);
		if (ret) {
			rhashtable_remove_fast(&tracker->peer_del_hash,
					       &entry->rhash_node,
					       tracker->hash_params);
			spin_unlock_bh(&tracker->lock);
			ath12k_warn(ath12k_pdev_to_ab(pdev),
				    "failed to add peer %pM mld %pM to mld del tracker: %d\n",
				    addr, mld_addr, ret);
			kfree(entry);
			return ret;
		}
	}
	spin_unlock_bh(&tracker->lock);

	/* Start the 10 second timer */
	mod_timer(&entry->timer,
		  jiffies + msecs_to_jiffies(ATH12K_PEER_DELETE_TIMEOUT_MS));

	atomic_inc(&pdev->peer_del_tracker_entries);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "added peer %pM mld %pM vdev %d to deletion tracker with timer\n",
		   addr, entry->mld_addr, vdev_id);

	return 0;
}

/* Remove a peer from the deletion tracking hash */
void ath12k_peer_del_tracker_remove(struct ath12k_pdev *pdev, u32 vdev_id, const u8 *addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;

	if (!tracker)
		return;

	spin_lock_bh(&tracker->lock);
	entry = rhashtable_lookup_fast(&tracker->peer_del_hash, addr,
				       tracker->hash_params);
	if (entry && entry->vdev_id == vdev_id) {
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		if (!is_zero_ether_addr(entry->mld_addr))
			rhashtable_remove_fast(&tracker->mld_del_hash,
					       &entry->mld_rhash_node,
					       tracker->mld_hash_params);
		spin_unlock_bh(&tracker->lock);

		del_timer_sync(&entry->timer);

		if (!is_zero_ether_addr(entry->mld_addr))
			ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
				   "removed peer %pM mld %pM vdev %d from deletion tracker and cancelled timer\n",
				   addr, entry->mld_addr, vdev_id);
		else
			ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
				   "removed peer %pM vdev %d from deletion tracker and cancelled timer\n",
				   addr, vdev_id);

		/* Wake up any waiters on hash_delete_queue */
		wake_up_all(&tracker->hash_delete_queue);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	} else {
		spin_unlock_bh(&tracker->lock);
		if (!entry) {
			ath12k_err(ath12k_pdev_to_ab(pdev),
				   "peer %pM not found in deletion tracker\n", addr);
		} else {
			ath12k_err(ath12k_pdev_to_ab(pdev),
				   "peer %pM vdev mismatch (expected %d, found %d)\n",
				   addr, vdev_id, entry->vdev_id);
		}
	}
}

/* Check if a peer is in the deletion tracking hash.
 * Returns -EEXIST if found, 0 if not found.
 */
int ath12k_peer_del_tracker_check(struct ath12k_pdev *pdev, const u8 *addr,
				  const u8 *mld_addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	bool check_mld = mld_addr && !is_zero_ether_addr(mld_addr);

	if (!tracker)
		return 0;

	spin_lock_bh(&tracker->lock);

	/* Search for link addr duplication. */
	if (rhashtable_lookup_fast(&tracker->peer_del_hash, addr,
				   tracker->hash_params)) {
		spin_unlock_bh(&tracker->lock);
		return -EEXIST;
	}

	/* In case of MLO peer, the MLD MAC will be added in the AST */

	/* Search for duplication of the in-progress delete, MLD MAC */
	if (rhashtable_lookup_fast(&tracker->mld_del_hash, addr,
				   tracker->mld_hash_params)) {
		spin_unlock_bh(&tracker->lock);
		return -EEXIST;
	}

	if (!check_mld) {
		spin_unlock_bh(&tracker->lock);
		return 0;
	}

	/* Search for duplication of the in-progress delete, link MAC */
	if (rhashtable_lookup_fast(&tracker->peer_del_hash, mld_addr,
				   tracker->hash_params)) {
		spin_unlock_bh(&tracker->lock);
		return -EEXIST;
	}

	/* Search for duplication of the in-progress delete, MLD MAC */
	if (rhashtable_lookup_fast(&tracker->mld_del_hash, mld_addr,
				   tracker->mld_hash_params)) {
		spin_unlock_bh(&tracker->lock);
		return -EEXIST;
	}

	spin_unlock_bh(&tracker->lock);
	return 0;
}

/* Wait for a peer to be removed from deletion tracker */
int ath12k_peer_del_tracker_wait(struct ath12k_pdev *pdev, const u8 *addr,
				 unsigned long timeout_ms, const u8 *mld_addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	long ret;

	if (!tracker)
		return -EINVAL;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "waiting for peer %pM to be removed from deletion tracker (timeout %lu ms)\n",
		   addr, timeout_ms);

	ret = wait_event_timeout(tracker->hash_delete_queue,
				 !ath12k_peer_del_tracker_check(pdev, addr,
								mld_addr),
				 msecs_to_jiffies(timeout_ms));

	if (!ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "timeout waiting for peer %pM deletion\n", addr);
		return -ETIMEDOUT;
	}

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer %pM removed from deletion tracker\n", addr);

	return 0;
}

/* Clear all peers for a specific vdev from deletion tracker */
int ath12k_peer_del_tracker_clear_vdev(struct ath12k_pdev *pdev, u32 vdev_id)
{
	struct ath12k *ar = pdev->ar;
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry, **entries_to_remove;
	struct rhashtable_iter iter;
	int count = 0, i, num_entries = 0;
	int max_entries;

	if (!tracker)
		return -EINVAL;

	if (!atomic_read(&pdev->peer_del_tracker_entries))
		return 0;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "clearing deletion tracker for vdev %d\n", vdev_id);

	max_entries = ar->max_num_stations;

	entries_to_remove = kcalloc(max_entries, sizeof(*entries_to_remove),
				    GFP_KERNEL);
	if (!entries_to_remove)
		return -ENOMEM;

	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		if (entry->vdev_id == vdev_id && num_entries < max_entries)
			entries_to_remove[num_entries++] = entry;
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	for (i = 0; i < num_entries; i++) {
		entry = entries_to_remove[i];
		del_timer_sync(&entry->timer);

		spin_lock_bh(&tracker->lock);
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		if (!is_zero_ether_addr(entry->mld_addr))
			rhashtable_remove_fast(&tracker->mld_del_hash,
					       &entry->mld_rhash_node,
					       tracker->mld_hash_params);
		spin_unlock_bh(&tracker->lock);

		ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
			   "removed peer %pM from deletion tracker (vdev %d)\n",
			   entry->addr, vdev_id);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
		count++;
	}

	if (count > 0)
		wake_up_all(&tracker->hash_delete_queue);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "cleared %d peers from deletion tracker for vdev %d\n",
		   count, vdev_id);

	kfree(entries_to_remove);

	return 0;
}

int ath12k_peer_del_tracker_clear_pdev(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry, **entries_to_remove;
	struct rhashtable_iter iter;
	int count = 0, i, num_entries = 0;
	int max_entries = ATH12K_PEER_DEL_TRACKER_MAX_ENTRIES;

	if (!tracker)
		return -EINVAL;

	if (!atomic_read(&pdev->peer_del_tracker_entries))
		return 0;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "clearing deletion tracker for pdev %d\n", pdev->pdev_id);

	entries_to_remove = kcalloc(max_entries, sizeof(*entries_to_remove),
				    GFP_KERNEL);
	if (!entries_to_remove)
		return -ENOMEM;

	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		if (num_entries < max_entries)
			entries_to_remove[num_entries++] = entry;
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	for (i = 0; i < num_entries; i++) {
		entry = entries_to_remove[i];
		del_timer_sync(&entry->timer);

		spin_lock_bh(&tracker->lock);
		rhashtable_remove_fast(&tracker->peer_del_hash, &entry->rhash_node,
				       tracker->hash_params);
		if (!is_zero_ether_addr(entry->mld_addr))
			rhashtable_remove_fast(&tracker->mld_del_hash,
					       &entry->mld_rhash_node,
					       tracker->mld_hash_params);
		spin_unlock_bh(&tracker->lock);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
		count++;
	}

	if (count > 0)
		wake_up_all(&tracker->hash_delete_queue);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "cleared %d peers from deletion tracker for pdev %d\n",
		   count, pdev->pdev_id);

	kfree(entries_to_remove);

	return 0;
}

static int ath12k_wait_for_peer_common(struct ath12k *ar, int vdev_id,
				       const u8 *addr, bool expect_mapped)
{
	int ret;

	ret = wait_event_timeout(ar->ab->peer_mapping_wq,
				 ar->peer_map_event.received  ||
				 test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)
				 , 3 * HZ);

	if (ret <= 0)
		return -ETIMEDOUT;

	return 0;
}

struct ath12k_peer_cleanup_ctx {
	struct ath12k *ar;
	u32 vdev_id;
	u32 num_ml_peers;
};

static bool ath12k_peer_cleanup_vdev_match(struct ath12k_dp_link_peer *peer,
					   void *context)
{
	struct ath12k_peer_cleanup_ctx *ctx = context;

	if (peer->vdev_id != ctx->vdev_id)
		return false;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return false;

	if (peer->mlo && !peer->is_bridge_peer)
		ctx->num_ml_peers++;

	ath12k_warn(ctx->ar->ab,
		    "removing stale remote peer %pM from vdev_id %d\n",
		    peer->addr, peer->vdev_id);

	return true;
}

void ath12k_peer_cleanup(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_peer_cleanup_ctx ctx = {
		.ar = ar,
		.vdev_id = vdev_id,
		.num_ml_peers = 0,
	};
	struct ath12k_base *ab = ar->ab;
	int count;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	count = ath12k_dp_link_peer_batch_cleanup(ar,
						  ath12k_peer_cleanup_vdev_match,
						  &ctx);
	if (count > 0) {
		ar->num_peers -= count;
		ar->num_ml_peers -= ctx.num_ml_peers;
		ath12k_dbg(ab, ATH12K_DBG_PEER,
			   "cleaned up %d stale peers from vdev_id %d\n",
			   count, vdev_id);
	}
}

static int ath12k_peer_delete_send(struct ath12k *ar, u32 vdev_id, const u8 *addr,
				   u32 mlo_hw_link_id_bitmap,
				   struct ath12k_sta *ahsta, int link_id,
				   bool peer_delete_send_mlo_hw_bitmap)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_sta *sta = NULL;
	const u8 *mld_addr = NULL;
	int ret;
	u16 link_id_mask = 0;


	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ahsta)
		sta = ath12k_ahsta_to_sta(ahsta);

	if (ath12k_is_mlo_sta(sta, ahsta))
		mld_addr = sta->addr;

	if (ahsta && link_id != -1) {
		ahsta->peer_delete_cmd_sent_bitmap |= BIT(link_id);
		link_id_mask = BIT(link_id);
	}

	if (ar->pdev->peer_del_tracker) {
		ret = ath12k_peer_del_tracker_add(ar->pdev, vdev_id, addr, mld_addr);
		if (ret)
			ath12k_warn(ab, "failed to add peer %pM to deletion tracker: %d\n",
				    addr, ret);
	}

	ret = ath12k_wmi_send_peer_delete_cmd(ar, addr, vdev_id,
					      mlo_hw_link_id_bitmap,
					      peer_delete_send_mlo_hw_bitmap);

	if (ret) {
		ath12k_warn(ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    vdev_id, addr, ret);

		if (ahsta && link_id_mask)
			ahsta->peer_delete_cmd_sent_bitmap &= ~link_id_mask;

		/* Remove the peer entry from the peer_del tracker when sending the WMI
		 * peer delete command fails.
		 */
		ath12k_peer_del_tracker_remove(ar->pdev, vdev_id, addr);

		return ret;
	}

	return 0;
}

static int __ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
				bool skip_peer_del, u32 mlo_hw_link_id_bitmap,
				bool peer_delete_send_mlo_hw_bitmap)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_link_sta *arsta;
	int link_id = -1;
	int ret;
	bool was_mlo = false;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->arsta_lock);
	arsta = ath12k_link_sta_find_by_addr(ar, addr);
	if (arsta && arsta->ahsta) {
		ahsta = arsta->ahsta;
		link_id = arsta->link_id;
		if (arsta->ahsta->is_mlo && !arsta->is_bridge_peer)
			was_mlo = true;
	}
	spin_unlock_bh(&ar->arsta_lock);

	ath12k_dp_link_peer_unassign(ar, vdev_id, addr);

	ath12k_dp_arch_link_peer_delete(ar->ab->dp, ar->ab, vdev_id, addr);

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab, "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
				vdev_id, addr, -EHOSTDOWN);

		return -EHOSTDOWN;
	}

	if (!skip_peer_del) {
		ret = ath12k_peer_delete_send(ar, vdev_id, addr,
					      mlo_hw_link_id_bitmap, ahsta,
					      link_id, peer_delete_send_mlo_hw_bitmap);
		if (ret)
			return ret;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (!arvif) {
		ath12k_warn(ab,"failed to get arvif with vdev_id %d,"
			    "skip ppeds ast override\n",
			    vdev_id);
	} else if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(ab, arvif, false);
#endif
	}

	rcu_read_unlock();
	/* Decrement ML peer count for this radio if it was an MLO station */
	if (was_mlo)
		ar->num_ml_peers--;

	if (arvif && !skip_peer_del)
		arvif->num_peers--;

	return 0;
}

int ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
		       bool skip_peer_del, u32 mlo_hw_link_id_bitmap,
		       bool peer_delete_send_mlo_hw_bitmap)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ret = __ath12k_peer_delete(ar, vdev_id, addr, skip_peer_del,
				   mlo_hw_link_id_bitmap,
				   peer_delete_send_mlo_hw_bitmap);
	if (ret && ret != -EHOSTDOWN)
		return ret;

	ar->num_peers--;

	return ret;
}

static int ath12k_wait_for_peer_created(struct ath12k *ar, int vdev_id, const u8 *addr)
{
	return ath12k_wait_for_peer_common(ar, vdev_id, addr, true);
}

static int ath12k_wait_for_peer_create_done(struct ath12k *ar, u32 vdev_id,
					    const u8 *addr)
{
	int ret;
	unsigned long time_left;

	/* Wait for HTT peer map event only when required */
	if (ar->ab->map_event_required) {
		ret = ath12k_wait_for_peer_created(ar, vdev_id, addr);
		if (ret) {
			ath12k_warn(ar->ab, "failed wait for peer create addr : %pM\n",
				    addr);
			WARN_ON(1);
			return ret;
		}
	}

	time_left = wait_for_completion_timeout(&ar->peer_create_done,
						3 * HZ);
	if (time_left == 0) {
		ath12k_warn(ar->ab, "Timeout in receiving peer create conf peer_addr : %pM\n",
			    addr);
		return -ETIMEDOUT;
	}

	return 0;
}

static int ath12k_track_peer_delete(struct ath12k *ar,
				    struct ieee80211_sta *sta, const u8 *addr)
{
	struct ath12k_sta *ahsta = NULL;
	const u8 *mld_addr = NULL;
	int ret;

	if (sta)
		ahsta = ath12k_sta_to_ahsta(sta);

	if (ath12k_is_mlo_sta(sta, ahsta))
		mld_addr = sta->addr;

	ret = ath12k_peer_del_tracker_check(ar->pdev, addr, mld_addr);
	if (!ret)
		return 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
		   "peer %pM delete is pending, waiting for 3 seconds\n",
		   addr);

	ret = ath12k_peer_del_tracker_wait(ar->pdev, addr,
					   ATH12K_PEER_DEL_TRACKER_TIMEOUT_MS,
					   mld_addr);
	if (ret)
		ath12k_err(ar->ab,
			   "peer %pM still in deletion tracker after 3s, cannot create\n",
			   addr);
	return ret;
}

int ath12k_peer_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
		       struct ieee80211_sta *sta,
		       struct ath12k_wmi_peer_create_arg *arg)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	u8 link_id = arvif->link_id;
	struct ath12k_sta *ahsta = NULL;
	int ret;
	u32 mlo_hw_link_id_bitmap = 0, peer_delete_send_mlo_hw_bitmap = 0;
	struct ath12k_peer_map_pending_event *map_event = &ar->peer_map_event;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		mlo_hw_link_id_bitmap = ahsta->mlo_hw_link_id_bitmap;
		peer_delete_send_mlo_hw_bitmap = ahsta->peer_delete_send_mlo_hw_bitmap;
	}

	/* Check if peer is in deletion tracker and wait if necessary */
	if (ar->pdev->peer_del_tracker) {
		ret = ath12k_track_peer_delete(ar, sta, arg->peer_addr);
		if (ret)
			return ret;
	}

	if (ar->num_peers >= (ar->max_num_peers - 1)) {
		ath12k_warn(ar->ab,
			    "failed to create peer due to insufficient peer entry resource in firmware\n");
		return -ENOBUFS;
	}

	reinit_completion(&ar->peer_create_done);

	memset(map_event, 0, sizeof(struct ath12k_peer_map_pending_event));

	memcpy(map_event->pending_peer_addr, arg->peer_addr, ETH_ALEN);
	map_event->pending_peer_vdev_id = arg->vdev_id;
	map_event->received = false;
	map_event->peer_id = ATH12K_MLO_PEER_ID_INVALID;

	ret = ath12k_wmi_send_peer_create_cmd(ar, arg);
	if (ret) {
		memset(map_event, 0, sizeof(struct ath12k_peer_map_pending_event));
		ath12k_warn(ar->ab,
			    "failed to send peer create vdev_id %d ret %d\n",
			    arg->vdev_id, ret);
		return ret;
	}

	ret = ath12k_wait_for_peer_create_done(ar, arg->vdev_id,
					       arg->peer_addr);

	if (ret) {
		memset(map_event, 0, sizeof(struct ath12k_peer_map_pending_event));
		return ret;
	}

	ret = ath12k_dp_link_peer_assign(ar, arvif->vdev_id,
					 sta, arg->peer_addr,
					 link_id, vif,
					 arvif->ahvif->dp_vif.ppe_vp_type,
					 arvif->ahvif->dp_vif.ppe_vp_num,
					 arg->mlo_bridge_peer);

	memset(map_event, 0, sizeof(struct ath12k_peer_map_pending_event));

	if (ret) {
		ath12k_peer_delete(ar, arg->vdev_id, arg->peer_addr, false, 0, false);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "peer created %pM\n", arg->peer_addr);

	ar->num_peers++;

	if (sta && sta->mlo) {
		/* Count one ML peer per radio for real link peers */
		if (!arg->mlo_bridge_peer)
			ar->num_ml_peers++;
	}

	return ret;
}

int ath12k_peer_dp_cp_link_peer_delete(struct ath12k_link_vif *arvif,
				       struct ath12k_sta *ahsta, u8 link_id,
				       u8 *addr, u32 mlo_hw_link_id_bitmap,
				       bool peer_delete_send_mlo_hw_bitmap)
{
	bool ml_peer_del_all = false;
	struct ath12k *ar;
	int ret;

	if (!arvif)
		return 0;

	ar = arvif->ar;

	ml_peer_del_all = ar->ab->hw_params->peer_del_all_support;
	ath12k_dp_peer_cleanup(ar, arvif->vdev_id, addr);
	ath12k_dp_cp_link_peer_unassign(ar, arvif, ahsta, link_id, addr);
	ath12k_dp_arch_link_peer_delete(ar->ab->dp, ar->ab,
					arvif->vdev_id, addr);

	if (ml_peer_del_all && arvif->peer_del_all_enable) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
			   "Skipping peer delete for %pM due to peer_del_all:%d\n",
			   addr, arvif->peer_del_all_enable);
		return 0;
	}

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab,
			    "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
			    arvif->vdev_id, addr, -EHOSTDOWN);
		return -EHOSTDOWN;
	}

	ret = ath12k_peer_delete_send(ar, arvif->vdev_id, addr,
				      mlo_hw_link_id_bitmap, ahsta,
				      link_id, peer_delete_send_mlo_hw_bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    arvif->vdev_id, addr, ret);
	}

	return ret;
}

int ath12k_peer_mlo_link_peers_delete(struct ath12k_vif *ahvif,
				      struct ath12k_sta *ahsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	char link_addr[ATH12K_NUM_MAX_LINKS][ETH_ALEN];
	unsigned long links;
	struct ath12k *ar;
	int ret, err_ret = 0, primary_link_id;
	u32 mlo_hw_link_id_bitmap;
	u8 link_id;
	bool bitmap_flag = 0;
	bool ml_peer_del_all = false;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!sta->mlo)
		return -EINVAL;

	/* FW expects delete of all link peers at once before waiting for reception
	 * of peer unmap or delete responses
	 */
	links = ahsta->links_map;
	primary_link_id = ahsta->primary_link_id;
	mlo_hw_link_id_bitmap = ahsta->mlo_hw_link_id_bitmap;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

		ar = arvif->ar;
		if (!ar)
			continue;

#ifdef CPTCFG_QCN_EXTN
		ath12k_smart_ant_api_peer_disconnect(arsta);
#endif

		memcpy(link_addr[link_id], arsta->addr, ETH_ALEN);
		ml_peer_del_all = ar->ab->hw_params->peer_del_all_support;

		bitmap_flag = ahsta->peer_delete_send_mlo_hw_bitmap;
		ret = ath12k_peer_dp_cp_link_peer_delete(arvif, ahsta, link_id,
							 link_addr[link_id],
							 mlo_hw_link_id_bitmap,
							 bitmap_flag);
		if (ret)
			err_ret = ret;

		ar->num_peers--;

		/*
		 * arvif::num_peers will be decremented during vdev stop
		 * after sending peer delete all WMI cmd.
		 * Skip for now when peer_del_all_enable is set.
		 */
		if (ml_peer_del_all && arvif->peer_del_all_enable)
			continue;

		if (ahsta->peer_delete_cmd_sent_bitmap & BIT(link_id))
			arvif->num_peers--;
	}

	return err_ret;
}

void ath12k_mac_peer_disassoc(struct ath12k_base *ab, struct ieee80211_sta *sta,
			      struct ath12k_sta *ahsta,
			      enum ath12k_debug_mask debug_mask)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (!ahsta->low_ack_sent) {
		ath12k_dbg_level(ab, debug_mask, ATH12K_DBG_L1,
				 "sending low ack for/disassoc:%pM\n",
				 sta->addr);
		/* set num of packets to maximum so that we distinguish in
		 * the hostapd to send disassoc irrespective of hostapd conf
		 */
		ieee80211_report_low_ack(sta, ATH12K_REPORT_LOW_ACK_NUM_PKT);
		/* Using this flag to avoid certain known warnings which
		 * will be triggerred when umac reset is happening
		 */
		ahsta->low_ack_sent = true;
		/* Track peers marked for deletion during Mode 2 recovery
		 * to send mlo_hw_link_id_bitmap only for them.
		 */
		if (ag && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
			ahsta->peer_delete_send_mlo_hw_bitmap = true;
	}
}

static u64 ath12k_htlist_hash_key(const u8 *addr)
{
	return ether_addr_to_u64(addr);
}

static u32 ath12k_link_sta_hash_idx(struct ath12k *ar, const u8 *addr)
{
	u64 key = ath12k_htlist_hash_key(addr);

	return hash_min(key, ar->arsta_hash_bits);
}

int ath12k_link_sta_hlist_add(struct ath12k *ar,
			      struct ath12k_link_sta *arsta)
{
	struct ath12k_link_sta *tmp;
	struct hlist_head *bucket;

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return -EINVAL;

	bucket = &ar->arsta_list[ath12k_link_sta_hash_idx(ar, arsta->addr)];
	hlist_for_each_entry(tmp, bucket, hlist_addr) {
		if (ether_addr_equal(tmp->addr, arsta->addr))
			return -EEXIST;
	}

	if (!hlist_unhashed(&arsta->hlist_addr)) {
		ath12k_warn(ar->ab, "arsta %pM already in hash list\n", arsta->addr);
		return -EEXIST;
	}

	hlist_add_head(&arsta->hlist_addr, bucket);
	return 0;
}

void ath12k_link_sta_hlist_delete(struct ath12k *ar,
				  struct ath12k_link_sta *arsta)
{
	lockdep_assert_held(&ar->arsta_lock);

	if (!hlist_unhashed(&arsta->hlist_addr))
		hlist_del_init(&arsta->hlist_addr);

	return;
}

int ath12k_link_sta_hlist_init(struct ath12k *ar)
{
	u32 sta_max, buckets;

	lockdep_assert_held(&ar->arsta_lock);

	if (ar->arsta_list) {
		__hash_init(ar->arsta_list, BIT(ar->arsta_hash_bits));
		return 0;
	}

	sta_max = ath12k_core_get_max_station_per_radio(ar->ab);

	ar->arsta_hash_bits = order_base_2(sta_max);
	if (!ar->arsta_hash_bits) {
		ath12k_warn(ar->ab, "sta_max=%u too small, using default\n",
			    sta_max);
		ar->arsta_hash_bits =
			order_base_2(ATH12K_CP_PEER_HASHTABLE_DEFAULT_ENTRIES);
	}

	buckets = BIT(ar->arsta_hash_bits);
	ar->arsta_list = kcalloc(buckets, sizeof(*ar->arsta_list), GFP_ATOMIC);
	if (!ar->arsta_list)
		return -ENOMEM;

	__hash_init(ar->arsta_list, buckets);

	return 0;
}

void ath12k_link_sta_hlist_head_destroy(struct ath12k *ar)
{
	lockdep_assert_held(&ar->arsta_lock);

	kfree(ar->arsta_list);
	ar->arsta_list = NULL;
	ar->arsta_hash_bits = 0;
}

void ath12k_link_sta_hlist_destroy(struct ath12k *ar)
{
	struct ath12k_link_sta *arsta;
	struct hlist_node *tmp;
	u32 bkt;
	u8 ar_bmp = BIT(ar->radio_idx);

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return;

	for (bkt = 0; bkt < BIT(ar->arsta_hash_bits); bkt++) {
		hlist_for_each_entry_safe(arsta, tmp, &ar->arsta_list[bkt], hlist_addr) {
			if (!hlist_unhashed(&arsta->hlist_addr)) {
				arsta->ahsta->ar_bitmap &= ~ar_bmp;
				hash_del(&arsta->hlist_addr);
			}
		}
	}
}

bool ath12k_link_sta_hlist_empty(struct ath12k *ar)
{
	lockdep_assert_held(&ar->arsta_lock);

	return !ar->arsta_list ||
	       __hash_empty(ar->arsta_list, BIT(ar->arsta_hash_bits));
}

struct ath12k_link_sta *ath12k_link_sta_find_by_addr(struct ath12k *ar,
						     const u8 *addr)
{
	struct ath12k_link_sta *arsta;
	struct hlist_head *bucket;

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return NULL;

	bucket = &ar->arsta_list[ath12k_link_sta_hash_idx(ar, addr)];
	hlist_for_each_entry(arsta, bucket, hlist_addr) {
		if (ether_addr_equal(arsta->addr, addr))
			return arsta;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_link_sta_find_by_addr);

struct ath12k_link_sta *ath12k_link_sta_find_by_addr_vdev_id(struct ath12k *ar,
							     const u8 *addr,
							     u32 vdev_id)
{
	struct ath12k_link_sta *arsta;
	struct hlist_head *bucket;

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return NULL;

	bucket = &ar->arsta_list[ath12k_link_sta_hash_idx(ar, addr)];
	hlist_for_each_entry(arsta, bucket, hlist_addr) {
		if (ether_addr_equal(arsta->addr, addr) &&
		    arsta->arvif->vdev_id == vdev_id)
			return arsta;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_link_sta_find_by_addr_vdev_id);

/**
 * ath12k_arsta_itr_on_ar_by_vdev_id - iterate over all arsta entries on a
 *                                     radio matching @vdev_id and invoke @cb
 * @ar:     radio whose arsta hash table is searched
 * @vdev_id: vdev identifier to match against arsta->arvif->vdev_id
 * @cb:     callback invoked for every matching arsta;
 *          called with ar->arsta_lock held (BH-disabled). Return 0 to
 *          continue iterating, negative errno to stop and propagate.
 * @data:   opaque context forwarded verbatim to @cb
 *
 * The caller must hold ar->arsta_lock.  The caller must not sleep inside @cb.
 *
 * Returns 0 if all matching arsta entries were visited successfully,
 * -ENODEV if the hash table is not initialised, or the first negative
 * errno returned by @cb.
 */
int ath12k_arsta_itr_on_ar_by_vdev_id(struct ath12k *ar, u32 vdev_id,
				      ath12k_arsta_vdev_iter_cb cb, void *data)
{
	struct ath12k_link_sta *arsta;
	struct hlist_node *tmp;
	u32 bkt;
	int ret;

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return -ENODEV;

	for (bkt = 0; bkt < BIT(ar->arsta_hash_bits); bkt++) {
		hlist_for_each_entry_safe(arsta, tmp, &ar->arsta_list[bkt],
					  hlist_addr) {
			if (!arsta->arvif ||
			    arsta->arvif->vdev_id != vdev_id)
				continue;

			ret = cb(ar, arsta, data);
			if (ret)
				return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_arsta_itr_on_ar_by_vdev_id);

/**
 * ath12k_arsta_itr_on_ab_by_addr - iterate over all radios to find and act on
 *                                  a link STA matching @addr
 * @ab:   base device whose radios are searched
 * @addr: link MAC address to look up
 * @cb:   callback invoked for the first matching arsta on each radio;
 *        called with ar->arsta_lock held (BH-disabled).
 * @data: opaque context forwarded verbatim to @cb
 *
 * Acquires rcu_read_lock() for the duration of the walk.  The caller must
 * not sleep inside @cb.
 *
 * Returns %true if a matching arsta was found and @cb was invoked,
 * %false otherwise.
 */
bool ath12k_arsta_itr_on_ab_by_addr(struct ath12k_base *ab, const u8 *addr,
				    ath12k_arsta_iter_cb cb, void *data)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	struct ath12k_link_sta *arsta;
	int i;

	rcu_read_lock();
	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (!pdev || !pdev->ar)
			continue;

		ar = pdev->ar;

		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, addr);
		if (!arsta) {
			spin_unlock_bh(&ar->arsta_lock);
			continue;
		}

		cb(ar, arsta, data);
		spin_unlock_bh(&ar->arsta_lock);
		rcu_read_unlock();
		return true;
	}
	rcu_read_unlock();

	return false;
}
EXPORT_SYMBOL(ath12k_arsta_itr_on_ab_by_addr);

/**
 * ath12k_arsta_itr_on_ab_by_addr_bh - iterate over all radios to find and act
 *                                     on a link STA matching @addr from
 *                                     BH-disabled context
 * @ab:   base device whose radios are searched
 * @addr: link MAC address to look up
 * @cb:   callback invoked for the first matching arsta on each radio;
 *        called with ar->arsta_lock held.
 * @data: opaque context forwarded verbatim to @cb
 *
 * Acquires rcu_read_lock() for the duration of the walk. The caller must
 * not sleep inside @cb.
 *
 * Returns %true if a matching arsta was found and @cb was invoked,
 * %false otherwise.
 */
bool ath12k_arsta_itr_on_ab_by_addr_bh(struct ath12k_base *ab, const u8 *addr,
				       ath12k_arsta_iter_cb cb, void *data)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	struct ath12k_link_sta *arsta;
	int i;

	rcu_read_lock();
	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (!pdev || !pdev->ar)
			continue;

		ar = pdev->ar;

		spin_lock(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, addr);
		if (!arsta) {
			spin_unlock(&ar->arsta_lock);
			continue;
		}

		cb(ar, arsta, data);
		spin_unlock(&ar->arsta_lock);
		rcu_read_unlock();
		return true;
	}
	rcu_read_unlock();

	return false;
}
EXPORT_SYMBOL(ath12k_arsta_itr_on_ab_by_addr_bh);

/* ahsta (ath12k_sta) group-level hashtable */
static u32 ath12k_sta_hash_idx(struct ath12k_hw_group *ag, const u8 *addr)
{
	return hash_min(ath12k_htlist_hash_key(addr), ag->ahsta_hash_bits);
}

int ath12k_sta_hlist_add(struct ath12k_hw_group *ag,
			 struct ath12k_sta *ahsta)
{
	struct ath12k_sta *tmp;
	struct hlist_head *bucket;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return -EINVAL;

	bucket = &ag->ahsta_list[ath12k_sta_hash_idx(ag, ahsta->addr)];
	hlist_for_each_entry(tmp, bucket, hlist_addr) {
		if (ether_addr_equal(tmp->addr, ahsta->addr))
			return -EEXIST;
	}

	if (!hlist_unhashed(&ahsta->hlist_addr)) {
		pr_warn("ath12k: ahsta %pM already in group hash list\n",
			ahsta->addr);
		return -EEXIST;
	}

	hlist_add_head(&ahsta->hlist_addr, bucket);
	return 0;
}
EXPORT_SYMBOL(ath12k_sta_hlist_add);

int ath12k_sta_hlist_delete(struct ath12k_hw_group *ag,
			    struct ath12k_sta *ahsta)
{
	lockdep_assert_held(&ag->ahsta_lock);

	if (!hlist_unhashed(&ahsta->hlist_addr))
		hlist_del_init(&ahsta->hlist_addr);

	return 0;
}
EXPORT_SYMBOL(ath12k_sta_hlist_delete);

int ath12k_sta_hlist_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	u32 sta_max = 0, buckets;
	int i;

	lockdep_assert_held(&ag->ahsta_lock);

	if (ag->ahsta_list) {
		__hash_init(ag->ahsta_list, BIT(ag->ahsta_hash_bits));
		return 0;
	}

	if (!ag->num_devices || !ag->num_hw)
		return -EINVAL;

	/* Size the table to cover all stations across every device in the group */
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (ab)
			sta_max += ath12k_core_get_max_station_per_radio(ab) *
					ab->num_radios;
	}

	if (!sta_max)
		sta_max = ATH12K_CP_PEER_HASHTABLE_DEFAULT_ENTRIES * ag->num_hw;

	ag->ahsta_hash_bits = order_base_2(sta_max);
	if (!ag->ahsta_hash_bits) {
		ath12k_hw_warn(ag->ah[0], "sta_max=%u too small, using default\n",
			       sta_max);
		ag->ahsta_hash_bits =
			order_base_2(ATH12K_CP_PEER_HASHTABLE_DEFAULT_ENTRIES *
				     ag->num_hw);
	}

	buckets = BIT(ag->ahsta_hash_bits);
	ag->ahsta_list = kcalloc(buckets, sizeof(*ag->ahsta_list), GFP_ATOMIC);
	if (!ag->ahsta_list)
		return -ENOMEM;

	__hash_init(ag->ahsta_list, buckets);
	return 0;
}
EXPORT_SYMBOL(ath12k_sta_hlist_init);

void ath12k_sta_hlist_head_destroy(struct ath12k_hw_group *ag)
{
	lockdep_assert_held(&ag->ahsta_lock);

	kfree(ag->ahsta_list);
	ag->ahsta_list = NULL;
	ag->ahsta_hash_bits = 0;
}
EXPORT_SYMBOL(ath12k_sta_hlist_head_destroy);

void ath12k_sta_hlist_destroy(struct ath12k_hw_group *ag)
{
	struct ath12k_sta *ahsta;
	struct hlist_node *tmp;
	u32 bkt;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return;

	for (bkt = 0; bkt < BIT(ag->ahsta_hash_bits); bkt++) {
		hlist_for_each_entry_safe(ahsta, tmp,
					  &ag->ahsta_list[bkt], hlist_addr)
			hash_del(&ahsta->hlist_addr);
	}
}
EXPORT_SYMBOL(ath12k_sta_hlist_destroy);

void ath12k_sta_hlist_destroy_with_no_ar(struct ath12k_hw_group *ag)
{
	struct ath12k_sta *ahsta;
	struct hlist_node *tmp;
	u32 bkt;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return;

	for (bkt = 0; bkt < BIT(ag->ahsta_hash_bits); bkt++) {
		hlist_for_each_entry_safe(ahsta, tmp,
					  &ag->ahsta_list[bkt], hlist_addr) {
			if (!ahsta->ar_bitmap)
				hlist_del_init(&ahsta->hlist_addr);
		}
	}
}
EXPORT_SYMBOL(ath12k_sta_hlist_destroy_with_no_ar);

struct ath12k_sta *ath12k_sta_find_by_addr(struct ath12k_hw_group *ag,
					   const u8 *addr)
{
	struct ath12k_sta *ahsta;
	struct hlist_head *bucket;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return NULL;

	bucket = &ag->ahsta_list[ath12k_sta_hash_idx(ag, addr)];
	hlist_for_each_entry(ahsta, bucket, hlist_addr) {
		if (ether_addr_equal(ahsta->addr, addr))
			return ahsta;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_sta_find_by_addr);

struct ath12k_sta *ath12k_sta_find_by_addr_and_ahvif(struct ath12k_hw_group *ag,
						     const u8 *addr,
						     const struct ath12k_vif *ahvif)
{
	struct ath12k_sta *ahsta;
	struct hlist_head *bucket;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return NULL;

	bucket = &ag->ahsta_list[ath12k_sta_hash_idx(ag, addr)];
	hlist_for_each_entry(ahsta, bucket, hlist_addr) {
		if (ether_addr_equal(ahsta->addr, addr) &&
		    ahsta->ahvif == ahvif)
			return ahsta;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_sta_find_by_addr_and_ahvif);

static bool ath12k_sta_find_duplicate(struct ath12k_hw_group *ag,
				      const u8 *addr, u8 radio_idx,
				      const struct ath12k_sta *exclude)
{
	struct ath12k_sta *ahsta;
	struct hlist_head *bucket;

	lockdep_assert_held(&ag->ahsta_lock);

	if (!ag->ahsta_list)
		return false;

	bucket = &ag->ahsta_list[ath12k_sta_hash_idx(ag, addr)];
	hlist_for_each_entry(ahsta, bucket, hlist_addr) {
		if (ether_addr_equal(ahsta->addr, addr)) {
			if (ahsta == exclude)
				continue;
			/* Actually ath12k_is_mlo_sta needs to be called instead of
			 * this mlo check. However this check is sufficient
			 */
			if (!ahsta->is_mlo)
				continue;
			if (ahsta->ar_bitmap & BIT(radio_idx))
				return true;
		}
	}
	return false;
}

/* Remote peer MAC check against all created vdev MACs on @ar.
 *
 * @ar : radio in which the duplicate detection is performed.
 * @link_mac: link-level MAC of the remote peer being created.
 * @mld_mac:  MLD MAC of the remote peer, or NULL for legacy peers.
 *
 * Checks that neither the peer link MAC nor its MLD MAC conflicts
 * with the link MAC or MLD MAC of any existing vdev on this radio.
 * For AP vdevs the bssid is already covered by self_arsta in
 * arsta_list (Phase 1); only the MLD MAC of an AP vdev is checked.
 *
 * ar->data_lock is held while iterating ar->arvifs.
 */
static int ath12k_remote_peer_vdev_mac_check(struct ath12k *ar,
					     const u8 *link_mac,
					     const u8 *mld_mac)
{
	struct ath12k_link_vif *arvif_itr;
	bool itr_is_mlo;
	const u8 *itr_mld_mac;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif_itr, &ar->arvifs, list) {
		if (!arvif_itr->is_created)
			continue;

		itr_is_mlo = ath12k_mac_is_ml_arvif(arvif_itr);
		itr_mld_mac = itr_is_mlo ? arvif_itr->ahvif->vif->addr : NULL;

		/* For AP vdevs, bssid is covered by self_arsta in arsta_list
		 * (Phase 1). Only check the MLD MAC.
		 */
		if (arvif_itr->ahvif->vdev_type != WMI_VDEV_TYPE_AP) {
			/* peer link MAC vs itr link MAC */
			if (ether_addr_equal(arvif_itr->addr, link_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[remote_peer]: peer link MAC %pM conflicts with vdev %d addr on radio %d arvif_itr->addr=%pM\n",
					    link_mac, arvif_itr->vdev_id,
					    ar->radio_idx, arvif_itr->addr);
				return -EEXIST;
			}

			/* peer MLD MAC vs itr link MAC */
			if (mld_mac &&
			    ether_addr_equal(arvif_itr->addr, mld_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[remote_peer]: peer MLD MAC %pM conflicts with vdev %d addr on radio %d\n",
					    mld_mac, arvif_itr->vdev_id,
					    ar->radio_idx);
				return -EEXIST;
			}
		}

		if (itr_is_mlo) {
			/* peer link MAC vs itr MLD MAC */
			if (ether_addr_equal(itr_mld_mac, link_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[remote_peer]: peer link MAC %pM conflicts with vdev %d MLD MAC on radio %d\n",
					    link_mac, arvif_itr->vdev_id,
					    ar->radio_idx);
				return -EEXIST;
			}

			/* peer MLD MAC vs itr MLD MAC */
			if (mld_mac &&
			    ether_addr_equal(itr_mld_mac, mld_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[remote_peer]: peer MLD MAC %pM conflicts with vdev %d MLD MAC on radio %d\n",
					    mld_mac, arvif_itr->vdev_id,
					    ar->radio_idx);
				return -EEXIST;
			}
		}
	}
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

/* Phase 3 helper: check the current vdev's own MACs against all other
 * created vdevs on @ar to detect conflicts before a peer is created.
 *
 * @ar : radio in which the duplicate detection is performed.
 * @arvif:    the link vif for which a peer is being created.
 *
 * The following arvif_itr entries are skipped during iteration:
 *   - not yet created (is_created == false)
 *   - self (arvif_itr == arvif): no conflict with own vdev
 *   - either side of a bridge/parent AP vdev pair sharing the same
 *     ahvif: both share the same MLD MAC by design
 *
 * For non-bridge AP vdevs the bssid is covered by self_arsta in
 * arsta_list and is therefore already checked in Phase 1; only the
 * MLD MAC of an AP vdev is checked here since it is never added to
 * arsta_list.
 *
 * ar->data_lock is held while iterating ar->arvifs as it guards all
 * list_add / list_del operations on that list.
 */
static int ath12k_self_peer_vdev_mac_check(struct ath12k *ar,
					   struct ath12k_link_vif *arvif)
{
	const u8 *cur_link_mac = arvif->addr;
	bool cur_is_mlo = ath12k_mac_is_ml_arvif(arvif);
	bool cur_is_bridge = ath12k_mac_is_bridge_vdev(arvif);
	const u8 *cur_mld_mac = cur_is_mlo ? arvif->ahvif->vif->addr : NULL;
	struct ath12k_link_vif *arvif_itr;
	bool itr_is_mlo;
	const u8 *itr_mld_mac;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif_itr, &ar->arvifs, list) {
		if (!arvif_itr->is_created)
			continue;

		/* Skip self */
		if (arvif_itr == arvif)
			continue;

		/* A bridge AP vdev and its parent AP vdev share the same ahvif
		 * (and thus the same MLD MAC) by design. Skip the check when
		 * either side of the pair is a bridge vdev.
		 */
		if ((cur_is_bridge ||
		     ath12k_mac_is_bridge_vdev(arvif_itr)) &&
		    arvif_itr->ahvif == arvif->ahvif)
			continue;

		itr_is_mlo = ath12k_mac_is_ml_arvif(arvif_itr);
		itr_mld_mac = itr_is_mlo ? arvif_itr->ahvif->vif->addr : NULL;

		/* For non-bridge AP vdevs, bssid is covered by self_arsta in
		 * arsta_list (Phase 1). Only check the MLD MAC.
		 */
		if (arvif_itr->ahvif->vdev_type != WMI_VDEV_TYPE_AP) {
			/* cur link MAC vs itr link MAC */
			if (ether_addr_equal(arvif_itr->addr, cur_link_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[self_peer]: vdev %d link MAC %pM conflicts with vdev %d addr on radio %d\n",
					    arvif->vdev_id, cur_link_mac,
					    arvif_itr->vdev_id, ar->radio_idx);
				return -EEXIST;
			}

			/* cur MLD MAC vs itr link MAC */
			if (cur_mld_mac &&
			    ether_addr_equal(arvif_itr->addr, cur_mld_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[self_peer]: vdev %d MLD MAC %pM conflicts with vdev %d addr on radio %d\n",
					    arvif->vdev_id, cur_mld_mac,
					    arvif_itr->vdev_id, ar->radio_idx);
				return -EEXIST;
			}
		}

		if (itr_is_mlo) {
			/* cur link MAC vs itr MLD MAC */
			if (ether_addr_equal(itr_mld_mac, cur_link_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[self_peer]: vdev %d link MAC %pM conflicts with vdev %d MLD MAC on radio %d\n",
					    arvif->vdev_id, cur_link_mac,
					    arvif_itr->vdev_id, ar->radio_idx);
				return -EEXIST;
			}

			/* cur MLD MAC vs itr MLD MAC */
			if (cur_mld_mac &&
			    ether_addr_equal(itr_mld_mac, cur_mld_mac)) {
				spin_unlock_bh(&ar->data_lock);
				ath12k_warn(ar->ab,
					    "mac_sanity[self_peer]: vdev %d MLD MAC %pM conflicts with vdev %d MLD MAC on radio %d\n",
					    arvif->vdev_id, cur_mld_mac,
					    arvif_itr->vdev_id, ar->radio_idx);
				return -EEXIST;
			}
		}
	}
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

int ath12k_cp_peer_sanity_check(struct ath12k *ar,
				struct ath12k_link_vif *arvif,
				struct ath12k_link_sta *arsta,
				struct ath12k_sta *ahsta)
{
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_link_sta *found_arsta;
	const u8 *new_link_mac = arsta->addr;
	bool is_mlo = ahsta && ahsta->is_mlo;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->arsta_lock);
	/* New legacy/MLO-link/self peer MAC vs existing link MAC of self/remote peer */
	found_arsta = ath12k_link_sta_find_by_addr(ar, new_link_mac);
	if (found_arsta) {
		ath12k_warn(ar->ab,
			    "mac_sanity: peer %pM already in arsta hash on radio %d vdev %d (self=%d)\n",
			    new_link_mac, ar->radio_idx,
			    found_arsta->arvif->vdev_id,
			    found_arsta->is_self_peer);
		spin_unlock_bh(&ar->arsta_lock);
		return -EEXIST;
	}

	/* New MLO peer's MLD MAC vs existing Link MAC of self/remote link peer */
	if (is_mlo && !(ether_addr_equal(ahsta->addr, new_link_mac))) {
		found_arsta = ath12k_link_sta_find_by_addr(ar, ahsta->addr);
		if (found_arsta) {
			ath12k_warn(ar->ab,
				    "mac_sanity: MLD MAC %pM already in arsta hash on radio %d vdev %d (self=%d)\n",
				    ahsta->addr, ar->radio_idx,
				    found_arsta->arvif->vdev_id,
				    found_arsta->is_self_peer);
			spin_unlock_bh(&ar->arsta_lock);
			return -EEXIST;
		}
	}
	spin_unlock_bh(&ar->arsta_lock);

	spin_lock_bh(&ag->ahsta_lock);
	/* New legacy/MLO-link/self peer MAC vs existing MLO peer MLD MAC
	 * in this radio.
	 * ahsta should be sent NULL for self peer.
	 */
	if (ath12k_sta_find_duplicate(ag, new_link_mac, ar->radio_idx, ahsta)) {
		spin_unlock_bh(&ag->ahsta_lock);
		ath12k_warn(ar->ab,
			    "mac_sanity: peer link MAC %pM conflicts with existing MLO MLD MAC on radio %d\n",
			    new_link_mac, ar->radio_idx);
		return -EEXIST;
	}

	/* New MLO peer's MLD MAC vs existing MLO peer MLD MAC in this radio */
	if (is_mlo && ath12k_sta_find_duplicate(ag, ahsta->addr, ar->radio_idx, ahsta)) {
		spin_unlock_bh(&ag->ahsta_lock);
		ath12k_warn(ar->ab,
			    "mac_sanity: MLO peer MLD MAC %pM conflicts with existing MLO ahsta on radio %d\n",
			    ahsta->addr, ar->radio_idx);
		return -EEXIST;
	}
	spin_unlock_bh(&ag->ahsta_lock);

	/* Phase 3: check MACs against existing vdev MACs on this radio.
	 * For self-peer: check the new vdev's own MACs vs existing vdevs.
	 * For remote peer: check the peer MACs vs existing vdev MACs.
	 */
	if (arsta->is_self_peer)
		return ath12k_self_peer_vdev_mac_check(ar, arvif);

	return ath12k_remote_peer_vdev_mac_check(ar, new_link_mac,
						    is_mlo ? ahsta->addr : NULL);
}
EXPORT_SYMBOL(ath12k_cp_peer_sanity_check);

int ath12k_peer_send_assoc_vendor_response(const struct ath12k_dp_link_peer *peer,
					   bool is_assoc)
{
	struct ieee80211_link_sta *link_sta;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k *ar;

	if (!peer) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer skipped assoc vendor response\n");
		return -EINVAL;
	}

	sta = ath12k_dp_link_peer_get_sta(peer);
	if (!sta) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid sta skipped assoc vendor response\n");
		return -EINVAL;
	}

	if (peer->link_id < 0) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer link id skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_lock();
	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = ahsta->link[peer->link_id];
	if (!(arsta && arsta->arvif)) {
		rcu_read_unlock();
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "invalid arsta for peer: %pM skipped assoc vendor response\n",
			   peer->addr);
		return -EINVAL;
	}

	ar = arsta->arvif->ar;

	link_sta = ath12k_mac_get_link_sta(arsta);

	if (!link_sta) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access link sta skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_unlock();

	if (is_assoc)
		ath12k_mac_vendor_send_assoc_event(arsta, link_sta, true);
	else
		ath12k_mac_vendor_send_disassoc_event(arsta, link_sta);

	return 0;
}

/**
 * ath12k_link_sta_find_by_vdev_id() - find the first arsta matching @vdev_id
 * @ar:      radio whose arsta hash table is searched
 * @vdev_id: vdev identifier to match against arsta->arvif->vdev_id
 *
 * Walks all buckets of the arsta hash table and returns the first
 * ath12k_link_sta whose associated arvif carries @vdev_id.
 *
 * Context: Caller must hold ar->arsta_lock.
 * Return: pointer to the matching ath12k_link_sta, or NULL if not found.
 */
struct ath12k_link_sta *ath12k_link_sta_find_by_vdev_id(struct ath12k *ar,
							u32 vdev_id)
{
	struct ath12k_link_sta *arsta;
	u32 bkt;

	lockdep_assert_held(&ar->arsta_lock);

	if (!ar->arsta_list)
		return NULL;

	for (bkt = 0; bkt < BIT(ar->arsta_hash_bits); bkt++) {
		hlist_for_each_entry(arsta, &ar->arsta_list[bkt], hlist_addr) {
			if (arsta->arvif &&
			    arsta->arvif->vdev_id == vdev_id)
				return arsta;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_link_sta_find_by_vdev_id);
