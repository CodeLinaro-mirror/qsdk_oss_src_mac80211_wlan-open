// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "core.h"
#include "vendor_services.h"
#include "telemetry_agent_if.h"
#include "debug.h"

#define AGENT_NOTIFY_EVENT_INIT	0
#define AGENT_NOTIFY_EVENT_DEINIT 1

static struct ath12k_vendor_service_info vendor_info;
#define GET_WIPHY(vendor_info) ((vendor_info).wiphy)
#define GET_WDEV(vendor_info) ((vendor_info).wdev)

static int
(*ath12k_vendor_service_init[ATH12K_RM_MAX_SERVICE])(
	struct ath12k_hw *ah,
	struct ath12k_vendor_service_info *info
);

static int
(*ath12k_vendor_service_deinit[ATH12K_RM_MAX_SERVICE])(
	struct ath12k_hw *ah,
	struct ath12k_vendor_service_info *info
);

static int ath12k_vendor_service_common(struct ath12k_hw *ah,
					struct ath12k_vendor_service_info *info,
					const u8 event_type, bool enable)
{
	if (ath12k_telemetry_notify_vendor_app_event(event_type, info->id,
						     info->service_data))
		return -EINVAL;

	if (event_type == AGENT_NOTIFY_EVENT_DEINIT)
		vendor_info.is_vendor_init_done = false;

	vendor_info.service_enabled[info->id] = enable;
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "vendor svc %s id: %d Enable: %d\n",
		   (event_type == AGENT_NOTIFY_EVENT_INIT) ? "init" : "deinit",
		   info->id, vendor_info.service_enabled[info->id]);

	return 0;
}

static int ath12k_vendor_service_common_init(struct ath12k_hw *ah,
					     struct ath12k_vendor_service_info *info)
{
	if (ath12k_vendor_is_service_enabled(info->id) &&
	    ath12k_vendor_service_common(ah, info, AGENT_NOTIFY_EVENT_DEINIT, false))
		return -EINVAL;

	return ath12k_vendor_service_common(ah, info, AGENT_NOTIFY_EVENT_INIT,
					    true);
}

static int ath12k_vendor_service_common_deinit(struct ath12k_hw *ah,
					       struct ath12k_vendor_service_info *info)
{
	if (!ath12k_vendor_is_service_enabled(info->id))
		return 0;

	return ath12k_vendor_service_common(ah, info,
					    AGENT_NOTIFY_EVENT_DEINIT, false);
}

void ath12k_vendor_create_resources(struct ath12k_vendor_service_info *vendor_info,
				    const u8 id)
{
	struct wiphy *wiphy = GET_WIPHY(*vendor_info);
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ah->radio->ab->ag;

	if (id >= ATH12K_RM_MAX_SERVICE)
		return;

	ath12k_telemetry_create_resources(ag);
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "Created ta resources using service id : %d\n", id);
}

void ath12k_vendor_destroy_resources(struct ath12k_vendor_service_info *vendor_info,
				     const u8 id)
{
	struct wiphy *wiphy = GET_WIPHY(*vendor_info);
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ah->radio->ab->ag;

	if (id >= ATH12K_RM_MAX_SERVICE)
		return;

	ath12k_telemetry_destroy_resources(ag);
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "Destroyed telemetry resources using id: %d\n", id);
}

static int ath12k_vendor_main_service_deinit(struct ath12k_hw *ah,
					     struct ath12k_vendor_service_info *info)
{
	struct ath12k_vendor_service_info tmp_info = {0};
	int id;

	if (!info)
		return -EINVAL;

	vendor_info.is_vendor_init_done = false;
	for (id = 0; id < ATH12K_RM_MAX_SERVICE; id++) {
		tmp_info.id = id;
		if (ath12k_vendor_service_common(ah, &tmp_info,
						 AGENT_NOTIFY_EVENT_DEINIT,
						 false))
			ath12k_err(NULL,
				    "vendor service: %d failed to deinit", id);
	}

	return 0;
}

static int ath12k_vendor_main_service_init(struct ath12k_hw *ah,
					   struct ath12k_vendor_service_info *info)
{
	if (!info)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "main svc init started for id: %d Enable: %d\n",
		   info->id, vendor_info.service_enabled[info->id]);

	if (ath12k_telemetry_is_agent_loaded())
		ath12k_vendor_destroy_resources(&vendor_info, info->id);

	if (vendor_info.is_vendor_init_done) {
		if (ath12k_vendor_service_common(ah, info,
						 AGENT_NOTIFY_EVENT_DEINIT,
						 false))
			return -EINVAL;

		vendor_info.is_vendor_init_done = false;
	}

	ath12k_vendor_create_resources(&vendor_info, info->id);
	if (ath12k_vendor_service_common(ah, info,
					 AGENT_NOTIFY_EVENT_INIT, true))
		return -EINVAL;

	return 0;
}

static int ath12k_vendor_dynamic_service_init(struct ath12k_hw *ah,
					      struct ath12k_vendor_service_info *info)
{
	set_bit(info->id, &vendor_info.dynamic_svc_bitmask);
	if (ath12k_telemetry_dynamic_app_init_deinit_notify(AGENT_NOTIFY_EVENT_INIT,
							    info->id,
							    info->service_data))
		return -EINVAL;
	vendor_info.service_enabled[info->id] = true;
	ath12k_dbg(NULL, ATH12K_DBG_RM, "Dynamic Init service ID: %d Enabled: %d\n",
		   info->id, vendor_info.service_enabled[info->id]);
	return 0;
}

static int ath12k_vendor_dynamic_service_deinit(struct ath12k_hw *ah,
						struct ath12k_vendor_service_info *info)
{
	clear_bit(info->id, &vendor_info.dynamic_svc_bitmask);
	if (ath12k_telemetry_dynamic_app_init_deinit_notify(AGENT_NOTIFY_EVENT_DEINIT,
							    info->id,
							    info->service_data))
		return -EINVAL;

	vendor_info.service_enabled[info->id] = false;
	vendor_info.is_vendor_init_done = false;
	ath12k_dbg(NULL, ATH12K_DBG_RM, "Dynamic De-Init service ID: %d Enabled: %d\n",
		   info->id, vendor_info.service_enabled[info->id]);
	return 0;
}

void ath12k_vendor_services_init(void)
{
	if (!ath12k_mlo_capable)
		return;

	memset(&vendor_info, 0, sizeof(struct ath12k_vendor_service_info));

	INIT_LIST_HEAD(&vendor_info.soc_list);
	mutex_init(&vendor_info.list_lock);

	vendor_info.wq = create_singlethread_workqueue("ath12k_vendor_wq");
	if (!vendor_info.wq) {
		ath12k_err(NULL, "vendor: failed to create vendor_wq for link processing");
		return;
	}

	/* Initialize the service function array */
	ath12k_vendor_service_init[ATH12K_RM_MAIN_SERVICE] =
		ath12k_vendor_main_service_init;
	ath12k_vendor_service_deinit[ATH12K_RM_MAIN_SERVICE] =
		ath12k_vendor_main_service_deinit;
	ath12k_vendor_service_init[ATH12K_RM_ENERGY_SERVICE] =
		ath12k_vendor_service_common_init;
	ath12k_vendor_service_deinit[ATH12K_RM_ENERGY_SERVICE] =
		ath12k_vendor_service_common_deinit;

	/* Initialize other serives as needed */
}
EXPORT_SYMBOL(ath12k_vendor_services_init);

void ath12k_vendor_services_deinit(void)
{
	/* Clean up resources if needed */
	struct ath12k_vendor_soc_device_info *soc_info, *tmp;
	struct ath12k_vendor_service_info info = {0};
	int id;

	if (!ath12k_mlo_capable)
		return;

	info.id = ATH12K_RM_MAIN_SERVICE;
	if (ath12k_vendor_service_deinit[info.id] &&
	    vendor_info.is_vendor_init_done)
		ath12k_vendor_service_deinit[info.id](NULL, &info);

	for (id = 0; id < ATH12K_RM_MAX_SERVICE; id++) {
		ath12k_vendor_service_init[id] = NULL;
		ath12k_vendor_service_deinit[id] = NULL;
	}

	flush_workqueue(vendor_info.wq);
	destroy_workqueue(vendor_info.wq);
	mutex_destroy(&vendor_info.list_lock);
	list_for_each_entry_safe(soc_info, tmp, &vendor_info.soc_list, list) {
		list_del(&soc_info->list);
		kfree(soc_info);
	}
	memset(&vendor_info, 0, sizeof(struct ath12k_vendor_service_info));
}
EXPORT_SYMBOL(ath12k_vendor_services_deinit);

static int ath12k_vendor_set_wireless_references(struct wiphy *wiphy,
					     struct wireless_dev *wdev)
{
	vendor_info.wiphy = wiphy;
	vendor_info.wdev = wdev;

	return 0;
}

int ath12k_vendor_initialize_service(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 struct ath12k_vendor_service_info *info)
{
	int ret;

	if (!info)
		return -EINVAL;

	if (info->id >= ATH12K_RM_MAX_SERVICE) {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Invalid sevice id received: %d\n", info->id);
		return -EINVAL;
	}

	if (!ath12k_vendor_service_init[info->id] ||
	    !ath12k_vendor_service_deinit[info->id]) {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Service id : %d not supported now\n", info->id);
		return -EOPNOTSUPP;
	}

	ath12k_vendor_set_wireless_references(wiphy, wdev);

	switch (info->init_config_type) {
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START:
		ret = ath12k_vendor_service_init[info->id](NULL, info);
		break;
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START:
		ret = ath12k_vendor_dynamic_service_init(NULL, info);
		break;
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP:
		ret = ath12k_vendor_dynamic_service_deinit(NULL, info);
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_RM, "Invalid config init received\n");
		return -EINVAL;
	};

	return ret;
}

