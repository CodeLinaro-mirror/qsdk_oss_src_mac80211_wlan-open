// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_queue.h"
#include "dp_pool.h"
#include "../debug.h"
#include "dp.h"
#include "dp_tx_flow_info.h"
#include "dp_tx_tid.h"

#define ATH12K_MAX_AMSDU_AGGR_LIMIT 256
#define ATH12K_WIFI_DESC_HARD_DROP_THRESHOLD 10240
#define ATH12K_FRAME_HEADER_SIZE 24
#define ATH12K_QOS_FRAME_HEADER_SIZE 26
#define ATH12K_MAX_NUM_OF_EXT_DESCRIPTORS 24
#define ATH12K_MPDU_SEQ_NUM_MASK 0xFFF
#define ATH12K_PN_ADDR_HIGH_BYTE_MASK 0x000000FF
#define ATH12K_CHIP0_BITMAP_MASK 0x1
#define ATH12K_CHIP1_BITMAP_MASK 0x2
#define ATH12K_CHIP2_BITMAP_MASK 0x4
#define ATH12K_CHIP3_BITMAP_MASK 0x8

int ath12k_wifi8_hal_tx_msdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 msduq_idx,
					 struct hal_tx_msdu_flow_info *ti)
{
	struct hal_tx_msdu_flow *msduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	msduq = pool_node_from_id(dp_hw_grp_wifi8->msduq_ctxt, msduq_idx);
	if (!msduq) {
		ath12k_err(ab, "MSDUQ flow is NULL\n");
		return -ENOMEM;
	}

	ath12k_core_dma_sync_single_for_cpu(dev, ti->paddr,
					    MSDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
	if (ath12k_wifi8_hal_msduq_is_valid(msduq)) {
		ath12k_err(ab, "MSDUQ flow %pK already valid\n", msduq);
		ath12k_wifi8_hal_msduq_set_invalid(dp_hw_grp, msduq, ti->paddr);
		return -EEXIST;
	}

	memset(msduq, 0, sizeof(*msduq));
	msduq->header.info0 = le32_encode_bits(HAL_WIFITQM_OWNED,
					       HAL_TX_DESCRIPTOR_HEADER_OWNER) |
			      le32_encode_bits(HAL_WIFITRANSMIT_FLOW_DESCRIPTOR,
					       HAL_TX_DESCRIPTOR_HEADER_BUFFER_TYPE) |
			      le32_encode_bits(ti->queue_number,
					       HAL_TX_DESCRIPTOR_HEADER_QUEUE_NUMBER);
	msduq->info0 = le32_encode_bits(1, HAL_TX_MSDU_FLOW_FLOW_VALID) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_EMPTY_TO_N_EMPTY) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_N_EMPTY_TO_EMPTY) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION2) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_ASSOCIATED_LINK_DESC_COUNT) |
		       le32_encode_bits(ti->tid, HAL_TX_MSDU_FLOW_TID) |
		       le32_encode_bits(ti->mlo, HAL_TX_MSDU_FLOW_MLO_VALID);
	msduq->info1 = le32_encode_bits((ATH12K_MAX_AMSDU_AGGR_LIMIT / 3),
					HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES);
	msduq->info3 = le32_encode_bits(ti->peer_id, HAL_TX_MSDU_FLOW_SW_PEER_ID) |
		       le32_encode_bits(((2 * ATH12K_MAX_AMSDU_AGGR_LIMIT) / 3),
					HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES_2);
	msduq->info9 = le32_encode_bits(HAL_WIFIDROP_ENABLED,
					HAL_TX_MSDU_FLOW_DROP_RULE) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP0_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP0) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP1_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP1) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_SLOW_DROP_NOTIFICATION) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_MED_DROP_NOTIFICATION) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_HARD_DROP_NOTIFICATION) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP2_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP2) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP3_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP3) |
		       le32_encode_bits(ATH12K_WIFI_DESC_HARD_DROP_THRESHOLD,
					HAL_TX_MSDU_FLOW_SLOW_DROP_THRESHOLD);
	msduq->info10 = le32_encode_bits(ATH12K_WIFI_DESC_HARD_DROP_THRESHOLD,
					 HAL_TX_MSDU_FLOW_MED_DROP_THRESHOLD) |
			le32_encode_bits(ATH12K_WIFI_DESC_HARD_DROP_THRESHOLD,
					 HAL_TX_MSDU_FLOW_HARD_DROP_THRESHOLD);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       MSDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MSDUQ setup:",
			msduq, sizeof(*msduq));
	return 0;
}

int ath12k_wifi8_hal_tx_mpdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 mpduq_idx,
					 struct hal_tx_mpdu_queue_head_info *ti)
{
	struct hal_tx_mpdu_queue_head *mpduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u32 paddr_lo;
	u8 paddr_hi;

	mpduq = pool_node_from_id(dp_hw_grp_wifi8->mpduq_ctxt, mpduq_idx);
	if (!mpduq) {
		ath12k_err(ab, "MPDUQ flow is NULL\n");
		return -ENOMEM;
	}

	ath12k_core_dma_sync_single_for_cpu(dev, ti->paddr,
					    MPDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
	if (ath12k_wifi8_hal_mpduq_is_valid(mpduq)) {
		ath12k_err(ab, "MPDUQ flow %pK already valid\n", mpduq);
		ath12k_wifi8_hal_mpduq_set_invalid(dp_hw_grp, mpduq, ti->paddr);
		return -EEXIST;
	}

	memset(mpduq, 0, sizeof(*mpduq));
	mpduq->header.info0 = le32_encode_bits(HAL_WIFITQM_OWNED,
					       HAL_TX_DESCRIPTOR_HEADER_OWNER) |
			      le32_encode_bits(HAL_WIFITRANSMIT_MPDU_HEAD_DESCRIPTOR,
					       HAL_TX_DESCRIPTOR_HEADER_BUFFER_TYPE) |
			      le32_encode_bits(ti->queue_number,
					       HAL_TX_DESCRIPTOR_HEADER_QUEUE_NUMBER);
	mpduq->info0 = le32_encode_bits(ti->mlo, HAL_TX_MPDU_QUEUE_HEAD_MLO_VALID) |
		       le32_encode_bits(HAL_WIFIMPDU_TYPE_BASIC,
					HAL_TX_MPDU_QUEUE_HEAD_MPDU_TYPE);
	mpduq->info2 = le32_encode_bits(1, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID) |
		       le32_encode_bits(0,
					HAL_TX_MPDU_QUEUE_HEAD_ASSOC_LINK_DESC_CNT) |
		       le32_encode_bits(ATH12K_MPDU_SEQ_NUM_MASK,
					HAL_TX_MPDU_QUEUE_HEAD_MPDU_LAST_SEQ_NUM);
	mpduq->info3 = le32_encode_bits(ti->tid, HAL_TX_MPDU_QUEUE_HEAD_TID) |
		       le32_encode_bits(ti->peer_id, HAL_TX_MPDU_QUEUE_HEAD_SW_PEER_ID);
	paddr_lo = lower_32_bits(ti->pn_dma_addr);
	mpduq->info4 = le32_encode_bits(paddr_lo,
					HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_31_0);
	paddr_hi = (u8)(upper_32_bits(ti->pn_dma_addr) & ATH12K_PN_ADDR_HIGH_BYTE_MASK);
	mpduq->info5 = le32_encode_bits(paddr_hi, HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_39_32);
	if (ti->wapi)
		mpduq->info5 |= le32_encode_bits(2, HAL_TX_MPDU_QUEUE_HEAD_PN_INC_VALUE);
	else
		mpduq->info5 |= le32_encode_bits(1, HAL_TX_MPDU_QUEUE_HEAD_PN_INC_VALUE);
	if (ti->encap_type == ATH12K_HW_TXRX_RAW ||
	    ti->encap_type == ATH12K_HW_TXRX_NATIVE_WIFI || ti->is_mgmtq) {
		mpduq->info5 |= le32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_MPDU_HDR_LEN);
	} else {
		if (ti->tid > MAX_VALID_DATA_TID)
			mpduq->info5 |= le32_encode_bits(ATH12K_FRAME_HEADER_SIZE,
						HAL_TX_MPDU_QUEUE_HEAD_MPDU_HDR_LEN);
		else
			mpduq->info5 |= le32_encode_bits(ATH12K_QOS_FRAME_HEADER_SIZE,
						HAL_TX_MPDU_QUEUE_HEAD_MPDU_HDR_LEN);
	}
	mpduq->info5 |= le32_encode_bits(ATH12K_MAX_NUM_OF_EXT_DESCRIPTORS,
					 HAL_TX_MPDU_QUEUE_HEAD_NUM_OF_EXT_DESC);
	mpduq->info9 = le32_encode_bits(ti->assoc_link_id,
					HAL_TX_MPDU_QUEUE_HEAD_LINK0_ID) |
		       le32_encode_bits(ti->link_id1, HAL_TX_MPDU_QUEUE_HEAD_LINK1_ID) |
		       le32_encode_bits(ti->link_id2, HAL_TX_MPDU_QUEUE_HEAD_LINK2_ID);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       MPDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MPDUQ setup:",
			mpduq, sizeof(*mpduq));
	return 0;
}

void ath12k_wifi8_hal_txpt_classify_info_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					       struct hal_txpt_classify_info *tx_tid_ptr,
					       struct hal_txpt_classify_data *ti)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	if (!tx_tid_ptr) {
		ath12k_err(ab, "txpt_classify_info is NULL\n");
		return;
	}
	switch (ti->subtype) {
	case HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP:
		tx_tid_ptr->info0 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
		break;
	case HAL_CLASSIFY_BANK_SUBTYPE_UDP:
		tx_tid_ptr->info1 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
		break;
	case HAL_CLASSIFY_BANK_SUBTYPE_MIXED:
		tx_tid_ptr->info0 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
		tx_tid_ptr->info1 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
		break;
	default:
		ath12k_err(ab, "Invalid classify bank subtype %d\n", ti->subtype);
		return;
	}

	tx_tid_ptr->info2 = le32_encode_bits(ti->flow_handler,
					     HAL_TXPT_CLASSIFY_TQM_FLOW_HANDLER) |
			    le32_encode_bits(ti->flow_loop_handler,
					     HAL_TXPT_CLASSIFY_TQM_FLOW_LOOP_HANDLER) |
			    le32_encode_bits(ti->msdu_drop,
					     HAL_TXPT_CLASSIFY_MSDU_DROP) |
			    le32_encode_bits(ti->metadata,
					     HAL_TXPT_CLASSIFY_METADATA);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       ATH12K_SIZE_OF_TID_INFO,
					       DMA_BIDIRECTIONAL);

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal CLASSIFY INFO setup:",
			tx_tid_ptr, sizeof(*tx_tid_ptr));
}

bool ath12k_wifi8_hal_msduq_is_valid(struct hal_tx_msdu_flow *msduq)
{
	return !!le32_get_bits(msduq->info0, HAL_TX_MSDU_FLOW_FLOW_VALID);
}

bool ath12k_wifi8_hal_mpduq_is_valid(struct hal_tx_mpdu_queue_head *mpduq)
{
	return !!le32_get_bits(mpduq->info2, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID);
}

u32 ath12k_wifi8_hal_get_txpt_flow_ptr(struct hal_txpt_classify_info *tx_tid_ptr,
				       enum hal_classify_bank_subtype subtype)
{
	if (subtype == HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP)
		return le32_get_bits(tx_tid_ptr->info0,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
	else if (subtype == HAL_CLASSIFY_BANK_SUBTYPE_UDP)
		return le32_get_bits(tx_tid_ptr->info1,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
	else if ((subtype == HAL_CLASSIFY_BANK_SUBTYPE_MIXED) &&
		 (le32_get_bits(tx_tid_ptr->info1,
				HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8) ==
		  le32_get_bits(tx_tid_ptr->info0,
				HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8)))
		return le32_get_bits(tx_tid_ptr->info1,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
	return 0;
}

void ath12k_wifi8_hal_msduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_msdu_flow *msduq,
					dma_addr_t paddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	msduq->info0 = le32_encode_bits(0, HAL_TX_MSDU_FLOW_FLOW_VALID);

	ath12k_core_dma_sync_single_for_device(dev, paddr,
					       MSDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);
}

void ath12k_wifi8_hal_mpduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_mpdu_queue_head *mpduq,
					dma_addr_t paddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	mpduq->info2 = le32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID);
	ath12k_core_dma_sync_single_for_device(dev, paddr,
					       MPDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);
}
