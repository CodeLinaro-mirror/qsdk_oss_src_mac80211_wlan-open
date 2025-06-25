// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_rx.h"
#include "../dp_tx.h"
#include "../hif.h"
#include "../dp_mon.h"
#include "../dp_cmn.h"
#include "dp_rx.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "hal.h"

static int ath12k_wifi7_dp_service_srng(struct ath12k_dp *dp,
					struct ath12k_ext_irq_grp *irq_grp,
					int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0, j;
	int tot_work_done = 0;
	enum dp_monitor_mode monitor_mode;
	u8 ring_mask, rx_mask, tx_mask;

	rx_mask = dp->hw_params->ring_mask->rx[grp_id];
	tx_mask = dp->hw_params->ring_mask->tx[grp_id];

	while (tx_mask) {
		i = fls(tx_mask) - 1;
		tx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_tx_completion_handler(dp, i, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_wbm_rel[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_wbm_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;

		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_mon_dest[grp_id]) {
		monitor_mode = ATH12K_DP_RX_MONITOR_MODE;
		ring_mask = dp->hw_params->ring_mask->rx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_mon_process_ring(dp, id, napi, budget,
								   monitor_mode);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->tx_mon_dest[grp_id]) {
		monitor_mode = ATH12K_DP_TX_MONITOR_MODE;
		ring_mask = dp->hw_params->ring_mask->tx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_mon_process_ring(dp, id, napi, budget,
								   monitor_mode);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->reo_status[grp_id])
		ath12k_wifi7_dp_rx_process_reo_status(dp);

	if (dp->hw_params->ring_mask->host2rxdma[grp_id]) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		LIST_HEAD(list);

		ath12k_dp_rx_bufs_replenish(dp, rx_ring, &list, 0);
	}

	/* TODO: Implement handler for other interrupts */

done:
	return tot_work_done;
}

static int ath12k_wifi7_dp_op_device_init(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_hif_ext_irq_setup(dp->ab, ath12k_wifi7_dp_service_srng, dp);

	return ret;
}

static void ath12k_wifi7_dp_op_device_deinit(struct ath12k_dp *dp)
{
	ath12k_hif_ext_irq_cleanup(dp->ab);
}

static struct ath12k_dp_arch_ops ath12k_wifi7_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi7_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi7_dp_op_device_deinit,
	.dp_tx_get_vdev_bank_config = ath12k_wifi7_dp_tx_get_vdev_bank_config,
	.dp_reo_cmd_send = ath12k_wifi7_dp_reo_cmd_send,
	.setup_pn_check_reo_cmd = ath12k_wifi7_dp_setup_pn_check_reo_cmd,
	.rx_peer_tid_delete = ath12k_wifi7_dp_rx_peer_tid_delete,
	.reo_cache_flush = ath12k_wifi7_dp_reo_cache_flush,
	.rx_link_desc_return = ath12k_wifi7_dp_rx_link_desc_return,
	.peer_rx_tid_reo_update = ath12k_wifi7_peer_rx_tid_reo_update,
	.alloc_reo_qdesc = ath12k_wifi7_dp_alloc_reo_qdesc,
	.peer_rx_tid_qref_setup = ath12k_wifi7_peer_rx_tid_qref_setup,
	.rx_fst_attach = ath12k_wifi7_dp_rx_fst_attach,
	.rx_fst_detach = ath12k_wifi7_dp_rx_fst_detach,
	.rx_flow_dump_entry = ath12k_wifi7_dp_rx_flow_dump_entry,
	.rx_flow_add_entry = ath12k_wifi7_dp_rx_flow_add_entry,
	.rx_flow_delete_entry = ath12k_wifi7_dp_rx_flow_delete_entry,
	.rx_flow_delete_all_entries = ath12k_wifi7_dp_rx_flow_delete_all_entries,
	.dump_fst_table = ath12k_wifi7_dp_dump_fst_table,
	.peer_migrate_reo_cmd = ath12k_wifi7_dp_peer_migrate_reo_cmd,
};

/* TODO: remove export once this file is built with wifi7 ko */
struct ath12k_dp *ath12k_wifi7_dp_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;

	dp = kzalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp->arch_ops = &ath12k_wifi7_dp_arch_ops;

	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hal = &ab->hal;

	return dp;
}

void ath12k_wifi7_dp_deinit(struct ath12k_dp *dp)
{
	kfree(dp);
}
