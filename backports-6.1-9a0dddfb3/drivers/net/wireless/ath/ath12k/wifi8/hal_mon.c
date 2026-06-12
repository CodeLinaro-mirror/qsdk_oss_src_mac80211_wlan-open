// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_rx.h"
#include "hal_rx_desc.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "hal_mon.h"
#include "hal_qcn9625.h"

#include "dp_rx.h"
#include "../hw.h"

extern const struct hal_mon_ops hal_qcn9625_mon_ops;

u32 ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get(void)
{
	return RX_MON_MPDU_START_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_mpdu_end_wmask_get(void)
{
	return RX_MON_MPDU_END_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_msdu_end_wmask_get(void)
{
	return RX_MON_MSDU_END_WMASK;
}

u32 ath12k_wifi8_hal_mon_rx_ppdu_end_usr_stats_wmask_get(void)
{
	return RX_MON_PPDU_END_USER_STATS_WMASK;
}

static __always_inline void
ath12k_wifi8_hal_mon_get_mac_addr(u32 addr_l32, u16 addr_h16, u8 *addr)
{
	memcpy(addr, &addr_l32, 4);
	memcpy(addr + 4, &addr_h16, ETH_ALEN - 4);
}

static __always_inline void
ath12k_wifi8_hal_mon_get_nrp_mac_addr(u16 addr_l16, u32 addr_h32, u8 *addr)
{
	memcpy(addr, &addr_l16, 2);
	memcpy(addr + 2, &addr_h32, ETH_ALEN - 2);
}

static __always_inline void
ath12k_wifi8_hal_mon_handle_ofdma_info(u32 *info,
				       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = info[12];
	rx_user_status->ul_ofdma_user_v0_word1 = info[13];
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_byte_count(u32 *info,
					 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[8],
			     HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[9],
			     HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT);
}

static __always_inline void
ath12k_wifi8_hal_mon_handle_ofdma_info_compact(u32 *info,
					       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = info[12];
	rx_user_status->ul_ofdma_user_v0_word1 = info[13];
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_byte_count_compact(
					u32 *info,
					struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[8],
			     HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT_CMPCT);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[9],
			     HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT_CMPCT);
}

static __always_inline void
ath12k_wifi8_hal_mon_populate_mu_user_info(struct hal_rx_mon_ppdu_info *ppdu_info,
					   struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ast_index = ppdu_info->ast_index;
	rx_user_status->tid = ppdu_info->tid;
	rx_user_status->tcp_ack_msdu_count =
		ppdu_info->tcp_ack_msdu_count;
	rx_user_status->tcp_msdu_count =
		ppdu_info->tcp_msdu_count;
	rx_user_status->udp_msdu_count =
		ppdu_info->udp_msdu_count;
	rx_user_status->other_msdu_count =
		ppdu_info->other_msdu_count;
	rx_user_status->frame_control = ppdu_info->frame_control;
	rx_user_status->frame_control_info_valid =
		ppdu_info->frame_control_info_valid;
	rx_user_status->data_sequence_control_info_valid =
		ppdu_info->data_sequence_control_info_valid;
	rx_user_status->first_data_seq_ctrl =
		ppdu_info->first_data_seq_ctrl;
	rx_user_status->preamble_type = ppdu_info->preamble_type;
	rx_user_status->ht_flags = ppdu_info->ht_flags;
	rx_user_status->vht_flags = ppdu_info->vht_flags;
	rx_user_status->he_flags = ppdu_info->he_flags;
	rx_user_status->rs_flags = ppdu_info->rs_flags;

	rx_user_status->mpdu_cnt_fcs_ok =
		ppdu_info->num_mpdu_fcs_ok;
	rx_user_status->mpdu_cnt_fcs_err =
		ppdu_info->num_mpdu_fcs_err;
	rx_user_status->retried_msdu_count =
		ppdu_info->retried_msdu_count;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_vht_sig_a(const struct hal_rx_vht_sig_a_info *vht_sig,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 nsts, info0, info1;
	u8 gi_setting;

	info0 = __le32_to_cpu(vht_sig->info0);
	info1 = __le32_to_cpu(vht_sig->info1);

	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_SU_MU_CODING);
	ppdu_info->mcs = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_MCS);
	gi_setting = u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_GI_SETTING);

	switch (gi_setting) {
	case HAL_RX_VHT_SIG_A_NORMAL_GI:
		ppdu_info->gi = HAL_RX_GI_0_8_US;
		break;
	case HAL_RX_VHT_SIG_A_SHORT_GI:
	case HAL_RX_VHT_SIG_A_SHORT_GI_AMBIGUITY:
		ppdu_info->gi = HAL_RX_GI_0_4_US;
		break;
	}

	ppdu_info->is_stbc = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_STBC);
	nsts = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_NSTS);
	if (ppdu_info->is_stbc && nsts > 0)
		nsts = ((nsts + 1) >> 1) - 1;

	ppdu_info->nss = u32_get_bits(nsts, VHT_SIG_SU_NSS_MASK) + 1;
	ppdu_info->bw = u32_get_bits(info0, HAL_RX_VHT_SIG_A_INFO_INFO0_BW);
	ppdu_info->beamformed = u32_get_bits(info1,
					     HAL_RX_VHT_SIG_A_INFO_INFO1_BEAMFORMED);
	ppdu_info->vht_flag_values5 = u32_get_bits(info0,
						   HAL_RX_VHT_SIG_A_INFO_INFO0_GROUP_ID);
	ppdu_info->vht_flag_values3[0] = (((ppdu_info->mcs) << 4) |
					    ppdu_info->nss);
	ppdu_info->vht_flag_values2 = ppdu_info->bw;
	ppdu_info->vht_flag_values4 =
		u32_get_bits(info1, HAL_RX_VHT_SIG_A_INFO_INFO1_SU_MU_CODING);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_ht_sig(const struct hal_rx_ht_sig_info *ht_sig,
				  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(ht_sig->info0);
	u32 info1 = __le32_to_cpu(ht_sig->info1);

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HT_SIG_INFO_INFO0_MCS);
	ppdu_info->bw = u32_get_bits(info0, HAL_RX_HT_SIG_INFO_INFO0_BW);
	ppdu_info->is_stbc = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_STBC);
	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_FEC_CODING);
	ppdu_info->gi = u32_get_bits(info1, HAL_RX_HT_SIG_INFO_INFO1_GI);
	ppdu_info->nss = (ppdu_info->mcs >> 3) + 1;
}

static __always_inline u8
ath12k_wifi8_hal_mon_map_legacy_rate_to_hw_rate(u8 rate)
{
	u8 ath12k_rate;

	/* Map hal_rx_legacy_rate to ath12k_hw_rate_cck */
	switch (rate) {
	case HAL_RX_LEGACY_RATE_LP_1_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_1M;
		break;
	case HAL_RX_LEGACY_RATE_LP_2_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_2M;
		break;
	case HAL_RX_LEGACY_RATE_LP_5_5_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_5_5M;
		break;
	case HAL_RX_LEGACY_RATE_LP_11_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_LP_11M;
		break;
	case HAL_RX_LEGACY_RATE_SP_2_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_2M;
		break;
	case HAL_RX_LEGACY_RATE_SP_5_5_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_5_5M;
		break;
	case HAL_RX_LEGACY_RATE_SP_11_MBPS:
		ath12k_rate = ATH12K_HW_RATE_CCK_SP_11M;
		break;
	default:
		ath12k_rate = rate;
		break;
	}

	return ath12k_rate;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_l_sig_b(const struct hal_rx_lsig_b_info *lsigb,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(lsigb->info0);
	u8 rate;

	rate = u32_get_bits(info0, HAL_RX_LSIG_B_INFO_INFO0_RATE);

	switch (rate) {
	case 1:
		rate = HAL_RX_LEGACY_RATE_LP_1_MBPS;
		break;
	case 2:
		rate = HAL_RX_LEGACY_RATE_LP_2_MBPS;
		break;
	case 3:
		rate = HAL_RX_LEGACY_RATE_LP_5_5_MBPS;
		break;
	case 4:
		rate = HAL_RX_LEGACY_RATE_LP_11_MBPS;
		break;
	case 5:
		rate = HAL_RX_LEGACY_RATE_SP_2_MBPS;
		break;
	case 6:
		rate = HAL_RX_LEGACY_RATE_SP_5_5_MBPS;
		break;
	case 7:
		rate = HAL_RX_LEGACY_RATE_SP_11_MBPS;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_INVALID;
		break;
	}

	ppdu_info->rate = ath12k_wifi8_hal_mon_map_legacy_rate_to_hw_rate(rate);
	ppdu_info->cck_flag = 1;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_l_sig_a(const struct hal_rx_lsig_a_info *lsiga,
				   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(lsiga->info0);
	u8 rate;

	rate = u32_get_bits(info0, HAL_RX_LSIG_A_INFO_INFO0_RATE);

	switch (rate) {
	case 8:
		rate = HAL_RX_LEGACY_RATE_OFDM_48_MBPS;
		break;
	case 9:
		rate = HAL_RX_LEGACY_RATE_OFDM_24_MBPS;
		break;
	case 10:
		rate = HAL_RX_LEGACY_RATE_OFDM_12_MBPS;
		break;
	case 11:
		rate = HAL_RX_LEGACY_RATE_OFDM_6_MBPS;
		break;
	case 12:
		rate = HAL_RX_LEGACY_RATE_OFDM_54_MBPS;
		break;
	case 13:
		rate = HAL_RX_LEGACY_RATE_OFDM_36_MBPS;
		break;
	case 14:
		rate = HAL_RX_LEGACY_RATE_OFDM_18_MBPS;
		break;
	case 15:
		rate = HAL_RX_LEGACY_RATE_OFDM_9_MBPS;
		break;
	default:
		rate = HAL_RX_LEGACY_RATE_OFDM_INVALID;
		break;
	}

	ppdu_info->rate = rate;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b2_ofdma(
				const struct hal_rx_he_sig_b2_ofdma_info *ofdma,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, value;

	info0 = __le32_to_cpu(ofdma->info0);

	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_DCM_KNOWN | HE_CODING_KNOWN;

	/* HE-data2 */
	ppdu_info->he_data2 |= HE_TXBF_KNOWN;

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_MCS);
	value = ppdu_info->mcs << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_DCM);
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_CODING);
	ppdu_info->ldpc = value;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	/* HE-data4 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	ppdu_info->nss = u32_get_bits(info0,
				      HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_NSTS) + 1;
	ppdu_info->beamformed = u32_get_bits(info0,
					     HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_TXBF);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b2_mu(
				const struct hal_rx_he_sig_b2_mu_info *he_sig_b2_mu,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, value;

	info0 = __le32_to_cpu(he_sig_b2_mu->info0);

	ppdu_info->he_data1 |= HE_MCS_KNOWN | HE_CODING_KNOWN;

	ppdu_info->mcs = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_MCS);
	value = ppdu_info->mcs << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_CODING);
	ppdu_info->ldpc = value;
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_ID);
	value = value << HE_STA_ID_SHIFT;
	ppdu_info->he_data4 |= value;

	ppdu_info->nss = u32_get_bits(info0, HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_NSTS) + 1;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_b1_mu(
				const struct hal_rx_he_sig_b1_mu_info *he_sig_b1_mu,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0 = __le32_to_cpu(he_sig_b1_mu->info0);
	u16 ru_tones;

	ru_tones = u32_get_bits(info0,
				HAL_RX_HE_SIG_B1_MU_INFO_INFO0_RU_ALLOCATION);
	ppdu_info->ru_alloc = ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	ppdu_info->he_RU[0] = ru_tones;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_mu(
				const struct hal_rx_he_sig_a_mu_dl_info *he_sig_a_mu_dl,
				struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, info1, value;
	u16 he_gi = 0, he_ltf = 0;

	info0 = __le32_to_cpu(he_sig_a_mu_dl->info0);
	info1 = __le32_to_cpu(he_sig_a_mu_dl->info1);

	ppdu_info->he_mu_flags = 1;

	ppdu_info->he_data1 = HE_MU_FORMAT_TYPE;
	ppdu_info->he_data1 |=
			HE_BSS_COLOR_KNOWN |
			HE_DL_UL_KNOWN |
			HE_LDPC_EXTRA_SYMBOL_KNOWN |
			HE_STBC_KNOWN |
			HE_DATA_BW_RU_KNOWN |
			HE_DOPPLER_KNOWN;

	ppdu_info->he_data2 =
			HE_GI_KNOWN |
			HE_LTF_SYMBOLS_KNOWN |
			HE_PRE_FEC_PADDING_KNOWN |
			HE_PE_DISAMBIGUITY_KNOWN |
			HE_TXOP_KNOWN |
			HE_MIDABLE_PERIODICITY_KNOWN;

	/* data3 */
	ppdu_info->he_data3 = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_BSS_COLOR);
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_LDPC_EXTRA);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_STBC);
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	ppdu_info->he_data4 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_MU_DL_INFO0_SPATIAL_REUSE);
	ppdu_info->he_data4 = value;

	/* data5 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_CP_LTF_SIZE);

	switch (value) {
	case 0:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_4_X;
		break;
	case 1:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_2_X;
		break;
	case 2:
		he_gi = HE_GI_1_6;
		he_ltf = HE_LTF_2_X;
		break;
	case 3:
		he_gi = HE_GI_3_2;
		he_ltf = HE_LTF_4_X;
		break;
	}

	ppdu_info->gi = he_gi;
	hal_get_radiotap_he_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;

	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_NUM_LTF_SYMB);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_PE_DISAM);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/*data6*/
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_DOPPLER_INDICATION);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_MU_DL_INFO1_TXOP_DURATION);
	value = value << HE_TXOP_SHIFT;
	ppdu_info->he_data6 |= value;

	/* HE-MU Flags */
	/* HE-MU-flags1 */
	ppdu_info->he_flags1 =
		HE_SIG_B_MCS_KNOWN |
		HE_SIG_B_DCM_KNOWN |
		HE_SIG_B_COMPRESSION_FLAG_1_KNOWN |
		HE_SIG_B_SYM_NUM_KNOWN |
		HE_RU_0_KNOWN;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_MCS_OF_SIGB);
	ppdu_info->he_flags1 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_DCM_OF_SIGB);
	value = value << HE_DCM_FLAG_1_SHIFT;
	ppdu_info->he_flags1 |= value;

	/* HE-MU-flags2 */
	ppdu_info->he_flags2 = HE_BW_KNOWN;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW);
	ppdu_info->he_flags2 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_COMP_MODE_SIGB);
	value = value << HE_SIG_B_COMPRESSION_FLAG_2_SHIFT;
	ppdu_info->he_flags2 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_MU_DL_INFO0_NUM_SIGB_SYMB);
	value = value - 1;
	value = value << HE_NUM_SIG_B_SYMBOLS_SHIFT;
	ppdu_info->he_flags2 |= value;

	ppdu_info->is_stbc = info1 &
			     HAL_RX_HE_SIG_A_MU_DL_INFO1_STBC;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_he_sig_su(const struct hal_rx_he_sig_a_su_info *he_sig_a,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 info0, info1, value;
	u32 dcm;
	u8 he_dcm = 0, he_stbc = 0;
	u16 he_gi = 0, he_ltf = 0;

	ppdu_info->he_flags = 1;

	info0 = __le32_to_cpu(he_sig_a->info0);
	info1 = __le32_to_cpu(he_sig_a->info1);

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_FORMAT_IND);
	if (value == 0)
		ppdu_info->he_data1 = HE_TRIG_FORMAT_TYPE;
	else
		ppdu_info->he_data1 = HE_SU_FORMAT_TYPE;

	ppdu_info->he_data1 |=
			HE_BSS_COLOR_KNOWN |
			HE_BEAM_CHANGE_KNOWN |
			HE_DL_UL_KNOWN |
			HE_MCS_KNOWN |
			HE_DCM_KNOWN |
			HE_CODING_KNOWN |
			HE_LDPC_EXTRA_SYMBOL_KNOWN |
			HE_STBC_KNOWN |
			HE_DATA_BW_RU_KNOWN |
			HE_DOPPLER_KNOWN;

	ppdu_info->he_data2 |=
			HE_GI_KNOWN |
			HE_TXBF_KNOWN |
			HE_PE_DISAMBIGUITY_KNOWN |
			HE_TXOP_KNOWN |
			HE_LTF_SYMBOLS_KNOWN |
			HE_PRE_FEC_PADDING_KNOWN |
			HE_MIDABLE_PERIODICITY_KNOWN;

	ppdu_info->he_data3 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_SU_INFO_INFO0_BSS_COLOR);
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_BEAM_CHANGE);
	value = value << HE_BEAM_CHANGE_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DL_UL_FLAG);
	value = value << HE_DL_UL_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_MCS);
	ppdu_info->mcs = value;
	value = value << HE_TRANSMIT_MCS_SHIFT;
	ppdu_info->he_data3 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DCM);
	he_dcm = value;
	value = value << HE_DCM_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_CODING);
	value = value << HE_CODING_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_LDPC_EXTRA);
	value = value << HE_LDPC_EXTRA_SYMBOL_SHIFT;
	ppdu_info->he_data3 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_STBC);
	he_stbc = value;
	value = value << HE_STBC_SHIFT;
	ppdu_info->he_data3 |= value;

	/* data4 */
	ppdu_info->he_data4 = u32_get_bits(info0,
					   HAL_RX_HE_SIG_A_SU_INFO_INFO0_SPATIAL_REUSE);

	/* data5 */
	value = u32_get_bits(info0,
			     HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_BW);
	ppdu_info->he_data5 = value;
	ppdu_info->bw = value;
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_CP_LTF_SIZE);

	switch (value) {
	case 0:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_1_X;
		break;
	case 1:
		he_gi = HE_GI_0_8;
		he_ltf = HE_LTF_2_X;
		break;
	case 2:
		he_gi = HE_GI_1_6;
		he_ltf = HE_LTF_2_X;
		break;
	case 3:
		if (he_dcm && he_stbc) {
			he_gi = HE_GI_0_8;
			he_ltf = HE_LTF_4_X;
		} else {
			he_gi = HE_GI_3_2;
			he_ltf = HE_LTF_4_X;
		}
		break;
	}
	ppdu_info->gi = he_gi;
	ppdu_info->ltf_size = he_ltf;
	hal_get_radiotap_he_gi_ltf(&he_gi, &he_ltf);
	value = he_gi << HE_GI_SHIFT;
	ppdu_info->he_data5 |= value;
	value = he_ltf << HE_LTF_SIZE_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS);
	value = (value << HE_LTF_SYM_SHIFT);
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_FACTOR);
	value = value << HE_PRE_FEC_PAD_SHIFT;
	ppdu_info->he_data5 |= value;

	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXBF);
	value = value << HE_TXBF_SHIFT;
	ppdu_info->he_data5 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_PE_DISAM);
	value = value << HE_PE_DISAMBIGUITY_SHIFT;
	ppdu_info->he_data5 |= value;

	/* data6 */
	value = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS);
	value++;
	ppdu_info->he_data6 = value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_DOPPLER_IND);
	value = value << HE_DOPPLER_SHIFT;
	ppdu_info->he_data6 |= value;
	value = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXOP_DURATION);
	value = value << HE_TXOP_SHIFT;
	ppdu_info->he_data6 |= value;

	ppdu_info->mcs =
		u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_MCS);
	ppdu_info->bw =
		u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_BW);
	ppdu_info->ldpc = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_CODING);
	ppdu_info->is_stbc = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_STBC);
	ppdu_info->beamformed = u32_get_bits(info1, HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXBF);
	dcm = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_DCM);
	ppdu_info->nss = u32_get_bits(info0, HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS) + 1;
	ppdu_info->dcm = dcm;
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_get_phy_version(const struct hal_mon_usig_cmn *cmn,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	ppdu_info->u_sig_info.phy_version =
			le32_get_bits(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_cmn(const struct hal_mon_usig_cmn *cmn,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 common;

	ppdu_info->u_sig_info.bw = le32_get_bits(cmn->info0,
						 HAL_RX_USIG_CMN_INFO0_BW);
	ppdu_info->u_sig_info.ul_dl = le32_get_bits(cmn->info0,
						    HAL_RX_USIG_CMN_INFO0_UL_DL);

	common = __le32_to_cpu(ppdu_info->u_sig_info.usig.common);
	common |= IEEE80211_RADIOTAP_EHT_USIG_COMMON_PHY_VER_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BW_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_UL_DL_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BSS_COLOR_KNOWN |
		  IEEE80211_RADIOTAP_EHT_USIG_COMMON_TXOP_KNOWN |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_PHY_VER) |
		  u32_encode_bits(ppdu_info->u_sig_info.bw,
				  IEEE80211_RADIOTAP_EHT_USIG_COMMON_BW) |
		  u32_encode_bits(ppdu_info->u_sig_info.ul_dl,
				  IEEE80211_RADIOTAP_EHT_USIG_COMMON_UL_DL) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BSS_COLOR,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_BSS_COLOR) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_TXOP,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_TXOP);
	ppdu_info->u_sig_info.usig.common = cpu_to_le32(common);

	switch (ppdu_info->u_sig_info.bw) {
	default:
		fallthrough;
	case HAL_BW_20:
		ppdu_info->bw = HAL_RX_BW_20MHZ;
		break;
	case HAL_BW_40:
		ppdu_info->bw = HAL_RX_BW_40MHZ;
		break;
	case HAL_BW_80:
		ppdu_info->bw = HAL_RX_BW_80MHZ;
		break;
	case HAL_BW_160:
		ppdu_info->bw = HAL_RX_BW_160MHZ;
		break;
	case HAL_BW_320_1:
	case HAL_BW_320_2:
		ppdu_info->bw = HAL_RX_BW_320MHZ;
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_cmn(const struct hal_mon_usig_cmn *cmn,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 common;

	ppdu_info->u_sig_info.phy_version =
			le32_get_bits(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION);
	ppdu_info->u_sig_info.bw = le32_get_bits(cmn->info0,
						 HAL_RX_USIG_CMN_INFO0_BW);
	ppdu_info->u_sig_info.ul_dl = le32_get_bits(cmn->info0,
						    HAL_RX_USIG_CMN_INFO0_UL_DL);

	common = __le32_to_cpu(ppdu_info->u_sig_info.uhr_usig.common);

	/* until UHR SIG radiotap header stabilizes, we will use EHT enums
	 * for common non-impacted fields
	 */
	common |= IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR_KNOWN |
		  IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP_KNOWN |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_PHY_VERSION,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_PHY_VER) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BW,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BW) |
		  u32_encode_bits(ppdu_info->u_sig_info.ul_dl,
				  IEEE80211_RADIOTAP_UHR_USIG_COMMON_UL_DL) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_BSS_COLOR,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BSS_COLOR) |
		  ATH12K_LE32_DEC_ENC(cmn->info0,
				      HAL_RX_USIG_CMN_INFO0_TXOP,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_TXOP);

	ppdu_info->u_sig_info.uhr_usig.common = cpu_to_le32(common);

	switch (ppdu_info->u_sig_info.bw) {
	default:
		fallthrough;
	case HAL_BW_20:
		ppdu_info->bw = HAL_RX_BW_20MHZ;
		break;
	case HAL_BW_40:
		ppdu_info->bw = HAL_RX_BW_40MHZ;
		break;
	case HAL_BW_80:
		ppdu_info->bw = HAL_RX_BW_80MHZ;
		break;
	case HAL_BW_160:
		ppdu_info->bw = HAL_RX_BW_160MHZ;
		break;
	case HAL_BW_320_1:
	case HAL_BW_320_2:
		ppdu_info->bw = HAL_RX_BW_320MHZ;
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_tb(const struct hal_mon_usig_tb *usig_tb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_eht_usig *usig = &ppdu_info->u_sig_info.usig;
	enum ieee80211_radiotap_eht_usig_tb spatial_reuse1, spatial_reuse2;
	u32 common, value, mask;

	spatial_reuse1 = IEEE80211_RADIOTAP_EHT_USIG2_TB_B3_B6_SPATIAL_REUSE_1;
	spatial_reuse2 = IEEE80211_RADIOTAP_EHT_USIG2_TB_B7_B10_SPATIAL_REUSE_2;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_tb->info0,
					      HAL_RX_USIG_TB_INFO0_PPDU_TYPE_COMP_MODE);

	common |= ATH12K_LE32_DEC_ENC(usig_tb->info0,
				      HAL_RX_USIG_TB_INFO0_RX_INTEG_CHECK_PASS,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_EHT_USIG1_TB_B20_B25_DISREGARD |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_EHT_USIG2_TB_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_EHT_USIG2_TB_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_1,
				     spatial_reuse1) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_2,
				     spatial_reuse2) |
		 IEEE80211_RADIOTAP_EHT_USIG2_TB_B11_B15_DISREGARD |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_CRC,
				     IEEE80211_RADIOTAP_EHT_USIG2_TB_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_TAIL,
				     IEEE80211_RADIOTAP_EHT_USIG2_TB_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_EHT_USIG1_TB_B20_B25_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B2_VALIDATE |
		spatial_reuse1 | spatial_reuse2 |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B11_B15_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B16_B19_CRC |
		IEEE80211_RADIOTAP_EHT_USIG2_TB_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_tb(const struct hal_mon_usig_tb *usig_tb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	enum ieee80211_radiotap_uhr_usig_tb spatial_reuse1, spatial_reuse2;
	u32 common, value, mask;

	spatial_reuse1 = IEEE80211_RADIOTAP_UHR_USIG2_TB_B3_B6_SPATIAL_REUSE_1;
	spatial_reuse2 = IEEE80211_RADIOTAP_UHR_USIG2_TB_B7_B10_SPATIAL_REUSE_2;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_tb->info0,
					      HAL_RX_USIG_TB_INFO0_PPDU_TYPE_COMP_MODE);

	common |= ATH12K_LE32_DEC_ENC(usig_tb->info0,
				      HAL_RX_USIG_TB_INFO0_RX_INTEG_CHECK_PASS,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_1,
				     spatial_reuse1) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_2,
				     spatial_reuse2) |
		 IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_CRC,
				     IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_tb->info0,
				     HAL_RX_USIG_TB_INFO0_TAIL,
				     IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_UHR_USIG1_TB_B20_B25_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B2_VALIDATE |
		spatial_reuse1 | spatial_reuse2 |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B11_B15_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_TB_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_mu(const struct hal_mon_usig_mu *usig_mu,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	enum ieee80211_radiotap_uhr_usig_mu sig_symb, punc;
	u32 common, value, mask, bss_color2_mask;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.cosr_cobf_disable =
		le32_get_bits(usig_mu->info0,
			      HAL_RX_USIG_MU_INFO0_VALIDATE_1_OR_COBF_COSR_DISABLE);

	if (!ppdu_info->u_sig_info.cosr_cobf_disable) {
		bss_color2_mask = IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B25_BSS_COLOR_2;
		value |= ATH12K_LE32_DEC_ENC(usig_mu->info0,
					     HAL_RX_USIG_UHR_CMN_INFO0_BSS_COLOR_2,
					     bss_color2_mask);
	} else {
		value |= IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
			IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;
	}

	sig_symb = IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM;
	punc = IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO;

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);
	ppdu_info->u_sig_info.eht_sig_mcs =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_EHT_SIG_MCS);
	ppdu_info->u_sig_info.num_eht_sig_sym =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_NUM_EHT_SIG_SYM);

	common |= ATH12K_LE32_DEC_ENC(usig_mu->info0,
				      HAL_RX_USIG_MU_INFO0_RX_INTEG_CHECK_PASS,
				      IEEE80211_RADIOTAP_UHR_USIG_COMMON_BAD_USIG_CRC);

	value |= u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE) |
		 u32_encode_bits(ppdu_info->u_sig_info.cosr_cobf_disable,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_PUNC_CH_INFO,
				     punc) |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.eht_sig_mcs,
				 IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS) |
		 u32_encode_bits(ppdu_info->u_sig_info.num_eht_sig_sym,
				 sig_symb) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_CRC,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_TAIL,
				     IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL);

	mask |=  IEEE80211_RADIOTAP_UHR_USIG2_MU_B0_B1_PPDU_TYPE |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B2_COBF_OR_COSR |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B3_B7_PUNCTURED_INFO |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B8_VALIDATE |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B9_B10_SIG_MCS |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B11_B15_NUM_UHR_SYG_SYM |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B16_B19_CRC |
		 IEEE80211_RADIOTAP_UHR_USIG2_MU_B20_B25_TAIL |
		 IEEE80211_RADIOTAP_UHR_USIG1_MU_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_UHR_USIG1_MU_B25_VALIDATE;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_mu(const struct hal_mon_usig_mu *usig_mu,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_eht_usig *usig = &ppdu_info->u_sig_info.usig;
	enum ieee80211_radiotap_eht_usig_mu sig_symb, punc;
	u32 common, value, mask;

	sig_symb = IEEE80211_RADIOTAP_EHT_USIG2_MU_B11_B15_EHT_SIG_SYMBOLS;
	punc = IEEE80211_RADIOTAP_EHT_USIG2_MU_B3_B7_PUNCTURED_INFO;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);
	ppdu_info->u_sig_info.eht_sig_mcs =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_EHT_SIG_MCS);
	ppdu_info->u_sig_info.num_eht_sig_sym =
				le32_get_bits(usig_mu->info0,
					      HAL_RX_USIG_MU_INFO0_NUM_EHT_SIG_SYM);

	common |= ATH12K_LE32_DEC_ENC(usig_mu->info0,
				      HAL_RX_USIG_MU_INFO0_RX_INTEG_CHECK_PASS,
				      IEEE80211_RADIOTAP_EHT_USIG_COMMON_BAD_USIG_CRC);

	value |= IEEE80211_RADIOTAP_EHT_USIG1_MU_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_EHT_USIG1_MU_B25_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_EHT_USIG2_MU_B0_B1_PPDU_TYPE) |
		 IEEE80211_RADIOTAP_EHT_USIG2_MU_B2_VALIDATE |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_PUNC_CH_INFO,
				     punc) |
		 IEEE80211_RADIOTAP_EHT_USIG2_MU_B8_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.eht_sig_mcs,
				 IEEE80211_RADIOTAP_EHT_USIG2_MU_B9_B10_SIG_MCS) |
		 u32_encode_bits(ppdu_info->u_sig_info.num_eht_sig_sym,
				 sig_symb) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_CRC,
				     IEEE80211_RADIOTAP_EHT_USIG2_MU_B16_B19_CRC) |
		 ATH12K_LE32_DEC_ENC(usig_mu->info0,
				     HAL_RX_USIG_MU_INFO0_TAIL,
				     IEEE80211_RADIOTAP_EHT_USIG2_MU_B20_B25_TAIL);

	mask |= IEEE80211_RADIOTAP_EHT_USIG1_MU_B20_B24_DISREGARD |
		IEEE80211_RADIOTAP_EHT_USIG1_MU_B25_VALIDATE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B2_VALIDATE |
		punc |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B8_VALIDATE |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B9_B10_SIG_MCS |
		sig_symb |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B16_B19_CRC |
		IEEE80211_RADIOTAP_EHT_USIG2_MU_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_elr(const struct hal_mon_usig_elr *elr,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ieee80211_radiotap_uhr_usig *usig = &ppdu_info->u_sig_info.uhr_usig;
	u32 common, value, mask;
	u32 elr_valdiate_mask, elr_crc_mask, elr_tail_mask;

	common = __le32_to_cpu(usig->common);
	value = __le32_to_cpu(usig->value);
	mask = __le32_to_cpu(usig->mask);

	elr_valdiate_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE;
	elr_crc_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC;
	elr_tail_mask = IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL;

	ppdu_info->u_sig_info.sta_id =
		le32_get_bits(elr->info0,
			      HAL_RX_USIG_ELR_INFO0_STA_ID);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
		le32_get_bits(elr->info0,
			      HAL_RX_USIG_ELR_INFO0_PPDU_TYPE_COMP_MODE);

	value |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		 IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		 u32_encode_bits(ppdu_info->u_sig_info.ppdu_type_comp_mode,
				 IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE) |
		 u32_encode_bits(ppdu_info->u_sig_info.sta_id,
				 IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_VALIDATE_1,
				     elr_valdiate_mask) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_CRC,
				     elr_crc_mask) |
		 ATH12K_LE32_DEC_ENC(elr->info0,
				     HAL_RX_USIG_ELR_INFO0_TAIL,
				     elr_tail_mask);

	mask |= IEEE80211_RADIOTAP_UHR_USIG1_ELR_B20_B24_DISREGARD |
		IEEE80211_RADIOTAP_UHR_USIG1_ELR_B25_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B0_B1_PPDU_TYPE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B2_B12_STA_ID |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B13_B15_ELR_VALIDATE |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B16_B19_CRC |
		IEEE80211_RADIOTAP_UHR_USIG2_ELR_B20_B25_TAIL;

	usig->common = cpu_to_le32(common);
	usig->value = cpu_to_le32(value);
	usig->mask = cpu_to_le32(mask);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_hdr(const struct hal_mon_usig_hdr *usig,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 comp_mode;

	ppdu_info->eht_usig = true;

	comp_mode = le32_get_bits(usig->non_cmn.mu.info0,
				  HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);

	if (comp_mode == 0 && ppdu_info->u_sig_info.ul_dl)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_tb(&usig->non_cmn.tb, ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_parse_u_sig_mu(&usig->non_cmn.mu, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_hdr(const struct hal_mon_usig_hdr *usig,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 comp_mode;

	comp_mode = le32_get_bits(usig->non_cmn.mu.info0,
				  HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE);

	if (comp_mode == 0 && ppdu_info->u_sig_info.ul_dl)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_tb(&usig->non_cmn.tb, ppdu_info);
	else if (comp_mode == 3)
		ath12k_wifi8_hal_mon_rx_parse_u_sig_elr(&usig->non_cmn.elr, ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_mu(&usig->non_cmn.mu, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_parse_u_sig_hdr(const struct hal_mon_usig_hdr *usig,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	ath12k_wifi8_hal_mon_rx_get_phy_version(&usig->cmn, ppdu_info);

	if (ppdu_info->u_sig_info.phy_version == HAL_EHT_PHY) {
		ppdu_info->eht_usig = true;
		/* phy version = 0 is for EHT */
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_cmn(&usig->cmn, ppdu_info);
		ath12k_wifi8_hal_mon_rx_parse_u_sig_eht_hdr(usig, ppdu_info);
	} else {
		ppdu_info->uhr_usig = true;
		/* phy version = 1 if for UHR */
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_cmn(&usig->cmn, ppdu_info);
		ath12k_wifi8_hal_mon_rx_parse_u_sig_uhr_hdr(usig, ppdu_info);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_aggr_tlv(struct hal_rx_mon_ppdu_info *ppdu_info,
			      u16 tlv_len, const void *tlv_data)
{
	if (tlv_len <= HAL_RX_MON_MAX_AGGR_SIZE - ppdu_info->tlv_aggr.cur_len) {
		memcpy(ppdu_info->tlv_aggr.buf + ppdu_info->tlv_aggr.cur_len,
		       tlv_data, tlv_len);
		ppdu_info->tlv_aggr.cur_len += tlv_len;
	}
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_frame_type_ndp(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 1 &&
	    usig_info->eht_sig_mcs == 0 &&
	    usig_info->num_eht_sig_sym == 0)
		return true;

	return false;
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_non_ofdma(const struct hal_rx_u_sig_info *usig_info)
{
	u32 ppdu_type_comp_mode = usig_info->ppdu_type_comp_mode;
	u32 ul_dl = usig_info->ul_dl;

	if ((ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_MIMO && ul_dl == 0) ||
	    (ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_OFDMA && ul_dl == 0) ||
	    (ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_MU_MIMO  && ul_dl == 1))
		return true;

	return false;
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_ofdma(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 0 && usig_info->ul_dl == 0)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_ndp(const struct hal_eht_sig_ndp_cmn_eb *eht_sig_ndp,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_EHT_KNOWN_EHT_LTF |
		 IEEE80211_RADIOTAP_EHT_KNOWN_NSS_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_BEAMFORMED_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_DISREGARD_S |
		 IEEE80211_RADIOTAP_EHT_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_EHT_KNOWN_TAIL1;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[0]);
	data |= ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_EHT_DATA0_SPATIAL_REUSE);
	/* GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 */
	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_EHT_LTF);

	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_CRC,
				    IEEE80211_RADIOTAP_EHT_DATA0_CRC1_O);

	data |= ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_DISREGARD,
				    IEEE80211_RADIOTAP_EHT_DATA0_DISREGARD_S);
	eht->data[0] = cpu_to_le32(data);

	data = __le32_to_cpu(eht->data[7]);
	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_NSS,
				    IEEE80211_RADIOTAP_EHT_DATA7_NSS_S);

	data |=	ATH12K_LE32_DEC_ENC(eht_sig_ndp->info0,
				    HAL_RX_EHT_SIG_NDP_CMN_INFO0_BEAMFORMED,
				    IEEE80211_RADIOTAP_EHT_DATA7_BEAMFORMED_S);
	eht->data[7] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_usig_overflow(const struct hal_eht_sig_usig_overflow *ovflow,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_EHT_KNOWN_EHT_LTF |
		 IEEE80211_RADIOTAP_EHT_KNOWN_LDPC_EXTRA_SYM_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_PRE_PADD_FACOR_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_PE_DISAMBIGUITY_OM |
		 IEEE80211_RADIOTAP_EHT_KNOWN_DISREGARD_O;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[0]);
	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_EHT_DATA0_SPATIAL_REUSE);

	/* GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 */
	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_EHT_LTF);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_EHT_DATA0_LDPC_EXTRA_SYM_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_EHT_DATA0_PRE_PADD_FACOR_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_EHT_DATA0_PE_DISAMBIGUITY_OM);

	data |=	ATH12K_LE32_DEC_ENC(ovflow->info0,
				    HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISREGARD,
				    IEEE80211_RADIOTAP_EHT_DATA0_DISREGARD_O);
	eht->data[0] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_non_ofdma_users(const struct hal_eht_sig_non_ofdma_cmn_eb *eb,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	u32 known, data;

	known = __le32_to_cpu(eht->known);
	known |= IEEE80211_RADIOTAP_EHT_KNOWN_NR_NON_OFDMA_USERS_M;
	eht->known = cpu_to_le32(known);

	data = __le32_to_cpu(eht->data[7]);
	data |=	ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_EHT_SIG_NON_OFDMA_INFO0_NUM_USERS,
				    IEEE80211_RADIOTAP_EHT_DATA7_NUM_OF_NON_OFDMA_USERS);
	eht->data[7] = cpu_to_le32(data);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_mumimo_user(const struct hal_eht_sig_mu_mimo *user,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_eht_info *eht_info = &ppdu_info->eht_info;
	u32 user_idx;

	if (eht_info->num_user_info >= ARRAY_SIZE(eht_info->user_info))
		return;

	user_idx = eht_info->num_user_info++;

	eht_info->user_info[user_idx] |=
		IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_SPATIAL_CONFIG_KNOWN_M |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_CODING) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_SPATIAL_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_SPATIAL_CONFIG_M);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_MCS);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(const struct hal_eht_sig_non_mu_mimo *user,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_eht_info *eht_info = &ppdu_info->eht_info;
	u32 user_idx;

	if (eht_info->num_user_info >= ARRAY_SIZE(eht_info->user_info))
		return;

	user_idx = eht_info->num_user_info++;

	eht_info->user_info[user_idx] |=
		IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_CODING_KNOWN |
		IEEE80211_RADIOTAP_EHT_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_EHT_USER_INFO_BEAMFORMING_KNOWN_O |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_CODING) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_NSS,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_NSS_O) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_BEAMFORMED,
				    IEEE80211_RADIOTAP_EHT_USER_INFO_BEAMFORMING_O);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_MCS);

	ppdu_info->nss = le32_get_bits(user->info0,
				       HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_NSS) + 1;
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_mu_mimo_user(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == HAL_RX_RECEPTION_TYPE_SU &&
	    usig_info->ul_dl == 1)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_non_ofdma(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_non_ofdma_cmn_eb *eb = tlv;

	ath12k_wifi8_hal_mon_parse_usig_overflow(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_non_ofdma_users(eb, ppdu_info);

	if (ath12k_wifi8_hal_mon_is_mu_mimo_user(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_mumimo_user(&eb->user_field.mu_mimo,
							   ppdu_info);
	else
		ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(&eb->user_field.n_mu_mimo,
							       ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_ru_allocation(const struct hal_eht_sig_ofdma_cmn_eb *eb,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_ofdma_cmn_eb1 *ofdma_cmn_eb1 = &eb->eb1;
	const struct hal_eht_sig_ofdma_cmn_eb2 *ofdma_cmn_eb2 = &eb->eb2;
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	enum ieee80211_radiotap_eht_data ru_123, ru_124, ru_125, ru_126;
	enum ieee80211_radiotap_eht_data ru_121, ru_122, ru_112, ru_111;
	u32 data;

	ru_123 = IEEE80211_RADIOTAP_EHT_DATA4_RU_ALLOC_CC_1_2_3;
	ru_124 = IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_4;
	ru_125 = IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_5;
	ru_126 = IEEE80211_RADIOTAP_EHT_DATA6_RU_ALLOC_CC_1_2_6;
	ru_121 = IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_1;
	ru_122 = IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_2;
	ru_112 = IEEE80211_RADIOTAP_EHT_DATA2_RU_ALLOC_CC_1_1_2;
	ru_111 = IEEE80211_RADIOTAP_EHT_DATA1_RU_ALLOC_CC_1_1_1;

	switch (ppdu_info->u_sig_info.bw) {
	case HAL_BW_320_2:
	case HAL_BW_320_1:
		data = __le32_to_cpu(eht->data[4]);
		/* CC1 2::3 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA4_RU_ALLOC_CC_1_2_3_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_3,
					    ru_123);
		eht->data[4] = cpu_to_le32(data);

		data = __le32_to_cpu(eht->data[5]);
		/* CC1 2::4 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_4_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_4,
					    ru_124);

		/* CC1 2::5 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA5_RU_ALLOC_CC_1_2_5_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_5,
					    ru_125);
		eht->data[5] = cpu_to_le32(data);

		data = __le32_to_cpu(eht->data[6]);
		/* CC1 2::6 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA6_RU_ALLOC_CC_1_2_6_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_6,
					    ru_126);
		eht->data[6] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_160:
		data = __le32_to_cpu(eht->data[3]);
		/* CC1 2::1 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_1,
					    ru_121);
		/* CC1 2::2 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA3_RU_ALLOC_CC_1_2_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_2,
					    ru_122);
		eht->data[3] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_80:
		data = __le32_to_cpu(eht->data[2]);
		/* CC1 1::2 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA2_RU_ALLOC_CC_1_1_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_2,
					    ru_112);
		eht->data[2] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_40:
		fallthrough;
	case HAL_BW_20:
		data = __le32_to_cpu(eht->data[1]);
		/* CC1 1::1 */
		data |=	IEEE80211_RADIOTAP_EHT_DATA1_RU_ALLOC_CC_1_1_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_1,
					    ru_111);
		eht->data[1] = cpu_to_le32(data);
		break;
	default:
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_ofdma(const void *tlv,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_eht_sig_ofdma_cmn_eb *ofdma = tlv;

	ath12k_wifi8_hal_mon_parse_usig_overflow(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_ru_allocation(ofdma, ppdu_info);

	ath12k_wifi8_hal_mon_parse_eht_non_mumimo_user(&ofdma->user_field.n_mu_mimo,
						       ppdu_info);
}

static __always_inline bool
ath12k_wifi8_hal_mon_is_frame_type_elr(const struct hal_rx_u_sig_info *usig_info)
{
	if (usig_info->ppdu_type_comp_mode == 3)
		return true;

	return false;
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_elr(const void *tlv,
				       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_elr_sig *elr = tlv;

	struct hal_rx_uhr_elr_info *elr_info = &ppdu_info->elr_info;
	u32 known, sig1, sig2;

	ppdu_info->is_uhr_elr = true;

	known = __le32_to_cpu(elr_info->known);
	known |= IEEE80211_RADIOTAP_UHR_ELR_ELR_VERSION_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_UL_DL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_MCS_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_CODING_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LENGTH_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_LDPC_EXTRA_SYM_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_STA_ID_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_DISREGARD_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL_KNOWN |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC_CHECKED |
		 IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC_CHECKED;

	elr_info->known = cpu_to_le32(known);

	sig1 = __le32_to_cpu(elr_info->sig1);
	sig1 |=	ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_ELR_VERSION,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_ELR_VERSION) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_UL_DL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_UL_DL) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_ELR_MCS,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_MCS) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_CODING,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CODING) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_LENGTH,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LENGTH) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_LDPC_EXTRA_SYM,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_LDPC_EXTRA_SYM) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_CRC,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_CRC) |
		ATH12K_LE32_DEC_ENC(elr->sig1.info0,
				    HAL_RX_UHR_ELR_SIG1_TAIL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG1_TAIL);

	elr_info->sig1 = cpu_to_le32(sig1);

	sig2 = __le32_to_cpu(elr_info->sig2);
	sig2 |=	ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_STA_ID,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_STA_ID) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_DISREGARD,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_DISREGARD) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_CRC,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_CRC) |
		ATH12K_LE32_DEC_ENC(elr->sig2.info0,
				    HAL_RX_UHR_ELR_SIG2_TAIL,
				    IEEE80211_RADIOTAP_UHR_ELR_SIG2_TAIL);

	elr_info->sig2 = cpu_to_le32(sig2);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma_cmn(const void *tlv,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_uhr_sig_non_ofdma_cmn_eb *eb =
			(struct hal_uhr_sig_non_ofdma_cmn_eb *)tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	struct hal_rx_radiotap_uhr *uhr = &uhr_info->uhr;
	u32 known, data, data7, non_ofdma_users_mask;

	known = __le32_to_cpu(uhr->known);

	known |= IEEE80211_RADIOTAP_UHR_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_GI_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_UHR_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_LDPC_EXTRA_SYM |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRE_PAD_FACTOR |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PE_DISAMBIGUITY |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_INTF_MITG_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_NR_NON_OFDMA_USERS_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_CRC_NO |
		 IEEE80211_RADIOTAP_UHR_KNOWN_ENCODING_BLOCK_TAIL_NO;

	data = __le32_to_cpu(uhr->data[0]);

	data |=	ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_UHR_DATA0_SPATIAL_REUSE) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_GI_LTF,
				    IEEE80211_RADIOTAP_UHR_DATA0_GI) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_UHR_LTF) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_LDPC_EXTRA_SYM_OM) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_UHR_DATA0_PRE_PAD_FACTOR_OM) |
		ATH12K_LE32_DEC_ENC(eb->info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_UHR_DATA0_PE_DISAMBIGUITY_OM);

	put_unaligned_le32(data, &uhr->data[0]);

	known |= (IEEE80211_RADIOTAP_UHR_KNOWN_DISREGARD_NO |
		  IEEE80211_RADIOTAP_UHR_KNOWN_INTF_MITG_NO |
		  IEEE80211_RADIOTAP_UHR_DATA7_USER_ENCODING_BLOCK_CRC |
		  IEEE80211_RADIOTAP_UHR_DATA7_USER_ENCODING_BLOCK_TAIL);

	put_unaligned_le32(known, &uhr->known);

	non_ofdma_users_mask = IEEE80211_RADIOTAP_UHR_DATA7_NUM_OF_NON_OFDMA_USERS;

	data7 = __le32_to_cpu(uhr->data[7]);

	data7 |= ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS,
				     non_ofdma_users_mask) |
		 ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_IM_INDICATION,
				     IEEE80211_RADIOTAP_UHR_DATA7_INTF_MIT_NO) |
		 ATH12K_LE32_DEC_ENC(eb->info0,
				     HAL_RX_UHR_SIG_NON_OFDMA_INFO0_DISREGARD,
				     IEEE80211_RADIOTAP_UHR_DATA7_DISREGARD_NO);

	ppdu_info->num_non_ofdma_users =
		le32_get_bits(eb->info0,
			      HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS);
	put_unaligned_le32(data7, &uhr->data[7]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_mumimo_user(const struct hal_uhr_sig_mu_mimo *user,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_DISREGARD_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_BSS_COLOR_IND_KNOWN_NO |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);

	user_data |=
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_SPATIAL_CONFIG,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_SPATIAL_CONFIG_NO) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_CODING,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_LDPC_MODE,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO);

	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_MCS);
	ppdu_info->ldpc = le32_get_bits(user->info0,
					HAL_RX_UHR_SIG_MUMIMO_USER_INFO0_LDPC_MODE);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_non_mumimo_user(const struct hal_uhr_sig_non_mu_mimo *user,
					       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;
	u8 ueqm, ldpc;
	u32 hal_mod_pat_m, mon_pat_mask, hal_uemq_m, uemq_mask;
	u32 hal_beamform_m, beamform_mask;
	u32 hal_coding_m, coding_mask;
	u32 hal_2xldpc_m, ldpc_mask;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	hal_mod_pat_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MOD_PATTERN;
	mon_pat_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_O;
	hal_uemq_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_UNEQ_MOD;
	uemq_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_O;
	hal_beamform_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_BEAMFORMED;
	beamform_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_NO;
	hal_coding_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_CODING;
	coding_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO;
	hal_2xldpc_m = HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC;
	ldpc_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_CRC,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC);

	user_data |=
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_STA_ID,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		ATH12K_LE32_DEC_ENC(user->info0,
				    HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MCS,
				    IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		u32_encode_bits(le32_get_bits(user->info0,
					      HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_NSS)
					      + 1,
					      IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_O);

	ueqm = le32_get_bits(user->info0,
			     HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_UNEQ_MOD);

	if (ueqm) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_KNOWN_O;
		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_mod_pat_m,
					    mon_pat_mask);
		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_uemq_m,
					    uemq_mask);
	} else {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O;

			user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_beamform_m,
					    beamform_mask);

		user_data |=
			ATH12K_LE32_DEC_ENC(user->info0,
					    hal_coding_m,
					    coding_mask);
	}

	ldpc = le32_get_bits(user->info0,
			     HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC);

	if (ldpc) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O;
		user_data |= ATH12K_LE32_DEC_ENC(user->info0,
						 hal_2xldpc_m,
						 ldpc_mask);
	}

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);

	ppdu_info->mcs = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_MCS);
	ppdu_info->nss = le32_get_bits(user->info0,
				       HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_NSS) + 1;
	ppdu_info->ldpc = le32_get_bits(user->info0,
					HAL_RX_UHR_SIG_NON_MUMIMO_USER_INFO0_LDPC);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_mumimo_su(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_uhr_sig_mumimo_su_eb *user = (struct hal_uhr_sig_mumimo_su_eb *)tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	u32 user_idx, user_known = 0, user_data = 0;
	u32 ueqm, ldpc, sta_id, mod_pat, beamformed, beamformed_mask, coding, coding_mask;
	u32 ldpc_mask, mod_pat_mask, uemq_mask;
	u64 hal_mod_pat_m, hal_beamformed_m;
	u64 hal_coding_m, hal_ldpc_m;

	hal_mod_pat_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_MOD_PATTERN;
	mod_pat_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_O;
	hal_beamformed_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_BEAMFORMED;
	beamformed_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_NO;
	hal_ldpc_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC;
	ldpc_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_NO;
	uemq_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_O;
	hal_coding_m = HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_CODING;
	coding_mask = IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_NO;

	if (uhr_info->num_user_info >= UHR_MAX_USER_INFO)
		return;

	user_idx = uhr_info->num_user_info++;

	user_known = __le32_to_cpu(uhr_info->user_known[user_idx]);

	user_known |=
		IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_MCS_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_KNOWN_O |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_CRC_KNOWN |
		IEEE80211_RADIOTAP_UHR_USER_INFO_USR_ENC_TAIL_KNOWN;

	sta_id = le64_get_bits(user->info0,
			       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_STA_ID);
	ppdu_info->mcs = le64_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_MCS);
	ppdu_info->nss = le64_get_bits(user->info0,
				       HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_NSS) + 1;
	ppdu_info->ldpc = le64_get_bits(user->info0,
					HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC);
	user_data |= u32_encode_bits(sta_id,
				     IEEE80211_RADIOTAP_UHR_USER_INFO_STA_ID) |
		u32_encode_bits(ppdu_info->mcs, IEEE80211_RADIOTAP_UHR_USER_INFO_MCS) |
		u32_encode_bits(le64_get_bits(user->info0,
					      HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_NSS)
					      + 1,
					      IEEE80211_RADIOTAP_UHR_USER_INFO_NSS_O);

	ueqm = le64_get_bits(user->info0,
			     HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_UNEQ_MOD);

	if (ueqm) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_KNOWN_O |
			      IEEE80211_RADIOTAP_UHR_USER_INFO_UEMQ_PATTERN_KNOWN_O;
		mod_pat = le64_get_bits(user->info0,
					hal_mod_pat_m);
		user_data |= u32_encode_bits(mod_pat, mod_pat_mask) |
			     u32_encode_bits(ueqm, uemq_mask);
	} else {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_BEAMFORMING_KNOWN_O |
			IEEE80211_RADIOTAP_UHR_USER_INFO_CODING_KNOWN_O;

			beamformed = le64_get_bits(user->info0,
						   hal_beamformed_m);
			coding = le64_get_bits(user->info0,
					       hal_coding_m);
			user_data |= u32_encode_bits(beamformed, beamformed_mask) |
				     u32_encode_bits(coding, coding_mask);
	}

	ldpc = le64_get_bits(user->info0,
			     HAL_RX_UHR_SIG_MUMIMO_SU_USER_INFO0_LDPC);

	if (ldpc) {
		user_known |= IEEE80211_RADIOTAP_UHR_USER_INFO_2XLDPC_KNOWN_O;
		user_data |= u32_encode_bits(ldpc, ldpc_mask);
	}

	put_unaligned_le32(user_known, &uhr_info->user_known[user_idx]);
	put_unaligned_le32(user_data, &uhr_info->user_info[user_idx]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_non_ofdma_cmn_eb *eb = tlv;

	ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma_cmn(tlv, ppdu_info);

	if (ppdu_info->num_non_ofdma_users)
		ath12k_wifi8_hal_mon_parse_uhr_mumimo_user(&eb->user_field.mu_mimo,
							   ppdu_info);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_mumimo_su(eb,
							     ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma_cmn(const void *tlv,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb *eb = tlv;
	struct hal_rx_uhr_info *uhr_info = &ppdu_info->uhr_info;
	struct hal_rx_radiotap_uhr *uhr = &uhr_info->uhr;
	u32 known, data, data7;

	known = __le32_to_cpu(uhr->known);

	known |= IEEE80211_RADIOTAP_UHR_KNOWN_SPATIAL_REUSE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_GI_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_UHR_LTF |
		 IEEE80211_RADIOTAP_UHR_KNOWN_LDPC_EXTRA_SYM |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRE_PAD_FACTOR |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PE_DISAMBIGUITY |
		 IEEE80211_RADIOTAP_UHR_KNOWN_DISREGARD_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL1 |
		 IEEE80211_RADIOTAP_UHR_KNOWN_CRC2_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_TAIL2_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_MRU_SIZE |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_MRU_INDEX |
		 IEEE80211_RADIOTAP_UHR_KNOWN_RU_ALLOC_TB_FMT_O |
		 IEEE80211_RADIOTAP_UHR_KNOWN_PRIMARY_80;

	data = __le32_to_cpu(uhr->data[0]);

	data |=	ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_SPATIAL_REUSE,
				    IEEE80211_RADIOTAP_UHR_DATA0_SPATIAL_REUSE) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_GI_LTF,
				    IEEE80211_RADIOTAP_UHR_DATA0_GI) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_NUM_LTF_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_UHR_LTF) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM,
				    IEEE80211_RADIOTAP_UHR_DATA0_LDPC_EXTRA_SYM_OM) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR,
				    IEEE80211_RADIOTAP_UHR_DATA0_PRE_PAD_FACTOR_OM) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_OVERFLOW_INFO0_DISAMBIGUITY,
				    IEEE80211_RADIOTAP_UHR_DATA0_PE_DISAMBIGUITY_OM);

	put_unaligned_le32(data, &uhr->data[0]);

	data7 = __le32_to_cpu(uhr->data[7]);

	data7 |= ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_NON_OFDMA_INFO0_NUM_USERS,
				    IEEE80211_RADIOTAP_UHR_DATA7_NUM_OF_NON_OFDMA_USERS) |
		ATH12K_LE32_DEC_ENC(eb->eb1.info0,
				    HAL_RX_UHR_SIG_NON_OFDMA_INFO0_IM_INDICATION,
				    IEEE80211_RADIOTAP_UHR_DATA7_INTF_MIT_NO);

	put_unaligned_le32(data7, &uhr->data[7]);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_ru_allocation(const struct hal_uhr_sig_ofdma_cmn_eb *eb,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb1 *ofdma_cmn_eb1 = &eb->eb1;
	const struct hal_uhr_sig_ofdma_cmn_eb2 *ofdma_cmn_eb2 = &eb->eb2;
	struct hal_rx_radiotap_uhr *uhr = &ppdu_info->uhr_info.uhr;
	enum ieee80211_radiotap_uhr_data ru_123, ru_124, ru_125, ru_126;
	enum ieee80211_radiotap_uhr_data ru_121, ru_122, ru_112, ru_111;
	u32 data;

	ru_123 = IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3;
	ru_124 = IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4;
	ru_125 = IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5;
	ru_126 = IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6;
	ru_121 = IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1;
	ru_122 = IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2;
	ru_112 = IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2;
	ru_111 = IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1;

	switch (ppdu_info->u_sig_info.bw) {
	case HAL_BW_320_2:
	case HAL_BW_320_1:
		data = __le32_to_cpu(uhr->data[4]);
		/* CC1 2::3 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA4_RU_ALLOC_CC_1_2_3_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_3,
					    ru_123);
		uhr->data[4] = cpu_to_le32(data);

		data = __le32_to_cpu(uhr->data[5]);
		/* CC1 2::4 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_4_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_4,
					    ru_124);

		/* CC1 2::5 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA5_RU_ALLOC_CC_1_2_5_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_5,
					    ru_125);
		uhr->data[5] = cpu_to_le32(data);

		data = __le32_to_cpu(uhr->data[6]);
		/* CC1 2::6 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA6_RU_ALLOC_CC_1_2_6_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_6,
					    ru_126);
		uhr->data[6] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_160:
		data = __le32_to_cpu(uhr->data[3]);
		/* CC1 2::1 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_1,
					    ru_121);
		/* CC1 2::2 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA3_RU_ALLOC_CC_1_2_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb2->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB2_RU_ALLOC_2_2,
					    ru_122);
		uhr->data[3] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_80:
		data = __le32_to_cpu(uhr->data[2]);
		/* CC1 1::2 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA2_RU_ALLOC_CC_1_1_2_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB1_RU_ALLOC_1_2,
					    ru_112);
		uhr->data[2] = cpu_to_le32(data);

		fallthrough;
	case HAL_BW_40:
		fallthrough;
	case HAL_BW_20:
		data = __le32_to_cpu(uhr->data[1]);
		/* CC1 1::1 */
		data |=	IEEE80211_RADIOTAP_UHR_DATA1_RU_ALLOC_CC_1_1_1_KNOWN |
			ATH12K_LE64_DEC_ENC(ofdma_cmn_eb1->info0,
					    HAL_RX_UHR_SIG_OFDMA_EB1_RU_ALLOC_1_1,
					    ru_111);
		uhr->data[1] = cpu_to_le32(data);
		break;
	default:
		break;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma(const void *tlv,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	const struct hal_uhr_sig_ofdma_cmn_eb *ofdma = tlv;

	ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma_cmn(tlv, ppdu_info);
	ath12k_wifi8_hal_mon_parse_uhr_ru_allocation(ofdma, ppdu_info);

	ath12k_wifi8_hal_mon_parse_uhr_non_mumimo_user(&ofdma->user_field.n_mu_mimo,
						       ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_uhr_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
				       const void *tlv_data)
{
	ppdu_info->is_uhr = true;

	if (ath12k_wifi8_hal_mon_is_frame_type_elr(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_uhr_sig_elr(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_non_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_uhr_sig_non_ofdma(tlv_data, ppdu_info);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_ofdma(tlv_data, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
				       const void *tlv_data)
{
	ppdu_info->is_eht = true;

	if (ath12k_wifi8_hal_mon_is_frame_type_ndp(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_ndp(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_non_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_non_ofdma(tlv_data, ppdu_info);
	else if (ath12k_wifi8_hal_mon_is_ofdma(&ppdu_info->u_sig_info))
		ath12k_wifi8_hal_mon_parse_eht_sig_ofdma(tlv_data, ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_eht_uhr_sig_hdr(struct hal_rx_mon_ppdu_info *ppdu_info,
					   const void *tlv_data)
{
	if (ppdu_info->u_sig_info.phy_version == HAL_EHT_PHY)
		ath12k_wifi8_hal_mon_parse_eht_sig_hdr(ppdu_info, tlv_data);
	else
		ath12k_wifi8_hal_mon_parse_uhr_sig_hdr(ppdu_info, tlv_data);
}

static __always_inline enum ath12k_eht_ru_size
ath12k_wifi8_hal_mon_hal_ru_size_to_ath12k_ru_size(u32 hal_ru_size)
{
	switch (hal_ru_size) {
	case HAL_EHT_RU_26:
		return ATH12K_EHT_RU_26;
	case HAL_EHT_RU_52:
		return ATH12K_EHT_RU_52;
	case HAL_EHT_RU_78:
		return ATH12K_EHT_RU_52_26;
	case HAL_EHT_RU_106:
		return ATH12K_EHT_RU_106;
	case HAL_EHT_RU_132:
		return ATH12K_EHT_RU_106_26;
	case HAL_EHT_RU_242:
		return ATH12K_EHT_RU_242;
	case HAL_EHT_RU_484:
		return ATH12K_EHT_RU_484;
	case HAL_EHT_RU_726:
		return ATH12K_EHT_RU_484_242;
	case HAL_EHT_RU_996:
		return ATH12K_EHT_RU_996;
	case HAL_EHT_RU_996x2:
		return ATH12K_EHT_RU_996x2;
	case HAL_EHT_RU_996x3:
		return ATH12K_EHT_RU_996x3;
	case HAL_EHT_RU_996x4:
		return ATH12K_EHT_RU_996x4;
	case HAL_EHT_RU_NONE:
		return ATH12K_EHT_RU_INVALID;
	case HAL_EHT_RU_996_484:
		return ATH12K_EHT_RU_996_484;
	case HAL_EHT_RU_996x2_484:
		return ATH12K_EHT_RU_996x2_484;
	case HAL_EHT_RU_996x3_484:
		return ATH12K_EHT_RU_996x3_484;
	case HAL_EHT_RU_996_484_242:
		return ATH12K_EHT_RU_996_484_242;
	default:
		return ATH12K_EHT_RU_INVALID;
	}
}

static __always_inline u32
ath12k_wifi8_hal_mon_ul_ofdma_ru_size_to_width(enum ath12k_eht_ru_size ru_size)
{
	switch (ru_size) {
	case ATH12K_EHT_RU_26:
		return RU_26;
	case ATH12K_EHT_RU_52:
		return RU_52;
	case ATH12K_EHT_RU_52_26:
		return RU_52_26;
	case ATH12K_EHT_RU_106:
		return RU_106;
	case ATH12K_EHT_RU_106_26:
		return RU_106_26;
	case ATH12K_EHT_RU_242:
		return RU_242;
	case ATH12K_EHT_RU_484:
		return RU_484;
	case ATH12K_EHT_RU_484_242:
		return RU_484_242;
	case ATH12K_EHT_RU_996:
		return RU_996;
	case ATH12K_EHT_RU_996_484:
		return RU_996_484;
	case ATH12K_EHT_RU_996_484_242:
		return RU_996_484_242;
	case ATH12K_EHT_RU_996x2:
		return RU_2X996;
	case ATH12K_EHT_RU_996x2_484:
		return RU_2X996_484;
	case ATH12K_EHT_RU_996x3:
		return RU_3X996;
	case ATH12K_EHT_RU_996x3_484:
		return RU_3X996_484;
	case ATH12K_EHT_RU_996x4:
		return RU_4X996;
	default:
		return RU_INVALID;
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_parse_user_info(const struct hal_receive_user_info *rx_usr_info,
				     u16 user_id,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *mon_rx_user_status = NULL;
	struct hal_rx_radiotap_eht *eht = &ppdu_info->eht_info.eht;
	enum ath12k_eht_ru_size rtap_ru_size = ATH12K_EHT_RU_INVALID;
	u32 ru_width, reception_type, ru_index = HAL_EHT_RU_INVALID;
	u32 ru_type_80_0, ru_start_index_80_0;
	u32 ru_type_80_1, ru_start_index_80_1;
	u32 ru_type_80_2, ru_start_index_80_2;
	u32 ru_type_80_3, ru_start_index_80_3;
	u32 ru_size = 0, num_80mhz_with_ru = 0;
	u64 ru_index_320mhz = 0;
	u32 ru_index_per80mhz;

	reception_type = le32_get_bits(rx_usr_info->info0,
				       HAL_RX_USR_INFO0_RECEPTION_TYPE);

	switch (reception_type) {
	case HAL_RECEPTION_TYPE_SU:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_SU;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_MIMO:
	case HAL_RECEPTION_TYPE_UL_MU_MIMO:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_MIMO;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFMA:
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO:
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO:
		ppdu_info->reception_type = HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO;
	}

	ppdu_info->is_stbc = le32_get_bits(rx_usr_info->info0, HAL_RX_USR_INFO0_STBC);
	ppdu_info->ldpc = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_LDPC);
	ppdu_info->dcm = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_STA_DCM);
	ppdu_info->bw = le32_get_bits(rx_usr_info->info1, HAL_RX_USR_INFO1_RX_BW);
	ppdu_info->mcs = le32_get_bits(rx_usr_info->info1, HAL_RX_USR_INFO1_MCS);
	ppdu_info->nss = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_NSS) + 1;

	if (user_id < HAL_MAX_UL_MU_USERS) {
		mon_rx_user_status = &ppdu_info->userstats[user_id];
		mon_rx_user_status->mcs = ppdu_info->mcs;
		mon_rx_user_status->nss = ppdu_info->nss;
	}

	if (!(ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO))
		return;

	/* RU allocation present only for OFDMA reception */
	ru_type_80_0 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_0);
	ru_start_index_80_0 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_0);
	if (ru_type_80_0 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_0;
		ru_index_per80mhz = ru_start_index_80_0;
		ru_index = ru_index_per80mhz;
		ru_index_320mhz |= HAL_RU_PER80(ru_type_80_0, 0, ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_1 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_1);
	ru_start_index_80_1 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_1);
	if (ru_type_80_1 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_1;
		ru_index_per80mhz = ru_start_index_80_1;
		ru_index = ru_index_per80mhz;
		ru_index_320mhz |= HAL_RU_PER80(ru_type_80_1, 1, ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_2 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_2);
	ru_start_index_80_2 = le32_get_bits(rx_usr_info->info3,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_2);
	if (ru_type_80_2 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_2;
		ru_index_per80mhz = ru_start_index_80_2;
		ru_index = ru_index_per80mhz;
		ru_index_320mhz |= HAL_RU_PER80(ru_type_80_2, 2, ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	ru_type_80_3 = le32_get_bits(rx_usr_info->info2, HAL_RX_USR_INFO2_RU_TYPE_80_3);
	ru_start_index_80_3 = le32_get_bits(rx_usr_info->info2,
					    HAL_RX_USR_INFO3_RU_START_IDX_80_3);
	if (ru_type_80_3 != HAL_EHT_RU_NONE) {
		ru_size += ru_type_80_3;
		ru_index_per80mhz = ru_start_index_80_3;
		ru_index = ru_index_per80mhz;
		ru_index_320mhz |= HAL_RU_PER80(ru_type_80_3, 3, ru_index_per80mhz);
		num_80mhz_with_ru++;
	}

	if (num_80mhz_with_ru > 1) {
		/* Calculate the MRU index */
		switch (ru_index_320mhz) {
		case HAL_EHT_RU_996_484_0:
		case HAL_EHT_RU_996x2_484_0:
		case HAL_EHT_RU_996x3_484_0:
			ru_index = 0;
			break;
		case HAL_EHT_RU_996_484_1:
		case HAL_EHT_RU_996x2_484_1:
		case HAL_EHT_RU_996x3_484_1:
			ru_index = 1;
			break;
		case HAL_EHT_RU_996_484_2:
		case HAL_EHT_RU_996x2_484_2:
		case HAL_EHT_RU_996x3_484_2:
			ru_index = 2;
			break;
		case HAL_EHT_RU_996_484_3:
		case HAL_EHT_RU_996x2_484_3:
		case HAL_EHT_RU_996x3_484_3:
			ru_index = 3;
			break;
		case HAL_EHT_RU_996_484_4:
		case HAL_EHT_RU_996x2_484_4:
		case HAL_EHT_RU_996x3_484_4:
			ru_index = 4;
			break;
		case HAL_EHT_RU_996_484_5:
		case HAL_EHT_RU_996x2_484_5:
		case HAL_EHT_RU_996x3_484_5:
			ru_index = 5;
			break;
		case HAL_EHT_RU_996_484_6:
		case HAL_EHT_RU_996x2_484_6:
		case HAL_EHT_RU_996x3_484_6:
			ru_index = 6;
			break;
		case HAL_EHT_RU_996_484_7:
		case HAL_EHT_RU_996x2_484_7:
		case HAL_EHT_RU_996x3_484_7:
			ru_index = 7;
			break;
		case HAL_EHT_RU_996x2_484_8:
			ru_index = 8;
			break;
		case HAL_EHT_RU_996x2_484_9:
			ru_index = 9;
			break;
		case HAL_EHT_RU_996x2_484_10:
			ru_index = 10;
			break;
		case HAL_EHT_RU_996x2_484_11:
			ru_index = 11;
			break;
		default:
			ru_index = HAL_EHT_RU_INVALID;
			break;
		}

		ru_size += 4;
	}

	rtap_ru_size = ath12k_wifi8_hal_mon_hal_ru_size_to_ath12k_ru_size(ru_size);
	if (rtap_ru_size != ATH12K_EHT_RU_INVALID) {
		u32 known, data;

		known = __le32_to_cpu(eht->known);
		known |= IEEE80211_RADIOTAP_EHT_KNOWN_RU_MRU_SIZE_OM;
		eht->known = cpu_to_le32(known);

		data = __le32_to_cpu(eht->data[1]);
		data |=	u32_encode_bits(rtap_ru_size,
					IEEE80211_RADIOTAP_EHT_DATA1_RU_SIZE);
		eht->data[1] = cpu_to_le32(data);
	}

	if (ru_index != HAL_EHT_RU_INVALID) {
		u32 known, data;

		known = __le32_to_cpu(eht->known);
		known |= IEEE80211_RADIOTAP_EHT_KNOWN_RU_MRU_INDEX_OM;
		eht->known = cpu_to_le32(known);

		data = __le32_to_cpu(eht->data[1]);
		data |=	u32_encode_bits(rtap_ru_size,
					IEEE80211_RADIOTAP_EHT_DATA1_RU_INDEX);
		eht->data[1] = cpu_to_le32(data);
	}

	if (mon_rx_user_status && ru_index != HAL_EHT_RU_INVALID &&
	    rtap_ru_size != ATH12K_EHT_RU_INVALID) {
		mon_rx_user_status->ul_ofdma_ru_start_index = ru_index;
		mon_rx_user_status->ul_ofdma_ru_size = rtap_ru_size;

		ru_width = ath12k_wifi8_hal_mon_ul_ofdma_ru_size_to_width(rtap_ru_size);

		mon_rx_user_status->ul_ofdma_ru_width = ru_width;
		mon_rx_user_status->ofdma_info_valid = 1;
	}
}

static void
ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 user_id = ppdu_info->user_id;

	/* set decap type to raw mode for management frames, control frames
	 * and NULL data frames
	 */
	if (ppdu_info->nrp_info.fc_valid && ppdu_info->mpdu_info[user_id].decap_type) {
		if (ieee80211_is_mgmt(ppdu_info->nrp_info.frame_control) ||
		    ieee80211_is_ctl(ppdu_info->nrp_info.frame_control) ||
		    ppdu_info->grp_id == HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA) {
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		}
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get(const void *tlv_data, u32 userid,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mpdu_start *mpdu_start =
		(struct hal_rx_mpdu_start *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[9], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = __le32_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);
	info[5] = __le32_to_cpu(mpdu_start->info5);
	info[6] = __le32_to_cpu(mpdu_start->info6);
	info[7] = __le32_to_cpu(mpdu_start->info7);
	info[8] = __le32_to_cpu(mpdu_start->info8);

	ppdu_info->grp_id = u32_get_bits(info[3],
					 HAL_RX_MPDU_START_INFO3_SW_GRP_ID);

	peer_id = u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_PEERID);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_TO_DS);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_FC_VALID);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->mpdu_info[user_id].raw_mpdu =
			u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_RAW_MPDU);
		if (ppdu_info->mpdu_info[user_id].raw_mpdu)
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		else
			ppdu_info->mpdu_info[user_id].decap_type =
				u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_DECAP_TYPE);
	}

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
		ath12k_wifi8_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
						      ppdu_info->nrp_info.mac_addr2);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userid = userid;
		ppdu_info->userstats[userid].sw_peer_id = peer_id;
		ppdu_info->userstats[userid].ampdu_id =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_PPDU_ID);
		ppdu_info->userstats[userid].filter_category =
			u32_get_bits(info[3],
				     HAL_RX_MPDU_START_INFO3_FILTER_CAT);
		ppdu_info->userstats[userid].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
		ppdu_info->userstats[userid].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[userid].frame_control =
				ppdu_info->nrp_info.frame_control;
	}

	ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode(ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact(
					const void *tlv_data, u32 userid,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_mpdu_start_compact *mpdu_start =
		(struct hal_rx_mon_mpdu_start_compact *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[9], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = __le32_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);
	info[5] = __le32_to_cpu(mpdu_start->info5);
	info[6] = __le32_to_cpu(mpdu_start->info6);
	info[7] = __le32_to_cpu(mpdu_start->info7);
	info[8] = __le32_to_cpu(mpdu_start->info8);

	ppdu_info->grp_id = u32_get_bits(info[3],
					 HAL_RX_MPDU_START_INFO3_SW_GRP_ID_CMPCT);

	peer_id = u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_PEERID_CMPCT);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID_CMPCT);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_TO_DS_CMPCT);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1],
			     HAL_RX_MPDU_START_INFO1_FC_VALID_CMPCT);
	ppdu_info->mpdu_info[user_id].raw_mpdu =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_RAW_MPDU_CMPCT);
	if (ppdu_info->mpdu_info[user_id].raw_mpdu)
		ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
	else
		ppdu_info->mpdu_info[user_id].decap_type =
			u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_DECAP_TYPE_CMPCT);

	ppdu_info->mpdu_len += u32_get_bits(info[5],
					    HAL_RX_MPDU_START_INFO5_MPDU_LEN_CMPCT);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[5], HAL_RX_MPDU_START_INFO5_MCAST_BCAST_CMPCT);
	ppdu_info->nrp_info.frame_control =
		u32_get_bits(info[6],
			     HAL_RX_MPDU_START_INFO6_FC_FIELD_CMPCT);

	addr_16 = u32_get_bits(info[7],
			       HAL_RX_MPDU_START_INFO7_ADDR2_15_0_CMPCT);
	addr_32 = u32_get_bits(info[8],
			       HAL_RX_MPDU_START_INFO8_ADDR2_47_16_CMPCT);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr(addr_16, addr_32,
						      ppdu_info->nrp_info.mac_addr2);

	if (userid < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userid = userid;
		ppdu_info->userstats[userid].sw_peer_id = peer_id;
		ppdu_info->userstats[userid].ampdu_id =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_PPDU_ID_CMPCT);
		ppdu_info->userstats[userid].filter_category =
			u32_get_bits(info[3],
				     HAL_RX_MPDU_START_INFO3_FILTER_CAT_CMPCT);
		ppdu_info->userstats[userid].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
		ppdu_info->userstats[userid].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[userid].frame_control =
				ppdu_info->nrp_info.frame_control;
	}
}

void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse(const void *tlv_data, u32 userid,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_MPDU_START_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact(tlv_data, userid,
								    ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get(tlv_data, userid,
							    ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_msdu_end_info_get(const void *tlv_data, u32 userid,
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
	ath12k_wifi8_hal_mon_parse_rx_msdu_end_err(info[2],
						   &ppdu_info->errmap);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_msdu_end_info_get_compact(const void *tlv_data, u32 userid,
						  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_msdu_end_compact *msdu_end =
		(struct hal_rx_mon_msdu_end_compact *)tlv_data;
	u32 info[3];

	info[0] = __le32_to_cpu(msdu_end->info0);
	info[1] = __le32_to_cpu(msdu_end->info1);
	info[2] = __le32_to_cpu(msdu_end->info2);

	ppdu_info->grp_id = u32_get_bits(info[0],
					 HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID_CMPCT);

	ppdu_info->decap_format = u32_get_bits(info[1],
					       HAL_RX_MSDU_END_INFO1_DECAP_FORMAT_CMPCT);
	ath12k_wifi8_hal_mon_parse_rx_msdu_end_err(info[2],
						   &ppdu_info->errmap);
}

void
ath12k_wifi8_hal_mon_rx_msdu_end_info_parse(const void *tlv_data, u32 userid,
					    struct hal_rx_mon_ppdu_info *ppdu_info,
					    u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_MSDU_END_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_msdu_end_info_get_compact(tlv_data, userid,
								  ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_msdu_end_info_get(tlv_data, userid,
							  ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get(const void *tlv_data, u32 userid,
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
	ppdu_info->retried_msdu_count =
		u32_get_bits(info[10],
			     HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT);

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
	case HAL_RX_PREAMBLE_11BN:	/* UHR */
		ppdu_info->is_uhr = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];
		ath12k_wifi8_hal_mon_handle_ofdma_info(info,
						       rxuser_stats);
		ath12k_wifi8_hal_mon_populate_byte_count(info,
							 rxuser_stats);
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get_compact(
						const void *tlv_data, u32 userid,
						struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_ppdu_end_user_stats_compact *ppdu_eu_stats =
		(struct hal_rx_mon_ppdu_end_user_stats_compact *)tlv_data;
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
			     HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR_CMPCT);
	ppdu_info->peer_id =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID_CMPCT);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID_CMPCT);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE_CMPCT);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK_CMPCT);
	ppdu_info->ast_index =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX_CMPCT);
	ppdu_info->tcp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT_CMPCT);
	ppdu_info->udp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT_CMPCT);
	ppdu_info->other_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT_CMPCT);
	ppdu_info->tcp_ack_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT_CMPCT);
	tid_bitmap = u32_get_bits(info[7],
				  HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP_CMPCT);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[11],
			     HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT_CMPCT);
	ppdu_info->retried_msdu_count =
		u32_get_bits(info[10],
			     HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT_CMPCT);

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
	case HAL_RX_PREAMBLE_11BN:	/* UHR */
		ppdu_info->is_uhr = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];
		ath12k_wifi8_hal_mon_handle_ofdma_info_compact(info,
							       rxuser_stats);
		ath12k_wifi8_hal_mon_populate_byte_count_compact(info,
								 rxuser_stats);
	}
}

void
ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_parse(const void *tlv_data, u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info,
						 u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_PPDU_EU_STATS_TLV_SIZE))
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get_compact(tlv_data,
								       userid,
								       ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_get(tlv_data,
							       userid,
							       ppdu_info);
}

enum hal_rx_mon_status
ath12k_wifi8_hal_mon_rx_parse_status_tlv(struct ath12k_hal *hal,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 struct hal_tlv_parsed_hdr *tlv_parsed_hdr)
{
	const void *tlv_data = tlv_parsed_hdr->data;
	u32 userid;
	u16 tlv_tag, tlv_len;

	tlv_tag = tlv_parsed_hdr->tag;
	tlv_len = tlv_parsed_hdr->len;
	userid = tlv_parsed_hdr->userid;

	if (userid < HAL_MAX_UL_MU_USERS)
		ppdu_info->user_id = userid;

	if (ppdu_info->tlv_aggr.in_progress && ppdu_info->tlv_aggr.tlv_tag != tlv_tag) {
		ath12k_wifi8_hal_mon_parse_eht_uhr_sig_hdr(ppdu_info,
							   ppdu_info->tlv_aggr.buf);

		ppdu_info->tlv_aggr.in_progress = false;
		ppdu_info->tlv_aggr.cur_len = 0;
	}

	switch (tlv_tag) {
	case HAL_RX_PPDU_START: {
		const struct hal_rx_ppdu_start *ppdu_start = tlv_data;
		u32 info[2];

		u64 ppdu_ts = ath12k_hal_le32hilo_to_u64(ppdu_start->ppdu_start_ts_63_32,
							 ppdu_start->ppdu_start_ts_31_0);

		info[0] = __le32_to_cpu(ppdu_start->info0);

		ppdu_info->ppdu_id = u32_get_bits(info[0],
						  HAL_RX_PPDU_START_INFO0_PPDU_ID);

		info[1] = __le32_to_cpu(ppdu_start->info1);
		ppdu_info->chan_num = u32_get_bits(info[1],
						   HAL_RX_PPDU_START_INFO1_CHAN_NUM);
		ppdu_info->freq = u32_get_bits(info[1],
					       HAL_RX_PPDU_START_INFO1_CHAN_FREQ);
		ppdu_info->ppdu_ts = ppdu_ts;

		if (ppdu_info->ppdu_id != ppdu_info->last_ppdu_id) {
			ppdu_info->last_ppdu_id = ppdu_info->ppdu_id;
			ppdu_info->num_users = 0;
		}
		break;
	}
	case HAL_RX_PPDU_END_USER_STATS: {
		ath12k_hal_mon_rx_ppdu_end_usr_stats_info_get(hal, tlv_data,
							      userid, ppdu_info,
							      tlv_len);

		if (userid < HAL_MAX_UL_MU_USERS) {
			struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];

			if (ppdu_info->num_mpdu_fcs_ok > 1 ||
			    ppdu_info->num_mpdu_fcs_err > 1)
				ppdu_info->userstats[userid].ampdu_present = true;

			ppdu_info->num_users += 1;

			ath12k_wifi8_hal_mon_populate_mu_user_info(ppdu_info,
								   rxuser_stats);
		}
		break;
	}
	case HAL_PHYRX_HT_SIG:
		ath12k_wifi8_hal_mon_parse_ht_sig(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_B:
		ath12k_wifi8_hal_mon_parse_l_sig_b(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_L_SIG_A:
		ath12k_wifi8_hal_mon_parse_l_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_VHT_SIG_A:
		ath12k_wifi8_hal_mon_parse_vht_sig_a(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_SU:
		ath12k_wifi8_hal_mon_parse_he_sig_su(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_A_MU_DL:
		ath12k_wifi8_hal_mon_parse_he_sig_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B1_MU:
		ath12k_wifi8_hal_mon_parse_he_sig_b1_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_MU:
		ath12k_wifi8_hal_mon_parse_he_sig_b2_mu(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_HE_SIG_B2_OFDMA:
		ath12k_wifi8_hal_mon_parse_he_sig_b2_ofdma(tlv_data, ppdu_info);
		break;

	case HAL_PHYRX_RSSI_LEGACY: {
		const struct hal_rx_phyrx_rssi_legacy_info *rssi = tlv_data;
		u32 info[2];

		info[0] = __le32_to_cpu(rssi->info0);
		info[1] = __le32_to_cpu(rssi->info1);

		/* TODO: Please note that the combined rssi will not be accurate
		 * in MU case. Rssi in MU needs to be retrieved from
		 * PHYRX_OTHER_RECEIVE_INFO TLV.
		 */
		ppdu_info->rssi_comb =
			u32_get_bits(info[1],
				     HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO1_RSSI_COMB);

		ppdu_info->bw = u32_get_bits(info[0],
					     HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO0_RX_BW);
		break;
	}
	case HAL_PHYRX_OTHER_RECEIVE_INFO: {
		const struct hal_phyrx_common_user_info *cmn_usr_info = tlv_data;

		ppdu_info->gi = le32_get_bits(cmn_usr_info->info0,
					      HAL_RX_PHY_CMN_USER_INFO0_GI);
		break;
	}
	case HAL_RX_PPDU_START_USER_INFO:
		ath12k_wifi8_hal_mon_parse_user_info(tlv_data, userid, ppdu_info);
		break;

	case HAL_RXPCU_PPDU_END_INFO: {
		const struct hal_rx_ppdu_end_duration *ppdu_rx_duration = tlv_data;
		u32 info[1];

		info[0] = __le32_to_cpu(ppdu_rx_duration->info2);
		ppdu_info->rx_antenna =
			u32_get_bits(info[0],
				     HAL_RX_PPDU_END_DURATION_INFO2_RX_ANTENNA);
		info[0] = __le32_to_cpu(ppdu_rx_duration->info3);
		ppdu_info->rx_duration =
			u32_get_bits(info[0], HAL_RX_PPDU_END_DURATION);
		ppdu_info->tsft = __le32_to_cpu(ppdu_rx_duration->info1);
		ppdu_info->tsft = (ppdu_info->tsft << 32) |
				   __le32_to_cpu(ppdu_rx_duration->info0);
		break;
	}
	case HAL_RX_MPDU_START: {
		ath12k_hal_mon_rx_mpdu_start_info_get(hal, tlv_data,
						      userid, ppdu_info,
						      tlv_len);

		return HAL_RX_MON_STATUS_MPDU_START;
	}
	case HAL_RX_MSDU_START:
		/* TODO: add msdu start parsing logic */
		break;
	case HAL_MON_BUF_ADDR:
		return HAL_RX_MON_STATUS_BUF_ADDR;
	case HAL_RX_MSDU_END:

		ath12k_hal_mon_rx_msdu_end_info_get(hal, tlv_data, userid, ppdu_info,
						    tlv_len);

		if (ppdu_info->grp_id == RX_MSDU_END_INFO0_SW_FRAMEGROUP_UCAST_DATA ||
		    ppdu_info->grp_id == RX_MSDU_END_INFO0_SW_FRAMEGROUP_MCAST_DATA) {
			if (userid < HAL_MAX_UL_MU_USERS)
				ppdu_info->userstats[userid].errmap = ppdu_info->errmap;
		}

		return HAL_RX_MON_STATUS_MSDU_END;
	case HAL_RX_MPDU_END:
		return HAL_RX_MON_STATUS_MPDU_END;
	case HAL_PHYRX_GENERIC_U_SIG:
		ath12k_wifi8_hal_mon_rx_parse_u_sig_hdr(tlv_data, ppdu_info);
		break;
	case HAL_PHYRX_GENERIC_EHT_OR_UHR_SIG:
		/* Handle the case where aggregation is in progress
		 * or the current TLV is one of the TLVs which should be
		 * aggregated
		 */
		if (!ppdu_info->tlv_aggr.in_progress) {
			ppdu_info->tlv_aggr.in_progress = true;
			ppdu_info->tlv_aggr.tlv_tag = tlv_tag;
			ppdu_info->tlv_aggr.cur_len = 0;
		}

		ppdu_info->is_eht = true;

		ath12k_wifi8_hal_mon_aggr_tlv(ppdu_info, tlv_len, tlv_data);
		break;
	case HAL_DUMMY:
		return HAL_RX_MON_STATUS_BUF_DONE;
	case HAL_RX_HEADER:
		const struct hal_mon_rx_hdr *rx_hdr = tlv_data;

		if (userid < HAL_MAX_UL_MU_USERS) {
			ppdu_info->mpdu_info[userid].raw_mpdu =
				le32_get_bits(rx_hdr->info0,
					      HAL_MON_RX_HDR_INFO0_RAW_MPDU);

			if (ppdu_info->mpdu_info[userid].raw_mpdu)
				ppdu_info->mpdu_info[userid].decap_type =
						DP_RX_DECAP_TYPE_RAW;
			else
				ppdu_info->mpdu_info[userid].decap_type =
					le32_get_bits(rx_hdr->info0,
						      HAL_MON_RX_HDR_INFO0_DECAP_TYPE);
		}
		return HAL_RX_MON_STATUS_RX_HDR;
	case HAL_MON_DROP:
		ppdu_info->is_drop_tlv = true;
		return HAL_RX_MON_STATUS_DROP_TLV;
	case HAL_RX_PPDU_END_STATUS_DONE:
		return HAL_RX_MON_STATUS_PPDU_DONE;
	case 0:
		return HAL_RX_MON_STATUS_PPDU_DONE;
	default:
		break;
	}

	return HAL_RX_MON_STATUS_PPDU_NOT_DONE;
}

u8 *ath12k_wifi8_hal_mon_rx_desc_get_msdu_payload(void *rx_desc)
{
	return ath12k_wifi8_hal_rx_desc_get_msdu_payload_qcn9625(rx_desc);
}

void ath12k_wifi8_hal_mon_ops_init(struct ath12k_hal *hal,
				   u8 hw_version)
{
	switch (hw_version) {
	case ATH12K_HW_QCN9625_HW10:
	case ATH12K_HW_QCN9589_HW10:
	case ATH12K_HW_QCN9625_HW20:
		hal->hal_mon_ops = &hal_qcn9625_mon_ops;
		hal->tlv_hdr_tag_shift = HAL_TLV_64_HDR_TAG_NOSHIFT;
		break;
	default:
		break;
	}
}
