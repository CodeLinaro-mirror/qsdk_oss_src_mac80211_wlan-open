// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt)
{
	/* Skip device_init if mgmt is not supported */
	if (!mgmt)
		return 0;

	return ath12k_mgmt_arch_op_device_init(mgmt);
}

void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt)
{
	if (!mgmt)
		return;

	ath12k_mgmt_arch_op_device_deinit(mgmt);
}
