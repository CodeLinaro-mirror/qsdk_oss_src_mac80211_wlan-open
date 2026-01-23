/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_MGMT_RX_H
#define ATH12K_MGMT_RX_H

#include "hw.h"
#include "hal.h"

struct ath12k_mgmt;

#define MGMT_RX_BUFFER_SIZE                2048
#define MGMT_RX_BUFFER_ALIGN_SIZE          128

#define MGMT_REO_DST_RING_SIZE             1024
#define MGMT_REO_EXCEPTION_RING_SIZE       1024
#define MGMT_WBM_IDLE_BUF_RING_SIZE        2048
#define MGMT_REFILL_RING_SIZE              MGMT_WBM_IDLE_BUF_RING_SIZE

#define MGMT_RX_DESC_COUNT                 2048
#define MGMT_RX_DESC_BLOCK_SIZE            512
#define NUM_MGMT_RX_DESC_BLOCKS            (MGMT_RX_DESC_COUNT / MGMT_RX_DESC_BLOCK_SIZE)

/* Descriptor hierarchy:
 * +--------+        +-------+-------+-----+-------+
 * | Block0 | -----> | Slot0 | Slot1 | ... | SlotM |
 * +--------+        +-------+-------+-----+-------+
 * |  ....  |
 * +--------+        +-------+-------+-----+-------+
 * | BlockN | -----> | Slot0 | Slot1 | ... | SlotM |
 * +--------+        +-------+-------+-----+-------+
 *
 * Of the available 20-bit cookie, b0-b8 are used to identify the slot and b9-b19 are used
 * to identify the block.
 */
#define MGMT_RX_DESC_SLOT_MASK             GENMASK(8, 0)
#define MGMT_RX_DESC_BLOCK_MASK            GENMASK(19, 9)
#define MGMT_RX_DESC_COOKIE_SHIFT          9

#define ATH12K_MGMT_RX_DESC_MAGIC          0xABBAABBA

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
#define ATH12K_MGMT_IRQ_GRP_ID_INVALID -1
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

	/* Rx buffer descriptors for mgmt */
	struct ath12k_rx_desc_info *rx_desc_baddr[NUM_MGMT_RX_DESC_BLOCKS];

	/* protects rx descriptors for arch-specific rx_refill_ring */
	spinlock_t rx_desc_lock;
	struct list_head rx_desc_free_list;

	/* must be last */
	u8 arch_priv[] __aligned(sizeof(void *));
};

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt);

void ath12k_mgmt_irq_grp_setup(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_irq_grp_cleanup(struct ath12k_mgmt *mgmt);
void ath12k_mgmt_irq_grp_enable(struct ath12k_mgmt_irq_grp *irq_grp);
void ath12k_mgmt_irq_grp_disable(struct ath12k_mgmt_irq_grp *irq_grp);

int ath12k_mgmt_srng_setup(struct ath12k_base *ab, struct mgmt_srng *ring,
			   enum hal_ring_type type, int ring_num,
			   int mac_id, int grp_id, int num_entries);
void ath12k_mgmt_srng_cleanup(struct ath12k_base *ab, struct mgmt_srng *ring);

static inline u32 ath12k_mgmt_gen_rx_desc_cookie(u16 block, u16 slot)
{
	return (u32)block << MGMT_RX_DESC_COOKIE_SHIFT | slot;
}

int ath12k_mgmt_rx_desc_init(struct ath12k_base *ab);
void ath12k_mgmt_rx_desc_cleanup(struct ath12k_base *ab);
size_t ath12k_mgmt_rx_desc_list_cut_nodes(struct list_head *used_list,
					  struct list_head *rx_desc_list,
					  size_t count);
size_t ath12k_mgmt_get_req_entries_from_refill_ring(struct ath12k_base *ab,
						    struct mgmt_srng *rx_refill_ring,
						    struct list_head *list);

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
