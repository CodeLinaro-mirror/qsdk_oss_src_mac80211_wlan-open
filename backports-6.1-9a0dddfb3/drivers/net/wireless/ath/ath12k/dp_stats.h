// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_STATS_H
#define ATH12K_DP_STATS_H

#include "hal.h"
#include "dp_cmn.h"
#include "cmn_defs.h"
#include "dp.h"
#include "ppe.h"
#include <linux/ip.h>
#include "qcn_extns/ath12k_cmn_extn.h"

struct ath12k_dp_link_peer;

#define INVALID_LINK_ID			0xFF
#define INVALID_SVC_ID			0xFF
#define DP_REO_RING_MAX			4

#define ATH12K_UHR_MCS_NUM	24	/* UHR (11BN) */
#define ATH12K_EHT_MCS_NUM	16
#define ATH12K_HE_MCS_NUM       14
#define ATH12K_VHT_MCS_NUM      10
#define ATH12K_BW_NUM           5
#define ATH12K_NSS_NUM          4
#define ATH12K_LEGACY_NUM       12
#define ATH12K_GI_NUM           4
#define ATH12K_HT_MCS_NUM       32

#define QOS_TID_MAX 8
#define QOS_TID_MDSUQ_MAX 2
#define QOS_TID_DEF_MSDUQ_MAX 2
#define VOW_DATA_TID_MAX 9

#define MAX_MCS_11B 7
#define MAX_MCS_11A 8
#define MAX_MCS_11N 8
#define MAX_MCS_11AC 12
#define MAX_MCS_11AX 14
#define MAX_MCS_11BE 16
#define MAX_MCS_11BN 24
#define MAX_MCS (24 + 1)

#define INVALID_RSSI	GENMASK(7, 0)
#define INVALID_RATE	GENMASK(7, 0)

#define IS_VALID_RSSI(rssi) ((rssi) <= 0)
#define IS_VALID_RATE(rate) ((rate) >= 0)

#define DP_AVG_JITTER_WEIGHT_DENOM 4
#define DP_AVG_DELAY_WEIGHT_DENOM 3

#define DP_TID_MAX	9
#define nla_total_size_nested(x) nla_total_size(x)
/**
 * enum ath12k_stats_object:	Defines the Stats specific to object
 * @STATS_OBJ_PEER:	Stats for station/peer associated to AP
 * @STATS_OBJ_VIF:	Stats for vif
 * @STATS_OBJ_RADIO:	Stats for particular Radio
 * @STATS_OBJ_DEVICE:	Stats for device
 * @STATS_OBJ_MAX:	Max supported objects
 */
enum ath12k_stats_object {
	STATS_OBJ_PEER,
	STATS_OBJ_VIF,
	STATS_OBJ_RADIO,
	STATS_OBJ_DEVICE,
	STATS_OBJ_MAX,
};

enum ath12k_dp_tx_enq_error {
	DP_TX_ENQ_SUCCESS = 0,
	DP_TX_ENQ_DROP_MISC,
	DP_TX_ENQ_DROP_VIF_TYPE_MON,
	DP_TX_ENQ_DROP_INV_LINK,
	DP_TX_ENQ_DROP_INV_ARVIF,
	DP_TX_ENQ_DROP_MGMT_FRAME,
	DP_TX_ENQ_DROP_MAX_TX_LIMIT,
	DP_TX_ENQ_DROP_INV_PDEV,
	DP_TX_ENQ_DROP_INV_PEER,
	DP_TX_ENQ_DROP_CRASH_FLUSH,
	DP_TX_ENQ_DROP_NON_DATA_FRAME,
	DP_TX_ENQ_DROP_SW_DESC_NA,
	DP_TX_ENQ_DROP_ENCAP_RAW,
	DP_TX_ENQ_DROP_ENCAP_802_3,
	DP_TX_ENQ_DROP_DMA_ERR,
	DP_TX_ENQ_DROP_EXT_DESC_NA,
	DP_TX_ENQ_DROP_HTT_MDATA_ERR,
	DP_TX_ENQ_DROP_TCL_DESC_NA,
	DP_TX_ENQ_TCL_DESC_RETRY,
	DP_TX_ENQ_DROP_INV_ARVIF_FAST,
	DP_TX_ENQ_DROP_INV_PDEV_FAST,
	DP_TX_ENQ_DROP_MAX_TX_LIMIT_FAST,
	DP_TX_ENQ_DROP_INV_ENCAP_FAST,
	DP_TX_ENQ_DROP_BRIDGE_VDEV,
	DP_TX_ENQ_DROP_ARSTA_NA,
	DP_TX_ENQ_DROP_CLONE,
	DP_TX_ENQ_DROP_MHDR_ERR,
	DP_TX_ENQ_DROP_FEAT_ERR,
	DP_TX_ENQ_DROP_QUEUE_STOP,
	DP_TX_ENQ_DROP_HW_ENQ_FAIL,
	DP_TX_ENQ_DROP_MCBC_ENCRY_FAIL,
	DP_TX_ENQ_DROP_MCBC_MSDU_INFO,
	DP_TX_ENQ_DROP_MCAST_NO_LINK,
	DP_TX_ENQ_DROP_FW_RECOVERY,
	DP_TX_ENQ_DROP_SKB_NO_LINEAR,
	DP_TX_ENQ_ERR_MAX,
};

enum ath12k_dp_tx_comp_error {
	DP_TX_COMP_ERR_MISC,
	DP_TX_COMP_ERR_INVALID_DESC,
	DP_TX_COMP_ERR_INVALID_PDEV,
	DP_TX_COMP_ERR_INVALID_VIF,
	DP_TX_COMP_ERR_INVALID_PEER,
	DP_TX_COMP_ERR_INVALID_LINK_PEER,
	DP_TX_COMP_ERR_DESC_INUSE,
	DP_TX_COMP_ERR_MAX,
};

enum ath12k_dp_rx_error {
	DP_RX_SUCCESS = 0,
	DP_RX_ERR_DROP_MISC,
	DP_RX_ERR_GET_SW_DESC_FROM_CK,
	DP_RX_ERR_GET_SW_DESC,
	DP_RX_ERR_DROP_REPLENISH,
	DP_RX_ERR_DROP_PARTNER_DP_NA,
	DP_RX_ERR_DROP_PDEV_NA,
	DP_RX_ERR_DROP_LAST_MSDU_NOT_FOUND,
	DP_RX_ERR_DROP_NWIFI_HDR_LEN_INVALID,
	DP_RX_ERR_DROP_INV_MSDU_LEN,
	DP_RX_ERR_DROP_MSDU_COALESCE_FAIL,
	DP_RX_ERR_DROP_H_MPDU,
	DP_RX_ERR_DROP_H_PPDU,
	DP_RX_ERR_DROP_INV_PEER,
	DP_RX_ERR_MAX,
};

enum ath12k_wbm_err_drop_reason {
	WBM_ERR_GET_SW_DESC_ERROR,
	WBM_ERR_GET_SW_DESC_FROM_CK_ERROR,
	WBM_ERR_INVALID_PEER_ID_ERROR,
	WBM_ERR_DESC_PARSE_ERROR,
	WBM_ERR_DROP_INVALID_COOKIE,
	WBM_ERR_DROP_INVALID_PUSH_REASON,
	WBM_ERR_DROP_INVALID_HW_ID,
	WBM_ERR_DROP_NULL_PARTNER_DP,
	WBM_ERR_DROP_PROCESS_NULL_PARTNER_DP,
	WBM_ERR_DROP_NULL_PDEV,
	WBM_ERR_DROP_NULL_AR,
	WBM_ERR_DROP_CAC_RUNNING,
	WBM_ERR_DROP_SCATTER_GATHER,
	WBM_ERR_DROP_INVALID_NWIFI_HDR_LEN,
	WBM_ERR_DROP_REO_GENERIC,
	WBM_ERR_DROP_RXDMA_GENERIC,
	WBM_ERR_DROP_MAX,
};

enum ath12k_dp_pkt_l3_proto_type {
	DP_PKT_TYPE_ARP = 0,
	DP_PKT_TYPE_IPV4,
	DP_PKT_TYPE_IPV6,
	DP_PKT_TYPE_EAPOL,
	DP_PKT_TYPE_EAPOL_M1,
	DP_PKT_TYPE_EAPOL_M2,
	DP_PKT_TYPE_EAPOL_M3,
	DP_PKT_TYPE_EAPOL_M4,
	DP_PKT_TYPE_EAPOL_G1,
	DP_PKT_TYPE_EAPOL_G2,
	DP_PKT_TYPE_L3_NS,
	DP_PKT_TYPE_L3_MAX,
};

enum ath12k_dp_pkt_l4_proto_type {
	DP_PKT_TYPE_TCP = 0,
	DP_PKT_TYPE_UDP,
	DP_PKT_TYPE_ICMP,
	DP_PKT_TYPE_ICMP_REQ,
	DP_PKT_TYPE_ICMP_RSP,
	DP_PKT_TYPE_IGMP,
	DP_PKT_TYPE_L4_NS,
	DP_PKT_TYPE_L4_MAX,
};

enum ath12k_dp_pkt_l5_proto_type {
	DP_PKT_TYPE_DHCP = 0,
	DP_PKT_TYPE_DHCP_DIS,
	DP_PKT_TYPE_DHCP_REQ,
	DP_PKT_TYPE_DHCP_OFR,
	DP_PKT_TYPE_DHCP_ACK,
	DP_PKT_TYPE_DHCP_NS,
	DP_PKT_TYPE_DNS_QUERY,
	DP_PKT_TYPE_DNS_RSP,
	DP_PKT_TYPE_L5_NS,
	DP_PKT_TYPE_L5_MAX,
};

enum ath12k_dp_proto_stats_rx_level {
	RX_RECV_FROM_HW = 0,
	RX_SENT_TO_STACK,
	RX_RECV_MAX,
};

enum ath12k_dp_proto_stats_tx_level {
	TX_RECV_FROM_STACK = 0,
	TX_RECV_FROM_STACK_FP,
	TX_ENQUEUE_HW,
	TX_ENQUEUE_HW_FP,
	TX_ENQUEUE_MAX,
};

enum ath12k_dp_proto_stats_tx_comp_level {
	TX_COMP = 0,
	TX_COMP_MAX,
};

enum ath12k_dp_debug_stats_mask {
	DP_ENABLE_STATS          = 0x00000001,
	DP_ENABLE_DEBUG_STATS    = 0x00000002,
	DP_ENABLE_EXT_TX_STATS   = 0x00000004,
	DP_ENABLE_EXT_RX_STATS   = 0x00000008,
	DP_ENABLE_TID_STATS      = 0x00000010,
	DP_ENABLE_ADVANCE_STATS  = 0x00000020,
	DP_ENABLE_PROTO_STATS    = 0x00000030,
	DP_ENABLE_VOW_STATS      = 0x00000040,
	DP_ENABLE_LATENCY_STATS    = 0x00000080,
	DP_ENABLE_QOS_STATS      = 0x80000000,
};

enum ath12k_counter_type {
	ATH12K_COUNTER_TYPE_BYTES,
	ATH12K_COUNTER_TYPE_PKTS,
	ATH12K_COUNTER_TYPE_MAX,
};

enum ath12k_stats_type {
	ATH12K_STATS_TYPE_SUCC,
	ATH12K_STATS_TYPE_FAIL,
	ATH12K_STATS_TYPE_RETRY,
	ATH12K_STATS_TYPE_AMPDU,
	ATH12K_STATS_TYPE_MAX,
};

struct dp_pkt_info {
	u64 num;
	u64 bytes;
};

struct pkt_type {
	u32 mcs_count[MAX_MCS];
};

struct ath12k_tx_pkt_info {
	u32 num_msdu;
	u32 num_mpdu;
	u32 mpdu_tried;
};

enum ath12k_mu_packet_type {
	TXRX_TYPE_MU_MIMO = 0,
	TXRX_TYPE_MU_OFDMA = 1,
	TXRX_TYPE_MU_MAX = 2,
};

#define WME_AC_MAX		4
#define MAX_RU_LOCATIONS        16
#define MAX_TRANSMIT_TYPES      9
#define MAX_PUNCTURED_MODE	5
#define RSSI_CHAIN_LEN		8

#define TID_TO_WME_AC(_tid) (      \
		(((_tid) == 0) || ((_tid) == 3)) ? WME_AC_BE : \
		(((_tid) == 1) || ((_tid) == 2)) ? WME_AC_BK : \
		(((_tid) == 4) || ((_tid) == 5)) ? WME_AC_VI : \
		WME_AC_VO)

/* VIF STATS MACROS */
#define DP_STATS_INC(_handle, _field, _delta, _ring) \
	do { \
		if (likely(_handle)) \
			_handle->stats[_ring]._field += _delta; \
	} while (0)

#define DP_RX_STATS_INC(_handle, _field, _delta, _ring) \
	do { \
		if (likely(_handle)) \
			_handle->rx_stats[_ring]._field += _delta; \
	} while (0)

#define DP_STATS_INCC(handle, field, delta, cond) \
	do { \
		if ((cond) && likely(handle)) \
			(handle->field) += (delta); \
	} while (0)

#define DP_STATS_INCR(handle, field, delta) \
	do { \
		if (likely(handle)) \
			(handle->field) += (delta); \
	} while (0)

#define DP_STATS_UPD(handle, field, delta) \
	do { \
		if (likely(handle)) \
			(handle->field) = (delta); \
	} while (0)

#define DP_STATS_INC_PKT(_handle, _field, _count, _bytes, _ring) \
	do { \
		DP_STATS_INC(_handle, _field.packets, _count, _ring); \
		DP_STATS_INC(_handle, _field.bytes, _bytes, _ring); \
	} while (0)

#define DP_RX_STATS_INC_PKT(_handle, _field, _count, _bytes, _ring) \
	do { \
		DP_RX_STATS_INC(_handle, _field.packets, _count, _ring); \
		DP_RX_STATS_INC(_handle, _field.bytes, _bytes, _ring); \
	} while (0)


/* DEVICE STATS MACROS */
#define DP_DEVICE_STATS_INC(_handle, _field, _delta) \
	do { \
		if (likely(_handle)) \
			_handle->device_stats._field += _delta; \
	} while (0)

#define DP_PEER_MISC_STATS_INC(_handle, _dir, _field, _link, _delta) \
	do { \
		if (likely(_handle)) \
			_handle->stats[_link]._dir._field += _delta; \
	} while (0)

/* PEER STATS MACROS */
#define DP_PEER_STATS_INC(_handle, _dir, _ring, _field, _link, _delta) \
	do { \
		if (likely(_handle)) \
			_handle->stats[_link]._dir[_ring]._field += _delta; \
	} while (0)

#define DP_PEER_STATS_PKT_LEN(_handle, _dir, _ring, _field, _link, _count, _bytes) \
	do { \
		DP_PEER_STATS_INC(_handle, _dir, _ring, _field.packets, _link, _count); \
		DP_PEER_STATS_INC(_handle, _dir, _ring, _field.bytes, _link, _bytes); \
	} while (0)

#define DP_PEER_STATS_COND_INC(_handle, _dir, _ring, _field, _link, _cond, _delta) \
	do { \
		if (_cond) \
			DP_PEER_STATS_INC(_handle, _dir, _ring, _field, _link, _delta); \
	} while (0)

#define DP_PEER_STATS_FIELD_INC(_handle, _field, _delta) \
	do { \
		if (likely(_handle)) \
		_handle->_field += _delta; \
	} while (0)

#define DP_PEER_LINK_STATS_CNT(_handle, _field, _delta, _link) \
	do { \
		DP_PEER_STATS_FIELD_INC(_handle, stats[_link]._field, _delta); \
	} while (0)

#define DP_PEER_PROTO_STATS_INC(_handle, _link, _dir, _ring, _lvl, _field, _delta) \
	do { \
		if (likely(_handle)) \
			_handle->stats[_link].proto->_dir[_ring][_lvl]._field += _delta; \
	} while (0)

/* PDEV TID STATS MACROS */
#define DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring, tid, array, reason) \
	((dp_pdev)->tid_stats.tid_tx[ring][tid].array[reason]++)

#define DP_PDEV_TID_RX_REASON_INC(dp_pdev, ring, tid, array, reason) \
	((dp_pdev)->tid_stats.tid_rx[ring][tid].array[reason]++)

#define DP_PDEV_TID_RX_INC(dp_pdev, ring, tid, counter) \
	(((dp_pdev)->tid_stats.tid_rx[ring][tid].counter)++)

struct ath12k_wbm_tx_stats {
	u64 wbm_tx_comp_stats[HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX];
};

struct ath12k_wbm_rx_stats {
	u32 rxdma_error[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_error[HAL_REO_DEST_RING_ERROR_CODE_MAX];
};

struct ath12k_dp_pkt_info {
	u32 packets;
	u64 bytes;
} __packed;

struct peer_airtime_consumption {
       u32 consumption;
       u16 avg_consumption_per_sec;
};

struct ath12k_mon_peer_airtime_stats {
       struct peer_airtime_consumption tx_airtime_consumption[WME_NUM_AC];
       struct peer_airtime_consumption rx_airtime_consumption[WME_NUM_AC];
       u64 last_update_time;
};

struct ath12k_peer_telemetry_stats {
       u32 tx_mpdu_retried;
       u32 tx_mpdu_total;
       u32 rx_mpdu_retried;
       u32 rx_mpdu_total;
       u16 tx_airtime_consumption[WME_NUM_AC]; // Energy Service FR
       u16 rx_airtime_consumption[WME_NUM_AC]; // Energy Service FR
       u8 snr;
};

struct ath12k_dp_mon_peer_stats {
       struct ath12k_mon_peer_airtime_stats mon_stats;
	u32 avg_snr;
	u8 rssi;
	u8 snr;
};

struct ath12k_htt_data_stats {
	u64 legacy[ATH12K_COUNTER_TYPE_MAX][ATH12K_LEGACY_NUM];
	u64 ht[ATH12K_COUNTER_TYPE_MAX][ATH12K_HT_MCS_NUM];
	u64 vht[ATH12K_COUNTER_TYPE_MAX][ATH12K_VHT_MCS_NUM];
	u64 he[ATH12K_COUNTER_TYPE_MAX][ATH12K_HE_MCS_NUM];
	u64 eht[ATH12K_COUNTER_TYPE_MAX][ATH12K_EHT_MCS_NUM];
	u64 uhr[ATH12K_COUNTER_TYPE_MAX][ATH12K_UHR_MCS_NUM]; /* UHR (11BN) */
	u64 bw[ATH12K_COUNTER_TYPE_MAX][ATH12K_BW_NUM];
	u64 nss[ATH12K_COUNTER_TYPE_MAX][ATH12K_NSS_NUM];
	u64 gi[ATH12K_COUNTER_TYPE_MAX][ATH12K_GI_NUM];
	u64 transmit_type[ATH12K_COUNTER_TYPE_MAX][HTT_PPDU_STATS_PPDU_TYPE_MAX];
	u64 ru_loc[ATH12K_COUNTER_TYPE_MAX][HAL_RX_RU_ALLOC_TYPE_MAX];
};

struct ath12k_htt_tx_stats {
	struct ath12k_htt_data_stats stats[ATH12K_STATS_TYPE_MAX];
	u8 rate_idx;
	u64 tx_duration;
	u64 ba_fails;
	u64 ack_fails;
	u16 ru_start;
	u16 ru_tones;
	u32 mu_group[MAX_MU_GROUP_ID];
	u8 ppdu_type;

	/* Ext HTT stats */
	/* MSDU Basic */
	struct dp_pkt_info tx_ucast_success;

	/* PPDU Basic */
	u32 tx_ppdus;

	/* MPDU Basic */
	u32 tx_mpdus_success;
	u32 tx_mpdus_tried;
	u32 retries_mpdu;

	/* Basic RSSI */
	int last_ack_rssi;
	int avg_ack_rssi;
	int rssi_chain[RSSI_CHAIN_LEN];

	/* Basic rate */
	u32 tx_rate;

	/* Advanced stats */
	u32 stbc;
	u32 ldpc;
	u32 wme_ac_type[WME_AC_MAX];
	u64 wme_ac_type_bytes[WME_AC_MAX];
	u32 excess_retries_per_ac[WME_AC_MAX];
	u32 ampdu_cnt;
	u32 non_ampdu_cnt;
	u32 num_ppdu_cookie_valid;
	u64 avg_tx_rate;
	u16 tx_ratecode;
	u32 last_tx_rate_mcs;
	u32 mcast_last_tx_rate;
	u32 mcast_last_tx_rate_mcs;
	u32 pream_punct_cnt;
	struct ath12k_tx_pkt_info ru_loc_mpdu_succ_tried[MAX_RU_LOCATIONS];
	struct ath12k_tx_pkt_info transmit_type_mpdu_succ_tried[MAX_TRANSMIT_TYPES];
	struct pkt_type su_be_ppdu_cnt;
	struct pkt_type mu_be_ppdu_cnt[TXRX_TYPE_MU_MAX];
	struct pkt_type su_bn_ppdu_cnt;
	struct pkt_type mu_bn_ppdu_cnt[TXRX_TYPE_MU_MAX];
	u32 punc_bw[MAX_PUNCTURED_MODE];
	u32 rts_success;
	u32 rts_failure;
	u32 bar_cnt;
	u32 ndpa_cnt;
	u64 tx_ppdu_duration;
	u8 tx_pwr;
	u32 tx_msdu_flush_rsn[HTT_FLUSH_MAX];

};

#define MAX_PUNCTURED_MODE 5

#define DP_AVG_RATE_FILTER_MIN 0
#define DP_AVG_RATE_FILTER_MAX 11000
#define DP_AVG_RATE_FILTER_DEFAULT 0

#define DP_ATH_RATE_EP_MULTIPLIER     BIT(7)
#define DP_ATH_EP_MUL(a, b)	      ((a) * (b))
#define DP_ATH_RATE_IN(c)  (DP_ATH_EP_MUL((c), DP_ATH_RATE_EP_MULTIPLIER))
#define DUMMY_MARKER	  0

/* Different Packet Types */
enum packet_std {
	DOT11_A = 0,
	DOT11_B = 1,
	DOT11_N = 2,
	DOT11_AC = 3,
	DOT11_AX = 4,
	DOT11_BA = 5,
	DOT11_BE = 6,
	DOT11_AZ = 7,
	DOT11_N_GF = 8,
	DOT11_BN = 9,	/* UHR */
	DOT11_MAX,
};

struct fw_mpdu_stats {
	u64 success_cnt;
	u64 failure_cnt;
};

struct msduq_tx_stats {
	u32 tx_failed;
	u32 retry_count;
	u32 total_retries_count;
	struct pkt_type pkt_type[DOT11_MAX];
};

enum hist_bucket_index {
	HIST_BUCKET_0,
	HIST_BUCKET_1,
	HIST_BUCKET_2,
	HIST_BUCKET_3,
	HIST_BUCKET_4,
	HIST_BUCKET_5,
	HIST_BUCKET_6,
	HIST_BUCKET_7,
	HIST_BUCKET_8,
	HIST_BUCKET_9,
	HIST_BUCKET_10,
	HIST_BUCKET_11,
	HIST_BUCKET_12,
	HIST_BUCKET_MAX,
};

enum hist_types {
	HIST_TYPE_SW_ENQEUE_DELAY,
	HIST_TYPE_HW_COMP_DELAY,
	HIST_TYPE_REAP_STACK,
	HIST_TYPE_HW_TX_COMP_DELAY,
	HIST_TYPE_DELAY_PERCENTILE,
	HIST_TYPE_HW_COMP_DELAY_TSF,
	HIST_TYPE_HW_COMP_DELAY_JITTER_TSF,
	HIST_TYPE_PDEV_SW_ENQEUE_DELAY,
	HIST_TYPE_PDEV_HW_TX_COMP_DELAY,
	HIST_TYPE_PDEV_SW_INTERFRAME_DELAY,
	HIST_TYPE_MAX,
};

struct hist_bucket {
	enum hist_types hist_type;
	u64 freq[HIST_BUCKET_MAX];
};

struct hist_stats {
	struct hist_bucket hist;
	u32 max;
	u32 min;
	u32 avg;
};

/**
 * enum ath12k_dp_tid_tx_sw_drop - TID TX sw drop reasons
 * @DP_TID_TX_DESC_ERR: TX descriptor error
 * @DP_TID_TX_DMA_MAP_ERR: DMA mapping error
 * @DP_TID_TX_HW_ENQUEUE: HW enqueue failure
 * @DP_TID_TX_SW_DROP_MAX: Maximum TX drop reasons
 */
enum ath12k_dp_tid_tx_sw_drop {
	DP_TID_TX_DESC_ERR,
	DP_TID_TX_DMA_MAP_ERR,
	DP_TID_TX_HW_ENQUEUE,
	DP_TID_TX_SW_DROP_MAX,
};

/** enum ath12k_dp_tid_rx_sw_drop
 * @DP_TID_RX_INVALID_PEER_VDEV
 * @DP_TID_RX_SW_DROP_MAX
 */
enum ath12k_dp_tid_rx_sw_drop {
	DP_TID_RX_INVALID_PEER_VDEV,
	DP_TID_RX_SW_DROP_MAX,
};

/**
 * struct ath12k_reo_error_stats
 * @reo_code_inv: Count of unknown REO error codes
 * @reo_code: counters for each REO error code
 */
struct ath12k_reo_error_stats {
	u32 reo_code_inv;
	u32 reo_code[HAL_REO_DEST_RING_ERROR_CODE_MAX];
};

/**
 * struct ath12k_rxdma_error_stats
 * @rxdma_code_inv: Count of unknown RXDMA error codes
 * @rxdma_code: counters for each RXDMA error code
 */
struct ath12k_rxdma_error_stats {
	u32 rxdma_code_inv;
	u32 rxdma_code[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
};

/**
 * struct ath12k_tid_rx_stats - Per-TID RX statistics
 * @msdu_cnt: Num of msdu received from HW
 * @mcast_msdu_cnt: Num Mcast Msdus received from HW
 * @bcast_msdu_cnt: Num Bcast Msdus received from HW
 * @delivered_to_stack: packets delivered to stack
 */
struct ath12k_tid_rx_stats {
	struct hist_stats to_stack_delay;
	struct hist_stats intfrm_delay;
	u32 msdu_cnt;
	u32 mcast_msdu_cnt;
	u32 bcast_msdu_cnt;
	u32 delivered_to_stack;
	u32 fail_cnt[DP_TID_RX_SW_DROP_MAX];
};

/**
 * struct ath12k_tid_tx_stats - Per-TID TX statistics
 * @swq_delay: Software Enqueue Delay counter
 * @hxtx_delay: Hardware TX Completion Delay counter
 * @intfrm_delay: Sofware Interframe Delay counter
 * @tqm_status_cnt: TQM release reason counters
 * @htt_status_cnt: HTT completion status counters
 * @swdrop_cnt: Software drop reason counters
 */
struct ath12k_tid_tx_stats {
	struct hist_stats swq_delay;
	struct hist_stats hwtx_delay;
	struct hist_stats intfrm_delay;
	u32 tqm_status_cnt[HAL_WBM_TQM_REL_REASON_MAX];
	u32 htt_status_cnt[HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX];
	u32 swdrop_cnt[DP_TID_TX_SW_DROP_MAX];
};

/**
 * struct ath12k_dp_pdev_tid_stats - VoW TID statistics
 * @tid_tx: Per-ring, per-TID TX statistics [ring][tid]
 * @tid_rx: Per-ring, per-TID RX statistics [ring][tid]
 * @tid_reo_err: Per-TID REO error statistics [tid]
 * @tid_rxdma_err: Per-TID RXDMA error statistics [tid]
 */
struct ath12k_dp_pdev_tid_stats {
	struct ath12k_tid_tx_stats tid_tx[DP_TCL_NUM_RING_MAX][VOW_DATA_TID_MAX];
	struct ath12k_tid_rx_stats tid_rx[DP_REO_DST_RING_MAX][VOW_DATA_TID_MAX];
	struct ath12k_reo_error_stats tid_reo_err[VOW_DATA_TID_MAX];
	struct ath12k_rxdma_error_stats tid_rxdma_err[VOW_DATA_TID_MAX];
};

/**
 * struct ath12k_dp_aggr_pdev_tid_stats
 * @tid_tx: per-TID TX statistics [tid]
 * @tid_rx: per-TID RX statistics [tid]
 * @tid_reo_err: Per-TID REO error statistics [tid]
 * @tid_rxdma_err: Per-TID RXDMA error statistics [tid]
 * Per-TID stats aggregated across rings
 */
struct ath12k_dp_aggr_pdev_tid_stats {
	struct ath12k_tid_tx_stats tid_tx[VOW_DATA_TID_MAX];
	struct ath12k_tid_rx_stats tid_rx[VOW_DATA_TID_MAX];
	struct ath12k_reo_error_stats tid_reo_err[VOW_DATA_TID_MAX];
	struct ath12k_rxdma_error_stats tid_rxdma_err[VOW_DATA_TID_MAX];
};

struct ath12k_dp_qos_tx_stats {
	struct dp_pkt_info tx_success;
	struct dp_pkt_info tx_failed;
	struct dp_pkt_info tx_ingress;
	struct {
		struct dp_pkt_info fw_rem;
		u32 fw_rem_notx;
		u32 fw_rem_tx;
		u32 age_out;
		u32 fw_reason1;
		u32 fw_reason2;
		u32 fw_reason3;
		u32 fw_rem_queue_disable;
		u32 fw_rem_no_match;
		u32 drop_threshold;
		u32 drop_link_desc_na;
		u32 invalid_drop;
		u32 mcast_vdev_drop;
		u32 invalid_rr;
	} dropped;
	struct fw_mpdu_stats svc_intval_stats;
	struct fw_mpdu_stats burst_size_stats;
	u32 queue_depth;
	u32 throughput;
	u32 ingress_rate;
	u32 min_throughput;
	u32 max_throughput;
	u32 avg_throughput;
	u32 per;
	u32 retries_pct;
	u32 total_retries_count;
	u32 retry_count;
	u32 multiple_retry_count;
	u32 failed_retry_count;
	u16 reinject_pkt;
	struct pkt_type pkt_type[DOT11_MAX];
};

struct ath12k_dp_qos_delay_stats {
	struct hist_stats delay_hist;
	u32 nwdelay_avg;
	u32 swdelay_avg;
	u32 hwdelay_avg;
	u32 invalid_delay_pkts;
	u32 delay_success;
	u32 delay_failure;
};

struct ath12k_dp_link_peer_qos_stats {
	struct ath12k_dp_qos_tx_stats tx[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	struct ath12k_dp_qos_delay_stats delay[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	u8 tid;
	u8 msduq;
};

struct ath12k_mld_qos_stats {
	u64 tx_success_pkts;
	u64 tx_failed_pkts;
	u64 tx_invalid_delay_pkts;
	u64 nwdelay_win_total;
	u64 swdelay_win_total;
	u64 hwdelay_win_total;
	struct fw_mpdu_stats svc_intval_stats;
	struct fw_mpdu_stats burst_size_stats;
	u32 queue_depth;
};

DECLARE_EWMA(avg_ack_rssi, 10, 8)

/**
 * struct ath12k_dp_peer_hw_tx_stats - Peer HW TX statistics
 *
 * @failed_bytes: Total bytes in failed TX frames
 * @drop_bytes:   Total bytes dropped during TX
 * @drop1_pkts:   Packets dropped due to AQM (Tx, No Tx, Aged, FW reason)
 * @drop2_pkts:   Feature specific packets drops (SMD/Reserved drops)
 */
struct ath12k_dp_peer_hw_tx_stats {
	u64 failed_bytes;
	u64 drop_bytes;
	u32 drop1_pkts;
	u32 drop2_pkts;
};

/**
 * struct ath12k_dp_peer_hw_rx_stats - Peer HW RX statistics
 *
 * @drop_ucast_bytes:  Total unicast bytes dropped
 * @drop_gcast_bytes:  Total multicast/broadcast bytes dropped
 * @drop2_pkts:        Packets dropped (SDWF - Backpressure/MSDU drop)
 * @drop_gcast_pkts:   Multicast/broadcast packets dropped
 */
struct ath12k_dp_peer_hw_rx_stats {
	u64 drop_ucast_bytes;
	u64 drop_gcast_bytes;
	u32 drop2_pkts;
	u32 drop_gcast_pkts;
};

/**
 * struct ath12k_dp_peer_hw_stats - Peer HW TX/RX statistics
 *
 * @hw_tx: Peer HW TX statistics
 * @hw_rx: Peer HW RX statistics
 */
struct ath12k_dp_peer_hw_stats {
	struct ath12k_dp_peer_hw_tx_stats hw_tx;
	struct ath12k_dp_peer_hw_rx_stats hw_rx;
};

/**
 * struct ath12k_dp_link_peer_hw_tx_stats - Per-link HW TX statistics
 * @sum_ack_rssi:       Signed sum of ACK RSSI values (dBm) across PPDUs
 * @sum_phy_rate:       Sum of PHY rates (in Kbps) across PPDUs
 * @acked_ppdu_count:   Number of PPDUs acknowledged by the peer
 */
struct ath12k_dp_link_peer_hw_tx_stats {
	s64 sum_ack_rssi;
	u64 sum_phy_rate;
	u32 acked_ppdu_count;
};

/**
 * struct ath12k_dp_link_peer_hw_rx_stats - Per-link HW RX statistics
 * @success_ucast_bytes:  Total unicast bytes successfully received
 * @success_gcast_bytes:  Total multicast/broadcast bytes successfully received
 * @failed_mpdu_bytes:    Total bytes in failed MPDUs
 * @sum_rssi:             Signed sum of RSSI values (dBm) across PPDUs
 * @sum_phy_rate:         Sum of PHY rates (in Kbps) across PPDUs
 * @drop1_ucast_pkts:     Unicast packets dropped (Backpressure/MSDU drop)
 * @success_gcast_pkts:   Multicast/broadcast packets successfully received
 * @failed_mpdu:          Number of failed MPDUs
 * @success_ppdu_count:   Number of PPDUs successfully received
 */
struct ath12k_dp_link_peer_hw_rx_stats {
	u64 success_ucast_bytes;
	u64 success_gcast_bytes;
	u64 failed_mpdu_bytes;
	s64 sum_rssi;
	u64 sum_phy_rate;
	u32 drop1_ucast_pkts;
	u32 success_gcast_pkts;
	u32 failed_mpdu;
	u32 success_ppdu_count;
};

/**
 * struct ath12k_dp_link_peer_hw_stats - Per-link HW TX/RX statistics container
 * @hw_tx: Per-link HW TX statistics
 * @hw_rx: Per-link HW RX statistics
 */
struct ath12k_dp_link_peer_hw_stats {
	struct ath12k_dp_link_peer_hw_tx_stats hw_link_tx;
	struct ath12k_dp_link_peer_hw_rx_stats hw_link_rx;
};

struct ath12k_dp_peer_delay_tx_stats {
	struct hist_stats tx_swq_delay;
	struct hist_stats hwtx_delay;
};

struct ath12k_dp_peer_delay_rx_stats {
	struct hist_stats to_stack_delay;
};

struct ath12k_dp_peer_delay_tid_stats {
	struct ath12k_dp_peer_delay_tx_stats tx_delay;
	struct ath12k_dp_peer_delay_rx_stats rx_delay;
};

struct ath12k_dp_peer_delay_stats {
	struct ath12k_dp_peer_delay_tid_stats delay_tid_stats[DP_TID_MAX]
							     [DP_REO_DST_RING_MAX];
};

struct ath12k_dp_peer_tid_jitter_stats {
	u32 tx_prev_delay;
	u32 tx_avg_jitter;
	u32 tx_avg_delay;
	u64 tx_avg_err;
	u64 tx_total_success;
	u64 tx_drop;
};

DECLARE_EWMA(avg_sojourn, 10, 8)

struct ath12k_dp_peer_tid_sojourn_stats {
	struct ewma_avg_sojourn avg_sojourn_msdu;
	u32 sum_sojourn_msdu;
	u32 num_msdus;
};

struct ath12k_dp_peer_sojourn_stats {
	struct ath12k_dp_peer_tid_sojourn_stats tid_stats[DP_TID_MAX]
							 [DP_REO_DST_RING_MAX];
};

struct ath12k_dp_peer_jitter_stats {
	struct ath12k_dp_peer_tid_jitter_stats tid_stats[DP_TID_MAX]
							[DP_REO_DST_RING_MAX];
};

struct ath12k_dp_mld_peer_stats {
	struct ath12k_dp_peer_hw_stats *hw_stats;
	struct ath12k_dp_peer_delay_stats *delay_stats;
	struct ath12k_dp_peer_jitter_stats *jitter_stats;
	struct ath12k_dp_peer_sojourn_stats *sojourn_stats;
	struct ath12k_mld_qos_stats *mld_qos_stats;
};

struct ath12k_dp_link_peer_stats {
	struct ath12k_htt_tx_stats *tx_stats;
	struct ath12k_rx_peer_stats *rx_stats;
	struct ath12k_dp_mon_peer_stats dp_mon_stats;
	struct ath12k_dp_link_peer_qos_stats *link_qos_stats;
	struct ath12k_dp_link_peer_hw_stats *hw_link_stats;
	unsigned long last_ack;
	unsigned long last_rx;
	u32 rx_dropped;
	u32 rx_retries;
	int last_ack_rssi;
	struct ewma_avg_ack_rssi avg_ack_rssi;
	/* BAR frame counter: always maintained, independent of extended RX stats mode */
	u32 num_bar;
};

struct ath12k_dp_peer_rx_stats {
	/* Basic */
	struct ath12k_dp_pkt_info recv_from_reo;
	struct ath12k_dp_pkt_info sent_to_stack;
	struct ath12k_dp_pkt_info sent_to_stack_fast;
	struct ath12k_dp_pkt_info sent_to_stack_ucast_fast;
	struct ath12k_dp_pkt_info sent_to_stack_mcast_fast;

	/* Debug and Advance */
	struct ath12k_dp_pkt_info mcast;
	struct ath12k_dp_pkt_info ucast;
	u32 non_amsdu;
	u32 msdu_part_of_amsdu;
	u32 mpdu_retry;
	struct ath12k_dp_pkt_info sg;
};

struct ath12k_dp_peer_tx_stats {
	/* Basic */
	struct ath12k_dp_pkt_info comp_pkt;
	struct ath12k_dp_pkt_info tx_success;
	u32 tx_failed;
	struct ath12k_dp_pkt_info tx_dropped;

	/* Debug and Advance */
	u32 wbm_rel_reason[HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX];
	u32 tqm_rel_reason[HAL_WBM_TQM_REL_REASON_MAX];
	u32 release_src_not_tqm;
	u32 retry_count;
	u32 total_msdu_retries;
	u32 multiple_retry_count;
	u32 ofdma;
	u32 amsdu_cnt;
	u32 non_amsdu_cnt;
	u32 inval_link_id_pkt_cnt;
	struct ath12k_dp_pkt_info mcast;
	struct ath12k_dp_pkt_info ucast;
	struct ath12k_dp_pkt_info bcast;
};

struct ath12k_dp_proto_stats {
	u64 l3[DP_PKT_TYPE_L3_MAX];
	u64 l4[DP_PKT_TYPE_L4_MAX];
	u64 l5[DP_PKT_TYPE_L5_MAX];
};

struct ath12k_dp_proto_stats_peer {
	struct ath12k_dp_proto_stats tx[DP_TCL_NUM_RING_MAX][TX_COMP_MAX];
	struct ath12k_dp_proto_stats rx[DP_REO_DST_RING_MAX][RX_RECV_MAX];
};

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
struct ath12k_dp_peer_mmesh_stats {
	u32 no_qos;
	u32 no_enc;
	u32 txinfo;
	u32 auto_rate;
	u32 direct;
	u32 tofw;
	u32 filter_drop;
	u32 rxhdr_updt;
	u32 rxkey_lookp_up_fail;
	u32 rxkey_lookp_up_succ;
	u32 rxhdr_alloc_fail;
};
#endif

struct ath12k_dp_peer_stats {
	struct ath12k_dp_peer_tx_stats tx[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_peer_rx_stats rx[DP_REO_DST_RING_MAX];
	struct ath12k_wbm_rx_stats wbm_err;
	struct ath12k_dp_proto_stats_peer *proto;
	struct ath12k_dp_peer_tid_agg_delay_stats *delay;
	struct ath12k_dp_peer_tid_agg_jitter_stats *jitter;
	struct ath12k_dp_peer_tid_agg_sojourn_stats *sojourn;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	struct ath12k_dp_peer_mmesh_stats mmesh_stat;
#endif
};

struct ath12k_dp_tx_ingress_stats {
	/* Basic */
	struct ath12k_dp_pkt_info recv_from_stack;
	struct ath12k_dp_pkt_info enque_to_hw;
	struct ath12k_dp_pkt_info enque_to_hw_fast;

	/* SG */
	struct ath12k_dp_pkt_info sg_pkt;
	u32 sg_dma_map_err;

	/* Debug and Advance */
	u32 encap_type[HAL_TCL_ENCAP_TYPE_MAX];
	u32 encrypt_type[HAL_ENCRYPT_TYPE_MAX];
	u32 desc_type[DP_TCL_DESC_TYPE_MAX];
	struct ath12k_dp_pkt_info mcast;

	/* Drop */
	u32 drop[DP_TX_ENQ_ERR_MAX];
};

struct ath12k_dp_proto_stats_vif {
	struct ath12k_dp_proto_stats tx[TX_ENQUEUE_MAX];
};

struct ath12k_dp_tx_vif_stats {
	struct ath12k_dp_tx_ingress_stats tx_i;
	struct ath12k_dp_proto_stats_vif *proto;
};

struct ath12k_dp_rx_vif_stats {
	struct ath12k_dp_pkt_info ppeds_rx;
};

struct ath12k_dp_aggr_vif_stats {
	struct ath12k_dp_tx_vif_stats stats[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_peer_stats peer_stats;
	struct ath12k_dp_link_peer_stats link_peer_stats;
	struct ath12k_dp_mld_peer_stats mld_stats;
};

struct ath12k_dp_aggr_pdev_stats {
	struct ath12k_dp_peer_stats peer_stats;
	struct ath12k_dp_link_peer_stats link_peer_stats;
	struct ath12k_dp_mld_peer_stats mld_stats;
};

struct ath12k_stats_feat {
	bool feat_tx;
	bool feat_rx;
	bool feat_sdwftx;
	bool feat_sdwfdelay;
	bool feat_proto;
	bool feat_tid;
	bool feat_delay;
	bool feat_jitter;
	bool feat_sojourn;
	bool feat_mon_stats;
};

struct ath12k_telemetry_command {
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	enum ath12k_stats_object obj;
	struct ath12k_stats_feat feat;
	u64 request_id;
	u8 link_id;
	u8 svc_id;
	char intf_name[IFNAMSIZ];
	u8 mac[ETH_ALEN];
};

enum ath12k_wlan_telemetry_feat {
	TELEMETRY_FEAT_TX,
	TELEMETRY_FEAT_RX,

	TELEMETRY_FEAT_MAX,
};

enum ath12k_peer_type {
	ATH12K_PEER_INVAL,
	ATH12K_LEGACY_PEER = 1,
	ATH12K_MLD_PEER,
	ATH12K_LINK_PEER,
};

/* Telemetry Peer Stats */
struct ath12k_telemetry_dp_peer {
	bool is_extended;
	int peer_type;
	struct ath12k_dp_peer_stats peer_stats;
	struct ath12k_dp_link_peer_stats link_peer_stats;
	struct ath12k_dp_mld_peer_stats mld_stats;
};

/* Telemetry Vif Stats */
struct ath12k_telemetry_dp_vif {
	bool is_extended;
	struct ath12k_dp_aggr_vif_stats aggr_vif_stats;
	struct ath12k_dp_rx_scan_radio_stats rx_scan_radio_stats;
};

/* Telemetry Radio Stats */
struct ath12k_telemetry_dp_radio {
	bool is_extended;
	struct ath12k_dp_aggr_pdev_stats aggr_pdev_stats;
};

/* Radio Control Path Stats - populated by ath12k_dp_get_radio_cp_stats() */
struct ath12k_radio_cp_stats {
	u32 tx_failed;
	u32 tx_rts_success;
	u32 tx_rts_fail;
	u32 rx_mgmt;
	u32 rx_ctrl;
	u32 rx_decrypt_err;
	u32 rx_mic_err;
	u32 rx_over_run;
	u32 rx_crc_err;
};

/* Telemetry Device Stats */
struct ath12k_telemetry_dp_device {
	bool is_extended;
	u32 rxdma_error[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_error[HAL_REO_DEST_RING_ERROR_CODE_MAX];
	u32 tx_comp_err[DP_TX_COMP_ERR_MAX][DP_TCL_NUM_RING_MAX];
	u32 rx_wbm_sw_drop_reason[WBM_ERR_DROP_MAX];
	u32 reo_sw_drop_reason[DP_RX_ERR_MAX][DP_REO_RING_MAX];
	struct ath12k_ppeds_stats ppeds_stats;
};

struct ath12k_rx_peer_rate_stats {
	u64 ht_mcs_count[HAL_RX_MAX_MCS_HT + 1];
	u64 vht_mcs_count[HAL_RX_MAX_MCS_VHT + 1];
	u64 he_mcs_count[HAL_RX_MAX_MCS_HE + 1];
	u64 be_mcs_count[HAL_RX_MAX_MCS_BE + 1];
	u64 bn_mcs_count[HAL_RX_MAX_MCS_BN + 1];	/* UHR (11BN) */
	u64 nss_count[HAL_RX_MAX_NSS];
	u64 bw_count[HAL_RX_BW_MAX];
	u64 gi_count[HAL_RX_GI_MAX];
	u64 legacy_count[HAL_RX_MAX_NUM_LEGACY_RATES];
	u64 rx_rate[HAL_RX_BW_MAX][HAL_RX_GI_MAX][HAL_RX_MAX_NSS][HAL_RX_MAX_MCS_HT + 1];
};

struct ath12k_rx_peer_user_stats {
	u64 ppdu_nss[HAL_RX_MAX_NSS];
	u32 mpdu_cnt_fcs_ok;
	u32 mpdu_cnt_fcs_err;
	struct pkt_type ppdu;
};

/**
 * struct ath12k_dp_peer_tid_agg_delay_stats - Container for aggregated all-rings stats
 * @tid_stats: Array of per-TID statistics aggregated across all rings
 *
 * This structure holds delay statistics for all TIDs, where each TID's statistics
 * represent the aggregation of data from all rings.
 */
struct ath12k_dp_peer_tid_agg_delay_stats {
	struct ath12k_dp_peer_delay_tid_stats tid_stats[DP_TID_MAX];
};

/**
 * struct ath12k_dp_peer_tid_agg_sojourn_stats - Container for all-rings sojourn stats
 * @tid_stats: Array of per-TID statistics aggregated across all rings
 *
 * This structure holds sojourn statistics for all TIDs, where each TID's statistics
 * represent the aggregation of data from all rings.
 */
struct ath12k_dp_peer_tid_agg_sojourn_stats {
	struct ath12k_dp_peer_tid_sojourn_stats tid_stats[DP_TID_MAX];
};

/**
 * struct ath12k_dp_peer_tid_agg_jitter_stats - Container for all-rings jitter stats
 * @tid_stats: Array of per-TID statistics aggregated across all rings
 *
 * This structure holds jitter statistics for all TIDs, where each TID's statistics
 * represent the aggregation of data from all rings.
 */
struct ath12k_dp_peer_tid_agg_jitter_stats {
	struct ath12k_dp_peer_tid_jitter_stats tid_stats[DP_TID_MAX];
};

#define MCS_VALID 1
#define MCS_INVALID 0
#define ATH12K_MAX_MCS_STRING_LEN 34

#define IEEE80211_FC0_TYPE_MASK		0x000c
#define IEEE80211_FC0_TYPE_DATA		0x0008
#define IEEE80211_FC0_SUBTYPE_MASK	0x00f0
#define IEEE80211_FC0_SUBTYPE_DATA	0x0000
#define IEEE80211_FC0_SUBTYPE_VHT_NDP_AN	0x0050
#define IEEE80211_FC0_SUBTYPE_BAR	0x0080

static const u8 max_mcs_by_preamble[HAL_RX_PREAMBLE_MAX] = {
	[HAL_RX_PREAMBLE_11A] = MAX_MCS_11A,
	[HAL_RX_PREAMBLE_11B] = MAX_MCS_11B,
	[HAL_RX_PREAMBLE_11N] = MAX_MCS_11N,
	[HAL_RX_PREAMBLE_11AC] = MAX_MCS_11AC,
	[HAL_RX_PREAMBLE_11AX] = MAX_MCS_11AX,
	[HAL_RX_PREAMBLE_11BE] = MAX_MCS_11BE,
	[HAL_RX_PREAMBLE_11BN] = MAX_MCS_11BN,	/* UHR */
};

struct ath12k_rx_peer_total_stats {
	u64 total_pkts;
	u64 total_bytes;
};

enum ath12k_cmn_bw_types {
	CMN_BW_20MHZ,
	CMN_BW_40MHZ,
	CMN_BW_80MHZ,
	CMN_BW_160MHZ,
	CMN_BW_240MHZ,
	CMN_BW_320MHZ,
	CMN_BW_CNT,
	CMN_BW_IDLE = 0xFF, /*default BW state */
};

#define PKT_BW_GAIN_20MHZ   0
#define PKT_BW_GAIN_40MHZ   3
#define PKT_BW_GAIN_80MHZ   6
#define PKT_BW_GAIN_160MHZ  9
#define PKT_BW_GAIN_320MHZ  12

DECLARE_EWMA(avg_snr, 0, 8)
DECLARE_EWMA(avg_snr_dp, 0, 8)
DECLARE_EWMA(avg_rssi, 10, 8)
DECLARE_EWMA(avg_rssi_dp, 10, 8)

/**
 * struct ath12k_dp_link_peer_rx_signal_stats - Per-peer signal statistics
 * @snr:              Current signal-to-noise ratio (SNR) in dB
 * @snr_avg:          Averaged SNR value (scaled/filtered)
 * @avg_snr:          EWMA (Exponentially Weighted Moving Average) tracker for SNR
 * @rssi_region_offset: Region-specific RSSI offset applied during conversion
 * @snr_dp:           Data path specific SNR value
 * @snr_dp_avg:       Averaged DP-specific SNR value
 * @avg_snr_dp:       EWMA tracker for DP-specific SNR
 *
 * @rssi:             Current received signal strength indicator (RSSI) in dBm
 * @rssi_avg:         Averaged RSSI value (scaled/filtered)
 * @avg_rssi:         EWMA tracker for RSSI
 * @rssi_dp:          Data path specific RSSI value
 * @rssi_dp_avg:      Averaged DP-specific RSSI value
 * @avg_rssi_dp:      EWMA tracker for DP-specific RSSI
 *
 * @channel_bw:       Represents the effective channel width (in MHz) associated with
 *                    the peer’s signal. Used to compute bandwidth-dependent offsets
 *                    during RSSI calculations.
 *
 * This structure holds both instantaneous and averaged signal quality
 * metrics (SNR and RSSI) for a given peer, including data path specific
 * values and EWMA smoothing helpers along with current bw info of signal.
 */
struct ath12k_dp_link_peer_rx_signal_stats {
	u8 snr;
	u16 snr_avg;
	struct ewma_avg_snr avg_snr;
	s8 rssi_region_offset;
	u8 snr_dp;
	u16 snr_dp_avg;
	struct ewma_avg_snr_dp avg_snr_dp;

	s8 rssi;
	s16 rssi_avg;
	struct ewma_avg_rssi avg_rssi;
	s8 rssi_dp;
	s16 rssi_dp_avg;
	struct ewma_avg_rssi_dp avg_rssi_dp;

	u8 channel_bw;
};

/**
 * struct ath12k_rx_peer_stats - Per-peer RX statistics
 *
 * @num_msdu: Total number of MSDUs received.
 * @num_mpdu_fcs_ok: Number of MPDUs received with FCS check passed.
 * @num_mpdu_fcs_err: Number of MPDUs received with FCS check failed.
 * @tcp_msdu_count: Number of MSDUs carrying TCP payload.
 * @udp_msdu_count: Number of MSDUs carrying UDP payload.
 * @other_msdu_count: Number of MSDUs carrying non-TCP/UDP payload.
 * @ampdu_msdu_count: Number of MSDUs received within A-MPDU aggregates.
 * @non_ampdu_msdu_count: Number of MSDUs received outside A-MPDU aggregates.
 * @stbc_count: Number of frames received using STBC (Space-Time Block Coding).
 * @beamformed_count: Number of frames received with beamforming enabled.
 * @coding_count: Array of counts per coding type (indexed by HAL_RX_SU_MU_CODING_MAX).
 * @tid_count: Array of MSDU counts per TID (Traffic Identifier),
 *             indexed by IEEE80211_NUM_TIDS + 1 (includes non-QoS).
 * @pream_cnt: Array of counts per preamble type (indexed by HAL_RX_PREAMBLE_MAX).
 * @reception_type: Array of counts per PPDU reception type
 *                  (indexed by HAL_RX_RECEPTION_TYPE_MAX).
 * @rx_duration: Total RX duration in microseconds.
 * @dcm_count: Number of frames received using Dual Carrier Modulation (DCM).
 * @ru_alloc_cnt: Array of counts per RU allocation type
 *                (indexed by HAL_RX_RU_ALLOC_TYPE_MAX).
 * @pkt_stats: Per-rate statistics based on packet counts.
 * @byte_stats: Per-rate statistics based on byte counts.
 *
 * SU + MU Basic Stats:
 * @num_msdu_bytes: Total MSDU bytes received.
 * @num_msdu_retry_count: Number of MSDU retries.
 * @num_mpdus: Total number of MPDUs received.
 * @num_mpdu_retry_count: Number of MPDU retries.
 * @num_ppdus: Total number of PPDUs received.
 *
 * Bitfield info:
 * @nss_info: Number of spatial streams (NSS).
 * @mcs_info: Modulation and Coding Scheme (MCS) index.
 * @bw_info: Bandwidth information (channel width).
 * @gi_info: Guard interval information.
 * @preamble_info: Preamble type information.
 *
 * Advance Stats:
 * @bar_count: Number of BlockAck Request (BAR) frames received.
 * @ndpa_count: Number of NDP Announcement (NDPA) frames received for MU-MIMO sounding.
 * @ppdu_reception: Number of PPDUs received per reception type
 *                  (indexed by HAL_RX_RECEPTION_TYPE_MAX).
 * @ppdu_nss: Number of PPDUs received per spatial stream (indexed by HAL_RX_MAX_NSS).
 * @proto_type: MSDU packet counts per 802.11 protocol type (indexed by DOT11_MAX).
 * @wme_ac_type: MSDU packets and bytes per WME Access Category
 *               (Voice, Video, Best Effort, Background).
 * @su_ppdu_count: PPDU SU packet counts per MCS per 802.11 protocol type
 * @punc_bw: Number of MSDUs received per punctured bandwidth mode
 *           (indexed by MAX_PUNCTURED_MODE).
 *
 * MU statistics:
 * @rx_mu: MU reception statistics per 802.11 protocol type and user type
 *         (indexed by DOT11_MAX and TXRX_TYPE_MU_MAX).
 *
 * Rate Stats :
 * @last_rx_rate: Last received data rate in kbps.
 * @rnd_avg_rx_rate: Rounded average RX data rate in kbps.
 * @avg_rx_rate: Filtered average RX data rate in kbps.
 * @rx_ratecode: Encoded RX ratecode.
 */
struct ath12k_rx_peer_stats {
	u64 num_msdu;
	u64 num_mpdu_fcs_ok;
	u64 num_mpdu_fcs_err;
	u64 tcp_msdu_count;
	u64 udp_msdu_count;
	u64 other_msdu_count;
	u64 ampdu_msdu_count;
	u64 non_ampdu_msdu_count;
	u64 stbc_count;
	u64 beamformed_count;
	u64 coding_count[HAL_RX_SU_MU_CODING_MAX];
	u64 tid_count[IEEE80211_NUM_TIDS + 1];
	u64 pream_cnt[HAL_RX_PREAMBLE_MAX];
	u64 reception_type[HAL_RX_RECEPTION_TYPE_MAX];
	u64 rx_duration;
	u64 dcm_count;
	u64 ru_alloc_cnt[HAL_RX_RU_ALLOC_TYPE_MAX];
	struct ath12k_rx_peer_rate_stats pkt_stats;
	struct ath12k_rx_peer_rate_stats byte_stats;
	/* SU + MU Basic Stats */
	u64 num_msdu_bytes;
	u32 num_msdu_retry_count;
	u64 num_mpdus;
	u32 num_mpdu_retry_count;
	u64 num_ppdus;

	u32 nss_info:4,
	    mcs_info:8,
	    bw_info:4,
	    gi_info:4,
	    preamble_info:4;

	/* Advance Stats */
	u32 num_bar;
	u32 num_ndpa;
	u64 ppdu_reception[HAL_RX_RECEPTION_TYPE_MAX];
	u64 ppdu_nss[HAL_RX_MAX_NSS];
	struct pkt_type proto_type[DOT11_MAX];
	struct ath12k_rx_peer_total_stats wme_ac_type[WME_NUM_AC];
	struct pkt_type su_ppdu_count[DOT11_MAX];
	u32 punc_bw[MAX_PUNCTURED_MODE];
	/* MU stats */
	struct ath12k_rx_peer_user_stats rx_mu[DOT11_MAX][TXRX_TYPE_MU_MAX];
	/* Rate stats */
	u32 last_rx_rate;
	u32 rnd_avg_rx_rate;
	u32 avg_rx_rate;
	u32 rx_ratecode;
	struct ath12k_dp_link_peer_rx_signal_stats signal_stats;
};

/* struct ath12k_dp_preserved_stats - Snapshot statistics for MLO datapath
 *
 * This structure is used to accumulate and preserve extended tx and rx and per-packet
 * statistics for peers that are deleted or unmapped in multi-link operation (MLO).
 * It is used for:
 *   - link_peer_delete_stats: holds stats for a link peer at the time of deletion,
 *     to be rolled up into the MLD peer.
 *   - link_peer_delete_stats: rolls up stats for link peers that have been
 *     unmapped or deleted, at the link VIF level.
 *   - link_vif_delete_stats: rolls up stats at the MLD VDEV level, including
 *     stats from deleted link VDEVs.
 *
 * This structure ensures that no statistics are lost during peer or VDEV teardown,
 * and that user-facing stats queries always reflect the latest and complete
 * aggregation, including contributions from deleted/unmapped peers.
 */
struct ath12k_dp_preserved_stats {
	struct ath12k_dp_peer_tx_stats per_pkt_tx[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_peer_rx_stats per_pkt_rx[DP_REO_DST_RING_MAX];
	struct ath12k_wbm_rx_stats wbm_err;
};

/* Stats aggregation functions */
void ath12k_dp_aggr_per_pkt_tx_stats(struct ath12k_dp_peer_tx_stats *dst,
				     struct ath12k_dp_peer_tx_stats *src);
void ath12k_dp_aggr_per_pkt_rx_stats(struct ath12k_dp_peer_rx_stats *dst,
				     struct ath12k_dp_peer_rx_stats *src);
void
ath12k_dp_update_tx_ext_htt_aggr_stats(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_htt_tx_stats *dst_peer_stats,
				       struct ath12k_htt_tx_stats *src_peer_stats);
void ath12k_dp_aggr_wbm_rx_stats(struct ath12k_wbm_rx_stats *dst,
				 struct ath12k_wbm_rx_stats *src);
void ath12k_dp_aggr_del_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
			      struct ath12k_dp_link_peer_stats *dst_link_peer_stats,
			      struct ath12k_dp_preserved_stats *src,
			      const char *stats_type);

/* Stats clear functions */
void ath12k_dp_clear_per_pkt_tx_stats(struct ath12k_dp_peer_stats *tx_peer_stats);
void ath12k_dp_clear_per_pkt_rx_stats(struct ath12k_dp_peer_stats *rx_peer_stats);
void ath12k_dp_clear_wbm_rx_stats(struct ath12k_wbm_rx_stats *wbm_stats);
void ath12k_dp_clear_preserved_stats(struct ath12k_dp_preserved_stats *stats);

s8 ath12k_dp_get_rssi_value(s8 snr,
			    struct ath12k_dp_link_peer_rx_signal_stats *stats,
			    struct wmi_rssi_dbm_conv_offsets *rssi_offsets,
			    bool ack_rssi);

int ath12k_dp_pdev_get_tid_stats(struct ath12k *ar,
				 struct ath12k_dp_aggr_pdev_tid_stats *tid_stats);

#define SKB_TRAC_ETH_TYPE_OFFSET			12
#define DP_ETH_TYPE_8021Q				0x8100
#define DP_ETH_TYPE_8021AD				0x88a8
#define SKB_TRAC_VLAN_ETH_TYPE_OFFSET			16
#define SKB_TRAC_DOUBLE_VLAN_ETH_TYPE_OFFSET		20
#define SKB_TRAC_IPV4_ETH_TYPE				0x0800
#define SKB_TRAC_IPV6_ETH_TYPE				0x86dd
#define SKB_TRAC_ARP_ETH_TYPE				0x0806
#define SKB_TRAC_EAPOL_ETH_TYPE				0x888E
#define SKB_TRAC_VLAN_IP_OFFSET				18
#define SKB_TRAC_DOUBLE_VLAN_IP_OFFSET			22
#define SKB_TRAC_IP_OFFSET				14
#define SKB_IPV4_PROTOCOL_FIELD_OFFSET			9
#define SKB_TRAC_TCP_TYPE				6
#define SKB_TRAC_UDP_TYPE				17
#define SKB_TRAC_ICMP_TYPE				1
#define SKB_TRAC_IGMP_TYPE				2
#define SKB_IPV4_HDR_SIZE_UNIT				4
#define SKB_TRAC_DHCP_SRV_PORT				67
#define SKB_TRAC_DHCP_CLI_PORT				68
#define SKB_PKT_DNS_DST_PORT_OFFSET			36
#define SKB_PKT_DNS_STANDARD_PORT			53
#define SKB_PKT_DNS_OVER_UDP_OPCODE_OFFSET		44
#define SKB_PKT_DNSOP_BITMAP				0xF800
#define SKB_PKT_DNSOP_STANDARD_QUERY			0x0000
#define SKB_PKT_DNSOP_STANDARD_RESPONSE			0x8000
#define SKB_PKT_DNS_SRC_PORT_OFFSET			34
#define SKB_PKT_ICMPV4OP_REQ				0x08
#define SKB_PKT_ICMPV4OP_REPLY				0x00
#define DHCP_OPTION53					0x35
#define DHCP_OPTION53_LENGTH				1
#define DHCP_OPTION53_OFFSET				0x11A
#define DHCP_OPTION53_LENGTH_OFFSET			0x11B
#define DHCP_OPTION53_STATUS_OFFSET			0x11C

#define DHCP_DISCOVER			(1)
#define DHCP_OFFER			(2)
#define DHCP_REQUEST			(3)
#define DHCP_ACK			(4)

static inline u16
ath12k_dp_get_ether_type(struct sk_buff *skb)
{
	u16 ether_type;

	if (skb->len < SKB_TRAC_ETH_TYPE_OFFSET + sizeof(u16))
		return 0;

	ether_type = get_unaligned((u16 *)(skb->data + SKB_TRAC_ETH_TYPE_OFFSET));

	if (unlikely(be16_to_cpu(ether_type) == DP_ETH_TYPE_8021Q))
		ether_type = get_unaligned((u16 *)(skb->data +
						   SKB_TRAC_VLAN_ETH_TYPE_OFFSET));
	else if (unlikely(be16_to_cpu(ether_type) == DP_ETH_TYPE_8021AD))
		ether_type = get_unaligned((u16 *)(skb->data +
					    SKB_TRAC_DOUBLE_VLAN_ETH_TYPE_OFFSET));

	return be16_to_cpu(ether_type);
}

static inline u8
ath12k_dp_get_l3_protocol_type(struct sk_buff *skb)
{
	u32 l3_type = 0;

	l3_type = ath12k_dp_get_ether_type(skb);

	switch (l3_type) {
	case SKB_TRAC_IPV4_ETH_TYPE:
		return DP_PKT_TYPE_IPV4;

	case SKB_TRAC_IPV6_ETH_TYPE:
		return DP_PKT_TYPE_IPV6;

	case SKB_TRAC_ARP_ETH_TYPE:
		return DP_PKT_TYPE_ARP;

	case SKB_TRAC_EAPOL_ETH_TYPE:
		return DP_PKT_TYPE_EAPOL;

	default:
		return DP_PKT_TYPE_L3_NS;
	}
}

static inline u8
ath12k_dp_get_ip_offset(struct sk_buff *skb)
{
	u16 ether_type;

	if (skb->len < SKB_TRAC_ETH_TYPE_OFFSET + sizeof(u16))
		return 0;

	ether_type = get_unaligned((u16 *)(skb->data + SKB_TRAC_ETH_TYPE_OFFSET));

	if (unlikely(ether_type == cpu_to_be16(DP_ETH_TYPE_8021Q)))
		return SKB_TRAC_VLAN_IP_OFFSET;
	else if (unlikely(ether_type == cpu_to_be16(DP_ETH_TYPE_8021AD)))
		return SKB_TRAC_DOUBLE_VLAN_IP_OFFSET;

	return SKB_TRAC_IP_OFFSET;
}

static inline u8
ath12k_dp_get_ipv4_proto(struct sk_buff *skb)
{
	u8 proto_type;
	u8 ipv4_offset;

	ipv4_offset = ath12k_dp_get_ip_offset(skb);

	if (skb->len < ipv4_offset + SKB_IPV4_PROTOCOL_FIELD_OFFSET + sizeof(u8))
		return 0;

	proto_type = get_unaligned((u8 *)(skb->data + ipv4_offset +
					  SKB_IPV4_PROTOCOL_FIELD_OFFSET));
	return proto_type;
}

static inline u8
ath12k_dp_get_l4_protocol_type(struct sk_buff *skb)
{
	u8 l4_type = 0;

	l4_type = ath12k_dp_get_ipv4_proto(skb);

	switch (l4_type) {
	case SKB_TRAC_TCP_TYPE:
		return DP_PKT_TYPE_TCP;

	case SKB_TRAC_UDP_TYPE:
		return DP_PKT_TYPE_UDP;

	case SKB_TRAC_ICMP_TYPE:
		return DP_PKT_TYPE_ICMP;

	case SKB_TRAC_IGMP_TYPE:
		return DP_PKT_TYPE_IGMP;

	default:
		return DP_PKT_TYPE_L4_NS;
	}
}

static inline bool
ath12k_dp_is_ipv4_dhcp_pkt(struct sk_buff *skb)
{
	u16 sport;
	u16 dport;
	u8 ipv4_offset;
	u8 ipv4_hdr_len;
	struct iphdr *iphdr;

	if (ath12k_dp_get_ether_type(skb) != SKB_TRAC_IPV4_ETH_TYPE)
		return false;

	ipv4_offset = ath12k_dp_get_ip_offset(skb);

	if (skb->len < ipv4_offset + sizeof(struct iphdr))
		return false;

	iphdr = (struct iphdr *)(skb->data + ipv4_offset);
	ipv4_hdr_len = iphdr->ihl * SKB_IPV4_HDR_SIZE_UNIT;

	if (skb->len < ipv4_offset + ipv4_hdr_len + 2 * sizeof(u16))
		return false;

	sport = get_unaligned((u16 *)(skb->data + ipv4_offset + ipv4_hdr_len));
	dport = get_unaligned((u16 *)(skb->data + ipv4_offset +
				      ipv4_hdr_len + sizeof(u16)));

	if ((sport == cpu_to_be16(SKB_TRAC_DHCP_SRV_PORT) &&
	     dport == cpu_to_be16(SKB_TRAC_DHCP_CLI_PORT)) ||
	    (sport == cpu_to_be16(SKB_TRAC_DHCP_CLI_PORT) &&
	     dport == cpu_to_be16(SKB_TRAC_DHCP_SRV_PORT)))
		return true;
	else
		return false;
}

static inline bool
ath12k_dp_is_dns_query(struct sk_buff *skb)
{
	u16 op_code;
	u16 tgt_port;

	if (skb->len < SKB_PKT_DNS_DST_PORT_OFFSET + sizeof(u16))
		return false;

	tgt_port = get_unaligned((u16 *)(skb->data + SKB_PKT_DNS_DST_PORT_OFFSET));
    /* Standard DNS query always happen on Dest Port 53. */
	if (tgt_port == cpu_to_be16(SKB_PKT_DNS_STANDARD_PORT)) {
		if (skb->len < SKB_PKT_DNS_OVER_UDP_OPCODE_OFFSET + sizeof(u16))
			return false;

		op_code = get_unaligned((u16 *)(skb->data +
					SKB_PKT_DNS_OVER_UDP_OPCODE_OFFSET));

	if ((be16_to_cpu(op_code) & SKB_PKT_DNSOP_BITMAP) ==
	    SKB_PKT_DNSOP_STANDARD_QUERY)
		return true;
	}
	return false;
}

static inline bool
ath12k_dp_is_dns_response(struct sk_buff *skb)
{
	u16 op_code;
	u16 src_port;

	if (skb->len < SKB_PKT_DNS_SRC_PORT_OFFSET + sizeof(u16))
		return false;

	src_port = get_unaligned((u16 *)(skb->data + SKB_PKT_DNS_SRC_PORT_OFFSET));
	/* Standard DNS response always comes on Src Port 53. */
	if (src_port == cpu_to_be16(SKB_PKT_DNS_STANDARD_PORT)) {
		if (skb->len < SKB_PKT_DNS_OVER_UDP_OPCODE_OFFSET + sizeof(u16))
			return false;

		op_code = get_unaligned((u16 *)(skb->data +
					SKB_PKT_DNS_OVER_UDP_OPCODE_OFFSET));

	if ((be16_to_cpu(op_code) & SKB_PKT_DNSOP_BITMAP) ==
	    SKB_PKT_DNSOP_STANDARD_RESPONSE)
		return true;
	}
	return false;
}

static inline u8
ath12k_dp_get_l5_protocol_type(struct sk_buff *skb)
{
	if (ath12k_dp_is_ipv4_dhcp_pkt(skb))
		return DP_PKT_TYPE_DHCP;
	else if (ath12k_dp_is_dns_query(skb))
		return DP_PKT_TYPE_DNS_QUERY;
	else if (ath12k_dp_is_dns_response(skb))
		return DP_PKT_TYPE_DNS_RSP;
	else
		return DP_PKT_TYPE_L5_NS;
}

static inline bool
ath12k_dp_is_icmpv4_req(struct sk_buff *skb)
{
	u8 op_code;
	u8 ipv4_offset;
	u8 ipv4_hdr_len;
	struct iphdr *iphdr;

	ipv4_offset = ath12k_dp_get_ip_offset(skb);

	if (skb->len < ipv4_offset + sizeof(struct iphdr))
		return false;

	iphdr = (struct iphdr *)(skb->data + ipv4_offset);
	ipv4_hdr_len = iphdr->ihl * SKB_IPV4_HDR_SIZE_UNIT;

	if (ipv4_hdr_len < sizeof(struct iphdr) ||
	    skb->len < ipv4_offset + ipv4_hdr_len + 1)
		return false;

	op_code = get_unaligned((u8 *)(skb->data + ipv4_offset + ipv4_hdr_len));

	if (op_code == SKB_PKT_ICMPV4OP_REQ)
		return true;

	return false;
}

static inline bool
ath12k_dp_is_icmpv4_rsp(struct sk_buff *skb)
{
	u8 op_code;
	u8 ipv4_offset;
	u8 ipv4_hdr_len;
	struct iphdr *iphdr;

	ipv4_offset = ath12k_dp_get_ip_offset(skb);

	if (skb->len < ipv4_offset + sizeof(struct iphdr))
		return false;

	iphdr = (struct iphdr *)(skb->data + ipv4_offset);
	ipv4_hdr_len = iphdr->ihl * SKB_IPV4_HDR_SIZE_UNIT;

	if (ipv4_hdr_len < sizeof(struct iphdr) ||
	    skb->len < ipv4_offset + ipv4_hdr_len + 1)
		return false;

	op_code = get_unaligned((u8 *)(skb->data + ipv4_offset + ipv4_hdr_len));

	if (op_code == SKB_PKT_ICMPV4OP_REPLY)
		return true;

	return false;
}

static inline u8
ath12k_dp_get_l4_protocol_subtype(struct sk_buff *skb)
{
	if (ath12k_dp_is_icmpv4_req(skb))
		return DP_PKT_TYPE_ICMP_REQ;
	else if (ath12k_dp_is_icmpv4_rsp(skb))
		return DP_PKT_TYPE_ICMP_RSP;
	else
		return DP_PKT_TYPE_L4_NS;
}

static inline enum ath12k_dp_pkt_l5_proto_type
ath12k_dp_get_dhcp_subtype(u8 *data)
{
	enum ath12k_dp_pkt_l5_proto_type subtype = DP_PKT_TYPE_DHCP_NS;

	if (data[DHCP_OPTION53_OFFSET] == DHCP_OPTION53 &&
	    data[DHCP_OPTION53_LENGTH_OFFSET] == DHCP_OPTION53_LENGTH) {
		switch (data[DHCP_OPTION53_STATUS_OFFSET]) {
		case DHCP_DISCOVER:
			subtype = DP_PKT_TYPE_DHCP_DIS;
			break;
		case DHCP_REQUEST:
			subtype = DP_PKT_TYPE_DHCP_REQ;
			break;
		case DHCP_OFFER:
			subtype = DP_PKT_TYPE_DHCP_OFR;
			break;
		case DHCP_ACK:
			subtype = DP_PKT_TYPE_DHCP_ACK;
			break;
		default:
			subtype = DP_PKT_TYPE_DHCP_NS;
			break;
		}
	}
	return subtype;
}

static inline enum ath12k_dp_pkt_l5_proto_type
ath12k_dp_get_l5_protocol_subtype(struct sk_buff *skb)
{
	return ath12k_dp_get_dhcp_subtype(skb->data);
}

static inline u8
ath12k_vow_tid_validate(u8 tid)
{
	if (tid >= VOW_DATA_TID_MAX)
		return VOW_DATA_TID_MAX - 1;

	return tid;
}

/**
 * ath12k_dp_update_hist_stats() - Update histogram stats
 * @hist_stats: Delay histogram
 * @value: Delay value
 *
 * Return: void
 */
void ath12k_dp_update_hist_stats(struct hist_stats *hist_stats, u32 value);

/**
 * ath12k_dp_hist_init() - Initialize the histogram object
 * @hist_stats: Hist stats object
 * @hist_type: Histogram type
 *
 * Return: Void
 */
void ath12k_dp_hist_init(struct hist_stats *hist_stats,
			 enum hist_types hist_type);

/**
 * ath12k_dp_tid_rx_stats_hist_init - Initialize delay histograms for per-TID RX stats
 * @dp_pdev: DP pdev handle
 *
 * Initializes delay histograms (reap-to-stack and interframe) for all
 * per ring, per tid during pdev allocation.
 */
void ath12k_dp_tid_rx_stats_hist_init(struct ath12k_pdev_dp *dp_pdev);

/**
 * ath12k_dp_tid_tx_stats_hist_init() - Initialize delay histograms for per-TID TX stats
 * @dp_pdev: DP pdev handle
 *
 * Initializes swq_delay, hwtx_delay, and intfrm_delay histogram objects for
 * every ring/TID slot in dp_pdev->tid_stats.  Must be called during pdev
 * allocation before any delay stats are updated.
 */
void ath12k_dp_tid_tx_stats_hist_init(struct ath12k_pdev_dp *dp_pdev);

void ath12k_dp_accumulate_hist_stats(struct hist_stats *src_hist_stats,
				     struct hist_stats *dst_hist_stats);

void ath12k_qos_tx_enqueue_peer_stats(struct ath12k_dp_peer *mld_peer,
				      u8 hw_link_id, u16 msduq_id,
				      unsigned int len);

void ath12k_sdwf_update_peer_mcs_stats(struct ath12k_dp_qos_tx_stats *qos_tx,
				       struct hal_tx_status *ts);

void ath12k_qos_stats_update(struct ath12k_dp_peer *mld_peer,
			     u8 hw_link_id,
			     struct ath12k *ar,
			     struct sk_buff *skb,
			     struct hal_tx_status *ts,
			     struct ath12k_pdev_dp *dp_pdev,
			     ktime_t timestamp,
			     u32 hw_delay);
#endif
