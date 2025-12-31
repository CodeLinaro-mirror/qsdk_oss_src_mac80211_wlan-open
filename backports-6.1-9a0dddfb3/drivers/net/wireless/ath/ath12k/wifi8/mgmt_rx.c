// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../mgmt_rx.h"
#include "mgmt_rx.h"
#include "hal.h"

void ath12k_wifi8_mgmt_service_srng(struct ath12k_base *ab,
				    struct ath12k_mgmt_irq_grp *irq_grp)
{}

#if LINUX_VERSION_IS_GEQ(6, 13, 0)
void ath12k_wifi8_mgmt_workqueue(struct work_struct *w)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_work(irq_grp, work, intr_wq);

	ath12k_wifi8_mgmt_service_srng(irq_grp->ab, irq_grp);
}
#else
void ath12k_wifi8_mgmt_tasklet(struct tasklet_struct *t)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_tasklet(irq_grp, t, intr_tq);

	ath12k_wifi8_mgmt_service_srng(irq_grp->ab, irq_grp);
}
#endif

int ath12k_wifi8_mgmt_op_device_init(struct ath12k_mgmt *mgmt)
{
	return 0;
}

void ath12k_wifi8_mgmt_op_device_deinit(struct ath12k_mgmt *mgmt)
{}

static struct ath12k_mgmt_arch_ops ath12k_wifi8_mgmt_arch_ops = {
	.mgmt_op_device_init = ath12k_wifi8_mgmt_op_device_init,
	.mgmt_op_device_deinit = ath12k_wifi8_mgmt_op_device_deinit,
};

struct ath12k_mgmt *ath12k_wifi8_mgmt_init(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8;
	struct ath12k_mgmt_irq_grp *irq_grp;

	mgmt = kzalloc(sizeof(*mgmt) + sizeof(*mgmt_wifi8), GFP_KERNEL);
	if (!mgmt)
		return NULL;

	irq_grp = kzalloc(sizeof(*irq_grp), GFP_KERNEL);
	if (!irq_grp) {
		kfree(mgmt);
		return NULL;
	}

	*irq_grp = (struct ath12k_mgmt_irq_grp){
		.grp_id = 0,
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		.irq_grp_handler = ath12k_wifi8_mgmt_workqueue,
#else
		.irq_grp_handler = ath12k_wifi8_mgmt_tasklet,
#endif
		.num_irq = 1,
	};

	mgmt->ab = ab;
	mgmt->dev = ab->dev;
	mgmt->hal = &ab->hal;
	mgmt->arch_ops = &ath12k_wifi8_mgmt_arch_ops;
	mgmt->hw_params = ab->hw_params;
	mgmt->irq_grp = irq_grp;
	mgmt->num_irq_grp = 1;

	return mgmt;
}

void ath12k_wifi8_mgmt_deinit(struct ath12k_mgmt *mgmt)
{
	kfree(mgmt->irq_grp);
	kfree(mgmt);
}
