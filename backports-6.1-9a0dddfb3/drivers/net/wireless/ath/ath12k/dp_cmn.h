/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_CMN_H
#define ATH12K_DP_CMN_H

#include "cmn_defs.h"

struct ath12k_hw_group;
struct ath12k;

struct ath12k_dp_hw_link {
	u8 device_id;
	u8 pdev_idx;
};

struct ath12k_dp_hw_group {
	struct ath12k_dp_hw_link hw_links[ATH12K_GROUP_MAX_RADIO];
	struct ath12k_dp *dp[ATH12K_MAX_SOCS];
};

void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp);
int ath12k_dp_cmn_device_init(struct ath12k_dp *dp);
void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag);
void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag);
void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar);

#endif
