/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI7_H
#define ATH12K_DP_WIFI7_H

#include "../dp_cmn.h"
#include "hw.h"

#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || \
	defined(CPTCFG_ATH12K_MEM_PROFILE_512M)
#define ATH12K_RX_DESC_COUNT	8192
#elif defined(CONFIG_ATH12K_MEM_PROFILE_256M) || \
	defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
#define ATH12K_RX_DESC_COUNT	8192
#else
#define ATH12K_RX_DESC_COUNT	(ath12k_dp_ring_cfg->rx_desc_count_wifi7)
#endif

#define ATH12K_NUM_RX_SPT_PAGES_DEFAULT	(12288 / ATH12K_MAX_SPT_ENTRIES)

#define ATH12K_NUM_RX_SPT_PAGES \
	(ATH12K_RX_DESC_COUNT / ATH12K_MAX_SPT_ENTRIES)

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp *ath12k_wifi7_dp_init(struct ath12k_base *ab);
void ath12k_wifi7_dp_deinit(struct ath12k_dp *dp);
int ath12k_wifi7_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi7_dp_pdev_free(struct ath12k_base *ab);
#endif
