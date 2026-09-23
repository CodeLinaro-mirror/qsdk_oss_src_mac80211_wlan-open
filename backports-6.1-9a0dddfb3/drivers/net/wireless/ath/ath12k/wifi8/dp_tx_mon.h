/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_DP_TX_MON_H
#define ATH12K_WIFI8_DP_TX_MON_H

u8 ath12k_wifi8_get_ext_mon_peer_filter_mode_locked(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi8_dp_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu);
void
ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask(struct htt_tx_mon_ring_selection_cfg_cmd
						*cmd, const struct htt_tx_ring_tlv_filter
						*htt_tlv_filter);
int ath12k_wifi8_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev, bool enable);
int
ath12k_wifi8_dp_ext_mon_add_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_ext_mon_tx_peer_params
					 *peer_param);
int
ath12k_wifi8_dp_ext_mon_remove_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					    struct ath12k_dp_ext_mon_tx_peer_params
					    *peer_param);

#endif
