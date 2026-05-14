/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_UMAC_RESET_H
#define ATH12K_WIFI8_UMAC_RESET_H

#include "../core.h"
#include "../umac_reset.h"

/* WiFi8-specific UMAC reset function declarations */
void ath12k_wifi8_umac_reset_handle_pre_reset(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_start(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab);
void ath12k_wifi8_umac_reset_handle_init_recovery(struct ath12k_base *ab);
int ath12k_cumac_hw_pre_reset(struct ath12k_base *ab);
int ath12k_cumac_hw_post_reset(struct ath12k_base *ab);
int ath12k_cumac_hw_reset(struct ath12k_base *ab);

/* CUMAC HW Reset Step Enumerations */

/**
 * enum cumac_hw_pre_reset_steps - Pre-reset sequence steps
 *
 * Defines the ordered steps for preparing hardware before UMAC reset
 */
enum {
	CUMAC_HW_PRE_RESET_START,
	CUMAC_HW_PRE_RESET_RXDMA_PREFETCH_DISABLE,
	CUMAC_HW_PRE_RESET_HALT_MLO_DOORBELLS,
	CUMAC_HW_PRE_RESET_PAUSE_GLOBAL_WSI,
	CUMAC_HW_PRE_RESET_DMAC_PMAC_DECOUPLE,
	CUMAC_HW_PRE_RESET_HALT_TCL,
	CUMAC_HW_PRE_RESET_DISABLE_SAM,
	CUMAC_HW_PRE_RESET_PAUSE_TQM,
	CUMAC_HW_PRE_RESET_DISABLE_WBM,
	CUMAC_HW_PRE_RESET_DISABLE_REO,
	CUMAC_HW_PRE_RESET_END,
	CUMAC_HW_PRE_RESET_MAX,
};

/**
 * enum cumac_hw_reset_steps - Reset sequence steps
 *
 * Defines the ordered steps for performing UMAC hardware reset
 */
enum {
	CUMAC_HW_RESET_START,
	CUMAC_HW_RESET_PREREQUISITES,
	CUMAC_HW_RESET_PRE_RING_RESET,
	CUMAC_HW_RESET_APPLY_SOFT_RESET,
	CUMAC_HW_RESET_POST_RING_RESET,
	CUMAC_HW_RESET_END,
	CUMAC_HW_RESET_MAX,
};

/**
 * enum cumac_hw_post_reset_steps - Post-reset sequence steps
 *
 * Defines the ordered steps for restoring hardware after UMAC reset
 */
enum {
	CUMAC_HW_POST_RESET_START,
	CUMAC_HW_POST_RESET_CLEAR_INTERRUPTS,
	CUMAC_HW_POST_RESET_ENABLE_WBM,
	CUMAC_HW_POST_RESET_ENABLE_REO,
	CUMAC_HW_POST_RESET_UNPAUSE_GLOBAL_WSI,
	CUMAC_HW_POST_RESET_UNHALT_MLO_DOORBELLS,
	CUMAC_HW_POST_RESET_ENABLE_RXDMA_PREFETCH,
	CUMAC_HW_POST_RESET_UNHALT_TCL,
	CUMAC_HW_POST_RESET_ENABLE_TQM,
	CUMAC_HW_POST_RESET_ENABLE_SAM,
	CUMAC_HW_POST_RESET_END,
	CUMAC_HW_POST_RESET_MAX,
};

/**
 * struct ath12k_cumac_hw_reset_timestamps - CUMAC HW reset timestamp tracking
 *
 * Stores per-step timestamps for each phase of the CUMAC HW reset process,
 * indexed by the corresponding step enum values.
 */
struct ath12k_cumac_hw_reset_timestamps {
	u64 cumac_hw_pre_reset_ts[CUMAC_HW_PRE_RESET_MAX];
	u64 cumac_hw_reset_ts[CUMAC_HW_RESET_MAX];
	u64 cumac_hw_post_reset_ts[CUMAC_HW_POST_RESET_MAX];
};

/**
 * typedef cumac_hw_reset_fn - Function pointer type for reset step handlers
 * @ab: Pointer to ath12k_base structure
 *
 * Return: 0 on success, negative error code on failure
 */
typedef int (*cumac_hw_reset_fn)(struct ath12k_base *ab, u32 arg);

/**
 * struct cumac_hw_reset_step - Descriptor for a single reset step
 * @fn: Function pointer to execute for this step
 * @name: Human-readable name of the step for logging
 */
struct cumac_hw_reset_step {
	cumac_hw_reset_fn fn;
	u32 arg;
	const char *name;
};

#endif /* ATH12K_WIFI8_UMAC_RESET_H */
