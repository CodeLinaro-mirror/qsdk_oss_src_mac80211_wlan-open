// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include "../pci.h"
#include "../erp.h"
#include "pci.h"
#include "../ini.h"

static int pci_err;

static int ath12k_wifi8_init(void)
{
	ath12k_erp_init();

	ath12k_cfg_global_init();

	pci_err = ath12k_wifi8_pci_init();
	if (pci_err)
		pr_warn("Failed to initialize ath12k WiFi8 PCI device: %d\n",
			pci_err);

	return pci_err;
}

static void ath12k_wifi8_exit(void)
{
	if (!pci_err)
		ath12k_wifi8_pci_exit();

	ath12k_cfg_global_deinit();

	ath12k_erp_deinit();
}

module_init(ath12k_wifi8_init);
module_exit(ath12k_wifi8_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11bn WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
