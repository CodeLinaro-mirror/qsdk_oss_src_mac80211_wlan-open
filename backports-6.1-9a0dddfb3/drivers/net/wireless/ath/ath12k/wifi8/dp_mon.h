/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../dp_mon.h"

extern const struct ath12k_dp_arch_mon_ops ath12k_wifi8_dp_arch_mon_dual_ring_ops;

static inline
void ath12k_wifi8_dp_mon_ops_register(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	if (ath12k_dp_get_mon_type(dp) == ATH12K_DP_MON_TYPE_DUAL_RING)
		dp_mon->mon_ops = &ath12k_wifi8_dp_arch_mon_dual_ring_ops;
}
