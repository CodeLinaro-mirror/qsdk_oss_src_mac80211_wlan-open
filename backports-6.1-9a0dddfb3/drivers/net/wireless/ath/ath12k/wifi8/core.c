// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/dmapool.h>
#include "../pci.h"

#include "../core.h"
#include "../debug.h"
#include "../erp.h"
#include "pci.h"
#include "wmi.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif

static int pci_err;
static struct dma_pool *ath12k_cu_mem_pool;

static int ath12k_wifi8_cu_mem_pool_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = NULL;
	int i;

	if (ath12k_cu_mem_pool)
		return 0;

	for (i = 0; i < ag->num_devices; i++) {
		if (ag->ab[i] && !ag->ab[i]->is_bypassed) {
			ab = ag->ab[i];
			break;
		}
	}

	if (!ab)
		return -EINVAL;

	if (!test_bit(WMI_TLV_SERVICE_SHARED_CU_MEM_MODEL_COUNT_DOWN,
		      ab->wmi_ab.svc_map))
		return 0;

	ath12k_cu_mem_pool = dma_pool_create("ath12k_cu_mem",
					     ab->dev, sizeof(struct ath12k_cu_mem),
					     0, 0);

	if (!ath12k_cu_mem_pool)
		return -ENOMEM;

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "cu_mem dma pool created: size %zu align 0 boundary 0\n",
		   sizeof(struct ath12k_cu_mem));

	return 0;
}

static void ath12k_wifi8_cu_mem_pool_deinit(void)
{
	if (!ath12k_cu_mem_pool)
		return;

	dma_pool_destroy(ath12k_cu_mem_pool);
	ath12k_cu_mem_pool = NULL;
}

static int ath12k_wifi8_cu_mem_alloc(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	if (!ath12k_cu_mem_pool)
		return -EINVAL;

	arvif->cu_mem = dma_pool_alloc(ath12k_cu_mem_pool,
				       GFP_KERNEL,
				       &arvif->cu_mem_paddr);
	if (!arvif->cu_mem)
		return -ENOMEM;

	memset(arvif->cu_mem, 0, sizeof(struct ath12k_cu_mem));

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "cu_mem alloc: vdev %d vaddr %p paddr %pad size %zu\n",
		   arvif->vdev_id, arvif->cu_mem, &arvif->cu_mem_paddr,
		   sizeof(struct ath12k_cu_mem));

	return 0;
}

static void ath12k_wifi8_cu_mem_free(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	if (!arvif->cu_mem)
		return;

	dma_pool_free(ath12k_cu_mem_pool,
		      arvif->cu_mem,
		      arvif->cu_mem_paddr);

	arvif->cu_mem = NULL;
}

const struct ath12k_cp_arch_ops ath12k_wifi8_cp_ops = {
	.cu_mem_pool_init = ath12k_wifi8_cu_mem_pool_init,
	.cu_mem_pool_deinit = ath12k_wifi8_cu_mem_pool_deinit,
	.cu_mem_alloc = ath12k_wifi8_cu_mem_alloc,
	.cu_mem_free = ath12k_wifi8_cu_mem_free,
	.cu_notify = ath12k_wifi8_cu_notify,
};

/* SMD feature flags - controls behaviour of SMD-related operations.
 * New per-feature knobs for the SMD topic should be added here so they
 * are all discoverable in one place via modinfo / /sys/module/ath12k_wifi8/parameters/.
 */
bool ath12k_wifi8_clear_vld_after_smd_ctx_fetch = true;
module_param_named(clear_vld_after_smd_ctx_fetch,
		   ath12k_wifi8_clear_vld_after_smd_ctx_fetch, bool, 0644);
MODULE_PARM_DESC(clear_vld_after_smd_ctx_fetch,
		 "Clear REO VLD after fetching SMD ctx (default: 1 - enabled)");

bool ath12k_wifi8_smd_skip_bitmap_update = true;
module_param_named(smd_skip_bitmap_update,
		   ath12k_wifi8_smd_skip_bitmap_update, bool, 0644);
MODULE_PARM_DESC(smd_skip_bitmap_update,
		 "On Target AP, updates SSN and PN but skips REO bitmap update (default: 1 - skips REO bitmap update)");

static int ath12k_wifi8_init(void)
{
	ath12k_erp_init();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_init();
#endif

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

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_deinit();
#endif

	ath12k_erp_deinit();
}

module_init(ath12k_wifi8_init);
module_exit(ath12k_wifi8_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11bn WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
