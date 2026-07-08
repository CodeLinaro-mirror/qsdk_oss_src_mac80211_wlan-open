/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_DP_TX_MON_H
#define ATH12K_WIFI8_DP_TX_MON_H

void
ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask(struct htt_tx_mon_ring_selection_cfg_cmd
						*cmd, const struct htt_tx_ring_tlv_filter
						*htt_tlv_filter);
int ath12k_wifi8_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev, bool enable);

#endif
