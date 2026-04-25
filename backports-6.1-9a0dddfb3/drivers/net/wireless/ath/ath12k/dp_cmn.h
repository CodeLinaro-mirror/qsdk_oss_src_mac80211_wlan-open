/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_CMN_H
#define ATH12K_DP_CMN_H

#include "cmn_defs.h"
#include "hw.h"

/* Max number of links for MLO connection */
#define ATH12K_DP_MAX_MLO_LINKS 4

struct ath12k_hw_group;
struct ath12k_sta;
struct ath12k;

struct dp_srng {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
};

struct ath12k_dp_hw_link {
	u8 device_id;
	u8 pdev_idx;
};
#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined(CPTCFG_ATH12K_MEM_PROFILE_512M)
#define DP_TX_COMP_RING_SIZE           16384
#define ATH12K_NUM_POOL_TX_DESC        16384
#define DP_REO2PPE_RING_SIZE	2048
#define DP_PPE2TCL_RING_SIZE	2048
#define DP_PPE_WBM2SW_RING_SIZE	8192
#define DP_TQM2PPE_RING_SIZE 8192
#define DP_RXDMA_BUF_RING_SIZE		8192
/* TODO: revisit this count during testing */
#define DP_RX_BUFFER_SIZE		1856
#elif defined(CONFIG_ATH12K_MEM_PROFILE_256M) || defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
#define DP_TX_COMP_RING_SIZE           16384
#define ATH12K_NUM_POOL_TX_DESC        16384
#define DP_REO2PPE_RING_SIZE    2048
#define DP_PPE2TCL_RING_SIZE    2048
#define DP_PPE_WBM2SW_RING_SIZE 8192
#define DP_TQM2PPE_RING_SIZE 8192
#define DP_RXDMA_BUF_RING_SIZE      4096
/* TODO: revisit this count during testing */
#define DP_RX_BUFFER_SIZE       1856
#else
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#define DP_TX_COMP_RING_SIZE           8192
#else
#define DP_TX_COMP_RING_SIZE           32768
#endif
#define ATH12K_NUM_POOL_TX_DESC                32768
#define DP_REO2PPE_RING_SIZE	16384
#define DP_PPE2TCL_RING_SIZE	8192
#define DP_PPE_WBM2SW_RING_SIZE	32768
#define DP_TQM2PPE_RING_SIZE 32768
#define DP_RXDMA_BUF_RING_SIZE		8192
/* TODO: revisit this count during testing */
#define DP_RX_BUFFER_SIZE		2048
#endif

#define ATH12K_PAGE_SIZE	PAGE_SIZE

/* Total 1024 entries in PPT, i.e 4K/4 considering 4K aligned
 * SPT pages which makes lower 12bits 0
 */
#define ATH12K_MAX_PPT_ENTRIES	1024

/* Total 512 entries in a SPT, i.e 4K Page/8 */
#define ATH12K_MAX_SPT_ENTRIES	512

#define ATH12K_TX_SPT_PAGES_PER_POOL \
	(ATH12K_NUM_POOL_TX_DESC / ATH12K_MAX_SPT_ENTRIES)
#define ATH12K_NUM_TX_SPT_PAGES	(ATH12K_TX_SPT_PAGES_PER_POOL * ATH12K_HW_MAX_QUEUES)

#define ATH12K_PPEDS_TX_SPT_PAGE_OFFSET 0
#define ATH12K_TX_SPT_PAGE_OFFSET ATH12K_NUM_PPEDS_TX_SPT_PAGES
#define ATH12K_RX_SPT_PAGE_OFFSET  \
	(ATH12K_NUM_PPEDS_TX_SPT_PAGES + ATH12K_NUM_TX_SPT_PAGES)
#define ATH12K_TX_SPT_OFFSET ATH12K_NUM_PPEDS_TX_SPT_PAGES
#define ATH12K_RX_SPT_OFFSET (ATH12K_TX_SPT_PAGES_PER_POOL * ATH12K_HW_MAX_QUEUES)

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#define ATH12K_NUM_PPEDS_TX_SPT_PAGES \
	(ath12k_ppeds_desc_params.num_ppeds_desc / ATH12K_MAX_SPT_ENTRIES)
#else
#define ATH12K_NUM_PPEDS_TX_SPT_PAGES 0
#endif

#define ATH12K_DP_RX_DESC_MAGIC	0xBABABABA

/* 4K aligned address have last 12 bits set to 0, this check is done
 * so that two spt pages address can be stored per 8bytes
 * of CMEM (PPT)
 */
#define ATH12K_SPT_4K_ALIGN_CHECK 0xFFF
#define ATH12K_SPT_4K_ALIGN_OFFSET 12
#define ATH12K_PPT_ADDR_OFFSET(ppt_index) (4 * (ppt_index))

/* To indicate HW of CMEM address, b0-31 are cmem base received via QMI */
#define ATH12K_CMEM_ADDR_MSB 0x10

/* Of 20 bits cookie, b0-b8 is to indicate SPT offset and b9-19 for PPT */
#define ATH12K_CC_SPT_MSB 8
#define ATH12K_CC_PPT_MSB 19
#define ATH12K_CC_PPT_SHIFT 9
#define ATH12K_DP_CC_COOKIE_SPT	GENMASK(8, 0)
#define ATH12K_DP_CC_COOKIE_PPT	GENMASK(19, 9)

#define MAX_DP_PEER_LIST_SIZE  16384
#define ATH12K_MAX_PEER_ID	2048
#define ATH12K_MAX_STA_ID	1536
#define DP_TCL_NUM_RING_MAX  MIN(NR_CPUS, 5)
#define ATH12K_DP_RX_REGULAR_RING_MAX MIN(NR_CPUS, 5)
#define DP_REO_DST_RING_MAX  (ATH12K_DP_RX_REGULAR_RING_MAX + 1)
#define ATH12K_DP_RX_ROAMING_RING1 ATH12K_DP_RX_REGULAR_RING_MAX
#define DP_TCL_DESC_TYPE_MAX 2
#define ATH12K_MAX_AHVIF_ID	255
#define ATH12K_INVALID_AHVIF_ID	0

struct ath12k_dp_hw {
	struct ath12k_dp_peer __rcu *dp_peer_list[MAX_DP_PEER_LIST_SIZE];
	DECLARE_BITMAP(free_peer_id_map, ATH12K_MAX_PEER_ID);
	DECLARE_BITMAP(free_sta_id_map, ATH12K_MAX_STA_ID);
	u16 last_peer_id;
	u16 last_sta_id;

	/* Lock for protection of dp_peer_list and peers */
	spinlock_t peer_lock;
	struct list_head peers;
};

struct ath12k_dp_hw_group {
	struct ath12k_dp_hw_link hw_links[ATH12K_GROUP_MAX_RADIO];
	struct ath12k_dp *dp[ATH12K_MAX_SOCS];
	struct dp_rx_fst *fst;
	u8 *tx_status_buf[ATH12K_HW_MAX_QUEUES];
	u8 *rx_status_buf[DP_REO_DST_RING_MAX];
	struct ath12k_spt_info *spt_info;
	u32 num_spt_pages;
	struct ath12k_tx_desc_info *txbaddr[ATH12K_NUM_TX_SPT_PAGES];
	struct list_head tx_desc_free_list[ATH12K_HW_MAX_QUEUES];
	struct list_head tx_spl_desc_free_list[ATH12K_HW_MAX_QUEUES];
	/* protects the free and used desc lists */
	spinlock_t tx_desc_lock[ATH12K_HW_MAX_QUEUES];
	bool tx_desc_initialized;
	/* protects shared TX SPT page and descriptor pool initialization across SOCs */
	struct mutex tx_init_lock;
	struct device *tx_spt_dev;

	/* Keep Last */
	u8 arch_data[] __aligned(sizeof(void *));
};

/* TODO: Move this to a seperate dp_stats file */
struct ath12k_per_peer_tx_stats {
	u32 succ_bytes;
	u32 retry_bytes;
	u32 failed_bytes;
	u32 duration;
	u16 succ_pkts;
	u16 retry_pkts;
	u16 failed_pkts;
	u16 ru_start;
	u16 ru_tones;
	u16 succ_mpdu_pkts;
	u16 retried_mpdu_pkts;
	u16 mpdu_tried;
	u8 ba_fails;
	u8 mu_grpid;
	u8 mu_pos;
	u8 rate;
	u8 bw;
	u8 flags;
	u8 mcs;
	u8 tid;
	u8 nss;
	bool is_ampdu;
	bool stbc;
	bool ldpc;
};

struct ath12k_dp_peer_create_params {
	struct ieee80211_sta *sta;
	bool is_mlo;
	bool is_vdev_peer;
	u8 hw_link_id;
	bool is_sta_bss_peer;
	u16 peer_id;
	u16 sta_id;
};

struct ath12k_dp_link_peer_rate_info {
	struct rate_info txrate;
	u64 rx_duration;
	u64 tx_duration;
	u8 rssi_comb;
	s8 signal_avg;
	u16 tx_retry_count;
	u16 tx_retry_failed;
	u32 rx_retries;
};

enum wme_ac {
	WME_AC_BE,
	WME_AC_BK,
	WME_AC_VI,
	WME_AC_VO,
	WME_NUM_AC
};

struct ath12k_dp_link_peer;
struct ath12k_dp_link_peer_rate_info;
void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp);
int ath12k_dp_cmn_device_init(struct ath12k_dp *dp);
void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag);
void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag);
int ath12k_dp_srng_alloc_aligned(struct ath12k_base *ab,
				 struct dp_srng *ring,
				 int num_entries,
				 int entry_sz,
				 bool cached);
void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar);
int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       struct ieee80211_sta *sta, u8 *addr, u8 link_id,
			       u32 hw_link_id, struct ieee80211_vif *vif,
			       u8 vp_type, int vp_num, bool mlo_bridge_peer);
void ath12k_dp_link_peer_unassign(struct ath12k *ar, u8 vdev_id, u8 *addr);
int
ath12k_dp_link_peer_batch_cleanup(struct ath12k *ar,
				  bool (*peer_match)(struct ath12k_dp_link_peer *,
						     void *),
				  void *context);
unsigned long ath12k_link_peer_last_active(struct ath12k_dp_link_peer *link_peer);
void ath12k_link_peer_get_sta_rate_info_stats(struct ath12k_dp_link_peer *link_peer,
					      struct ath12k_dp_link_peer_rate_info *rate_info);
bool ath12k_dp_link_peer_reset_rx_stats(struct ath12k_dp *dp, const u8 *addr);
bool ath12k_dp_link_peer_reset_tx_stats(struct ath12k_dp *dp, const u8 *addr);
int ath12k_dp_mon_init(struct ath12k_dp *dp);
void ath12k_dp_mon_deinit(struct ath12k_dp *dp);
void ath12k_dp_cp_link_peer_unassign(struct ath12k *ar, struct ath12k_link_vif *arvif,
				     struct ath12k_sta *ahsta, u8 link_id, u8 *addr);
struct ath12k_dp_peer *ath12k_dp_peer_find(struct ath12k_dp_hw *dp_hw, u8 *addr);
u16 ath12k_dp_peer_get_peer_id(struct ath12k_dp_hw *dp_hw, u8 *addr);
u16 ath12k_dp_peer_get_sta_id(struct ath12k_dp_hw *dp_hw, u8 *addr);
#endif
