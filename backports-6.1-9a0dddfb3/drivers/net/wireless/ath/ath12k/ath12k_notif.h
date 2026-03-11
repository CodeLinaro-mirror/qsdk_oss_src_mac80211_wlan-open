/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _ATH12K_NOTIF_H_
#define _ATH12K_NOTIF_H_

#include <linux/skbuff.h>
#include <linux/notifier.h>
#include "hal_mon_cmn.h"
#include "dp_htt.h"
#include "debug.h"

/**
 * enum ath12k_event_type - PPDU event types
 * @ATH12K_EVENT_PPDU_RX_COMPLETE: RX PPDU completion
 * @ATH12K_EVENT_PPDU_TX_COMPLETE: TX PPDU completion
 */
enum ath12k_event_type {
	ATH12K_EVENT_PPDU_RX_COMPLETE = 1,
	ATH12K_EVENT_PPDU_TX_COMPLETE,
};

/**
 * struct ath12k_ppdu_rx_info - RX PPDU event payload
 * @ppdu_info: HAL RX monitor PPDU information
 *
 * Embedded in SKB data buffer passed to notifier callbacks.
 */
struct ath12k_ppdu_rx_info {
	struct hal_rx_mon_ppdu_info ppdu_info;
	/* DRIVER-ADDED CONTEXT FIELDS */
} __packed;

/**
 * struct ath12k_ppdu_tx_info - TX PPDU event payload
 * @ppdu_info: HTT TX PPDU statistics
 *
 * Embedded in SKB data buffer passed to notifier callbacks.
 */
struct ath12k_ppdu_tx_info {
	struct htt_ppdu_stats_info ppdu_info;
} __packed;

/**
 * struct ath12k_ppdu_event - PPDU notifier event wrapper
 * @skb: SKB containing PPDU info (ath12k_ppdu_rx_info or
 *       ath12k_ppdu_tx_info depending on event type)
 *
 * Listeners must not hold references to @skb or its data beyond
 * the callback context. The SKB is owned by the driver.
 */
struct ath12k_ppdu_event {
	struct sk_buff *skb;      /* SKB containing PPDU info */
};

int ath12k_register_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
int ath12k_unregister_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
bool ath12k_ppdu_notifier_has_listeners(enum ath12k_event_type event_type);
int ath12k_ppdu_notifier_call_chain(unsigned long val, void *v);

#endif /* _ATH12K_NOTIF_H_ */
