// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_STATS_H
#define ATH12K_DP_STATS_H

#include "hal.h"
#include "dp_cmn.h"

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

struct ath12k_dp_pkt_info {
	u32 packets;
	u64 bytes;
} __packed;

struct ath12k_dp_tx_ingress_stats {
	/* Basic */
	struct ath12k_dp_pkt_info recv_from_stack;
	struct ath12k_dp_pkt_info enque_to_hw;
	struct ath12k_dp_pkt_info enque_to_hw_fast;

	/* Debug and Advance */
	u32 encap_type[HAL_TCL_ENCAP_TYPE_MAX];
	u32 encrypt_type[HAL_ENCRYPT_TYPE_MAX];
	u32 desc_type[DP_TCL_DESC_TYPE_MAX];

	/* Drop */
	u32 drop[DP_TX_ENQ_ERR_MAX];
};

struct ath12k_dp_tx_vif_stats {
	struct ath12k_dp_tx_ingress_stats tx_i;
};
#endif
