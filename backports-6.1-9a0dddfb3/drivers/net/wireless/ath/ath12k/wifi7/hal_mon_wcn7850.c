// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "hal_mon.h"

static void
ath12k_wifi7_hal_mon_mpdu_start_info_get_wcn7850(const void *tlv_data,
						 u32 *info)
{
	struct hal_rx_mpdu_start *mpdu_start =
		(struct hal_rx_mpdu_start *)tlv_data;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = __le32_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);
	info[5] = __le32_to_cpu(mpdu_start->info5);
	info[6] = __le32_to_cpu(mpdu_start->info6);
	info[7] = __le32_to_cpu(mpdu_start->info7);
	info[8] = __le32_to_cpu(mpdu_start->info8);
}

static void
ath12k_wifi7_hal_mon_msdu_end_info_get_wcn7850(const void *tlv_data,
					       u32 *info)
{
	struct hal_rx_msdu_end *msdu_end =
		(struct hal_rx_msdu_end *)tlv_data;

	info[0] = __le32_to_cpu(msdu_end->info0);
	info[1] = __le32_to_cpu(msdu_end->info1);
	info[2] = __le32_to_cpu(msdu_end->info2);
}

static void
ath12k_wifi7_hal_mon_ppdu_eu_stats_info_get_wcn7850(const void *tlv_data,
						    u32 *info)
{
	struct hal_rx_ppdu_end_user_stats *ppdu_eu_stats =
		(struct hal_rx_ppdu_end_user_stats *)tlv_data;

	info[0] = __le32_to_cpu(ppdu_eu_stats->info0);
	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);
	info[6] = __le32_to_cpu(ppdu_eu_stats->info6);
	info[7] = __le32_to_cpu(ppdu_eu_stats->info7);
	info[8] = __le32_to_cpu(ppdu_eu_stats->info8);
	info[9] = __le32_to_cpu(ppdu_eu_stats->info9);
	info[10] = __le32_to_cpu(ppdu_eu_stats->info10);
	info[11] = __le32_to_cpu(ppdu_eu_stats->info11);
	info[12] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
	info[13] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);
}

const struct hal_mon_ops hal_wcn7850_mon_ops = {
	.rx_mpdu_start_info_get = ath12k_wifi7_hal_mon_mpdu_start_info_get_wcn7850,
	.rx_msdu_end_info_get = ath12k_wifi7_hal_mon_msdu_end_info_get_wcn7850,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_ppdu_eu_stats_info_get_wcn7850,
};
