/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/export.h>
#include "telemetry.h"
#include "telemetry_agent_if.h"
#include "telemetry_agent.h"
#include "debug.h"
#include "dp_peer.h"
#include "dp_mon.h"
#include "sdwf.h"
#include <linux/module.h>

struct telemetry_agent_ops *g_agent_ops;

EXPORT_SYMBOL(g_agent_ops);

int ath12k_telemetry_ab_agent_create_handler(struct ath12k_base *ab)
{
	struct agent_psoc_obj psoc_obj;

	if (!ab || !g_agent_ops ||
	    !g_agent_ops->agent_psoc_create_handler)
		return -EINVAL;

	memset(&psoc_obj, 0, sizeof(psoc_obj));

	psoc_obj.psoc_id = ath12k_get_ab_device_id(ab);
	psoc_obj.psoc_back_pointer = ab;

	g_agent_ops->agent_psoc_create_handler(ab, &psoc_obj);

	return 0;
}

int ath12k_telemetry_pdev_agent_create_handler(struct ath12k_pdev *pdev)
{
	struct agent_pdev_obj pdev_obj;
	struct ath12k_base *ab;

	if (!pdev || !g_agent_ops ||
	    !g_agent_ops->agent_pdev_create_handler)
		return -EINVAL;

	memset(&pdev_obj, 0, sizeof(pdev_obj));

	ab = ath12k_pdev_to_ab(pdev);
	pdev_obj.psoc_back_pointer = ab;
	pdev_obj.pdev_back_pointer = pdev;
	pdev_obj.psoc_id = ath12k_get_ab_device_id(ab);
	pdev_obj.pdev_id = ath12k_get_pdev_id(pdev);
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "back ptr pdev: %p (pdev: %p) id: %d (pdev id: %d)\n",
		   pdev_obj.pdev_back_pointer, pdev,
		   pdev_obj.pdev_id, pdev->pdev_id);
	g_agent_ops->agent_pdev_create_handler(pdev, &pdev_obj);

	return 0;
}

static int ath12k_telemetry_create_destroy_peer_agent(struct ath12k_base *ab,
						      struct ath12k_pdev *pdev,
						      struct ath12k_dp_link_peer *peer,
						      const bool is_create)
{
	struct agent_peer_obj peer_obj;

	if (!g_agent_ops ||
	    !g_agent_ops->agent_peer_create_handler)
		return -EINVAL;
	memset(&peer_obj, 0, sizeof(peer_obj));
	memset(&peer->peer_stats.dp_mon_stats, 0,
	       sizeof(struct ath12k_dp_mon_peer_stats));
	peer_obj.peer_back_pointer = peer;
	peer_obj.psoc_back_pointer = ab;
	peer_obj.pdev_back_pointer = pdev;

	peer_obj.psoc_id = ath12k_get_ab_device_id(ab);
	peer_obj.pdev_id = ath12k_get_pdev_id(pdev);
	ether_addr_copy(peer_obj.peer_mac_addr, peer->addr);
	peer_obj.peer_id = peer->peer_id;
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "id: %d peer: %p (ab:%p - pdev:%p) soc id: %d (pdev id: %d)\n",
		   peer_obj.peer_id,
		   peer_obj.peer_back_pointer, ab, pdev,
		   peer_obj.psoc_id, peer_obj.pdev_id);

	if (is_create)
		g_agent_ops->agent_peer_create_handler(peer, &peer_obj);
	else
		g_agent_ops->agent_peer_destroy_handler(peer, &peer_obj);

	return 0;
}

int ath12k_telemetry_ab_peer_agent_create(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev = NULL;
	struct ath12k_dp_link_peer *peer, *tmp;

	if (!ab)
		return -EINVAL;

	spin_lock_bh(&ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (!peer || !peer->vif || !peer->sta || !peer->assoc_success)
			continue;

		if (ath12k_peer_get_peer_type(peer) != NL80211_IFTYPE_AP) {
			ath12k_dbg(NULL, ATH12K_DBG_RM,
				   "Peer reference for non sta type is not supported\n");
			continue;
		}

		pdev = &ab->pdevs[peer->pdev_idx];
		ath12k_telemetry_create_destroy_peer_agent(ab, pdev, peer, true);
	}
	spin_unlock_bh(&ab->dp->dp_lock);

	return 0;
}

/* FIXME: The telemetry_agent is not loaded by default. Due to this limitation,
 * the telemetry agent must be aware of resources created before its module is loaded.
 * Therefore, this subroutine needs to be called during the RM initialization path.
 * RM initiates the handshake, and ath12k invokes the corresponding telemetry agent
 * operations to create and destroy resources within the telemetry agent module
 * based on the ath12k context.
 *
 * Initialization steps:
 * 1. Create a psoc reference in the telemetry agent (TA).
 * 2. Create a pdev reference in TA.
 * 3. Create a peer reference in TA if the peer exists and is in an associated state.
 *
 * Destruction steps:
 * 3. Destroy the peer reference in TA if the peer exists.
 * 2. Destroy the pdev reference in TA.
 * 1. Destroy the psoc reference in TA.
 *
 * This routine may become unnecessary once the above limitation is resolved.
 */
static void ath12k_telemetry_create_resources(void)
{
	struct ath12k_hw_group *ag;
	struct ath12k_pdev *pdev;
	struct ath12k_base *ab;
	int i, ret, pdev_idx;

	ag = ath12k_core_get_ag();
	if (!ag) {
		ath12k_err(NULL, "Fails to get ag, skipped to create telemetry resources\n");
		return;
	}

	mutex_lock(&ag->mutex);
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		ret = ath12k_telemetry_ab_agent_create_handler(ab);
		if (ret) {
			ath12k_err(ab,
				   "Unable to create telemetry psoc agent object: %d\n",
				    ret);
			continue;
		}

		for (pdev_idx = 0; pdev_idx < ab->num_radios; pdev_idx++) {
			pdev = &ab->pdevs[pdev_idx];
			ret = ath12k_telemetry_pdev_agent_create_handler(pdev);
			if (ret) {
				ath12k_err(ab,
					   "Unable to create telemetry pdev agent object: %d\n",
					   ret);
				continue;
			}
		}

		ret = ath12k_telemetry_ab_peer_agent_create(ab);
		if (ret)
			continue;
	}
	mutex_unlock(&ag->mutex);
}

static u32 ath12k_telemetry_agent_init(void)
{
	int status = 0;

	ath12k_telemetry_create_resources();
	ath12k_info(NULL, "telemetry agent init Done\n");
	return status;
}

int ath12k_telemetry_ab_agent_delete_handler(struct ath12k_base *ab)
{
	struct agent_psoc_obj psoc_obj;

	if (!ab || !g_agent_ops ||
	   !g_agent_ops->agent_psoc_create_handler)
		return -EINVAL;

	psoc_obj.psoc_id = ath12k_get_ab_device_id(ab);
	psoc_obj.psoc_back_pointer = ab;

	g_agent_ops->agent_psoc_destroy_handler(ab, &psoc_obj);

	return 0;
}

int ath12k_telemetry_pdev_agent_delete_handler(struct ath12k_pdev *pdev)
{
	struct agent_pdev_obj pdev_obj;
	struct ath12k_base *ab;

	if (!pdev || !g_agent_ops ||
	    !g_agent_ops->agent_pdev_create_handler)
		return -EINVAL;

	memset(&pdev_obj, 0, sizeof(pdev_obj));

	ab = ath12k_pdev_to_ab(pdev);
	pdev_obj.psoc_back_pointer = ab;
	pdev_obj.pdev_back_pointer = pdev;
	pdev_obj.psoc_id = ath12k_get_ab_device_id(ab);
	pdev_obj.pdev_id = ath12k_get_pdev_id(pdev);
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "back ptr pdev: %p (pdev: %p) id: %d (pdev id: %d)\n",
		   pdev_obj.pdev_back_pointer, pdev,
		   pdev_obj.pdev_id, pdev->pdev_id);
	g_agent_ops->agent_pdev_destroy_handler(pdev, &pdev_obj);

	return 0;
}

int ath12k_telemetry_ab_peer_agent_destroy(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev = NULL;
	struct ath12k_dp_link_peer *peer, *tmp;

	if (!ab) {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "ab is null, fails to create peer agent in TA\n");
		return -EINVAL;
	}

	spin_lock_bh(&ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (!peer || !peer->vif || !peer->sta)
			continue;

		if (ath12k_peer_get_peer_type(peer) != NL80211_IFTYPE_AP)
			continue;

		pdev = &ab->pdevs[peer->pdev_idx];
		ath12k_telemetry_create_destroy_peer_agent(ab, pdev, peer, false);
	}
	spin_unlock_bh(&ab->dp->dp_lock);

	return 0;
}

static void ath12k_telemetry_destroy_resources(void)
{
	struct ath12k_hw_group *ag;
	struct ath12k_pdev *pdev;
	struct ath12k_base *ab;
	int i, ret, pdev_idx;

	ag = ath12k_core_get_ag();
	if (!ag) {
		ath12k_err(NULL, "Fails to get ag, skipped to destroy telemetry resources, expect unknown behavior\n");
		return;
	}

	mutex_lock(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		ret = ath12k_telemetry_ab_peer_agent_destroy(ab);
		if (ret)
			continue;

		for (pdev_idx = 0; pdev_idx < ab->num_radios; pdev_idx++) {
			pdev = &ab->pdevs[pdev_idx];
			ret = ath12k_telemetry_pdev_agent_delete_handler(pdev);
			if (ret) {
				ath12k_err(ab,
					   "Unable to destroy telemetry pdev agent object: %d\n",
					   ret);
				continue;
			}
		}

		ret = ath12k_telemetry_ab_agent_delete_handler(ab);
		if (ret) {
			ath12k_err(ab,
				   "Unable to destroy telemetry psoc agent object: %d\n",
				   ret);
			continue;
		}
	}

	mutex_unlock(&ag->mutex);
}

static u32 ath12k_telemetry_agent_deinit(void)
{
	int status = 0;

	ath12k_telemetry_destroy_resources();
	ath12k_info(NULL, "telemetry agent deinit\n");
	return status;
}

int ath12k_get_psoc_info(void *obj, struct agent_psoc_iface_init_obj *stats)
{
	struct ath12k_base *ab = (struct ath12k_base *)obj;

	if (!ab || !stats)
		return -EINVAL;
	memset(stats, 0, sizeof(*stats));
	stats->soc_id = ath12k_get_ab_device_id(ab);
	stats->max_peers = ath12k_get_peer_count(ab, true);
	stats->num_peers = ath12k_get_peer_count(ab, false);

	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "Psoc info: id:%d max_peers:%d num_peer:%d\n",
		   stats->soc_id, stats->max_peers, stats->num_peers);

	return 0;
}

int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops)
{
	g_agent_ops = agent_ops;
	g_agent_ops->agent_get_psoc_info = ath12k_get_psoc_info;
	g_agent_ops->agent_get_pdev_info = ath12k_get_pdev_info;
	g_agent_ops->agent_get_peer_info = ath12k_get_peer_info;
	g_agent_ops->agent_get_pdev_stats = ath12k_get_pdev_stats;
	g_agent_ops->agent_get_peer_stats = ath12k_get_peer_stats;
	g_agent_ops->agent_get_emesh_pdev_stats = NULL;
	g_agent_ops->agent_get_emesh_peer_stats = NULL;

	g_agent_ops->sawf_get_tput_stats = ath12k_sawf_get_tput_stats;
	g_agent_ops->sawf_get_mpdu_stats = ath12k_sawf_get_mpdu_stats;
	g_agent_ops->sawf_get_drop_stats = ath12k_sawf_get_drop_stats;
	g_agent_ops->sawf_get_msduq_tx_stats = ath12k_sawf_get_msduq_tx_stats;
	g_agent_ops->sawf_notify_breach = ath12k_sawf_notify_breach;

	ath12k_info(NULL, "registered telemetry agent ops: %p", g_agent_ops);

	ath12k_telemetry_agent_init();
	ath12k_info(NULL, "Init telemetry agent resources");

	return 0;
}
EXPORT_SYMBOL(register_telemetry_agent_ops);

int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops)
{
	ath12k_telemetry_agent_deinit();
	g_agent_ops = NULL;
	ath12k_info(NULL, "unregistered telemetry agent ops: %p", g_agent_ops);
	return 0;
}
EXPORT_SYMBOL(unregister_telemetry_agent_ops);

int ath12k_get_pdev_stats(void *obj, struct agent_link_iface_stats_obj *stats)
{
	ath12k_err(NULL, "ath12k_get_pdev_stats - not implemented \n");
	return -1;
}

int ath12k_get_peer_info(void *obj, struct agent_peer_iface_init_obj *stats)
{
	ath12k_err(NULL, "ath12k_get_peer_info - not implemented \n");
	return -1;
}

int ath12k_get_pdev_info(void *obj, struct agent_pdev_iface_init_obj *stats)
{
	ath12k_err(NULL, "ath12k_get_pdev_info - not implemented \n");
	return -1;
}

int ath12k_get_peer_stats(void *obj, struct agent_peer_iface_stats_obj *stats)
{
	ath12k_err(NULL, "ath12k_get_peer_stats - not implemented \n");
	return -1;
}

int ath12k_telemetry_set_mov_avg_params(u32 num_pkt,
					u32 num_win)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_updt_delay_mvng(num_pkt, num_win);

	return -ENOENT;
}

int ath12k_telemetry_set_sla_params(u32 num_pkt,
				    u32 time_sec)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_updt_sla_params(num_pkt, time_sec);

	return -ENOENT;
}

int ath12k_telemetry_set_sla_cfg(struct ath12k_sla_thershold_cfg param)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_set_sla_config(param.svc_id,
							 param.min_throughput_rate,
							 param.max_throughput_rate,
							 param.burst_size,
							 param.service_interval,
							 param.delay_bound,
							 param.msdu_ttl,
							 param.msdu_rate_loss,
							 param.per,
							 param.mcs_min_thres,
							 param.mcs_max_thres,
							 param.retries_thres));

	return -ENOENT;
}

int ath12k_telemetry_set_sla_detect_cfg(struct ath12k_sla_detect_cfg param)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_set_sla_detect_config(param.sla_detect,
								param.min_throughput_rate,
								param.max_throughput_rate,
								param.burst_size,
								param.service_interval,
								param.delay_bound,
								param.msdu_ttl,
								param.msdu_rate_loss,
								param.per,
								param.mcs_min_thres,
								param.mcs_max_thres,
								param.retries_thres));

	return -ENOENT;
}

void *ath12k_telemetry_peer_ctx_alloc(void *peer, void *sawf_stats,
				      u8 *mac_addr,
				      u8 svc_id, u8 hostq_id)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_alloc_peer(peer, sawf_stats,
						    mac_addr,
						    svc_id,
						    hostq_id);
	return NULL;
}

void ath12k_telemetry_peer_ctx_free(void *telemetry_peer_ctx)
{
	if (g_agent_ops)
		g_agent_ops->sawf_free_peer(telemetry_peer_ctx);
}

int ath12k_telemetry_update_tid_msduq(void *telemetry_peer_ctx,
				      u8 hostq_id, u8 tid, u8 msduq_idx)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_updt_queue_info(telemetry_peer_ctx,
							 hostq_id, tid,
							 msduq_idx);
	return -ENOENT;
}

int ath12k_telemetry_set_svclass_cfg(bool enable, u8 svc_id,
				     u32 min_tput_rate,
				     u32 max_tput_rate,
				     u32 burst_size,
				     u32 svc_interval,
				     u32 delay_bound,
				     u32 msdu_ttl,
				     u32 msdu_rate_loss)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_set_svclass_cfg(enable, svc_id,
							 min_tput_rate,
							 max_tput_rate,
							 burst_size,
							 svc_interval,
							 delay_bound,
							 msdu_ttl,
							 msdu_rate_loss);

	return -ENOENT;
}

int ath12k_telemetry_update_delay(void *telemetry_ctx, u8 tid,
				  u8 queue, u64 pass,
				  u64 fail)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_push_delay(telemetry_ctx, tid,
						 queue, pass, fail);
	return -ENOENT;
}
EXPORT_SYMBOL(ath12k_telemetry_update_delay);

int ath12k_telemetry_update_delay_mvng(void *telemetry_ctx,
				       u8 tid, u8 queue,
				       u64 nwdelay_winavg,
				       u64 swdelay_winavg,
				       u64 hwdelay_winavg)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_push_delay_mvng(telemetry_ctx,
							 tid, queue,
							 nwdelay_winavg,
							 swdelay_winavg,
							 hwdelay_winavg);

	return -ENOENT;
}
EXPORT_SYMBOL(ath12k_telemetry_update_delay_mvng);

bool ath12k_telemetry_update_msdu_drop(void *telemetry_ctx,
				       u8 tid, u8 queue,
				       u64 success,
				       u64 failure_drop,
				       u64 failure_ttl)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_push_msdu_drop(telemetry_ctx, tid,
							 queue, success,
							 failure_drop,
							 failure_ttl));
	return -ENOENT;
}
EXPORT_SYMBOL(ath12k_telemetry_update_msdu_drop);

int ath12k_telemetry_reset_peer_stats(u8 *peer_mac)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_reset_peer_stats(peer_mac);

	return -ENOENT;
}

int ath12k_sawf_get_tput_stats(void *soc, void *arg, u64 *in_bytes,
			       u64 *in_cnt, u64 *tx_bytes,
			       u64 *tx_cnt, u8 tid, u8 msduq)
{
	return ath12k_telemetry_get_sawf_tx_stats_tput(soc, arg,
						       in_bytes, in_cnt,
						       tx_bytes, tx_cnt,
						       tid, msduq);
}

int ath12k_sawf_get_mpdu_stats(void *soc, void *arg, u64 *svc_int_pass,
			       u64 *svc_int_fail, u64 *burst_pass,
			       u64 *burst_fail, u8 tid, u8 msduq)
{
	return ath12k_telemetry_get_sawf_tx_stats_mpdu(soc, arg, svc_int_pass,
						       svc_int_fail, burst_pass,
						       burst_fail, tid, msduq);
}

int ath12k_sawf_get_drop_stats(void *soc, void *arg, u64 *pass,
			       u64 *drop, u64 *drop_ttl,
			       u8 tid, u8 msduq)
{
	return ath12k_telemetry_get_sawf_tx_stats_drop(soc, arg, pass, drop,
						       drop_ttl, tid, msduq);
}

int ath12k_sawf_get_msduq_tx_stats(void *soc, void *arg,
				   void *msduq_tx_stats,
				   u8 msduq)
{
	return ath12k_telemetry_get_msduq_tx_stats(soc, arg,
						   msduq_tx_stats, msduq);
}

void ath12k_sawf_notify_breach(u8 *mac_addr,
			       u8 svc_id,
			       u8 param,
			       bool set_clear,
			       u8 tid, u8 queue)
{
	ath12k_telemetry_breach_indication(mac_addr, svc_id, param, set_clear, tid);
}

int ath12k_telemetry_get_rate(void *telemetry_ctx, u8 tid,
			      u8 queue, u32 *egress_rate,
			      u32 *ingress_rate)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_pull_rate(telemetry_ctx, tid, queue,
						    egress_rate, ingress_rate));
	return -ENOENT;
}

int ath12k_telemetry_get_tx_rate(void *telemetry_ctx, u8 tid, u8 msduq,
				 u32 *min_tput, u32 *max_tput,
				 u32 *avg_tput, u32 *per,
				 u32 *retries_pct)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_pull_tx_rate(telemetry_ctx,
						       tid, msduq,
						       min_tput, max_tput,
						       avg_tput, per,
						       retries_pct));
	return -ENOENT;
}

int ath12k_telemetry_get_mov_avg(void *telemetry_ctx, u8 tid,
				 u8 queue, u32 *nwdelay_avg,
				 u32 *swdelay_avg,
				 u32 *hwdelay_avg)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_pull_mov_avg(telemetry_ctx, tid,
						   queue, nwdelay_avg,
						   swdelay_avg, hwdelay_avg));
	return -ENOENT;
}
