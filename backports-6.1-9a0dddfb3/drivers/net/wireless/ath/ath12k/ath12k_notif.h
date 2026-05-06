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
 * enum ath12k_event_type - WDI event types
 * @ATH12K_EVENT_PPDU_RX_COMPLETE: RX PPDU completion
 * @ATH12K_EVENT_PPDU_TX_COMPLETE: TX PPDU completion
 * @ATH12K_EVENT_EXT_MON_RX: RX Extended monitor
 * @ATH12K_EVENT_FSE_UPDATE: Flow/Search/Steering Engine update event
 */
enum ath12k_event_type {
	ATH12K_EVENT_PPDU_RX_COMPLETE = 1,
	ATH12K_EVENT_PPDU_TX_COMPLETE,
	ATH12K_EVENT_EXT_MON_RX,
	ATH12K_EVENT_FSE_UPDATE,
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

/**
 * struct ath12k_ext_mon_rx_event - Extended monitor RX notifier event
 * @mpdu: The raw MPDU SKB as captured by the driver.
 *        No radiotap header is prepended by the driver. The rx_status
 *        is stored in the SKB's control buffer (cb) and can be accessed
 *        via IEEE80211_SKB_RXCB(mpdu). If the listener forwards the
 *        frame to mac80211 via ieee80211_rx_ni(), mac80211 reads the
 *        rx_status from the cb and prepends the radiotap header.
 *
 *        The driver frees this SKB after srcu_notifier_call_chain()
 *        returns. Listeners that need to retain the frame MUST call
 *        skb_clone() inside the callback and take ownership of the
 *        clone; they must NOT free or hold a reference to @mpdu itself.
 *
 * @hw: Pointer to the ieee80211_hw instance this frame arrived on.
 *      This pointer is valid ONLY for the duration of the notifier
 *      callback. Listeners MUST NOT store this pointer in any global
 *      or persistent context for use after the callback returns — doing
 *      so results in undefined behaviour as the hardware may be torn
 *      down at any time.
 *
 *      If a listener needs to reference the hardware beyond the
 *      callback lifetime (e.g. from a deferred work item), it must
 *      extract hw->wiphy->perm_addr during the callback and use that
 *      MAC address to look up the corresponding wiphy/ieee80211_hw
 *      safely at the point of use.
 *
 * Passed to every registered callback on the ext_mon RX SRCU notifier
 * chain. The chain fires from rxmon workqueue (process) context;
 * callbacks may sleep.
 */
struct ath12k_ext_mon_rx_event {
	struct sk_buff *mpdu;
	struct ieee80211_hw *hw;
};

/**
 * enum ath12k_fse_op - FSE update operation codes
 * @ATH12K_FSE_OP_ADD: Add flow entry
 * @ATH12K_FSE_OP_DELETE: Delete flow entry
 */
enum ath12k_fse_op {
	ATH12K_FSE_OP_ADD = 0,
	ATH12K_FSE_OP_DELETE = 1,
};

/**
 * struct ath12k_fse_update_event - Payload for ATH12K_EVENT_FSE_UPDATE
 * @src_ip:   Source IP address (4 x u32 in host order;
 *            IPv4 uses [0] only, IPv6 uses all four words)
 * @src_port: Source L4 port number (host order)
 * @dest_ip:  Destination IP address (same layout as @src_ip, host order)
 * @dest_port: Destination L4 port number (host order)
 * @protocol: IP protocol number (e.g. 6 = TCP, 17 = UDP)
 * @version:  IP version (4 or 6)
 * @op:       Operation: %ATH12K_FSE_OP_ADD or %ATH12K_FSE_OP_DELETE
 *
 * Passed as the @v argument to ath12k_fse_update_notifier_call_chain().
 * The struct is stack-allocated by the driver and is valid only during
 * the callback. Listeners must not hold a reference to it beyond the
 * callback context.
 *
 * Note: All IP addresses and port numbers are in host byte order, as
 * ntohl() conversion is already applied by ath12k_dp_rx_ppeds_fse_update_flow_info().
 */
struct ath12k_fse_update_event {
	u32 src_ip[4];
	u32 src_port;
	u32 dest_ip[4];
	u32 dest_port;
	u8  protocol;
	u8  version;
	u8  op;
};

int ath12k_register_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
int ath12k_unregister_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask);
bool ath12k_ppdu_notifier_has_listeners(enum ath12k_event_type event_type);
int ath12k_ppdu_notifier_call_chain(unsigned long val, void *v);
int ath12k_register_ext_mon_rx_notifier(struct notifier_block *nb);
int ath12k_unregister_ext_mon_rx_notifier(struct notifier_block *nb);
bool ath12k_ext_mon_rx_notifier_has_listeners(void);
int ath12k_ext_mon_rx_notifier_call_chain(unsigned long val, void *rx_event);

/* FSE_UPDATE notifier APIs */
int ath12k_register_fse_update_notifier(struct notifier_block *nb,
					 unsigned long event_mask);
int ath12k_unregister_fse_update_notifier(struct notifier_block *nb,
					   unsigned long event_mask);
bool ath12k_fse_update_notif_has_listeners(void);
int ath12k_fse_update_notif_call_chain(void *v);

#endif /* _ATH12K_NOTIF_H_ */
