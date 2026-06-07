/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DPMON2_H
#define ATH12K_DPMON2_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

struct workqueue_struct;

int ath12k_wifi8_dp_mon_rx_dual_ring_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi8_dp_mon_rx_dual_ring_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi8_dp_mon_rx_wq_init(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi8_dp_mon_rx_wq_deinit(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi8_dp_ext_mon_validate_request(struct ath12k_pdev_dp *dp_pdev,
					     const struct ath12k_ext_mon_config *req);
#endif
