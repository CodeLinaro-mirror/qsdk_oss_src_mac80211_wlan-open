/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

int ath12k_wifi7_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_wifi7_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_wifi7_dp_mon_rx_buf_setup(struct ath12k_dp *dp);
void ath12k_wifi7_dp_mon_rx_buf_free(struct ath12k_dp *dp);
int ath12k_wifi7_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
