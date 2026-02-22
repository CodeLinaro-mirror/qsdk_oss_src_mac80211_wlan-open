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
