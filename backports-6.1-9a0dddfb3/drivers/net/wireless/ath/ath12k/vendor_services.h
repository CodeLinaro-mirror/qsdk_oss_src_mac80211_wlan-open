/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_VENDOR_SERVICES_H
#define ATH12K_VENDOR_SERVICES_H

#include <linux/types.h>
#include <net/netlink.h>
#include <linux/refcount.h>

#include "core.h"
#include "vendor.h"

enum ath12k_vendor_service_id {
	ATH12K_RM_MAIN_SERVICE = 0,
	ATH12K_UNUSED_RM_PMLO_SERVICE,
	ATH12K_UNUSED_RM_DETSCHED_SERVICE,
	ATH12K_UNUSED_RM_ERP_SERVICE,
	ATH12K_UNUSED_RM_ADMCTRL_SERVICE,
	ATH12K_RM_ENERGY_SERVICE,
	ATH12K_UNUSED_RM_IFLI_PROXY_SERVICE,
	ATH12K_RM_MAX_SERVICE
};

struct ath12k_vendor_service_ops {
	int (*init)(struct ath12k_base *ab, const void *data, size_t len);
	void (*deinit)(struct ath12k_base *ab);
	int (*process_event)(struct ath12k_base *ab, const void *data, size_t len);
};

enum ath12k_link_band_caps {
	LINK_BAND_INVALID,
	LINK_BAND_2GHz,
	LINK_BAND_5GHz,
	LINK_BAND_5GHz_LOW,
	LINK_BAND_5GHz_HIGH,
	LINK_BAND_6GHz,
	LINK_BAND_6GHz_LOW,
	LINK_BAND_6GHz_HIGH,
};

struct ath12k_vendor_link_sm {
	u8 prev_state;
	u8 curr_state;
};

struct ath12k_vendor_link_info {
	u16 hw_link_id;
	u8 link_mac_addr[6];
	u8 chan_bw;
	u16 chan_freq;
	enum ath12k_link_band_caps band_cap;
	u8 tx_chain_mask;
	u8 rx_chain_mask;
	struct ath12k_vendor_link_sm state;
};

struct ath12k_vendor_soc_device_info {
	struct list_head list;
	u8 soc_id;
	u8 num_radios;
	struct ath12k_vendor_link_info link_info[3];
};

struct ath12k_generic_app_info {
	u8 app_version;
	u8 driver_version;
	u8 num_soc_devices;
	struct ath12k_vendor_soc_device_info soc_info[5];
};

struct ath12k_vendor_service_info {
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct ath12k_generic_app_info app_info;
	enum ath12k_vendor_service_id id;
	u64 service_data;
	const struct ath12k_vendor_service_ops *ops;
	unsigned long dynamic_svc_bitmask;
	u8 init_config_type;
	bool service_enabled[ATH12K_RM_MAX_SERVICE];
	bool is_vendor_init_done;
	struct workqueue_struct *wq;
	struct list_head soc_list;
	struct mutex list_lock;
};

void ath12k_vendor_services_init(void);
void ath12k_vendor_services_deinit(void);

int ath12k_vendor_initialize_service(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     struct ath12k_vendor_service_info *info);
bool ath12k_vendor_is_service_enabled(const u8 svc_id);

#endif /* ATH12K_VENDOR_SERVICES_H */
