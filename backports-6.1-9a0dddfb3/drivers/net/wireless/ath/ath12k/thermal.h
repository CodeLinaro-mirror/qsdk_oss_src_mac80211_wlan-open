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

#define ATH12K_THERMAL_TEMP_LOW_MARK -100
#define ATH12K_THERMAL_TEMP_HIGH_MARK 150
#define ATH12K_THERMAL_THROTTLE_MAX     100
#define ATH12K_THERMAL_DEFAULT_DUTY_CYCLE 100
#define ATH12K_HWMON_NAME_LEN           15
#define ATH12K_THERMAL_SYNC_TIMEOUT_HZ (5 * HZ)
#define THERMAL_LEVELS  1

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#define ETH_PORT_COUNT 3
#define ATH12K_DEFAULT_POWER_REDUCTION 0xFF
#define ACTIVE_PDEV_TH 2
#endif

struct ath12k_thermal {
	struct thermal_cooling_device *cdev;
	struct completion wmi_sync;
	struct device *hwmon_dev;

	/* protected by conf_mutex */
	u32 throttle_state;
	/* temperature value in Celcius degree
	 * protected by data_lock
	 */
	int temperature;
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
};

enum ath12k_ps_metric_change {
	PS_WLAN_DBS_CHANGE = 0,
	PS_ETH_PORT_CHANGE = 1,
	PS_METRIC_CHANGE_MAX,
};
#endif

void ath12k_ath_update_active_pdev_count(struct ath12k *ar);

#if IS_REACHABLE(CONFIG_THERMAL)
int ath12k_thermal_register(struct ath12k_base *sc);
void ath12k_thermal_unregister(struct ath12k_base *sc);
int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state);
void ath12k_thermal_event_temperature(struct ath12k *ar, int temperature);
#else
static inline int ath12k_thermal_register(struct ath12k_base *sc)
{
	return 0;
}

static inline void ath12k_thermal_unregister(struct ath12k_base *sc)
{
}

static inline int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state)
{
	return 0;
}

static inline void ath12k_thermal_event_temperature(struct ath12k *ar,
						    int temperature)
{
}

#endif
#endif /* _ATH12K_THERMAL_ */
