/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ATH12K_TELEMETRY_AGENT_IF_H
#define ATH12K_TELEMETRY_AGENT_IF_H

#include "telemetry.h"
#include "telemetry_agent_wifi_driver_if.h"

u32 ath12k_telemetry_agent_init(void);
u32 ath12k_telemetry_agent_deinit(void);
int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);
int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);

int ath12k_get_pdev_stats(void *obj, struct agent_link_iface_stats_obj *stats);
int ath12k_get_peer_info(void *obj, struct agent_peer_iface_init_obj *stats);
int ath12k_get_pdev_info(void *obj, struct agent_pdev_iface_init_obj *stats);
int ath12k_get_peer_stats(void *obj, struct agent_peer_iface_stats_obj *stats);
int ath12k_get_psoc_info(void *obj, struct agent_psoc_iface_init_obj *statis);

int ath12k_telemetry_set_mov_avg_params(u32 num_pkt, u32 num_win);
int ath12k_telemetry_set_sla_params(u32 num_pkt, u32 time_sec);
int ath12k_telemetry_set_sla_cfg(struct ath12k_sla_thershold_cfg param);
int ath12k_telemetry_set_sla_detect_cfg(struct ath12k_sla_detect_cfg param);
#endif /* ATH12K_TELEMETRY_AGENT_IF_H */
