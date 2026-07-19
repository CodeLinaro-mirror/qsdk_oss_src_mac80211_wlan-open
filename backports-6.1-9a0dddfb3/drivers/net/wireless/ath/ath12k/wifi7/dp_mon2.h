/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DPMON2_H
#define ATH12K_DPMON2_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

struct workqueue_struct;

int ath12k_dp_mon_rx_dual_ring_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_dual_ring_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_rx_wq_init(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_wq_deinit(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi7_dp_ext_mon_validate_request(struct ath12k_pdev_dp *dp_pdev,
					     const struct ath12k_ext_mon_config *req);
int ath12k_wifi7_dp_ext_mon_filter(struct sk_buff *mpdu,
				   struct ath12k_dp_tx_ext_mon_config *tx_ext_mon);
int ath12k_wifi7_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					 bool enable);
int
ath12k_wifi7_dp_ext_mon_add_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_ext_mon_tx_peer_params *peer_param);
int
ath12k_wifi7_dp_ext_mon_remove_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_ext_mon_tx_peer_params *peer_param);

#endif
