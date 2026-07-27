// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "core.h"
#include "debug.h"
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#include <soc/qcom/eawtp.h>
#endif

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
struct ath12k_ps_context ath12k_global_ps_ctx;

uint8_t ath12k_get_number_of_active_eth_ports(void)
{
	struct eawtp_port_info pact_info = {0};

	if (ath12k_global_ps_ctx.get_actv_eth_ports_cb)
		ath12k_global_ps_ctx.get_actv_eth_ports_cb(0, &pact_info);

	return pact_info.num_active_port;
}

static void
ath12k_pdev_notify_power_save_metric(u8 count, u8 idx_map,
				     enum ath12k_ps_metric_change metric)
{
	u8 idx, idx_i;
	u8 update_ps_change = 0;
	u8 power_reduction_dbm = 0;
	struct ath12k *tmp_ar;
	struct ath12k_base *tmp_ab;
	struct ath12k_hw_group *ag = ath12k_global_ps_ctx.ag;
	struct ath12k_pdev *pdev;

	if (ag->eth_power_reduction == ATH12K_DEFAULT_POWER_REDUCTION ||
	    ag->dbs_power_reduction == ATH12K_DEFAULT_POWER_REDUCTION) {
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: dbs_power_reduction and eth_power_reduction values are not set\n");
		return;
	}

	switch (metric) {
	case PS_WLAN_DBS_CHANGE:
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: Change in active pdevs active_pdevs:%u\n",
			   count);
		/* Update to FW about Power Reduction value only when there
		 * is a change in DBS status:
		 * 1. If DBS in, Get Ethernet Port count and check
		 *    a. If Ethernet Port count is lower than TH,
		 *       send dbs_pwr_reduction_dbm to FW
		 *    b. If Ethernet Port count is greter than the TH,
		 *       send dbs_pwr_reduction_dbm + eth_pwr_reduction_dbm
		 *       to FW
		 * 2. If DBS out, update power reduction dbm to 0
		 */
		if (ath12k_global_ps_ctx.num_active_pdev != count) {
			if (count >= ACTIVE_PDEV_TH) {
				/* Set ath12k_global_ps_ctx.num_active_port */
				power_reduction_dbm = ag->dbs_power_reduction;
				if (ath12k_global_ps_ctx.num_active_port > ETH_PORT_COUNT)
					power_reduction_dbm += ag->eth_power_reduction;
				update_ps_change |= (1 << metric);
				ath12k_global_ps_ctx.dbs_state = DBS_IN;
			} else if ((count < ACTIVE_PDEV_TH) &&
				   (ath12k_global_ps_ctx.num_active_pdev >= ACTIVE_PDEV_TH)) {
				update_ps_change |= (1 << metric);
				ath12k_global_ps_ctx.dbs_state = DBS_OUT;
			}
			ath12k_global_ps_ctx.num_active_pdev = count;
		}
		break;
	case PS_ETH_PORT_CHANGE:
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: Change in active ethernet ports active_eth_ports:%u\n",
			   count);
		/* Update Power Reduction value to FW only for DBS IN state
		 * when there is a change in active ethernet port count:
		 * 1. If DBS IN state,
		 *     a. If Ethernet Port count is crossing the TH,
		 *        send dbs_pwr_reduction_dbm + eth_pwr_reduction_dbm
		 *        from ini to FW
		 *     b. If Ethernet Port count is coming below the TH,
		 *        send dbs_pwr_reduction_dbm to FW
		 * 2. If DBS OUT state, No Update
		 */
		if (ath12k_global_ps_ctx.num_active_port != count &&
		    ath12k_global_ps_ctx.dbs_state == DBS_IN) {
			if (ath12k_global_ps_ctx.num_active_port <= ETH_PORT_COUNT &&
			    count > ETH_PORT_COUNT) {
				power_reduction_dbm = ag->eth_power_reduction +
						      ag->dbs_power_reduction;
				update_ps_change |= (1 << metric);
			} else if (ath12k_global_ps_ctx.num_active_port > ETH_PORT_COUNT &&
				   count <= ETH_PORT_COUNT) {
				power_reduction_dbm = ag->dbs_power_reduction;
				update_ps_change |= (1 << metric);
			}
		}
		ath12k_global_ps_ctx.num_active_port = count;
		break;
	default:
		break;
	}

	if (update_ps_change) {
		for (idx = 0; idx < ag->num_hw; idx++) {
			tmp_ab = ag->ab[idx];
			if (!tmp_ab)
				continue;
			for (idx_i = 0; idx_i < tmp_ab->num_radios; idx_i++) {
				if (ath12k_global_ps_ctx.dbs_state == DBS_IN &&
				    !(idx_map & (1 << idx)))
					continue;
				rcu_read_lock();
				pdev = rcu_dereference(tmp_ab->pdevs_active[idx_i]);
				if (!pdev) {
					rcu_read_unlock();
					continue;
				}
				tmp_ar = pdev->ar;
				if (tmp_ar && tmp_ar->num_stations) {
					ath12k_dbg(tmp_ab, ATH12K_DBG_WMI,
						   "Power and Thermal optimization sending WMI command to reduces power power_reduction_dbm:%u\n",
						   power_reduction_dbm);
					ath12k_wmi_pdev_set_param(tmp_ar,
								  WMI_PDEV_PARAM_PWR_REDUCTION_IN_QUARTER_DB,
								  power_reduction_dbm,
								  tmp_ar->pdev->pdev_id);
				}
				rcu_read_unlock();
			}
		}
	}
}

static int
netstandby_eawtp_wifi_notify_active_eth_ports(void *app_data,
					      struct eawtp_port_info *ntfy_info)
{
	struct ath12k *tmp_ar;
	u8 idx, idx_i, active_eth_ports = 0, idx_map = 0;
	struct ath12k_base *ab_tmp;
	struct ath12k_pdev *pdev;
	struct ath12k_hw_group *ag = ath12k_global_ps_ctx.ag;

	if (!ntfy_info) {
		ath12k_info(NULL, "WIFI-Netstandby: Invalid Port Info!");
		return -EINVAL;
	}

	active_eth_ports = ntfy_info->num_active_port;

	for (idx = 0; idx < ag->num_hw; idx++) {
		ab_tmp = ag->ab[idx];

		if (!ab_tmp)
			continue;

		for (idx_i = 0; idx_i < ab_tmp->num_radios; idx_i++) {
			rcu_read_lock();
			pdev = rcu_dereference(ab_tmp->pdevs_active[idx_i]);
			if (pdev && pdev->ar) {
				tmp_ar = ab_tmp->pdevs_active[idx_i]->ar;
				if (tmp_ar && tmp_ar->num_stations)
					idx_map |= (1 << idx);
			}
			rcu_read_unlock();
		}
	}

	ath12k_pdev_notify_power_save_metric(active_eth_ports, idx_map, PS_ETH_PORT_CHANGE);

	return 0;
}

int eawtp_wifi_get_and_register_cb(struct eawtp_reg_info *info)
{
	if (!info)
		return -1;

	ath12k_global_ps_ctx.get_actv_eth_ports_cb = info->get_active_ports_cb;
	ath12k_get_number_of_active_eth_ports();
	info->ntfy_port_status_to_wifi_cb = netstandby_eawtp_wifi_notify_active_eth_ports;

	ath12k_info(NULL, "WIFI-Netstandby_eawtp: WIFI registration complete");

	return 0;
}
EXPORT_SYMBOL(eawtp_wifi_get_and_register_cb);

int eawtp_wifi_unregister_cb(void)
{
	ath12k_global_ps_ctx.get_actv_eth_ports_cb = NULL;

	ath12k_info(NULL, "WIFI-Netstandby_eawtp: WIFI unregistered");

	return 0;
}
EXPORT_SYMBOL(eawtp_wifi_unregister_cb);

void ath12k_ath_update_active_pdev_count(struct ath12k *ar)
{
	struct ath12k_hw_group *ag;
	u8 idx, idx_i, active_pdev = 0, idx_map = 0;
	struct ath12k_base *ab, *ab_tmp;
	struct ath12k_pdev *pdev;

	ab = ar->ab;

	if (!ab)
		ath12k_dbg(NULL, ATH12K_DBG_WMI, "Power and thermal optimization: ab is NULL");

	ag = ab->ag;

	if (!ag)
		return;

	for (idx = 0; idx < ag->num_hw; idx++) {
		ab_tmp = ag->ab[idx];
		if (!ab_tmp)
			continue;
		for (idx_i = 0; idx_i < ab_tmp->num_radios; idx_i++) {
			rcu_read_lock();
			pdev = rcu_dereference(ab_tmp->pdevs_active[idx_i]);
			if (pdev && pdev->ar && pdev->ar->num_stations) {
				active_pdev++;
				idx_map |= (1 << idx);
			}
			rcu_read_unlock();
		}
	}

	ath12k_pdev_notify_power_save_metric(active_pdev, idx_map, PS_WLAN_DBS_CHANGE);
}
#endif

/*
 * Per-chipset thermal level configuration tables.
 *
 * The upstream design provides two generic entries (IPA and XFEM). The
 * backport extends this with per-chipset variants for IPQ5424 and QCN9625
 * which have different thermal operating ranges.
 *
 * IPQ5424 runs hotter (higher thresholds) due to its SoC integration.
 * QCN9625 runs cooler (lower thresholds) due to its power envelope.
 */
static const struct ath12k_wmi_tt_level_config_param
tt_level_configs[ATH12K_TT_CFG_IDX_MAX][ENHANCED_THERMAL_LEVELS] = {
	[ATH12K_TT_CFG_IDX_IPA] = {
		[0] = {	.tmplwm = -100, .tmphwm = 115, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 110, .tmphwm = 120, .dcoffpercent = 0,
			.pout_reduction_db = 12	},
		[2] = { .tmplwm = 115, .tmphwm = 125, .dcoffpercent = 50,
			.pout_reduction_db = 12	},
		[3] = { .tmplwm = 120, .tmphwm = 130, .dcoffpercent = 90,
			.pout_reduction_db = 12	},
		[4] = { .tmplwm = 125, .tmphwm = 130, .dcoffpercent = 100,
			.pout_reduction_db = 12	},
	},
	[ATH12K_TT_CFG_IDX_XFEM] = {
		[0] = {	.tmplwm = -100,	.tmphwm = 105, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 100, .tmphwm = 110, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[2] = { .tmplwm = 105, .tmphwm = 115, .dcoffpercent = 50,
			.pout_reduction_db = 0 },
		[3] = {	.tmplwm = 110, .tmphwm = 120, .dcoffpercent = 90,
			.pout_reduction_db = 0 },
		[4] = { .tmplwm = 115, .tmphwm = 120, .dcoffpercent = 100,
			.pout_reduction_db = 0 },
	},
	[ATH12K_TT_CFG_IDX_XFEM_IPQ5424] = {
		[0] = {	.tmplwm = -100,	.tmphwm = 105, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 100, .tmphwm = 110, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[2] = { .tmplwm = 105, .tmphwm = 120, .dcoffpercent = 50,
			.pout_reduction_db = 0 },
		[3] = { .tmplwm = 110, .tmphwm = 130, .dcoffpercent = 90,
			.pout_reduction_db = 0 },
		[4] = { .tmplwm = 125, .tmphwm = 130, .dcoffpercent = 100,
			.pout_reduction_db = 0 },
	},
	[ATH12K_TT_CFG_IDX_IPA_IPQ5424] = {
		[0] = {	.tmplwm = -100,	.tmphwm = 125, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 120, .tmphwm = 130, .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[2] = { .tmplwm = 125, .tmphwm = 135, .dcoffpercent = 50,
			.pout_reduction_db = 0 },
		[3] = { .tmplwm = 130, .tmphwm = 140, .dcoffpercent = 90,
			.pout_reduction_db = 0 },
		[4] = { .tmplwm = 135, .tmphwm = 140, .dcoffpercent = 100,
			.pout_reduction_db = 0 },
	},
	[ATH12K_TT_CFG_IDX_XFEM_QCN9625] = {
		[0] = {	.tmplwm = -100,	.tmphwm = 95,  .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 90,  .tmphwm = 100, .dcoffpercent = 0,
			.pout_reduction_db = 12	},
		[2] = { .tmplwm = 95,  .tmphwm = 105, .dcoffpercent = 50,
			.pout_reduction_db = 12	},
		[3] = { .tmplwm = 100, .tmphwm = 110, .dcoffpercent = 90,
			.pout_reduction_db = 12	},
		[4] = { .tmplwm = 105, .tmphwm = 115, .dcoffpercent = 100,
			.pout_reduction_db = 12	},
	},
	[ATH12K_TT_CFG_IDX_IPA_QCN9625] = {
		[0] = {	.tmplwm = -100,	.tmphwm = 95,  .dcoffpercent = 0,
			.pout_reduction_db = 0 },
		[1] = { .tmplwm = 90,  .tmphwm = 100, .dcoffpercent = 0,
			.pout_reduction_db = 12	},
		[2] = { .tmplwm = 95,  .tmphwm = 105, .dcoffpercent = 50,
			.pout_reduction_db = 12	},
		[3] = { .tmplwm = 100, .tmphwm = 110, .dcoffpercent = 90,
			.pout_reduction_db = 12	},
		[4] = { .tmplwm = 105, .tmphwm = 115, .dcoffpercent = 100,
			.pout_reduction_db = 12	},
	},
};

static enum ath12k_thermal_cfg_idx ath12k_thermal_cfg_index(struct ath12k *ar)
{
	bool is_ipa = test_bit(WMI_TLV_SERVICE_IS_TARGET_IPA, ar->ab->wmi_ab.svc_map);

	switch (ar->ab->hw_params->hw_rev) {
	case ATH12K_HW_IPQ5424_HW10:
		return is_ipa ? ATH12K_TT_CFG_IDX_IPA_IPQ5424
			      : ATH12K_TT_CFG_IDX_XFEM_IPQ5424;
	case ATH12K_HW_QCN9625_HW10:
		return is_ipa ? ATH12K_TT_CFG_IDX_IPA_QCN9625
			      : ATH12K_TT_CFG_IDX_XFEM_QCN9625;
	default:
		return is_ipa ? ATH12K_TT_CFG_IDX_IPA
			      : ATH12K_TT_CFG_IDX_XFEM;
	}
}

int ath12k_thermal_throttling_config_default(struct ath12k *ar)
{
	struct ath12k_wmi_thermal_mitigation_arg param = {};
	int ret;

	if (test_bit(WMI_TLV_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		param.num_levels = ENHANCED_THERMAL_LEVELS;
	else
		param.num_levels = THERMAL_LEVELS;

	param.levelconf = ar->thermal.tt_level_configs;

	ret = ath12k_wmi_send_thermal_mitigation_cmd(ar, &param);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to send thermal mitigation cmd for default config: %d\n",
			    ret);
	return ret;
}

void ath12k_thermal_init_configs(struct ath12k *ar)
{
	enum ath12k_thermal_cfg_idx cfg_idx;

	cfg_idx = ath12k_thermal_cfg_index(ar);
	ar->thermal.tt_level_configs = &tt_level_configs[cfg_idx][0];
}

static int
ath12k_thermal_get_max_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long *state)
{
	*state = ATH12K_THERMAL_THROTTLE_MAX;

	return 0;
}

static int
ath12k_thermal_get_cur_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long *state)
{
	struct ath12k *ar = cdev->devdata;

	mutex_lock(&ar->thermal.lock);
	*state = ar->thermal.throttle_state;
	mutex_unlock(&ar->thermal.lock);

	return 0;
}

int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state)
{
	struct ath12k_wmi_thermal_mitigation_arg param = {};
	struct ath12k_wmi_tt_level_config_param cfg = {};
	int ret;

	param.num_levels = 1;
	cfg.dcoffpercent = throttle_state;
	param.levelconf = &cfg;

	ret = ath12k_wmi_send_thermal_mitigation_cmd(ar, &param);
	if (ret)
		ath12k_warn(ar->ab, "failed to send thermal mitigation cmd: %d\n",
			    ret);

	return ret;
}

static int
ath12k_thermal_set_cur_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long throttle_state)
{
	struct ath12k *ar = cdev->devdata;

	if (throttle_state > ATH12K_THERMAL_THROTTLE_MAX)
		return -EINVAL;

	scoped_guard(mutex, &ar->thermal.lock) {
		if (ar->thermal.throttle_state == throttle_state)
			return 0;
		ar->thermal.throttle_state = throttle_state;
	}

	if (throttle_state == 0)
		return ath12k_thermal_throttling_config_default(ar);

	return ath12k_thermal_set_throttling(ar, throttle_state);
}

static const struct thermal_cooling_device_ops ath12k_thermal_ops = {
	.get_max_state = ath12k_thermal_get_max_throttle_state,
	.get_cur_state = ath12k_thermal_get_cur_throttle_state,
	.set_cur_state = ath12k_thermal_set_cur_throttle_state,
};

static ssize_t ath12k_thermal_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ath12k *ar = dev_get_drvdata(dev);
	unsigned long time_left;
	int ret, temperature;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	reinit_completion(&ar->thermal.wmi_sync);
	ret = ath12k_wmi_send_pdev_temperature_cmd(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to read temperature %d\n", ret);
		return ret;
	}

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags))
		return -ESHUTDOWN;

	time_left = wait_for_completion_timeout(&ar->thermal.wmi_sync,
						ATH12K_THERMAL_SYNC_TIMEOUT_HZ);
	if (!time_left) {
		ath12k_warn(ar->ab, "failed to synchronize thermal read\n");
		return -ETIMEDOUT;
	}

	spin_lock_bh(&ar->data_lock);
	temperature = ar->thermal.temperature;
	spin_unlock_bh(&ar->data_lock);

	/* display in millidegree celsius */
	return sysfs_emit(buf, "%d\n", temperature * 1000);
}

void ath12k_thermal_event_temperature(struct ath12k *ar, int temperature)
{
	spin_lock_bh(&ar->data_lock);
	ar->thermal.temperature = temperature;
	spin_unlock_bh(&ar->data_lock);
	complete_all(&ar->thermal.wmi_sync);
}

static SENSOR_DEVICE_ATTR_RO(temp1_input, ath12k_thermal_temp, 0);

static struct attribute *ath12k_hwmon_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ath12k_hwmon);

static int ath12k_thermal_setup_radio(struct ath12k_base *ab, int i)
{
	char pdev_name[20];
	struct ath12k *ar;
	int ret;

	ar = ab->pdevs[i].ar;
	if (!ar)
		return 0;

	ar->thermal.cdev =
		thermal_cooling_device_register("ath12k_thermal", ar,
						&ath12k_thermal_ops);
	if (IS_ERR(ar->thermal.cdev)) {
		ret = PTR_ERR(ar->thermal.cdev);
		ar->thermal.cdev = NULL;
		ath12k_err(ar->ab, "failed to register cooling device: %d\n",
			   ret);
		return ret;
	}

	scnprintf(pdev_name, sizeof(pdev_name), "cooling_device%u",
		  ar->hw_link_id);

	ret = sysfs_create_link(&ar->ah->hw->wiphy->dev.kobj,
				&ar->thermal.cdev->device.kobj, pdev_name);
	if (ret) {
		ath12k_err(ab, "failed to create cooling device symlink: %d\n",
			   ret);
		goto unregister_cdev;
	}

	ar->thermal.hwmon_dev =
		hwmon_device_register_with_groups(&ar->ah->hw->wiphy->dev,
						  "ath12k_hwmon", ar,
						  ath12k_hwmon_groups);
	if (IS_ERR(ar->thermal.hwmon_dev)) {
		ret = PTR_ERR(ar->thermal.hwmon_dev);
		ar->thermal.hwmon_dev = NULL;
		ath12k_err(ar->ab, "failed to register hwmon device: %d\n",
			   ret);
		return ret;
		goto remove_sysfs;
	}

	return 0;

remove_sysfs:
	sysfs_remove_link(&ar->ah->hw->wiphy->dev.kobj, pdev_name);
unregister_cdev:
	thermal_cooling_device_unregister(ar->thermal.cdev);
	ar->thermal.cdev = NULL;
	return ret;
}

static void ath12k_thermal_cleanup_radio(struct ath12k_base *ab, int i)
{
	struct ath12k *ar;
	char pdev_name[20];

	ar = ab->pdevs[i].ar;
	if (!ar || !ar->thermal.hwmon_dev)
		return;

	hwmon_device_unregister(ar->thermal.hwmon_dev);
	ar->thermal.hwmon_dev = NULL;

	scnprintf(pdev_name, sizeof(pdev_name), "cooling_device%u",
		  ar->hw_link_id);
	sysfs_remove_link(&ar->ah->hw->wiphy->dev.kobj, pdev_name);

	thermal_cooling_device_unregister(ar->thermal.cdev);
	ar->thermal.cdev = NULL;
}

int ath12k_thermal_register(struct ath12k_base *ab)
{
	int i, ret;

	if (!IS_REACHABLE(CONFIG_HWMON))
		return 0;

	for (i = 0; i < ab->num_radios; i++) {
		ret = ath12k_thermal_setup_radio(ab, i);
		if (ret)
			goto out;
	}

	return 0;
out:
	for (i--; i >= 0; i--)
		ath12k_thermal_cleanup_radio(ab, i);

	return ret;
}

void ath12k_thermal_unregister(struct ath12k_base *ab)
{
	int i;

	if (!IS_REACHABLE(CONFIG_HWMON))
		return;

	for (i = 0; i < ab->num_radios; i++)
		ath12k_thermal_cleanup_radio(ab, i);
}

