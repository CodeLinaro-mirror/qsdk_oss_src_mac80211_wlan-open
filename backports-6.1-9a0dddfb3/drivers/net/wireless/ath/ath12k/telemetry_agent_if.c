// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/export.h>
#include <ath/ath_dp_accel_cfg.h>
#include <ath/ath_sawf.h>

struct telemetry_agent_ops *g_agent_ops;

EXPORT_SYMBOL(g_agent_ops);

int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops) {
	return 0;
}
EXPORT_SYMBOL(register_telemetry_agent_ops);

int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops){
	return 0;
}
EXPORT_SYMBOL(unregister_telemetry_agent_ops);

u32 ath_get_metadata_info(struct ath_dp_metadata_param *dp_metadata_param)
{
	        return false;
}
EXPORT_SYMBOL(ath_get_metadata_info);

void ath_sawf_uplink(struct ath_ul_params *ecm_ath_ul_params)
{
	return;
}
EXPORT_SYMBOL(ath_sawf_uplink);
