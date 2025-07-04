/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "sdwf.h"

#ifndef ATH12K_QOS_H
#define ATH12K_QOS_H

/* Min throughput limit 0 - 10 Gb/s
 * Granularity: 1 Kb/s
 */
#define QOS_MIN_MIN_THROUGHPUT 0
#define QOS_MAX_MIN_THROUGHPUT (10 * 1024 * 1024)

/* Max throughput limit 0 - 10 Gb/s.
 * Granularity: 1 Kb/s
 */
#define QOS_MIN_MAX_THROUGHPUT 0
#define QOS_MAX_MAX_THROUGHPUT (10 * 1024 * 1024)

/* Service interval limit 0 - 10 secs.
 * Granularity: 100 µs
 */
#define QOS_MIN_SVC_INTERVAL 0
#define QOS_MAX_SVC_INTERVAL (10 * 100 * 100)

/* Burst size 0 - 16 MB.
 * Granularity: 1 Byte.
 */
#define QOS_MIN_BURST_SIZE 0
#define QOS_MAX_BURST_SIZE (16 * 1024 * 1024)

/* Delay bound limit 0 - 10 secs
 * Granularity: 100 µs
 */
#define QOS_MIN_DELAY_BOUND 0
#define QOS_MAX_DELAY_BOUND (10 * 100 * 100)

/* Msdu TTL limit 0 - 10 secs.
 * Granularity: 100 µs
 */
#define QOS_MIN_MSDU_TTL 0
#define QOS_MAX_MSDU_TTL (10 * 100 * 100)

/* Priority limit 0 - 127.
 * Higher the numerical value, higher is the priority.
 */
#define QOS_MIN_PRIORITY 0
#define QOS_MAX_PRIORITY 127

/* TID limit 0 - 7
 */
#define QOS_MIN_TID 0
#define QOS_MAX_TID 7

/* MSDU Loss Rate limit 0 - 100%.
 * Granularity: 0.01%
 */
#define QOS_MIN_MSDU_LOSS_RATE 0
#define QOS_MAX_MSDU_LOSS_RATE 10000

enum QOS_PARAM_DEFAULTS {
	QOS_PARAM_DEFAULT_MIN_THROUGHPUT = 0,
	QOS_PARAM_DEFAULT_MAX_THROUGHPUT = 0xffffffff,
	QOS_PARAM_DEFAULT_BURST_SIZE     = 0,
	QOS_PARAM_DEFAULT_SVC_INTERVAL   = 0xffffffff,
	QOS_PARAM_DEFAULT_DELAY_BOUND    = 0xffffffff,
	QOS_PARAM_DEFAULT_TIME_TO_LIVE   = 0xffffffff,
	QOS_PARAM_DEFAULT_PRIORITY       = 0,
	QOS_PARAM_DEFAULT_TID            = 0xff,
	QOS_PARAM_DEFAULT_MSDU_LOSS_RATE = 0,
	QOS_PARAM_DEFAULT_UL_OFDMA_DISABLE = 0,
	QOS_PARAM_DEFAULT_UL_MU_MIMO_DISABLE = 0,
};

#define QOS_DL_PROFILE_MAX 128
#define QOS_UL_PROFILE_MAX 128

#define QOS_PROFILES_MAX (QOS_DL_PROFILE_MAX + QOS_UL_PROFILE_MAX)

/* For 11be SCS, QoS profiles are created to store all
 * the QoS characteristics elements and assigned a
 * QoS profile ID.
 *
 * QOS_DL_PROFILE_MAX QoS profiles supported for DL
 *
 * The QoS Profile ID will be used to fetch the QoS characteristics
 * for 11be QoS SCS
 *
 * For Legacy SCS, only TID is used as QoS parameter, Qos Profile is
 * not created, but only QoS Profile ID is used to derive the TID
 *
 * The following IDs are used for identifying the QoS Profiles *
 * DL QoS Profile IDs	:   0 - 127
 * UL QoS Profile IDs	: 128 - 255
 * Legacy DL Profile IDs: 256 - 263
 */

#define QOS_DL_ID_MIN 0
#define QOS_DL_ID_MAX (QOS_DL_PROFILE_MAX - 1)

#define QOS_UL_ID_MIN (QOS_DL_ID_MAX + 1)
#define QOS_UL_ID_MAX (QOS_UL_ID_MIN + QOS_UL_PROFILE_MAX - 1)

#define QOS_LEGACY_DL_MAX_TID 8
#define QOS_LEGACY_DL_ID_MIN (QOS_UL_ID_MAX + 1)
#define QOS_LEGACY_DL_ID_MAX (QOS_UL_ID_MAX + QOS_LEGACY_DL_MAX_TID)

#define QOS_ID_MAX  (QOS_LEGACY_DL_ID_MAX + 1)

#define QOS_ID_INVALID QOS_ID_MAX
#define QOS_INVALID_TID 0xFF

struct ath12k_qos_params {
	u8 priority;
	u8 tid;
	u32 min_service_interval;
	u32 max_service_interval;
	u32 min_data_rate;
	u32 delay_bound;
	u16 max_msdu_size;
	u32 serv_start_time;
	u8 serv_start_time_link_id;
	u32 mean_data_rate;
	u32 burst_size;
	u32 msdu_life_time;
	u32 msdu_delivery_info;
	u16 medium_time;
	/* Params for Internal debug */
	bool ul_ofdma_disable;
	bool ul_mu_mimo_disable;
};

struct ath12k_qos {
	u8 ref_count;
	struct ath12k_qos_params params;
};

struct ath12k_qos_ctx {
	struct ath12k_sdwf_svc svc_class[QOS_PROFILES_MAX];
	struct ath12k_qos profiles[QOS_PROFILES_MAX];
	/* Lock to protect access to QoS profile data */
	spinlock_t profile_lock;
};

enum qos_profile_dir {
	QOS_PROFILE_DL,
	QOS_PROFILE_UL,
};

struct ath12k_qos_ctx *ath12k_get_qos(struct ath12k_base *ab);

void ath12k_qos_set_default(struct ath12k_qos_params *param);
u8 ath12k_qos_get_tid(struct ath12k_base *ab,
		      u16 id);
bool ath12k_qos_configured(struct ath12k_base *ab, u16 id);

u16 ath12k_qos_get_legacy_id(struct ath12k_base *ab, u8 tid);

u16 ath12k_qos_configure(struct ath12k_base *ab, struct ath12k *ar,
			 struct ath12k_qos_params *params,
			 enum qos_profile_dir qos_dir, u8 *mac_addr);
int ath12k_qos_disable(struct ath12k_base *ab, struct ath12k *ar,
		       enum qos_profile_dir qos_dir,
		       u16 id, u8 *mac_addr);
int ath12k_qos_update(struct ath12k_base *ab, struct ath12k *ar,
		      struct ath12k_qos_params *params,
		      enum qos_profile_dir qos_dir,
		      u16 id, u8 *mac_addr);
#endif /* ATH12K_QOS_H */
