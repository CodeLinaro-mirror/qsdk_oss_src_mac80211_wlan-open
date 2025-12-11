// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor dual ring
 * model
 */
#include <linux/if_vlan.h>
#include "../dp_mon.h"
#include "../debug.h"
#include "../dp_mon_filter.h"

const struct ath12k_dp_arch_mon_ops ath12k_wifi8_dp_arch_mon_dual_ring_ops = {
	.rx_srng_setup = ath12k_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_dp_mon_rx_srng_cleanup,
	.rx_buf_setup = ath12k_dp_mon_rx_buf_setup,
	.rx_buf_free = ath12k_dp_mon_rx_buf_free,
	.rx_htt_srng_setup = ath12k_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = ath12k_dp_mon_pdev_rx_srng_setup,
	.mon_pdev_rx_srng_cleanup = ath12k_dp_mon_pdev_rx_srng_cleanup,
	.mon_pdev_rx_htt_srng_setup = ath12k_dp_mon_pdev_rx_htt_srng_setup,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.mon_pdev_rx_mpdu_list_init = ath12k_dp_mon_pdev_rx_mpdu_list_init,
	.rx_filter_alloc = ath12k_dp_mon_rx_filter_alloc,
	.rx_filter_free = ath12k_dp_mon_rx_filter_free,
};
