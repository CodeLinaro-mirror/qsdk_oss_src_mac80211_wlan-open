/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_MON1_H
#define ATH12K_DP_TX_MON1_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

struct workqueue_struct;

/* ext_mon peer filtering behavior */
enum ath12k_dp_tx_ext_mon_peer_filter {
	ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER =	1,
	ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER,
	ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER,
	ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER_MAX,
};

int ath12k_wifi7_dp_ext_mon_filter(struct sk_buff *mpdu,
				   struct ath12k_dp_tx_ext_mon_config *tx_ext_mon);
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
ath12k_wifi7_dp_mon_tx_get_spc_bitmap(struct ath12k_base *ab);
#endif
