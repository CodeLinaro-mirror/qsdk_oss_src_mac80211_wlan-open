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

/* Rate values in kbps - for rate conversion */
#define ATH12K_RATE_1MBPS_KBPS             1000
#define ATH12K_RATE_2MBPS_KBPS             2000
#define ATH12K_RATE_5_5MBPS_KBPS           5500
#define ATH12K_RATE_6MBPS_KBPS             6000
#define ATH12K_RATE_9MBPS_KBPS             9000
#define ATH12K_RATE_11MBPS_KBPS            11000
#define ATH12K_RATE_12MBPS_KBPS            12000
#define ATH12K_RATE_18MBPS_KBPS            18000
#define ATH12K_RATE_24MBPS_KBPS            24000
#define ATH12K_RATE_36MBPS_KBPS            36000
#define ATH12K_RATE_48MBPS_KBPS            48000
#define ATH12K_RATE_54MBPS_KBPS            54000

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
