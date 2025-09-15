/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH_SAWF_H
#define ATH_SAWF_H
#include <linux/netdevice.h>
#include <net/mac80211.h>

#define SAWF_MSDUQ_ID_INVALID   0x3F

struct ath_sawf_callbacks {
	void (*sawf_ul_callback)(struct net_device *dest_ndev,
				 u8 dest_mac[],
				 struct net_device *src_ndev,
				 u8 src_mac[],
				 u8 fwd_service_id,
				 u8 rev_service_id,
				 u8 add_or_sub,
				 u32 fw_mark_metadata,
				 u32 rv_mark_metadata);
};

struct ath_ul_params {
	struct net_device *dst_dev;
	struct net_device *src_dev;
	u8 *dst_mac;
	u8 *src_mac;
	u8 fw_service_id;
	u8 rv_service_id;
	u8 start_or_stop;
	u32 fw_mark_metadata;
	u32 rv_mark_metadata;
};

int ath_sawf_msduq_callback_register(struct ath_sawf_callbacks *ath_cb);
void ath_sawf_msduq_callback_unregister(void);
void ath_sawf_uplink(struct ath_ul_params *ecm_ath_ul_params);
#endif
