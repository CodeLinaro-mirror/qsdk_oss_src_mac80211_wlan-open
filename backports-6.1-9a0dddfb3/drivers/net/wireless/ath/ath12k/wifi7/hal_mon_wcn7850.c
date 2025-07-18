// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "hal_mon.h"

static void
ath12k_wifi7_hal_mon_mpdu_start_info_get_wcn7850(const void *tlv_data, u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mpdu_start *mpdu_start =
		(struct hal_rx_mpdu_start *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[9], addr_32;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = __le32_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);
	info[5] = __le32_to_cpu(mpdu_start->info5);
	info[6] = __le32_to_cpu(mpdu_start->info6);
	info[7] = __le32_to_cpu(mpdu_start->info7);
	info[8] = __le32_to_cpu(mpdu_start->info8);

	ppdu_info->grp_id = u32_get_bits(info[1],
					 HAL_RX_MPDU_START_INFO1_SW_GRP_ID);

	peer_id = u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_PEERID);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[3],
					      HAL_RX_MPDU_START_INFO3_MPDU_RETRY);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_ADDR2_VALID);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[3],
			     HAL_RX_MPDU_START_INFO3_TO_DS);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[3],
			     HAL_RX_MPDU_START_INFO3_FC_VALID);

	ppdu_info->mpdu_len += u32_get_bits(info[5],
					    HAL_RX_MPDU_START_INFO5_MPDU_LEN);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[5], HAL_RX_MPDU_START_INFO5_MCAST_BCAST);
	ppdu_info->nrp_info.frame_control =
		u32_get_bits(info[6],
			     HAL_RX_MPDU_START_INFO6_FC_FIELD);
	addr_16 = u32_get_bits(info[7],
			       HAL_RX_MPDU_START_INFO7_ADDR2_15_0);
	addr_32 = u32_get_bits(info[8],
			       HAL_RX_MPDU_START_INFO8_ADDR2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi7_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
						      ppdu_info->nrp_info.mac_addr2);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userid = userid;
		ppdu_info->userstats[userid].sw_peer_id = peer_id;
		ppdu_info->userstats[userid].ampdu_id =
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_PPDU_ID);
		ppdu_info->userstats[userid].filter_category =
			u32_get_bits(info[1],
				     HAL_RX_MPDU_START_INFO1_FILTER_CAT);
		ppdu_info->userstats[userid].mpdu_retry +=
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_MPDU_RETRY);
		ppdu_info->userstats[userid].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[userid].frame_control =
				ppdu_info->nrp_info.frame_control;
	}
}

static void
ath12k_wifi7_hal_mon_msdu_end_info_get_wcn7850(const void *tlv_data, u32 userid,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_msdu_end *msdu_end =
		(struct hal_rx_msdu_end *)tlv_data;
	u32 info[3];

	info[0] = __le32_to_cpu(msdu_end->info0);
	info[1] = __le32_to_cpu(msdu_end->info1);
	info[2] = __le32_to_cpu(msdu_end->info2);

	ppdu_info->grp_id = u32_get_bits(info[0],
					 HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID);

	ppdu_info->decap_format = u32_get_bits(info[1],
					       HAL_RX_MSDU_END_INFO1_DECAP_FORMAT);
	ath12k_wifi7_hal_mon_parse_rx_msdu_end_err(info[2],
						   &ppdu_info->errmap);
}

static __always_inline void
ath12k_wifi7_hal_mon_handle_ofdma_info_wcn7850(u32 *info,
					       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = info[12];
	rx_user_status->ul_ofdma_user_v0_word1 = info[13];
}

static __always_inline void
ath12k_wifi7_hal_mon_populate_byte_count_wcn7850(u32 *info,
						 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[8],
			     HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[9],
			     HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT);
}

static void
ath12k_wifi7_hal_mon_ppdu_eu_stats_info_get_wcn7850(const void *tlv_data, u32 userid,
						    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_ppdu_end_user_stats *ppdu_eu_stats =
		(struct hal_rx_ppdu_end_user_stats *)tlv_data;
	u32 tid_bitmap;
	u32 info[14];

	info[0] = __le32_to_cpu(ppdu_eu_stats->info0);
	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);
	info[6] = __le32_to_cpu(ppdu_eu_stats->info6);
	info[7] = __le32_to_cpu(ppdu_eu_stats->info7);
	info[8] = __le32_to_cpu(ppdu_eu_stats->info8);
	info[9] = __le32_to_cpu(ppdu_eu_stats->info9);
	info[10] = __le32_to_cpu(ppdu_eu_stats->info10);
	info[11] = __le32_to_cpu(ppdu_eu_stats->info11);
	info[12] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
	info[13] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);

	ppdu_info->num_mpdu_fcs_err =
		u32_get_bits(info[1],
			      HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR);
	ppdu_info->peer_id =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK);
	ppdu_info->ast_index =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX);
	ppdu_info->tcp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT);
	ppdu_info->udp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT);
	ppdu_info->other_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT);
	ppdu_info->tcp_ack_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT);
	tid_bitmap = u32_get_bits(info[7],
				  HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[11],
			     HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT);
	switch (ppdu_info->preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		ppdu_info->ht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AC:
		ppdu_info->vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->he_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11BE:
		ppdu_info->is_eht = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];
		ath12k_wifi7_hal_mon_handle_ofdma_info_wcn7850(info,
							       rxuser_stats);
		ath12k_wifi7_hal_mon_populate_byte_count_wcn7850(info,
								 rxuser_stats);
		rxuser_stats->retried_msdu_count =
			u32_get_bits(info[10],
				     HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT);
	}
}

const struct hal_mon_ops hal_wcn7850_mon_ops = {
	.rx_mpdu_start_info_get = ath12k_wifi7_hal_mon_mpdu_start_info_get_wcn7850,
	.rx_msdu_end_info_get = ath12k_wifi7_hal_mon_msdu_end_info_get_wcn7850,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_ppdu_eu_stats_info_get_wcn7850,
};
