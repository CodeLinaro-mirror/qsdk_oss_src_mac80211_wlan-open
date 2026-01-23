// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../dp.h"
#include "umac_reset.h"
#include "hal_queue.h"
#include "dp.h"

/* WiFi8-specific UMAC reset implementations - currently empty stubs for future use */

void ath12k_wifi8_umac_reset_handle_init_recovery(struct ath12k_base *ab)
{
	/* Pause TX during UMAC reset */
	set_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

	ath12k_hif_irq_disable(ab);
	ath12k_hif_mgmt_irq_disable(ab);
}

void ath12k_wifi8_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_base *cumac_ab;
	int cpu;

	mlo_umac_reset = &ag->mlo_umac_reset;

	/* Handle only once */
	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);

	/* Schedule dummy tasks on all online CPUs to ensure
	 * no ath12k_wifi8_dp_service_srng instances are running.
	 * Each task is bound to a specific CPU, and only after all
	 * CPU-bound tasks complete will pre_reset_done be sent.
	 */

	for_each_online_cpu(cpu) {
		ath12k_umac_reset_enqueue_task(ag,
					       ath12k_dummy_pre_reset_callback,
					       ab,
					       ATH12K_UMAC_RESET_DO_PRE_RESET,
					       ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
					       cpu);
	}
}

void ath12k_wifi8_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	int i, n_link_desc, ret, j;
	struct hal_srng *srng = NULL;
	struct ath12k_base *cumac_ab;
	struct ath12k_dp *dp;
	unsigned long end;
	struct ath12k *ar;

	if (!ag)
		return;

	mlo_umac_reset = &ag->mlo_umac_reset;

	/* Handle only once */
	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);
	ath12k_dp_srng_hw_ring_disable(cumac_ab);
	ath12k_wifi8_srng_hw_ring_disable(cumac_ab);

	end = jiffies + msecs_to_jiffies(2);

	while (time_before(jiffies, end))
		;

	dp = ath12k_ab_to_dp(cumac_ab);
	ath12k_dp_clear_link_desc_pool(dp);

	/* Cleanup TX queues for all peers after UMAC reset */
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;

			if (!ar)
				continue;

			ret = ath12k_wifi8_cleanup_all_peers_tx_queues(&ar->ah->dp_hw,
								       ag->dp_hw_grp,
								       &ar->dp);
			if (ret)
				ath12k_warn(cumac_ab, "Failed to cleanup TX queues for all peers: %d\n",
					    ret);
			else
				ath12k_dbg(cumac_ab, ATH12K_DBG_BOOT, "Successfully cleaned up TX queues for all peers\n");
		}
	}

	ret = ath12k_wbm_idle_ring_setup(cumac_ab, &n_link_desc);
	if (ret)
		ath12k_warn(cumac_ab, "failed to setup wbm_idle_ring: %d\n", ret);

	srng = &cumac_ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	ret = ath12k_dp_link_desc_setup(cumac_ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret)
		ath12k_warn(cumac_ab, "failed to setup link desc: %d\n", ret);

	ath12k_dp_srng_common_setup(cumac_ab);
	ath12k_wifi8_dp_tx_ring_setup(cumac_ab);
	ath12k_wifi8_dp_rx_ring_setup(cumac_ab);

	ath12k_dp_umac_tx_desc_cleanup(cumac_ab);
	ath12k_dp_umac_rx_desc_cleanup(cumac_ab);

	ath12k_dp_rx_reo_cmd_list_cleanup(cumac_ab);
}

void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	ath12k_hif_irq_enable(ab);
	ath12k_hif_mgmt_irq_enable(ab);

	/* Resume TX during UMAC reset */
	clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);
}
