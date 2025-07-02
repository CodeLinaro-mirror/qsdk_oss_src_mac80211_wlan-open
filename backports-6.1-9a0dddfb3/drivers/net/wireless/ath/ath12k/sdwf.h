/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ATH12K_SDWF_H
#define ATH12K_SDWF_H

#include "ath/ath_dp_accel_cfg.h"

#define SDWF_PEER_MSDUQ_INVALID 0xFFFF

#define SDWF_VALID		1
#define SDWF_VALID_MASK		BIT(22)
#define SDWF_VALID_TAG		0xAA

#define SDWF_TAG_ID		GENMASK(31, 24)
#define SDWF_PEER_MSDUQ_ID	GENMASK(15, 0)
#define SDWF_PEER_ID		GENMASK(15, 6)
#define SDWF_MSDUQ_ID		GENMASK(5, 0)

#define SCS_SVC_ID_MASK		GENMASK(15, 8)
#define SCS_PROTOCOL_MASK	GENMASK(7, 0)

struct ath12k_sdwf_svc {
	u16 dl_qos_id;
	bool configured;
	u16 ul_qos_id;
};

int ath12k_sdwf_map_service_class(struct ath12k_base *ab, u16 svc_id,
				  u16 dl_qos_id, u16 ul_qos_id);
int ath12k_sdwf_unmap_service_class(struct ath12k_base *ab, u16 svc_id);
bool ath12k_sdwf_service_configured(struct ath12k_base *ab, u16 svc_id);

u16 ath12k_sdwf_get_dl_qos_id(struct ath12k_base *ab, u16 svc_id);
u16 ath12k_sdwf_get_ul_qos_id(struct ath12k_base *ab, u16 svc_id);

u32 ath_encode_sdwf_metadata(u16 msduq_peer);

u16 ath12k_sdwf_get_msduq_peer(struct wireless_dev *wdev, u8 *peer_mac,
			       struct sawf_param *dl_params,
			       bool scs_mscs);

struct ath12k *ath12k_sdwf_get_ar_from_vif(struct wireless_dev *wdev,
					   struct ieee80211_vif *vif,
					   u8 *peer_mac, u16 *peer_id);
#endif
