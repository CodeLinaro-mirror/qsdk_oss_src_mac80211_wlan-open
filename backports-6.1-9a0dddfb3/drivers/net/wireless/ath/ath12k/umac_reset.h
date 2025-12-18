/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_UMAC_RESET_H
#define ATH12K_UMAC_RESET_H

#include "core.h"
#include "dp.h"
#include "hif.h"
#include "debug.h"
bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab);
void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
						   enum dp_umac_reset_tx_cmd tx_event);
/**
 * typedef umac_reset_handler_fn - Function pointer type for UMAC reset handlers
 * @ab: Pointer to ath12k_base structure
 *
 * This function pointer type is used for UMAC reset callback functions
 * that are enqueued for processing during reset operations.
 */
typedef void (*umac_reset_handler_fn)(struct ath12k_base *ab);

/**
 * struct ath12k_umac_reset_task - Task structure for UMAC reset operations
 * @list: List head for task queue
 * @callback: Function to execute for this task
 * @ab: Pointer to ath12k_base structure
 * @event: RX event type for debugging
 * @tx_cmd: TX command to send to target after callback completion
 * @task_id: Unique task identifier used for task_map bitmap tracking
 * @bound_cpu_id: CPU ID to bind this task to, or ATH12K_UMAC_RESET_CPU_UNBOUND
 *
 * This structure is used to queue UMAC reset tasks for multi-core processing.
 * Each task represents a unit of work that needs to be executed during UMAC
 * reset operations, and can be processed on any available CPU core.
 * If bound_cpu_id is set to a specific CPU ID, the task will only be processed
 * by that CPU. If set to ATH12K_UMAC_RESET_CPU_UNBOUND, any CPU can process it.
 */
struct ath12k_umac_reset_task {
	struct list_head list;
	umac_reset_handler_fn callback;
	struct ath12k_base *ab;
	enum dp_umac_reset_recover_action event;
	enum dp_umac_reset_tx_cmd tx_cmd;
	int task_id;
	int bound_cpu_id;
};

/* Macro to indicate task is not bound to any specific CPU */
#define ATH12K_UMAC_RESET_CPU_UNBOUND -1

/**
 * ath12k_umac_reset_tasklet_handler_percpu - Per-CPU tasklet handler
 * @t: Pointer to tasklet structure
 *
 * Handles UMAC reset tasks on a per-CPU basis for better performance.
 */
void ath12k_umac_reset_tasklet_handler_percpu(struct tasklet_struct *t);

/**
 * ath12k_umac_reset_enqueue_task - Enqueue a task for multi-core processing
 * @ag: Pointer to hardware group
 * @callback: Callback function to execute
 * @ab: Pointer to ath12k_base structure
 * @rx_event: RX event type
 * @tx_cmd: TX command type
 * @bound_cpu_id: CPU ID to bind task to, or ATH12K_UMAC_RESET_CPU_UNBOUND
 *
 * Enqueues a UMAC reset task for processing on available CPU cores.
 * If bound_cpu_id is set to a specific CPU, only that CPU will process the task.
 * If set to ATH12K_UMAC_RESET_CPU_UNBOUND, any CPU can process it.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_umac_reset_enqueue_task(struct ath12k_hw_group *ag,
				   umac_reset_handler_fn callback,
				   struct ath12k_base *ab,
				   enum dp_umac_reset_recover_action rx_event,
				   enum dp_umac_reset_tx_cmd tx_cmd,
				   int bound_cpu_id);

void ath12k_dummy_pre_reset_callback(struct ath12k_base *ab);
#endif /*ATH12K_UMAC_RESET_H*/
