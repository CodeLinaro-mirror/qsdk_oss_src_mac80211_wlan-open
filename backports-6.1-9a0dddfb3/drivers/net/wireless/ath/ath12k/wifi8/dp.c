// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_rx.h"
#include "../dp_tx.h"
#include "../hif.h"
#include "../dp_cmn.h"
#include "dp_rx.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "hal.h"
#include "dp_peer.h"
#include "dp_ast.h"
#include "dp_htt.h"
#include "dp_tx_flow_info.h"

extern struct ppe_ds_wlan_ops_v2 ppeds_wlanops_v2;
struct ath12k_ppeds_arch_ops ath12k_wifi8_arch_ppeds_ops;

static int ath12k_wifi8_dp_service_srng(struct ath12k_dp *dp,
					struct ath12k_ext_irq_grp *irq_grp,
					int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0;
	int tot_work_done = 0;
	u8 rx_mask, tx_mask;

	rx_mask = dp->hw_params->ring_mask->rx[grp_id];
	tx_mask = dp->hw_params->ring_mask->tx[grp_id];

	while (tx_mask) {
		i = fls(tx_mask) - 1;
		tx_mask ^= 1 << i;
		work_done = ath12k_wifi8_dp_tx_completion_handler(dp, i, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi8_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_wbm_rel[grp_id]) {
		work_done =
			ath12k_wifi8_dp_rx_process_reo_err(dp,
							   HAL_REO_DEST_REL_ERR_RING_NUM,
							   napi, budget);
		budget -= work_done;
		tot_work_done += work_done;

		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi8_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->reo_status[grp_id])
		ath12k_wifi8_dp_rx_process_reo_status(dp);

	if (dp->hw_params->ring_mask->host2rxdma[grp_id]) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		LIST_HEAD(list);
		size_t req_entries;

		req_entries = ath12k_dp_get_req_entries_from_buf_ring(dp->ab,
								      rx_ring, &list);
		if (req_entries)
			ath12k_dp_rx_bufs_replenish(dp, rx_ring, &list);
	}

	/* TODO: Implement handler for other interrupts */

done:
	return tot_work_done;
}

static int ath12k_wifi8_dp_reoq_lut_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	if (!ab->hw_params->reoq_lut_support)
		return 0;

	ret = ath12k_dp_alloc_reoq_lut(ab, &dp->reoq_lut);
	if (ret) {
		ath12k_warn(ab, "failed to allocate memory for reoq table");
		return ret;
	}

	/* Bits in the register have address [39:8] LUT base address to be
	 * allocated such that LSBs are assumed to be zero. Also, current
	 * design supports paddr up to 4 GB max hence it fits in 32 bit register only
	 */
	ath12k_hal_write_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);
	ath12k_hal_write_ml_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);

	ath12k_hal_reoq_lut_addr_read_enable(ab);
	ath12k_hal_reoq_lut_set_max_peerid(ab);

	return 0;
}

static void ath12k_wifi8_dp_reoq_lut_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (dp->reoq_lut.vaddr_unaligned) {
		ath12k_hal_dma_free_coherent(ab->dev, dp->reoq_lut.size,
					     dp->reoq_lut.vaddr_unaligned,
					     dp->reoq_lut.paddr_unaligned);
		dp->reoq_lut.vaddr_unaligned = NULL;
	}
}

static void ath12k_wifi8_dp_umac_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	if (!dp_wifi8->cumac) {
		ath12k_warn(ab, "Skipping ring deinit for non-cumac target");
		return;
	}

	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

	ath12k_ppeds_detach(ab);
	ath12k_dp_cc_cleanup(ab);
	ath12k_wifi8_dp_reoq_lut_cleanup(ab);
	ath12k_dp_deinit_bank_profiles(ab);
	ath12k_wifi8_dp_tx_ring_cleanup(ab);
	ath12k_dp_srng_common_cleanup(ab);

	ath12k_dp_rx_reo_cmd_list_cleanup(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_nss_plugin_unregister_ops(ab);
#endif

	ath12k_wifi8_dp_rx_ring_free(ab);
	ath12k_dp_ast_table_deinit(dp->dp_hw_grp);
	ath12k_dp_pn_counter_page_free(dp->dp_hw_grp);
	ath12k_wifi8_dp_tx_pool_destroy(dp->dp_hw_grp);
}

static int ath12k_wifi8_dp_umac_init(struct ath12k_dp *dp)
{
	int ret;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8;
	struct hal_srng *srng = NULL;
	u32 n_link_desc = 0;
	int i;

	if (!dp_wifi8->cumac) {
		ath12k_warn(ab, "Skipping ring init for non-cumac target");
		return 0;
	}

	INIT_LIST_HEAD(&dp->reo_cmd_list);
	INIT_LIST_HEAD(&dp->reo_cmd_cache_flush_list);
	INIT_LIST_HEAD(&dp->reo_cmd_update_rx_queue_list);
	spin_lock_init(&dp->reo_cmd_update_rx_queue_lock);
	spin_lock_init(&dp->reo_cmd_lock);
	dp->reo_cmd_cache_flush_count = 0;
	dp->idle_link_rbm =
			ath12k_hal_get_idle_link_rbm(&ab->hal, ab->device_id);

	ret = ath12k_wbm_idle_ring_setup(ab, &n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);
		return ret;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	ret = ath12k_dp_link_desc_setup(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup link desc: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_cc_init(ab);

	if (ret) {
		ath12k_warn(ab, "failed to setup cookie converter %d\n", ret);
		goto fail_link_desc_cleanup;
	}

	ret = ath12k_dp_init_bank_profiles(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup bank profiles %d\n", ret);
		goto fail_hw_cc_cleanup;
	}
	ath12k_wifi8_hal_tx_configure_bank_register_default(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_nss_plugin_register_ops(ab);
	if (ret) {
		ath12k_warn(ab, "failed to register nss plugin %d\n", ret);
		goto fail_dp_bank_profiles_cleanup;
	}
#endif

	ret = ath12k_ppeds_attach(ab);
	if (ret) {
		ath12k_warn(ab, "failed to attach PPE DS %d\n", ret);
		goto fail_nss_plugin_unregister;
	}

	ret = ath12k_dp_srng_common_setup(ab);
	if (ret)
		goto fail_ppeds_detach;

	ret = ath12k_wifi8_dp_tx_ring_setup(ab);
	if (ret)
		goto fail_cmn_srng_cleanup;

	ret = ath12k_wifi8_dp_reoq_lut_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup reoq table %d\n", ret);
		goto fail_tx_ring_cleanup;
	}

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		dp->tx_ring[i].tcl_data_ring_id = i;

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		ath12k_hal_tx_set_dscp_tid_map(ab, ath12k_default_dscp_tid_map, i);

	ret = ath12k_wifi8_dp_rx_ring_setup(ab);
	if (ret) {
		ath12k_warn(ab, "rx allod failed ret = %d\n", ret);
		goto fail_dp_rx_free;
	}

	/* Initialize cumac pointer in hw_group */
	dp_hw_group_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	dp_hw_group_wifi8->cumac_dp = dp;

	ret = ath12k_dp_ast_table_init(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(ab, "dp ast table init failed %d\n", ret);
		goto fail_dp_rx_free;
	}

	ret = ath12k_dp_pn_counter_page_init(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(ab, "dp pn counter page init failed %d\n", ret);
		goto fail_ast_table_cleanup;
	}

	ret = ath12k_wifi8_dp_tx_pool_create(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(dp, "dp pool create for queues failed %d\n", ret);
		goto fail_pn_counter_page_free;
	}

	ath12k_info(ab, "CUMAC init successful");
	return 0;
fail_pn_counter_page_free:
	ath12k_dp_pn_counter_page_free(dp->dp_hw_grp);

fail_ast_table_cleanup:
	ath12k_dp_ast_table_deinit(dp->dp_hw_grp);

fail_dp_rx_free:
	ath12k_wifi8_dp_rx_ring_free(ab);
	ath12k_wifi8_dp_reoq_lut_cleanup(ab);

fail_tx_ring_cleanup:
	ath12k_wifi8_dp_tx_ring_cleanup(ab);

fail_cmn_srng_cleanup:
	ath12k_dp_srng_common_cleanup(ab);

fail_ppeds_detach:
	ath12k_ppeds_detach(ab);

fail_nss_plugin_unregister:
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_nss_plugin_unregister_ops(ab);
#endif

#ifndef PLATFORM_SDX85
fail_dp_bank_profiles_cleanup:
#endif
	ath12k_dp_deinit_bank_profiles(ab);

fail_hw_cc_cleanup:
	ath12k_dp_cc_cleanup(ab);

fail_link_desc_cleanup:
	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

	return ret;

}

static int ath12k_wifi8_dp_op_device_init(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_hif_ext_irq_setup(dp->ab, ath12k_wifi8_dp_service_srng, dp);
	if (ret) {
		ath12k_warn(dp, "hif ext irq setup failed %d\n", ret);
		return ret;
	}

	return 0;
}

static void ath12k_wifi8_dp_op_device_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (!ab)
		return;

	ath12k_hif_ext_irq_cleanup(ab);
}

static int ath12k_wifi8_dp_op_mlo_init(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_wifi8_dp_umac_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp umac init failed %d\n", ret);
		return ret;
	}

	return 0;
}

static void ath12k_wifi8_dp_op_mlo_deinit(struct ath12k_dp *dp)
{
	ath12k_wifi8_dp_umac_deinit(dp);
}

static struct ath12k_dp_hw_group *ath12k_wifi8_dp_hw_group_alloc(void)
{
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;

	dp_hw_grp = kzalloc(sizeof(*dp_hw_grp) + sizeof(*dp_hw_grp_wifi8), GFP_KERNEL);
	if (!dp_hw_grp)
		return NULL;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	dp_hw_grp_wifi8->dp_hw_grp = dp_hw_grp;

	return dp_hw_grp;
}

static void ath12k_wifi8_dp_vif_configure(struct ath12k_dp *dp,
					  struct ath12k_vif *ahvif,
					  enum ath12k_dp_op_type optype)
{
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	u32 old_bank_config, new_bank_config;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp *central_dp;
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(dp->dp_hw_grp);
	int bank_id;
	bool mec_support;

	if (!central_ab) {
		ath12k_err(dp->ab, "Central umac not configured unable to dp_vif");
		return;
	}

	central_dp = central_ab->dp;
	if (optype == ATH12K_DP_OP_DEINIT) {
		if (dp_vif->bank_id != DP_INVALID_BANK_ID)
			ath12k_dp_tx_put_bank_profile(central_dp, dp_vif->bank_id);
		return;
	} else if (optype == ATH12K_DP_OP_INIT) {
		/*TODO keep vdev_id check disabled for initial emulation */
		dp_vif->vdev_id_check_en = false;
		ath12k_dp_update_vdev_search(ahvif);

		/* convert vdev params into hal_tx_bank_config */
		new_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config(ab, ahvif, 0,
									  false);
		bank_id = ath12k_dp_tx_get_bank_profile(central_dp, new_bank_config);
		dp_vif->bank_id = bank_id;

		mec_support = test_bit(WMI_SERVICE_MEC_AGING_TIMER_SUPPORT,
				       ab->wmi_ab.svc_map);
		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
		    ath12k_frame_mode == ATH12K_HW_TXRX_ETHERNET && mec_support)
			ath12k_wifi8_hal_vdev_mcast_ctrl_set
				(central_ab, dp_vif->dp_vif_id,
				 HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY_TX);
		else
			/*TODO change this to tqm once mcast tqm path is enabled in FW*/
			ath12k_wifi8_hal_vdev_mcast_ctrl_set
				(central_ab, dp_vif->dp_vif_id,
				 HAL_TX_PACKET_CONTROL_CONFIG_TO_FW_EXCEPTION);

		/* TODO: error path for bank id failure */
		if (bank_id == DP_INVALID_BANK_ID) {
			ath12k_err(central_ab, "Failed to initialize DP TX Banks");
			return;
		}
	} else if (optype == ATH12K_DP_OP_UPDATE) {
		old_bank_config = ath12k_dp_tx_get_bank_config_from_id(central_dp,
								   dp_vif->bank_id);
		new_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config(ab, ahvif,
									  0, false);
		if (old_bank_config != new_bank_config) {
			ath12k_dp_tx_put_bank_profile(central_dp, dp_vif->bank_id);
			bank_id = ath12k_dp_tx_get_bank_profile(central_dp,
								new_bank_config);
			dp_vif->bank_id = bank_id;
		}
		return;
	}
}

static void ath12k_wifi8_dp_link_vif_configure(struct ath12k_dp *dp,
					       struct ath12k_vif *ahvif,
					       u8 link_id,
					       enum ath12k_dp_op_type optype)
{
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_link_vif *arvif = ahvif->link[link_id];
	struct ath12k *ar = arvif->ar;

	if (optype == ATH12K_DP_OP_DEINIT)
		return;

	if (optype == ATH12K_DP_OP_INIT) {
		dp_link_vif->vdev_id = arvif->vdev_id;
		dp_link_vif->lmac_id = ar->lmac_id;
		dp_link_vif->pdev_idx = ar->pdev_idx;

		/* TODO change to dp_vif_id allocation later for
		 * now assign to link vdev_id
		 */
		dp_vif->dp_vif_id = dp_link_vif->vdev_id;

		dp_link_vif->tcl_metadata = u32_encode_bits(1, HTT_TCL_META_DATA_TYPE) |
			u32_encode_bits(arvif->vdev_id,
					HTT_TCL_META_DATA_VDEV_ID) |
			u32_encode_bits(dp_link_vif->pdev_idx,
					HTT_TCL_META_DATA_PDEV_ID);

		/* set HTT extension valid bit to 0 by default */
		dp_link_vif->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;
	}

	/* TODO for now update dp_vif when link params updated */
	if (dp_vif->bank_id == DP_INVALID_BANK_ID)
		ath12k_wifi8_dp_vif_configure(dp, ahvif, ATH12K_DP_OP_INIT);
	else
		ath12k_wifi8_dp_vif_configure(dp, ahvif, ATH12K_DP_OP_UPDATE);
}

static struct ath12k_dp_arch_ops ath12k_wifi8_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi8_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi8_dp_op_device_deinit,
	.dp_op_mlo_init = ath12k_wifi8_dp_op_mlo_init,
	.dp_op_mlo_deinit = ath12k_wifi8_dp_op_mlo_deinit,
	.dp_tx_get_vdev_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config,
	.dp_reo_cmd_send = ath12k_wifi8_dp_reo_cmd_send,
	.setup_pn_check_reo_cmd = ath12k_wifi8_dp_setup_pn_check_reo_cmd,
	.rx_peer_tid_delete = ath12k_wifi8_dp_rx_peer_tid_delete,
	.reo_cache_flush = ath12k_wifi8_dp_reo_cache_flush,
	.rx_link_desc_return = ath12k_wifi8_dp_rx_link_desc_return,
	.peer_rx_tid_reo_update = ath12k_wifi8_peer_rx_tid_reo_update,
	.alloc_reo_qdesc = ath12k_wifi8_dp_alloc_reo_qdesc,
	.peer_rx_tid_qref_setup = ath12k_wifi8_peer_rx_tid_qref_setup,
	.dp_pdev_alloc = ath12k_wifi8_dp_pdev_alloc,
	.dp_pdev_free = ath12k_wifi8_dp_pdev_free,
	.rx_fst_attach = ath12k_wifi8_dp_rx_fst_attach,
	.rx_fst_detach = ath12k_wifi8_dp_rx_fst_detach,
	.rx_flow_dump_entry = ath12k_wifi8_dp_rx_flow_dump_entry,
	.rx_flow_add_entry = ath12k_wifi8_dp_rx_flow_add_entry,
	.rx_flow_delete_entry = ath12k_wifi8_dp_rx_flow_delete_entry,
	.rx_flow_delete_all_entries = ath12k_wifi8_dp_rx_flow_delete_all_entries,
	.dump_fst_table = ath12k_wifi8_dp_dump_fst_table,
	.dp_hw_group_alloc = ath12k_wifi8_dp_hw_group_alloc,
	.peer_migrate_reo_cmd = ath12k_wifi8_dp_peer_migrate_reo_cmd,
	.sdwf_reinject_handler = ath12k_wifi8_sdwf_reinject_handler,
	.dp_tx_ring_setup = ath12k_wifi8_dp_tx_ring_setup,
	.dp_peer_create = ath12k_wifi8_dp_peer_create,
	.dp_peer_delete = ath12k_wifi8_dp_peer_delete,
	.dp_peer_assoc = ath12k_wifi8_dp_peer_assoc,
	.dp_link_peer_create = ath12k_wifi8_dp_link_peer_create,
	.dp_link_peer_delete = ath12k_wifi8_dp_link_peer_delete,
	.peer_cleanup_indication = ath12k_dp_htt_peer_cleanup_indication,
	.dp_ppeds_tx_completion_handler = ath12k_wifi8_ppeds_tx_completion_handler,
	.dp_link_peer_assoc = ath12k_wifi8_dp_link_peer_assoc,
	.dp_get_peer_mgmt_flowq = ath12k_wifi8_get_mgmt_flowq,
	.dp_get_peer_holq = ath12k_wifi8_get_holq,
	.dp_vif_configure = ath12k_wifi8_dp_vif_configure,
	.dp_link_vif_configure = ath12k_wifi8_dp_link_vif_configure,
};

struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_wifi8 *dp_wifi8;
	int ret;

	dp = kzalloc(sizeof(*dp) + sizeof(*dp_wifi8), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp_wifi8 = ath12k_get_dp_wifi8(dp);
	dp_wifi8->dp = dp;

	dp->arch_ops = &ath12k_wifi8_dp_arch_ops;

	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hal = &ab->hal;
	dp->global_peer_id_supported = true;

	ret = ath12k_dp_mon_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp_mon_init failed %d\n", ret);
		goto dp_err;
	}

	/* Temperorily set cumac to true here. This has to be changed later and
	 * will come from CP
	 */
	dp_wifi8 = ath12k_get_dp_wifi8(dp);
	dp_wifi8->cumac = true;

	return dp;
dp_err:
	ath12k_wifi8_dp_deinit(dp);
	return NULL;
}

void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp)
{
	kfree(dp);
}
