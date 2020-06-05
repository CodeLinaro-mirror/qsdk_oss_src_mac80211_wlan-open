/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef ATH11K_NSS_H
#define ATH11K_NSS_H

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
#include <nss_api_if.h>
#include <nss_cmn.h>

#endif
struct ath11k;
struct ath11k_base;
struct ath11k_vif;
struct ath11k_peer;
struct ath11k_sta;
struct hal_rx_mon_ppdu_info;
struct hal_rx_user_status;

/* NSS DBG macro is not included as part of debug enum to avoid
 * frequent changes during upgrade*/
#define ATH11K_DBG_NSS	0x80000000

/* WIFILI Supported Target Types */
#define ATH11K_WIFILI_TARGET_TYPE_UNKNOWN   0xFF
#define ATH11K_WIFILI_TARGET_TYPE_QCA8074   20
#define ATH11K_WIFILI_TARGET_TYPE_QCA8074V2 24
#define ATH11K_WIFILI_TARGET_TYPE_QCA6018   25
#define ATH11K_WIFILI_TARGET_TYPE_QCN9074   26
#define ATH11K_WIFILI_TARGET_TYPE_QCA5018   29

/* Max limit for NSS Queue */
#define ATH11K_WIFIILI_MAX_TX_PROCESSQ 1024

/* Max TX Desc limit */
#define ATH11K_WIFILI_MAX_TX_DESC	65536

/* TX Desc related info */
/*TODO : Check this again during experiments for lowmem or
 changes for platforms based on num radios supported */
#define ATH11K_WIFILI_DBDC_NUM_TX_DESC (1024 * 8)
#define ATH11K_WIFILI_DBTC_NUM_TX_DESC (1024 * 8)

// TODO Revisit these page size calc
#define WIFILI_NSS_TX_DESC_SIZE 20*4
#define WIFILI_NSS_TX_EXT_DESC_SIZE 40*4
/* Number of desc per page(12bit) should be<4096, page limit per 1024 byte is 80*3=240 */
#define WIFILI_NSS_TX_DESC_PAGE_LIMIT 240
#define WIFILI_NSS_MAX_MEM_PAGE_SIZE (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE  (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_RX_DESC_POOL_WEIGHT 3

/* Status of the NSS messages sent from driver */
#define ATH11K_NSS_MSG_ACK	0
/* Timeout for waiting for response from NSS on TX msg */
#define ATH11K_NSS_MSG_TIMEOUT_MS 5000

/* Init Flags */
#define WIFILI_NSS_CCE_DISABLED 0x1
#define WIFILI_ADDTL_MEM_SEG_SET 0x000000002

/* ATH11K NSS PEER Info */
/* Host memory allocated for peer info storage in nss */
#define WIFILI_NSS_PEER_BYTE_SIZE NSS_WIFILI_PEER_SIZE

/* ATH11K NSS Stats */
#define ATH11K_NSS_STATS_ENABLE 1
#define ATH11K_NSS_STATS_DISABLE 0

/* TX Buf cfg range */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE 4

/* TODO : Analysis based on platform */
/* TX Limit till 64 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE0 8192
/* TX Limit till 128 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE1 8192
/* TX Limit till 256 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE2 8192
/* TX Limit > 256 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE3 8192

#define ATH11K_NSS_MAX_NUMBER_OF_PAGE 96

#define NSS_TX_TID_MAX 8

#define ATH11K_NSS_TXRX_NETDEV_STATS(txrx, vif, len, pkt_count) \
do {	\
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);	\
	struct pcpu_sw_netstats *tstats;	\
	\
	if (!wdev)	\
		break;	\
	tstats = this_cpu_ptr(netdev_tstats(wdev->netdev));	\
	u64_stats_update_begin(&tstats->syncp);	\
	tstats->txrx ## _packets += pkt_count;	\
	tstats->txrx ## _bytes += len;	\
	u64_stats_update_end(&tstats->syncp);	\
} while (0)

enum ath11k_nss_vdev_cmd {
	ATH11K_NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD,
	ATH11K_NSS_WIFI_VDEV_SECURITY_TYPE_CMD,
	ATH11K_NSS_WIFI_VDEV_ENCAP_TYPE_CMD,
	ATH11K_NSS_WIFI_VDEV_DECAP_TYPE_CMD,
};

enum ath11k_nss_opmode {
	ATH11K_NSS_OPMODE_UNKNOWN,
	ATH11K_NSS_OPMODE_AP,
	ATH11K_NSS_OPMODE_IBSS,
	ATH11K_NSS_OPMODE_STA,
	ATH11K_NSS_OPMODE_MONITOR,
};

struct peer_stats {
	u64 last_rx;
	u64 last_ack;
	u32 tx_packets;
	u32 tx_bytes;
	u32 tx_retries;
	u32 tx_failed;
	u32 rx_packets;
	u32 rx_bytes;
	u32 rx_dropped;
	u32 last_rxdrop;
	struct rate_info rxrate;
};

enum ath11k_nss_peer_sec_type {
	PEER_SEC_TYPE_NONE,
	PEER_SEC_TYPE_WEP128,
	PEER_SEC_TYPE_WEP104,
	PEER_SEC_TYPE_WEP40,
	PEER_SEC_TYPE_TKIP,
	PEER_SEC_TYPE_TKIP_NOMIC,
	PEER_SEC_TYPE_AES_CCMP,
	PEER_SEC_TYPE_WAPI,
	PEER_SEC_TYPE_AES_CCMP_256,
	PEER_SEC_TYPE_AES_GCMP,
	PEER_SEC_TYPE_AES_GCMP_256,
	PEER_SEC_TYPES_MAX
};

/* this holds the memory allocated for nss managed peer info */
struct ath11k_nss_peer {
	uint32_t *vaddr;
	dma_addr_t paddr;
	struct peer_stats *nss_stats;
	struct completion complete;
};

/* Structure to hold the vif related info for nss offload support */
struct arvif_nss {
	/* dynamic ifnum allocated by nss driver for vif */
	int if_num;
	/* Used for completion status for vdev config nss messages */
	struct completion complete;
	/* Keep the copy of encap type for nss */
	int encap;
	/* Keep the copy of decap type for nss */
	int decap;
	bool created;
};

/* Structure to hold the pdev/radio related info for nss offload support */
struct ath11k_nss {
	/* dynamic ifnum allocated by nss driver for pdev */
	int if_num;
	/* Radio/pdev Context obtained on pdev register */
	void* ctx;
};

/* Structure to hold the soc related info for nss offload support */
struct ath11k_soc_nss {
	/* turn on/off nss offload support in ath11k */
	bool enabled;
	/* turn on/off nss stats support in ath11k */
	bool stats_enabled;
	/* soc nss ctx */
	void* ctx;
	/* if_num to be used for soc related nss messages */
	int if_num;
	/* Completion to nss message response */
	struct completion complete;
	/* Response to nss messages are stored here on msg callback
	* used only in contention free messages during init */
	int response;
	/* Below is used for identifying allocated tx descriptors */
	dma_addr_t tx_desc_paddr[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
	uint32_t * tx_desc_vaddr[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
	uint32_t tx_desc_size[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
};

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
int ath11k_nss_tx(struct ath11k_vif *arvif, struct sk_buff *skb);
int ath11k_nss_vdev_set_cmd(struct ath11k_vif *arvif, enum ath11k_nss_vdev_cmd cmd,
			    int val);
int ath11k_nss_vdev_create(struct ath11k_vif *arvif);
void ath11k_nss_vdev_delete(struct ath11k_vif *arvif);
int ath11k_nss_vdev_up(struct ath11k_vif *arvif);
int ath11k_nss_vdev_down(struct ath11k_vif *arvif);
int ath11k_nss_peer_delete(struct ath11k_base *ab, u8 *addr);
int ath11k_nss_set_peer_authorize(struct ath11k *ar, u16 peer_id);
int ath11k_nss_peer_create(struct ath11k_base *ab, struct ath11k_peer *peer);
void ath11k_nss_peer_stats_enable(struct ath11k *ar);
void ath11k_nss_peer_stats_disable(struct ath11k *ar);
int ath11k_nss_set_peer_sec_type(struct ath11k *ar, struct ath11k_peer *peer,
				 struct ieee80211_key_conf *key_conf);
void ath11k_nss_update_sta_stats(struct station_info *sinfo,
				 struct ieee80211_sta *sta,
				 struct ath11k_sta *arsta);
void ath11k_nss_update_sta_rxrate(struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath11k_peer *peer,
				  struct hal_rx_user_status *user_stats);
int ath11k_nss_setup(struct ath11k_base *ab);
int ath11k_nss_teardown(struct ath11k_base *ab);
void ath11k_nss_ext_rx_stats(struct ath11k_base *ab, struct htt_rx_ring_tlv_filter *tlv_filter);
#else
static inline int ath11k_nss_tx(struct ath11k_vif *arvif, struct sk_buff *skb)
{
	return 0;
}

static inline int ath11k_nss_vdev_set_cmd(struct ath11k_vif *arvif, enum ath11k_nss_vdev_cmd cmd,
					  int val)
{
	return 0;
}

static inline int ath11k_nss_vdev_create(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_set_peer_authorize(struct ath11k *ar, u16 peer_id)
{
	return 0;
}

static inline void ath11k_nss_vdev_delete(struct ath11k_vif *arvif)
{
}

static inline void ath11k_nss_update_sta_stats(struct station_info *sinfo,
					       struct ieee80211_sta *sta,
					       struct ath11k_sta *arsta)
{
	return;
}

static inline void ath11k_nss_update_sta_rxrate(struct hal_rx_mon_ppdu_info *ppdu_info,
						struct ath11k_peer *peer,
						struct hal_rx_user_status *user_stats)
{
	return;
}

static inline int ath11k_nss_vdev_up(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_vdev_down(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_peer_delete(struct ath11k_base *ab, u8 *addr)
{
	return 0;
}

static inline int ath11k_nss_peer_create(struct ath11k_base *ab, struct ath11k_peer *peer)
{
	return 0;
}

static inline void ath11k_nss_peer_stats_enable(struct ath11k *ar)
{
	return;
}

static inline void ath11k_nss_peer_stats_disable(struct ath11k *ar)
{
	return;
}

static inline int ath11k_nss_set_peer_sec_type(struct ath11k *ar, struct ath11k_peer *peer,
					       struct ieee80211_key_conf *key_conf)
{
	return 0;
}

static inline int ath11k_nss_setup(struct ath11k_base *ab)
{
	return 0;
}

static inline int ath11k_nss_teardown(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_nss_ext_rx_stats(struct ath11k_base *ab,
					   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	return;
}
#endif /* CPTCFG_ATH11K_NSS_SUPPORT */
#endif
