// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "dp_mon_filter.h"
#include "debugfs.h"
#include "dp_rx.h"

int ath12k_dp_mon_rx_filter_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_rx_filter **rx_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev) {
		ath12k_warn(dp, "Monitor pdev is NULL\n");
		return -EINVAL;
	}

	rx_filter = (struct dp_mon_rx_filter **)kzalloc((
			sizeof(struct dp_mon_rx_filter *) * DP_MON_FILTER_MAX_MODE),
			GFP_KERNEL);

	if (!rx_filter) {
		ath12k_warn(dp, "rx filter alloc failed\n");
		return -ENOMEM;
	}

	dp_mon_pdev->rx_filter = rx_filter;

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		rx_filter[mode] = kzalloc(sizeof(struct dp_mon_rx_filter) *
					DP_MON_FILTER_SRNG_TYPE_MAX, GFP_KERNEL);
		if (!rx_filter[mode])
			goto free_rx_filter;
	}

	return 0;

free_rx_filter:
	ath12k_dp_mon_rx_filter_free(dp_pdev);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_filter_alloc);

void ath12k_dp_mon_rx_filter_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_rx_filter **rx_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev) {
		ath12k_warn(dp, "Monitor pdev is NULL\n");
		return;
	}

	rx_filter = dp_mon_pdev->rx_filter;

	if (!rx_filter) {
		ath12k_warn(dp, "Monitor rx filter is NULL\n");
		return;
	}

	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		if (!rx_filter[mode])
			continue;
		kfree(rx_filter[mode]);
		rx_filter[mode] = NULL;
	}

	kfree(rx_filter);
	dp_mon_pdev->rx_filter = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_filter_free);

static void
ath12k_dp_mon_rx_display_tlv_filters(struct ath12k_dp *dp,
					struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct ath12k_base *ab =  dp->ab;
	ath12k_dbg(ab, ATH12K_DBG_DATA, "Enable: %d",
		   !tlv_filter->rxmon_disable);
	ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MPDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MSDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "rx packet: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_RX_PACKET));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MSDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_MPDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "packet header: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "header per msdu: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "attention: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_ATTENTION));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu start: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_START));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end user stats: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end user stats ext: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end status done: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE));
	ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu start user info: %d",
		   u32_get_bits(tlv_filter->rx_filter,
				HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO));
}

void ath12k_dp_mon_rx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_filter_mode mode,
				      struct dp_mon_rx_filter *filter)
{
	struct ath12k_base *ab =  dp->ab;
	struct htt_rx_ring_tlv_filter *tlv_filter =
				&filter->rx_tlv_filter;

	ath12k_dbg(ab, ATH12K_DBG_DATA, "RX MON RING TLV FILTER CONFIG");
	ath12k_dbg(ab, ATH12K_DBG_DATA, "[Mode: %d]: Valid: %d",
		   mode, filter->valid);
	if (filter->valid) {
		ath12k_dp_mon_rx_display_tlv_filters(dp, tlv_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "offset valid: %d",
			   tlv_filter->offset_valid);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "drop threshold valid: %d",
			   tlv_filter->drop_threshold_valid);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "rx drop threshold: %d",
			   tlv_filter->rx_drop_threshold);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable fp: %d", tlv_filter->enable_fp);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable mo: %d", tlv_filter->enable_mo);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable md: %d", tlv_filter->enable_md);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log mgmt type: %d",
			   tlv_filter->enable_log_mgmt_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log ctrl type: %d",
			   tlv_filter->enable_log_ctrl_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable log data type: %d",
			   tlv_filter->enable_log_data_type);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len mgmt: %d",
			   tlv_filter->conf_len_mgmt);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len ctrl: %d",
			   tlv_filter->conf_len_ctrl);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "conf len data: %d",
			   tlv_filter->conf_len_data);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable rx tlv offset: %d",
			   tlv_filter->enable_rx_tlv_offset);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "rx tlv offset: %d",
			   tlv_filter->rx_tlv_offset);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_mgmt_filter: 0x%x",
			   tlv_filter->fp_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_ctrl_filter: 0x%x",
			   tlv_filter->fp_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_data_filter: 0x%x",
			   tlv_filter->fp_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_mgmt_filter: 0x%x",
			   tlv_filter->mo_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_ctrl_filter: 0x%x",
			   tlv_filter->mo_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_data_filter: 0x%x",
			   tlv_filter->mo_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_mgmt_filter: 0x%x",
			   tlv_filter->md_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_ctrl_filter: 0x%x",
			   tlv_filter->md_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_data_filter: 0x%x",
			   tlv_filter->md_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu start word mask: 0x%x",
			   tlv_filter->rx_mon_mpdu_start_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mpdu end word mask: 0x%x",
			   tlv_filter->rx_mon_mpdu_end_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "msdu end word mask: 0x%x",
			   tlv_filter->rx_mon_msdu_end_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "ppdu end usr stats word mask: 0x%x",
			   tlv_filter->rx_mon_ppdu_end_usr_stats_wmask);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable_fp_packet: %d",
			   tlv_filter->enable_fp_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_mgmt_filter: 0x%x",
			   tlv_filter->fp_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_ctrl_filter: 0x%x",
			   tlv_filter->fp_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "fp_packet_data_filter: 0x%x",
			   tlv_filter->fp_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable_mo_packet: %d",
			   tlv_filter->enable_mo_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_mgmt_filter: 0x%x",
			   tlv_filter->mo_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_ctrl_filter: 0x%x",
			   tlv_filter->mo_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "mo_packet_data_filter: 0x%x",
			   tlv_filter->mo_packet_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "enable md packet: %d",
			   tlv_filter->enable_md_packet);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_mgmt_filter: 0x%x",
			   tlv_filter->md_packet_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_ctrl_filter: 0x%x",
			   tlv_filter->md_packet_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DATA, "md_packet_data_filter: 0x%x",
			   tlv_filter->md_packet_data_filter);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_display_filters);

static void
ath12k_dp_mon_rx_setup_stats_filter(struct ath12k_dp *dp,
				    struct htt_rx_ring_tlv_filter *rx_tlv_filter,
				    enum dp_mon_stats_mode mode)
{
	u32 rx_filter = 0;

	rx_filter = HTT_RX_MON_FILTER_TLV_FLAGS;

	if (mode == ATH12k_DP_MON_EXTD_STATS)
		rx_filter = HTT_RX_MON_FILTER_TLV_EXTD_FLAGS;

	rx_tlv_filter->rx_filter = rx_filter;
	rx_tlv_filter->enable_fp = 1;
	rx_tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	rx_tlv_filter->fp_ctrl_filter = FILTER_CTRL_CTRLWRAP | FILTER_CTRL_BA_REQ |
					FILTER_CTRL_BA | FILTER_CTRL_PSPOLL |
					FILTER_CTRL_RTS | FILTER_CTRL_CTS |
					FILTER_CTRL_ACK | FILTER_CTRL_CFEND |
					FILTER_CTRL_CFEND_CFACK;
	rx_tlv_filter->fp_data_filter = FILTER_DATA_NULL | FILTER_DATA_MCAST |
					FILTER_DATA_UCAST;

	rx_tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	rx_tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	rx_tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	rx_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_stats_config_filter(struct ath12k_pdev_dp *dp_pdev,
					  enum dp_mon_stats_mode stats_mode,
					  bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode filter_mode = DP_MON_FILTER_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_stats_filter(dp, rx_tlv_filter,
						    stats_mode);
		ath12k_dp_mon_rx_display_filters(dp, filter_mode, &rx_filter);
		dp_mon_pdev->rx_filter[filter_mode][srng_type] = rx_filter;
	} else {
		ath12k_dp_mon_rx_display_filters(dp, filter_mode, &rx_filter);
		dp_mon_pdev->rx_filter[filter_mode][srng_type] = rx_filter;
	}
}

void ath12k_dp_mon_rx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_filter_srng_type srng_type,
				     struct dp_mon_rx_filter *rx_mon_filter)
{
	u32 mode = 0;
	struct ath12k_base *ab = dp->ab;
	struct htt_rx_ring_tlv_filter *dst_tlv_filter = &rx_mon_filter->rx_tlv_filter;
	struct htt_rx_ring_tlv_filter *src_tlv_filter;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_rx_filter *src_rx_mon_filter;

	/*
	 * loop through all the modes
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		src_rx_mon_filter = &dp_mon_pdev->rx_filter[mode][srng_type];
		src_tlv_filter = &src_rx_mon_filter->rx_tlv_filter;

		if (!src_rx_mon_filter->valid)
			continue;

		rx_mon_filter->valid = true;
		dst_tlv_filter->offset_valid |= src_tlv_filter->offset_valid;
		dst_tlv_filter->rx_filter |= src_tlv_filter->rx_filter;
		dst_tlv_filter->drop_threshold_valid |=
					src_tlv_filter->drop_threshold_valid;
		dst_tlv_filter->rx_drop_threshold |=
					src_tlv_filter->rx_drop_threshold;
		dst_tlv_filter->rx_hdr_len |=
					src_tlv_filter->rx_hdr_len;
		dst_tlv_filter->enable_log_mgmt_type |=
					src_tlv_filter->enable_log_mgmt_type;
		dst_tlv_filter->enable_log_ctrl_type |=
					src_tlv_filter->enable_log_ctrl_type;
		dst_tlv_filter->enable_log_data_type |=
					src_tlv_filter->enable_log_data_type;
		dst_tlv_filter->conf_len_ctrl |= src_tlv_filter->conf_len_ctrl;
		dst_tlv_filter->conf_len_mgmt |= src_tlv_filter->conf_len_mgmt;
		dst_tlv_filter->conf_len_data |= src_tlv_filter->conf_len_data;
		dst_tlv_filter->enable_rx_tlv_offset |=
					src_tlv_filter->enable_rx_tlv_offset;
		dst_tlv_filter->rx_tlv_offset |= src_tlv_filter->rx_tlv_offset;

		dst_tlv_filter->enable_fp |= src_tlv_filter->enable_fp;
		dst_tlv_filter->fp_mgmt_filter |= src_tlv_filter->fp_mgmt_filter;
		dst_tlv_filter->fp_ctrl_filter |= src_tlv_filter->fp_ctrl_filter;
		dst_tlv_filter->fp_data_filter |= src_tlv_filter->fp_data_filter;
		dst_tlv_filter->enable_mo |= src_tlv_filter->enable_mo;
		dst_tlv_filter->mo_mgmt_filter |= src_tlv_filter->mo_mgmt_filter;
		dst_tlv_filter->mo_ctrl_filter |= src_tlv_filter->mo_ctrl_filter;
		dst_tlv_filter->mo_data_filter |= src_tlv_filter->mo_data_filter;
		dst_tlv_filter->enable_md |= src_tlv_filter->enable_md;
		dst_tlv_filter->md_mgmt_filter |= src_tlv_filter->md_mgmt_filter;
		dst_tlv_filter->md_ctrl_filter |= src_tlv_filter->md_ctrl_filter;
		dst_tlv_filter->md_data_filter |= src_tlv_filter->md_data_filter;

		dst_tlv_filter->rx_mon_mpdu_start_wmask |=
					src_tlv_filter->rx_mon_mpdu_start_wmask;
		dst_tlv_filter->rx_mon_mpdu_end_wmask |=
					src_tlv_filter->rx_mon_mpdu_end_wmask;
		dst_tlv_filter->rx_mon_msdu_end_wmask |=
					src_tlv_filter->rx_mon_msdu_end_wmask;
		dst_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask |=
					src_tlv_filter->rx_mon_ppdu_end_usr_stats_wmask;
		dst_tlv_filter->enable_fp_packet |= src_tlv_filter->enable_fp_packet;
		dst_tlv_filter->fp_packet_mgmt_filter |=
					src_tlv_filter->fp_packet_mgmt_filter;
		dst_tlv_filter->fp_packet_ctrl_filter |=
					src_tlv_filter->fp_packet_ctrl_filter;
		dst_tlv_filter->fp_packet_data_filter |=
					src_tlv_filter->fp_packet_data_filter;
		dst_tlv_filter->enable_mo_packet |= src_tlv_filter->enable_mo_packet;
		dst_tlv_filter->mo_packet_mgmt_filter |=
					src_tlv_filter->mo_packet_mgmt_filter;
		dst_tlv_filter->mo_packet_ctrl_filter |=
					src_tlv_filter->mo_packet_ctrl_filter;
		dst_tlv_filter->mo_packet_data_filter |=
					src_tlv_filter->mo_packet_data_filter;
		dst_tlv_filter->enable_md_packet |= src_tlv_filter->enable_md_packet;
		dst_tlv_filter->md_packet_mgmt_filter |=
					src_tlv_filter->md_packet_mgmt_filter;
		dst_tlv_filter->md_packet_ctrl_filter |=
					src_tlv_filter->md_packet_ctrl_filter;
		dst_tlv_filter->md_packet_data_filter |=
					src_tlv_filter->md_packet_data_filter;

		dst_tlv_filter->rx_mon_fpmo_data_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_data_hdrlen;
		dst_tlv_filter->rx_mon_fpmo_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_fpmo_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_fpmo_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_fp_data_hdrlen |=
					src_tlv_filter->rx_mon_fp_data_hdrlen;
		dst_tlv_filter->rx_mon_fp_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_fp_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_fp_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_fp_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_mo_data_hdrlen |=
					src_tlv_filter->rx_mon_mo_data_hdrlen;
		dst_tlv_filter->rx_mon_mo_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_mo_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_mo_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_mo_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_md_data_hdrlen |=
					src_tlv_filter->rx_mon_md_data_hdrlen;
		dst_tlv_filter->rx_mon_md_ctrl_hdrlen |=
					src_tlv_filter->rx_mon_md_ctrl_hdrlen;
		dst_tlv_filter->rx_mon_md_mgmt_hdrlen |=
					src_tlv_filter->rx_mon_md_mgmt_hdrlen;
		dst_tlv_filter->rx_mon_enable_hdr_per_ppdu |=
					src_tlv_filter->rx_mon_enable_hdr_per_ppdu;

		dst_tlv_filter->sw0_buf_src_ppdu_hdr_en |=
					src_tlv_filter->sw0_buf_src_ppdu_hdr_en;
		dst_tlv_filter->mo_ppdu_hdr_en |=
					src_tlv_filter->mo_ppdu_hdr_en;
		dst_tlv_filter->md_ppdu_hdr_en |=
					src_tlv_filter->md_ppdu_hdr_en;
		dst_tlv_filter->fp_ppdu_hdr_en |=
					src_tlv_filter->fp_ppdu_hdr_en;
		dst_tlv_filter->fpmo_ppdu_hdr_en |=
					src_tlv_filter->fpmo_ppdu_hdr_en;
		dst_tlv_filter->fpmo_qos_null_data_ppdu_hdr_en |=
					src_tlv_filter->fpmo_qos_null_data_ppdu_hdr_en;
		dst_tlv_filter->fpmo_qos_null_tb_data_ppdu_hdr_en |=
					src_tlv_filter->fpmo_qos_null_tb_data_ppdu_hdr_en;
		dst_tlv_filter->fpmo_null_data_ppdu_hdr_en |=
					src_tlv_filter->fpmo_null_data_ppdu_hdr_en;
		dst_tlv_filter->fpmo_ucast_data_ppdu_hdr_en |=
					src_tlv_filter->fpmo_ucast_data_ppdu_hdr_en;
		dst_tlv_filter->fpmo_mcast_data_ppdu_hdr_en |=
					src_tlv_filter->fpmo_mcast_data_ppdu_hdr_en;
		dst_tlv_filter->fp_qos_null_data_ppdu_hdr_en |=
					src_tlv_filter->fp_qos_null_data_ppdu_hdr_en;
		dst_tlv_filter->fp_qos_null_tb_data_ppdu_hdr_en |=
					src_tlv_filter->fp_qos_null_tb_data_ppdu_hdr_en;
		dst_tlv_filter->fp_null_data_ppdu_hdr_en |=
					src_tlv_filter->fp_null_data_ppdu_hdr_en;
		dst_tlv_filter->fp_ucast_data_ppdu_hdr_en |=
					src_tlv_filter->fp_ucast_data_ppdu_hdr_en;
		dst_tlv_filter->fp_mcast_data_ppdu_hdr_en |=
					src_tlv_filter->fp_mcast_data_ppdu_hdr_en;

		ath12k_dbg(ab, ATH12K_DBG_DATA, "Updated Rx filters for mode: %d", mode);
		ath12k_dp_mon_rx_display_filters(dp, mode, rx_mon_filter);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_prepare_filter);

int
ath12k_dp_mon_rx_config_filters(struct ath12k_dp *dp,
				struct ath12k_pdev_dp *dp_pdev,
				enum dp_mon_filter_srng_type srng_type,
				struct htt_rx_ring_tlv_filter *rx_tlv_filter)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int i, ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		int ring_buf_size, mac_id, ring_id;
		enum hal_ring_type ring_type;

		switch (srng_type) {
		case DP_MON_FILTER_SRNG_TYPE_RXMON_DEST:
			ring_id = dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id;
			ring_type = HAL_RXDMA_MONITOR_DST;
			mac_id = dp_pdev->mac_id + i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF:
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ring_type = HAL_RXDMA_BUF;
			mac_id = i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF:
			ring_id =
			dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
			ring_type = HAL_RXDMA_MONITOR_BUF;
			mac_id = i;
			ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;
			break;
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS:
			ring_id =
			dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
			ring_type = HAL_RXDMA_MONITOR_STATUS;
			mac_id = i;
			ring_buf_size = RX_MON_STATUS_BUF_SIZE;
			break;
		default:
			return -EINVAL;
		}
		ret = ath12k_dp_tx_htt_rx_filter_setup(dp->ab, ring_id, mac_id,
						       ring_type, ring_buf_size,
						       rx_tlv_filter);
		if (ret) {
			ath12k_err(dp->ab,
				   "failed to setup filter for monitor ring id = %d"
				   "srng_type = %d ret = %d\n", ring_id, srng_type, ret);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_config_filters);

int ath12k_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	int ret = 0;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;

	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
					      rx_tlv_filter);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_ring_filter);

void
ath12k_dp_mon_rx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_rx_ring_tlv_filter *tlv_filter,
				       u32 rx_mon_tlv_filter_flags)
{
	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;
	tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	tlv_filter->enable_log_mgmt_type = true;
	tlv_filter->enable_log_ctrl_type = true;
	tlv_filter->enable_log_data_type = true;

	tlv_filter->conf_len_ctrl = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_mgmt = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_data = HTT_RX_RING_DEFAULT_DMA_LENGTH;

	tlv_filter->enable_rx_tlv_offset = true;
	tlv_filter->rx_tlv_offset = HTT_RX_RING_PKT_TLV_OFFSET;

	tlv_filter->enable_fp = 1;
	tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_data_filter = FILTER_DATA_ALL;
	tlv_filter->enable_mo = 1;
	tlv_filter->mo_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_data_filter = FILTER_DATA_ALL;

	tlv_filter->enable_fp_packet = 1;
	tlv_filter->fp_packet_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_packet_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_packet_data_filter = FILTER_DATA_ALL;
	tlv_filter->enable_mo_packet = 1;
	tlv_filter->mo_packet_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_packet_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_packet_data_filter = FILTER_DATA_ALL;

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);

	tlv_filter->rx_mon_fp_data_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_ctrl_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_mgmt_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_data_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_ctrl_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_mgmt_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_setup_mon_mode_filter);

void ath12k_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev,
					     bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	u32 rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_mon_mode_filter(dp,
						       rx_tlv_filter,
						       rx_mon_tlv_filter_flags);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

void static
ath12k_dp_mon_rx_setup_nrp_filters(struct ath12k_dp *dp,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	u32 rx_filter = 0;

	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MPDU_START;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END;
	rx_filter |= HTT_RX_FILTER_TLV_FLAGS_MPDU_END;

	tlv_filter->rx_filter = rx_filter;
	tlv_filter->enable_md = 1;
	tlv_filter->md_data_filter = FILTER_DATA_ALL;

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_nrp_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_NRP_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_nrp_filters(dp, rx_tlv_filter);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

static void
ath12k_dp_mon_rx_setup_smart_mon_filters(struct ath12k_dp *dp,
					 struct htt_rx_ring_tlv_filter *tlv_filter,
					 u8 smart_mon_filter)
{
	u32 rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING;

	tlv_filter->offset_valid = false;
	tlv_filter->rx_filter = rx_mon_tlv_filter_flags;
	tlv_filter->drop_threshold_valid = true;
	tlv_filter->rx_drop_threshold = HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE;
	tlv_filter->rx_hdr_len = HTT_RX_HDR_LEN_64_BYTES;

	tlv_filter->enable_log_mgmt_type = true;
	tlv_filter->enable_log_ctrl_type = true;
	tlv_filter->enable_log_data_type = true;

	tlv_filter->conf_len_ctrl = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_mgmt = HTT_RX_RING_DEFAULT_DMA_LENGTH;
	tlv_filter->conf_len_data = HTT_RX_RING_DEFAULT_DMA_LENGTH;

	tlv_filter->enable_rx_tlv_offset = true;
	tlv_filter->rx_tlv_offset = HTT_RX_RING_PKT_TLV_OFFSET;

	tlv_filter->enable_md = 1;
	tlv_filter->enable_md_packet = 1;

	if (!(smart_mon_filter & DP_SMART_MON_DATA_FILTER)) {
		tlv_filter->md_data_filter = FILTER_DATA_ALL;
		tlv_filter->md_packet_data_filter = FILTER_DATA_ALL;
	}

	if (!(smart_mon_filter & DP_SMART_MON_MGMT_FILTER)) {
		tlv_filter->md_mgmt_filter = FILTER_MGMT_ALL;
		tlv_filter->md_packet_mgmt_filter = FILTER_MGMT_ALL;
	}

	if (!(smart_mon_filter & DP_SMART_MON_CTRL_FILTER)) {
		tlv_filter->md_ctrl_filter = FILTER_CTRL_ALL;
		tlv_filter->md_packet_ctrl_filter = FILTER_CTRL_ALL;
	}

	tlv_filter->rx_mon_mpdu_start_wmask =
		ath12k_hal_mon_rx_mpdu_start_wmask(dp->hal);
	tlv_filter->rx_mon_mpdu_end_wmask =
		ath12k_hal_mon_rx_mpdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_msdu_end_wmask =
		ath12k_hal_mon_rx_msdu_end_wmask(dp->hal);
	tlv_filter->rx_mon_ppdu_end_usr_stats_wmask =
		ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(dp->hal);
}

void ath12k_dp_mon_rx_smart_mon_config_filter(struct ath12k_pdev_dp *dp_pdev,
					      bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_NRP_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	if (enable) {
		rx_filter.valid = true;
		rx_tlv_filter = &rx_filter.rx_tlv_filter;
		ath12k_dp_mon_rx_setup_smart_mon_filters(dp, rx_tlv_filter,
							 dp_mon_pdev->smart_mon_filter);
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	} else {
		memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
		ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
		dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
	}
}

void ath12k_dp_mon_rx_wmask_subscribe(void *ptr,
				      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;

	if (tlv_filter->rxmon_disable)
		return;

	if (tlv_filter->rx_mon_mpdu_start_wmask > 0 &&
	    tlv_filter->rx_mon_msdu_end_wmask > 0 &&
	    tlv_filter->rx_mon_ppdu_end_usr_stats_wmask > 0) {
		cmd->info2 |=
			le32_encode_bits(true,
					 HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACT_SET);
		cmd->rx_mon_mpdu_start_end_mask =
		le32_encode_bits(tlv_filter->rx_mon_mpdu_start_wmask,
				 HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_START_MASK);
		cmd->rx_mon_mpdu_start_end_mask |=
			le32_encode_bits(tlv_filter->rx_mon_mpdu_end_wmask,
					 HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_END_MASK);
		cmd->rx_mon_msdu_end_word_mask =
			le32_encode_bits(tlv_filter->rx_mon_msdu_end_wmask,
					 HTT_RX_RING_SELECTION_CFG_RX_MON_MSDU_END_MASK);
		cmd->rx_mon_ppdu_end_usr_stats_wmask =
		le32_encode_bits(tlv_filter->rx_mon_ppdu_end_usr_stats_wmask,
				 HTT_RX_RING_SELECTION_CFG_RX_MON_PPDU_END_USR_STATS_MASK);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_wmask_subscribe);

void ath12k_dp_mon_rx_enable_packet_filters(void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;
	u32 word;
	u32 fp_packet_mgmt_filter = tlv_filter->fp_packet_mgmt_filter;
	u32 fp_packet_ctrl_filter = tlv_filter->fp_packet_ctrl_filter;
	u32 fp_packet_data_filter = tlv_filter->fp_packet_data_filter;
	u32 mo_packet_mgmt_filter = tlv_filter->mo_packet_mgmt_filter;
	u32 mo_packet_ctrl_filter = tlv_filter->mo_packet_ctrl_filter;
	u32 mo_packet_data_filter = tlv_filter->mo_packet_data_filter;
	u32 md_packet_mgmt_filter = tlv_filter->md_packet_mgmt_filter;
	u32 md_packet_ctrl_filter = tlv_filter->md_packet_ctrl_filter;
	u32 md_packet_data_filter = tlv_filter->md_packet_data_filter;

	if (tlv_filter->rxmon_disable)
		return;

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(&word,
							     fp_packet_mgmt_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(&word,
							     mo_packet_mgmt_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(&word,
							     md_packet_mgmt_filter);
	cmd->pkt_type_en_data_flag0 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(&word,
							     fp_packet_mgmt_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(&word,
							     mo_packet_mgmt_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(&word,
							     md_packet_mgmt_filter);
	cmd->pkt_type_en_data_flag1 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(&word,
							     fp_packet_ctrl_filter);
	if (tlv_filter->enable_mo_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(&word,
							     mo_packet_ctrl_filter);
	if (tlv_filter->enable_md_packet)
		ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(&word,
							     md_packet_ctrl_filter);
	cmd->pkt_type_en_data_flag2 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fp_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(&word,
							     fp_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(&word,
							     fp_packet_data_filter);
	}
	if (tlv_filter->enable_mo_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(&word,
							     mo_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(&word,
							     mo_packet_data_filter);
	}
	if (tlv_filter->enable_md_packet) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(&word,
							     md_packet_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(&word,
							     md_packet_data_filter);
	}
	cmd->pkt_type_en_data_flag3 = cpu_to_le32(word);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_enable_packet_filters);

static void
ath12k_dp_mon_rx_pktlog_cmn_status(struct htt_rx_ring_tlv_filter *tlv_filter)
{
	tlv_filter->enable_mo = 1;
	tlv_filter->enable_fp = 1;
	tlv_filter->offset_valid = false;
	tlv_filter->fp_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->fp_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->fp_data_filter = FILTER_DATA_ALL;
	tlv_filter->mo_mgmt_filter = FILTER_MGMT_ALL;
	tlv_filter->mo_ctrl_filter = FILTER_CTRL_ALL;
	tlv_filter->mo_data_filter = FILTER_DATA_ALL;
}

static void
ath12k_dp_mon_rx_setup_pktlog_lite(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter = &rx_filter.rx_tlv_filter;

	rx_filter.valid = true;
	rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_PKTLOG_LITE;
	ath12k_dp_mon_rx_pktlog_cmn_status(rx_tlv_filter);

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

static void
ath12k_dp_mon_rx_pktlog_reset(struct ath12k_pdev_dp *dp_pdev,
			      enum dp_mon_filter_mode mode)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

static void
ath12k_dp_mon_rx_setup_pktlog_full(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKTLOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type = DP_MON_FILTER_SRNG_TYPE_RXMON_DEST;
	struct htt_rx_ring_tlv_filter *rx_tlv_filter = &rx_filter.rx_tlv_filter;

	rx_filter.valid = true;
	rx_tlv_filter->rx_filter = HTT_RX_FILTER_TLV_PKTLOG_FULL;
	ath12k_dp_mon_rx_pktlog_cmn_status(rx_tlv_filter);

	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

void ath12k_dp_mon_pktlog_config_filter(struct ath12k_pdev_dp *dp_pdev,
					enum ath12k_pktlog_mode mode,
					u32 filter, bool enable)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int ret = 0;

	if (enable) {
		if (dp->rx_pktlog_mode == mode) {
			ath12k_err(dp->ab, "This mode is already configured\n");
			return;
		}

		switch (mode) {
		case ATH12K_PKTLOG_MODE_LITE:
			dp->rx_pktlog_mode = ATH12K_PKTLOG_MODE_LITE;
			ret = ath12k_dp_tx_htt_h2t_ppdu_stats_req(dp_pdev->ar,
						HTT_PPDU_STATS_TAG_PKTLOG);
			if (ret)
				ath12k_err(dp->ab,
					   "failed to enable pktlog T2H: %d\n",
					   ret);

			ath12k_dp_mon_rx_setup_pktlog_lite(dp_pdev);
			break;
		case ATH12K_PKTLOG_MODE_FULL:
			if (!(filter & ATH12K_PKTLOG_RX))
				return;

			dp->rx_pktlog_mode = ATH12K_PKTLOG_MODE_FULL;

			ath12k_dp_mon_rx_setup_pktlog_full(dp_pdev);
			break;

		default:
			ath12k_err(dp->ab, "Please set a valid mode\n");
			break;
		}
	} else {
		switch (mode) {
		case ATH12K_PKTLOG_MODE_LITE:
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
					      DP_MON_FILTER_PKTLOG_LITE_MODE);
			break;
		case ATH12K_PKTLOG_MODE_FULL:
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
					      DP_MON_FILTER_PKTLOG_FULL_MODE);
			break;
		case ATH12K_PKTLOG_DISABLED:
			if (dp->rx_pktlog_mode == ATH12K_PKTLOG_DISABLED)
				return;

			ret = ath12k_dp_tx_htt_h2t_ppdu_stats_req(dp_pdev->ar,
						HTT_PPDU_STATS_TAG_DEFAULT);
			if (ret)
				ath12k_err(dp->ab,
				   "failed to reset htt ppdu stats: %d\n",ret);

			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
					      DP_MON_FILTER_PKTLOG_LITE_MODE);
			ath12k_dp_mon_rx_pktlog_reset(dp_pdev,
					      DP_MON_FILTER_PKTLOG_FULL_MODE);
			dp->rx_pktlog_mode = ATH12K_PKTLOG_DISABLED;
			break;

		default:
			ath12k_err(dp->ab, "Please set a valid mode\n");
			break;
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_pktlog_config_filter);

void ath12k_dp_htt_rx_filter_rxmon_cfg(void *ptr,
				       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd =
				(struct htt_rx_ring_selection_cfg_cmd *)ptr;

	cmd->info0 |= le32_encode_bits(!tlv_filter->rxmon_disable,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_EN_RXMON);
	cmd->info0 |=
		le32_encode_bits(!tlv_filter->rxmon_disable,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PKT_TYPE_EN_DATA);
}
EXPORT_SYMBOL(ath12k_dp_htt_rx_filter_rxmon_cfg);

int ath12k_dp_mon_tx_filter_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_tx_filter **tx_mon_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_tx_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	size_t rq_size = sizeof(struct dp_mon_tx_filter *) * DP_MON_TX_FILTER_MAX;

	if (!dp_mon_pdev || !dp) {
		ath12k_err(NULL, "Monitor pdev/dp is NULL\n");
		return -EINVAL;
	}

	tx_mon_filter = kzalloc(rq_size, GFP_KERNEL);
	if (!tx_mon_filter)
		return -ENOMEM;

	dp_mon_pdev->tx_mon_filter = tx_mon_filter;
	rq_size = sizeof(struct dp_mon_tx_filter) * DP_MON_TX_FILTER_SRNG_TYPE_MAX;
	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		tx_mon_filter[mode] = kzalloc(rq_size, GFP_KERNEL);
		if (!tx_mon_filter[mode])
			goto free_tx_filter;
	}

	return 0;

free_tx_filter:
	ath12k_dp_mon_tx_filter_free(dp_pdev);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_filter_alloc);

void ath12k_dp_mon_tx_filter_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_tx_filter **tx_mon_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	enum dp_mon_tx_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev || !dp) {
		ath12k_err(NULL, "Monitor pdev/dp is NULL - skip free\n");
		return;
	}

	tx_mon_filter = dp_mon_pdev->tx_mon_filter;

	if (!tx_mon_filter) {
		ath12k_warn(dp, "Monitor tx filter is NULL - skip free\n");
		return;
	}

	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		if (!tx_mon_filter[mode])
			continue;
		kfree(tx_mon_filter[mode]);
		tx_mon_filter[mode] = NULL;
	}

	kfree(tx_mon_filter);
	dp_mon_pdev->tx_mon_filter = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_filter_free);

void ath12k_dp_mon_tx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_tx_filter_mode mode,
				      struct dp_mon_tx_filter *filter)
{
	struct ath12k_base *ab =  dp->ab;
	struct htt_tx_ring_tlv_filter *src_tlv_filter =
				&filter->filter;
	if (!ab) {
		ath12k_err(NULL, "ath12k base invalid - skip tx filter display\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "TX MON RING TLV FILTER CONFIG");
	ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "[Mode: %d]: Valid: %d",
		   mode, filter->valid);
	if (filter->valid) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "downstream TLV Flags: 0x%X",
			   src_tlv_filter->tx_mon_downstream_tlv_flags);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-0: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags0);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-1: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags1);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-2: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags2);

		/* Print wmask configuration */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask pcu_ppdu_setup_init: 0x%X",
			   src_tlv_filter->wmask.pcu_ppdu_setup_init);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_peer_entry: 0x%X",
			   src_tlv_filter->wmask.tx_peer_entry);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_queue_ext: 0x%X",
			   src_tlv_filter->wmask.tx_queue_ext);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_status_end: 0x%X",
			   src_tlv_filter->wmask.tx_fes_status_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask response_end_status: 0x%X",
			   src_tlv_filter->wmask.response_end_status);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_status_prot: 0x%X",
			   src_tlv_filter->wmask.tx_fes_status_prot);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_setup: 0x%X",
			   src_tlv_filter->wmask.tx_fes_setup);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_msdu_start: 0x%X",
			   src_tlv_filter->wmask.tx_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_mpdu_start: 0x%X",
			   src_tlv_filter->wmask.tx_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask rxpcu_user_setup: 0x%X",
			   src_tlv_filter->wmask.rxpcu_user_setup);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask compaction_enable: %d",
			   src_tlv_filter->wmask.compaction_enable);

		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt filter enable: %d",
			   src_tlv_filter->tx_mon_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data filter enable: %d",
			   src_tlv_filter->tx_mon_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl filter enable: %d",
			   src_tlv_filter->tx_mon_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length data: %d",
			   src_tlv_filter->tx_mon_data_pkt_dma_len);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length ctrl: %d",
			   src_tlv_filter->tx_mon_ctrl_pkt_dma_len);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length mgmt: %d",
			   src_tlv_filter->tx_mon_mgmt_pkt_dma_len);

		/* Print MPDU/MSDU start/end flags */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_end: %d",
			   src_tlv_filter->mgmt_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_msdu_end: %d",
			   src_tlv_filter->mgmt_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_msdu_start: %d",
			   src_tlv_filter->mgmt_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_start: %d",
			   src_tlv_filter->mgmt_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_end: %d",
			   src_tlv_filter->ctrl_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_msdu_end: %d",
			   src_tlv_filter->ctrl_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_msdu_start: %d",
			   src_tlv_filter->ctrl_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_start: %d",
			   src_tlv_filter->ctrl_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_end: %d",
			   src_tlv_filter->data_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_msdu_end: %d",
			   src_tlv_filter->data_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_msdu_start: %d",
			   src_tlv_filter->data_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_start: %d",
			   src_tlv_filter->data_mpdu_start);

		/* Print additional boolean flags */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "txmon_disable: %d",
			   src_tlv_filter->txmon_disable);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_msdu_log_en: %d",
			   src_tlv_filter->mgmt_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_msdu_log_en: %d",
			   src_tlv_filter->ctrl_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_msdu_log_en: %d",
			   src_tlv_filter->data_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_log_typ: %d",
			   src_tlv_filter->mgmt_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_log_typ: %d",
			   src_tlv_filter->ctrl_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_log_typ: %d",
			   src_tlv_filter->data_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mac_addr_filter_en: %d",
			   src_tlv_filter->mac_addr_filter_en);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_display_filters);

void
ath12k_dp_mon_tx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_tx_ring_tlv_filter *src_tlv_filter)
{
	src_tlv_filter->tx_mon_downstream_tlv_flags =
					HTT_TX_MON_FILTER_DW_STRM_TLV_DEFAULT_MODE;
	src_tlv_filter->tx_mon_upstream_tlv_flags0 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG0;
	src_tlv_filter->tx_mon_upstream_tlv_flags1 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG1;
	src_tlv_filter->tx_mon_upstream_tlv_flags2 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG2;

	src_tlv_filter->tx_mon_mgmt_filter = 0x1;
	src_tlv_filter->tx_mon_data_filter = 0x1;
	src_tlv_filter->tx_mon_ctrl_filter = 0x1;

	src_tlv_filter->mgmt_mpdu_end = 1;
	src_tlv_filter->mgmt_msdu_end = 1;
	src_tlv_filter->mgmt_msdu_start = 1;
	src_tlv_filter->mgmt_mpdu_start = 1;
	src_tlv_filter->ctrl_mpdu_end = 1;
	src_tlv_filter->ctrl_msdu_end = 1;
	src_tlv_filter->ctrl_msdu_start = 1;
	src_tlv_filter->ctrl_mpdu_start = 1;
	src_tlv_filter->data_mpdu_end = 1;
	src_tlv_filter->data_msdu_end = 1;
	src_tlv_filter->data_msdu_start = 1;
	src_tlv_filter->data_mpdu_start = 1;

	src_tlv_filter->mgmt_mpdu_msdu_log_en = 1;
	src_tlv_filter->ctrl_mpdu_msdu_log_en = 1;
	src_tlv_filter->data_mpdu_msdu_log_en = 1;

	src_tlv_filter->mgmt_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	src_tlv_filter->ctrl_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	src_tlv_filter->data_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;

	src_tlv_filter->tx_mon_mgmt_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	src_tlv_filter->tx_mon_data_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	src_tlv_filter->tx_mon_ctrl_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_setup_mon_mode_filter);

int ath12k_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
				   bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter filter = {0};

	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	enum dp_mon_tx_filter_mode mode = DP_MON_TX_FULL_MONITOR;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!dp || !dp->hal) {
		ath12k_err(NULL, "dp / dp hal  invalid - skipping tx mon mode config\n");
		return -EINVAL;
	}

	if (enable) {
		filter.valid = true;
		src_tlv_filter = &filter.filter;
		ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
		ath12k_hal_mon_tx_get_wmask_config(dp->hal, &src_tlv_filter->wmask);
		dp_mon_pdev->tx_mon_filter[mode][srng_type] = filter;
	} else {
		dp_mon_pdev->tx_mon_filter[mode][srng_type] = filter;
	}
	ath12k_dp_mon_tx_display_filters(dp, mode, &filter);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_config_filter);

void ath12k_dp_mon_tx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_tx_filter_srng_type srng_type,
				     struct dp_mon_tx_filter *tx_mon_filter)
{
	enum dp_mon_tx_filter_mode mode = 0;
	struct htt_tx_ring_tlv_filter *dst_tlv_filter = &tx_mon_filter->filter;
	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_tx_filter *src_tx_mon_filter;
	struct hal_tx_mon_wmask_config *src_wmask;
	struct hal_tx_mon_wmask_config *dest_wmask = &tx_mon_filter->filter.wmask;

	/*
	 * loop through all the modes
	 */
	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		src_tx_mon_filter = &dp_mon_pdev->tx_mon_filter[mode][srng_type];
		src_tlv_filter = &src_tx_mon_filter->filter;
		src_wmask = &src_tlv_filter->wmask;

		if (!src_tx_mon_filter->valid)
			continue;

		tx_mon_filter->valid = true;
		dst_tlv_filter->tx_mon_downstream_tlv_flags |=
			src_tlv_filter->tx_mon_downstream_tlv_flags;
		dst_tlv_filter->tx_mon_upstream_tlv_flags0 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags0;
		dst_tlv_filter->tx_mon_upstream_tlv_flags1 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags1;
		dst_tlv_filter->tx_mon_upstream_tlv_flags2 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags2;
		dst_tlv_filter->tx_mon_mgmt_filter |=
					src_tlv_filter->tx_mon_mgmt_filter;
		dst_tlv_filter->tx_mon_data_filter |=
					src_tlv_filter->tx_mon_data_filter;
		dst_tlv_filter->tx_mon_ctrl_filter |=
					src_tlv_filter->tx_mon_ctrl_filter;

		dst_tlv_filter->mgmt_mpdu_end |= src_tlv_filter->mgmt_mpdu_end;
		dst_tlv_filter->mgmt_msdu_end |= src_tlv_filter->mgmt_msdu_end;
		dst_tlv_filter->mgmt_msdu_start |= src_tlv_filter->mgmt_msdu_start;
		dst_tlv_filter->mgmt_mpdu_start |= src_tlv_filter->mgmt_mpdu_start;
		dst_tlv_filter->ctrl_mpdu_end |= src_tlv_filter->ctrl_mpdu_end;
		dst_tlv_filter->ctrl_msdu_end |= src_tlv_filter->ctrl_msdu_end;
		dst_tlv_filter->ctrl_msdu_start |= src_tlv_filter->ctrl_msdu_start;
		dst_tlv_filter->ctrl_mpdu_start |= src_tlv_filter->ctrl_mpdu_start;
		dst_tlv_filter->data_mpdu_end |= src_tlv_filter->data_mpdu_end;
		dst_tlv_filter->data_msdu_end |= src_tlv_filter->data_msdu_end;
		dst_tlv_filter->data_msdu_start |= src_tlv_filter->data_msdu_start;
		dst_tlv_filter->data_mpdu_start |= src_tlv_filter->data_mpdu_start;
		dst_tlv_filter->mgmt_mpdu_msdu_log_en |=
					src_tlv_filter->mgmt_mpdu_msdu_log_en;
		dst_tlv_filter->ctrl_mpdu_msdu_log_en |=
					src_tlv_filter->ctrl_mpdu_msdu_log_en;
		dst_tlv_filter->data_mpdu_msdu_log_en |=
					src_tlv_filter->data_mpdu_msdu_log_en;

		dst_tlv_filter->mgmt_log_typ |= src_tlv_filter->mgmt_log_typ;
		dst_tlv_filter->ctrl_log_typ |= src_tlv_filter->ctrl_log_typ;
		dst_tlv_filter->data_log_typ |= src_tlv_filter->data_log_typ;

		dst_tlv_filter->txmon_disable |= src_tlv_filter->txmon_disable;
		dst_tlv_filter->tx_mon_mgmt_pkt_dma_len |=
					src_tlv_filter->tx_mon_mgmt_pkt_dma_len;
		dst_tlv_filter->tx_mon_data_pkt_dma_len |=
					src_tlv_filter->tx_mon_data_pkt_dma_len;
		dst_tlv_filter->tx_mon_ctrl_pkt_dma_len |=
					src_tlv_filter->tx_mon_ctrl_pkt_dma_len;

		dest_wmask->pcu_ppdu_setup_init |= src_wmask->pcu_ppdu_setup_init;
		dest_wmask->tx_peer_entry |= src_wmask->tx_peer_entry;
		dest_wmask->tx_queue_ext |= src_wmask->tx_queue_ext;
		dest_wmask->tx_fes_status_end |= src_wmask->tx_fes_status_end;
		dest_wmask->response_end_status |= src_wmask->response_end_status;
		dest_wmask->tx_fes_status_prot |= src_wmask->tx_fes_status_prot;
		dest_wmask->tx_fes_setup |= src_wmask->tx_fes_setup;
		dest_wmask->tx_msdu_start |= src_wmask->tx_msdu_start;
		dest_wmask->tx_mpdu_start |= src_wmask->tx_mpdu_start;
		dest_wmask->rxpcu_user_setup |= src_wmask->rxpcu_user_setup;
		dest_wmask->compaction_enable |= src_wmask->compaction_enable;

		ath12k_generic_dbg(ATH12K_DBG_DP_MON_TX, "Updated Tx filters for mode: %d",
				   mode);
		ath12k_dp_mon_tx_display_filters(dp, mode, tx_mon_filter);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_prepare_filter);

int
ath12k_dp_mon_tx_htt_update_filters(struct ath12k_dp *dp,
				    struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_tx_filter_srng_type srng_type,
				    struct htt_tx_ring_tlv_filter *tx_tlv_filter)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = -EINVAL;
	enum hal_ring_type ring_type = HAL_TX_MONITOR_DST;

	int ring_buf_size, mac_id, ring_id;

	if (!dp_mon_pdev || !dp->ab) {
		ath12k_err(NULL, "Tx Mon: mon pdev/base invalid - skip filter config\n");
		return ret;
	}

	ring_id = dp_mon_pdev->tx_mon_dst_ring.ring_id;
	mac_id = dp_pdev->mac_id;
	ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;

	ret = ath12k_dp_htt_mon_tx_filter_setup(dp->ab, ring_id, mac_id,
						ring_type, ring_buf_size,
						tx_tlv_filter);
	if (ret) {
		ath12k_err(dp->ab,
			   "Tx Mon filter setup fail ring = %d srng_type = %d (%d)\n",
			   ring_id, srng_type, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_htt_update_filters);

int ath12k_dp_mon_tx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter tx_mon_filter = {0};
	struct htt_tx_ring_tlv_filter *tx_tlv_filter;
	int ret = -EINVAL;
	enum dp_mon_tx_filter_srng_type srng_type = DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!dp) {
		ath12k_err(NULL, "dp invalid - skipping tx mon filter update\n");
		return ret;
	}

	tx_tlv_filter = &tx_mon_filter.filter;
	ath12k_dp_mon_tx_prepare_filter(dp, dp_pdev, srng_type, &tx_mon_filter);
	if (tx_mon_filter.valid)
		tx_tlv_filter->txmon_disable = false;
	else
		tx_tlv_filter->txmon_disable = true;

	ret = ath12k_dp_mon_tx_htt_update_filters(dp, dp_pdev, srng_type,
						  tx_tlv_filter);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_update_ring_filter);
