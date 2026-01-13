/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_MGMT_RX_H
#define ATH12K_MGMT_RX_H

#include "hw.h"
#include "hal.h"

struct ath12k_mgmt;

struct mgmt_srng {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
};

#define ATH12K_MGMT_IRQ_GRP_NUM_MAX 2
#define ATH12K_MGMT_IRQ_PER_GRP_NUM_MAX 2
#define MGMT_IRQ_NAME_LEN 20

struct ath12k_mgmt_arch_ops {
	int (*mgmt_op_device_init)(struct ath12k_mgmt *mgmt);
	void (*mgmt_op_device_deinit)(struct ath12k_mgmt *mgmt);
};

struct ath12k_mgmt_irq_grp {
	int grp_id;
	struct ath12k_base *ab;
	char irq_name[MGMT_IRQ_NAME_LEN];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
	struct work_struct intr_wq;
	void (*irq_grp_handler)(struct work_struct *w);
#else
	struct tasklet_struct intr_tq;
	void (*irq_grp_handler)(struct tasklet_struct *t);
#endif
	int num_irq;
	int irqs[ATH12K_MGMT_IRQ_PER_GRP_NUM_MAX];
};

struct ath12k_mgmt {
	struct ath12k_base *ab;
	struct device *dev;
	struct ath12k_hal *hal;
	struct ath12k_mgmt_arch_ops *arch_ops;
	const struct ath12k_hw_params *hw_params;
	struct ath12k_mgmt_irq_grp *irq_grp;
	u8 num_irq_grp;

	/* must be last */
	u8 arch_priv[] __aligned(sizeof(void *));
};

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt);

static inline int ath12k_mgmt_arch_op_device_init(struct ath12k_mgmt *mgmt)
{
	if (!mgmt->arch_ops->mgmt_op_device_init)
		return -EOPNOTSUPP;

	return mgmt->arch_ops->mgmt_op_device_init(mgmt);
}

static inline void ath12k_mgmt_arch_op_device_deinit(struct ath12k_mgmt *mgmt)
{
	if (!mgmt->arch_ops->mgmt_op_device_deinit)
		return;

	mgmt->arch_ops->mgmt_op_device_deinit(mgmt);
}
#endif
