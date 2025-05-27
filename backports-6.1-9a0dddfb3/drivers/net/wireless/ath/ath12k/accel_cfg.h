/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
 */

#ifndef ACCEL_CFG_H
#define ACCEL_CFG_H

#include <ath/ath_dp_accel_cfg.h>

/**
 * ath12k_dp_accel_cfg_init() - Initialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_init(struct ath12k_base *ab);

/**
 * ath12k_dp_accel_cfg_deinit() - Deinitialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_deinit(struct ath12k_base *ab);
#endif
