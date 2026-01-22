// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp_peer.h"
#include "dp_stats.h"
#include "debug.h"

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
 * ath12k_dp_aggr_htt_tx_stats - Aggregate HTT TX statistics
 * @dst: Destination HTT TX stats structure
 * @src: Source HTT TX stats structure to aggregate from
 *
 * Aggregates HTT TX statistics including rate information (legacy, HT, VHT,
 * HE, EHT), bandwidth, NSS, GI, transmit type, RU location, TX duration,
 * BA fails, ACK fails, and MU group information.
 */
void ath12k_dp_aggr_htt_tx_stats(struct ath12k_htt_tx_stats *dst,
				 const struct ath12k_htt_tx_stats *src)
{
	int i, j;

	for (i = 0; i < ATH12K_STATS_TYPE_MAX; i++) {
		for (j = 0; j < ATH12K_COUNTER_TYPE_MAX; j++) {
			for (int k = 0; k < ATH12K_LEGACY_NUM; k++)
				dst->stats[i].legacy[j][k] += src->stats[i].legacy[j][k];
			for (int k = 0; k < ATH12K_HT_MCS_NUM; k++)
				dst->stats[i].ht[j][k] += src->stats[i].ht[j][k];
			for (int k = 0; k < ATH12K_VHT_MCS_NUM; k++)
				dst->stats[i].vht[j][k] += src->stats[i].vht[j][k];
			for (int k = 0; k < ATH12K_HE_MCS_NUM; k++)
				dst->stats[i].he[j][k] += src->stats[i].he[j][k];
			for (int k = 0; k < ATH12K_EHT_MCS_NUM; k++)
				dst->stats[i].eht[j][k] += src->stats[i].eht[j][k];
			for (int k = 0; k < ATH12K_BW_NUM; k++)
				dst->stats[i].bw[j][k] += src->stats[i].bw[j][k];
			for (int k = 0; k < ATH12K_NSS_NUM; k++)
				dst->stats[i].nss[j][k] += src->stats[i].nss[j][k];
			for (int k = 0; k < ATH12K_GI_NUM; k++)
				dst->stats[i].gi[j][k] += src->stats[i].gi[j][k];
			for (int k = 0; k < HTT_PPDU_STATS_PPDU_TYPE_MAX; k++)
				dst->stats[i].transmit_type[j][k] +=
						src->stats[i].transmit_type[j][k];
			for (int k = 0; k < HAL_RX_RU_ALLOC_TYPE_MAX; k++)
				dst->stats[i].ru_loc[j][k] += src->stats[i].ru_loc[j][k];
		}
	}
	dst->tx_duration += src->tx_duration;
	dst->ba_fails += src->ba_fails;
	dst->ack_fails += src->ack_fails;
	for (i = 0; i < MAX_MU_GROUP_ID; i++)
		dst->mu_group[i] += src->mu_group[i];
}

/**
 * ath12k_dp_aggr_rx_peer_stats - Aggregate RX peer statistics
 * @dst: Destination RX peer stats structure
 * @src: Source RX peer stats structure to aggregate from
 *
 * Aggregates comprehensive RX peer statistics including MSDU/MPDU counts,
 * protocol-specific counts (TCP/UDP), AMPDU information, STBC, beamforming,
 * coding types, TID counts, preamble types, reception types, RX duration,
 * DCM, RU allocation, and detailed rate statistics (MCS, NSS, BW, GI, legacy).
 */
void ath12k_dp_aggr_rx_peer_stats(struct ath12k_rx_peer_stats *dst,
				  const struct ath12k_rx_peer_stats *src)
{
	int i, j, k, l;

	dst->num_msdu += src->num_msdu;
	dst->num_mpdu_fcs_ok += src->num_mpdu_fcs_ok;
	dst->num_mpdu_fcs_err += src->num_mpdu_fcs_err;
	dst->tcp_msdu_count += src->tcp_msdu_count;
	dst->udp_msdu_count += src->udp_msdu_count;
	dst->other_msdu_count += src->other_msdu_count;
	dst->ampdu_msdu_count += src->ampdu_msdu_count;
	dst->non_ampdu_msdu_count += src->non_ampdu_msdu_count;
	dst->stbc_count += src->stbc_count;
	dst->beamformed_count += src->beamformed_count;
	for (i = 0; i < HAL_RX_SU_MU_CODING_MAX; i++)
		dst->coding_count[i] += src->coding_count[i];
	for (i = 0; i <= IEEE80211_NUM_TIDS; i++)
		dst->tid_count[i] += src->tid_count[i];
	for (i = 0; i < HAL_RX_PREAMBLE_MAX; i++)
		dst->pream_cnt[i] += src->pream_cnt[i];
	for (i = 0; i < HAL_RX_RECEPTION_TYPE_MAX; i++)
		dst->reception_type[i] += src->reception_type[i];
	dst->rx_duration += src->rx_duration;
	dst->dcm_count += src->dcm_count;
	for (i = 0; i < HAL_RX_RU_ALLOC_TYPE_MAX; i++)
		dst->ru_alloc_cnt[i] += src->ru_alloc_cnt[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_HT; i++)
		dst->pkt_stats.ht_mcs_count[i] += src->pkt_stats.ht_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_VHT; i++)
		dst->pkt_stats.vht_mcs_count[i] += src->pkt_stats.vht_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_HE; i++)
		dst->pkt_stats.he_mcs_count[i] += src->pkt_stats.he_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_BE; i++)
		dst->pkt_stats.be_mcs_count[i] += src->pkt_stats.be_mcs_count[i];
	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		dst->pkt_stats.nss_count[i] += src->pkt_stats.nss_count[i];
	for (i = 0; i < HAL_RX_BW_MAX; i++)
		dst->pkt_stats.bw_count[i] += src->pkt_stats.bw_count[i];
	for (i = 0; i < HAL_RX_GI_MAX; i++)
		dst->pkt_stats.gi_count[i] += src->pkt_stats.gi_count[i];
	for (i = 0; i < HAL_RX_MAX_NUM_LEGACY_RATES; i++)
		dst->pkt_stats.legacy_count[i] += src->pkt_stats.legacy_count[i];
	for (i = 0; i < HAL_RX_BW_MAX; i++)
		for (j = 0; j < HAL_RX_GI_MAX; j++)
			for (k = 0; k < HAL_RX_MAX_NSS; k++)
				for (l = 0; l <= HAL_RX_MAX_MCS_HT; l++)
					dst->pkt_stats.rx_rate[i][j][k][l] +=
						src->pkt_stats.rx_rate[i][j][k][l];

	for (i = 0; i <= HAL_RX_MAX_MCS_HT; i++)
		dst->byte_stats.ht_mcs_count[i] += src->byte_stats.ht_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_VHT; i++)
		dst->byte_stats.vht_mcs_count[i] += src->byte_stats.vht_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_HE; i++)
		dst->byte_stats.he_mcs_count[i] += src->byte_stats.he_mcs_count[i];
	for (i = 0; i <= HAL_RX_MAX_MCS_BE; i++)
		dst->byte_stats.be_mcs_count[i] += src->byte_stats.be_mcs_count[i];
	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		dst->byte_stats.nss_count[i] += src->byte_stats.nss_count[i];
	for (i = 0; i < HAL_RX_BW_MAX; i++)
		dst->byte_stats.bw_count[i] += src->byte_stats.bw_count[i];
	for (i = 0; i < HAL_RX_GI_MAX; i++)
		dst->byte_stats.gi_count[i] += src->byte_stats.gi_count[i];
	for (i = 0; i < HAL_RX_MAX_NUM_LEGACY_RATES; i++)
		dst->byte_stats.legacy_count[i] += src->byte_stats.legacy_count[i];
	for (i = 0; i < HAL_RX_BW_MAX; i++)
		for (j = 0; j < HAL_RX_GI_MAX; j++)
			for (k = 0; k < HAL_RX_MAX_NSS; k++)
				for (l = 0; l <= HAL_RX_MAX_MCS_HT; l++)
					dst->byte_stats.rx_rate[i][j][k][l] +=
						src->byte_stats.rx_rate[i][j][k][l];

	dst->num_msdu_bytes += src->num_msdu_bytes;
	dst->num_msdu_retry_count += src->num_msdu_retry_count;
	dst->num_mpdus += src->num_mpdus;
	dst->num_mpdu_retry_count += src->num_mpdu_retry_count;
	dst->num_ppdus += src->num_ppdus;

	dst->num_bar += src->num_bar;
	dst->num_ndpa += src->num_ndpa;

	for (i = 0; i < HAL_RX_RECEPTION_TYPE_MAX; i++)
		dst->ppdu_reception[i] += src->ppdu_reception[i];

	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		dst->ppdu_nss[i] += src->ppdu_nss[i];

	for (i = 0; i < DOT11_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++) {
			dst->proto_type[i].mcs_count[j] +=
				src->proto_type[i].mcs_count[j];
		}
	}

	for (i = 0; i < WME_NUM_AC; i++) {
		dst->wme_ac_type[i].total_pkts += src->wme_ac_type[i].total_pkts;
		dst->wme_ac_type[i].total_bytes += src->wme_ac_type[i].total_bytes;
	}

	for (i = 0; i < DOT11_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++)
			dst->su_ppdu_count[i].mcs_count[j] +=
				src->su_ppdu_count[i].mcs_count[j];
	}

	for (i = 0; i < MAX_PUNCTURED_MODE; i++)
		dst->punc_bw[i] += src->punc_bw[i];

	for (i = 0; i < DOT11_MAX; i++) {
		for (j = 0; j < TXRX_TYPE_MU_MAX; j++) {
			dst->rx_mu[i][j].mpdu_cnt_fcs_ok +=
				src->rx_mu[i][j].mpdu_cnt_fcs_ok;
			dst->rx_mu[i][j].mpdu_cnt_fcs_err +=
				src->rx_mu[i][j].mpdu_cnt_fcs_err;

			for (k = 0; k < HAL_RX_MAX_NSS; k++) {
				dst->rx_mu[i][j].ppdu_nss[k] +=
					src->rx_mu[i][j].ppdu_nss[k];
			}

			for (k = 0; k < MAX_MCS; k++) {
				dst->rx_mu[i][j].ppdu.mcs_count[k] +=
					src->rx_mu[i][j].ppdu.mcs_count[k];
			}
		}
	}
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
 * ath12k_dp_aggr_deleted_stats() - Aggregate stats from a deleted entity
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

void ath12k_dp_aggr_deleted_stats(struct ath12k *ar,
				  struct ath12k_dp_peer_stats *dst_peer_stats,
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

	if (!ar || !dst_link_peer_stats) {
		ath12k_info(NULL, "Extended stats not present\n");
		return;
	}

	if (ath12k_extd_tx_stats_enabled(ar))
		ath12k_dp_aggr_htt_tx_stats(dst_link_peer_stats->tx_stats,
					    &src->tx_stats);
	if (ath12k_extd_rx_stats_enabled(ar))
		ath12k_dp_aggr_rx_peer_stats(dst_link_peer_stats->rx_stats,
					     &src->rx_stats);
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
 * @link_peer: Peer info (for bw_info)
 *
 * Returns: Calculated RSSI value (s8)
 */
s8 ath12k_dp_get_rssi_value(s8 snr,
			    struct ath12k_dp_link_peer_rx_signal_stats *stats,
			    struct wmi_rssi_dbm_conv_offsets *rssi_offsets,
			    struct ath12k_dp_link_peer *link_peer,
			    bool ack_rssi)
{
	s8 rssi_comb;
	u8 bw_info, bw_offset = 0;
	s8 rssi_val;

	if (!link_peer)
		return 0;


	if (!ack_rssi && link_peer->peer_stats.rx_stats) {
		bw_info = link_peer->peer_stats.rx_stats->bw_info;
		bw_offset = ath12k_dp_get_bw_offset(bw_info);
	}
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
