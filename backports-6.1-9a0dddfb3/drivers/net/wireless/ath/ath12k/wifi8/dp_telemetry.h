/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TELEMETRY_H
#define ATH12K_DP_TELEMETRY_H

#include "../core.h"

int ath12k_wifi8_dp_telemetry_ring_setup(struct ath12k_base *ab);
int ath12k_wifi8_dp_telemetry_ring_cleanup(struct ath12k_base *ab);
int ath12k_wifi8_dp_process_tx_peer_telemetry(struct ath12k_dp *dp);
int ath12k_wifi8_dp_process_rx_peer_telemetry(struct ath12k_dp *dp);

#endif
