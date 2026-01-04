/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI8_H
#define ATH12K_DP_WIFI8_H

#include "../core.h"
#include "../dp_cmn.h"
#include "hw.h"
#include "dp_ast.h"

#define DP_TX_EXCEPTION_RING_SIZE      512

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp_wifi8 {
	struct ath12k_dp *dp;
	bool cumac;
	struct dp_srng tx_exception;
	struct dp_srng tcl_cmd_ring;
	struct dp_srng tcl_status_ring;
	struct dp_srng reo_dst_high_prio_ring;
};

struct ath12k_dp_hw_group_wifi8 {
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp *cumac_dp;
	struct ath12k_dp_global_ast_table dp_ast_base;
	struct ath12k_pn_page_info *pn_page_info;
	u8 num_pn_pages;
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

static inline struct ath12k_base *
		ath12k_dp_get_ab_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->ab;
}

static inline struct device *
		ath12k_dp_get_dev_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->dev;
}

struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp);
#endif
