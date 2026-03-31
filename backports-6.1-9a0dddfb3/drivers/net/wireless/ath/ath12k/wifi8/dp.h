/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI8_H
#define ATH12K_DP_WIFI8_H

#include "../core.h"
#include "../dp_cmn.h"
#include "hw.h"
#include "dp_ast.h"
#include "hal.h"

#define DP_TX_EXCEPTION_RING_SIZE      512
#define DP_WBM_REFILL_RING_MAX         4
#define DP_WBM_REFILL_RING_SIZE        512
#define DP_WBM_IDLE_BUF_RING_SIZE      16384
#define DP_FSE_CMD_RING_SIZE		256
#define DP_PPE2WBM_REFILL_RING_MAX     3
#define DP_PPE2WBM_REFILL_RING_SIZE    512
#define DP_PPE2WBM_IDLE_BUF_RING_SIZE  8192
#define DP_REO_FLUSH_RING_SIZE		256

#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || \
	defined(CPTCFG_ATH12K_MEM_PROFILE_512M)
#define ATH12K_RX_DESC_COUNT	8192
#elif defined(CONFIG_ATH12K_MEM_PROFILE_256M) || \
	defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
#define ATH12K_RX_DESC_COUNT	8192
#else
#define ATH12K_RX_DESC_COUNT	16384
#endif

#define ATH12K_NUM_RX_SPT_PAGES \
	(ATH12K_RX_DESC_COUNT / ATH12K_MAX_SPT_ENTRIES)

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp_htt_cmd_retry_info {
	DECLARE_BITMAP(htt_retry_peer_id_map, ATH12K_MAX_PEER_ID);
	u8 retry_count;
};

struct ath12k_wifi8_tx_exc_stats {
	u32 tx_exceptions;
	u32 null_flowq_pkts;
	u32 reinject_pkts;
	u32 invalid_desc;
	u32 vdev_id_check_fail;
	u32 addrx_invalid;
	u32 addrx_timeout;
	u32 msdu_drop;
	u32 illegal_pkts;
	u32 illegal_pkt_hdr;
	u32 peer_ptr_null;
	u32 bank_not_configured;
	u32 msdu_len_err;
	u32 to_sw_pkts;
	u32 parse_err;
	u32 classify_info_sel_exceed;
	u32 bank_id_exceed;
	u32 buf_len_err;
};

struct ath12k_wifi8_rx_stats {
	u32 rx_flush_pkts;
	u32 rx_mgmt_flush_pkts;
};

struct ath12k_wifi8_dp_stats {
	struct ath12k_wifi8_tx_exc_stats tx_exc_stats;
	struct ath12k_wifi8_rx_stats rx_stats;
};

struct ath12k_dp_wifi8 {
	struct ath12k_dp *dp;
	bool cumac;
	atomic_t sam_cmd_num;
	struct dp_srng tx_exception;
	struct dp_srng tcl_cmd_ring;
	struct dp_srng tcl_status_ring;
	struct dp_srng reo_dst_high_prio_ring;
	struct dp_srng wbm_refill_ring[DP_WBM_REFILL_RING_MAX];
	struct dp_srng wbm_idle_buf_ring;
	struct dp_srng reo_high_prio_cmd_ring;
	struct dp_srng tqm_cmd_ring;
	struct dp_srng tqm_status_ring;
	struct dp_srng fse_cmd_ring;
	struct dp_srng sam_cmd_ring;
	struct dp_srng sam_status_ring;
	struct dp_srng rx_ase_cmd_ring;
	struct dp_srng rx_ase_status_ring;
	struct dp_srng reo_flush_ring;
	struct ath12k_wifi8_dp_stats stats;

	/* SAM command ring staging in words */
	u32 *sam_cmd_staging;
	struct dp_srng ppe2wbm_refill_ring[DP_PPE2WBM_REFILL_RING_MAX];
	struct dp_srng ppe2wbm_idle_buf_ring;
};

struct ath12k_dp_hw_group_wifi8 {
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp *cumac_dp;
	struct ath12k_dp_global_ast_table dp_ast_base;
	struct ath12k_pn_page_info *pn_page_info;
	struct pool_ctxt_t *msduq_ctxt;
	struct pool_ctxt_t *sw_msduq_ctxt;
	struct pool_ctxt_t *mpduq_ctxt;
	struct pool_ctxt_t *sw_mpduq_ctxt;
	/* lock for tx flow pool */
	spinlock_t tx_pool_lock;
	u8 num_pn_pages;
	struct completion peer_init_done;

	struct ath12k_dp_htt_cmd_retry_info retry_info[ATH12K_GROUP_MAX_RADIO];
	/* lock for htt cmd retry info */
	spinlock_t htt_cmd_retry_lock;
	struct delayed_work dp_htt_retry_dwork;
	atomic_t retry_work_active;
	struct timer_list hw_grp_timer;
	/* lock for hw grp timer */
	spinlock_t hw_grp_timer_lock;
	struct list_head timer_list_head;
	u32 current_timer_val;
	u32 timer_entry_count;
	struct list_head mec_entry_list_head;
	u16 mec_timer_key;

	DECLARE_BITMAP(msduq_sam_id_alloc_map, MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED);
	DECLARE_BITMAP(mpduq_sam_id_alloc_map, MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED);
	u16 last_msduq_sam_id;
	u16 last_mpduq_sam_id;
	/* lock for sam id alloc map*/
	spinlock_t sam_id_lock;
};

struct dp_hw_grp_timer_entry {
	void *arg;
	u32 timeout_ms;
	u8 scaling_factor;
	u8 counter;
	u16 key_value;
	struct list_head list;
	void (*cmd_callback)(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			     void *arg);
};

struct ath12k_dp_hw_grp_timer_entry_param {
	u32 timeout_ms;
	void *arg;
	void (*callback)(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			 void *arg);
	u16 key_value;
};

void ath12k_dp_hw_group_timer_fn(struct timer_list *timer);
int ath12k_dp_hw_group_add_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
				       struct ath12k_dp_hw_grp_timer_entry_param *param);
bool ath12k_dp_hw_group_del_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
					u16 key_value);

static inline struct ath12k_dp_wifi8 *ath12k_get_dp_wifi8(struct ath12k_dp *dp)
{
	return (struct ath12k_dp_wifi8 *)dp->arch_data;
}

static inline struct ath12k_dp *ath12k_get_dp(struct ath12k_dp_wifi8 *dp_wifi8)
{
	return dp_wifi8->dp;
}

static inline struct ath12k_dp_hw_group_wifi8 *
		ath12k_get_dp_hw_group_wifi8(struct ath12k_dp_hw_group *dp_hw_grp)
{
	return (struct ath12k_dp_hw_group_wifi8 *)dp_hw_grp->arch_data;
}

static inline struct ath12k_dp_hw_group *
		ath12k_get_dp_hw_group(struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8)
{
	return dp_hw_group_wifi8->dp_hw_grp;
}

static inline struct ath12k_base *
		ath12k_dp_get_ab_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->ab;
}

static inline struct device *
		ath12k_dp_get_dev_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->dev;
}

static inline struct ath12k_dp *ath12k_get_central_dp(struct ath12k_dp *dp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp;
}


struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp);
void ath12k_wifi8_srng_hw_ring_disable(struct ath12k_base *ab);
#endif
