// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../dp.h"
#include "umac_reset.h"

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "../ppe.h"
#endif

static void ath12k_wifi7_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	set_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);
	ath12k_hif_mgmt_irq_disable(ab);

	ath12k_hif_irq_disable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ath12k_dp_ppeds_service_enable_disable(ab, true);
		ath12k_dp_ppeds_interrupt_stop(ab);
		ath12k_dp_ppeds_stop(ab);
		ath12k_dp_ppeds_service_enable_disable(ab, false);
	}
#endif

 /*
  * Memset the wbm link desc pool to 0 at this point, so that by the time
  * FW responds with post_reset_start, we would have finished the memset.
  * This will save a few milliseconds.
  */

	ath12k_dp_clear_link_desc_pool(dp);
}

void ath12k_wifi7_umac_reset_handle_pre_reset_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int cpu;

	ath12k_wifi7_umac_reset_handle_pre_reset(ab);

	/* Schedule dummy tasks on all online CPUs to ensure
	 * no ath12k_wifi7_dp_service_srng instances are running.
	 * Each task is bound to a specific CPU, and only after all
	 * CPU-bound tasks complete will pre_reset_done be sent.
	 */

	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	for_each_online_cpu(cpu) {
		ath12k_umac_reset_enqueue_task(ag,
					       ath12k_dummy_pre_reset_callback,
					       ab,
					       ATH12K_UMAC_RESET_DO_PRE_RESET,
					       ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
					       cpu);  /* Bind to specific CPU */
	}
}

static void ath12k_wifi7_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;
	int i, n_link_desc, ret;
	struct hal_srng *srng = NULL;
	unsigned long end;

	ath12k_dp_srng_hw_ring_disable(ab);

	/* Busy wait for 2 ms to make sure the rings are
	 * in idle state before enabling it
	 */
	end = jiffies + msecs_to_jiffies(2);
	while (time_before(jiffies, end))
		;

	ret = ath12k_wbm_idle_ring_setup(ab, &n_link_desc);

	if (ret)
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);

	dp = ath12k_ab_to_dp(ab);
	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	ret = ath12k_dp_link_desc_setup(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret)
		ath12k_warn(ab, "failed to setup link desc: %d\n", ret);

	ath12k_dp_srng_common_setup(ab);
	dp->arch_ops->dp_tx_ring_setup(ab);
	ath12k_dp_umac_txrx_desc_cleanup(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		ath12k_dp_ppeds_tx_desc_cleanup(ab);
#endif

	ret = ath12k_dp_srng_setup(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_REL_ERR_RING_NUM, 0,
				   DP_RX_RELEASE_RING_SIZE);
	if (ret)
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);

	ath12k_dp_rxdma_ring_setup(ab);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		ret = ath12k_dp_srng_setup(ab, &dp->reo_dst_ring[i],
					   HAL_REO_DST, i, 0,
					   DP_REO_DST_RING_SIZE);
		if (ret)
			ath12k_warn(ab, "failed to setup reo_dst_ring\n");
	}

	ath12k_dp_rx_reo_cmd_list_cleanup(ab);

	ath12k_dp_tid_cleanup(ab);
}

void ath12k_wifi7_umac_reset_handle_post_reset_start_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	ath12k_umac_reset_enqueue_task(ag,
				       ath12k_wifi7_umac_reset_handle_post_reset_start,
				       ab,
				       ATH12K_UMAC_RESET_DO_POST_RESET_START,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);
}

static void ath12k_wifi7_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	ath12k_hif_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ath12k_dp_ppeds_start(ab);
		ath12k_dp_ppeds_interrupt_start(ab);
	}
#endif
	ath12k_hif_mgmt_irq_enable(ab);
	clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);
}

void ath12k_wifi7_umac_reset_handle_post_reset_complete_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	ath12k_umac_reset_enqueue_task(ag,
				       ath12k_wifi7_umac_reset_handle_post_reset_complete,
				       ab,
				       ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);
}
