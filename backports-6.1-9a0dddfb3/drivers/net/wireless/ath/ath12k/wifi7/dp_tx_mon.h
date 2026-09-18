/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_MON1_H
#define ATH12K_DP_TX_MON1_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

struct workqueue_struct;

#define ATH12K_EXT_MON_PKT_CAP_ALL_PROTOS       0x3F

u8 ath12k_wifi7_get_ext_mon_peer_filter_mode_locked(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi7_dp_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu);
int ath12k_wifi7_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					 bool enable);
int
ath12k_wifi7_dp_ext_mon_add_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_ext_mon_tx_peer_params
					 *peer_param);
int
ath12k_wifi7_dp_ext_mon_remove_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					    struct ath12k_dp_ext_mon_tx_peer_params
					    *peer_param);
u32
ath12k_wifi7_dp_mon_tx_handle_spc_bitmap(struct ath12k_base *ab);
#endif
