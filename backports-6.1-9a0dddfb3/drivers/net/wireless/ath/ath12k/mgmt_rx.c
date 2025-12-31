// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"

void ath12k_mgmt_irq_grp_setup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		INIT_WORK(&irq_grp->intr_wq, irq_grp->irq_grp_handler);
#else
		tasklet_setup(&irq_grp->intr_tq, irq_grp->irq_grp_handler);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_setup);

void ath12k_mgmt_irq_grp_cleanup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		cancel_work_sync(&irq_grp->intr_wq);
#else
		tasklet_kill(&irq_grp->intr_tq);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_cleanup);

void ath12k_mgmt_irq_grp_enable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->irqs[i]);
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_enable);

void ath12k_mgmt_irq_grp_disable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->irqs[i]);
}

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
