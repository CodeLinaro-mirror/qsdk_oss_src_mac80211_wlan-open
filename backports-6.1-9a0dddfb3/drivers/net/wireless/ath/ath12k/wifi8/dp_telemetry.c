// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "dp.h"
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
