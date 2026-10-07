// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * PPDU event notifier framework for ath12k.
 *
 * Provides atomic notifier chain for RX/TX PPDU completion events,
 * and an SRCU notifier chain for extended monitor MPDU delivery,
 * allowing external modules to subscribe to data path events without
 * modifying core driver code. Uses per-event reference counting to
 * enable fast listener checks in hot paths.
 */

#include <linux/notifier.h>
#include "ath12k_notif.h"
#include "debug.h"
#include "dp.h"

/*
 * Global notifier chain for ath12k PPDU events.
 * Uses atomic notifier chain because PPDU notifications are
 * triggered from atomic context.
 */
ATOMIC_NOTIFIER_HEAD(ath12k_ppdu_notifier_chain);
EXPORT_SYMBOL_GPL(ath12k_ppdu_notifier_chain);

ATOMIC_NOTIFIER_HEAD(ath12k_fse_update_notifier_chain);
/* Export notifier chain for FSE listeners */
EXPORT_SYMBOL_GPL(ath12k_fse_update_notifier_chain);

/**
 * ath12k_ppdu_notifier_call_chain - Dispatch PPDU event
 * @val: Event type (ATH12K_EVENT_PPDU_RX_COMPLETE or
 *       ATH12K_EVENT_PPDU_TX_COMPLETE)
 * @v: Event payload (struct ath12k_ppdu_event)
 *
 * Invokes all registered listeners synchronously. The payload
 * is valid only during the callback.
 *
 * Return: NOTIFY_DONE if all listeners processed the event,
 *         NOTIFY_STOP_MASK if any listener requested to stop
 *         the chain
 */
int ath12k_ppdu_notifier_call_chain(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&ath12k_ppdu_notifier_chain, val, v);
}
EXPORT_SYMBOL_GPL(ath12k_ppdu_notifier_call_chain);

int ath12k_fse_update_notif_call_chain(void *v)
{
	return atomic_notifier_call_chain(&ath12k_fse_update_notifier_chain,
					  ATH12K_EVENT_FSE_UPDATE, v);
}

/*
 * PPDU TX notification workqueue.
 *
 * Producer (CE SOFTIRQ, ppdu_list_lock held):
 *   - list_add_tail -> ctx->queue
 *   - queue_depth++  (counts items in ingress queue)
 *   - wiphy_work_queue()
 *
 * Consumer (WQ thread, process context):
 *   - O(1) splice ctx->queue -> local_list under lock; queue_depth reset to 0
 *     at splice time — producer starts counting from 0 again immediately.
 *   - alloc_skb(GFP_KERNEL) + memcpy + notifier_call_chain per item
 *
 * New ingress goes into ctx->queue while the worker drains local_list
 * concurrently.  queue_depth bounds total live items to
 * ATH12K_PPDU_NOTIFY_MAX_DEPTH regardless of which list holds them.
 */
static void ath12k_ppdu_notify_work_fn(struct wiphy *wiphy,
				       struct wiphy_work *work)
{
	struct ath12k_ppdu_notify_ctx *ctx =
		container_of(work, struct ath12k_ppdu_notify_ctx, work);
	struct ath12k_ppdu_notify_item *item, *tmp;
	struct ath12k_ppdu_tx_info *ppdu_evt_data;
	struct ath12k_ppdu_event event;
	struct sk_buff *skb;
	LIST_HEAD(local_list);

	spin_lock_bh(&ctx->lock);
	list_splice_tail_init(&ctx->queue, &local_list);
	ctx->queue_depth = 0;
	spin_unlock_bh(&ctx->lock);

	list_for_each_entry_safe(item, tmp, &local_list, node) {
		list_del(&item->node);

		skb = alloc_skb(sizeof(*ppdu_evt_data), GFP_KERNEL);
		if (unlikely(!skb)) {
			kfree(item->ppdu_info);
			kfree(item);
			continue;
		}

		ppdu_evt_data = skb_put_zero(skb, sizeof(*ppdu_evt_data));
		memcpy(&ppdu_evt_data->ppdu_info, item->ppdu_info,
		       sizeof(struct htt_ppdu_stats_info));
		ppdu_evt_data->device_id = item->device_id;

		kfree(item->ppdu_info);
		kfree(item);

		memset(&event, 0, sizeof(event));
		event.skb = skb;
		atomic_notifier_call_chain(&ath12k_ppdu_notifier_chain,
					   ATH12K_EVENT_PPDU_TX_COMPLETE,
					   &event);
		kfree_skb(skb);
	}
}

/**
 * ath12k_ppdu_notify_ctx_init - initialise TX PPDU notify context
 * @ctx: notify context embedded in ath12k_pdev_dp
 */
void ath12k_ppdu_notify_ctx_init(struct ath12k_ppdu_notify_ctx *ctx)
{
	spin_lock_init(&ctx->lock);
	INIT_LIST_HEAD(&ctx->queue);
	ctx->queue_depth = 0;
	wiphy_work_init(&ctx->work, ath12k_ppdu_notify_work_fn);
}
EXPORT_SYMBOL(ath12k_ppdu_notify_ctx_init);

/**
 * ath12k_ppdu_notify_ctx_deinit - tear down TX PPDU notify context
 * @wiphy: the wiphy whose workqueue hosts the work item
 * @ctx:   notify context embedded in ath12k_pdev_dp
 */
void ath12k_ppdu_notify_ctx_deinit(struct wiphy *wiphy,
				   struct ath12k_ppdu_notify_ctx *ctx)
{
	struct ath12k_ppdu_notify_item *item, *tmp;
	LIST_HEAD(drain_list);

	/* Step 1: cancel pending/running work */
	wiphy_work_cancel(wiphy, &ctx->work);

	/* Step 2: drain ingress queue.
	 * wiphy_work_cancel ensures the worker has finished — local_list inside
	 * the work_fn is fully drained, so only ctx->queue can hold residual items.
	 */
	spin_lock_bh(&ctx->lock);
	list_splice_tail_init(&ctx->queue, &drain_list);
	ctx->queue_depth = 0;
	spin_unlock_bh(&ctx->lock);

	list_for_each_entry_safe(item, tmp, &drain_list, node) {
		list_del(&item->node);
		kfree(item->ppdu_info);
		kfree(item);
	}

	/* Step 3: flush work queued in the splice->cancel window */
	wiphy_work_flush(wiphy, &ctx->work);
}
EXPORT_SYMBOL(ath12k_ppdu_notify_ctx_deinit);
