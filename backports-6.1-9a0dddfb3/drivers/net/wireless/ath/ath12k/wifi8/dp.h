/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI8_H
#define ATH12K_DP_WIFI8_H

#include "../dp_cmn.h"
#include "hw.h"

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp_wifi8 {
	struct ath12k_dp *dp;
	bool cumac;
};

struct ath12k_dp_hw_group_wifi8 {
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp *cumac_dp;
};

static inline struct ath12k_dp_wifi8 *ath12k_get_dp_wifi8(struct ath12k_dp *dp)
{
	return (struct ath12k_dp_wifi8 *)dp->arch_data;
}

static inline struct ath12k_dp *ath12k_get_dp(struct ath12k_dp_wifi8 *dp_wifi8)
{
	return dp_wifi8->dp;
}

static inline struct ath12k_dp_hw_group_wifi8 *
		ath12k_get_dp_hw_group_wifi8(struct ath12k_dp_hw_group *dp_hw_grp)
{
	return (struct ath12k_dp_hw_group_wifi8 *)dp_hw_grp->arch_data;
}

static inline struct ath12k_dp_hw_group *
		ath12k_get_dp_hw_group(struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8)
{
	return dp_hw_group_wifi8->dp_hw_grp;
}

struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp);
int ath12k_wifi8_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi8_dp_pdev_free(struct ath12k_base *ab);
#endif
