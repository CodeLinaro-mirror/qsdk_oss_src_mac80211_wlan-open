/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_HAL_H
#define ATH12K_HAL_H

#include "hw.h"

struct ath12k_base;
struct hal_rx_reo_queue;

#define HAL_CE_REMAP_REG_BASE  (ab->ce_remap_base_addr)

#define HAL_RING_BASE_ALIGN	8
#define HAL_REO_QLUT_ADDR_ALIGN 256

#define HAL_LINK_DESC_SIZE			(32 << 2)
#define HAL_LINK_DESC_ALIGN			128
#define HAL_NUM_MPDUS_PER_LINK_DESC		6
#define HAL_NUM_TX_MSDUS_PER_LINK_DESC		7
#define HAL_NUM_RX_MSDUS_PER_LINK_DESC		6
#define HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC	12
#define HAL_MAX_AVAIL_BLK_RES			3

/* SRNG registers are split into two groups R0 and R2 */
#define HAL_SRNG_REG_GRP_R0	0
#define HAL_SRNG_REG_GRP_R2	1
#define HAL_SRNG_NUM_REG_GRP    2

#define HAL_ADDR_LSB_REG_MASK		0xffffffff
#define HAL_ADDR_MSB_REG_SHIFT		32

#define HAL_WBM2SW_REL_ERR_RING_NUM 3
#define HAL_TX_ADDRX_EN		1
#define HAL_TX_ADDRY_EN		2

#define HAL_TX_ADDR_SEARCH_DEFAULT	0
#define HAL_TX_ADDR_SEARCH_INDE		1

#define HAL_SHADOW_NUM_REGS_MAX			40

#define HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX	32704
/* TODO: Check with hw team on the supported scatter buf size */
#define HAL_WBM_IDLE_SCATTER_NEXT_PTR_SIZE	8
#define HAL_WBM_IDLE_SCATTER_BUF_SIZE (HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX - \
				       HAL_WBM_IDLE_SCATTER_NEXT_PTR_SIZE)

/* TODO: 16 entries per radio times MAX_VAPS_SUPPORTED */
#define HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX	32
#define HAL_DSCP_TID_TBL_SIZE			24

#define RU_INVALID		0
#define RU_26			1
#define RU_52			2
#define RU_106			4
#define RU_242			9
#define RU_484			18
#define RU_996			37
#define RU_2X996		74
#define RU_3X996		111
#define RU_4X996		148
#define RU_52_26		(RU_52 + RU_26)
#define RU_106_26		(RU_106 + RU_26)
#define RU_484_242		(RU_484 + RU_242)
#define RU_996_484		(RU_996 + RU_484)
#define RU_996_484_242		(RU_996 + RU_484_242)
#define RU_2X996_484		(RU_2X996 + RU_484)
#define RU_3X996_484		(RU_3X996 + RU_484)

/* TODO: number of PMACs */
#define HAL_SRNG_NUM_PMACS      3
#define HAL_SRNG_NUM_DMAC_RINGS (HAL_SRNG_RING_ID_DMAC_CMN_ID_END - \
				 HAL_SRNG_RING_ID_DMAC_CMN_ID_START)
#define HAL_SRNG_RINGS_PER_PMAC (HAL_SRNG_RING_ID_PMAC1_ID_END - \
				 HAL_SRNG_RING_ID_PMAC1_ID_START)
#define HAL_SRNG_NUM_PMAC_RINGS (HAL_SRNG_NUM_PMACS * HAL_SRNG_RINGS_PER_PMAC)
#define HAL_SRNG_RING_ID_MAX    (HAL_SRNG_RING_ID_DMAC_CMN_ID_END + \
				 HAL_SRNG_NUM_PMAC_RINGS)

#define HAL_AST_IDX_INVALID	0xFFFF
#define HAL_RX_MAX_MCS		12
#define HAL_RX_MAX_MCS_HT	31
#define HAL_RX_MAX_MCS_VHT	9
#define HAL_RX_MAX_MCS_HE	11
#define HAL_RX_MAX_MCS_BE	15
#define HAL_RX_MAX_NSS		8
#define HAL_RX_MAX_NUM_LEGACY_RATES 12

enum hal_rx_su_mu_coding {
	HAL_RX_SU_MU_CODING_BCC,
	HAL_RX_SU_MU_CODING_LDPC,
	HAL_RX_SU_MU_CODING_MAX,
};

enum hal_rx_gi {
	HAL_RX_GI_0_8_US,
	HAL_RX_GI_0_4_US,
	HAL_RX_GI_1_6_US,
	HAL_RX_GI_3_2_US,
	HAL_RX_GI_MAX,
};

enum hal_rx_bw {
	HAL_RX_BW_20MHZ,
	HAL_RX_BW_40MHZ,
	HAL_RX_BW_80MHZ,
	HAL_RX_BW_160MHZ,
	HAL_RX_BW_320MHZ,
	HAL_RX_BW_MAX,
};

enum hal_rx_preamble {
	HAL_RX_PREAMBLE_11A,
	HAL_RX_PREAMBLE_11B,
	HAL_RX_PREAMBLE_11N,
	HAL_RX_PREAMBLE_11AC,
	HAL_RX_PREAMBLE_11AX,
	HAL_RX_PREAMBLE_11BA,
	HAL_RX_PREAMBLE_11BE,
	HAL_RX_PREAMBLE_MAX,
};

enum hal_rx_reception_type {
	HAL_RX_RECEPTION_TYPE_SU,
	HAL_RX_RECEPTION_TYPE_MU_MIMO,
	HAL_RX_RECEPTION_TYPE_MU_OFDMA,
	HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO,
	HAL_RX_RECEPTION_TYPE_MAX,
};

enum hal_rx_legacy_rate {
	HAL_RX_LEGACY_RATE_1_MBPS,
	HAL_RX_LEGACY_RATE_2_MBPS,
	HAL_RX_LEGACY_RATE_5_5_MBPS,
	HAL_RX_LEGACY_RATE_6_MBPS,
	HAL_RX_LEGACY_RATE_9_MBPS,
	HAL_RX_LEGACY_RATE_11_MBPS,
	HAL_RX_LEGACY_RATE_12_MBPS,
	HAL_RX_LEGACY_RATE_18_MBPS,
	HAL_RX_LEGACY_RATE_24_MBPS,
	HAL_RX_LEGACY_RATE_36_MBPS,
	HAL_RX_LEGACY_RATE_48_MBPS,
	HAL_RX_LEGACY_RATE_54_MBPS,
	HAL_RX_LEGACY_RATE_INVALID,
};

enum hal_srng_ring_id {
	HAL_SRNG_RING_ID_REO2SW0 = 0,
	HAL_SRNG_RING_ID_REO2SW1,
	HAL_SRNG_RING_ID_REO2SW2,
	HAL_SRNG_RING_ID_REO2SW3,
	HAL_SRNG_RING_ID_REO2SW4,
	HAL_SRNG_RING_ID_REO2SW5,
	HAL_SRNG_RING_ID_REO2SW6,
	HAL_SRNG_RING_ID_REO2SW7,
	HAL_SRNG_RING_ID_REO2SW8,
	HAL_SRNG_RING_ID_REO2TCL,
	HAL_SRNG_RING_ID_REO2PPE,

	HAL_SRNG_RING_ID_SW2REO  = 16,
	HAL_SRNG_RING_ID_SW2REO1,
	HAL_SRNG_RING_ID_SW2REO2,
	HAL_SRNG_RING_ID_SW2REO3,

	HAL_SRNG_RING_ID_REO_CMD,
	HAL_SRNG_RING_ID_REO_STATUS,

	HAL_SRNG_RING_ID_SW2TCL1 = 24,
	HAL_SRNG_RING_ID_SW2TCL2,
	HAL_SRNG_RING_ID_SW2TCL3,
	HAL_SRNG_RING_ID_SW2TCL4,
	HAL_SRNG_RING_ID_SW2TCL5,
	HAL_SRNG_RING_ID_SW2TCL6,
	HAL_SRNG_RING_ID_PPE2TCL1 = 30,

	HAL_SRNG_RING_ID_SW2TCL_CMD = 40,
	HAL_SRNG_RING_ID_SW2TCL1_CMD,
	HAL_SRNG_RING_ID_TCL_STATUS,

	HAL_SRNG_RING_ID_CE0_SRC = 64,
	HAL_SRNG_RING_ID_CE1_SRC,
	HAL_SRNG_RING_ID_CE2_SRC,
	HAL_SRNG_RING_ID_CE3_SRC,
	HAL_SRNG_RING_ID_CE4_SRC,
	HAL_SRNG_RING_ID_CE5_SRC,
	HAL_SRNG_RING_ID_CE6_SRC,
	HAL_SRNG_RING_ID_CE7_SRC,
	HAL_SRNG_RING_ID_CE8_SRC,
	HAL_SRNG_RING_ID_CE9_SRC,
	HAL_SRNG_RING_ID_CE10_SRC,
	HAL_SRNG_RING_ID_CE11_SRC,
	HAL_SRNG_RING_ID_CE12_SRC,
	HAL_SRNG_RING_ID_CE13_SRC,
	HAL_SRNG_RING_ID_CE14_SRC,
	HAL_SRNG_RING_ID_CE15_SRC,

	HAL_SRNG_RING_ID_CE0_DST = 81,
	HAL_SRNG_RING_ID_CE1_DST,
	HAL_SRNG_RING_ID_CE2_DST,
	HAL_SRNG_RING_ID_CE3_DST,
	HAL_SRNG_RING_ID_CE4_DST,
	HAL_SRNG_RING_ID_CE5_DST,
	HAL_SRNG_RING_ID_CE6_DST,
	HAL_SRNG_RING_ID_CE7_DST,
	HAL_SRNG_RING_ID_CE8_DST,
	HAL_SRNG_RING_ID_CE9_DST,
	HAL_SRNG_RING_ID_CE10_DST,
	HAL_SRNG_RING_ID_CE11_DST,
	HAL_SRNG_RING_ID_CE12_DST,
	HAL_SRNG_RING_ID_CE13_DST,
	HAL_SRNG_RING_ID_CE14_DST,
	HAL_SRNG_RING_ID_CE15_DST,

	HAL_SRNG_RING_ID_CE0_DST_STATUS = 100,
	HAL_SRNG_RING_ID_CE1_DST_STATUS,
	HAL_SRNG_RING_ID_CE2_DST_STATUS,
	HAL_SRNG_RING_ID_CE3_DST_STATUS,
	HAL_SRNG_RING_ID_CE4_DST_STATUS,
	HAL_SRNG_RING_ID_CE5_DST_STATUS,
	HAL_SRNG_RING_ID_CE6_DST_STATUS,
	HAL_SRNG_RING_ID_CE7_DST_STATUS,
	HAL_SRNG_RING_ID_CE8_DST_STATUS,
	HAL_SRNG_RING_ID_CE9_DST_STATUS,
	HAL_SRNG_RING_ID_CE10_DST_STATUS,
	HAL_SRNG_RING_ID_CE11_DST_STATUS,
	HAL_SRNG_RING_ID_CE12_DST_STATUS,
	HAL_SRNG_RING_ID_CE13_DST_STATUS,
	HAL_SRNG_RING_ID_CE14_DST_STATUS,
	HAL_SRNG_RING_ID_CE15_DST_STATUS,

	HAL_SRNG_RING_ID_WBM_IDLE_LINK = 120,
	HAL_SRNG_RING_ID_WBM_SW0_RELEASE,
	HAL_SRNG_RING_ID_WBM_SW1_RELEASE,
	HAL_SRNG_RING_ID_WBM_PPE_RELEASE = 123,

	HAL_SRNG_RING_ID_WBM2SW0_RELEASE = 128,
	HAL_SRNG_RING_ID_WBM2SW1_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW2_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW3_RELEASE, /* RX ERROR RING */
	HAL_SRNG_RING_ID_WBM2SW4_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW5_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW6_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW7_RELEASE,

	HAL_SRNG_RING_ID_UMAC_ID_END = 159,

	/* Common DMAC rings shared by all LMACs */
	HAL_SRNG_RING_ID_DMAC_CMN_ID_START = 160,
	HAL_SRNG_SW2RXDMA_BUF0 = HAL_SRNG_RING_ID_DMAC_CMN_ID_START,
	HAL_SRNG_SW2RXDMA_BUF1 = 161,
	HAL_SRNG_SW2RXDMA_BUF2 = 162,

	HAL_SRNG_SW2RXMON_BUF0 = 168,

	HAL_SRNG_SW2TXMON_BUF0 = 176,

	HAL_SRNG_RING_ID_DMAC_CMN_ID_END = 183,
	HAL_SRNG_RING_ID_PMAC1_ID_START = 184,

	HAL_SRNG_RING_ID_WMAC1_SW2RXMON_BUF0 = HAL_SRNG_RING_ID_PMAC1_ID_START,

	HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
	HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
	HAL_SRNG_RING_ID_WMAC1_RXMON2SW0 = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
	HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_DESC,
	HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
	HAL_SRNG_RING_ID_WMAC1_TXMON2SW0_BUF0,
	HAL_SRNG_RING_ID_WMAC1_SW2TXMON_BUF0,

	HAL_SRNG_RING_ID_PMAC1_ID_END,
};

enum hal_ring_type {
	HAL_REO_DST,
	HAL_REO_EXCEPTION,
	HAL_REO_REINJECT,
	HAL_REO_CMD,
	HAL_REO_STATUS,
	HAL_TCL_DATA,
	HAL_TCL_CMD,
	HAL_TCL_STATUS,
	HAL_CE_SRC,
	HAL_CE_DST,
	HAL_CE_DST_STATUS,
	HAL_WBM_IDLE_LINK,
	HAL_SW2WBM_RELEASE,
	HAL_WBM2SW_RELEASE,
	HAL_RXDMA_BUF,
	HAL_RXDMA_DST,
	HAL_RXDMA_MONITOR_BUF,
	HAL_RXDMA_MONITOR_STATUS,
	HAL_RXDMA_MONITOR_DST,
	HAL_RXDMA_MONITOR_DESC,
	HAL_RXDMA_DIR_BUF,
	HAL_PPE2TCL,
	HAL_PPE_RELEASE,
	HAL_TX_MONITOR_BUF,
	HAL_TX_MONITOR_DST,
	HAL_MAX_RING_TYPES,
};

/**
 * enum hal_reo_cmd_type: Enum for REO command type
 * @HAL_REO_CMD_GET_QUEUE_STATS: Get REO queue status/stats
 * @HAL_REO_CMD_FLUSH_QUEUE: Flush all frames in REO queue
 * @HAL_REO_CMD_FLUSH_CACHE: Flush descriptor entries in the cache
 * @HAL_REO_CMD_UNBLOCK_CACHE: Unblock a descriptor's address that was blocked
 *      earlier with a 'REO_FLUSH_CACHE' command
 * @HAL_REO_CMD_FLUSH_TIMEOUT_LIST: Flush buffers/descriptors from timeout list
 * @HAL_REO_CMD_UPDATE_RX_QUEUE: Update REO queue settings
 */
enum hal_reo_cmd_type {
	HAL_REO_CMD_GET_QUEUE_STATS     = 0,
	HAL_REO_CMD_FLUSH_QUEUE         = 1,
	HAL_REO_CMD_FLUSH_CACHE         = 2,
	HAL_REO_CMD_UNBLOCK_CACHE       = 3,
	HAL_REO_CMD_FLUSH_TIMEOUT_LIST  = 4,
	HAL_REO_CMD_UPDATE_RX_QUEUE     = 5,
};

enum hal_reo_dest_ring_error_code {
	HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID,
	HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA,
	HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN,
	HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED,
	HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED,
	HAL_REO_DEST_RING_ERROR_CODE_MAX,
};

enum hal_reo_entr_rxdma_ecode {
	HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MPDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FCS_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_UNECRYPTED_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LIMIT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_WIFI_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_SA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLOW_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLUSH_REQUEST_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_FRAG_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MAX,
};

enum hal_encrypt_type {
	HAL_ENCRYPT_TYPE_WEP_40,
	HAL_ENCRYPT_TYPE_WEP_104,
	HAL_ENCRYPT_TYPE_TKIP_NO_MIC,
	HAL_ENCRYPT_TYPE_WEP_128,
	HAL_ENCRYPT_TYPE_TKIP_MIC,
	HAL_ENCRYPT_TYPE_WAPI,
	HAL_ENCRYPT_TYPE_CCMP_128,
	HAL_ENCRYPT_TYPE_OPEN,
	HAL_ENCRYPT_TYPE_CCMP_256,
	HAL_ENCRYPT_TYPE_GCMP_128,
	HAL_ENCRYPT_TYPE_AES_GCMP_256,
	HAL_ENCRYPT_TYPE_WAPI_GCM_SM4,
};

enum hal_wbm_htt_tx_comp_status {
	HAL_WBM_REL_HTT_TX_COMP_STATUS_OK,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX,
};

/**
 * enum hal_reo_cmd_status: Enum for execution status of REO command
 * @HAL_REO_CMD_SUCCESS: Command has successfully executed
 * @HAL_REO_CMD_BLOCKED: Command could not be executed as the queue
 *			 or cache was blocked
 * @HAL_REO_CMD_FAILED: Command execution failed, could be due to
 *			invalid queue desc
 * @HAL_REO_CMD_RESOURCE_BLOCKED:
 * @HAL_REO_CMD_DRAIN:
 */
enum hal_reo_cmd_status {
	HAL_REO_CMD_SUCCESS		= 0,
	HAL_REO_CMD_BLOCKED		= 1,
	HAL_REO_CMD_FAILED		= 2,
	HAL_REO_CMD_RESOURCE_BLOCKED	= 3,
	HAL_REO_CMD_DRAIN		= 0xff,
};

struct hal_srng_params {
	dma_addr_t ring_base_paddr;
	u32 *ring_base_vaddr;
	int num_entries;
	u32 intr_batch_cntr_thres_entries;
	u32 intr_timer_thres_us;
	u32 flags;
	u32 max_buffer_len;
	u32 low_threshold;
	u32 high_threshold;
	dma_addr_t msi_addr;
	dma_addr_t msi2_addr;
	u32 msi_data;
	u32 msi2_data;

	/* Add more params as needed */
};

enum hal_srng_dir {
	HAL_SRNG_DIR_SRC,
	HAL_SRNG_DIR_DST
};

struct hal_rx_desc_data {
	u32 freq;
	u32 err_bitmap;
	u32 enctype;
	u32 msdu_done:1,
	    is_decrypted:1,
	    ip_csum_fail:1,
	    l4_csum_fail:1,
	    is_first_msdu:1,
	    is_last_msdu:1,
	    mesh_ctrl_present:1,
	    mac_addr2_valid:1,
	    fill_crypto_hdr:1,
	    seq_ctl_valid:1,
	    fc_valid:1;
	u16 msdu_len;
	u16 peer_id;
	u16 seq_no;
	u8 *mpdu_start_addr2;
	u8 pkt_type;
	u8 l3_pad_bytes;
	u8 decap;
	u8 bw;
	u8 rate_mcs;
	u8 nss;
	u8 sgi;
	u8 tid;
};

struct ath12k_buffer_address {
        __le32 info0;
        __le32 info1;
} __packed;

struct hal_ce_srng_dest_desc;

struct hal_ce_srng_dst_status_desc;

struct hal_ce_srng_src_desc;

struct wbm_link_desc {
        struct ath12k_buffer_address buf_addr_info;
} __packed;

struct wbm_idle_scatter_list {
        dma_addr_t paddr;
        struct wbm_link_desc *vaddr;
};

/* srng flags */
#define HAL_SRNG_FLAGS_MSI_SWAP			0x00000008
#define HAL_SRNG_FLAGS_RING_PTR_SWAP		0x00000010
#define HAL_SRNG_FLAGS_DATA_TLV_SWAP		0x00000020
#define HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN	0x00010000
#define HAL_SRNG_FLAGS_MSI_INTR			0x00020000
#define HAL_SRNG_FLAGS_HIGH_THRESH_INTR_EN	0x00080000
#define HAL_SRNG_FLAGS_LMAC_RING		0x80000000

/* Common SRNG ring structure for source and destination rings */
struct hal_srng {
	/* Unique SRNG ring ID */
	u8 ring_id;

	/* Ring initialization done */
	u8 initialized;

	/* Interrupt/MSI value assigned to this ring */
	int irq;

	/* Physical base address of the ring */
	dma_addr_t ring_base_paddr;

	/* Virtual base address of the ring */
	u32 *ring_base_vaddr;

	/* Number of entries in ring */
	u32 num_entries;

	/* Ring size */
	u32 ring_size;

	/* Ring size mask */
	u32 ring_size_mask;

	/* Size of ring entry */
	u32 entry_size;

	/* Interrupt timer threshold - in micro seconds */
	u32 intr_timer_thres_us;

	/* Interrupt batch counter threshold - in number of ring entries */
	u32 intr_batch_cntr_thres_entries;

	/* MSI Address */
	dma_addr_t msi_addr;

	/* MSI data */
	u32 msi_data;

	/* MSI2 Address */
	dma_addr_t msi2_addr;

	/* MSI2 data */
	u32 msi2_data;

	/* Misc flags */
	u32 flags;

	/* Lock for serializing ring index updates */
	spinlock_t lock;

	struct lock_class_key lock_key;

	/* Start offset of SRNG register groups for this ring
	 * TBD: See if this is required - register address can be derived
	 * from ring ID
	 */
	u32 hwreg_base[HAL_SRNG_NUM_REG_GRP];

	u64 timestamp;

	/* Source or Destination ring */
	enum hal_srng_dir ring_dir;

	union {
		struct {
			/* SW tail pointer */
			u32 tp;

			/* Shadow head pointer location to be updated by HW */
			volatile u32 *hp_addr;

			/* Cached head pointer */
			u32 cached_hp;

			/* Tail pointer location to be updated by SW - This
			 * will be a register address and need not be
			 * accessed through SW structure
			 */
			u32 *tp_addr;

			/* Current SW loop cnt */
			u32 loop_cnt;

			/* max transfer size */
			u16 max_buffer_length;

			/* head pointer at access end */
			u32 last_hp;
		} dst_ring;

		struct {
			/* SW head pointer */
			u32 hp;

			/* SW reap head pointer */
			u32 reap_hp;

			/* Shadow tail pointer location to be updated by HW */
			u32 *tp_addr;

			/* Cached tail pointer */
			u32 cached_tp;

			/* Head pointer location to be updated by SW - This
			 * will be a register address and need not be accessed
			 * through SW structure
			 */
			u32 *hp_addr;
			u32 *hp_addr_direct;

			/* Low threshold - in number of ring entries */
			u32 low_threshold;

			/* tail pointer at access end */
			u32 last_tp;
		} src_ring;
	} u;
};

/* Interrupt mitigation - Batch threshold in terms of number of frames */
#define HAL_SRNG_INT_BATCH_THRESHOLD_TX 256
#define HAL_SRNG_INT_BATCH_THRESHOLD_RX 128
#define HAL_SRNG_INT_BATCH_THRESHOLD_OTHER 1

/* Interrupt mitigation - timer threshold in us */
#define HAL_SRNG_INT_TIMER_THRESHOLD_TX 1000
#define HAL_SRNG_INT_TIMER_THRESHOLD_RX 500
#define HAL_SRNG_INT_TIMER_THRESHOLD_OTHER 256

enum hal_srng_mac_type {
	ATH12K_HAL_SRNG_UMAC,
	ATH12K_HAL_SRNG_DMAC,
	ATH12K_HAL_SRNG_PMAC
};

/* HW SRNG configuration table */
struct hal_srng_config {
	int start_ring_id;
	u16 max_rings;
	u16 entry_size;
	u32 reg_start[HAL_SRNG_NUM_REG_GRP];
	u16 reg_size[HAL_SRNG_NUM_REG_GRP];
	enum hal_srng_mac_type mac_type;
	enum hal_srng_dir ring_dir;
	u32 max_size;
};

/**
 * enum hal_rx_buf_return_buf_manager - manager for returned rx buffers
 *
 * @HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST: Buffer returned to WBM idle buffer list
 * @HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 0 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_WBM_DEV1_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 1 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_WBM_DEV2_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 2 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_FW_BM: Buffer returned to FW
 * @HAL_RX_BUF_RBM_SW0_BM: For ring 0 -- returned to host
 * @HAL_RX_BUF_RBM_SW1_BM: For ring 1 -- returned to host
 * @HAL_RX_BUF_RBM_SW2_BM: For ring 2 -- returned to host
 * @HAL_RX_BUF_RBM_SW3_BM: For ring 3 -- returned to host
 * @HAL_RX_BUF_RBM_SW4_BM: For ring 4 -- returned to host
 * @HAL_RX_BUF_RBM_SW5_BM: For ring 5 -- returned to host
 * @HAL_RX_BUF_RBM_SW6_BM: For ring 6 -- returned to host
 */

enum hal_rx_buf_return_buf_manager {
	HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST,
	HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_WBM_DEV1_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_WBM_DEV2_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_FW_BM,
	HAL_RX_BUF_RBM_SW0_BM,
	HAL_RX_BUF_RBM_SW1_BM,
	HAL_RX_BUF_RBM_SW2_BM,
	HAL_RX_BUF_RBM_SW3_BM,
	HAL_RX_BUF_RBM_SW4_BM,
	HAL_RX_BUF_RBM_SW5_BM,
	HAL_RX_BUF_RBM_SW6_BM,
};

enum hal_pn_type {
	HAL_PN_TYPE_NONE,
	HAL_PN_TYPE_WPA,
	HAL_PN_TYPE_WAPI_EVEN,
	HAL_PN_TYPE_WAPI_UNEVEN,
};

enum hal_ce_desc {
	HAL_CE_DESC_SRC,
	HAL_CE_DESC_DST,
	HAL_CE_DESC_DST_STATUS,
};

#define HAL_HASH_ROUTING_RING_TCL 0
#define HAL_HASH_ROUTING_RING_SW1 1
#define HAL_HASH_ROUTING_RING_SW2 2
#define HAL_HASH_ROUTING_RING_SW3 3
#define HAL_HASH_ROUTING_RING_SW4 4
#define HAL_HASH_ROUTING_RING_REL 5
#define HAL_HASH_ROUTING_RING_FW  6

struct hal_reo_status_header {
	u16 cmd_num;
	enum hal_reo_cmd_status cmd_status;
	u16 cmd_exe_time;
	u32 timestamp;
};

struct ath12k_hw_hal_params {
	enum hal_rx_buf_return_buf_manager rx_buf_rbm;
	u32	  wbm2sw_cc_enable;
};

struct ath12k_hw_regs {
	u32 hal_tcl1_ring_id;
	u32 hal_tcl1_ring_misc;
	u32 hal_tcl1_ring_tp_addr_lsb;
	u32 hal_tcl1_ring_tp_addr_msb;
	u32 hal_tcl1_ring_consumer_int_setup_ix0;
	u32 hal_tcl1_ring_consumer_int_setup_ix1;
	u32 hal_tcl1_ring_msi1_base_lsb;
	u32 hal_tcl1_ring_msi1_base_msb;
	u32 hal_tcl1_ring_msi1_data;
	u32 hal_tcl_ring_base_lsb;
	u32 hal_tcl1_ring_base_lsb;
	u32 hal_tcl1_ring_base_msb;
	u32 hal_tcl2_ring_base_lsb;

	u32 hal_tcl_status_ring_base_lsb;

	u32 hal_reo1_qdesc_addr;
	u32 hal_reo1_qdesc_max_peerid;

	u32 hal_wbm_idle_ring_base_lsb;
	u32 hal_wbm_idle_ring_misc_addr;
	u32 hal_wbm_r0_idle_list_cntl_addr;
	u32 hal_wbm_r0_idle_list_size_addr;
	u32 hal_wbm_scattered_ring_base_lsb;
	u32 hal_wbm_scattered_ring_base_msb;
	u32 hal_wbm_scattered_desc_head_info_ix0;
	u32 hal_wbm_scattered_desc_head_info_ix1;
	u32 hal_wbm_scattered_desc_tail_info_ix0;
	u32 hal_wbm_scattered_desc_tail_info_ix1;
	u32 hal_wbm_scattered_desc_ptr_hp_addr;

	u32 hal_wbm_sw_release_ring_base_lsb;
	u32 hal_wbm_sw1_release_ring_base_lsb;
	u32 hal_wbm0_release_ring_base_lsb;
	u32 hal_wbm1_release_ring_base_lsb;

	u32 pcie_qserdes_sysclk_en_sel;
	u32 pcie_pcs_osc_dtct_config_base;

	u32 hal_umac_ce0_src_reg_base;
	u32 hal_umac_ce0_dest_reg_base;
	u32 hal_umac_ce1_src_reg_base;
	u32 hal_umac_ce1_dest_reg_base;

	u32 hal_ppe_rel_ring_base;

	u32 hal_reo2_ring_base;
	u32 hal_reo1_misc_ctrl_addr;
	u32 hal_reo1_sw_cookie_cfg0;
	u32 hal_reo1_sw_cookie_cfg1;
	u32 hal_reo1_qdesc_lut_base0;
	u32 hal_reo1_qdesc_lut_base1;
	u32 hal_reo1_ring_base_lsb;
	u32 hal_reo1_ring_base_msb;
	u32 hal_reo1_ring_id;
	u32 hal_reo1_ring_misc;
	u32 hal_reo1_ring_hp_addr_lsb;
	u32 hal_reo1_ring_hp_addr_msb;
	u32 hal_reo1_ring_producer_int_setup;
	u32 hal_reo1_ring_msi1_base_lsb;
	u32 hal_reo1_ring_msi1_base_msb;
	u32 hal_reo1_ring_msi1_data;
	u32 hal_reo1_aging_thres_ix0;
	u32 hal_reo1_aging_thres_ix1;
	u32 hal_reo1_aging_thres_ix2;
	u32 hal_reo1_aging_thres_ix3;

	u32 hal_reo2_sw0_ring_base;

	u32 hal_sw2reo_ring_base;
	u32 hal_sw2reo1_ring_base;

	u32 hal_reo_cmd_ring_base;

	u32 hal_reo_status_ring_base;
};

struct ath12k_hw_version_map {
	const struct ath12k_hw_hal_params *hal_params;
	const struct ath12k_hw_regs *hw_regs;
};

/* HAL context to be used to access SRNG APIs (currently used by data path
 * and transport (CE) modules)
 */
struct ath12k_hal {
	/* HAL internal state for all SRNG rings.
	 */
	struct hal_srng srng_list[HAL_SRNG_RING_ID_MAX];

	/* SRNG configuration table */
	struct hal_srng_config *srng_config;

	/* Remote pointer memory for HW/FW updates */
	struct {
		u32 *vaddr;
		dma_addr_t paddr;
	} rdp;

	/* Shared memory for ring pointer updates from host to FW */
	struct {
		u32 *vaddr;
		dma_addr_t paddr;
	} wrp;

	struct device *dev;
	const struct hal_ops *hal_ops;
	const struct ath12k_hw_regs *regs;
	const struct ath12k_hw_hal_params *hal_params;
	/* Available REO blocking resources bitmap */
	u8 avail_blk_resource;

	u8 current_blk_index;

	/* shadow register configuration */
	u32 shadow_reg_addr[HAL_SHADOW_NUM_REGS_MAX];
	int num_shadow_reg_configured;

	u32 hal_desc_sz;

	const struct ath12k_hal_tcl_to_wbm_rbm_map *tcl_to_wbm_rbm_map;
};

/* Maps WBM ring number and Return Buffer Manager Id per TCL ring */
struct ath12k_hal_tcl_to_wbm_rbm_map  {
	u8 wbm_ring_num;
	u8 rbm_id;
};

struct hal_rx_ops {
	bool (*rx_desc_get_first_msdu)(struct hal_rx_desc *desc);
	bool (*rx_desc_get_last_msdu)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_l3_pad_bytes)(struct hal_rx_desc *desc);
	u8 *(*rx_desc_get_hdr_status)(struct hal_rx_desc *desc);
	bool (*rx_desc_encrypt_valid)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_encrypt_type)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_decap_type)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_mesh_ctl)(struct hal_rx_desc *desc);
	bool (*rx_desc_get_mpdu_seq_ctl_vld)(struct hal_rx_desc *desc);
	bool (*rx_desc_get_mpdu_fc_valid)(struct hal_rx_desc *desc);
	u16 (*rx_desc_get_mpdu_start_seq_no)(struct hal_rx_desc *desc);
	u16 (*rx_desc_get_msdu_len)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_msdu_sgi)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_msdu_rate_mcs)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_msdu_rx_bw)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_msdu_freq)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_msdu_pkt_type)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_msdu_nss)(struct hal_rx_desc *desc);
	u8 (*rx_desc_get_mpdu_tid)(struct hal_rx_desc *desc);
	u16 (*rx_desc_get_mpdu_peer_id)(struct hal_rx_desc *desc);
	void (*rx_desc_copy_end_tlv)(struct hal_rx_desc *fdesc,
				     struct hal_rx_desc *ldesc);
	u32 (*rx_desc_get_mpdu_start_tag)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_mpdu_ppdu_id)(struct hal_rx_desc *desc);
	void (*rx_desc_set_msdu_len)(struct hal_rx_desc *desc, u16 len);
	struct rx_attention *(*rx_desc_get_attention)(struct hal_rx_desc *desc);
	u8 *(*rx_desc_get_msdu_payload)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_mpdu_start_offset)(void);
	u32 (*rx_desc_get_msdu_end_offset)(void);
	bool (*rx_desc_mac_addr2_valid)(struct hal_rx_desc *desc);
	u8* (*rx_desc_mpdu_start_addr2)(struct hal_rx_desc *desc);
	bool (*rx_desc_is_da_mcbc)(struct hal_rx_desc *desc);
	void (*rx_desc_get_dot11_hdr)(struct hal_rx_desc *desc,
				      struct ieee80211_hdr *hdr);
	void (*rx_desc_get_crypto_header)(struct hal_rx_desc *desc,
					  u8 *crypto_hdr,
					  enum hal_encrypt_type enctype);
	bool (*dp_rx_h_msdu_done)(struct hal_rx_desc *desc);
	bool (*dp_rx_h_l4_cksum_fail)(struct hal_rx_desc *desc);
	bool (*dp_rx_h_ip_cksum_fail)(struct hal_rx_desc *desc);
	bool (*dp_rx_h_is_decrypted)(struct hal_rx_desc *desc);
	u32 (*dp_rx_h_mpdu_err)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_desc_size)(void);
	u8 (*rx_desc_get_msdu_src_link_id)(struct hal_rx_desc *desc);
};

struct hal_wbm_idle_scatter_list;
struct hal_wbm_link_desc;

struct hw_regs_version_mapping {
	u8 hw_version;
	const struct ath12k_hw_regs *hw_regs;
};

struct hal_ops {
	int (*hal_init)(struct ath12k_hal *hal, u8 hw_version);
	int (*create_srng_config)(struct ath12k_hal *hal);
	void (*rx_desc_copy_end_tlv)(struct hal_rx_desc *fdesc,
				     struct hal_rx_desc *ldesc);
	void (*rx_desc_get_crypto_header)(struct hal_rx_desc *desc,
					  u8 *crypto_hdr,
					  enum hal_encrypt_type enctype);
	u16 (*rx_desc_get_mpdu_frame_ctl)(struct hal_rx_desc *desc);
	void (*rx_desc_get_dot11_hdr)(struct hal_rx_desc *desc,
				      struct ieee80211_hdr *hdr);
	void (*rx_desc_set_msdu_len)(struct hal_rx_desc *desc, u16 len);
	u8 (*rx_desc_get_msdu_src_link_id)(struct hal_rx_desc *desc);
	void (*extract_rx_desc_data)(struct hal_rx_desc_data *rx_desc_data,
				     struct hal_rx_desc *rx_desc,
				     struct hal_rx_desc *ldesc);
	void (*ce_dst_setup)(struct ath12k_base *ab,
			     struct hal_srng *srng, int ring_num);
	void (*set_umac_srng_ptr_addr)(struct ath12k_base *ab,
				       struct hal_srng *srng,
				       enum hal_ring_type type, int ring_num);
	void (*srng_src_hw_init)(struct ath12k_base *ab, struct hal_srng *srng);
	void (*srng_dst_hw_init)(struct ath12k_base *ab, struct hal_srng *srng);
	int (*srng_update_shadow_config)(struct ath12k_base *ab,
					 enum hal_ring_type ring_type,
					 int ring_num);
	int (*srng_get_ring_id)(struct ath12k_hal *hal, enum hal_ring_type type,
				int ring_num, int mac_id);
	u32 (*ce_get_desc_size)(enum hal_ce_desc type);
	void (*ce_src_set_desc)(struct hal_ce_srng_src_desc *desc,
				dma_addr_t paddr, u32 len, u32 id,
				u8 byte_swap_data);
	void (*ce_dst_set_desc)(struct hal_ce_srng_dest_desc *desc,
				dma_addr_t paddr);
	u32 (*ce_dst_status_get_length)(struct hal_ce_srng_dst_status_desc *desc);
	void (*set_link_desc_addr)(struct hal_wbm_link_desc *desc, u32 cookie,
				   dma_addr_t paddr,
				   enum hal_rx_buf_return_buf_manager rbm);
	void (*setup_link_idle_list)(struct ath12k_base *ab,
				     struct hal_wbm_idle_scatter_list *sbuf,
				     u32 nsbufs, u32 tot_link_desc,
				     u32 end_offset);
	void (*tx_set_dscp_tid_map)(struct ath12k_base *ab, int id);
	void (*tx_configure_bank_register)(struct ath12k_base *ab,
					   u32 bank_config, u8 bank_id);
	void (*write_ml_reoq_lut_addr)(struct ath12k_base *ab,
				       dma_addr_t paddr);
	void (*write_reoq_lut_addr)(struct ath12k_base *ab, dma_addr_t paddr);
	void (*reo_qdesc_setup)(struct hal_rx_reo_queue *qdesc,
				int tid, u32 ba_window_size,
				u32 start_seq, enum hal_pn_type type);
	void (*reo_init_cmd_ring)(struct ath12k_base *ab,
				  struct hal_srng *srng);
	void (*reo_hw_setup)(struct ath12k_base *ab, u32 ring_hash_map);
	void (*rx_buf_addr_info_set)(struct ath12k_buffer_address *binfo,
				     dma_addr_t paddr, u32 cookie, u8 manager);
	void (*rx_buf_addr_info_get)(struct ath12k_buffer_address *binfo,
				     dma_addr_t *paddr, u32 *msdu_cookies,
				     u8 *rbm);
	void (*cc_config)(struct ath12k_base *ab);
	enum hal_rx_buf_return_buf_manager
        (*get_idle_link_rbm)(struct ath12k_hal *hal, u8 device_id);
};

static inline
enum nl80211_he_ru_alloc ath12k_he_ru_tones_to_nl80211_he_ru_alloc(u16 ru_tones)
{
	enum nl80211_he_ru_alloc ret;

	switch (ru_tones) {
	case RU_52:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_52;
		break;
	case RU_106:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_106;
		break;
	case RU_242:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_242;
		break;
	case RU_484:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_484;
		break;
	case RU_996:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_996;
		break;
	case RU_26:
		fallthrough;
	default:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_26;
		break;
	}
	return ret;
}

u8 ath12k_hal_rx_get_msdu_src_link(struct ath12k_base *ab,
				   struct hal_rx_desc *desc);
void ath12k_hal_rx_desc_end_tlv_copy(struct ath12k_base *ab,
				     struct hal_rx_desc *fdesc,
				     struct hal_rx_desc *ldesc);
void ath12k_hal_rxdesc_set_msdu_len(struct ath12k_base *ab,
				    struct hal_rx_desc *desc,
				    u16 len);
void ath12k_hal_rx_desc_get_dot11_hdr(struct ath12k_base *ab,
				      struct hal_rx_desc *desc,
				      struct ieee80211_hdr *hdr);
void ath12k_hal_rx_desc_get_crypto_header(struct ath12k_base *ab,
					  struct hal_rx_desc *desc,
					  u8 *crypto_hdr,
					  enum hal_encrypt_type enctype);
u16 ath12k_hal_rxdesc_get_mpdu_frame_ctrl(struct ath12k_base *ab,
					  struct hal_rx_desc *desc);
void ath12k_hal_reo_qdesc_setup(struct ath12k_hal *hal,
				struct hal_rx_reo_queue *qdesc,
				int tid, u32 ba_window_size,
				u32 start_seq, enum hal_pn_type type);
void ath12k_hal_setup_link_idle_list(struct ath12k_base *ab,
				     struct wbm_idle_scatter_list *sbuf,
				     u32 nsbufs, u32 tot_link_desc,
				     u32 end_offset);
dma_addr_t ath12k_hal_srng_get_tp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng);
dma_addr_t ath12k_hal_srng_get_hp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng);
u32 ath12k_hal_ce_get_desc_size(struct ath12k_hal *hal, enum hal_ce_desc type);
void ath12k_hal_ce_dst_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_dest_desc *desc,
				dma_addr_t paddr);
void ath12k_hal_ce_src_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_src_desc *desc,
				dma_addr_t paddr, u32 len, u32 id,
				u8 byte_swap_data);
int ath12k_hal_srng_get_entrysize(struct ath12k_base *ab, u32 ring_type);
int ath12k_hal_srng_get_max_entries(struct ath12k_base *ab, u32 ring_type);
void ath12k_hal_srng_get_params(struct ath12k_base *ab, struct hal_srng *srng,
				struct hal_srng_params *params);
void *ath12k_hal_srng_dst_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng);
void *ath12k_hal_srng_dst_peek(struct ath12k_base *ab, struct hal_srng *srng);
int ath12k_hal_srng_dst_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr);
void *ath12k_hal_srng_src_get_next_reaped(struct ath12k_base *ab,
					  struct hal_srng *srng);
void *ath12k_hal_srng_src_reap_next(struct ath12k_base *ab,
				    struct hal_srng *srng);
void *ath12k_hal_srng_src_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng);
int ath12k_hal_srng_src_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr);
void ath12k_hal_srng_access_begin(struct ath12k_base *ab,
				  struct hal_srng *srng);
void ath12k_hal_srng_access_end(struct ath12k_base *ab, struct hal_srng *srng);
int ath12k_hal_srng_setup(struct ath12k_base *ab, enum hal_ring_type type,
			  int ring_num, int mac_id,
			  struct hal_srng_params *params);
void ath12k_hal_dump_srng_stats(struct ath12k_base *ab);
void ath12k_hal_srng_get_shadow_config(struct ath12k_base *ab,
				       u32 **cfg, u32 *len);
int ath12k_hal_srng_update_shadow_config(struct ath12k_base *ab,
					 enum hal_ring_type ring_type,
					int ring_num);
void ath12k_hal_srng_shadow_config(struct ath12k_base *ab);
void ath12k_hal_srng_shadow_update_hp_tp(struct ath12k_base *ab,
					 struct hal_srng *srng);
void ath12k_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
int ath12k_hal_srng_init(struct ath12k_base *ab);
void ath12k_hal_srng_deinit(struct ath12k_base *ab);
void ath12k_hal_set_link_desc_addr(struct ath12k_hal *hal,
				   struct wbm_link_desc *desc, u32 cookie,
				   dma_addr_t paddr, int rbm);
u32
ath12k_hal_ce_dst_status_get_length(struct ath12k_hal *hal,
				    struct hal_ce_srng_dst_status_desc *desc);
void ath12k_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, int id);
void ath12k_hal_tx_configure_bank_register(struct ath12k_base *ab,
					   u32 bank_config, u8 bank_id);
void ath12k_hal_write_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr);
void
ath12k_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr);
void ath12k_hal_reo_init_cmd_ring(struct ath12k_base *ab,
                                 struct hal_srng *srng);
void ath12k_hal_reo_hw_setup(struct ath12k_base *ab, u32 ring_hash_map);
void ath12k_hal_rx_buf_addr_info_set(struct ath12k_hal *hal,
				     struct ath12k_buffer_address *binfo,
				     dma_addr_t paddr, u32 cookie, u8 manager);
void ath12k_hal_rx_buf_addr_info_get(struct ath12k_hal *hal,
				     struct ath12k_buffer_address *binfo,
				     dma_addr_t *paddr, u32 *msdu_cookies,
				     u8 *rbm);
void ath12k_hal_cc_config(struct ath12k_base *ab);
enum hal_rx_buf_return_buf_manager
ath12k_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id);
#endif
