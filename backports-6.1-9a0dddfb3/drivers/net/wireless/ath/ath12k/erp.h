/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef ATH12K_ERP_H
#define ATH12K_ERP_H

int ath12k_vendor_parse_rm_erp(struct wiphy *wiphy, struct wireless_dev *wdev,
			       struct nlattr *attrs);
void ath12k_erp_init(void);
void ath12k_erp_deinit(void);
bool ath12k_erp_in_progress(void);
void ath12k_erp_handle_trigger(struct work_struct *work);
void ath12k_erp_handle_ssr(struct ath12k *ar);

#endif /* ATH12K_ERP_H */

