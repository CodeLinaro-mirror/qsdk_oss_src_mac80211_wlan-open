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

#define INVALID_LINK_ID                0xFF
#define DP_REO_RING_MAX			4

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

/* VIF STATS MACROS */
#define DP_STATS_INC(_handle, _field, _delta, _ring) \
	do { \
		if (likely(_handle)) \
			_handle->stats[_ring]._field += _delta; \
	} while (0)

#define DP_STATS_INC_PKT(_handle, _field, _count, _bytes, _ring) \
	do { \
		DP_STATS_INC(_handle, _field.packets, _count, _ring); \
		DP_STATS_INC(_handle, _field.bytes, _bytes, _ring); \
	} while (0)

/* DEVICE STATS MACROS */
#define DP_DEVICE_STATS_INC(_handle, _field, _delta) \
	do { \
		if (likely(_handle)) \
			_handle->device_stats._field += _delta; \
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
};

struct ath12k_dp_link_peer_stats {
	struct ath12k_htt_tx_stats *tx_stats;
	struct ath12k_rx_peer_stats *rx_stats;
	struct ath12k_dp_mon_peer_stats dp_mon_stats;
	u32 rx_retries;
};

struct ath12k_dp_peer_rx_stats {
	/* Basic */
	struct ath12k_dp_pkt_info recv_from_reo;
	struct ath12k_dp_pkt_info sent_to_stack;
	struct ath12k_dp_pkt_info sent_to_stack_fast;

	/* Debug and Advance */
	u32 mcast;
	u32 ucast;
	u32 non_amsdu;
	u32 msdu_part_of_amsdu;
	u32 mpdu_retry;
};

struct ath12k_dp_peer_tx_stats {
	/* Basic */
	struct ath12k_dp_pkt_info comp_pkt;
	struct ath12k_dp_pkt_info tx_success;
	u32 tx_failed;

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
	u32 mcast;
	u32 ucast;
	u32 bcast;
};

struct ath12k_dp_peer_stats {
	struct ath12k_dp_peer_tx_stats tx[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_peer_rx_stats rx[DP_REO_DST_RING_MAX];
	struct ath12k_wbm_rx_stats wbm_err;
};

struct ath12k_dp_tx_ingress_stats {
	/* Basic */
	struct ath12k_dp_pkt_info recv_from_stack;
	struct ath12k_dp_pkt_info enque_to_hw;
	struct ath12k_dp_pkt_info enque_to_hw_fast;

	/* Debug and Advance */
	u32 encap_type[HAL_TCL_ENCAP_TYPE_MAX];
	u32 encrypt_type[HAL_ENCRYPT_TYPE_MAX];
	u32 desc_type[DP_TCL_DESC_TYPE_MAX];
	u32 mcast;

	/* Drop */
	u32 drop[DP_TX_ENQ_ERR_MAX];
};

struct ath12k_dp_tx_vif_stats {
	struct ath12k_dp_tx_ingress_stats tx_i;
};

struct ath12k_dp_aggr_vif_stats {
	struct ath12k_dp_tx_vif_stats stats[DP_TCL_NUM_RING_MAX];
	struct ath12k_dp_peer_stats peer_stats;
};

struct ath12k_dp_aggr_pdev_stats {
	struct ath12k_dp_peer_stats peer_stats;
};

struct ath12k_stats_feat {
	bool feat_tx;
	bool feat_rx;
};

struct ath12k_telemetry_command {
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	enum ath12k_stats_object obj;
	struct ath12k_stats_feat feat;
	u64 request_id;
	u8 link_id;
	char intf_name[IFNAMSIZ];
	u8 mac[ETH_ALEN];
};

enum ath12k_wlan_telemetry_feat {
	TELEMETRY_FEAT_TX,
	TELEMETRY_FEAT_RX,

	TELEMETRY_FEAT_MAX,
};

/* Telemetry Peer Stats */
struct ath12k_telemetry_dp_peer {
	bool is_extended;
	struct ath12k_dp_peer_stats peer_stats;
};

/* Telemetry Vif Stats */
struct ath12k_telemetry_dp_vif {
	bool is_extended;
	struct ath12k_dp_aggr_vif_stats aggr_vif_stats;
};

/* Telemetry Radio Stats */
struct ath12k_telemetry_dp_radio {
	bool is_extended;
	struct ath12k_dp_aggr_pdev_stats aggr_pdev_stats;
};

/* Telemetry Device Stats */
struct ath12k_telemetry_dp_device {
	bool is_extended;
	u32 rxdma_error[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_error[HAL_REO_DEST_RING_ERROR_CODE_MAX];
	u32 tx_comp_err[DP_TX_COMP_ERR_MAX][DP_TCL_NUM_RING_MAX];
	u32 rx_wbm_sw_drop_reason[WBM_ERR_DROP_MAX];
	u32 reo_sw_drop_reason[DP_RX_ERR_MAX][DP_REO_RING_MAX];
};
#endif
