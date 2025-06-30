/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_MON_H
#define ATH12K_DP_MON_H

#include "core.h"
#include "dp_peer.h"
#include "debug.h"

#include "hal_mon_cmn.h"

#define ATH12K_MON_RX_DOT11_OFFSET	5
#define ATH12K_MON_RX_PKT_OFFSET	8
#define ATH12K_DP_WLAN_MAX_AC		4

#define DP_RXDMA_MON_STATUS_RING_SIZE	1024
#define DP_RXDMA_MONITOR_DESC_RING_SIZE	4096
#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined (CPTCFG_ATH12K_MEM_PROFILE_512M)
#define DP_RXDMA_MONITOR_BUF_RING_SIZE  256
#define DP_RXDMA_MONITOR_DST_RING_SIZE  512
#else
#define DP_RXDMA_MONITOR_BUF_RING_SIZE 4096
#define DP_RXDMA_MONITOR_DST_RING_SIZE 8192
#endif
#define DP_TX_MONITOR_BUF_RING_SIZE	4096
#define DP_TX_MONITOR_DEST_RING_SIZE	2048

#define DP_TX_MONITOR_BUF_SIZE		2048
#define DP_TX_MONITOR_BUF_SIZE_MIN	48
#define DP_TX_MONITOR_BUF_SIZE_MAX	8192

#define DP_RX_MON_BUFFER_SIZE		2048
#define RX_MON_STATUS_BASE_BUF_SIZE	2048
#define RX_MON_STATUS_BUF_ALIGN		128
#define RX_MON_STATUS_BUF_RESERVATION	128
#define RX_MON_STATUS_BUF_SIZE		(RX_MON_STATUS_BASE_BUF_SIZE - \
				 (RX_MON_STATUS_BUF_RESERVATION + \
				  RX_MON_STATUS_BUF_ALIGN + \
				  SKB_DATA_ALIGN(sizeof(struct skb_shared_info))))

#define DP_MON_RXDMA_BUF_COOKIE_BUF_ID		GENMASK(17, 0)
#define DP_MON_RXDMA_BUF_COOKIE_PDEV_ID 	GENMASK(19, 18)


struct ath12k_mon_data;

struct dp_rxdma_mon_ring {
	struct dp_srng refill_buf_ring;
	struct idr bufs_idr;
	/* Protects bufs_idr */
	spinlock_t idr_lock;
	int bufs_max;
};

struct ath12k_dp_arch_mon_ops {
	int (*rx_srng_setup)(struct ath12k_dp *dp);
	void (*rx_srng_cleanup)(struct ath12k_dp *dp);
	int (*rx_buf_setup)(struct ath12k_dp *dp);
	void (*rx_buf_free)(struct ath12k_dp *dp);
	int (*rx_htt_srng_setup)(struct ath12k_dp *dp);
	int (*mon_pdev_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_free)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
				      u32 mac_id);
	void (*mon_pdev_rx_srng_cleanup)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_htt_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
					  u32 mac_id);
	void (*mon_pdev_rx_attach)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_rx_mpdu_list_init)(struct ath12k_mon_data *pmon);
	int (*mon_rx_srng_process)(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				      struct napi_struct *napi, int *budget);
};

struct ath12k_dp_mon {
	struct dp_rxdma_mon_ring rxdma_mon_buf_ring;
	struct dp_rxdma_mon_ring tx_mon_buf_ring;
	struct dp_rxdma_mon_ring rx_mon_status_refill_ring[MAX_RXDMA_PER_PDEV];
	const struct ath12k_dp_arch_mon_ops *mon_ops;
};

enum dp_monitor_type {
	ATH12K_DP_MON_TYPE_QUAD_RING,
	ATH12K_DP_MON_TYPE_DUAL_RING
};

enum dp_mon_tx_ppdu_info_type {
	DP_MON_TX_PROT_PPDU_INFO,
	DP_MON_TX_DATA_PPDU_INFO
};

enum dp_mon_tx_medium_protection_type {
	DP_MON_TX_MEDIUM_NO_PROTECTION,
	DP_MON_TX_MEDIUM_RTS_LEGACY,
	DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW,
	DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW,
	DP_MON_TX_MEDIUM_CTS2SELF,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR
};

enum dp_mon_status_buf_state {
	DP_MON_STATUS_MATCH,
	DP_MON_STATUS_NO_DMA,
	DP_MON_STATUS_LAG,
	DP_MON_STATUS_LEAD,
	DP_MON_STATUS_REPLINISH,
};

struct dp_mon_qosframe_addr4 {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctrl;
	u8 addr4[ETH_ALEN];
	__le16 qos_ctrl;
} __packed;

struct dp_mon_frame_min_one {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
} __packed;

struct dp_mon_packet_info {
	u64 cookie;
	u16 dma_length;
	bool msdu_continuation;
	bool truncated;
};

struct dp_mon_mpdu {
	struct list_head list;
	struct sk_buff *head;
	struct sk_buff *tail;
	u32 err_bitmap;
	u8 decap_format;
};

struct dp_mon_tx_ppdu_info {
	bool is_used;
	struct hal_tx_mon_ppdu_info tx_info;
	struct list_head dp_tx_mon_mpdu_list;
	struct dp_mon_mpdu *tx_mon_mpdu;
};

struct ath12k_pdev_mon_stats {
	u32 status_ppdu_state;
	u32 status_ppdu_start;
	u32 status_ppdu_end;
	u32 status_ppdu_compl;
	u32 status_ppdu_start_mis;
	u32 status_ppdu_end_mis;
	u32 status_ppdu_done;
	u32 dest_ppdu_done;
	u32 dest_mpdu_done;
	u32 dest_mpdu_drop;
	u32 dup_mon_linkdesc_cnt;
	u32 dup_mon_buf_cnt;
	u32 dest_mon_stuck;
	u32 dest_mon_not_reaped;
};

#define DP_MON_MAX_STATUS_BUF 32

struct ath12k_mon_data {
	struct dp_link_desc_bank link_desc_banks[DP_LINK_DESC_BANKS_MAX];
	struct hal_rx_mon_ppdu_info mon_ppdu_info;

	u32 mon_ppdu_status;
	u32 mon_last_buf_cookie;
	u64 mon_last_linkdesc_paddr;
	u16 chan_noise_floor;
	u32 err_bitmap;
	u8 decap_format;

	struct ath12k_pdev_mon_stats rx_mon_stats;
	enum dp_mon_status_buf_state buf_state;
	/* lock for monitor data */
	spinlock_t mon_lock;
	struct sk_buff_head rx_status_q;
	struct dp_mon_mpdu *mon_mpdu;
	struct list_head dp_rx_mon_mpdu_list;
	struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info;
	struct dp_mon_tx_ppdu_info *tx_data_ppdu_info;
};

struct ath12k_pdev_mon_dp {
	struct dp_srng rxdma_mon_dst_ring[MAX_RXDMA_PER_PDEV];
	struct dp_srng tx_mon_dst_ring[MAX_RXDMA_PER_PDEV];

	struct ieee80211_rx_status rx_status;
	struct ath12k_mon_data mon_data;
};

static inline enum dp_monitor_type
ath12k_dp_get_mon_type(struct ath12k_dp *dp)
{
	return ((dp->hw_params->rxdma1_enable) ? ATH12K_DP_MON_TYPE_DUAL_RING :
				ATH12K_DP_MON_TYPE_QUAD_RING);
}

static inline
const struct ath12k_dp_arch_mon_ops *ath12k_dp_mon_ops_get(struct ath12k_dp *dp)
{
	if (dp && dp->dp_mon)
		return dp->dp_mon->mon_ops;

	return NULL;
}

enum hal_rx_mon_status
ath12k_dp_mon_rx_parse_mon_status(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *pmon,
				  struct sk_buff *skb,
				  struct napi_struct *napi);
int ath12k_dp_mon_buf_replenish(struct ath12k_base *ab,
				struct dp_rxdma_mon_ring *buf_ring,
				int req_entries);
struct sk_buff *ath12k_dp_mon_tx_alloc_skb(void);
enum hal_tx_mon_status
ath12k_dp_mon_tx_parse_mon_status(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *pmon,
				  struct sk_buff *skb,
				  struct napi_struct *napi,
				  u32 ppdu_id);
void ath12k_dp_mon_rx_process_ulofdma(struct hal_rx_mon_ppdu_info *ppdu_info);
int ath12k_dp_mon_rx_dual_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
				       struct napi_struct *napi, int *budget);
int ath12k_dp_get_peer_telemetry_stats(struct ath12k_base *ab,
                                      const u8 *peer_addr,
                                      struct ath12k_peer_telemetry_stats *stats);

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             int pdev_id);

void ath12k_dp_rxdma_mon_buf_ring_free(struct ath12k_base *ab,
				       struct dp_rxdma_mon_ring *rx_ring);
int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_buf_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id);
void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id);
void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon);
void ath12k_dp_rx_mon_dest_process(struct ath12k *ar, int mac_id,
				   u32 quota, struct napi_struct *napi);
int ath12k_dp_pkt_set_pktlen(struct sk_buff *skb, u32 len);

static inline
int ath12k_dp_mon_rx_alloc(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_setup) {
		ret = mon_ops->rx_srng_setup(dp);
		if (ret)
			return ret;
	}

	if (mon_ops && mon_ops->rx_buf_setup) {
		ret = mon_ops->rx_buf_setup(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_free(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_cleanup)
		mon_ops->rx_srng_cleanup(dp);

	if (mon_ops && mon_ops->rx_buf_free)
		mon_ops->rx_buf_free(dp);
}

static inline
int ath12k_dp_mon_rx_htt_setup(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_htt_srng_setup)
		ret = mon_ops->rx_htt_srng_setup(dp);

	return ret;

}

static inline
int ath12k_dp_mon_pdev_init(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp = dp_pdev->dp;
	int ret = 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_alloc)
		ret = mon_ops->mon_pdev_alloc(dp_pdev);

	return ret;
}

static inline
void ath12k_dp_mon_pdev_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp = dp_pdev->dp;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_free)
		mon_ops->mon_pdev_free(dp_pdev);
}

static inline
int ath12k_dp_mon_pdev_rx_alloc(struct ath12k_pdev_dp *dp_pdev,
				u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL\n");
		return -EINVAL;
	}

	if (mon_ops->mon_pdev_rx_srng_setup) {
		ret = mon_ops->mon_pdev_rx_srng_setup(dp_pdev,
						      mac_id);
		if (ret) {
			ath12k_warn(dp, "failed to setup HAL_RXDMA_MONITOR_DST\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static inline
int ath12k_dp_mon_pdev_rx_htt_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops->mon_pdev_rx_htt_srng_setup) {
		ret = mon_ops->mon_pdev_rx_htt_srng_setup(dp_pdev,
							  mac_id);
		if (ret) {
			ath12k_warn(dp, "htt setup failed for HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	if (mon_ops->mon_pdev_rx_attach)
		mon_ops->mon_pdev_rx_attach(dp_pdev);

	return 0;
}

static inline
void ath12k_dp_mon_pdev_rx_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_rx_srng_cleanup)
		mon_ops->mon_pdev_rx_srng_cleanup(dp_pdev);
}
#endif
