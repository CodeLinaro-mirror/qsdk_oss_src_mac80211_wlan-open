// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/pci.h>

#include "../pci.h"
#include "pci.h"
#include "../pcic.h"
#include "../core.h"
#include "../hif.h"
#include "../mhi.h"

#define QCN9625_DEVICE_ID		0x1113

static const struct pci_device_id ath12k_wifi8_pci_id_table[] = {
	{ PCI_VDEVICE(QCOM, QCN9625_DEVICE_ID) },
	{0}
};

MODULE_DEVICE_TABLE(pci, ath12k_wifi8_pci_id_table);


static struct ath12k_pci_driver ath12k_wifi8_pci_driver = {
	.name = "ath12k_wifi8_pci",
	.id_table = ath12k_wifi8_pci_id_table,
};

int ath12k_wifi8_pci_init(void)
{
	int ret;

	ret = ath12k_pci_register_driver(ATH12K_DEVICE_FAMILY_WIFI8,
					 &ath12k_wifi8_pci_driver);
	if (ret) {
		pr_err("Failed to register ath12k WiFi-8 driver: %d\n",
		       ret);
		return ret;
	}

	return 0;
}

void ath12k_wifi8_pci_exit(void)
{
	ath12k_pci_unregister_driver(ATH12K_DEVICE_FAMILY_WIFI8);
}
