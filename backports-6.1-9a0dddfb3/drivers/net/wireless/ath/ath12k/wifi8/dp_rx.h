/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_RX_WIFI8_H
#define ATH12K_DP_RX_WIFI8_H

#include "../core.h"
#include "../dp_rx.h"
#include "hal_desc.h"
#include "hal.h"

enum dp_rx_ppeds_wbm_refill_ring {
	PPE2WBM_HW_REFILL_RING = 0,
	PPE2WBM_SW_REFILL_RING = 1,
};

struct dp_rx_fse {
	struct hal_rx_fse *hal_fse;
	u32 flow_hash;
	u32 flow_id;
	u8 reo_indication;
	bool is_valid;
};

int ath12k_wifi8_dp_rx_wbm_buf_ring_init(struct ath12k_base *ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_dp_rx_ppe2wbm_idle_buff_init(struct ath12k_base *ab);
#endif
int ath12k_wifi8_dp_reo_cmd_send(struct ath12k_base *ab,
				 void *data, size_t len,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    struct hal_reo_status *status));
int ath12k_wifi8_dp_reo_cmd_send_highprio(struct ath12k_base *ab,
					  void *data, size_t len,
					  enum hal_reo_cmd_type type,
					  struct ath12k_hal_reo_cmd *cmd,
					  void (*cb)(struct ath12k_dp *dp, void *ctx,
						     struct hal_reo_status *status));
struct ath12k_reo_dp_cmd_desc {
	void *data;
	size_t len;
	void (*cb)(struct ath12k_dp *dp, void *ctx,
		   struct hal_reo_status *status);
};

int ath12k_wifi8_dp_reo_cmd_send_highprio_n(struct ath12k_base *ab,
					    struct ath12k_reo_cmd_entry *entries,
					    struct ath12k_reo_dp_cmd_desc *dp_descs,
					    int n);
int ath12k_wifi8_dp_fse_cmd_send(struct ath12k_base *ab,
				 struct hal_fse_cmd *fse_cmd);
int ath12k_wifi8_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget);
int ath12k_wifi8_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi,
			       int budget);
void ath12k_wifi8_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid);
bool ath12k_wifi8_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src);
int ath12k_wifi8_dp_reo_cache_flush(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid);
int ath12k_wifi8_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn);
void ath12k_wifi8_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id,
					 u16 tid, dma_addr_t paddr);
int ath12k_wifi8_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action);
int ath12k_wifi8_dp_rx_process_reo_status(struct ath12k_dp *dp, int budget);

int ath12k_wifi8_dp_rx_peer_tid_setup(struct ath12k *ar, const u8 *peer_mac, int vdev_id,
				      u8 tid, u32 ba_win_sz, u16 ssn,
				      enum hal_pn_type pn_type);
void ath12k_wifi8_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					    struct ath12k_dp_rx_tid *rx_tid,
					    u32 cipher, enum set_key_cmd key_cmd);
int ath12k_wifi8_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned,
				    u16 stats_id);
int ath12k_wifi8_dp_rxdma_ring_sel_config_qcn9625(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst);
void ath12k_wifi8_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst);

static inline
void ath12k_wifi8_dp_extract_rx_spd_data(struct ath12k_hal *hal,
					 struct hal_rx_spd_data *rx_info,
					 struct hal_rx_desc *rx_desc, int set)
{
	hal->hal_ops->extract_rx_spd_data(rx_info, rx_desc, set);
}

static inline
void ath12k_wifi8_dp_extract_rx_desc_data(struct ath12k_dp *dp,
					  struct hal_rx_desc_data *rx_desc_data,
					  struct hal_rx_desc *rx_desc,
					  struct hal_rx_desc *ldesc)
{
	dp->hw_params->hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

void ath12k_wifi8_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp);
ssize_t ath12k_wifi8_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size);
int ath12k_wifi8_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id);
void ath12k_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
			       struct hal_reo_status *status);
void ath12k_wifi8_dp_rx_ring_free(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_ring_setup(struct ath12k_base *ab);
int ath12k_wifi8_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi8_dp_pdev_free(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_flow_fse_cache_operation(struct ath12k_base *ab,
						enum dp_flow_fst_operation op_code,
						struct hal_flow_tuple_info *tuple_info);
int ath12k_wifi8_dp_rx_process_reo_flush_err(struct ath12k_dp *dp, int budget);
int
ath12k_wifi8_peer_rx_tid_reo_update_for_smd(struct ath12k_base *ab,
					    struct ath12k_dp_hw *dp_hw,
					    const u8 *peer_addr,
					    struct ath12k_rx_smd_ctx_per_tid *rx_tid_ctx);
int ath12k_wifi8_peer_rx_tid_svld_reset(struct ath12k_base *ab,
					struct ath12k_dp_hw *dp_hw,
					const u8 *peer_addr);
void ath12k_wifi8_peer_rx_tid_reo_clear_vld_cmd_init(struct ath12k_dp_rx_tid *rx_tid,
						     struct ath12k_hal_reo_cmd *cmd);
int ath12k_wifi8_peer_rx_tid_reo_clear_vld(struct ath12k_base *ab,
					   struct ath12k_dp_hw *dp_hw,
					   const u8 *peer_addr,
					   u8 tid);
/* Module parameter: controls whether REO VLD is cleared after fetching SMD ctx.
 * Declared in wifi8/core.c; extern here so dp.c and any future SMD callers
 * can gate their behaviour without adding new function arguments.
 */
extern bool ath12k_wifi8_clear_vld_after_smd_ctx_fetch;
extern bool ath12k_wifi8_smd_skip_bitmap_update;
int ath12k_wifi8_dp_rx_ase_htt_srng_setup(struct ath12k_base *ab);

/* SMD BSS Transition: Rx Q Info park / restore */
int ath12k_wifi8_dp_smd_prep_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    const u8 *addr);
int ath12k_wifi8_dp_smd_exec_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    const u8 *addr);
void ath12k_wifi8_dp_smd_clear_old_peer_rx_lut(struct ath12k_dp *dp,
					       struct ath12k_dp_peer *dp_peer);
#endif
