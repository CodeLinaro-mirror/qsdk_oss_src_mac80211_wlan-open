/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/export.h>
#include "debug.h"
#include "telemetry.h"
#include "telemetry_agent_if.h"
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
