// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../dp.h"
#include "umac_reset.h"

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
	/* TODO: Add WiFi8-specific post-reset start handling */
}

void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	ath12k_hif_irq_enable(ab);
	ath12k_hif_mgmt_irq_enable(ab);

	/* Resume TX during UMAC reset */
	clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);
}
