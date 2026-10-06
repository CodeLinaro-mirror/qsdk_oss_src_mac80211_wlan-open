/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef ATH12K_WIFI8_PTP_H
#define ATH12K_WIFI8_PTP_H

#include "../core.h"
#include "../debug.h"
#include "../debugfs.h"
#include <linux/module.h>

int  ath12k_timesync_init(struct ath12k_base *ab);
void ath12k_timesync_deinit(struct ath12k_base *ag);

int  ath12k_ts_mode_sysfs_init(struct ath12k_hw_group *ag);
void ath12k_ts_mode_sysfs_deinit(struct ath12k_hw_group *ag);

#endif /* ATH12K_WIFI8_PTP_H */
