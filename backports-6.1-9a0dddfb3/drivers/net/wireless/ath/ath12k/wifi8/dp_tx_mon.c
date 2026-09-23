// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the wifi8 TX monitor filter configuration, mirroring
 * wifi7/dp_tx_mon.c.
 */
#include <linux/if_vlan.h>
#include "../dp_mon.h"
#include "../debug.h"
#include "hal_qcn9625.h"
#include "hal_mon.h"
#include "../peer.h"
#include "../dp_mon_filter.h"
#include "dp_mon2.h"
#include "dp_tx_mon.h"
#include "../trace.h"
#include "../ath12k_notif.h"

void
ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask(struct htt_tx_mon_ring_selection_cfg_cmd
						*cmd, const struct htt_tx_ring_tlv_filter
						*htt_tlv_filter)
{
	/*word 13*/
	cmd->tlv_word_mask_in6 |=
		le32_encode_bits(htt_tlv_filter->wmask.mactx_user_desc_common,
				 HTT_TX_MON_WMASK_IN6_MACTX_USR_DESC_CMN_MASK);
	/*word 14*/
	cmd->tlv_word_mask_in7 |=
		le32_encode_bits(htt_tlv_filter->wmask.rx_resp_required_info,
				 HTT_TX_MON_WMASK_IN7_RX_RESP_REQD_INFO_MASK);
	cmd->tlv_filter_mask_in4 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_upstream_tlv_flags3,
				 HTT_TX_FILTER_MASK_IN4_UPSTREAM_TLV_FLAGS3) |
		le32_encode_bits(htt_tlv_filter->tx_mon_downstream_tlv_flags1,
				 HTT_TX_FILTER_MASK_IN4_DOWNSTREAM_TLV_FLAGS1);

	/* words 18-20: global type+subtype filter. Each dword packs the pkt
	 * tier in [15:0] and the status tier in [31:16].
	 */
	cmd->type_en_mgmt =
		le32_encode_bits(htt_tlv_filter->pkt_subtype_mgmt,
				 HTT_TX_MON_TYPE_EN_PKT_SUBTYPE_MASK) |
		le32_encode_bits(htt_tlv_filter->stat_subtype_mgmt,
				 HTT_TX_MON_TYPE_EN_STATUS_SUBTYPE_MASK);
	cmd->type_en_ctrl =
		le32_encode_bits(htt_tlv_filter->pkt_subtype_ctrl,
				 HTT_TX_MON_TYPE_EN_PKT_SUBTYPE_MASK) |
		le32_encode_bits(htt_tlv_filter->stat_subtype_ctrl,
				 HTT_TX_MON_TYPE_EN_STATUS_SUBTYPE_MASK);
	cmd->type_en_data =
		le32_encode_bits(htt_tlv_filter->pkt_subtype_data,
				 HTT_TX_MON_TYPE_EN_PKT_SUBTYPE_MASK) |
		le32_encode_bits(htt_tlv_filter->stat_subtype_data,
				 HTT_TX_MON_TYPE_EN_STATUS_SUBTYPE_MASK);

	if (!htt_tlv_filter->pkt_subtype_mgmt && !htt_tlv_filter->pkt_subtype_ctrl &&
	    !htt_tlv_filter->pkt_subtype_data) {
		cmd->type_en_mgmt |= cpu_to_le32(HTT_TX_MON_PKT_ALL_SUBTYPES);
		cmd->type_en_ctrl |= cpu_to_le32(HTT_TX_MON_PKT_ALL_SUBTYPES);
		cmd->type_en_data |= cpu_to_le32(HTT_TX_MON_PKT_ALL_SUBTYPES);
	}

	/* wifi8-only info0 control bit (BIT 28). Safe here: this op is
	 * only installed on the wifi8 arch-ops, where the HTT skb is sized to
	 * the full struct. pkt_buf_cnt_en (BIT 31) is encoded unconditionally
	 * in the common filter_setup and must not be re-encoded here.
	 */
	if (htt_tlv_filter->peer_ppdu_drop)
		cmd->info0 |= le32_encode_bits(1,
			HTT_TX_MON_RING_CFG_CMD_INFO0_PEER_PPDU_DROP);
}
EXPORT_SYMBOL(ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask);

/* Populate the EXT_MON filter slot from the persistent tx_ext_mon_config.
 * Mirrors ath12k_wifi7_dp_mon_tx_setup_ext_mon_filter, but translates the
 * per-type subtype masks into the wifi8 pkt/status subtype fields. The pkt
 * tier comes from fp.filter[]; the status tier from fp.status_filter[].
 *
 * Also mirrors wifi7's payload DMA length (fp.len[]) and MPDU/PPDU log-type
 * config. Without these, tx_mon_{mgmt,ctrl,data}_pkt_dma_len stay 0, which
 * HTT encodes literally as CONF_DMA_LEN_* = 0 (not "full packet") whenever
 * the corresponding tx_mon_*_filter is set - i.e. the frame type is enabled
 * for capture but its payload DMA length is programmed to zero, producing
 * a malformed/truncated payload in the captured pcap.
 */
static void
ath12k_wifi8_dp_mon_tx_setup_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev,
					    struct htt_tx_ring_tlv_filter *f)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon =
		&dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *cfg;
	u8 mgmt_len, ctrl_len, data_len;

	/* TLV subscription — same defaults as full monitor mode */
	f->tx_mon_downstream_tlv_flags = HTT_TX_MON_FILTER_DW_STRM_TLV_DEFAULT_MODE;
	f->tx_mon_upstream_tlv_flags0 = HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG0;
	f->tx_mon_upstream_tlv_flags1 = HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG1;
	f->tx_mon_upstream_tlv_flags2 = HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG2;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	cfg = tx_ext_mon->tx_ext_mon_config;
	if (!cfg || !cfg->enable) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		return;
	}

	/* pkt tier -> [15:0]; status tier -> [31:16] */
	f->pkt_subtype_mgmt = cfg->fp.filter[ATH12K_EXT_MON_FRAME_MGMT];
	f->pkt_subtype_ctrl = cfg->fp.filter[ATH12K_EXT_MON_FRAME_CTRL];
	f->pkt_subtype_data = cfg->fp.filter[ATH12K_EXT_MON_FRAME_DATA];
	f->stat_subtype_mgmt = cfg->fp.status_filter[ATH12K_EXT_MON_FRAME_MGMT];
	f->stat_subtype_ctrl = cfg->fp.status_filter[ATH12K_EXT_MON_FRAME_CTRL];
	f->stat_subtype_data = cfg->fp.status_filter[ATH12K_EXT_MON_FRAME_DATA];

	/* coarse type enables: set if any subtype requested in either tier */
	if (f->pkt_subtype_mgmt || f->stat_subtype_mgmt)
		f->tx_mon_mgmt_filter = 0x1;
	if (f->pkt_subtype_ctrl || f->stat_subtype_ctrl)
		f->tx_mon_ctrl_filter = 0x1;
	if (f->pkt_subtype_data || f->stat_subtype_data)
		f->tx_mon_data_filter = 0x1;

	/* Only the pkt tier carries payload. A status-tier-only request
	 * (stat_subtype_* set, pkt_subtype_* clear) must not get a payload
	 * DMA length programmed - that would contradict the status tier's
	 * "MPDU status TLVs only, no payload" design intent.
	 */
	mgmt_len = cfg->fp.len[ATH12K_EXT_MON_FRAME_MGMT];
	ctrl_len = cfg->fp.len[ATH12K_EXT_MON_FRAME_CTRL];
	data_len = cfg->fp.len[ATH12K_EXT_MON_FRAME_DATA];

	if (f->pkt_subtype_mgmt)
		f->tx_mon_mgmt_pkt_dma_len =
			ath12k_dp_mon_tx_get_ext_mon_filter_len(mgmt_len);
	if (f->pkt_subtype_ctrl)
		f->tx_mon_ctrl_pkt_dma_len =
			ath12k_dp_mon_tx_get_ext_mon_filter_len(ctrl_len);
	if (f->pkt_subtype_data)
		f->tx_mon_data_pkt_dma_len =
			ath12k_dp_mon_tx_get_ext_mon_filter_len(data_len);

	switch (cfg->level) {
	case ATH12K_EXT_MON_FILTER_LEVEL_MPDU:
	case ATH12K_EXT_MON_FILTER_LEVEL_PPDU:
		f->mgmt_mpdu_msdu_log_en = 1;
		f->mgmt_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		f->ctrl_mpdu_msdu_log_en = 1;
		f->ctrl_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		f->data_mpdu_msdu_log_en = 1;
		f->data_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		break;
	default:
		break;
	}

	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
}

/* wifi8 mode-aware TX monitor filter config. Mirrors the wifi7 version:
 * FULL_MONITOR / PKTLOG_HYBRID use the full-monitor filter, EXT_MON_MODE
 * uses the subtype-aware ext-mon filter, and SPL_PKT_CAP does exhaustive
 * MAC-filtered capture.
 */
int ath12k_wifi8_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					 bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter filter = {0};
	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;
	u8 mode;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_err(NULL, "TX Monitor: mon pdev / tx mon is NULL\n");
		return -EINVAL;
	}
	if (!dp || !dp->hal) {
		ath12k_err(NULL, "dp / dp hal invalid - skipping tx mon mode config\n");
		return -EINVAL;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	mode = tx_mon->tx_monitor_mode;

	if (enable) {
		filter.valid = true;
		src_tlv_filter = &filter.filter;
		switch (mode) {
		case DP_MON_TX_FULL_MONITOR:
		case DP_MON_TX_FILTER_PKTLOG_HYBRID:
			ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
			break;
		case DP_MON_TX_FILTER_EXT_MON_MODE:
			ath12k_wifi8_dp_mon_tx_setup_ext_mon_filter(dp_pdev,
								    src_tlv_filter);
			break;
		case DP_MON_TX_FILTER_SPL_PKT_CAP:
			ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
			src_tlv_filter->mac_addr_filter_en = 1;
			break;
		default:
			ath12k_err(NULL, "Tx monitor mode invalid - skipping tx mon mode config\n");
			return -EINVAL;
		}
		ath12k_hal_mon_tx_get_wmask_config(dp->hal, &src_tlv_filter->wmask);
		tx_mon->tx_mon_filter[mode][srng_type] = filter;
	} else {
		tx_mon->tx_mon_filter[mode][srng_type] = filter;
	}
	ath12k_dp_mon_tx_display_filters(dp, mode, &filter);
	return 0;
}
EXPORT_SYMBOL(ath12k_wifi8_dp_mon_tx_config_filter);
