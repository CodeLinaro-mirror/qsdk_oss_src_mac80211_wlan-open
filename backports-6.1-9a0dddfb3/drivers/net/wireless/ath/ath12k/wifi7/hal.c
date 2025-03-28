// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "hw.h"
#include "hal_desc.h"
#include "../hal.h"
#include "hal.h"

const struct ath12k_hw_version_map ath12k_wifi7_hw_ver_map[] = {
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_qcn9274,
		.hw_regs = &qcn9274_v1_regs,
	},
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_qcn9274,
		.hw_regs = &qcn9274_v2_regs,
	},
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_wcn7850,
		.hw_regs = &wcn7850_regs,
	},
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_ipq5332,
		.hw_regs = &ipq5332_regs,
	},
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_ipq5332,
		.hw_regs = &ipq5424_regs,
	},
	{
		.hal_params = &ath12k_wifi7_hw_hal_params_ipq5332,
		.hw_regs = &qcn6432_regs,
	},
};
