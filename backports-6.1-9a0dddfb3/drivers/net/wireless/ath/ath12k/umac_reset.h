/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_UMAC_RESET_H
#define ATH12K_UMAC_RESET_H

#include "core.h"
#include "dp.h"
#include "hif.h"
#include "debug.h"
bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab);
void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
						   enum dp_umac_reset_tx_cmd tx_event);
#endif /*ATH12K_UMAC_RESET_H*/
