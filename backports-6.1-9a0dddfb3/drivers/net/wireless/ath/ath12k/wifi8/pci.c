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
#include "hw.h"
#include "mhi.h"

#define QCN9625_DEVICE_ID		0x1113

static const struct pci_device_id ath12k_wifi8_pci_id_table[] = {
	{ PCI_VDEVICE(QCOM, QCN9625_DEVICE_ID) },
	{0}
};

MODULE_DEVICE_TABLE(pci, ath12k_wifi8_pci_id_table);

static int ath12k_wifi8_pci_probe(struct pci_dev *pdev,
				  const struct pci_device_id *pci_dev)
{
	struct ath12k_base *ab = pci_get_drvdata(pdev);
	struct ath12k_pci *ab_pci;
	u32 msi;
	int ret;

	ab = pci_get_drvdata(pdev);
	if (!ab)
		return -EINVAL;

	ab_pci = ath12k_pci_priv(ab);
	if (!ab_pci)
		return -EINVAL;

	switch (pci_dev->device) {
	case QCN9625_DEVICE_ID:
		if (!of_property_read_u32(ab->dev->of_node, "qcom,msi", &msi) &&
		    msi == ATH12K_MSI_16) {
			dev_info(&pdev->dev, "ath12k supported MSI %d\n", msi);
			ab->msi.config =
				&ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI_16];
		} else {
			ab->msi.config = &ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI];
		}
		ab->static_window_map = true;
		ab->hw_rev = ATH12K_HW_QCN9625_HW10;
		break;

	default:
		dev_err(&pdev->dev, "Unknown WiFi-8 PCI device found: 0x%x\n",
			pci_dev->device);
		return -EOPNOTSUPP;
	}

	ret = ath12k_wifi8_hw_init(ab);
	if (ret) {
		dev_err(&pdev->dev, "WiFi-8 hw_init failed: %d\n", ret);
		return ret;
	}

	return 0;
}


static struct ath12k_pci_driver ath12k_wifi8_pci_driver = {
	.name = "ath12k_wifi8_pci",
	.id_table = ath12k_wifi8_pci_id_table,
	.ops.probe = ath12k_wifi8_pci_probe,
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
