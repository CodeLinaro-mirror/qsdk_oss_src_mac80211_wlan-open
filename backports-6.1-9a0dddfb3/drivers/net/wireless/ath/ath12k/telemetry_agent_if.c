/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/export.h>
#include "debug.h"
#include "telemetry.h"
#include "telemetry_agent_if.h"
#include "sdwf.h"
#include <linux/module.h>

struct telemetry_agent_ops *g_agent_ops;

EXPORT_SYMBOL(g_agent_ops);

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

	return 0;
}
EXPORT_SYMBOL(register_telemetry_agent_ops);

int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops)
{
	g_agent_ops = NULL;
	ath12k_info(NULL, "unregistered telemetry agent ops: %p", g_agent_ops);
	return 0;
}
EXPORT_SYMBOL(unregister_telemetry_agent_ops);

u32 ath12k_telemetry_agent_init(void)
{
	int status = 0;

	/* TODO */
	ath12k_info(NULL, "telemetry agent init Done\n");
	return status;
}

u32 ath12k_telemetry_agent_deinit(void)
{
	int status = 0;

	/* TODO */
	ath12k_info(NULL, "telemetry agent deinit\n");
	return status;
}

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

int ath12k_get_psoc_info(void *obj, struct agent_psoc_iface_init_obj *stats)
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
		return (g_agent_ops->sawf_set_sla_cfg(param.svc_id,
						      param.min_throughput_rate,
						      param.max_throughput_rate,
						      param.burst_size,
						      param.service_interval,
						      param.delay_bound,
						      param.msdu_ttl,
						      param.msdu_rate_loss));

	return -ENOENT;
}

int ath12k_telemetry_set_sla_detect_cfg(struct ath12k_sla_detect_cfg param)
{
	if (g_agent_ops)
		return (g_agent_ops->sawf_set_sla_dtct_cfg(param.sla_detect,
							   param.min_throughput_rate,
							   param.max_throughput_rate,
							   param.burst_size,
							   param.service_interval,
							   param.delay_bound,
							   param.msdu_ttl,
							   param.msdu_rate_loss));

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
