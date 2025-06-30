// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "hal_mon.h"

const struct hal_mon_ops hal_qcn9274_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_get_compact,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_get_compact,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact,
};

const struct hal_mon_ops hal_ipq5332_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_get_compact,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_get_compact,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact,
};

const struct hal_mon_ops hal_ipq5424_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_get_compact,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_get_compact,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact,
};

const struct hal_mon_ops hal_qcn6432_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_get_compact,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_get_compact,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact,
};
