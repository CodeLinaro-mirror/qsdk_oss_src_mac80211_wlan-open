/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_UMAC_RESET_H
#define ATH12K_WIFI8_UMAC_RESET_H

#include "../core.h"
#include "../umac_reset.h"

/* WiFi8-specific UMAC reset function declarations */
void ath12k_wifi8_umac_reset_handle_pre_reset(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_start(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab);

#endif /* ATH12K_WIFI8_UMAC_RESET_H */
