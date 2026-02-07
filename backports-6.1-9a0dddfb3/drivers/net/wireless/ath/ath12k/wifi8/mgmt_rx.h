/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_MGMT_RX_WIFI8_H
#define ATH12K_MGMT_RX_WIFI8_H

#include "hw.h"
#include "../mgmt_rx.h"

struct ath12k_base;

struct ath12k_mgmt_wifi8 {
	struct mgmt_srng reo_dst_rx_ring;
	struct mgmt_srng reo_dst_rx_err_ring;
	struct mgmt_srng wbm_refill_ring;
	struct mgmt_srng wbm_idle_buf_ring;
};

struct ath12k_mgmt *ath12k_wifi8_mgmt_init(struct ath12k_base *ab);
void ath12k_wifi8_mgmt_deinit(struct ath12k_mgmt *mgmt);

int ath12k_wifi8_mgmt_wbm_ring_sel_config_qcn9625(struct ath12k_base *ab);

static inline struct ath12k_mgmt_wifi8 *ath12k_get_mgmt_wifi8(struct ath12k_mgmt *mgmt)
{
	return (struct ath12k_mgmt_wifi8 *)mgmt->arch_priv;
}

static inline
void ath12k_wifi8_mgmt_extract_rx_desc_data(struct ath12k_mgmt *mgmt,
					    struct hal_rx_desc_data *rx_desc_data,
					    struct hal_rx_desc *rx_desc,
					    struct hal_rx_desc *ldesc)
{
	mgmt->hw_params->hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

#endif
