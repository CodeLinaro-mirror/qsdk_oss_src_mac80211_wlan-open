// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "dp_tx_mon.h"

/**
 * ath12k_dp_mon_tx_setup_ppdu_desc() - Setup TX monitor PPDU descriptor pool
 * @dp_pdev: Pointer to DP PDEV context for device-specific operations
 *
 * This function initializes the TX monitor PPDU descriptor pool and associated
 * management structures. It allocates memory for descriptor pool, initializes
 * list management structures, and populates the free descriptor list for
 * efficient descriptor allocation during TX monitor operations.
 *
 * The function performs the following initialization sequence:
 * 1. Allocates memory pool for PPDU descriptors using kcalloc()
 * 2. Initializes spinlock for thread-safe descriptor list operations
 * 3. Initializes free, used, and processing descriptor lists
 * 4. Populates free list with all allocated descriptors
 * 5. Updates statistics counters for descriptor tracking
 *
 * Context: Called during TX monitor initialization in process context
 * Locking: Initializes and uses tx_mon_ppdu_desc_lock for list operations
 *
 * Return: 0 on success, -ENOMEM on memory allocation failure
 */
static int
ath12k_dp_mon_tx_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	size_t alloc_size = sizeof(struct ath12k_dp_mon_ppdu_desc);
	int i;

	if (dp_mon_pdev->tx_mon_ppdu_desc_pool) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor PPDU desc pool already allocated, reusing\n");
		dp_mon_pdev->tx_mon_ppdu_desc_initialized = true;
		return 0;
	}

	dp_mon_pdev->tx_mon_ppdu_desc_pool = kcalloc(ATH12K_DP_MON_NUM_PPDU_DESC,
						     alloc_size, GFP_KERNEL);
	if (unlikely(!dp_mon_pdev->tx_mon_ppdu_desc_pool)) {
		ath12k_warn(dp_pdev->dp->ab,
			    "Failed to allocate monitor PPDU desc pool\n");
		return -ENOMEM;
	}

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX MON SETUP: Allocated PPDU desc pool at %p, size=%zu\n",
		   dp_mon_pdev->tx_mon_ppdu_desc_pool,
		   alloc_size * ATH12K_DP_MON_NUM_PPDU_DESC);

	spin_lock_init(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	INIT_LIST_HEAD(&dp_mon_pdev->tx_mon_ppdu_desc_free_list);
	INIT_LIST_HEAD(&dp_mon_pdev->tx_mon_ppdu_desc_used_list);
	INIT_LIST_HEAD(&dp_mon_pdev->tx_mon_ppdu_desc_proc_list);

	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	for (i = 0; i < ATH12K_DP_MON_NUM_PPDU_DESC; i++) {
		INIT_LIST_HEAD(&dp_mon_pdev->tx_mon_ppdu_desc_pool[i].list);
		list_add_tail(&dp_mon_pdev->tx_mon_ppdu_desc_pool[i].list,
			      &dp_mon_pdev->tx_mon_ppdu_desc_free_list);
		dp_mon_pdev->mon_stats.ppdu_desc_free++;
	}
	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	dp_mon_pdev->tx_mon_ppdu_desc_initialized = true;

	return 0;
}

/**
 * ath12k_dp_mon_tx_wq_init() - Initialize TX monitor work queue
 * @dp_pdev: DP pdev handle
 *
 * This function initializes the TX monitor work queue and related structures.
 * with proper error handling and initialization order.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_dp_mon_tx_wq_init(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_mon_data *mon_data;
	size_t radiotap_vendor_size = sizeof(struct ieee80211_radiotap_vendor_ns) +
		sizeof(struct ath12k_rtap_vendor_ns);

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return -EINVAL;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	mon_data = &dp_mon_pdev->mon_data;
	if (!mon_data->rtap_vendor_tlv) {
		mon_data->rtap_vendor_tlv = kzalloc(radiotap_vendor_size,
						    GFP_KERNEL);
		if (!mon_data->rtap_vendor_tlv)
			return -ENOMEM;

		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor vendor TLV allocated\n");
	} else {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor vendor TLV already allocated, reusing\n");
	}

	if (WARN_ON(dp_mon_pdev->txmon_wq)) {
		ath12k_err(dp_pdev->dp->ab,
			   "TX monitor work queue not cleaned up properly\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&dp_mon_pdev->tx_mon_desc_work_list);

	dp_mon_pdev->txmon_wq =
		alloc_workqueue("ath12k_txmon_wq",
				WQ_UNBOUND, 1);
	if (!dp_mon_pdev->txmon_wq) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX monitor work queue allocation failed\n");
		goto free_vendor_tlv;
	}

	return 0;

free_vendor_tlv:
	kfree(mon_data->rtap_vendor_tlv);
	mon_data->rtap_vendor_tlv = NULL;
	return -ENOMEM;
}

/**
 * ath12k_dp_mon_tx_cleanup_ppdu_desc() - Cleanup TX monitor PPDU descriptors
 * @dp_pdev: Pointer to DP PDEV context for cleanup operations
 *
 * This function performs complete cleanup of TX monitor PPDU descriptor pool
 * and associated resources. It safely deallocates the descriptor pool memory
 * and resets the pool pointer to prevent use-after-free conditions.
 *
 * The function ensures safe cleanup by:
 * 1. Acquiring the descriptor pool spinlock to prevent concurrent access
 * 2. Freeing the allocated descriptor pool memory using kfree()
 * 3. Setting the pool pointer to NULL to prevent dangling pointer access
 * 4. Releasing the spinlock after cleanup completion
 *
 * Context: Called during TX monitor shutdown in process context
 * Locking: Uses tx_mon_ppdu_desc_lock for safe memory deallocation
 */
static void ath12k_dp_mon_tx_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev)
		return;

	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	kfree(dp_mon_pdev->tx_mon_ppdu_desc_pool);
	dp_mon_pdev->tx_mon_ppdu_desc_pool = NULL;
	dp_mon_pdev->tx_mon_ppdu_desc_initialized = false;
	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
}

/**
 * ath12k_dp_mon_tx_wq_deinit() - Deinitialize TX monitor work queue
 * @dp_pdev: DP pdev handle
 *
 * This function deinitializes the TX monitor work queue and frees resources.
 */
static void ath12k_dp_mon_tx_wq_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_mon_data *mon_data;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	mon_data = &dp_mon_pdev->mon_data;

	if (dp_mon_pdev->txmon_wq) {
		cancel_work_sync(&dp_mon_pdev->txmon_work);
		destroy_workqueue(dp_mon_pdev->txmon_wq);
		dp_mon_pdev->txmon_wq = NULL;
	}

	kfree(mon_data->rtap_vendor_tlv);
	mon_data->rtap_vendor_tlv = NULL;
}

/**
 * ath12k_dp_mon_tx_wq_start() - Start TX monitor work queue and PPDU descriptors
 * @dp_pdev: Pointer to DP PDEV context for device access and configuration
 * @mac_id: MAC ID for the physical device (used for logging and identification)
 *
 * This function initializes the TX monitor work queue infrastructure and sets up
 * PPDU descriptor management for TX monitor functionality. It should be called
 * after basic TX monitor ring allocation is complete but before any TX monitor
 * operations begin.
 *
 * The function performs initialization in the following order:
 * 1. Setup PPDU descriptors for TX monitor frame processing
 * 2. Initialize work queue for asynchronous TX monitor processing
 * 3. Provide proper cleanup on any failure
 *
 * This function is typically called during interface bring-up or when TX monitor
 * functionality needs to be activated. It complements the basic resource allocation
 * done in ath12k_dp_mon_tx_pdev_alloc().
 *
 * Context: Can be called from process context during interface initialization.
 * Locking: Uses internal locking through architecture-specific operations.
 *
 * Return: 0 on success, negative error code on failure
 *         -EINVAL if invalid parameters or missing operations
 *         Architecture-specific error codes from setup operations
 */
int ath12k_dp_mon_tx_wq_start(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	int ret = 0;

	if (unlikely(!dp_pdev)) {
		ath12k_err(NULL, "Tx Mon: Invalid DP Pdev\n");
		return -EINVAL;
	}

	ret = ath12k_dp_mon_tx_setup_ppdu_desc(dp_pdev);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "failed to setup TX mon ppdu desc: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_mon_tx_wq_init(dp_pdev);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "failed to init TX mon workqueue for pdev_id %d: %d\n",
			    mac_id, ret);
		goto cleanup_ppdu_desc;
	}
	return 0;

cleanup_ppdu_desc:
	ath12k_dp_mon_tx_cleanup_ppdu_desc(dp_pdev);
	return ret;
}

/**
 * ath12k_dp_mon_tx_wq_stop() - Stop TX monitor work queue and cleanup descriptors
 * @dp_pdev: Pointer to DP PDEV context for cleanup operations
 *
 * This function performs complete cleanup of TX monitor work queue infrastructure
 * and PPDU descriptor resources. It should be called during interface shutdown
 * or when TX monitor functionality needs to be deactivated.
 *
 * The function performs cleanup in the following order:
 * 1. Deinitialize work queue and cancel any pending work
 * 2. Cleanup PPDU descriptors and free associated resources
 * 3. Ensure all resources are properly released
 *
 * This function is the counterpart to ath12k_dp_mon_tx_wq_start() and should
 * be called during interface teardown. It complements the basic resource
 * deallocation done in ath12k_dp_mon_tx_pdev_free().
 *
 * Context: Can be called from process context during interface shutdown.
 * Locking: Uses internal locking through architecture-specific operations.
 */
void ath12k_dp_mon_tx_wq_stop(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;

	if (unlikely(!dp_pdev))
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	if (!dp_mon_pdev)
		return;

	if (dp_mon_pdev->tx_mon_wq_initialized) {
		ath12k_dp_mon_tx_wq_deinit(dp_pdev);
		dp_mon_pdev->tx_mon_wq_initialized = false;
	}

	if (dp_mon_pdev->tx_mon_ppdu_desc_initialized) {
		ath12k_dp_mon_tx_cleanup_ppdu_desc(dp_pdev);
		dp_mon_pdev->tx_mon_ppdu_desc_initialized = false;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_wq_stop);

/**
 * ath12k_dp_mon_tx_desc_free() - Free monitor descriptors
 * @local_list: List of descriptors to free
 * @dp_mon: DP monitor handle
 */
static void
ath12k_dp_mon_tx_desc_free(struct list_head *local_list,
			   struct ath12k_dp_mon *dp_mon)
{
	spin_lock_bh(&dp_mon->tx_mon_desc_lock);
	list_splice_tail_init(local_list, &dp_mon->tx_mon_desc_free_list);
	spin_unlock_bh(&dp_mon->tx_mon_desc_lock);
}

/**
 * ath12k_dp_mon_tx_free_pkt_buf() - Free packet buffers from TLV data
 * @pdev_dp: Pointer to DP PDEV context containing device and statistics info
 * @mon_buf: Pointer to monitor buffer containing TLV data to be parsed
 * @mon_buf_len: Length of valid data in the monitor buffer
 *
 * This function parses TLV data from a monitor status buffer and frees any
 * packet buffers referenced by HAL_TX_MON_BUF_ADDR TLVs. It performs
 * comprehensive validation and cleanup of monitor descriptors and their
 * associated DMA buffers.
 *
 * The function handles:
 * - TLV parsing using HAL 64-bit TLV header format with proper alignment
 * - Cookie validation using magic number verification for memory safety
 * - DMA buffer unmapping to ensure proper cache coherency
 * - Page fragment deallocation to prevent memory leaks
 * - Descriptor state validation and error reporting
 * - Statistics tracking for monitoring system health
 *
 * TLV Processing Flow:
 * 1. Parse each TLV header to extract tag and length
 * 2. For HAL_TX_MON_BUF_ADDR TLVs, extract packet info and descriptor cookie
 * 3. Validate descriptor magic number and usage state
 * 4. Unmap DMA buffer and free page fragment
 * 5. Add descriptor to cleanup list for return to free pool
 * 6. Continue to next TLV with proper alignment
 *
 * Context: Called from work queue context during TLV flushing operations.
 * Locking: Uses internal locking for descriptor list management.
 */
static void
ath12k_dp_mon_tx_free_pkt_buf(struct ath12k_pdev_dp *pdev_dp,
			      u8 *mon_buf, u32 mon_buf_len)
{
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = &ab->hal;
	struct dp_mon_packet_info *packet_info;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_dp_mon_desc *pkt_desc;
	struct list_head mon_desc_used_list;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_pdev_tx_mon_stats *tx_mon_stats;
	u8 *ptr = mon_buf;
	u16 tlv_tag, tlv_len;

	tx_mon_stats = &pdev_dp->dp_mon_pdev->tx_mon_stats;
	INIT_LIST_HEAD(&mon_desc_used_list);

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_TAG);
		ptr += sizeof(*tlv);

		tlv_len = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_LEN);

		if (ath12k_hal_is_mon_buf_addr_tlv(hal, tlv_tag)) {
			packet_info = (struct dp_mon_packet_info *)ptr;
			pkt_desc = (struct ath12k_dp_mon_desc *)
				(uintptr_t)(packet_info->cookie);

			if (unlikely(!pkt_desc)) {
				ath12k_warn(ab,
					    "mon_flush: NULL pkt_desc received in macid %d\n",
					    pdev_dp->mac_id);
				goto next_tlv;
			}

			if (unlikely(pkt_desc->magic !=
				     ATH12K_MON_MAGIC_VALUE)) {
				ath12k_warn(ab,
					    "mon_flush: invalid magic value in macid %d\n",
					    pdev_dp->mac_id);
				goto next_tlv;
			}

			list_add_tail(&pkt_desc->list, &mon_desc_used_list);

			if (unlikely(pkt_desc->in_use != DP_MON_DESC_TO_HW)) {
				ath12k_warn(ab,
					    "mon_flush: invalid in_use=[%d] flag, macid %d\n",
					    pkt_desc->in_use, pdev_dp->mac_id);
				goto next_tlv;
			}

			ath12k_core_dma_unmap_page(dp->dev, pkt_desc->paddr,
						   ATH12K_DP_MON_TX_BUF_SIZE,
						   DMA_FROM_DEVICE);
			tx_mon_stats->tx_pkt_tlv_free++;

			page_frag_free(pkt_desc->mon_buf);
			pkt_desc->mon_buf = NULL;
			pkt_desc->in_use = DP_MON_DESC_H_PROC_ERR;
		}

next_tlv:
		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);
	} while ((ptr - mon_buf) < mon_buf_len);

	if (likely(!list_empty(&mon_desc_used_list)))
		ath12k_dp_mon_tx_desc_free(&mon_desc_used_list, dp_mon);
}

/**
 * ath12k_dp_tx_mon_flush_tlv() - Flush TLV data and free associated resources
 * @pdev_dp: Pointer to DP PDEV context for device access and statistics
 * @status_desc: Pointer to status descriptor containing TLV buffer information
 *
 * This function performs comprehensive cleanup of a monitor status descriptor
 * by flushing its TLV data and freeing all associated resources. It serves as
 * the primary cleanup function for monitor status buffers that contain TLV
 * data with embedded packet buffer references.
 *
 * The function orchestrates the complete cleanup process:
 * - Delegates TLV parsing and packet buffer cleanup to helper function
 * - Updates system statistics for monitoring buffer usage
 * - Frees the monitor status buffer itself using page fragment allocator
 * - Ensures all resources are properly released to prevent memory leaks
 *
 * This function is called in several scenarios:
 * - Normal PPDU processing completion when TLV data is no longer needed
 * - Error recovery when PPDU processing fails and cleanup is required
 * - Descriptor list flushing during system shutdown or error conditions
 * - Ring processing overflow when descriptors must be discarded
 *
 * Resource Management:
 * - Calls ath12k_dp_mon_tx_free_pkt_buf() for embedded packet buffer cleanup
 * - Updates mon_stats->status_buf_free counter for buffer tracking
 * - Uses page_frag_free() for efficient memory deallocation
 * - Ensures proper cleanup ordering to prevent use-after-free conditions
 *
 * Context: Called from work queue context or error handling paths.
 * Locking: Internal locking handled by called functions.
 * Memory: Frees both embedded packet buffers and the status buffer itself.
 */
static void
ath12k_dp_tx_mon_flush_tlv(struct ath12k_pdev_dp *pdev_dp,
			   struct ath12k_dp_mon_status_desc *status_desc)
{
	struct ath12k_pdev_tx_mon_stats *mon_stats;
	u8 *mon_buf = status_desc->mon_buf;
	u32 mon_buf_len = status_desc->buf_len;
	u8 *ptr = mon_buf;

	ath12k_dp_mon_tx_free_pkt_buf(pdev_dp, ptr, mon_buf_len);

	mon_stats = &pdev_dp->dp_mon_pdev->tx_mon_stats;
	mon_stats->tx_status_buf_free++;
	page_frag_free(mon_buf);
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
static void
ath12k_dp_tx_mon_flush_desc_list(struct ath12k_pdev_dp *dp_pdev,
				 struct list_head *mon_desc_list)
{
	struct ath12k_dp_mon_desc *tmp_desc, *entry_desc;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;
	struct ath12k_dp_mon_status_desc desc;

	list_for_each_entry_safe(entry_desc, tmp_desc,
				 mon_desc_list, list) {
		if (unlikely(!entry_desc->mon_buf))
			continue;

		ath12k_core_dma_unmap_page(dp_pdev->dp->dev, entry_desc->paddr,
					   ATH12K_DP_MON_TX_BUF_SIZE,
					   DMA_FROM_DEVICE);

		desc.mon_buf = entry_desc->mon_buf;
		desc.buf_len = entry_desc->buf_len;
		desc.end_of_ppdu = entry_desc->end_of_ppdu;

		ath12k_dp_tx_mon_flush_tlv(dp_pdev, &desc);

		entry_desc->mon_buf = NULL;
		entry_desc->buf_len = 0;
		entry_desc->end_of_ppdu = false;
	}

	/* Free descriptor list */
	ath12k_dp_mon_tx_desc_free(mon_desc_list, dp_mon);
}

/**
 * ath12k_dp_tx_mon_get_ppdu_desc() - Get PPDU descriptor from free list
 * @dp_mon_pdev: Monitor PDEV context
 *
 * Allocates a PPDU descriptor from the free list for use in TX monitor
 * processing. The descriptor is initialized and ready for use.
 *
 * Return: Pointer to PPDU descriptor on success, NULL if free list empty
 */
static struct ath12k_dp_mon_ppdu_desc *
ath12k_dp_tx_mon_get_ppdu_desc(struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;

	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	ppdu_desc = list_first_entry_or_null(&dp_mon_pdev->tx_mon_ppdu_desc_free_list,
					     struct ath12k_dp_mon_ppdu_desc, list);
	if (likely(ppdu_desc)) {
		list_del(&ppdu_desc->list);
		dp_mon_pdev->mon_stats.ppdu_desc_free--;
	}

	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);

	return ppdu_desc;
}

/**
 * ath12k_dp_tx_mon_prep_wq() - Prepare descriptors for work queue processing
 * @mon_desc_used_list: List of monitor descriptors containing TLV data
 * @dp_mon_pdev: Monitor PDEV context
 *
 * This function prepares monitor descriptors for work queue processing by:
 * 1. Allocating a PPDU descriptor from free list
 * 2. Associating monitor descriptors with the PPDU descriptor
 * 3. Queuing the PPDU descriptor for work queue processing
 *
 * This is called from NAPI context when end of PPDU is detected during
 * ring processing. The actual heavy processing is deferred to work queue.
 *
 * Return: 0 on success, -ENOENT if no PPDU descriptor available
 */
static int ath12k_dp_tx_mon_prep_wq(struct list_head *mon_desc_used_list,
				    struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_desc *desc;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	int desc_cnt;

	ppdu_desc = ath12k_dp_tx_mon_get_ppdu_desc(dp_mon_pdev);
	if (unlikely(!ppdu_desc)) {
		mon_stats->ppdu_desc_free_list_empty_cnt++;
		return -ENOENT;
	}

	/* Copy monitor descriptors to PPDU descriptor */
	list_for_each_entry(desc, mon_desc_used_list, list) {
		desc_cnt = ppdu_desc->status_desc_cnt;
		if (unlikely(desc_cnt >= ATH12K_DP_MON_STATUS_BUF)) {
			/* On overflow, reset and add to used list, return error */
			ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
			spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
			list_add_tail(&ppdu_desc->list,
				      &dp_mon_pdev->tx_mon_ppdu_desc_free_list);
			mon_stats->ppdu_desc_free++;
			spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
			return -EOVERFLOW;
		}

		/* Copy individual fields from descriptor to status_desc */
		ppdu_desc->status_desc[desc_cnt].mon_buf = desc->mon_buf;
		ppdu_desc->status_desc[desc_cnt].paddr = desc->paddr;
		ppdu_desc->status_desc[desc_cnt].buf_len = desc->buf_len;
		ppdu_desc->status_desc[desc_cnt].end_of_ppdu = desc->end_of_ppdu;
		ppdu_desc->status_desc_cnt++;
	}

	/* Add PPDU descriptor to used list for work queue processing */
	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	list_add_tail(&ppdu_desc->list, &dp_mon_pdev->tx_mon_ppdu_desc_used_list);
	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);

	return 0;
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

		mon_desc->in_use = DP_MON_DESC_STATUS_REAP;

		/* The hardware reports buffer length as (actual_length - 1),
		 * likely due to internal indexing or alignment constraints.
		 * To obtain the true buffer length for processing, increment
		 * the reported end_offset by 1 before using it.
		 */
		end_offset += 1;
		if (unlikely(end_offset > ATH12K_DP_MON_TX_BUF_SIZE)) {
			ath12k_warn(ab, "TX Mon: invalid offset %u received in mac_id %d\n",
				    end_offset, mac_id);
			end_offset = ATH12K_DP_MON_TX_BUF_SIZE - 1;
		}
		mon_desc->buf_len = end_offset;
		list_add_tail(&mon_desc->list, mon_desc_head);

		status_frag = (u8 *)mon_desc->mon_buf;
		if (unlikely(!status_frag)) {
			ath12k_err(ab, "TX Mon: NULL buffer received in mac_id %d\n",
				   mac_id);
			goto move_next;
		}

		if (end_reason == HAL_MON_FLUSH_DETECTED ||
		    end_reason == HAL_MON_PPDU_TRUNCATED) {
			ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
							 mon_desc_head);
			ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
				   "TX Mon: Flush Detected - Buffers Dropped\n");
			goto move_next;
		}

		if (end_reason == HAL_MON_END_OF_PPDU) {
			*budget -= 1;
			mon_desc->end_of_ppdu = true;
			ret = ath12k_dp_tx_mon_prep_wq(mon_desc_head,
						       dp_mon_pdev);
			if (ret) {
				ath12k_warn(ab,
					    "TX Mon: Failed to add mon desc to ppdu ret %d",
					    ret);
				ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
								 mon_desc_head);
				goto move_next;
			}

			if (queue_work(dp_mon_pdev->txmon_wq, &dp_mon_pdev->txmon_work))
				dp_mon_pdev->tx_mon_stats.tx_work_queue_scheduled++;

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
