// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "hal_mon.h"

static __always_inline void
ath12k_wifi8_hal_mon_set_mon_buf_desc(void *desc, u32 addr_lo,
				      u32 addr_hi, u64 cookie)
{
	struct hal_mon_buf_ring *mon_buf_desc = (struct hal_mon_buf_ring *)desc;

	mon_buf_desc->paddr_lo = addr_lo;
	mon_buf_desc->paddr_hi = addr_hi;
	mon_buf_desc->cookie = cookie;
}

const struct hal_mon_ops hal_qcn9625_mon_ops = {
	.get_mon_mpdu_start_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi8_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi8_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi8_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_parse,
	.rx_desc_get_msdu_payload =
		ath12k_wifi8_hal_mon_rx_desc_get_msdu_payload,
	.hal_mon_set_mon_buf_desc = ath12k_wifi8_hal_mon_set_mon_buf_desc,
};
