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
void ath12k_dp_mon_rx_wq_deinit(struct ath12k_pdev_dp *dp_pdev, bool destroy);
int ath12k_wifi7_dp_ext_mon_validate_request(struct ath12k_pdev_dp *dp_pdev,
					     const struct ath12k_ext_mon_config *req);
void
ath12k_wifi7_dp_ext_mon_setup_rx_filter(struct htt_rx_ring_tlv_filter *tlv_filter,
					const struct ath12k_dp_rx_ext_mon *rx_ext_mon);
struct ath12k_ext_mon_peer_info;
int ath12k_wifi7_ext_mon_handle_rx_peer(struct ath12k_pdev_dp *dp_pdev,
					const struct ath12k_ext_mon_peer_info *peer_info,
					enum ath12k_ext_mon_peer_action action,
					u32 vdev_id, struct list_head *peers_to_wmi,
					struct ath12k_ext_mon_wmi_entry *entry,
					struct ath12k_dp_ext_mon_peer *peer);
#endif
