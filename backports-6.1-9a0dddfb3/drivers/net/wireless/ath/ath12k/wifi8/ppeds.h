/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_PPE_H
#define ATH12K_WIFI8_PPE_H

#define PPEDS_CLASSIFY_READ_FULL_PKT 3

extern struct ppe_ds_wlan_ops_v2 ppeds_wlanops_v2;
extern struct ath12k_ppeds_arch_ops ath12k_wifi8_arch_ppeds_ops;

enum ath12k_reo2ppe_rdi {
	PPEDS_REO2PPE1_RDI = 11,
	PPEDS_REO2PPE2_RDI = 12,
	PPEDS_REO2PPE3_RDI = 13,

};

#endif
