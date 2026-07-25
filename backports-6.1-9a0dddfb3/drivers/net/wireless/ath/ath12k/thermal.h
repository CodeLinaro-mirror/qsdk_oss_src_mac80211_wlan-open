/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#include <soc/qcom/eawtp.h>
#include "core.h"
#endif

#ifndef _ATH12K_THERMAL_
#define _ATH12K_THERMAL_
#include <linux/mutex.h>

#define ATH12K_THERMAL_SYNC_TIMEOUT_HZ (5 * HZ)

#define ATH12K_THERMAL_DEFAULT_DUTY_CYCLE 100
#define ATH12K_THERMAL_THROTTLE_MAX 100

/**
 * enum ath12k_thermal_cfg_idx - index into the per-chipset thermal level table
 *
 * The upstream design provides two generic entries (IPA and XFEM). The
 * backport extends this with per-chipset variants for IPQ5424 and QCN9625
 * which have different thermal operating ranges.
 */
enum ath12k_thermal_cfg_idx {
	/* Internal Power Amplifier Device - generic */
	ATH12K_TT_CFG_IDX_IPA,
	/* External Power Amplifier Device or External Front End Module - generic */
	ATH12K_TT_CFG_IDX_XFEM,
	/* XFEM variant for IPQ5424 (higher operating temperature range) */
	ATH12K_TT_CFG_IDX_XFEM_IPQ5424,
	/* IPA variant for IPQ5424 (higher operating temperature range) */
	ATH12K_TT_CFG_IDX_IPA_IPQ5424,
	/* XFEM variant for QCN9625 (lower operating temperature range) */
	ATH12K_TT_CFG_IDX_XFEM_QCN9625,
	/* IPA variant for QCN9625 (lower operating temperature range) */
	ATH12K_TT_CFG_IDX_IPA_QCN9625,
	ATH12K_TT_CFG_IDX_MAX,
};

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#define ETH_PORT_COUNT 3
#define ATH12K_DEFAULT_POWER_REDUCTION 0xFF
#define ACTIVE_PDEV_TH 2
#endif

struct ath12k_thermal {
	struct completion wmi_sync;

	/* temperature value in Celsius degree protected by data_lock. */
	int temperature;
	struct device *hwmon_dev;
	const struct ath12k_wmi_tt_level_config_param *tt_level_configs;
	struct thermal_cooling_device *cdev;
	/* Serialize thermal operations and hwmon reads */
	struct mutex lock;
	u32 throttle_state;
};

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
/* Enum to indicate DBS states */
enum ath12k_dbs_in_out_state {
	DBS_OUT,
	DBS_IN,
};

struct ath12k_ps_context {
	u8 num_active_pdev;
	u8 num_active_port;
	struct ath12k_hw_group *ag;
	enum ath12k_dbs_in_out_state dbs_state;
	eawtp_get_num_active_ports_cb_t get_actv_eth_ports_cb;
};

enum ath12k_ps_metric_change {
	PS_WLAN_DBS_CHANGE = 0,
	PS_ETH_PORT_CHANGE = 1,
	PS_METRIC_CHANGE_MAX,
};
#endif

void ath12k_ath_update_active_pdev_count(struct ath12k *ar);

#if IS_REACHABLE(CONFIG_THERMAL)
int ath12k_thermal_register(struct ath12k_base *ab);
void ath12k_thermal_unregister(struct ath12k_base *ab);
void ath12k_thermal_event_temperature(struct ath12k *ar, int temperature);
int ath12k_thermal_throttling_config_default(struct ath12k *ar);
void ath12k_thermal_init_configs(struct ath12k *ar);
int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state);
#else
static inline int ath12k_thermal_register(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_thermal_unregister(struct ath12k_base *ab)
{
}

static inline void ath12k_thermal_event_temperature(struct ath12k *ar,
						    int temperature)
{
}

static inline int ath12k_thermal_throttling_config_default(struct ath12k *ar)
{
	return 0;
}

static inline void ath12k_thermal_init_configs(struct ath12k *ar)
{
}

static inline int ath12k_thermal_set_throttling(struct ath12k *ar,
						u32 throttle_state)
{
	return 0;
}
#endif
#endif /* _ATH12K_THERMAL_ */
