/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR 512

#define INITIATOR_WINDOW 0
#define RESPONSE_WINDOW  1

#define ATH12K_DP_MON_TX_BA_PER_STA_TID_INF_SZ     2
#define ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ       2
#define ATH12K_DP_MON_TX_BA_CTRL_SZ                2
#define ATH12K_DP_MON_TX_BA_BITMAP_BASE_SZ         4
#define ATH12K_DP_MON_TX_BA_BITMAP_SZ_MAX          7

#define ATH12K_BA_DURATION_US  32

#define ATH12K_DP_MON_TX_BA_BITMAP_BYTES(sz) \
	(ATH12K_DP_MON_TX_BA_BITMAP_BASE_SZ << (sz))

#define ATH12K_MU_BA_CTRL_MULTI_TID  0x0016

#define ATH12K_DP_MON_TX_MU_BA_INFO_SZ(bitmap_sz)  \
	((ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ) +\
	 (ATH12K_DP_MON_TX_BA_PER_STA_TID_INF_SZ) +\
	 (4 << (bitmap_sz)))

#define ATH12K_FREQ_2GHZ_MIN               2412
#define ATH12K_FREQ_2GHZ_MAX               2484
#define ATH12K_FREQ_5GHZ_MIN               5150
#define ATH12K_FREQ_5GHZ_MAX               5895
#define ATH12K_FREQ_6GHZ_MIN               5925
#define ATH12K_FREQ_6GHZ_MAX               7125

/* Frame timing constants */
#define ATH12K_ACK_TX_TIME_US       44
#define ATH12K_CTS_TX_TIME_US       44
#define ATH12K_SIFS_2GHZ_US         10
#define ATH12K_SIFS_5GHZ_US         16
#define ATH12K_DEFAULT_NOISE_FLOOR  -95

/* Rate Table */
#define ATH12K_11B_RATE_0MCS (11 * 2)
#define ATH12K_11B_RATE_1MCS (5.5 * 2)
#define ATH12K_11B_RATE_2MCS (2 * 2)
#define ATH12K_11B_RATE_3MCS (1 * 2)
#define ATH12K_11B_RATE_4MCS ATH12K_11B_RATE_0MCS
#define ATH12K_11B_RATE_5MCS ATH12K_11B_RATE_1MCS
#define ATH12K_11B_RATE_6MCS ATH12K_11B_RATE_2MCS

#define ATH12K_11A_RATE_0MCS (48 * 2)
#define ATH12K_11A_RATE_1MCS (24 * 2)
#define ATH12K_11A_RATE_2MCS (12 * 2)
#define ATH12K_11A_RATE_3MCS (6 * 2)
#define ATH12K_11A_RATE_4MCS (54 * 2)
#define ATH12K_11A_RATE_5MCS (36 * 2)
#define ATH12K_11A_RATE_6MCS (18 * 2)
#define ATH12K_11A_RATE_7MCS (9 * 2)

#define ATH12K_LEGACY_MCS0 0
#define ATH12K_LEGACY_MCS1 1
#define ATH12K_LEGACY_MCS2 2
#define ATH12K_LEGACY_MCS3 3
#define ATH12K_LEGACY_MCS4 4
#define ATH12K_LEGACY_MCS5 5
#define ATH12K_LEGACY_MCS6 6
#define ATH12K_LEGACY_MCS7 7

/* Legacy rate values in 500kbps units - for radiotap */
#define ATH12K_RATE_1MBPS_500KBPS	2
#define ATH12K_RATE_2MBPS_500KBPS	4
#define ATH12K_RATE_5_5MBPS_500KBPS	11
#define ATH12K_RATE_6MBPS_500KBPS	12
#define ATH12K_RATE_9MBPS_500KBPS	18
#define ATH12K_RATE_11MBPS_500KBPS	22
#define ATH12K_RATE_12MBPS_500KBPS	24
#define ATH12K_RATE_18MBPS_500KBPS	36
#define ATH12K_RATE_24MBPS_500KBPS	48
#define ATH12K_RATE_36MBPS_500KBPS	72
#define ATH12K_RATE_48MBPS_500KBPS	96
#define ATH12K_RATE_54MBPS_500KBPS	108

/* L-SIG field masks for extracting rate and length */
#define ATH12K_LSIG_RATE_MASK              GENMASK(3, 0)
#define ATH12K_LSIG_LENGTH_MASK            GENMASK(16, 5)
#define ATH12K_RADIOTAP_LSIG_LENGTH_SHIFT  4

/* Legacy rate values in 100kbps units - for radiotap */
#define ATH12K_LEGACY_RATE_1MBPS_100KBPS   10
#define ATH12K_LEGACY_RATE_2MBPS_100KBPS   20
#define ATH12K_LEGACY_RATE_5_5MBPS_100KBPS 55
#define ATH12K_LEGACY_RATE_6MBPS_100KBPS   60
#define ATH12K_LEGACY_RATE_9MBPS_100KBPS   90
#define ATH12K_LEGACY_RATE_11MBPS_100KBPS  110
#define ATH12K_LEGACY_RATE_12MBPS_100KBPS  120
#define ATH12K_LEGACY_RATE_18MBPS_100KBPS  180
#define ATH12K_LEGACY_RATE_24MBPS_100KBPS  240
#define ATH12K_LEGACY_RATE_36MBPS_100KBPS  360
#define ATH12K_LEGACY_RATE_48MBPS_100KBPS  480
#define ATH12K_LEGACY_RATE_54MBPS_100KBPS  540
#define ATH12K_LEGACY_RATE_DEFAULT_100KBPS 60

/* Rate status configuration */
#define ATH12K_RATE_STATUS_N_RATES         1
#define ATH12K_RATE_STATUS_TRY_COUNT       1
#define ATH12K_RATE_STATUS_DEFAULT_BW      0
#define ATH12K_RATE_STATUS_DEFAULT_NSS     1
#define ATH12K_RATE_STATUS_DEFAULT_IDX	  -1

struct ieee80211_frame_min {
	__le16 frame_control;
	__le16 duration;
	u8 ra[ETH_ALEN];
} __packed __aligned(2);

struct ieee80211_ctl_frm {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
} __packed;

struct ieee80211_mu_block_ack_hdr {
	__le16 frame_control;
	__le16 duration;
	u8 ra[ETH_ALEN];
	u8 ta[ETH_ALEN];
	__le16 ba_control;
} __packed;

enum txmon_generated_response {
	TXMON_GEN_RESP_SELFGEN_ACK = 0,
	TXMON_GEN_RESP_SELFGEN_CTS,
	TXMON_GEN_RESP_SELFGEN_BA,
	TXMON_GEN_RESP_SELFGEN_MBA,
	TXMON_GEN_RESP_SELFGEN_CBF,
	TXMON_GEN_RESP_SELFGEN_TRIG,
	TXMON_GEN_RESP_SELFGEN_NDP_LMR
};

int ath12k_dp_mon_tx_process_ring(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				  struct napi_struct *napi, int *budget);

/* TX Monitor PPDU Processing */
void ath12k_dp_tx_mon_process_ppdu(struct work_struct *work);
