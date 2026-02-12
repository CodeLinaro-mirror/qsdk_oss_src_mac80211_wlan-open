// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal.h"
#include "hal.h"
#include "../hif.h"
#include "../debug.h"

#define HAL_TX_BITS_PER_TID 3
#define HAL_TX_NUM_DSCP_REG_SIZE 32

void ath12k_wifi8_hal_tx_cmd_desc_setup(struct ath12k_base *ab,
					struct hal_tcl_data_cmd *tcl_cmd,
					struct hal_tx_info *ti)
{
	tcl_cmd->buf_addr_info.info0 =
		le32_encode_bits(ti->paddr, BUFFER_ADDR_INFO0_ADDR);
	tcl_cmd->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)ti->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	tcl_cmd->buf_addr_info.info1 |=
		le32_encode_bits((ti->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(ti->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	tcl_cmd->info0 =
		le32_encode_bits(ti->type, HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE) |
		le32_encode_bits(ti->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID) |
		le32_encode_bits(ti->vdev_id, HAL_TCL_DATA_CMD_INFO0_VDEV_ID) |
		le32_encode_bits(ti->data_len, HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH);

	tcl_cmd->search_index = cpu_to_le32(ti->bss_ast_idx);
	tcl_cmd->info1 =
		le32_encode_bits(ti->bss_ast_hash, HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM) |
		le32_encode_bits(ti->lookup_override,
				 HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE) |
		le32_encode_bits(ti->tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HEADER_LENGTH_READ_SEL);

	tcl_cmd->info2 = cpu_to_le32(ti->flags0) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_MSDU_COLOR) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_FORMAT) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_VALID);

	tcl_cmd->info3 = cpu_to_le32(ti->flags1) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT) |
		le32_encode_bits(ti->pkt_offset, HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH) |
		/* TODO: Chaitanya : confirm if this link is on which pkt is getting
		 * enqueued
		 */
		le32_encode_bits(ti->link_id, HAL_TCL_DATA_CMD_INFO3_LINK_ID);

	tcl_cmd->tcl_cmd_number = cpu_to_le32(ti->meta_data_flags);

	tcl_cmd->info4 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID);

	tcl_cmd->insert_vlan_tci_override_val = 0;
	tcl_cmd->info5 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_INSERT_VLAN_TCI_OVERRIDE_EN) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_RING_ID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_LOOPING_COUNT);
}

void ath12k_update_dscp_register(struct ath12k_base *ab, u32 addr, u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, addr);
	reg_val &= ~mask;
	reg_val |= value;
	ath12k_hif_write32(ab, addr, reg_val);
}

void ath12k_wifi8_hal_tx_update_dscp_tid_map(struct ath12k_base *ab,
					     int id, u8 dscp, u8 tid)
{
	u32 ctrl_reg_val;
	u32 addr;
	u32 start_index, end_index;
	u32 mask;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Calculate start and end indices within the register */
	start_index = dscp * HAL_TX_BITS_PER_TID;
	end_index = (start_index + (HAL_TX_BITS_PER_TID - 1)) %
		    HAL_TX_NUM_DSCP_REG_SIZE;
	addr += (4 * (start_index / HAL_TX_NUM_DSCP_REG_SIZE));
	start_index %= HAL_TX_NUM_DSCP_REG_SIZE;

	if (end_index < start_index) {
		/* Handle the case where the TID value spans two registers */
		mask = GENMASK((HAL_TX_NUM_DSCP_REG_SIZE - 1), start_index);
		value = (tid << start_index) & mask;
		/* Update the first register */
		ath12k_update_dscp_register(ab, addr, mask, value);

		/* Update the second register */
		addr = addr + 4;
		mask = GENMASK(end_index, 0);
		value = (tid >> (HAL_TX_NUM_DSCP_REG_SIZE - start_index)) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	} else {
		/* Handle the case where the TID value fits within one register */
		mask = GENMASK(end_index, start_index);
		value = (tid << start_index) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);
}

void ath12k_wifi8_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id)
{
	u32 ctrl_reg_val;
	u32 addr;
	u8 hw_map_val[HAL_DSCP_TID_TBL_SIZE], count = 0;
	int i;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Configure each DSCP-TID mapping in three bits there by configure
	 * three bytes in an iteration.
	 */
	for (i = 0; i < DSCP_TID_MAP_TBL_ENTRY_SIZE; i += 8) {
		value = 0;

		value |= u32_encode_bits(map[i], HAL_TCL1_RING_FIELD_DSCP_TID_MAP0);
		value |= u32_encode_bits(map[i + 1], HAL_TCL1_RING_FIELD_DSCP_TID_MAP1);
		value |= u32_encode_bits(map[i + 2], HAL_TCL1_RING_FIELD_DSCP_TID_MAP2);
		value |= u32_encode_bits(map[i + 3], HAL_TCL1_RING_FIELD_DSCP_TID_MAP3);
		value |= u32_encode_bits(map[i + 4], HAL_TCL1_RING_FIELD_DSCP_TID_MAP4);
		value |= u32_encode_bits(map[i + 5], HAL_TCL1_RING_FIELD_DSCP_TID_MAP5);
		value |= u32_encode_bits(map[i + 6], HAL_TCL1_RING_FIELD_DSCP_TID_MAP6);
		value |= u32_encode_bits(map[i + 7], HAL_TCL1_RING_FIELD_DSCP_TID_MAP7);

		memcpy(&hw_map_val[count], &value, 3);
		count += 3;
	}

	for (i = 0; i < HAL_DSCP_TID_TBL_SIZE; i += 4) {
		ath12k_hif_write32(ab, addr, *(u32 *)&hw_map_val[i]);
		addr += 4;
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG,
			   ctrl_reg_val);
}

int
ath12k_wifi8_hal_invalidate_tx_cache_cmd_send(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      struct ath12k_hal_tx_cmd_ring_param *param)
{
	struct hal_tcl_gse_cmd *tx_gse_cmd;
	int ret = 0;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	tx_gse_cmd = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!tx_gse_cmd) {
		ret = -ENOBUFS;
		goto out;
	}
	tx_gse_cmd->control_buffer_addr_31_0 = cpu_to_le32(param->ctrl_buf_addr);
	tx_gse_cmd->info0 =
		le32_encode_bits(((u64)param->ctrl_buf_addr >> HAL_ADDR_MSB_REG_SHIFT),
				 HAL_TCL_GSE_CMD_INFO0_CONTROL_BUFFER_ADDR_39_32) |
		le32_encode_bits(param->cmd_num,
				 HAL_TCL_GSE_CMD_INFO0_GSE_CTRL) |
		le32_encode_bits(0, /*tcl2sw */
				 HAL_TCL_GSE_CMD_INFO0_STATUS_DESTINATION_RING_ID);
	tx_gse_cmd->info1 = le32_encode_bits(1, /*tcl cmd ring */
					     HAL_TCL_GSE_CMD_INFO1_TCL_CMD_TYPE);
	tx_gse_cmd->cmd_meta_data_31_0 = cpu_to_le32(param->meta_data_0);
out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi8_hal_tqm_remove_msdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_remove_msdu *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	struct ath12k_dp *dp = ab->dp;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MSDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_remove_msdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	desc->cmd_hdr.info0 = le32_encode_bits(dp->tqm_cmd_num++, HAL_TQM_CMD_NUMBER);
	if (unlikely(dp->tqm_cmd_num == 0))
		dp->tqm_cmd_num = 1;

	desc->cmd_hdr.info1 = le32_encode_bits(0x7F, HAL_TQM_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->remove_msdu_params.msdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->remove_msdu_params.msdu_q_paddr)
			& 0x000000ff);
	desc->info0 = le32_encode_bits(paddr_lo,
				       HAL_TQM_MSDU_FLOW_DESC_ADDR_31_0);

	desc->info1 = le32_encode_bits(paddr_hi,
				       HAL_TQM_MSDU_FLOW_DESC_ADDR_39_32);

	desc->info2 = le32_encode_bits(cmd->remove_msdu_params.type,
				       HAL_TQM_MSDU_REMOVE_MSDU_CMD_TYPE) |
		le32_encode_bits(cmd->remove_msdu_params.count,
				 HAL_TQM_MSDU_REMOVE_COUNT) |
		le32_encode_bits(
			cmd->remove_msdu_params.block_tx_notify_frame_removal,
			HAL_TQM_MSDU_BLOCK_TX_NOTIFY_FRAME_REMOVAL);

	//need to check where to increment this value??????????????
	return le32_get_bits(desc->cmd_hdr.info0, HAL_TQM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tqm_remove_mpdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_remove_mpdu *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	struct ath12k_dp *dp = ab->dp;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MPDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_remove_mpdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	desc->cmd_hdr.info0 = le32_encode_bits(dp->tqm_cmd_num++, HAL_TQM_CMD_NUMBER);
	if (unlikely(dp->tqm_cmd_num == 0))
		dp->tqm_cmd_num = 1;

	desc->cmd_hdr.info1 = le32_encode_bits(0x7F, HAL_TQM_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->remove_mpdu_params.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->remove_mpdu_params.mpdu_q_paddr)
			& 0x000000ff);
	desc->info0 = le32_encode_bits(paddr_lo,
				       HAL_TQM_MPDU_QUEUE_DESC_ADDR_31_0);

	desc->info1 = le32_encode_bits(paddr_hi,
				       HAL_TQM_MPDU_QUEUE_DESC_ADDR_39_32) |
		le32_encode_bits(
			cmd->remove_mpdu_params.block_tx_notify_frame_removal,
			HAL_TQM_MPDU_BLOCK_TX_NOTIFY_FRAME_REMOVAL);

	desc->info2 = le32_encode_bits(cmd->remove_mpdu_params.type,
				       HAL_TQM_MPDU_REMOVE_MPDU_CMD_TYPE) |
		le32_encode_bits(cmd->remove_mpdu_params.count,
				 HAL_TQM_MPDU_REMOVE_COUNT);

	return le32_get_bits(desc->cmd_hdr.info0, HAL_TQM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tqm_sync_cmd(struct ath12k_base *ab,
				  struct hal_tlv_64_hdr *tlv,
				  struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tqm_sync_cmd *desc;
	u32 data_lo, data_hi;
	uintptr_t cb_func_addr, cb_ctxt_addr;
	struct ath12k_dp *dp = ab->dp;

	tlv->tl = le64_encode_bits(HAL_TQM_SYNC_CMD_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_sync_cmd *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	desc->cmd_hdr.info0 = le32_encode_bits(dp->tqm_cmd_num++, HAL_TQM_CMD_NUMBER);
	if (unlikely(dp->tqm_cmd_num == 0))
		dp->tqm_cmd_num = 1;

	desc->cmd_hdr.info1 = le32_encode_bits(0x7F, HAL_TQM_SESSION_ID) |
		le32_encode_bits(1, HAL_TQM_STATUS_REQUIRED_FOR_HOST) |
		le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
				 HAL_TQM_HOST_STATUS_RING) |
		le32_encode_bits(cmd->std.peer_id,
				 HAL_TQM_SW_PEER_ID_FOR_COMPARISON);

	if (cmd->tqm_sync_params.data_only) {
		data_lo = lower_32_bits(cmd->tqm_sync_params.cb_data);
		data_hi = upper_32_bits(cmd->tqm_sync_params.cb_data);
		desc->info0 = le32_encode_bits(data_lo, HAL_TQM_SYNC_SW_METADATA_31_0);
		desc->info1 = le32_encode_bits(data_hi, HAL_TQM_SYNC_SW_METADATA_63_32);
	} else {
		cb_func_addr = (u64)(uintptr_t)cmd->tqm_sync_params.cb_func;
		cb_ctxt_addr = (u64)(uintptr_t)cmd->tqm_sync_params.cb_ctxt;
		desc->info0 = le32_encode_bits(lower_32_bits(cb_func_addr),
					       HAL_TQM_SYNC_SW_METADATA_31_0);
		desc->info1 = le32_encode_bits(upper_32_bits(cb_func_addr),
					       HAL_TQM_SYNC_SW_METADATA_63_32);
		desc->info2 = le32_encode_bits(lower_32_bits(cb_ctxt_addr),
					       HAL_TQM_SYNC_SW_METADATA_95_64);
		desc->info3 = le32_encode_bits(upper_32_bits(cb_ctxt_addr),
					       HAL_TQM_SYNC_SW_METADATA_127_96);
		data_lo = lower_32_bits(cmd->tqm_sync_params.cb_data);
		data_hi = upper_32_bits(cmd->tqm_sync_params.cb_data);
		desc->info4 = le32_encode_bits(data_lo, HAL_TQM_SYNC_SW_METADATA_159_128);
		desc->info5 = le32_encode_bits(data_hi, HAL_TQM_SYNC_SW_METADATA_191_160);
	}
	return le32_get_bits(desc->cmd_hdr.info0, HAL_TQM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tqm_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_tlv_tag_be type,
				  struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tlv_64_hdr *tqm_desc;
	int ret;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	tqm_desc = (struct hal_tlv_64_hdr *)ath12k_hal_srng_src_get_tqm_next_entry(
									ab, srng, type);
	if (!tqm_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	switch (type) {
	case HAL_TQM_REMOVE_MSDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_msdu_cmd(ab, tqm_desc, cmd);
		break;
	case HAL_TQM_REMOVE_MPDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_mpdu_cmd(ab, tqm_desc, cmd);
		break;
	case HAL_TQM_SYNC_CMD_BO:
		ret = ath12k_wifi8_hal_tqm_sync_cmd(ab, tqm_desc, cmd);
		break;
	default:
		ath12k_warn(ab, "Unknown tqm command %d\n", type);
		ret = -EINVAL;
		break;
	}

out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

void ath12k_wifi8_hal_tqm_remove_msdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status)
{
	struct hal_tqm_remove_msdu_status *desc =
		(struct hal_tqm_remove_msdu_status *)tlv->value;

	status->status_hdr.status_num =  le32_get_bits(desc->status_hdr.info0,
						       HAL_TQM_STATUS_NUMBER);
	status->status_hdr.cmd_execution_status = le32_get_bits(
						desc->status_hdr.info2,
						HAL_TQM_STATUS_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info2,
							   HAL_TQM_STATUS_RING);
	status->remove_msdu.remove_msdu_cmd_type = le32_get_bits(
						desc->info0,
						HAL_TQM_MSDU_REMOVE_MSDU_CMD_TYPE);
	status->remove_msdu.removed_msdu_count = le32_get_bits(
						desc->info0,
						HAL_TQM_MSDU_REMOVED_MSDU_COUNT);
	status->remove_msdu.tx_flow_number = le32_get_bits(
						desc->info4,
						HAL_TQM_MSDU_TX_FLOW_NUMBER);
}

void ath12k_wifi8_hal_tqm_remove_mpdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status)
{
	struct hal_tqm_remove_mpdu_status *desc =
		(struct hal_tqm_remove_mpdu_status *)tlv->value;

	status->status_hdr.status_num =  le32_get_bits(desc->status_hdr.info0,
						       HAL_TQM_STATUS_NUMBER);
	status->status_hdr.cmd_execution_status = le32_get_bits(
						desc->status_hdr.info2,
						HAL_TQM_STATUS_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info2,
							   HAL_TQM_STATUS_RING);
	status->remove_mpdu.remove_mpdu_cmd_type = le32_get_bits(
						desc->info0,
						HAL_TQM_MPDU_REMOVE_MPDU_CMD_TYPE);
	status->remove_mpdu.removed_mpdu_count = le32_get_bits(
						desc->info0,
						HAL_TQM_MPDU_REMOVED_MPDU_COUNT);
	status->remove_mpdu.tx_mpdu_queue_number = le32_get_bits(
						desc->info8,
						HAL_TQM_MPDU_TX_MPDU_QUEUE_NUMBER);
}

void ath12k_wifi8_hal_tqm_sync_cmd_status(struct ath12k_base *ab,
					  struct hal_tlv_64_hdr *tlv,
					  struct hal_tqm_status *status)
{
	struct hal_tqm_sync_cmd_status *desc =
		(struct hal_tqm_sync_cmd_status *)tlv->value;

	status->status_hdr.status_num =  le32_get_bits(desc->status_hdr.info0,
						       HAL_TQM_STATUS_NUMBER);
	status->status_hdr.cmd_execution_status = le32_get_bits(
						desc->status_hdr.info2,
						HAL_TQM_STATUS_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info2,
							   HAL_TQM_STATUS_RING);
	status->sync_status.metadata_0 = le32_get_bits(
						desc->info0,
						HAL_TQM_SYNC_STATUS_SW_METADATA_31_0);
	status->sync_status.metadata_1 = le32_get_bits(
						desc->info1,
						HAL_TQM_SYNC_STATUS_SW_METADATA_63_32);
	status->sync_status.metadata_2 = le32_get_bits(
						desc->info2,
						HAL_TQM_SYNC_STATUS_SW_METADATA_95_64);
}
