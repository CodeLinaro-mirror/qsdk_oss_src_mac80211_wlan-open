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
#include "dp.h"

#define HAL_TX_BITS_PER_TID 3
#define HAL_TX_NUM_DSCP_REG_SIZE 32

#define HAL_TX_NUM_MAX_LINKS	5

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

	tcl_cmd->info3 =
		cpu_to_le32(ti->flags1) |
		le32_encode_bits(ti->tx_notify_frame,
				 HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT) |
		le32_encode_bits(ti->pkt_offset, HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH) |
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

u8 ath12k_wifi8_hal_tx_get_tid_from_dscp(struct ath12k_base *ab,
					 int id, u8 dscp)
{
	u32 ctrl_reg_val;
	u32 addr;
	u32 start_index, end_index;
	u32 reg_val;
	u32 part0, part1;
	u32 tid;
	const u32 tid_mask = (1U << HAL_TX_BITS_PER_TID) - 1;

	/* Enable read/write access to DSCP->TID table */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	start_index = dscp * HAL_TX_BITS_PER_TID;
	end_index = (start_index + (HAL_TX_BITS_PER_TID - 1)) %
		    HAL_TX_NUM_DSCP_REG_SIZE;

	addr += 4 * (start_index / HAL_TX_NUM_DSCP_REG_SIZE);
	start_index %= HAL_TX_NUM_DSCP_REG_SIZE;

	if (end_index < start_index) {
		/* Value spans two 32-bit registers */
		reg_val = ath12k_hif_read32(ab, addr);
		part0 = (reg_val & GENMASK(HAL_TX_NUM_DSCP_REG_SIZE - 1, start_index))
			>> start_index;

		reg_val = ath12k_hif_read32(ab, addr + 4);
		part1 = reg_val & GENMASK(end_index, 0);

		tid = (part1 << (HAL_TX_NUM_DSCP_REG_SIZE - start_index)) | part0;
	} else {
		/* Value fits within one 32-bit register */
		reg_val = ath12k_hif_read32(ab, addr);
		tid = (reg_val & GENMASK(end_index, start_index)) >> start_index;
	}

	/* Disable access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	return (u8)(tid & tid_mask);
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
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MSDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_remove_msdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_TQM_CMD_NUMBER);

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
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_REMOVE_MPDU_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_remove_mpdu *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_TQM_CMD_NUMBER);

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
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_SYNC_CMD_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_sync_cmd *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info0);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_TQM_CMD_NUMBER);

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

static inline
int ath12k_wifi8_hal_srng_write_words(struct ath12k_base *ab,
				      struct hal_srng *srng,
				      u32 cmd_size,
				      void *desc_start,
				      const u32 *src_words)
{
	u32 hp, ring_size, i, ring_idx;
	u32 *ring_base;

	ring_base = (u32 *)srng->ring_base_vaddr;
	hp = (u32)((u32 *)desc_start - ring_base);
	ring_size = srng->ring_size;
	ring_idx = hp;
	for (i = 0; i < cmd_size; i++) {
		ring_base[ring_idx] = src_words[i];
		ring_idx++;
		if (ring_idx == ring_size)
			ring_idx = 0;
	}
	return 0;
}

int ath12k_wifi8_hal_tqm_get_mpduq_stats(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd)
{
	struct ath12k_dp *dp = ab->dp;
	struct hal_tqm_get_mpduq_stats *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_GET_MPDUQ_STATS_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_get_mpduq_stats *)tlv->value;
	memset_startat(desc, 0, cmd_hdr.info1);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num);

	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_TQM_CMD_NUMBER);
	desc->cmd_hdr.info1 = le32_encode_bits(0x7F, HAL_TQM_SESSION_ID) |
			      le32_encode_bits(1, HAL_TQM_STATUS_REQUIRED_FOR_HOST) |
			      le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
					       HAL_TQM_HOST_STATUS_RING) |
			      le32_encode_bits(cmd->std.peer_id,
					       HAL_TQM_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->get_mpduq_stats.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->get_mpduq_stats.mpdu_q_paddr)
			& 0x000000ff);
	desc->info0 = le32_encode_bits(paddr_lo,
				       HAL_TQM_GET_MPDUQ_STATS_INFO0_MPDUQ_ADDR_LO);

	desc->info1 = le32_encode_bits(paddr_hi,
				       HAL_TQM_GET_MPDUQ_STATS_INFO1_MPDUQ_ADDR_HI) |
		      le32_encode_bits(cmd->get_mpduq_stats.clear_stats,
				       HAL_TQM_GET_MPDUQ_STATS_INFO1_CLEAR_STATS);

	return le32_get_bits(desc->cmd_hdr.info0, HAL_TQM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tqm_update_mpduq(struct ath12k_base *ab,
				      struct hal_tlv_64_hdr *tlv,
				      struct ath12k_hal_tqm_cmd *cmd)
{
	struct ath12k_dp *dp = ab->dp;
	struct hal_tqm_update_mpduq *desc;
	u32 paddr_lo;
	u8 paddr_hi;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_TQM_UPDATE_MPDUQ_BO, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_tqm_update_mpduq *)tlv->value;
	memset(desc, 0, sizeof(*desc));
	memset_startat(desc, 0, cmd_hdr.info1);

	cmd_num = atomic_inc_return(&dp->tqm_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp->tqm_cmd_num);

	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_TQM_CMD_NUMBER);
	desc->cmd_hdr.info1 = le32_encode_bits(0x7F, HAL_TQM_SESSION_ID) |
			      le32_encode_bits(1, HAL_TQM_STATUS_REQUIRED_FOR_HOST) |
			      le32_encode_bits(HAL_TQM_HOST_STATUS_RING_0,
					       HAL_TQM_HOST_STATUS_RING) |
			      le32_encode_bits(cmd->std.peer_id,
					       HAL_TQM_SW_PEER_ID_FOR_COMPARISON);

	paddr_lo = lower_32_bits(cmd->update_mpduq.mpdu_q_paddr);
	paddr_hi = (u8)(upper_32_bits(cmd->update_mpduq.mpdu_q_paddr)
			& 0x000000ff);
	desc->info0 = le32_encode_bits(paddr_lo,
				       HAL_TQM_UPDATE_MPDUQ_INFO0_MPDUQ_ADDR_LO);
	desc->info1 = le32_encode_bits(paddr_hi,
				       HAL_TQM_UPDATE_MPDUQ_INFO1_MPDUQ_ADDR_HI);

	if (cmd->update_mpduq.max_lsn_valid) {
		desc->info14 |=
			le32_encode_bits(cmd->update_mpduq.max_lsn_valid,
					 HAL_TQM_UPDATE_MPDUQ_INFO14_MAX_LSN_VALID) |
			le32_encode_bits(cmd->update_mpduq.max_lsn,
					 HAL_TQM_UPDATE_MPDUQ_INFO14_MAX_LSN);

		desc->info16 |=
		le32_encode_bits(1,
				 HAL_TQM_UPDATE_MPDUQ_INFO16_UPDATE_MAX_LSN_VALID) |
		le32_encode_bits(1,
				 HAL_TQM_UPDATE_MPDUQ_INFO16_UPDATE_MAX_LSN);
	}

	if (cmd->update_mpduq.sn_num_valid) {
		desc->info1 |=
			le32_encode_bits(cmd->update_mpduq.sn_num_valid,
					 HAL_TQM_UPDATE_MPDUQ_INFO1_SEQ_NUM_UPDATE_VALID);
		desc->info2 |=
			le32_encode_bits(cmd->update_mpduq.sn_num,
					 HAL_TQM_UPDATE_MPDUQ_INFO2_START_SEQ_NUM) |
			le32_encode_bits(cmd->update_mpduq.sn_num,
					 HAL_TQM_UPDATE_MPDUQ_INFO2_LAST_SEQ_NUM);
	}

	return le32_get_bits(desc->cmd_hdr.info0, HAL_TQM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tqm_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_tlv_tag_be type,
				  struct ath12k_hal_tqm_cmd *cmd)
{
	struct hal_tlv_64_hdr *tqm_desc;
	struct hal_tlv_64_hdr *tlv_desc;
	u32 cmd_size;
	int ret;
	int err;

	if (!ab->tqm_cmd_staging) {
		ath12k_err(ab, "TQM staging buffer not allocated\n");
		return -ENOMEM;
	}
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	tlv_desc = (struct hal_tlv_64_hdr *)ab->tqm_cmd_staging;
	if (!tlv_desc) {
		ret = -ENOMEM;
		goto out;
	}
	switch (type) {
	case HAL_TQM_REMOVE_MSDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_msdu_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_REMOVE_MPDU_BO:
		ret = ath12k_wifi8_hal_tqm_remove_mpdu_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_SYNC_CMD_BO:
		ret = ath12k_wifi8_hal_tqm_sync_cmd(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_GET_MPDUQ_STATS_BO:
		ret = ath12k_wifi8_hal_tqm_get_mpduq_stats(ab, tlv_desc, cmd);
		break;
	case HAL_TQM_UPDATE_MPDUQ_BO:
		ret = ath12k_wifi8_hal_tqm_update_mpduq(ab, tlv_desc, cmd);
		break;

	default:
		ath12k_warn(ab, "Unknown tqm command %d\n", type);
		ret = -EINVAL;
		goto out;
	}

	tqm_desc = (struct hal_tlv_64_hdr *)
			ath12k_hal_srng_src_get_next_entry_by_cmd_size(ab, srng, type);
	if (!tqm_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	cmd_size = ath12k_hal_srng_get_cmd_size(type);
	if (!cmd_size || cmd_size >= srng->ring_size ||
	    cmd_size > HAL_TQM_CMD_MAX_WORDS) {
		ret = -EINVAL;
		goto out;
	}
	err = ath12k_wifi8_hal_srng_write_words(ab, srng, cmd_size,
						(void *)tqm_desc,
						ab->tqm_cmd_staging);
	if (err) {
		ret = err;
		goto out;
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

void ath12k_wifi8_hal_tqm_get_mpduq_stats_cmd_status(struct ath12k_base *ab,
						     struct hal_tlv_64_hdr *tlv,
						     struct hal_tqm_status *status)
{
	struct hal_tqm_get_mpduq_stats_cmd_status *desc =
		(struct hal_tqm_get_mpduq_stats_cmd_status *)tlv->value;

	status->status_hdr.status_num =  le32_get_bits(desc->status_hdr.info0,
						       HAL_TQM_STATUS_NUMBER);
	status->status_hdr.cmd_execution_status =
				le32_get_bits(desc->status_hdr.info2,
					      HAL_TQM_STATUS_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info2,
							   HAL_TQM_STATUS_RING);

	status->mpduq_stats.mpdu_cnt =
			le32_get_bits(desc->info0,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO0_MPDU_COUNT);
	status->mpduq_stats.start_seq_num =
			le32_get_bits(desc->info2,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO2_START_SEQ_NUM);
	status->mpduq_stats.tid =
			le32_get_bits(desc->info2,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO2_TID);
	status->mpduq_stats.pn_31_0 =
			le32_get_bits(desc->info3,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO3_PN_31_0);
	status->mpduq_stats.pn_47_32 =
			le32_get_bits(desc->info4,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO4_PN_47_32);
	status->mpduq_stats.sw_peer_id =
			le32_get_bits(desc->info4,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO4_SW_PEER_ID);
	status->mpduq_stats.last_seq_num =
			le32_get_bits(desc->info15,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO15_LAST_SEQ_NUM);
	status->mpduq_stats.max_lsn_valid =
		le32_get_bits(desc->info15,
			      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO15_MAX_LSN_VALID);
	status->mpduq_stats.max_lsn =
			le32_get_bits(desc->info15,
				      HAL_TQM_GET_MPDUQ_STATS_STATUS_INFO15_MAX_LSN);
}

void ath12k_wifi8_hal_tqm_update_mpduq_cmd_status(struct ath12k_base *ab,
						  struct hal_tlv_64_hdr *tlv,
						  struct hal_tqm_status *status)
{
	struct hal_tqm_update_mpduq_cmd_status *desc =
		(struct hal_tqm_update_mpduq_cmd_status *)tlv->value;

	status->status_hdr.status_num = le32_get_bits(desc->status_hdr.info0,
						      HAL_TQM_STATUS_NUMBER);
	status->status_hdr.cmd_execution_status =
				le32_get_bits(desc->status_hdr.info2,
					      HAL_TQM_STATUS_CMD_EXECUTION_STATUS);
	status->status_hdr.tqm_status_ring = le32_get_bits(desc->status_hdr.info2,
							   HAL_TQM_STATUS_RING);
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

int ath12k_wifi8_hal_tqm_cmd_staging_alloc(struct ath12k_base *ab)
{
	/* Allocate single staging buffer in ab for the TQM command ring. */
	ab->tqm_cmd_staging = kzalloc(HAL_TQM_CMD_MAX_BYTES,
				      GFP_ATOMIC);

	if (!ab->tqm_cmd_staging) {
		ath12k_err(ab, "failed to alloc TQM staging");
		return -ENOMEM;
	}
	return 0;
}

void ath12k_wifi8_hal_tqm_cmd_staging_free(struct ath12k_base *ab)
{
	kfree(ab->tqm_cmd_staging);
	ab->tqm_cmd_staging = NULL;
}

int ath12k_wifi8_hal_tx_sam_mpduq_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tlv_64_hdr *tlv)
{
	struct hal_sam_mpdu_queue_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH);

	desc = (struct hal_sam_mpdu_queue_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* TODO: In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_SAM_CMD_NUMBER);
	desc->info0 = le32_encode_bits(0, HAL_SAM_MPDU_START_MPDU_QUEUE_SAM_ID) |
		      le32_encode_bits(MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED - 1,
				       HAL_SAM_MPDU_END_MPDU_QUEUE_SAM_ID);

	return le32_get_bits(desc->cmd_hdr.info0, HAL_SAM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tx_sam_msduq_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tlv_64_hdr *tlv)
{
	struct hal_sam_msdu_queue_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH);

	desc = (struct hal_sam_msdu_queue_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* TODO: In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_SAM_CMD_NUMBER);
	desc->info0 = le32_encode_bits(0, HAL_SAM_MSDU_START_MSDU_QUEUE_SAM_ID) |
		      le32_encode_bits(MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED - 1,
				       HAL_SAM_MSDU_END_MSDU_QUEUE_SAM_ID);

	return le32_get_bits(desc->cmd_hdr.info0, HAL_SAM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tx_sam_peer_clear_cmd(struct ath12k_dp_wifi8 *dp_wifi8,
					   struct hal_tlv_64_hdr *tlv, int src_link_id)
{
	struct hal_sam_peer_clear_programming *desc;
	u32 cmd_num;

	tlv->tl = le64_encode_bits(HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
				   HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_64_HDR_LENGTH) |
		  le64_encode_bits(src_link_id, HAL_TLV_64_HDR_SRC_LINK_ID);

	desc = (struct hal_sam_peer_clear_programming *)tlv->value;

	cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);
	if (unlikely(cmd_num == 0))
		cmd_num = atomic_inc_return(&dp_wifi8->sam_cmd_num);

	/* TODO: In v2 hardware, the HAL_SAM_CMD_STATUS_REQUIRED_TO_SW bit
	 * must be set in the command header to receive status.
	 */
	desc->cmd_hdr.info0 = le32_encode_bits(cmd_num, HAL_SAM_CMD_NUMBER);
	desc->info0 = le32_encode_bits(0, HAL_SAM_PEER_START_PEER_ID) |
		      le32_encode_bits(ATH12K_MAX_STA_ID - 1, HAL_SAM_PEER_END_PEER_ID);

	return le32_get_bits(desc->cmd_hdr.info0, HAL_SAM_CMD_NUMBER);
}

int ath12k_wifi8_hal_tx_sam_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				     int src_link_id, enum hal_tlv_tag_be type)
{
	struct hal_tlv_64_hdr *sam_desc, *tlv_desc;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	u32 cmd_size;
	int ret;

	if (!dp_wifi8->sam_cmd_staging) {
		ath12k_err(ab, "SAM staging buffer not allocated\n");
		return -ENOMEM;
	}

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	tlv_desc = (struct hal_tlv_64_hdr *)dp_wifi8->sam_cmd_staging;
	memset(tlv_desc, 0, sizeof(*tlv_desc));

	switch (type) {
	case HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_mpduq_clear_cmd(dp_wifi8, tlv_desc);
		break;
	case HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_msduq_clear_cmd(dp_wifi8, tlv_desc);
		break;
	case HAL_SAM_PEER_CLEAR_PROGRAMMING_BO:
		ret = ath12k_wifi8_hal_tx_sam_peer_clear_cmd(dp_wifi8, tlv_desc,
							     src_link_id);
		break;
	default:
		ath12k_warn(ab, "Unknown sam command %d\n", type);
		ret = -EINVAL;
		goto out;
	}

	sam_desc = (struct hal_tlv_64_hdr *)
			ath12k_hal_srng_src_get_next_entry_by_cmd_size(ab, srng, type);
	if (!sam_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	cmd_size = ath12k_hal_srng_get_cmd_size(type);
	if (cmd_size > HAL_SAM_CMD_MAX_WORDS) {
		ret = -EINVAL;
		goto out;
	}
	ret = ath12k_wifi8_hal_srng_write_words(ab, srng, cmd_size,
						(void *)sam_desc,
						dp_wifi8->sam_cmd_staging);
	if (ret)
		ath12k_warn(ab, "Failed to write SAM cmd to ring: %d\n", ret);
out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return ret;
}

void ath12k_wifi8_hal_tx_sam_program_clear(struct ath12k_base *ab)
{
	struct hal_srng *srng;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	int i, ret;

	srng = &ab->hal.srng_list[dp_wifi8->sam_cmd_ring.ring_id];
	ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, -1,
					       HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO);

	if (ret < 0)
		ath12k_warn(ab, "failed to send SAM mpdu clear command: %d\n", ret);

	ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, -1,
					       HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO);

	if (ret < 0)
		ath12k_warn(ab, "failed to send SAM msdu clear command: %d\n", ret);

	/* Send peer clear command over all links.
	 * TODO: In v2 hardware, a new link_mask field allows a single SAM command
	 * to be sent for all links, with bits set for each link.
	 */
	for (i = 0; i < HAL_TX_NUM_MAX_LINKS; i++) {
		ret = ath12k_wifi8_hal_tx_sam_cmd_send(ab, srng, i,
						       HAL_SAM_PEER_CLEAR_PROGRAMMING_BO);

		if (ret < 0)
			ath12k_warn(ab,
				    "failed to send SAM peer clear command for link %d: %d\n",
				    i, ret);
	}
}

void ath12k_wifi8_hal_tx_sam_status(struct ath12k_base *ab, struct hal_tlv_64_hdr *tlv)
{
	struct hal_sam_cmd_status *desc = (struct hal_sam_cmd_status *)tlv->value;
	int cmd_num = le32_get_bits(desc->status_hdr.info0,
				    HAL_SAM_STATUS_CMD_NUMBER);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "cmd_num %d\n", cmd_num);
}

int ath12k_wifi8_hal_sam_cmd_staging_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	/* Allocate single staging buffer for the SAM command ring. */
	dp_wifi8->sam_cmd_staging = kzalloc(HAL_SAM_CMD_MAX_BYTES, GFP_ATOMIC);

	if (!dp_wifi8->sam_cmd_staging)
		return -ENOMEM;

	return 0;
}

void ath12k_wifi8_hal_sam_cmd_staging_free(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	kfree(dp_wifi8->sam_cmd_staging);
	dp_wifi8->sam_cmd_staging = NULL;
}
