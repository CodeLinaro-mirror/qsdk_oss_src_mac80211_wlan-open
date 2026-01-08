// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_flow_info.h"
#include "dp.h"
#include "../debug.h"
#include "dp_htt.h"
#include "dp_peer.h"
#include "dp_pool.h"

int ath12k_dp_tx_classify_info_alloc(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t *tx_classify_info_paddr,
				     void **tx_classify_info_vaddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	size_t alloc_size = ATH12K_NUM_TX_CLASSIFY_BANKS *
			    ATH12K_TX_CLASSIFY_INFO_SIZE_SINGLE;
	void *vaddr;
	dma_addr_t paddr;

	vaddr = kzalloc(alloc_size, GFP_ATOMIC);
	if (!vaddr)
		return -ENOMEM;

	paddr = ath12k_core_dma_map_single(dev, vaddr, alloc_size, DMA_BIDIRECTIONAL);
	if (!paddr) {
		kfree(vaddr);
		return -ENOMEM;
	}

	*tx_classify_info_vaddr = vaddr;
	*tx_classify_info_paddr = paddr;
	return 0;
}

void ath12k_dp_tx_classify_info_free(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t tx_classify_info_paddr,
				     void *tx_classify_info_vaddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	ath12k_core_dma_unmap_single(dev, tx_classify_info_paddr,
				     ATH12K_NUM_TX_CLASSIFY_BANKS *
				     ATH12K_TX_CLASSIFY_INFO_SIZE_SINGLE,
				     DMA_BIDIRECTIONAL);
	kfree(tx_classify_info_vaddr);
}

void ath12k_dp_pn_counter_page_free(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info = dp_hw_grp_wifi8->pn_page_info;
	int i;

	if (!pn_info || !dev)
		return;
	for (i = 0; i < dp_hw_grp_wifi8->num_pn_pages; i++) {
		if (!pn_info[i].vaddr) {
			ath12k_err(NULL, "pn_page %d is NULL\n", i);
			continue;
		}
		ath12k_core_dma_unmap_single(dev, pn_info[i].paddr,
					     PAGE_SIZE, DMA_BIDIRECTIONAL);
		kfree(pn_info[i].vaddr);
		pn_info[i].vaddr = NULL;
	}

	kfree(pn_info);
	dp_hw_grp_wifi8->pn_page_info = NULL;
}

int ath12k_dp_pn_counter_page_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info;
	u8 num_pn_pages = ATH12K_DP_PN_NUM_PAGE;
	int ret;
	int i;

	if (dp_hw_grp_wifi8->pn_page_info)
		return 0;

	pn_info = kcalloc(num_pn_pages, sizeof(struct ath12k_pn_page_info),
			  GFP_KERNEL);

	if (!pn_info) {
		ath12k_warn(ab, "Failed to allocate PN page info");
		return -ENOMEM;
	}

	for (i = 0; i < num_pn_pages; i++) {
		pn_info[i].vaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
		if (!pn_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		pn_info[i].paddr = ath12k_core_dma_map_single(dev,
							      pn_info[i].vaddr,
							      PAGE_SIZE,
							      DMA_BIDIRECTIONAL);
		if (!pn_info[i].paddr) {
			ath12k_warn(ab, "PN page DMA error");
			ret = -ENOMEM;
			goto free;
		}
		if (pn_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "PN page allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	dp_hw_grp_wifi8->num_pn_pages = num_pn_pages;
	dp_hw_grp_wifi8->pn_page_info = (void *)pn_info;
	return 0;

free:
	ath12k_dp_pn_counter_page_free(dp_hw_grp);
	return ret;
}

dma_addr_t ath12k_dp_get_page_paddr(struct ath12k_dp_hw_group *dp_hw_grp,
				    u16 sw_peer_id)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info = dp_hw_grp_wifi8->pn_page_info;
	u8 page_index = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_INDEX);
	u8 page_offset = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_OFFSET);
	dma_addr_t page_paddr;

	if (!pn_info)
		return 0;

	page_paddr = pn_info[page_index].paddr;
	page_offset = page_offset * ATH12K_DP_PN_COUNTER_SIZE;
	return (dma_addr_t)(((u8 *)page_paddr) + page_offset);
}

int ath12k_wifi8_dp_tx_pool_create(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u32 aligned_size;

	spin_lock_init(&dp_hw_grp_wifi8->tx_pool_lock);
	dp_hw_grp_wifi8->msduq_ctxt = init_memory_pool(ab, MSDU_STRUCT_SZ,
						       NUM_TOTAL_MSDU_QUEUES,
						       true);
	if (!dp_hw_grp_wifi8->msduq_ctxt)
		goto error;

	if (dma_map_pages(ab, dp_hw_grp_wifi8->msduq_ctxt)) {
		ath12k_err(ab, "DMA MAP failed for MSDUQ\n");
		goto error1;
	}

	aligned_size = sizeof(struct ath12k_dp_msdu_q_info);
	aligned_size = roundup_pow_of_two(aligned_size);
	dp_hw_grp_wifi8->sw_msduq_ctxt = init_memory_pool(ab, aligned_size,
							  NUM_TOTAL_MSDU_QUEUES,
							  true);
	if (!dp_hw_grp_wifi8->sw_msduq_ctxt)
		goto error1;

	dp_hw_grp_wifi8->mpduq_ctxt = init_memory_pool(ab, MPDU_STRUCT_SZ,
						       NUM_TOTAL_MPDU_QUEUES,
						       true);
	if (!dp_hw_grp_wifi8->mpduq_ctxt)
		goto error2;

	if (dma_map_pages(ab, dp_hw_grp_wifi8->mpduq_ctxt)) {
		ath12k_err(ab, "DMA MAP failed for MPDUQ\n");
		goto error3;
	}

	aligned_size = sizeof(struct ath12k_dp_mpdu_q_info);
	aligned_size = roundup_pow_of_two(aligned_size);
	dp_hw_grp_wifi8->sw_mpduq_ctxt = init_memory_pool(ab, aligned_size,
							  NUM_TOTAL_MPDU_QUEUES,
							  true);
	if (!dp_hw_grp_wifi8->sw_mpduq_ctxt)
		goto error3;

	return 0;
error3:
	pool_destroy(ab, dp_hw_grp_wifi8->mpduq_ctxt);
	dp_hw_grp_wifi8->mpduq_ctxt = NULL;
error2:
	pool_destroy(ab, dp_hw_grp_wifi8->sw_msduq_ctxt);
	dp_hw_grp_wifi8->sw_msduq_ctxt = NULL;
error1:
	pool_destroy(ab, dp_hw_grp_wifi8->msduq_ctxt);
	dp_hw_grp_wifi8->msduq_ctxt = NULL;
error:
	return -ENOMEM;
}

void ath12k_wifi8_dp_tx_pool_destroy(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (dp_hw_grp_wifi8->msduq_ctxt) {
		dma_unmap_pages(ab, dp_hw_grp_wifi8->msduq_ctxt);
		pool_destroy(ab, dp_hw_grp_wifi8->msduq_ctxt);
		dp_hw_grp_wifi8->msduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->sw_msduq_ctxt) {
		pool_destroy(ab, dp_hw_grp_wifi8->sw_msduq_ctxt);
		dp_hw_grp_wifi8->sw_msduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->mpduq_ctxt) {
		dma_unmap_pages(ab, dp_hw_grp_wifi8->mpduq_ctxt);
		pool_destroy(ab, dp_hw_grp_wifi8->mpduq_ctxt);
		dp_hw_grp_wifi8->mpduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->sw_mpduq_ctxt) {
		pool_destroy(ab, dp_hw_grp_wifi8->sw_mpduq_ctxt);
		dp_hw_grp_wifi8->sw_mpduq_ctxt = NULL;
	}
}

int ath12k_dp_tx_mcast_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct ath12k_dp_msdu_q_info *msduq;
	struct list_head mpduq_list_head;
	struct list_head msduq_list_head;
	int ret;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_list_head);
	INIT_LIST_HEAD(&msduq_list_head);
	spin_lock_bh(&tx_info->tx_q_lock);

	/* mcast mpdu queue */
	mpduq = tx_info->mcast_mpduq;
	if (mpduq && mpduq->mpduq_state == ATH12K_TX_Q_CREATED)
		list_add_tail(&mpduq->list, &mpduq_list_head);

	/* mcast msdu queue */
	msduq = tx_info->mcast_msduq;
	if (msduq && msduq->msduq_state == ATH12K_TX_Q_CREATED)
		list_add_tail(&msduq->list, &msduq_list_head);

	ret = ath12k_dp_tx_htt_peer_msduq_mpduq_setup(dp_hw_grp,
						      dp_peer,
						      &mpduq_list_head,
						      &msduq_list_head,
						      ATH12K_GROUP_MAX_RADIO,
						      true);
	spin_unlock_bh(&tx_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tx_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_dp_peer *dp_peer,
					u8 link_id)
{
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct list_head mpduq_pending_list_head;
	struct list_head msduq_pending_list_head;
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct ath12k_dp_msdu_q_info *msduq;
	int i;
	int j;
	int ret;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_pending_list_head);
	INIT_LIST_HEAD(&msduq_pending_list_head);
	spin_lock_bh(&tx_info->tx_q_lock);

	/* data tid queues */
	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		mpduq = tx_info->tid_info[i].mpduq;
		if (mpduq && mpduq->mpduq_state == ATH12K_TX_Q_CREATED)
			list_add_tail(&mpduq->list, &mpduq_pending_list_head);

		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			msduq = tx_info->tid_info[i].msduq[j];
			if (msduq && msduq->msduq_state == ATH12K_TX_Q_CREATED)
				list_add_tail(&msduq->list, &msduq_pending_list_head);
		}
	}

	ret = ath12k_dp_tx_htt_peer_msduq_mpduq_setup(dp_hw_grp, dp_peer,
						      &mpduq_pending_list_head,
						      &msduq_pending_list_head,
						      link_id,
						      false);
	spin_unlock_bh(&tx_info->tx_q_lock);
	return ret;
}
