// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp_peer.h"
#include "dp_stats.h"
#include "debug.h"
#include "debugfs.h"
#include "qos.h"
#include "sdwf.h"
#include "telemetry.h"
#include "telemetry_agent_if.h"

/*
 * ath12k_dp_hist_sw_enq_dbucket: Software enqueue delay bucket in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */
static u16 ath12k_dp_hist_sw_enq_dbucket[HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};

/*
 * ath12k_dp_hist_hw_enque_dbucket: HW enqueue to Completion Delay in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */
static u16 ath12k_dp_hist_hw_enque_dbucket[HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};

/*
 * ath12k_dp_hist_rx_reap2stack_dbucket: Reap to stack bucket
 * @index_0 = 0_5 ms
 * @index_1 = 5_10 ms
 * @index_2 = 10_15 ms
 * @index_3 = 15_20 ms
 * @index_4 = 20_25 ms
 * @index_5 = 25_30 ms
 * @index_6 = 30_35 ms
 * @index_7 = 35_40 ms
 * @index_8 = 40_45 ms
 * @index_9 = 46_50 ms
 * @index_10 = 51_55 ms
 * @index_11 = 56_60 ms
 * @index_12 = 60+ ms
 */
static u16 ath12k_dp_hist_rx_reap2stack_dbucket[HIST_BUCKET_MAX] = {
	0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};

/*
 * ath12k_dp_hist_hw_tx_comp_dbucket: tx hw completion delay bucket in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */
static u16 ath12k_dp_hist_hw_tx_comp_dbucket[HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};

/*
 * ath12k_dp_pdev_sw_enq_dbucket: Pdev software enqueue delay bucket in ms
 * @index_0 = 0_1 ms
 * @index_1 = 1_2 ms
 * @index_2 = 2_3 ms
 * @index_3 = 3_4 ms
 * @index_4 = 4_5 ms
 * @index_5 = 5_6 ms
 * @index_6 = 6_7 ms
 * @index_7 = 7_8 ms
 * @index_8 = 8_9 ms
 * @index_9 = 9_10 ms
 * @index_10 = 10_11 ms
 * @index_11 = 11_12 ms
 * @index_12 = 12+ ms
 */
static u16 ath12k_dp_pdev_sw_enq_dbucket[HIST_BUCKET_MAX] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

/*
 * ath12k_dp_pdev_intfrm_dbucket: Pdev software inter-frame delay bucket in ms
 * @index_0 = 0_5 ms
 * @index_1 = 5_10 ms
 * @index_2 = 10_15 ms
 * @index_3 = 15_20 ms
 * @index_4 = 20_25 ms
 * @index_5 = 25_30 ms
 * @index_6 = 30_35 ms
 * @index_7 = 35_40 ms
 * @index_8 = 40_45 ms
 * @index_9 = 45_50 ms
 * @index_10 = 50_55 ms
 * @index_11 = 55_60 ms
 * @index_12 = 60+ ms
 */
static u16 ath12k_dp_pdev_intfrm_dbucket[HIST_BUCKET_MAX] = {
	0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};

/*
 * ath12k_dp_pdev_hwtx_dbucket: Pdev HW TX completion delay bucket in ms
 * @index_0 = 0_10 ms
 * @index_1 = 10_20 ms
 * @index_2 = 20_30 ms
 * @index_3 = 30_40 ms
 * @index_4 = 40_50 ms
 * @index_5 = 50_60 ms
 * @index_6 = 60_70 ms
 * @index_7 = 70_80 ms
 * @index_8 = 80_90 ms
 * @index_9 = 90_100 ms
 * @index_10 = 100_250 ms
 * @index_11 = 250_500 ms
 * @index_12 = 500+ ms
 */
static u16 ath12k_dp_pdev_hwtx_dbucket[HIST_BUCKET_MAX] = {
	0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 250, 500};

/**
 * ath12k_dp_aggr_per_pkt_tx_stats - Aggregate per-packet TX statistics
 * @dst_tx_stats: Destination TX stats structure
 * @src_tx_stats: Source TX stats structure to aggregate from
 *
 * Aggregates per-packet TX statistics from source to destination across all
 * WBM release reasons and TQM release reasons. Used during peer/VIF deletion
 * to preserve statistics.
 */
void ath12k_dp_aggr_per_pkt_tx_stats(struct ath12k_dp_peer_tx_stats *dst_tx_stats,
				     struct ath12k_dp_peer_tx_stats *src_tx_stats)
{
	int j;

	dst_tx_stats->comp_pkt.packets += src_tx_stats->comp_pkt.packets;
	dst_tx_stats->comp_pkt.bytes += src_tx_stats->comp_pkt.bytes;
	dst_tx_stats->tx_success.packets += src_tx_stats->tx_success.packets;
	dst_tx_stats->tx_success.bytes += src_tx_stats->tx_success.bytes;
	dst_tx_stats->tx_failed += src_tx_stats->tx_failed;
	for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
		dst_tx_stats->wbm_rel_reason[j] += src_tx_stats->wbm_rel_reason[j];
	for (j = 0; j < HAL_WBM_TQM_REL_REASON_MAX; j++)
		dst_tx_stats->tqm_rel_reason[j] += src_tx_stats->tqm_rel_reason[j];
	dst_tx_stats->release_src_not_tqm += src_tx_stats->release_src_not_tqm;
	dst_tx_stats->retry_count += src_tx_stats->retry_count;
	dst_tx_stats->total_msdu_retries += src_tx_stats->total_msdu_retries;
	dst_tx_stats->multiple_retry_count += src_tx_stats->multiple_retry_count;
	dst_tx_stats->ofdma += src_tx_stats->ofdma;
	dst_tx_stats->amsdu_cnt += src_tx_stats->amsdu_cnt;
	dst_tx_stats->non_amsdu_cnt += src_tx_stats->non_amsdu_cnt;
	dst_tx_stats->inval_link_id_pkt_cnt += src_tx_stats->inval_link_id_pkt_cnt;
	dst_tx_stats->ucast.packets += src_tx_stats->ucast.packets;
	dst_tx_stats->ucast.bytes += src_tx_stats->ucast.bytes;
	dst_tx_stats->mcast.packets += src_tx_stats->mcast.packets;
	dst_tx_stats->mcast.bytes += src_tx_stats->mcast.bytes;
	dst_tx_stats->bcast.packets += src_tx_stats->bcast.packets;
	dst_tx_stats->bcast.bytes += src_tx_stats->bcast.bytes;
}

/**
 * ath12k_dp_aggr_per_pkt_rx_stats - Aggregate per-packet RX statistics
 * @dst_rx_stats: Destination RX stats structure
 * @src_rx_stats: Source RX stats structure to aggregate from
 *
 * Aggregates per-packet RX statistics from source to destination including
 * packets received from REO, sent to stack, multicast/unicast counts, and
 * AMSDU/retry information.
 */
void ath12k_dp_aggr_per_pkt_rx_stats(struct ath12k_dp_peer_rx_stats *dst_rx_stats,
				     struct ath12k_dp_peer_rx_stats *src_rx_stats)
{
	dst_rx_stats->recv_from_reo.packets += src_rx_stats->recv_from_reo.packets;
	dst_rx_stats->recv_from_reo.bytes += src_rx_stats->recv_from_reo.bytes;
	dst_rx_stats->sent_to_stack.packets += src_rx_stats->sent_to_stack.packets;
	dst_rx_stats->sent_to_stack.bytes += src_rx_stats->sent_to_stack.bytes;
	dst_rx_stats->sent_to_stack_fast.packets +=
						src_rx_stats->sent_to_stack_fast.packets;
	dst_rx_stats->sent_to_stack_fast.bytes += src_rx_stats->sent_to_stack_fast.bytes;
	dst_rx_stats->mcast.packets += src_rx_stats->mcast.packets;
	dst_rx_stats->mcast.bytes += src_rx_stats->mcast.bytes;
	dst_rx_stats->ucast.packets += src_rx_stats->ucast.packets;
	dst_rx_stats->ucast.bytes += src_rx_stats->ucast.bytes;
	dst_rx_stats->non_amsdu += src_rx_stats->non_amsdu;
	dst_rx_stats->msdu_part_of_amsdu += src_rx_stats->msdu_part_of_amsdu;
	dst_rx_stats->mpdu_retry += src_rx_stats->mpdu_retry;
}

/**
 * ath12k_dp_update_tx_ext_htt_aggr_stats - Aggregate HTT TX statistics
 * @dst: Destination HTT TX stats structure
 * @src: Source HTT TX stats structure to aggregate from
*/
void
ath12k_dp_update_tx_ext_htt_aggr_stats(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_htt_tx_stats *dst_peer_stats,
				       struct ath12k_htt_tx_stats *src_peer_stats)
{
	int i, j, k;

	if (!dst_peer_stats || !src_peer_stats)
		return;

	/* htt stats */
	for (i = 0; i < ATH12K_STATS_TYPE_MAX; i++) {
		for (j = 0; j < ATH12K_COUNTER_TYPE_MAX; j++) {
			for (k = 0; k < ATH12K_LEGACY_NUM; k++) {
				dst_peer_stats->stats[i].legacy[j][k] +=
					src_peer_stats->stats[i].legacy[j][k];
			}

			for (k = 0; k < ATH12K_HT_MCS_NUM; k++) {
				dst_peer_stats->stats[i].ht[j][k] +=
					src_peer_stats->stats[i].ht[j][k];
			}

			for (k = 0; k < ATH12K_VHT_MCS_NUM; k++) {
				dst_peer_stats->stats[i].vht[j][k] +=
					src_peer_stats->stats[i].vht[j][k];
			}

			for (k = 0; k < ATH12K_HE_MCS_NUM; k++) {
				dst_peer_stats->stats[i].he[j][k] +=
					src_peer_stats->stats[i].he[j][k];
			}

			for (k = 0; k < ATH12K_EHT_MCS_NUM; k++) {
				dst_peer_stats->stats[i].eht[j][k] +=
					src_peer_stats->stats[i].eht[j][k];
			}

			for (k = 0; k < ATH12K_UHR_MCS_NUM; k++) {
				dst_peer_stats->stats[i].uhr[j][k] +=
					src_peer_stats->stats[i].uhr[j][k];
			}

			for (k = 0; k < ATH12K_BW_NUM; k++) {
				dst_peer_stats->stats[i].bw[j][k] +=
					src_peer_stats->stats[i].bw[j][k];
			}

			for (k = 0; k < ATH12K_NSS_NUM; k++) {
				dst_peer_stats->stats[i].nss[j][k] +=
					src_peer_stats->stats[i].nss[j][k];
			}

			for (k = 0; k < ATH12K_GI_NUM; k++) {
				dst_peer_stats->stats[i].gi[j][k] +=
					src_peer_stats->stats[i].gi[j][k];
			}

			for (k = 0; k < HTT_PPDU_STATS_PPDU_TYPE_MAX; k++) {
				dst_peer_stats->stats[i].transmit_type[j][k] +=
					src_peer_stats->stats[i].transmit_type[j][k];
			}

			for (k = 0; k < HAL_RX_RU_ALLOC_TYPE_MAX; k++) {
				dst_peer_stats->stats[i].ru_loc[j][k] +=
					src_peer_stats->stats[i].ru_loc[j][k];
			}
		}
	}

	dst_peer_stats->ba_fails += src_peer_stats->ba_fails;
	dst_peer_stats->ack_fails += src_peer_stats->ack_fails;

	/* TX Unicast Success */
	dst_peer_stats->tx_ucast_success.num += src_peer_stats->tx_ucast_success.num;
	dst_peer_stats->tx_ucast_success.bytes += src_peer_stats->tx_ucast_success.bytes;

	/* TX PPDUs */
	dst_peer_stats->tx_ppdus += src_peer_stats->tx_ppdus;

	/* MPDU BASIC */
	dst_peer_stats->tx_mpdus_success += src_peer_stats->tx_mpdus_success;
	dst_peer_stats->tx_mpdus_tried += src_peer_stats->tx_mpdus_tried;
	dst_peer_stats->retries_mpdu += src_peer_stats->retries_mpdu;

	if (!ath12k_dp_advance_stats_enabled(dp_pdev))
		return;

	/* DEBUG/ADV */
	dst_peer_stats->stbc += src_peer_stats->stbc;
	dst_peer_stats->ldpc += src_peer_stats->ldpc;

	/* WME AC Type Array */
	for (i = 0; i < WME_AC_MAX; i++)
		dst_peer_stats->wme_ac_type[i] += src_peer_stats->wme_ac_type[i];

	/* WME AC Type Bytes Array */
	for (i = 0; i < WME_AC_MAX; i++)
		dst_peer_stats->wme_ac_type_bytes[i] +=
			src_peer_stats->wme_ac_type_bytes[i];

	/* Excess Retries Per AC Array */
	for (i = 0; i < WME_AC_MAX; i++)
		dst_peer_stats->excess_retries_per_ac[i] +=
			src_peer_stats->excess_retries_per_ac[i];

	/* AMPDU/Non-AMPDU Counts */
	dst_peer_stats->ampdu_cnt += src_peer_stats->ampdu_cnt;
	dst_peer_stats->non_ampdu_cnt += src_peer_stats->non_ampdu_cnt;
	dst_peer_stats->num_ppdu_cookie_valid += src_peer_stats->num_ppdu_cookie_valid;

	dst_peer_stats->pream_punct_cnt += src_peer_stats->pream_punct_cnt;

	/* RU Location Array */
	for (i = 0; i < MAX_RU_LOCATIONS; i++) {
		dst_peer_stats->ru_loc_mpdu_succ_tried[i].num_mpdu +=
			src_peer_stats->ru_loc_mpdu_succ_tried[i].num_mpdu;
		dst_peer_stats->ru_loc_mpdu_succ_tried[i].mpdu_tried +=
			src_peer_stats->ru_loc_mpdu_succ_tried[i].mpdu_tried;
	}

	/* Transmit Type Array */
	for (i = 0; i < MAX_TRANSMIT_TYPES; i++) {
		dst_peer_stats->transmit_type_mpdu_succ_tried[i].num_mpdu +=
			src_peer_stats->transmit_type_mpdu_succ_tried[i].num_mpdu;
		dst_peer_stats->transmit_type_mpdu_succ_tried[i].mpdu_tried +=
			src_peer_stats->transmit_type_mpdu_succ_tried[i].mpdu_tried;
	}

	/* SU BE PPDU Count */
	for (i = 0; i < MAX_MCS; i++)
		dst_peer_stats->su_be_ppdu_cnt.mcs_count[i] +=
			src_peer_stats->su_be_ppdu_cnt.mcs_count[i];

	/* MU BE PPDU Count Array */
	for (i = 0; i < TXRX_TYPE_MU_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++)
			dst_peer_stats->mu_be_ppdu_cnt[i].mcs_count[j] +=
				src_peer_stats->mu_be_ppdu_cnt[i].mcs_count[j];
	}

	/* SU BN PPDU Count */
	for (i = 0; i < MAX_MCS; i++)
		dst_peer_stats->su_bn_ppdu_cnt.mcs_count[i] +=
			src_peer_stats->su_bn_ppdu_cnt.mcs_count[i];

	/* MU BN PPDU Count Array */
	for (i = 0; i < TXRX_TYPE_MU_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++)
			dst_peer_stats->mu_bn_ppdu_cnt[i].mcs_count[j] +=
				src_peer_stats->mu_bn_ppdu_cnt[i].mcs_count[j];
	}

	/* Punctured BW Array */
	for (i = 0; i < MAX_PUNCTURED_MODE; i++)
		dst_peer_stats->punc_bw[i] += src_peer_stats->punc_bw[i];

	/* RTS/BAR/NDPA Counts */
	dst_peer_stats->rts_success += src_peer_stats->rts_success;
	dst_peer_stats->rts_failure += src_peer_stats->rts_failure;
	dst_peer_stats->bar_cnt += src_peer_stats->bar_cnt;
	dst_peer_stats->ndpa_cnt += src_peer_stats->ndpa_cnt;

	/* TX MSDU Flush Reason Array */
	for (i = 0; i < HTT_FLUSH_MAX; i++)
		dst_peer_stats->tx_msdu_flush_rsn[i] +=
			src_peer_stats->tx_msdu_flush_rsn[i];

}

/**
 * ath12k_dp_aggr_wbm_rx_stats - Aggregate WBM RX error statistics
 * @dst: Destination WBM RX stats structure
 * @src: Source WBM RX stats structure to aggregate from
 *
 * Aggregates WBM (Wireless Buffer Manager) RX error statistics including
 * RXDMA errors and REO errors.
 */
void ath12k_dp_aggr_wbm_rx_stats(struct ath12k_wbm_rx_stats *dst,
				 struct ath12k_wbm_rx_stats *src)
{
	int i;

	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		dst->rxdma_error[i] += src->rxdma_error[i];

	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		dst->reo_error[i] += src->reo_error[i];
}

/**
 * ath12k_dp_clear_wbm_rx_stats - Clear WBM RX error statistics
 * @wbm_stats: WBM RX stats structure to clear
 *
 * Clears all WBM RX error statistics by zeroing the structure.
 */
void ath12k_dp_clear_wbm_rx_stats(struct ath12k_wbm_rx_stats *wbm_stats)
{
	memset(wbm_stats, 0, sizeof(struct ath12k_wbm_rx_stats));
}

/**
 * ath12k_dp_clear_per_pkt_tx_stats - Clear per-packet TX statistics
 * @tx_peer_stats: Peer stats structure containing TX stats to clear
 *
 * Clears per-packet TX statistics across all TCL rings.
 */
void ath12k_dp_clear_per_pkt_tx_stats(struct ath12k_dp_peer_stats *tx_peer_stats)
{
	int i;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		memset(&tx_peer_stats->tx[i], 0, sizeof(struct ath12k_dp_peer_tx_stats));
}

/**
 * ath12k_dp_clear_per_pkt_rx_stats - Clear per-packet RX statistics
 * @rx_peer_stats: Peer stats structure containing RX stats to clear
 *
 * Clears per-packet RX statistics across all REO destination rings.
 */
void ath12k_dp_clear_per_pkt_rx_stats(struct ath12k_dp_peer_stats *rx_peer_stats)
{
	int i;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		memset(&rx_peer_stats->rx[i], 0, sizeof(struct ath12k_dp_peer_rx_stats));
}

/**
 * ath12k_dp_aggr_del_stats() - Aggregate stats from a deleted entity
 * @dst_peer_stats: The destination peer stats structure.
 * @dst_link_peer_stats: Destination link peer stats structure
 * @src: The source structure containing stats from the deleted entity.
 * @stats_type: A string for logging that identifies the aggregation context.
 *
 * This function preserves historical statistics by merging data from a
 * deleted or disassociated network entity into a parent or aggregate
 * structure. This is essential for accurate, long-term telemetry and
 * debugging.
 *
 * If @src is NULL, the function returns without performing any aggregation.
 *
 * The function iterates over all TCL and REO rings, invoking per-ring
 * helpers to merge the counters. Also the extended HTT and Rx peer stats
 * would be aggregated.
 *
 * It is used in several hierarchical scenarios:
 * 1. MLD Peer Aggregation:
 *    When a link peer in MLD is deleted, this function is used to store the stats of
 *    deleted link peer to 'link_peer_delete_stats' of respective MLD peer.
 *
 * 2. Link VIF Aggregation:
 *    When a link peer is deleted, this function is used to preserve the
 *    stats of link peer into 'link_peer_delete_stats' of link VIF.
 *
 * 3. MLD VIF Aggregation:
 *    When a link VIF is deleted, this function is used to preserve the deleted
 *    link VIF into 'link_vif_delete_stats' of MLD VIF.
 */

void ath12k_dp_aggr_del_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
			      struct ath12k_dp_link_peer_stats *dst_link_peer_stats,
			      struct ath12k_dp_preserved_stats *src,
			      const char *stats_type)
{
	u8 i;

	if (!src || !dst_peer_stats) {
		ath12k_err(NULL, "%s not found\n", stats_type);
		return;
	}

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_tx_stats(&dst_peer_stats->tx[i],
						&src->per_pkt_tx[i]);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_rx_stats(&dst_peer_stats->rx[i],
						&src->per_pkt_rx[i]);

	ath12k_dp_aggr_wbm_rx_stats(&dst_peer_stats->wbm_err,
				    &src->wbm_err);
}

static u8 ath12k_dp_get_bw_offset(u8 bw)
{
	switch (bw) {
	case HAL_RX_BW_20MHZ:
		return PKT_BW_GAIN_20MHZ;
	case HAL_RX_BW_40MHZ:
		return PKT_BW_GAIN_40MHZ;
	case HAL_RX_BW_80MHZ:
		return PKT_BW_GAIN_80MHZ;
	case HAL_RX_BW_160MHZ:
		return PKT_BW_GAIN_160MHZ;
	case HAL_RX_BW_320MHZ:
		return PKT_BW_GAIN_320MHZ;
	default:
		return 0;
	}
}

/**
 * ath12k_dp_get_rssi_value - Calculate RSSI based on given SNR
 * @snr:      Input SNR (either snr or snr_dp)
 * @stats:    Peer signal stats (for region offset etc.)
 * @rssi_offsets: Conversion offsets
 *
 * Returns: Calculated RSSI value (s8)
 */
s8 ath12k_dp_get_rssi_value(s8 snr,
			    struct ath12k_dp_link_peer_rx_signal_stats *stats,
			    struct wmi_rssi_dbm_conv_offsets *rssi_offsets,
			    bool ack_rssi)
{
	s8 rssi_comb, rssi_val;
	u8 bw_offset = 0;

	if (ack_rssi && snr < 0)
		return 0;

	bw_offset = ath12k_dp_get_bw_offset(stats->channel_bw);

	/* Common offset calculation */
	rssi_comb = stats->rssi_region_offset +
		rssi_offsets->avg_nf_dbm +
		rssi_offsets->rssi_temp_offset + bw_offset;

	/* RSSI calculation */
	rssi_val = snr + rssi_comb;
	if (snr > rssi_offsets->xlna_bypass_threshold)
		rssi_val += rssi_offsets->xlna_bypass_offset;

	return rssi_val;
}

/**
 * ath12k_dp_clear_preserved_stats - Clear preserved statistics
 * @stats: Preserved stats structure to clear
 *
 */
void ath12k_dp_clear_preserved_stats(struct ath12k_dp_preserved_stats *stats)
{
	int i;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		memset(&stats->per_pkt_tx[i], 0, sizeof(stats->per_pkt_tx[i]));
	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		memset(&stats->per_pkt_rx[i], 0, sizeof(stats->per_pkt_rx[i]));
	memset(&stats->wbm_err, 0, sizeof(stats->wbm_err));
}

/**
 * ath12k_dp_accumulate_hist_stats() - Accumulate src hist_stats into dst
 * @src_hist_stats: Source histogram stats (per-ring)
 * @dst_hist_stats: Destination histogram stats (aggregated output)
 *
 * Accumulates frequency buckets and updates min/max/avg in dst.
 * The hist_type of dst is preserved; src's hist_type is ignored.
 */
void ath12k_dp_accumulate_hist_stats(struct hist_stats *src_hist_stats,
				     struct hist_stats *dst_hist_stats)
{
	u8 index;

	if (!src_hist_stats || !dst_hist_stats)
		return;

	for (index = 0; index < HIST_BUCKET_MAX; index++)
		dst_hist_stats->hist.freq[index] += src_hist_stats->hist.freq[index];

	if (src_hist_stats->min < dst_hist_stats->min)
		dst_hist_stats->min = src_hist_stats->min;

	if (src_hist_stats->max > dst_hist_stats->max)
		dst_hist_stats->max = src_hist_stats->max;

	if (!dst_hist_stats->avg)
		dst_hist_stats->avg = src_hist_stats->avg;
	else if (src_hist_stats->avg)
		dst_hist_stats->avg = (dst_hist_stats->avg + src_hist_stats->avg) / 2;
}

/*
 * ath12k_dp_pdev_get_tid_stats - Aggregate per-TID statistics across rings
 * @ar: ath12k radio instance
 * @tid_stats: Pre-allocated structure for aggregated statistics
 *
 * Aggregates per-TID counters, delays, and errors from all hardware rings
 * into per-TID totals.
 *
 * Return: 0 on success, -EINVAL if parameters are invalid
 */
int ath12k_dp_pdev_get_tid_stats(struct ath12k *ar,
				 struct ath12k_dp_aggr_pdev_tid_stats *tid_stats)
{
	struct ath12k_tid_tx_stats *per_ring_tx;
	struct ath12k_tid_rx_stats *per_ring_rx;
	struct ath12k_tid_tx_stats *aggr_tx;
	struct ath12k_tid_rx_stats *aggr_rx;
	u8 tid;
	int ring_id, i;

	if (!ar || !tid_stats)
		return -EINVAL;

	/* Clear output structure */
	memset(tid_stats, 0, sizeof(*tid_stats));

	for (tid = 0; tid < VOW_DATA_TID_MAX; tid++) {
		aggr_tx = &tid_stats->tid_tx[tid];
		aggr_tx->swq_delay.min = U32_MAX;
		aggr_tx->hwtx_delay.min = U32_MAX;
		aggr_tx->intfrm_delay.min = U32_MAX;

		aggr_rx = &tid_stats->tid_rx[tid];
		aggr_rx->to_stack_delay.min = U32_MAX;
		aggr_rx->intfrm_delay.min = U32_MAX;
	}

	/* Aggregate stats for each TID (0-8) */
	for (tid = 0; tid < VOW_DATA_TID_MAX; tid++) {
		aggr_tx = &tid_stats->tid_tx[tid];
		aggr_rx = &tid_stats->tid_rx[tid];
		/* Aggregate TX counters from all TCL rings */
		for (ring_id = 0; ring_id < DP_TCL_NUM_RING_MAX; ring_id++) {
			per_ring_tx = &ar->dp.tid_stats.tid_tx[ring_id][tid];

			/* Aggregate TQM status counters */
			for (i = 0; i < HAL_WBM_TQM_REL_REASON_MAX_EXT; i++)
				aggr_tx->tqm_status_cnt[i] +=
					per_ring_tx->tqm_status_cnt[i];

			/* Aggregate HTT status counters */
			for (i = 0; i < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; i++)
				aggr_tx->htt_status_cnt[i] +=
					per_ring_tx->htt_status_cnt[i];

			/* Aggregate SW drop counters */
			for (i = 0; i < DP_TID_TX_SW_DROP_MAX; i++)
				aggr_tx->swdrop_cnt[i] +=
					per_ring_tx->swdrop_cnt[i];

			/* Aggregate delay histograms */
			ath12k_dp_accumulate_hist_stats(&per_ring_tx->swq_delay,
							&aggr_tx->swq_delay);
			ath12k_dp_accumulate_hist_stats(&per_ring_tx->hwtx_delay,
							&aggr_tx->hwtx_delay);
			ath12k_dp_accumulate_hist_stats(&per_ring_tx->intfrm_delay,
							&aggr_tx->intfrm_delay);
		}

		/* Aggregate RX counters from all REO rings */
		for (ring_id = 0; ring_id < DP_REO_DST_RING_MAX; ring_id++) {
			per_ring_rx = &ar->dp.tid_stats.tid_rx[ring_id][tid];

			/* Aggregate normal RX counters */
			aggr_rx->msdu_cnt += per_ring_rx->msdu_cnt;
			aggr_rx->mcast_msdu_cnt += per_ring_rx->mcast_msdu_cnt;
			aggr_rx->bcast_msdu_cnt += per_ring_rx->bcast_msdu_cnt;
			aggr_rx->delivered_to_stack += per_ring_rx->delivered_to_stack;

			/* Aggregate Rx SW drop counters */
			for (i = 0; i < DP_TID_RX_SW_DROP_MAX_EXT; i++)
				aggr_rx->fail_cnt[i] += per_ring_rx->fail_cnt[i];

			ath12k_dp_accumulate_hist_stats(&per_ring_rx->to_stack_delay,
							&aggr_rx->to_stack_delay);
			ath12k_dp_accumulate_hist_stats(&per_ring_rx->intfrm_delay,
							&aggr_rx->intfrm_delay);
		}

		tid_stats->tid_reo_err[tid] = ar->dp.tid_stats.tid_reo_err[tid];
		tid_stats->tid_rxdma_err[tid] = ar->dp.tid_stats.tid_rxdma_err[tid];

		if (aggr_tx->swq_delay.min == U32_MAX)
			aggr_tx->swq_delay.min = 0;
		if (aggr_tx->hwtx_delay.min == U32_MAX)
			aggr_tx->hwtx_delay.min = 0;
		if (aggr_tx->intfrm_delay.min == U32_MAX)
			aggr_tx->intfrm_delay.min = 0;
		if (aggr_rx->to_stack_delay.min == U32_MAX)
			aggr_rx->to_stack_delay.min = 0;
		if (aggr_rx->intfrm_delay.min == U32_MAX)
			aggr_rx->intfrm_delay.min = 0;
	}

	return 0;
}

/*
 * ath12k_dp_hist_find_bucket_idx() - Find the bucket index
 * @bucket_array: Bucket array
 * @value: Frequency value
 *
 * Return: The bucket index
 */
static int ath12k_dp_hist_find_bucket_idx(u16 *bucket_array, u32 value)
{
	u8 idx = HIST_BUCKET_0;

	for (; idx < (HIST_BUCKET_MAX - 1); idx++) {
		if (value <= bucket_array[idx + 1])
			break;
	}

	return idx;
}

/**
 * ath12k_dp_hist_fill_buckets() - Fill the histogram frequency buckets
 * @hist_bucket: Histogram bukcets
 * @value: Frequency value
 *
 * Return: void
 */
static void ath12k_dp_hist_fill_buckets(struct hist_bucket *hist_bucket, u32 value)
{
	enum hist_types hist_type;
	u8 idx = HIST_BUCKET_MAX;

	if (unlikely(!hist_bucket))
		return;

	hist_type = hist_bucket->hist_type;

	switch (hist_type) {
	case HIST_TYPE_SW_ENQEUE_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_hist_sw_enq_dbucket[0],
						   value);
		break;
	case HIST_TYPE_HW_COMP_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_hist_hw_enque_dbucket[0],
						   value);
		break;
	case HIST_TYPE_REAP_STACK:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_hist_rx_reap2stack_dbucket[0],
						   value);
		break;
	case HIST_TYPE_HW_TX_COMP_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_hist_hw_tx_comp_dbucket[0],
						   value);
		break;
	case HIST_TYPE_PDEV_SW_ENQEUE_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_pdev_sw_enq_dbucket[0],
						   value);
		break;
	case HIST_TYPE_PDEV_HW_TX_COMP_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_pdev_hwtx_dbucket[0],
						   value);
		break;
	case HIST_TYPE_PDEV_SW_INTERFRAME_DELAY:
		idx =
		    ath12k_dp_hist_find_bucket_idx(&ath12k_dp_pdev_intfrm_dbucket[0],
						   value);
		break;
	default:
		__ath12k_warn(NULL, "Unknown hist_type %d\n", hist_type);
		break;
	}

	if (idx == HIST_BUCKET_MAX)
		return;

	hist_bucket->freq[idx]++;
}

void ath12k_dp_update_hist_stats(struct hist_stats *hist_stats, u32 value)
{
	if (unlikely(!hist_stats))
		return;

	ath12k_dp_hist_fill_buckets(&hist_stats->hist, value);

	if (value < hist_stats->min)
		hist_stats->min = value;

	if (value > hist_stats->max)
		hist_stats->max = value;

	if (unlikely(!hist_stats->avg))
		hist_stats->avg = value;
	else
		hist_stats->avg = (hist_stats->avg + value) / 2;
}
EXPORT_SYMBOL(ath12k_dp_update_hist_stats);

void ath12k_dp_hist_init(struct hist_stats *hist_stats,
			 enum hist_types hist_type)
{
	memset(hist_stats, 0, sizeof(struct hist_stats));
	hist_stats->min =  U32_MAX;
	hist_stats->hist.hist_type = hist_type;
}
EXPORT_SYMBOL(ath12k_dp_hist_init);

/**
 * ath12k_dp_tid_rx_stats_hist_init - Initialize delay histograms for per-TID RX stats
 * @dp_pdev: DP pdev handle
 *
 * Initializes delay histograms (reap-to-stack and interframe) for all
 * per ring, per tid during pdev allocation.
 */
void ath12k_dp_tid_rx_stats_hist_init(struct ath12k_pdev_dp *dp_pdev)
{
	int ring, tid;

	if (!dp_pdev)
		return;

	for (ring = 0; ring < DP_REO_DST_RING_MAX; ring++) {
		for (tid = 0; tid < VOW_DATA_TID_MAX; tid++) {
			struct ath12k_tid_rx_stats *tid_rx;

			tid_rx = &dp_pdev->tid_stats.tid_rx[ring][tid];
			ath12k_dp_hist_init(&tid_rx->to_stack_delay,
					    HIST_TYPE_REAP_STACK);
			ath12k_dp_hist_init(&tid_rx->intfrm_delay,
					    HIST_TYPE_PDEV_SW_INTERFRAME_DELAY);
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_tid_rx_stats_hist_init);

/**
 * ath12k_dp_tid_tx_stats_hist_init() - Initialize delay histograms for per-TID TX stats
 * @dp_pdev: DP pdev handle
 *
 * Initializes the SW enqueue delay, HW TX completion delay, and interframe
 * delay histogram objects for every ring/TID slot in dp_pdev->tid_stats.
 * Must be called during pdev allocation before any delay stats are updated.
 */
void ath12k_dp_tid_tx_stats_hist_init(struct ath12k_pdev_dp *dp_pdev)
{
	int ring, tid;

	if (!dp_pdev)
		return;

	for (ring = 0; ring < DP_TCL_NUM_RING_MAX; ring++) {
		for (tid = 0; tid < VOW_DATA_TID_MAX; tid++) {
			struct ath12k_tid_tx_stats *tid_tx =
				&dp_pdev->tid_stats.tid_tx[ring][tid];

			ath12k_dp_hist_init(&tid_tx->swq_delay,
					    HIST_TYPE_PDEV_SW_ENQEUE_DELAY);
			ath12k_dp_hist_init(&tid_tx->hwtx_delay,
					    HIST_TYPE_PDEV_HW_TX_COMP_DELAY);
			ath12k_dp_hist_init(&tid_tx->intfrm_delay,
					    HIST_TYPE_PDEV_SW_INTERFRAME_DELAY);
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_tid_tx_stats_hist_init);

void ath12k_qos_tx_enqueue_peer_stats(struct ath12k_dp_peer *mld_peer,
				      u8 hw_link_id, u16 msduq_id,
				      unsigned int len)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_qos_tx_stats *qos_tx;
	struct ath12k_mld_qos_stats *mld_qos;
	u8 tid, q_id;

	if (!mld_peer || !mld_peer->qos)
		return;

	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(mld_peer, hw_link_id);
	if (!link_peer || !link_peer->peer_stats.link_qos_stats) {
		ath12k_err(NULL, "Qos stats not initialized\n");
		return;
	}

	if (unlikely(msduq_id >= QOS_MSDUQ_MAX || msduq_id < MSDUQ_MAX_DEF))
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);

	qos_tx = &link_peer->peer_stats.link_qos_stats->tx[tid][q_id];

	spin_lock_bh(&mld_peer->qos->lock);
	if (mld_peer->mld_stats.mld_qos_stats) {
		mld_qos = &mld_peer->mld_stats.mld_qos_stats[
				tid * QOS_TID_MDSUQ_MAX + q_id];
		mld_qos->queue_depth++;
	}

	qos_tx->tx_ingress.num++;
	qos_tx->tx_ingress.bytes += len;
	spin_unlock_bh(&mld_peer->qos->lock);
}
EXPORT_SYMBOL(ath12k_qos_tx_enqueue_peer_stats);

void ath12k_sdwf_update_peer_mcs_stats(struct ath12k_dp_qos_tx_stats *qos_tx,
				       struct hal_tx_status *ts)
{
	u8 mcs = MAX_MCS, pkt_type;

	mcs = ts->mcs;
	pkt_type = ts->pkt_type;

	if (pkt_type > HAL_TX_RATE_STATS_PKT_TYPE_11BE ||
	    pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BA)
		return;

	if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE)
		pkt_type = DOT11_BE;

	switch (pkt_type) {
	case DOT11_A:
		mcs = (mcs >= MAX_MCS_11A) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_B:
		mcs = (mcs >= MAX_MCS_11B) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_N:
		mcs = (mcs >= MAX_MCS_11N) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AC:
		mcs = (mcs >= MAX_MCS_11AC) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AX:
		mcs = (mcs >= MAX_MCS_11AX) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_BE:
		mcs = (mcs >= MAX_MCS_11BE) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_BN:
		mcs = (mcs >= MAX_MCS_11BN) ? (MAX_MCS - 1) : mcs;
		break;
	default:
		break;
	}

	if (mcs != MAX_MCS)
		qos_tx->pkt_type[pkt_type].mcs_count[mcs]++;
}
EXPORT_SYMBOL(ath12k_sdwf_update_peer_mcs_stats);

#ifndef HW_TX_DELAY_MAX
#define HW_TX_DELAY_MAX				0x1000000
#endif
#ifndef ATH12K_MOV_AVG_PKT_WIN
#define ATH12K_MOV_AVG_PKT_WIN			10
#endif
#ifndef ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER
#define ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER	1000
#endif

void ath12k_qos_stats_update(struct ath12k_dp_peer *mld_peer,
			     u8 hw_link_id,
			     struct ath12k *ar,
			     struct sk_buff *skb,
			     struct hal_tx_status *ts,
			     struct ath12k_pdev_dp *dp_pdev,
			     ktime_t timestamp,
			     u32 hw_delay)
{
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_mld_qos_stats *mld_qos;
	struct ath12k_dp_qos_tx_stats *qos_tx;
	struct ath12k_dp_qos_delay_stats *qos_delay;
	void *telemetry_peer_ctx = NULL;
	u64 enqueue_timestamp, total_delay_pkts, tmp_div;
	u32 len, q_id, tid, nw_delay, sw_delay, delay_bound;
	u32 pkt_win, num_pkts, dropped_age_out = 0;
	u16 msduq_id;
	u8 link_id, qos_id;

	if (!mld_peer || !ts || !dp_pdev)
		return;

	if (!(skb->mark & QOS_VALID_TAG))
		return;

	msduq_id = u32_get_bits(skb->mark, SDWF_MSDUQ_ID);
	if (msduq_id >= QOS_MSDUQ_MAX || msduq_id < MSDUQ_MAX_DEF)
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);
	len = skb->len;

	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) & ATH12K_QOS_STATS_BASIC))
		return;

	if (mld_peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS)
		link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(mld_peer,
								     dp_pdev->hw_link_id);
	else
		link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(mld_peer,
								       hw_link_id);

	if (link_id >= ATH12K_NUM_MAX_LINKS) {
		ath12k_err(ar->ab, "link peer NA with link_id: %u\n", link_id);
		return;
	}

	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(mld_peer, hw_link_id);
	if (!link_peer) {
		ath12k_err(ar->ab, "link peer not present with link_id: %u\n",
			   link_id);
		return;
	}

	if (!mld_peer->qos)
		return;

	spin_lock_bh(&mld_peer->qos->lock);

	if (!mld_peer->mld_stats.mld_qos_stats) {
		spin_unlock_bh(&mld_peer->qos->lock);
		return;
	}

	mld_qos = &mld_peer->mld_stats.mld_qos_stats[tid * QOS_TID_MDSUQ_MAX + q_id];

	if (!link_peer->peer_stats.link_qos_stats) {
		spin_unlock_bh(&mld_peer->qos->lock);
		return;
	}

	qos_tx = &link_peer->peer_stats.link_qos_stats->tx[tid][q_id];

	switch (ts->status) {
	case HAL_WBM_TQM_REL_REASON_FRAME_ACKED:
		mld_qos->tx_success_pkts++;
		qos_tx->tx_success.num++;
		qos_tx->tx_success.bytes += len;
		if (ts->transmit_cnt > 1) {
			qos_tx->total_retries_count += (ts->transmit_cnt - 1);
			qos_tx->retry_count++;
			if (ts->transmit_cnt > 2)
				qos_tx->multiple_retry_count++;
		}
		ath12k_sdwf_update_peer_mcs_stats(qos_tx, ts);
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
		qos_tx->dropped.fw_rem.num++;
		qos_tx->dropped.fw_rem.bytes += len;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
		qos_tx->dropped.fw_rem_tx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX:
		qos_tx->dropped.fw_rem_notx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
		qos_tx->dropped.age_out++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1:
		qos_tx->dropped.fw_reason1++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2:
		qos_tx->dropped.fw_reason2++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3:
		qos_tx->dropped.fw_reason3++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE:
		qos_tx->dropped.fw_rem_queue_disable++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING:
		qos_tx->dropped.fw_rem_no_match++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
		qos_tx->dropped.drop_threshold++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL:
		qos_tx->dropped.drop_link_desc_na++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU:
		qos_tx->dropped.invalid_drop++;
		break;
	case HAL_WBM_TQM_REL_REASON_MULTICAST_DROP:
		qos_tx->dropped.mcast_vdev_drop++;
		break;
	default:
		qos_tx->dropped.invalid_rr++;
		break;
	}

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		qos_tx->tx_failed.num++;
		qos_tx->tx_failed.bytes += len;
		mld_qos->tx_failed_pkts++;
		if (ts->transmit_cnt > DP_RETRY_COUNT)
			qos_tx->failed_retry_count++;
	}

	mld_qos->queue_depth--;

	ath12k_telemetry_get_sla_num_pkts(&num_pkts);
	if (mld_peer->qos)
		telemetry_peer_ctx = mld_peer->qos->telemetry_peer_ctx;

	tmp_div = mld_qos->tx_success_pkts + mld_qos->tx_failed_pkts;
	if ((!(do_div(tmp_div, num_pkts))) && telemetry_peer_ctx) {
		if (mld_peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS) {
			dropped_age_out = qos_tx->dropped.age_out;
		} else {
			struct ath12k_dp_link_peer *tmp_peer = NULL;
			struct ath12k_dp_qos_tx_stats *tmp_qos_tx = NULL;
			u8 index;

			for (index = 0; index < ATH12K_DP_PEER_MAX_MLO_LINKS; index++) {
				struct ath12k_dp_link_peer_qos_stats *qos_link_stats;

				tmp_peer =
				ath12k_dp_link_peer_find_by_hw_link_id(mld_peer, index);
				if (!tmp_peer ||
				    !tmp_peer->peer_stats.link_qos_stats)
					continue;

				qos_link_stats = tmp_peer->peer_stats.link_qos_stats;
				tmp_qos_tx = &qos_link_stats->tx[tid][q_id];
				dropped_age_out += tmp_qos_tx->dropped.age_out;
				tmp_peer = NULL;
			}
		}
		ath12k_telemetry_update_msdu_drop(telemetry_peer_ctx, tid, msduq_id,
						  mld_qos->tx_success_pkts,
						  mld_qos->tx_failed_pkts,
						  dropped_age_out);
	}

	qos_delay = &link_peer->peer_stats.link_qos_stats->delay[tid][q_id];

	if (hw_delay > HW_TX_DELAY_MAX) {
		mld_qos->tx_invalid_delay_pkts++;
		qos_delay->invalid_delay_pkts++;
		goto out;
	}

	mld_qos->hwdelay_win_total += hw_delay;
	ath12k_dp_update_hist_stats(&qos_delay->delay_hist, hw_delay);

	nw_delay = u32_get_bits(skb->mark, QOS_NW_DELAY);
	mld_qos->nwdelay_win_total += nw_delay;

	enqueue_timestamp = ktime_to_us(timestamp);
	sw_delay = enqueue_timestamp ? (u32)enqueue_timestamp : 0;
	mld_qos->swdelay_win_total += sw_delay;

	ath12k_telemetry_get_sla_mov_avg_num_pkt(&pkt_win);
	if (!pkt_win)
		pkt_win = ATH12K_MOV_AVG_PKT_WIN;

	total_delay_pkts = mld_qos->tx_success_pkts +
			   mld_qos->tx_failed_pkts -
			   mld_qos->tx_invalid_delay_pkts;
	tmp_div = total_delay_pkts;

	if (telemetry_peer_ctx && !(do_div(tmp_div, pkt_win))) {
		u32 nwdelay_avg, hwdelay_avg, swdelay_avg;

		nwdelay_avg = div_u64(mld_qos->nwdelay_win_total, pkt_win);
		swdelay_avg = div_u64(mld_qos->swdelay_win_total, pkt_win);
		hwdelay_avg = div_u64(mld_qos->hwdelay_win_total, pkt_win);
		mld_qos->nwdelay_win_total = 0;
		mld_qos->swdelay_win_total = 0;
		mld_qos->hwdelay_win_total = 0;

		ath12k_telemetry_update_delay_mvng(telemetry_peer_ctx,
						   tid, msduq_id,
						   nwdelay_avg,
						   swdelay_avg,
						   hwdelay_avg);
	}

	if (!mld_peer->qos) {
		ath12k_err(ar->ab, "link peer's qos not present\n");
		goto out;
	}

	qos_id = mld_peer->qos->msduq_map[tid][q_id].qos_id;

	if (ath12k_get_qos_params_delay_bound(ar->ab, qos_id,
					      &delay_bound)) {
		if (hw_delay > (delay_bound *
				ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER))
			qos_delay->delay_failure++;
		else
			qos_delay->delay_success++;

		tmp_div = total_delay_pkts;
		if (!(do_div(tmp_div, num_pkts)) && telemetry_peer_ctx) {
			u64 delay_success = 0, delay_failure = 0;

			if (mld_peer->qos_stats_lvl ==
			    ATH12K_QOS_SINGLE_LINK_STATS) {
				delay_success = qos_delay->delay_success;
				delay_failure = qos_delay->delay_failure;
			} else {
				struct ath12k_dp_link_peer *tmp_peer = NULL;
				struct ath12k_dp_qos_delay_stats *tmp_qos_delay = NULL;
				struct ath12k_dp_link_peer_qos_stats *qos_link_stats;
				u8 idx;

				for (idx = 0; idx < ATH12K_DP_PEER_MAX_MLO_LINKS; idx++) {
					tmp_peer =
					ath12k_dp_link_peer_find_by_hw_link_id(mld_peer,
									       idx);
					if (!tmp_peer ||
					    !tmp_peer->peer_stats.link_qos_stats)
						continue;

					qos_link_stats =
						tmp_peer->peer_stats.link_qos_stats;
					tmp_qos_delay = &qos_link_stats->delay[tid][q_id];
					delay_success += tmp_qos_delay->delay_success;
					delay_failure += tmp_qos_delay->delay_failure;
					tmp_peer = NULL;
				}
			}
			ath12k_telemetry_update_delay(telemetry_peer_ctx,
						      tid, msduq_id,
						      delay_success,
						      delay_failure);
		}
	}

out:
	spin_unlock_bh(&mld_peer->qos->lock);
}
EXPORT_SYMBOL(ath12k_qos_stats_update);
