// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "dp.h"
#include "hal.h"
#include "dp_telemetry.h"

int ath12k_wifi8_dp_telemetry_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	int ret;

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tx_peer_telemetry_ring,
				   HAL_PEER_TX_TELEMETRY, 0, 0,
				   DP_TELEMETRY_TX_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "Peer Tx Telemetry ring setup failed: %d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8->rx_peer_telemetry_ring,
				   HAL_PEER_RX_TELEMETRY, 0, 0,
				   DP_TELEMETRY_RX_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "Peer Rx Telemetry ring setup failed: %d\n",
			    ret);
		goto err;
	}
err:
	return ret;
}

int ath12k_wifi8_dp_telemetry_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tx_peer_telemetry_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_peer_telemetry_ring);

	return 0;
}

/* TODO: add telemetry desc ring handlings */
int ath12k_wifi8_dp_process_tx_peer_telemetry(struct ath12k_dp *dp)
{
	return 0;
}

int ath12k_wifi8_dp_process_rx_peer_telemetry(struct ath12k_dp *dp)
{
	return 0;
}

int ath12k_wifi8_dp_telemetry_init(struct ath12k_dp *dp)
{
	ath12k_wifi8_hal_tasc_peer_clk_cycle_config(dp->ab);
	ath12k_wifi8_hal_tasc_peer_tx_cfg(dp->ab, true);

	ath12k_wifi8_hal_tasc_peer_tx_window(dp->ab,
					     DP_TELEMETRY_PEER_2PEER_WINDOW);
	ath12k_wifi8_hal_tasc_peer_tx_max_peer(
					dp->ab,
					DP_TELEMETRY_MAX_UCAST_PEERS,
					DP_TELEMETRY_MAX_GCAST_DL_PEERS,
					DP_TELEMETRY_MAX_GCAST_UL_PEERS);

	ath12k_wifi8_hal_tasc_peer_tx_fail_drop_default(dp->ab);
	ath12k_wifi8_hal_num_transmission_map_default(dp->ab);

	ath12k_wifi8_hal_tasc_peer_rx_cfg(dp->ab, true);

	ath12k_wifi8_hal_tasc_peer_rx_window(dp->ab,
					     DP_TELEMETRY_PEER_2PEER_WINDOW);
	ath12k_wifi8_hal_tasc_peer_rx_max_peer(
					dp->ab,
					DP_TELEMETRY_MAX_UCAST_PEERS,
					DP_TELEMETRY_MAX_GCAST_DL_PEERS);

	ath12k_wifi8_hal_tasc_peer_rx_fail_drop_default(dp->ab);

	return 0;
}

int ath12k_wifi8_dp_telemetry_deinit(struct ath12k_dp *dp)
{
	ath12k_wifi8_hal_tasc_peer_tx_cfg(dp->ab, false);
	ath12k_wifi8_hal_tasc_peer_rx_cfg(dp->ab, false);
	return 0;
}

int ath12k_wifi8_dp_telemetry_peer_config(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX])
{
	ath12k_wifi8_hal_tasc_peer_tx_set_id(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_tx_band(dp->ab, link_band_id, 1);

	ath12k_wifi8_hal_tasc_peer_rx_set_id(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_rx_band(dp->ab, link_band_id, 1);
	return 0;
}

int ath12k_wifi8_dp_telemetry_peer_delete(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX])
{
	ath12k_wifi8_hal_tasc_reset_peer_tx(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_tx_band(dp->ab, link_band_id, 0);
	ath12k_wifi8_hal_tasc_reset_peer_rx(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_rx_band(dp->ab, link_band_id, 0);
	return 0;
}
