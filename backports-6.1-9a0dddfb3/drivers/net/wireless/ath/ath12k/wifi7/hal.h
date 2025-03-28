/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_HAL_WIFI7_H
#define ATH12K_HAL_WIFI7_H

extern const struct ath12k_hw_regs wcn7850_regs;
extern const struct ath12k_hw_regs qcn9274_v1_regs;
extern const struct ath12k_hw_regs qcn9274_v2_regs;
extern const struct ath12k_hw_regs ipq5332_regs;
extern const struct ath12k_hw_regs ipq5424_regs;
extern const struct ath12k_hw_regs qcn6432_regs;

extern const struct ath12k_hw_hal_params ath12k_wifi7_hw_hal_params_wcn7850;
extern const struct ath12k_hw_hal_params ath12k_wifi7_hw_hal_params_qcn9274;
extern const struct ath12k_hw_hal_params ath12k_wifi7_hw_hal_params_ipq5332;

extern const struct ath12k_hw_version_map ath12k_wifi7_hw_ver_map[];
#endif
