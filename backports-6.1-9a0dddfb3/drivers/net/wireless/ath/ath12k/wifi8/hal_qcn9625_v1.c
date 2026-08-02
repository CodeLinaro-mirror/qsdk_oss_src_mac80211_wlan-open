// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * QCN9625 / QCN9589 HW1.0 (E3R65) monitor ops — self-contained.
 *
 * HW1.0 silicon is frozen. The only genuine v1.0-vs-v2.0 (E3R86) silicon
 * divergences are in the RX monitor MPDU-start path:
 *
 *   1. struct hal_rx_mpdu_start — info0 sits one word earlier on v1.0
 *      (see hal_rx_mpdu_start_hw10 below).
 *   2. struct hal_rx_mon_mpdu_start_compact — matching member reorder
 *      (see hal_rx_mon_mpdu_start_compact_hw10 below).
 *   3. The RX_MON_MPDU_START WMASK encrypt_type selector is BIT(0) on v1.0
 *      vs BIT(1) on v2.0 (see RX_MON_MPDU_START_WMASK_HW10 below).
 *
 * These feed exactly two hal_mon_ops entries: rx_mpdu_start_info_get and
 * get_mon_mpdu_start_wmask, which the HW1.0 mon-ops-init helper below overrides
 * on top of the generic hal_qcn9625_mon_ops_base table (defined in
 * hal_mon_qcn9625.c). Every other entry comes from that base, so no monitor op
 * can be silently dropped from the v1.0 table.
 *
 * This file compiles against the live (v2.0) wifi8/ headers. The _hw10
 * structs and the two get_nrp/decap helpers below are local to this file.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "hal_mon.h"
#include "hal_qcn9625.h"

/* v1.0 descriptor layouts. Field-extraction masks (HAL_RX_MPDU_START_*) are
 * identical to v2.0 — only the word ordering differs.
 */
struct hal_rx_mpdu_start_hw10 {
	__le32 rsvd0;
	__le32 info0;
	__le32 rsvd1[6];
	__le32 info1;
	__le32 rsvd2;
	__le16 rsvd3;
	__le16 sw_peer_id;
	__le16 info2;
	__le16 phy_ppdu_id;
	__le32 info3;
	__le32 info4;
	__le16 mpdu_frame_control_field;
	__le16 rsvd4;
	__le32 rsvd5;
	__le16 rsvd6;
	__le16 mac_addr_ad2_15_0;
	__le32 mac_addr_ad2_47_16;
	__le32 rsvd7[20];
} __packed;

struct hal_rx_mon_mpdu_start_compact_hw10 {
	__le32 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 rsvd1;
	__le16 rsvd2;
	__le16 sw_peer_id;
	__le16 info2;
	__le16 phy_ppdu_id;
	__le32 info3;
	__le32 info4;
	__le16 mpdu_frame_control_field;
	__le16 rsvd3;
	__le32 rsvd4;
	__le16 rsvd5;
	__le16 mac_addr_ad2_15_0;
	__le32 mac_addr_ad2_47_16;
} __packed;

/*
 * v1.0 WMASK: the encrypt_type selector is BIT(0), vs BIT(1) on v2.0. All
 * other selectors match v2.0 and are reused from hal_mon.h.
 */
#define MPDU_START_SELECT_INFO0_ENCYRPT_TYP_HW10		BIT(0)

#define RX_MON_MPDU_START_WMASK_HW10					\
		(MPDU_START_SELECT_INFO0_ENCYRPT_TYP_HW10 |		\
		 MPDU_START_SELECT_INFO1_FC_ADDR2_DS_RETRY |		\
		 MPDU_START_SELECT_INFO2 |				\
		 MPDU_START_SELECT_INFO3_INFO4 |			\
		 MPDU_START_SELECT_FC |					\
		 MPDU_START_SELECT_AD2)

/*
 * Local copies of two static helpers from hal_mon.c. They are not exported in
 * any header, and their shared home would be hal_mon.h — but hal_mon.h does not
 * include dp_rx.h (DP_RX_DECAP_TYPE_RAW) or hal_mon_cmn.h (hal_rx_mon_ppdu_info)
 * that set_decap_type_raw_mode needs, so copying here avoids adding those
 * includes to hal_mon.h just for this file.
 */
static __always_inline void
ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(u16 addr_l16, u32 addr_h32, u8 *addr)
{
	memcpy(addr, &addr_l16, 2);
	memcpy(addr + 2, &addr_h32, ETH_ALEN - 2);
}

static void
ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode_hw10(
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 user_id = ppdu_info->user_id;

	if (ppdu_info->nrp_info.fc_valid && ppdu_info->mpdu_info[user_id].decap_type) {
		if (ieee80211_is_mgmt(ppdu_info->nrp_info.frame_control) ||
		    ieee80211_is_ctl(ppdu_info->nrp_info.frame_control) ||
		    ppdu_info->grp_id == HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA) {
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		}
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_hw10(const void *tlv_data,
						 u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mpdu_start_hw10 *mpdu_start =
		(struct hal_rx_mpdu_start_hw10 *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_TO_DS);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_FC_VALID);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->mpdu_info[user_id].raw_mpdu =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU);
		if (ppdu_info->mpdu_info[user_id].raw_mpdu)
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		else
			ppdu_info->mpdu_info[user_id].decap_type =
				u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE);
	}

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);
	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(addr_16, addr_32,
							   ppdu_info->nrp_info.mac_addr2);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userid = userid;
		ppdu_info->userstats[userid].sw_peer_id = peer_id;
		ppdu_info->userstats[userid].ampdu_id =
			le16_to_cpu(mpdu_start->phy_ppdu_id);
		ppdu_info->userstats[userid].filter_category =
			u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_FILTER_CAT);
		ppdu_info->userstats[userid].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
		ppdu_info->userstats[userid].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[userid].frame_control =
				ppdu_info->nrp_info.frame_control;
	}

	ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode_hw10(ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact_hw10(
					const void *tlv_data, u32 userid,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_mpdu_start_compact_hw10 *mpdu_start =
		(struct hal_rx_mon_mpdu_start_compact_hw10 *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID_CMPCT);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID_CMPCT);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_TO_DS_CMPCT);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_FC_VALID_CMPCT);
	ppdu_info->mpdu_info[user_id].raw_mpdu =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU_CMPCT);
	if (ppdu_info->mpdu_info[user_id].raw_mpdu)
		ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
	else
		ppdu_info->mpdu_info[user_id].decap_type =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE_CMPCT);

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN_CMPCT);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST_CMPCT);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);

	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(addr_16, addr_32,
							   ppdu_info->nrp_info.mac_addr2);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userid = userid;
		ppdu_info->userstats[userid].sw_peer_id = peer_id;
		ppdu_info->userstats[userid].ampdu_id =
			le16_to_cpu(mpdu_start->phy_ppdu_id);
		ppdu_info->userstats[userid].filter_category =
			u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_FILTER_CAT_CMPCT);
		ppdu_info->userstats[userid].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
		ppdu_info->userstats[userid].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[userid].frame_control =
				ppdu_info->nrp_info.frame_control;
	}
}

static u32
ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get_hw10(void)
{
	return RX_MON_MPDU_START_WMASK_HW10;
}

static void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse_hw10(const void *tlv_data,
						   u32 userid,
						   struct hal_rx_mon_ppdu_info *ppdu_info,
						   u32 tlv_len)
{
	if (likely(tlv_len < sizeof(struct hal_rx_mpdu_start_hw10)))
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact_hw10(tlv_data,
									 userid,
									 ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_hw10(tlv_data, userid,
								 ppdu_info);
}

/*
 * HW1.0 monitor ops. Start from the generic QCN9625 base table and override
 * only the two version-specific entries. hal_qcn9625_mon_ops_base is defined in
 * hal_mon_qcn9625.c and holds every shared entry, so those cannot drift or be
 * silently dropped from the HW1.0 table.
 */
static struct hal_mon_ops hal_qcn9625_hw10_mon_ops;

void ath12k_wifi8_hal_qcn9625_hw10_mon_ops_init(struct ath12k_hal *hal)
{
	hal_qcn9625_hw10_mon_ops = hal_qcn9625_mon_ops_base;
	hal_qcn9625_hw10_mon_ops.get_mon_mpdu_start_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get_hw10;
	hal_qcn9625_hw10_mon_ops.rx_mpdu_start_info_get =
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse_hw10;

	hal->hal_mon_ops = &hal_qcn9625_hw10_mon_ops;
}
