/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "core.h"
#include "debug.h"
#include "vendor.h"
#include "telemetry.h"
#include <linux/module.h>
#include "telemetry_agent_if.h"
#include "telemetry_agent_wifi_driver_if.h"

#define MIN_THERSHOLD_PERCENTAGE 0
#define MAX_THERSHOLD_PERCENTAGE 100

static struct ath12k_telemetry_ctx *telemetry_ctx;

void ath12k_telemetry_init(struct ath12k_base *ab)
{
	if (telemetry_ctx)
		return;

	telemetry_ctx = kzalloc(sizeof(*telemetry_ctx), GFP_KERNEL);
	if (!telemetry_ctx) {
		ath12k_err(NULL, "telemetry context failed to initialize\n");
		return;
	}
	ath12k_info(NULL, "telemetry context initialized\n");
}

void ath12k_telemetry_deinit(struct ath12k_base *ab)
{
	if (!telemetry_ctx)
		return;

	kfree(telemetry_ctx);
	telemetry_ctx = NULL;
	ath12k_info(NULL, "telemetry context freed\n");
}

int ath12k_telemetry_sdwf_sla_samples_config(struct ath12k_sla_samples_cfg param)
{
	struct ath12k_sla_samples_cfg *telemetry_param = NULL;
	int ret;

	if (param.moving_avg_pkt == 0 || param.moving_avg_win == 0 ||
	    param.sla_num_pkt == 0 || param.sla_time_sec == 0) {
		ath12k_err(NULL, "invalid telemetry sla samples configuration\n");
		return -EINVAL;
	}

	if (telemetry_ctx)
		telemetry_param = &telemetry_ctx->sla_samples_params;

	ret = ath12k_telemetry_set_mov_avg_params(param.moving_avg_pkt,
						  param.moving_avg_win);

	if (!ret) {
		if (telemetry_param) {
			telemetry_param->moving_avg_pkt = param.moving_avg_pkt;
			telemetry_param->moving_avg_win = param.moving_avg_win;
		}
		ath12k_info(NULL, "telemetry sla samples configuration "
			    "movavgpkt:%d movavgwin:%d done\n",
			    param.moving_avg_pkt, param.moving_avg_win);
	}

	if (ret && ret != -ENOENT) {
		ath12k_err(NULL, "telemetry failed to set mov avg params ret:%d\n",
			   ret);
		return ret;
	}

	ret = ath12k_telemetry_set_sla_params(param.sla_num_pkt,
					      param.sla_time_sec);

	if (!ret) {
		if (telemetry_param) {
			telemetry_param->sla_num_pkt = param.sla_num_pkt;
			telemetry_param->sla_time_sec = param.sla_time_sec;
		}
		ath12k_info(NULL, "telemetry sla samples configuration "
			    "slanumpkt:%d slatimesec:%d done\n",
			    param.sla_num_pkt, param.sla_time_sec);
	}

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla params ret:%d\n",
		   ret);
	return ret;
}

int ath12k_telemetry_sdwf_sla_thershold_config(struct ath12k_sla_thershold_cfg param)
{
	int ret;

	if (param.min_throughput_rate < MIN_THERSHOLD_PERCENTAGE ||
	    param.min_throughput_rate > MAX_THERSHOLD_PERCENTAGE ||
	    param.max_throughput_rate < MIN_THERSHOLD_PERCENTAGE ||
	    param.max_throughput_rate > MAX_THERSHOLD_PERCENTAGE ||
	    param.burst_size < MIN_THERSHOLD_PERCENTAGE ||
	    param.burst_size > MAX_THERSHOLD_PERCENTAGE ||
	    param.service_interval < MIN_THERSHOLD_PERCENTAGE ||
	    param.service_interval > MAX_THERSHOLD_PERCENTAGE ||
	    param.delay_bound < MIN_THERSHOLD_PERCENTAGE ||
	    param.delay_bound > MAX_THERSHOLD_PERCENTAGE ||
	    param.msdu_ttl < MIN_THERSHOLD_PERCENTAGE ||
	    param.msdu_ttl > MAX_THERSHOLD_PERCENTAGE ||
	    param.msdu_rate_loss < MIN_THERSHOLD_PERCENTAGE ||
	    param.msdu_rate_loss > MAX_THERSHOLD_PERCENTAGE) {
		ath12k_err(NULL, "invalid telemetry sla thershold configuration\n");
		return -EINVAL;
	}

	ret = ath12k_telemetry_set_sla_cfg(param);

	if (!ret)
		ath12k_info(NULL, "telemetry sla thershold configuration done, "
			    "svcid: %d MinThrRate:%d MaxThrRate:%d BurstSize:%d "
			    "serviceInt:%d \n DelayBound: %d MsduTtl:%d "
			    "msdurateloss:%d\n", param.svc_id,
			    param.min_throughput_rate,
			    param.max_throughput_rate,
			    param.burst_size,
			    param.service_interval,
			    param.delay_bound,
			    param.msdu_ttl,
			    param.msdu_rate_loss);

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla threshold configs ret:%d\n",
		   ret);
	return ret;
}

int ath12k_telemetry_sdwf_sla_detection_config(struct ath12k_sla_detect_cfg param)
{
	int ret;

	if ((param.sla_detect == SLA_DETECT_NUM_PACKET) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.burst_size || param.service_interval)) {
		ath12k_info(NULL, "unsupported sla detect config for number of packets.\n");
		return -1;
	}

	if (((param.sla_detect == SLA_DETECT_PER_SECOND)) &&
	    (param.burst_size || param.service_interval || param.delay_bound ||
	     param.msdu_ttl || param.msdu_rate_loss)) {
		ath12k_info(NULL, "unsupported sla detect config for per second.\n");
		return -1;
	}

	if ((param.sla_detect == SLA_DETECT_MOV_AVG) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.burst_size || param.service_interval || param.msdu_ttl ||
	     param.msdu_rate_loss)) {
		ath12k_info(NULL, "unsupported sla detect config for moving average.\n");
		return -1;
	}

	if ((param.sla_detect == SLA_DETECT_NUM_SECOND) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.delay_bound)) {
		ath12k_info(NULL, "unsupported sla detect config for number of seconds.\n");
		return -1;
	}

	ret = ath12k_telemetry_set_sla_detect_cfg(param);

	if (!ret)
		ath12k_info(NULL, "telemetry sla detection configuration done, detect option: %d "
			    "MinThrRate:%d MaxThrRate:%d BurstSize:%d ServiceInt:%d \n DelayBound: %d "
			    "MsduTtl:%d MsduRateLoss:%d\n", param.sla_detect,
			    param.min_throughput_rate,
			    param.max_throughput_rate, param.burst_size,
			    param.service_interval, param.delay_bound,
			    param.msdu_ttl, param.msdu_rate_loss);

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla detection configs ret:%d\n",
		   ret);
	return ret;
}
