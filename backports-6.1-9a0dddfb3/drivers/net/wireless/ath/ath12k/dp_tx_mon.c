// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "dp_tx_mon.h"

/**
 * ath12k_dp_mon_tx_desc_free() - Free monitor descriptors
 * @local_list: List of descriptors to free
 * @dp_mon: DP monitor handle
 */
void ath12k_dp_mon_tx_desc_free(struct list_head *local_list,
				struct ath12k_dp_mon *dp_mon)
{
	spin_lock_bh(&dp_mon->tx_mon_desc_lock);
	list_splice_tail_init(local_list, &dp_mon->tx_mon_desc_free_list);
	spin_unlock_bh(&dp_mon->tx_mon_desc_lock);
}

/**
 * ath12k_dp_tx_mon_flush_desc_list() - Flush monitor descriptor list
 * @dp_pdev: DP PDEV context
 * @mon_desc_list: List of monitor descriptors to flush
 *
 * This function flushes monitor descriptors when errors occur during
 * ring processing. It processes any TLV data in the descriptors and
 * returns the descriptors to the free list.
 *
 * Called from NAPI context when PPDU preparation fails or during
 * error recovery scenarios.
 */
void ath12k_dp_tx_mon_flush_desc_list(struct ath12k_pdev_dp *dp_pdev,
				      struct list_head *mon_desc_list)
{
	struct ath12k_dp_mon_desc *tmp_desc, *entry_desc;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;

	list_for_each_entry_safe(entry_desc, tmp_desc,
				 mon_desc_list, list) {
		if (unlikely(!entry_desc->mon_buf))
			continue;

		ath12k_core_dma_unmap_page(dp_pdev->dp->dev, entry_desc->paddr,
					   ATH12K_DP_MON_TX_BUF_SIZE,
					   DMA_FROM_DEVICE);
	}

	/* Free descriptor list */
	ath12k_dp_mon_tx_desc_free(mon_desc_list, dp_mon);
}

/**
 * ath12k_dp_mon_tx_process_ring() - Process TX monitor destination ring
 * @dp: DP context
 * @mac_id: MAC ID for the radio
 * @napi: NAPI context for budget management
 * @budget: Processing budget (number of descriptors to process)
 *
 * This is the main entry point for TX monitor ring processing, called from
 * NAPI context. It implements the "work queue heavy, lighter tasklet" architecture:
 *
 * NAPI Context (Minimal Work):
 * - Ring descriptor processing
 * - TLV fragment collection
 * - Work queue scheduling at end of PPDU
 * - Budget management
 *
 * Work Queue Context (Heavy Work - scheduled by this function):
 * - Heavy TLV parsing
 * - Frame generation
 * - Stack delivery
 *
 * The function processes ring entries within the given budget, collecting
 * TLV fragments until end of PPDU is detected, then schedules work queue
 * for heavy processing.
 *
 * Return: Number of ring descriptors processed
 */
int ath12k_dp_mon_tx_process_ring(struct ath12k_pdev_dp *dp_pdev,
				  int mac_id,
				  struct napi_struct *napi,
				  int *budget)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_hal *hal = &ab->hal;
	struct ath12k_mon_ring_desc_info desc_info;
	struct hal_srng *tx_mon_dst_ring;
	struct list_head *mon_desc_head;
	void *ring_entry;
	u32 ring_id, end_offset, end_reason;
	int num_buffs_reaped = 0;
	u8 *status_frag = NULL;
	int ret;

	if (unlikely(!dp_mon_pdev || !budget || !dp_mon)) {
		ath12k_warn(ab, "TX Mon: Invalid parameters\n");
		return 0;
	}

	mon_desc_head = &dp_mon_pdev->tx_mon_desc_work_list;
	ring_id = dp_mon_pdev->tx_mon_dst_ring.ring_id;
	tx_mon_dst_ring = &ab->hal.srng_list[ring_id];

	if (unlikely(!tx_mon_dst_ring || !tx_mon_dst_ring->ring_base_vaddr)) {
		ath12k_warn(ab,
			    "TX Mon: Invalid srng for ring_id=%u\n", ring_id);
		return 0;
	}

	spin_lock_bh(&tx_mon_dst_ring->lock);
	ath12k_hal_srng_access_begin(ab, tx_mon_dst_ring);

	/* Process ring entries within budget - minimal work in NAPI context */
	while (*budget &&
	       (ring_entry = ath12k_hal_srng_dst_peek(ab, tx_mon_dst_ring))) {
		struct ath12k_dp_mon_desc *mon_desc;

		if (!hal->hal_mon_ops ||
		    !hal->hal_mon_ops->extract_tx_mon_ring_desc) {
			ath12k_err(ab,
				   "TX monitor ring desc extraction not supported\n");
			goto move_next;
		}

		ret = ath12k_hal_mon_extract_tx_mon_ring_desc(hal,
							      ring_entry,
							      &desc_info);
		if (ret) {
			ath12k_err(ab,
				   "Failed to extract TX mon ring desc: %d\n",
				   ret);
			goto move_next;
		}

		if (desc_info.empty_desc) {
			dp_mon_pdev->tx_mon_stats.empty_descriptors++;

			if (desc_info.end_reason == HAL_MON_PPDU_TRUNCATED) {
				dp_mon_pdev->tx_mon_stats.truncated_ppdu++;
				ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: truncated ppdu end, mac_id=%d\n",
					   mac_id);
				ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
								 mon_desc_head);
			} else {
				ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: empty desc mac_id=%d\n",
					   mac_id);
			}
			goto move_next;
		}
		end_reason = desc_info.end_reason;
		end_offset = desc_info.end_offset;

		mon_desc = desc_info.mon_desc;
		if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
			ath12k_err(ab, "TX Mon: invalid in_use=[%d] flag, mac_id %d\n",
				   mon_desc->in_use, mac_id);
			goto move_next;
		}

		status_frag = (u8 *)mon_desc->mon_buf;
		if (unlikely(!status_frag)) {
			ath12k_err(ab, "TX Mon: NULL buffer received in mac_id %d\n",
				   mac_id);
			goto move_next;
		}

		mon_desc->in_use = DP_MON_DESC_STATUS_REAP;
		list_add_tail(&mon_desc->list, mon_desc_head);
		if (end_reason == HAL_MON_FLUSH_DETECTED ||
		    end_reason == HAL_MON_PPDU_TRUNCATED) {
			ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
							 mon_desc_head);
			goto move_next;
		}

		if (unlikely(end_offset > ATH12K_DP_MON_TX_BUF_SIZE)) {
			ath12k_warn(ab, "TX Mon: invalid offset %u received in mac_id %d\n",
				    end_offset, mac_id);
			end_offset = ATH12K_DP_MON_TX_BUF_SIZE - 1;
		}

		/* The hardware reports buffer length as (actual_length - 1),
		 * likely due to internal indexing or alignment constraints.
		 * To obtain the true buffer length for processing, increment
		 * the reported end_offset by 1 before using it.
		 */
		mon_desc->buf_len = end_offset + 1;

		if (end_reason == HAL_MON_END_OF_PPDU) {
			*budget -= 1;
			mon_desc->end_of_ppdu = true;
			ath12k_dp_mon_tx_desc_free(mon_desc_head, dp_mon);
		}

move_next:
		ring_entry = ath12k_hal_srng_dst_get_next_entry(ab,
								tx_mon_dst_ring);
		num_buffs_reaped++;
	}

	ath12k_hal_srng_access_end(ab, tx_mon_dst_ring);
	spin_unlock_bh(&tx_mon_dst_ring->lock);

	return num_buffs_reaped;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_process_ring);
