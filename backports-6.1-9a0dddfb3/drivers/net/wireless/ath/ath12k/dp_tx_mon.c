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

	INIT_WORK(&dp_mon_pdev->txmon_work,
		  ath12k_dp_tx_mon_process_ppdu);
	dp_mon_pdev->tx_mon_wq_initialized = true;

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
 * ath12k_dp_mon_tx_prep_ppdu_info() - Prepare PPDU information for TX monitor processing
 * @dp_mon_pdev: Pointer to monitor PDEV context containing processing state
 * @ppdu_desc: Pointer to PPDU descriptor containing status descriptors with TLV data
 *
 * This function prepares PPDU information structures for TX monitor frame processing
 * by parsing the initial TLV data to extract user count and initializing the
 * processing contexts for both protection and data frames.
 *
 * The function performs the following key operations:
 * 1. Validates PPDU descriptor and status descriptor availability
 * 2. Parses the first TLV header to extract tag, length, and user ID information
 * 3. Determines the number of users in the PPDU from HAL TLV parsing
 * 4. Initializes protection and data PPDU info structures with user counts
 * 5. Initializes MPDU queues for all potential users (up to HAL_MAX_UL_MU_USERS)
 *
 * TLV Processing Details:
 * The function examines the first status descriptor's monitor buffer, which contains
 * TLV data from the hardware. It extracts the TLV header using HAL operations to
 * determine the TLV tag, user ID, and length. This information is then used to
 * parse the number of users in the PPDU through HAL-specific parsing functions.
 */
static int
ath12k_dp_mon_tx_prep_ppdu_info(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				struct ath12k_dp_mon_ppdu_desc *ppdu_desc)
{
	struct ath12k_mon_data *mon_data = &dp_mon_pdev->mon_data;
	struct ath12k_pdev_dp *dp_pdev = dp_mon_pdev->dp_pdev;
	u8 num_users = 0;
	struct ath12k_dp_mon_status_desc *status_desc;
	enum hal_tx_mon_status hal_status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;
	struct sk_buff_head *mpdu_q;
	struct hal_tlv_64_hdr *tlv_hdr;
	void *tlv_data;
	u16 tlv_tag, tlv_len;
	u32 tlv_userid = 0;
	int i;

	if (unlikely(!ppdu_desc->status_desc_cnt)) {
		ath12k_warn(dp_pdev->dp->ab, "status_desc_cnt %d ",
			    ppdu_desc->status_desc_cnt);
		return -EINVAL;
	}

	status_desc = &ppdu_desc->status_desc[0];
	if (unlikely(!status_desc || !status_desc->mon_buf)) {
		ath12k_warn(dp_pdev->dp->ab, "status desc %p , mon buf%p ",
			    status_desc, status_desc ? status_desc->mon_buf : 0);
		return -EINVAL;
	}

	if (status_desc->buf_len < sizeof(struct hal_tlv_64_hdr)) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: Buffer too small for TLV header: %u < %zu\n",
			    status_desc->buf_len, sizeof(struct hal_tlv_64_hdr));
		return -EINVAL;
	}

	tlv_hdr = (struct hal_tlv_64_hdr *)status_desc->mon_buf;
	ath12k_hal_get_tlv_params(dp_pdev->dp->hal, tlv_hdr->tl,
				  &tlv_tag, &tlv_userid, &tlv_len);

	if (sizeof(struct hal_tlv_64_hdr) + tlv_len > status_desc->buf_len) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: TLV length exceeds buffer: %u + %u > %u\n",
			    (u32)sizeof(struct hal_tlv_64_hdr),
			    tlv_len, status_desc->buf_len);
		return -EINVAL;
	}

	tlv_data = (u8 *)status_desc->mon_buf + sizeof(struct hal_tlv_64_hdr);

	hal_status = ath12k_hal_mon_tx_status_get_num_user(dp_pdev->dp->hal,
							   tlv_tag,
							   tlv_data,
							   &num_users,
							   tlv_len);
	if (hal_status == HAL_TX_MON_STATUS_PPDU_NOT_DONE || !num_users) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Failed to get num_users");
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "hal_status=%d, num_users=%u, tlv_tag=0x%x\n",
			   hal_status, num_users, tlv_tag);
		return -EINVAL;
	}

	mon_data->prot_ppdu_info.tx_info.num_users = 1;
	mon_data->data_ppdu_info.tx_info.num_users = num_users;

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		mpdu_q = mon_data->prot_ppdu_info.tx_info.rx_status.mpdu_q;
		skb_queue_head_init(&mpdu_q[i]);
		mpdu_q = mon_data->data_ppdu_info.tx_info.rx_status.mpdu_q;
		skb_queue_head_init(&mpdu_q[i]);
	}

	return 0;
}

/**
 * ath12k_dp_mon_tx_deep_free_ppdu_info() - Deep cleanup of PPDU info structures
 * @pdev_dp: Pointer to DP PDEV context for device-specific operations
 * @mon_data: Pointer to monitor data containing PPDU info structures to clean
 *
 * This function performs comprehensive cleanup of all MPDU socket buffers
 * queued in both protection and data PPDU information structures. It ensures
 * complete memory deallocation to prevent memory leaks during TX monitor
 * processing cleanup or error recovery scenarios.
 *
 * The function performs deep cleanup by:
 * 1. Iterating through all possible user queues (up to HAL_MAX_UL_MU_USERS)
 * 2. Dequeuing and freeing all MPDU socket buffers from data PPDU queues
 * 3. Dequeuing and freeing all MPDU socket buffers from protection PPDU queues
 * 4. Using dev_kfree_skb_any() for safe deallocation in any context
 */
static void
ath12k_dp_mon_tx_deep_free_ppdu_info(struct ath12k_pdev_dp *pdev_dp,
				     struct ath12k_mon_data *mon_data)
{
	int i;
	struct hal_tx_mon_ppdu_info *data_ppdu_info =
		&mon_data->data_ppdu_info.tx_info;
	struct hal_tx_mon_ppdu_info *prot_ppdu_info =
		&mon_data->prot_ppdu_info.tx_info;

	struct sk_buff_head *mpdu_q;
	struct sk_buff *mpdu;

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		mpdu_q = &data_ppdu_info->rx_status.mpdu_q[i];
		while ((mpdu = skb_dequeue(mpdu_q)))
			dev_kfree_skb_any(mpdu);
		mpdu_q = &prot_ppdu_info->rx_status.mpdu_q[i];
		while ((mpdu = skb_dequeue(mpdu_q)))
			dev_kfree_skb_any(mpdu);
	}
}

/**
 * ath12k_dp_tx_mon_update_stats() - Update comprehensive TX monitor statistics
 * @dp_pdev: DP PDEV context
 * @ppdu_info: PPDU information from TLV parsing
 *
 * This function updates detailed TX monitor statistics based on parsed PPDU
 * information. It tracks frame types, PHY modes, rates, transmission status,
 * and other detailed metrics for monitoring and debugging purposes.
 */
static void ath12k_dp_tx_mon_update_stats(struct ath12k_pdev_dp *dp_pdev,
					  struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon_stats *tx_stats;
	struct hal_tx_mon_ppdu_info *tx_info;

	if (unlikely(!dp_pdev || !ppdu_info))
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	if (unlikely(!dp_mon_pdev))
		return;

	tx_stats = &dp_mon_pdev->tx_mon_stats;
	tx_info = &ppdu_info->tx_info;

	tx_stats->tx_ppdu_processed++;

	if (tx_info->is_data)
		tx_stats->tx_data_frames++;

	if (tx_info->num_users > 1) {
		tx_stats->tx_mu_ppdu_count++;
		tx_stats->tx_mu_user_count += tx_info->num_users;
	} else {
		tx_stats->tx_su_ppdu_count++;
	}

	dp_mon_pdev->mon_stats.num_ppdu_processed++;
	if (tx_info->is_data)
		dp_mon_pdev->mon_stats.pkt_tlv_processed++;
}

/**
 * ath12k_dp_tx_mon_update_ampdu_info() - Update AMPDU aggregation information
 * @ppdu_info: Pointer to PPDU information structure to update
 * @user_idx: User index for multi-user scenarios (0-based)
 *
 * This function analyzes MPDU count and user status information to determine
 * if the current transmission represents an AMPDU (Aggregated MPDU) and sets
 * appropriate flags for radiotap header generation and frame analysis.
 *
 * AMPDU Detection Logic:
 * The function uses different detection methods based on transmission type:
 * - Single User: AMPDU detected when MPDU count > 1
 * - Multi User: AMPDU detected when per-user MPDU count > 1 OR user has AMPDU flag set
 *
 * For single user transmissions, the function examines the total MPDU count
 * in the PPDU. For multi-user transmissions, it analyzes per-user statistics
 * from the rx_user_status array to determine aggregation on a per-user basis.
 *
 * When AMPDU is detected, the function sets the RX_FLAG_AMPDU_DETAILS flag
 * in the rx_status structure, which is used by the radiotap generation code
 * to include appropriate AMPDU information in monitor mode frames.
 */
static void
ath12k_dp_tx_mon_update_ampdu_info(struct dp_mon_tx_ppdu_info *ppdu_info,
				   u8 user_idx)
{
	struct hal_tx_mon_ppdu_info *tx_info = &ppdu_info->tx_info;
	struct hal_rx_mon_ppdu_info *rx_status = &tx_info->rx_status;
	bool is_ampdu = false;
	u32 mpdu_count = 0;
	struct hal_rx_user_status *rx_user_status = tx_info->rx_status.userstats;

	if (tx_info->num_users == 1) {
		mpdu_count = ppdu_info->num_mpdu_fcs_ok;
		is_ampdu = (mpdu_count > 1);
	} else if (user_idx < tx_info->num_users) {
		mpdu_count = rx_user_status[user_idx].mpdu_cnt_fcs_ok;
		is_ampdu = (mpdu_count > 1) || rx_user_status[user_idx].is_ampdu;
	} else {
		return;
	}

	if (is_ampdu)
		rx_status->ampdu_flag |= RX_FLAG_AMPDU_DETAILS;
}

/**
 * ath12k_dp_tx_mon_gen_rts() - Generate RTS frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for address selection
 * @window_flag: Initiator window flag for address ordering
 *
 * Creates RTS frame with proper 802.11 header fields.
 * Radiotap space will be added separately during delivery processing.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_rts(struct ath12k_pdev_dp *pdev_dp,
			 struct dp_mon_tx_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *status_info,
			 u8 window_flag)
{
	struct sk_buff *skb;
	struct ieee80211_rts *rts;
	struct hal_tx_mon_ppdu_info *tx_info;
	u16 duration_le;
	u16 frame_control;
	size_t rts_frame_size = sizeof(struct ieee80211_rts);

	if (!ppdu_info || !status_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    rts_frame_size);
	if (!skb)
		return NULL;

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	rts = (struct ieee80211_rts *)skb_put_zero(skb, rts_frame_size);

	frame_control = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS;
	rts->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	rts->duration = duration_le;

	if (!status_info->protection_addr)
		status_info = &pdev_dp->dp_mon_pdev->mon_data.data_status_info;

	if (window_flag == INITIATOR_WINDOW) {
		memcpy(rts->ra, status_info->addr1, ETH_ALEN);
		memcpy(rts->ta, status_info->addr2, ETH_ALEN);
	} else {
		memcpy(rts->ra, status_info->addr2, ETH_ALEN);
		memcpy(rts->ta, status_info->addr1, ETH_ALEN);
	}

	tx_info->is_used = 1;
	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_cts2self() - Generate CTS frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for address selection
 * @window_flag: Window flag for address ordering
 *
 * Creates CTS-to-self frame for medium protection. Uses standard
 * CTS frame format with self-addressing.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_cts2self(struct ath12k_pdev_dp *pdev_dp,
			      struct dp_mon_tx_ppdu_info *ppdu_info,
			      struct hal_tx_mon_status_info *status_info,
			      u8 window_flag)
{
	struct sk_buff *skb;
	struct ieee80211_cts *cts;
	struct hal_tx_mon_ppdu_info *tx_info;
	u16 duration_le;
	u16 frame_control;
	size_t cts_frame_size = sizeof(struct ieee80211_cts);

	if (!ppdu_info || !status_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    cts_frame_size);
	if (!skb)
		return NULL;

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	cts = (struct ieee80211_cts *)skb_put_zero(skb,
						   cts_frame_size);

	frame_control = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS;
	cts->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	cts->duration = duration_le;

	if (window_flag != INITIATOR_WINDOW)
		status_info = &pdev_dp->dp_mon_pdev->mon_data.data_status_info;

	memcpy(cts->ra, status_info->addr2, ETH_ALEN);

	tx_info->is_used = 1;
	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_qos_null_3addr() - Generate 3-address QoS NULL frame
 * @pdev_dp: Pointer to DP PDEV context for device-specific operations
 * @ppdu_info: PPDU info structure
 *
 * Creates 3-address QoS NULL frame for medium protection. Uses standard
 * 3-address data frame format without window-based logic.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_qos_null_3addr(struct ath12k_pdev_dp *pdev_dp,
				    struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr *qos_null;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	u16 duration_le;
	u16 frame_control;
	size_t qos_null_frame_size = sizeof(struct ieee80211_qos_hdr);

	if (!ppdu_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    qos_null_frame_size);
	if (!skb)
		return NULL;

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	qos_null = (struct ieee80211_qos_hdr *)skb_put_zero(skb,
							    qos_null_frame_size);

	frame_control = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC;
	qos_null->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	qos_null->duration_id = duration_le;

	status_info = &pdev_dp->dp_mon_pdev->mon_data.data_status_info;
	memcpy(qos_null->addr1, status_info->addr1, ETH_ALEN);
	memcpy(qos_null->addr2, status_info->addr2, ETH_ALEN);
	memcpy(qos_null->addr3, status_info->addr3, ETH_ALEN);

	qos_null->qos_ctrl = cpu_to_le16(0);

	tx_info->is_used = 1;

	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_qos_null_4addr() - Generate 4-address QoS NULL frame
 * @pdev_dp: Pointer to DP PDEV context for device-specific operations
 * @ppdu_info: PPDU info structure
 *
 * Creates 4-address QoS NULL frame for medium protection. Uses standard
 * 4-address data frame format without window-based logic.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_qos_null_4addr(struct ath12k_pdev_dp *pdev_dp,
				    struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr_4addr *qos_null_4addr;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	u16 duration_le;
	u16 frame_control;
	size_t frame_size = sizeof(struct ieee80211_qos_hdr_4addr);

	if (!ppdu_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    frame_size);
	if (!skb)
		return NULL;

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	qos_null_4addr = (struct ieee80211_qos_hdr_4addr *)skb_put_zero(skb,
									frame_size);

	frame_control = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC |
		IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS;
	qos_null_4addr->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	qos_null_4addr->duration_id = duration_le;

	status_info = &pdev_dp->dp_mon_pdev->mon_data.data_status_info;

	memcpy(qos_null_4addr->addr1, status_info->addr1, ETH_ALEN);
	memcpy(qos_null_4addr->addr2, status_info->addr2, ETH_ALEN);
	memcpy(qos_null_4addr->addr3, status_info->addr3, ETH_ALEN);
	memcpy(qos_null_4addr->addr4, status_info->addr2, ETH_ALEN);

	qos_null_4addr->qos_ctrl = cpu_to_le16(0);
	tx_info->is_used = 1;

	return skb;
}

/**
 * ath12k_dp_tx_mon_generate_prot_frm() - Generate protection frame
 * @pdev_dp: DP pdev handle
 * @tx_prot_ppdu_info: Protection PPDU information
 *
 * Generates protection frames (RTS/CTS) based on protection type
 * specified in PPDU information. The generated frame is added to
 * the protection PPDU's MPDU queue for delivery to monitor stack.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_generate_prot_frm(struct ath12k_pdev_dp *pdev_dp,
				   struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info)
{
	struct sk_buff *skb = NULL;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct sk_buff_head *mpdu_q;
	struct hal_tx_mon_status_info *status_info;
	u32 protection_type;
	u8 window_flag = INITIATOR_WINDOW;

	tx_info = &tx_prot_ppdu_info->tx_info;
	mpdu_q = &tx_info->rx_status.mpdu_q[0];

	status_info = &pdev_dp->dp_mon_pdev->mon_data.prot_status_info;
	protection_type = status_info->medium_prot_type;

	switch (protection_type) {
	case DP_MON_TX_MEDIUM_NO_PROTECTION:
		tx_info->is_used = 0;
		return 0;
	case DP_MON_TX_MEDIUM_RTS_LEGACY:
	case DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW:
	case DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW:
		skb = ath12k_dp_tx_mon_gen_rts(pdev_dp,
					       tx_prot_ppdu_info,
					       status_info,
					       window_flag);
		break;

	case DP_MON_TX_MEDIUM_CTS2SELF:
		skb = ath12k_dp_tx_mon_gen_cts2self(pdev_dp,
						    tx_prot_ppdu_info,
						    status_info,
						    window_flag);
		break;

	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR:
		skb = ath12k_dp_tx_mon_gen_qos_null_3addr(pdev_dp,
							  tx_prot_ppdu_info);
		break;

	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR:
		skb = ath12k_dp_tx_mon_gen_qos_null_4addr(pdev_dp,
							  tx_prot_ppdu_info);
		break;

	default:
		ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: No protection frame needed, type=%u\n",
			   protection_type);
		return 0;
	}

	if (!skb) {
		ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: Failed to generate protection frame type %u\n",
			   protection_type);
		return -ENOMEM;
	}

	skb_queue_tail(mpdu_q, skb);
	tx_prot_ppdu_info->contains_host_frames = true;

	ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX monitor: Generated protection frame type %u\n",
		   protection_type);

	return 0;
}

/**
 * ath12k_dp_tx_mon_free_last_mpdu_q() - Free incomplete MPDU from queue
 * @dp_mon_pdev: Monitor pdev context
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * Frees the last incomplete MPDU from the specified user's queue.
 */
void ath12k_dp_tx_mon_free_last_mpdu_q(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				       struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				       u32 usr_idx)
{
	struct hal_rx_mon_mpdu_info *mpdu_info;
	struct sk_buff_head *mpdu_q;
	struct sk_buff *skb;

	if (!dp_mon_pdev || !tx_ppdu_info)
		return;

	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];
	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];

	/*
	 * Only clean up if MPDU end was not received
	 * This indicates an incomplete MPDU that needs cleanup
	 */
	if (!mpdu_info->mpdu_end_received) {
		skb = skb_dequeue_tail(mpdu_q);
		if (skb)
			dev_kfree_skb_any(skb);

		/* Mark MPDU as ended to prevent further processing */
		mpdu_info->mpdu_end_received = true;
		mpdu_info->mpdu_start_received = false;
	}
}

/**
 * ath12k_dp_tx_mon_process_mpdu_start() - Process MPDU start TLV
 * @dp_pdev: DP pdev handle
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_process_mpdu_start(struct ath12k_pdev_dp *dp_pdev,
				    struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				    u32 usr_idx)
{
	struct sk_buff_head *mpdu_q;
	struct sk_buff *skb;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;

	if (usr_idx >= HAL_MAX_UL_MU_USERS) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: Invalid user index %u >= %u\n",
			    usr_idx, HAL_MAX_UL_MU_USERS);
		return -EINVAL;
	}

	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];
	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];

	if (!mpdu_info->mpdu_end_received) {
		skb = skb_dequeue_tail(mpdu_q);
		if (likely(skb))
			dev_kfree_skb_any(skb);
	}

	mpdu_info->mpdu_start_received = true;
	mpdu_info->mpdu_end_received = false;

	skb = dev_alloc_skb(ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);
	if (unlikely(!skb)) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: SKB allocation failed for user %u\n",
			    usr_idx);
		mpdu_info->mpdu_start_received = false;
		return -ENOMEM;
	}

	skb_reserve(skb, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);
	tx_ppdu_info->contains_host_frames = false;
	skb_queue_tail(mpdu_q, skb);

	ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Mon: MPDU header SKB allocated for user %u (reserved=%u)\n",
		   usr_idx, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);

	return 0;
}

/**
 * ath12k_dp_tx_mon_generate_data_frm() - Generate data frame with fragments
 * @ppdu_info: Data PPDU information
 * @user_idx: User index
 * @take_ref: Whether to take reference on buffer page
 *           - true: Increment page reference count (buffer may be reused)
 *           - false: Transfer page ownership (buffer consumed)
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_dp_tx_mon_generate_data_frm(struct dp_mon_tx_ppdu_info *ppdu_info,
				       u8 user_idx, bool take_ref)
{
	struct sk_buff *skb;
	struct hal_rx_mon_ppdu_info *rx_status;
	struct sk_buff_head *mpdu_q;
	struct hal_tx_mon_ppdu_info *tx_info;
	void *buffer_addr;
	u32 buffer_length;
	struct page *page;
	int frag_offset;

	if (!ppdu_info || user_idx >= HAL_MAX_UL_MU_USERS)
		return -EINVAL;

	tx_info = &ppdu_info->tx_info;
	rx_status = &tx_info->rx_status;
	mpdu_q = &rx_status->mpdu_q[user_idx];

	skb = skb_peek_tail(mpdu_q);
	if (!skb)
		return -ENOENT;

	if (ppdu_info->has_buffer_data && ppdu_info->buffer_addr) {
		buffer_addr = ppdu_info->buffer_addr;
		buffer_length = ppdu_info->buffer_length;

		page = virt_to_head_page(buffer_addr);
		frag_offset = buffer_addr - page_address(page);

		skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
				page,
				frag_offset,
				buffer_length,
				ATH12K_DP_MON_TX_BUF_SIZE);

		if (take_ref)
			skb_frag_ref(skb, skb_shinfo(skb)->nr_frags);
	}
	return 0;
}

/**
 * ath12k_dp_tx_mon_extract_buffer_info() - Extract buffer information only
 * @dp_mon: DP monitor handle
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * This function ONLY extracts buffer info and stores it in ppdu_info.
 * It does NOT add fragments - that's done by ath12k_dp_tx_mon_generate_data_frm().
 * Incorporates robust validation and error handling from original implementation.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_extract_buffer_info(struct ath12k_dp_mon *dp_mon,
				     struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				     u32 usr_idx)
{
	struct hal_tx_mon_ppdu_info *hal_info;
	struct hal_tx_mon_packet_info *packet_info = NULL;
	struct list_head mon_desc_used_list;
	struct ath12k_dp_mon_desc *mon_desc;
	struct sk_buff_head *mpdu_q;
	struct sk_buff *header_skb;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	int ret = 0;

	INIT_LIST_HEAD(&mon_desc_used_list);

	if (usr_idx >= HAL_MAX_UL_MU_USERS) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid user index %u >= %u\n",
			    usr_idx, HAL_MAX_UL_MU_USERS);
		return -EINVAL;
	}

	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];

	if (!mpdu_info->mpdu_start_received) {
		ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Buffer without MPDU start for user %u\n",
			   usr_idx);
		return -EINVAL;
	}

	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];

	hal_info = &tx_ppdu_info->tx_info;
	packet_info = &hal_info->packet_info;

	if (!packet_info || !packet_info->sw_cookie) {
		tx_ppdu_info->has_buffer_data = false;
		ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: No packet buffer info for user %u\n",
			   usr_idx);
		return 0;
	}

	mon_desc = (struct ath12k_dp_mon_desc *)(uintptr_t)(packet_info->sw_cookie);

	if (unlikely(mon_desc->magic != ATH12K_MON_MAGIC_VALUE)) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid magic value 0x%x for user %u\n",
			    mon_desc->magic, usr_idx);
		ret = -EINVAL;
		goto return_mon_desc;
	}

	if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid descriptor state %d for user %u\n",
			    mon_desc->in_use, usr_idx);
		ret = -EINVAL;
		goto return_mon_desc;
	}

	mon_desc->in_use = DP_MON_DESC_REPLENISH;
	list_add_tail(&mon_desc->list, &mon_desc_used_list);
	tx_ppdu_info->buffer_addr = mon_desc->mon_buf;
	mon_desc->mon_buf = NULL;

	if (!tx_ppdu_info->buffer_addr) {
		ret = -EINVAL;
		goto return_mon_desc;
	}

	if (packet_info->dma_length > ATH12K_DP_MON_TX_BUF_SIZE) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid DMA length %u for user %u\n",
			    packet_info->dma_length, usr_idx);
		ret = -EINVAL;
		page_frag_free(tx_ppdu_info->buffer_addr);
		goto return_mon_desc;
	}

	header_skb = skb_peek_tail(mpdu_q);
	if (!header_skb) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: No header SKB in queue for user %u\n",
			    usr_idx);
		ret = -EINVAL;
		page_frag_free(tx_ppdu_info->buffer_addr);
		goto return_mon_desc;
	}

	ath12k_core_dma_unmap_page(dp_mon->dp->dev, mon_desc->paddr,
				   ATH12K_DP_MON_TX_BUF_SIZE,
				   DMA_FROM_DEVICE);

	tx_ppdu_info->buffer_length = packet_info->dma_length;
	tx_ppdu_info->msdu_continuation = packet_info->msdu_continuation;
	tx_ppdu_info->truncated = packet_info->truncated;
	tx_ppdu_info->has_buffer_data = true;

return_mon_desc:

	ath12k_dp_mon_tx_desc_free(&mon_desc_used_list, dp_mon);
	return ret;
}

/**
 * ath12k_dp_tx_mon_update_ppdu_info_status() - Update PPDU info based on TLV status
 * @pdev_dp: DP pdev handle
 * @tx_ppdu_info: Could be either of below
 * tx_data_ppdu_info: Data PPDU information structure
 * tx_prot_ppdu_info: Protection PPDU information structure
 * @tx_tlv_hdr: TLV header pointer
 * @status_frag: Status fragment buffer
 * @tlv_status: TLV status from HAL parsing
 *
 * This function processes different TLV status types and performs
 * appropriate actions including frame generation, buffer management,
 * and PPDU information updates. It serves as the central dispatcher
 * for TLV-based processing in TX monitor.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_update_ppdu_info_status(struct ath12k_pdev_dp *pdev_dp,
					 struct dp_mon_tx_ppdu_info *tx_ppdu_info,
					 u32 tlv_status)
{
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	u32 usr_idx;
	int ret = 0;

	if (unlikely(!tx_ppdu_info))
		return -EINVAL;

	tx_info = &tx_ppdu_info->tx_info;
	usr_idx = tx_info->cur_usr_idx;

	switch (tlv_status) {
	case HAL_TX_MON_FES_SETUP:
		break;

	case HAL_TX_MON_RESPONSE_REQUIRED_INFO:
		break;

	case HAL_TX_MON_FES_STATUS_START_PROT:
		break;

	case HAL_TX_MON_FES_STATUS_START_PPDU:
		break;

	case HAL_TX_MON_FES_STATUS_PROT:
		tx_ppdu_info->tx_info.rx_status.ppdu_ts =
			tx_ppdu_info->tx_info.rx_status.ppdu_ts << 1;

		ret = ath12k_dp_tx_mon_generate_prot_frm(pdev_dp,
							 tx_ppdu_info);
		break;

	case HAL_TX_MON_MPDU_START:
		ret = ath12k_dp_tx_mon_process_mpdu_start(pdev_dp,
							  tx_ppdu_info,
							  usr_idx);
		if (ret) {
			ath12k_warn(pdev_dp->dp->ab,
				    "TX Mon: Failed to process MPDU start: %d\n",
				    ret);
		}
		break;

	case HAL_TX_MON_MPDU_END:
		/* MPDU end - mark MPDU as complete */
		if (usr_idx < HAL_MAX_UL_MU_USERS) {
			mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];
			mpdu_info->mpdu_end_received = true;
			mpdu_info->mpdu_start_received = false;
		}
		break;

	case HAL_TX_MON_MSDU_START:
		/* MSDU start processing */
		break;

	case HAL_TX_MON_DATA:
		/* Data frame generation - keep buffer reference for reuse */
		tx_info->is_used = 1;
		ret = ath12k_dp_tx_mon_generate_data_frm(tx_ppdu_info,
							 usr_idx, true);
		if (ret) {
			ath12k_warn(pdev_dp->dp->ab,
				    "TX Mon: Failed to gen data frm for user %u: %d\n",
				    usr_idx, ret);
			tx_info->is_used = 0;
		}
		break;

	case HAL_TX_MON_BUFFER_ADDR:
		/*
		 * Buffer address processing - extract buffer and transfer ownership
		 */
		tx_info->is_used = 1;
		ret = ath12k_dp_tx_mon_extract_buffer_info(pdev_dp->dp_mon_pdev->dp_mon,
							   tx_ppdu_info, usr_idx);
		if (ret) {
			ath12k_warn(pdev_dp->dp->ab,
				    "TX Mon: Failed to extract buffer info: %d\n",
				    ret);
			tx_info->is_used = 0;
		} else {
			ret = ath12k_dp_tx_mon_generate_data_frm(tx_ppdu_info,
								 usr_idx, false);
			if (ret) {
				ath12k_warn(pdev_dp->dp->ab,
					    "TX Mon: Failed to add buffer fragment: %d\n",
					    ret);
				tx_info->is_used = 0;
			}
		}
		break;

	case HAL_TX_MON_FES_STATUS_END:
		/*
		 * FES status end - clean up incomplete MPDUs for all users
		 */
		u32 num_users = tx_ppdu_info->tx_info.num_users;
		u8 i;

		for (i = 0; i < num_users && i < HAL_MAX_UL_MU_USERS; i++) {
			ath12k_dp_tx_mon_free_last_mpdu_q(pdev_dp->dp_mon_pdev,
							  tx_ppdu_info,
							  i);
		}
		break;

	case HAL_TX_MON_FW2SW:
		status_info = &pdev_dp->dp_mon_pdev->mon_data.data_status_info;
		tx_ppdu_info->tx_info.rx_status.freq = status_info->freq;

		break;

	default:
		break;
	}

	return ret;
}

/**
 * ath12k_dp_mon_tx_process_tlv() - Process TLV data with early filtering
 * @pdev_dp: DP PDEV context
 * @status_desc: Status descriptor containing TLV data
 * @mon_data: Monitor data structure
 *
 * This function parses TLV data using HAL functions and applies early software
 * filtering during TLV processing for optimal performance.
 */
static void
ath12k_dp_mon_tx_process_tlv(struct ath12k_pdev_dp *pdev_dp,
			     struct ath12k_dp_mon_status_desc *status_desc,
			     struct ath12k_mon_data *mon_data)
{
	struct dp_mon_tx_ppdu_info *ppdu_info;
	struct hal_tlv_64_hdr *tlv_hdr;
	u16 tlv_tag, tlv_len, tlv_userid;
	u16 buf_len = status_desc->buf_len;
	u8 *tx_tlv_start = status_desc->mon_buf;
	u8 *mon_buf_iter = status_desc->mon_buf;
	enum hal_tx_mon_status tlv_status;
	int ret;

	do {
		tlv_hdr = (struct hal_tlv_64_hdr *)mon_buf_iter;

		tlv_tag = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_HDR_TAG);
		tlv_len = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_HDR_LEN);
		tlv_userid = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_USR_ID);

		ppdu_info = ath12k_hal_mon_tx_ppdu_info(&pdev_dp->dp->ab->hal,
							mon_data, tlv_tag);

		tlv_status =
			ath12k_hal_mon_tx_parse_status(&pdev_dp->dp->ab->hal,
						       mon_data,
						       &ppdu_info->tx_info,
						       tlv_tag,
						       mon_buf_iter + sizeof(*tlv_hdr),
						       tlv_userid, tlv_len,
						       tx_tlv_start);

		/* Process TLV status and update PPDU information */
		ret = ath12k_dp_tx_mon_update_ppdu_info_status(pdev_dp,
							       ppdu_info,
							       tlv_status);
		if (ret) {
			ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "TLV status processing failed: %d", ret);
		}

		mon_buf_iter += sizeof(*tlv_hdr) + tlv_len;
		mon_buf_iter = PTR_ALIGN(mon_buf_iter, HAL_TLV_64_ALIGN);

	} while ((mon_buf_iter - tx_tlv_start) < buf_len);
}

/**
 * ath12k_dp_mon_tx_populate_ppdu_info() - Populate PPDU with channel and metadata
 * @dp_pdev: Pointer to DP PDEV context for accessing radio information
 * @status_desc: Pointer to status descriptor (currently unused but reserved)
 * @mon_data: Pointer to monitor data containing PPDU information structures
 *
 * This function populates PPDU information structures with essential metadata
 * including channel information, band classification, and AMPDU detection for
 * both protection and data frame PPDUs. It serves as the central point for
 * enriching hardware-provided PPDU data with software-derived information.
 *
 * Channel Information Population:
 * The function extracts current channel information from the active radio
 * context (ar->rx_channel) and populates both data and protection PPDU
 * structures with:
 * - Channel frequency in MHz for radiotap channel field
 * - Channel number for protocol analysis
 * - Band classification (2.4GHz, 5GHz, 6GHz) for proper frame handling
 */
static void
ath12k_dp_mon_tx_populate_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_mon_status_desc *status_desc,
				    struct ath12k_mon_data *mon_data)
{
	struct dp_mon_tx_ppdu_info *data_ppdu_info = &mon_data->data_ppdu_info;
	struct dp_mon_tx_ppdu_info *prot_ppdu_info = &mon_data->prot_ppdu_info;
	struct hal_tx_mon_ppdu_info *data_hal_info = &data_ppdu_info->tx_info;
	struct hal_tx_mon_ppdu_info *prot_hal_info = &prot_ppdu_info->tx_info;
	struct hal_rx_mon_ppdu_info *data_rx_status = &data_hal_info->rx_status;
	struct hal_rx_mon_ppdu_info *prot_rx_status = &prot_hal_info->rx_status;
	struct ath12k *ar = dp_pdev->ar;
	u32 usr_idx, num_users;

	num_users = data_hal_info->num_users;
	if (ar && ar->rx_channel) {
		u32 chan_freq = ar->rx_channel->center_freq;
		u32 chan_num = ar->rx_channel->hw_value;
		u32 band;

		if (chan_freq >= 2412 && chan_freq <= 2484)
			band = NL80211_BAND_2GHZ;
		else if (chan_freq >= 5170 && chan_freq <= 5895)
			band = NL80211_BAND_5GHZ;
		else if (chan_freq >= 5925 && chan_freq <= 7125)
			band = NL80211_BAND_6GHZ;
		else
			band = NL80211_BAND_2GHZ;

		if (unlikely(!data_rx_status->freq)) {
			data_ppdu_info->chan_freq = chan_freq;
			data_ppdu_info->chan_num = chan_num;
			data_rx_status->freq = chan_freq;
			data_rx_status->chan_num = chan_num;
			data_rx_status->band = band;
		}

		if (unlikely(!prot_rx_status->freq)) {
			prot_ppdu_info->chan_freq = chan_freq;
			prot_ppdu_info->chan_num = chan_num;
			prot_rx_status->freq = chan_freq;
			prot_rx_status->chan_num = chan_num;
			prot_rx_status->band = band;
		}
	}

	if (num_users == 1) {
		ath12k_dp_tx_mon_update_ampdu_info(data_ppdu_info, 0);
	} else {
		for (usr_idx = 0;
		     usr_idx < num_users && usr_idx < HAL_MAX_UL_MU_USERS;
		     usr_idx++) {
			ath12k_dp_tx_mon_update_ampdu_info(data_ppdu_info, usr_idx);
		}
	}
}

/**
 * ath12k_dp_tx_mon_process_ppdu() - Work queue handler for TX monitor
 * @work: Work structure containing the monitor pdev context
 *
 * This function processes PPDU descriptors in work queue context, performing
 * heavy TLV parsing, software filtering, frame generation, and stack delivery.
 * It follows the efficient batch processing pattern while adding
 * comprehensive error handling and bridge functions for complete functionality.
 *
 * Processing Flow:
 * 1. Move descriptors from used to processing list (batch processing)
 * 2. Process each PPDU with validation and error handling
 * 3. Perform TLV parsing and software filtering
 * 4. Generate frames and deliver to stack
 * 5. Return descriptors to free pool for reuse
 */
void ath12k_dp_tx_mon_process_ppdu(struct work_struct *work)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev =
		container_of(work, struct ath12k_pdev_mon_dp, txmon_work);
	struct ath12k_pdev_dp *pdev_dp = dp_mon_pdev->dp_pdev;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_status_desc *status_desc;
	struct ath12k_mon_data *mon_data = &dp_mon_pdev->mon_data;
	struct ath12k_pdev_tx_mon_stats *tx_stats = &dp_mon_pdev->tx_mon_stats;
	int desc_idx, desc_count = 0;
	int ppdu_processed = 0;
	int total_status_desc = 0, prep_failed = 0;

	if (unlikely(!pdev_dp || !dp_mon_pdev)) {
		ath12k_err(pdev_dp ? pdev_dp->dp->ab : NULL,
			   "TX Mon: Invalid parameters in work queue\n");
		return;
	}

	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	list_splice_init(&dp_mon_pdev->tx_mon_ppdu_desc_used_list,
			 &dp_mon_pdev->tx_mon_ppdu_desc_proc_list);
	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);

	list_for_each_entry(ppdu_desc,
			    &dp_mon_pdev->tx_mon_ppdu_desc_proc_list,
			    list) {
		if (unlikely(ppdu_desc->status_desc_cnt == 0)) {
			ath12k_warn(pdev_dp->dp->ab,
				    "TX Mon: Invalid PPDU desc or zero status count\n");
			tx_stats->tx_ppdu_desc_invalid++;
			desc_count++;
			continue;
		}

		if (unlikely(ppdu_desc->status_desc_cnt > ATH12K_DP_MON_STATUS_BUF)) {
			ath12k_warn(pdev_dp->dp->ab,
				    "TX Mon: PPDU desc overflow count=%u max=%u\n",
				    ppdu_desc->status_desc_cnt, ATH12K_DP_MON_STATUS_BUF);
			tx_stats->tx_ppdu_desc_overflow++;
			ppdu_desc->status_desc_cnt = ATH12K_DP_MON_STATUS_BUF;
			tx_stats->tx_work_queue_stalls++;
		}

		if (ath12k_dp_mon_tx_prep_ppdu_info(dp_mon_pdev, ppdu_desc)) {
			tx_stats->tx_ppdu_parse_errors++;
			prep_failed++;
			goto ppdu_prep_failed;
		}

		for (desc_idx = 0; desc_idx < ppdu_desc->status_desc_cnt; desc_idx++) {
			status_desc = &ppdu_desc->status_desc[desc_idx];

			if (unlikely(!status_desc->mon_buf)) {
				ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: Null buffer address in ppdu desc idx=%d\n",
					   desc_idx);
				tx_stats->tx_status_buf_null++;
				continue;
			}

			ath12k_core_dma_unmap_page(pdev_dp->dp->dev,
						   status_desc->paddr,
						   ATH12K_DP_MON_TX_BUF_SIZE,
						   DMA_FROM_DEVICE);

			ath12k_dp_mon_tx_process_tlv(pdev_dp,
						     status_desc, mon_data);
			ath12k_dp_mon_tx_populate_ppdu_info(pdev_dp,
							    status_desc,
							    mon_data);

			if (status_desc->end_of_ppdu) {
				ath12k_dp_tx_mon_update_stats(pdev_dp,
							      &mon_data->data_ppdu_info);
			}

			page_frag_free(status_desc->mon_buf);
			status_desc->mon_buf = NULL;
			status_desc->paddr = 0;
			status_desc->buf_len = 0;
			status_desc->end_of_ppdu = false;
			total_status_desc++;
		}
		ppdu_processed++;

ppdu_prep_failed:
		desc_count++;
		/* Reset PPDU descriptor for reuse */
		ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
		/* Deep cleanup of PPDU info */
		ath12k_dp_mon_tx_deep_free_ppdu_info(pdev_dp, mon_data);
	}

	spin_lock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);
	list_splice_tail_init(&dp_mon_pdev->tx_mon_ppdu_desc_proc_list,
			      &dp_mon_pdev->tx_mon_ppdu_desc_free_list);
	dp_mon_pdev->mon_stats.ppdu_desc_free += desc_count;
	spin_unlock_bh(&dp_mon_pdev->tx_mon_ppdu_desc_lock);

	/* Update statistics */
	tx_stats->tx_ppdu_processed += ppdu_processed;
	tx_stats->tx_status_desc_processed += total_status_desc;

	ath12k_dbg(pdev_dp->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Mon: Work queue processed %d PPDUs, %d status descriptors\n",
		   ppdu_processed, total_status_desc);
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
