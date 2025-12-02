// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "core.h"
#include "coredump.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "hif.h"
#include "dp.h"
#include "umac_reset.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif

/* State machine transition validation table */
static const bool umac_reset_state_transition_valid
	[ATH12K_UMAC_RESET_STATE_MAX][ATH12K_UMAC_RESET_STATE_MAX] = {
	/* From IDLE */
	[ATH12K_UMAC_RESET_STATE_IDLE] = {
		[ATH12K_UMAC_RESET_STATE_INIT] = true,
	},
	/* From INIT */
	[ATH12K_UMAC_RESET_STATE_INIT] = {
		[ATH12K_UMAC_RESET_STATE_TRIGGER_SENT] = true,
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From TRIGGER_SENT */
	[ATH12K_UMAC_RESET_STATE_TRIGGER_SENT] = {
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_START] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From PRE_RESET_START */
	[ATH12K_UMAC_RESET_STATE_PRE_RESET_START] = {
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From PRE_RESET_DONE */
	[ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_START] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_START */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_START] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_DONE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_DONE */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_DONE] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_COMPLETE */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE] = {
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From ERROR */
	[ATH12K_UMAC_RESET_STATE_ERROR] = {
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
	},
};

static const char *ath12k_umac_reset_state_to_str(enum ath12k_umac_reset_state state)
{
	switch (state) {
	case ATH12K_UMAC_RESET_STATE_IDLE:
		return "IDLE";
	case ATH12K_UMAC_RESET_STATE_INIT:
		return "INIT";
	case ATH12K_UMAC_RESET_STATE_TRIGGER_SENT:
		return "TRIGGER_SENT";
	case ATH12K_UMAC_RESET_STATE_PRE_RESET_START:
		return "PRE_RESET_START";
	case ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE:
		return "PRE_RESET_DONE";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_START:
		return "POST_RESET_START";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_DONE:
		return "POST_RESET_DONE";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE:
		return "POST_RESET_COMPLETE";
	case ATH12K_UMAC_RESET_STATE_ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}

static bool ath12k_umac_reset_validate_transition(struct ath12k_base *ab,
						  enum ath12k_umac_reset_state from,
						  enum ath12k_umac_reset_state to)
{
	if (from >= ATH12K_UMAC_RESET_STATE_MAX || to >= ATH12K_UMAC_RESET_STATE_MAX)
		return false;

	return umac_reset_state_transition_valid[from][to];
}

static int ath12k_umac_reset_state_transition(struct ath12k_base *ab,
					      enum ath12k_umac_reset_state new_state)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	enum ath12k_umac_reset_state old_state;
	unsigned long flags;

	spin_lock_irqsave(&umac_reset->state_lock, flags);

	old_state = umac_reset->current_state;

	/* Validate transition */
	if (!ath12k_umac_reset_validate_transition(ab, old_state, new_state)) {
		/* Track error and transition to ERROR state */
		umac_reset->state_error_count++;
		umac_reset->error_from_state = old_state;
		umac_reset->prev_state = old_state;
		umac_reset->current_state = ATH12K_UMAC_RESET_STATE_ERROR;
		umac_reset->state_transition_count[ATH12K_UMAC_RESET_STATE_ERROR]++;
		umac_reset->state_entry_time[ATH12K_UMAC_RESET_STATE_ERROR] =
							jiffies_to_msecs(jiffies);
		spin_unlock_irqrestore(&umac_reset->state_lock, flags);

		ath12k_warn(ab, "[UMAC_RESET] Invalid state transition: %s -> %s\n",
			    ath12k_umac_reset_state_to_str(old_state),
			    ath12k_umac_reset_state_to_str(new_state));
		return -EINVAL;
	}

	/* Perform transition */
	umac_reset->prev_state = old_state;
	umac_reset->current_state = new_state;
	umac_reset->state_transition_count[new_state]++;
	umac_reset->state_entry_time[new_state] = jiffies_to_msecs(jiffies);

	spin_unlock_irqrestore(&umac_reset->state_lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "[UMAC_RESET] State transition: %s -> %s (count: %u)\n",
		   ath12k_umac_reset_state_to_str(old_state),
		   ath12k_umac_reset_state_to_str(new_state),
		   umac_reset->state_transition_count[new_state]);

	return 0;
}

static enum ath12k_umac_reset_state ath12k_umac_reset_get_state(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	enum ath12k_umac_reset_state state;
	unsigned long flags;

	spin_lock_irqsave(&umac_reset->state_lock, flags);
	state = umac_reset->current_state;
	spin_unlock_irqrestore(&umac_reset->state_lock, flags);

	return state;
}

int ath12k_htt_umac_reset_msg_send(struct ath12k_base *ab,
				   struct ath12k_htt_umac_reset_setup_cmd_params *params)
{
	struct sk_buff *skb;
	struct htt_dp_umac_reset_setup_req_cmd *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct htt_dp_umac_reset_setup_req_cmd *)skb->data;
	cmd->msg_info = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP,
					HTT_H2T_MSG_TYPE_SET);
	cmd->msg_info |= u32_encode_bits(0, HTT_H2T_MSG_METHOD);
	cmd->msg_info |= u32_encode_bits(0, HTT_T2H_MSG_METHOD);
	cmd->msi_data = params->msi_data;
	cmd->msg_shared_mem.size = sizeof(struct htt_h2t_paddr_size);
	cmd->msg_shared_mem.addr_lo = params->addr_lo;
	cmd->msg_shared_mem.addr_hi = params->addr_hi;


	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);

	if (ret) {
		ath12k_warn(ab, "DP UMAC INIT msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "DP UMAC INIT msg sent from host\n");
	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_htt_umac_reset_send_start_pre_reset_cmd(struct ath12k_base *ab, int is_initiator,
						   int is_target_recovery)
{
	struct sk_buff *skb;
	struct h2t_umac_hang_recovery_start_pre_reset *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct h2t_umac_hang_recovery_start_pre_reset*)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET,
				   HTT_H2T_UMAC_RESET_MSG_TYPE);
	cmd->hdr |= u32_encode_bits(is_initiator, HTT_H2T_UMAC_RESET_IS_INITIATOR_SET);
	cmd->hdr |= u32_encode_bits(!is_target_recovery, HTT_H2T_UMAC_RESET_IS_TARGET_RECOVERY_SET);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
                ath12k_warn(ab, "failed to send htt umac reset pre reset start: %d\n",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_get_umac_reset_intr_offset(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_NUM_MAX; i++) {
		if (ab->hw_params->ring_mask->umac_dp_reset[i])
			return i;
	}
	return 0;
}

int ath12k_htt_umac_reset_setup_cmd(struct ath12k_base *ab)
{
	int msi_data_count;
	struct ath12k_htt_umac_reset_setup_cmd_params params = {};
	u32 msi_data_start, msi_irq_start;
	int ret, intr_ctxt;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

	intr_ctxt = ath12k_get_umac_reset_intr_offset(ab);

	if (!ab->hw_params->umac_reset_ipc) {
		ret = ath12k_hif_get_user_msi_vector(ab, "DP",
						     &msi_data_count, &msi_data_start,
						     &msi_irq_start);
		if (ret) {
			ath12k_err(ab, "Failed to fill msi_data\n");
			return ret;
		}

		params.msi_data = (intr_ctxt % msi_data_count) + msi_data_start;
	} else {
		params.msi_data = ab->hw_params->umac_reset_ipc;
	}

	params.addr_lo = umac_reset->shmem_paddr_aligned & HAL_ADDR_LSB_REG_MASK;
	params.addr_hi = (u64)umac_reset->shmem_paddr_aligned >> HAL_ADDR_MSB_REG_SHIFT;

	return ath12k_htt_umac_reset_msg_send(ab, &params);
}

/**
 * ath12k_umac_reset_schedule_tasklet - SMP callback to schedule tasklet
 * @info: Pointer to mlo_umac_reset structure
 *
 * Called on target CPU via smp_call_function_single
 */
static void ath12k_umac_reset_schedule_tasklet(void *info)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = info;
	int cpu = smp_processor_id();

	tasklet_hi_schedule(&mlo_umac_reset->tasklet[cpu]);
}

int ath12k_dp_umac_reset_init(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	int alloc_size, ret, cpu;

	if (!ab->hw_params->support_umac_reset)
		return 0;

	umac_reset = &ab->dp_umac_reset;
	umac_reset->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;

	alloc_size = sizeof(struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t) +
			    ATH12K_DP_UMAC_RESET_SHMEM_ALIGN - 1;

	umac_reset->shmem_vaddr_unaligned = dma_alloc_coherent(ab->dev,
                                                     alloc_size,
                                                     &umac_reset->shmem_paddr_unaligned,
                                                     GFP_KERNEL);
	if (!umac_reset->shmem_vaddr_unaligned) {
		ath12k_warn(ab, "Failed to allocate memory with size:%u\n", alloc_size);
		return -ENOMEM;
	}

	umac_reset->shmem_vaddr_aligned =
		PTR_ALIGN(umac_reset->shmem_vaddr_unaligned, ATH12K_DP_UMAC_RESET_SHMEM_ALIGN);
	umac_reset->shmem_paddr_aligned =
		umac_reset->shmem_paddr_unaligned + ((unsigned long)umac_reset->shmem_vaddr_aligned -
				(unsigned long)umac_reset->shmem_vaddr_unaligned);
	umac_reset->shmem_size = alloc_size;
	umac_reset->shmem_vaddr_aligned->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;
	umac_reset->intr_offset = ath12k_get_umac_reset_intr_offset(ab);
	memset(&umac_reset->ts, 0, sizeof(struct ath12k_umac_reset_ts));

	/* Initialize state machine */
	spin_lock_init(&umac_reset->state_lock);
	umac_reset->current_state = ATH12K_UMAC_RESET_STATE_IDLE;
	umac_reset->prev_state = ATH12K_UMAC_RESET_STATE_IDLE;
	memset(umac_reset->state_transition_count, 0,
	       sizeof(umac_reset->state_transition_count));
	memset(umac_reset->state_entry_time, 0, sizeof(umac_reset->state_entry_time));
	umac_reset->state_error_count = 0;
	umac_reset->error_from_state = ATH12K_UMAC_RESET_STATE_IDLE;

	/* Initialize per-CPU call_single_data structures for async SMP calls */
	if (ag) {
		mlo_umac_reset = &ag->mlo_umac_reset;
		for_each_possible_cpu(cpu) {
			mlo_umac_reset->csd[cpu].func =
						ath12k_umac_reset_schedule_tasklet;
			mlo_umac_reset->csd[cpu].info = mlo_umac_reset;
		}
	}

	ret = ath12k_hif_dp_umac_reset_irq_config(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to register interrupt for UMAC RECOVERY\n");
		goto shmem_free;
	}

	ret = ath12k_htt_umac_reset_setup_cmd(ab);
	if (ret) {
		ath12k_warn(ab, "Unable to setup UMAC RECOVERY\n");
		goto free_irq;
	}

	ath12k_hif_dp_umac_reset_enable_irq(ab);
	return 0;

free_irq:
	ath12k_hif_dp_umac_reset_free_irq(ab);
shmem_free:
	dma_free_coherent(ab->dev,
			  umac_reset->shmem_size,
			  umac_reset->shmem_vaddr_unaligned,
			  umac_reset->shmem_paddr_unaligned);
	umac_reset->shmem_vaddr_unaligned = NULL;
	return ret;
}

void ath12k_umac_reset_pre_reset_validation(struct ath12k_base *ab, bool *is_initiator,
					    bool *is_target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	*is_initiator = *is_target_recovery = false;

	if (!ag)
		return;

	mlo_umac_reset = &ag->mlo_umac_reset;

	spin_lock_bh(&mlo_umac_reset->lock);
	if (mlo_umac_reset->initiator_chip == ab->device_id)
		*is_initiator = true;
	if (mlo_umac_reset->umac_reset_info & BIT(1))
		*is_target_recovery = true;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

void ath12k_umac_reset_completion(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (!mlo_umac_reset || ab->powered_off)
		return;

	if (!(mlo_umac_reset->umac_reset_info & BIT(0)))
		return;

	spin_lock_bh(&mlo_umac_reset->lock);
	mlo_umac_reset->umac_reset_info = 0;
	mlo_umac_reset->initiator_chip = 0;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

void ath12k_umac_reset_send_htt(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr_aligned;
	enum ath12k_umac_reset_state next_state;
	bool is_initiator, is_target_recovery;
	int ret;

	shmem_vaddr_aligned = umac_reset->shmem_vaddr_aligned;

	switch(tx_event) {
	case ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE:
		ath12k_umac_reset_pre_reset_validation(ab, &is_initiator,
						       &is_target_recovery);
		ret = ath12k_htt_umac_reset_send_start_pre_reset_cmd(ab,
								     is_initiator,
								     is_target_recovery);
		if (ret)
			ath12k_warn(ab, "Unable to send umac trigger\n");
		/* Transition to PRE_RESET_DONE state */
		next_state = ATH12K_UMAC_RESET_STATE_TRIGGER_SENT;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_PRE_RESET_DONE_SET);
		/* Transition to PRE_RESET_DONE state */
		next_state = ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_START_DONE_SET);
		/* Transition to POST_RESET_DONE state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_DONE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
				ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_COMPLETE_DONE);
		/* Transition back to IDLE state */
		next_state = ATH12K_UMAC_RESET_STATE_IDLE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
        }

	if (tx_event == ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE) {
		ath12k_umac_reset_completion(ab);
		ath12k_info(ab, "MLO UMAC Recovery completed\n");
	}

	return;
}

int ath12k_umac_reset_notify_target(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_base *partner_ab;
	struct ath12k_hw_group *ag = ab->ag;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags))
			continue;

		ath12k_umac_reset_send_htt(partner_ab, tx_event);
	}

	return 0;
}

bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	bool umac_in_progress = false;

	if (!ab->hw_params->support_umac_reset)
		return umac_in_progress;

	spin_lock_bh(&mlo_umac_reset->lock);
	if (mlo_umac_reset->umac_reset_info &
			ATH12K_IS_UMAC_RESET_IN_PROGRESS)
		umac_in_progress = true;
	spin_unlock_bh(&mlo_umac_reset->lock);

	return umac_in_progress;
}
EXPORT_SYMBOL(ath12k_dp_umac_reset_in_progress);

int ath12k_umac_reset_initiate_recovery(struct ath12k_base *ab,
					bool target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	enum ath12k_umac_reset_state current_state;
	int ret, i;

	/* Check current state before initiating recovery */
	current_state = ath12k_umac_reset_get_state(ab);
	if (current_state != ATH12K_UMAC_RESET_STATE_IDLE) {
		ath12k_warn(ab, "Cannot initiate recovery from state %s, expected INIT\n",
			    ath12k_umac_reset_state_to_str(current_state));
		return -EINVAL;
	}

	spin_lock_bh(&mlo_umac_reset->lock);

	if (mlo_umac_reset->umac_reset_info & ATH12K_IS_UMAC_RESET_IN_PROGRESS) {
		spin_unlock_bh(&mlo_umac_reset->lock);
		ath12k_warn(ab, "UMAC RECOVERY IS IN PROGRESS\n");
		WARN_ON(1);
		return -ECANCELED;
	}

	mlo_umac_reset->umac_reset_info = BIT(0); /* UMAC recovery is in progress */
	if (target_recovery)
		mlo_umac_reset->umac_reset_info |= BIT(1); /* Target recovery */

	atomic_set(&mlo_umac_reset->response_chip, 0);
	mlo_umac_reset->initiator_chip = ab->device_id;

	for (i = 0; i < ag->num_devices; i++) {
		struct ath12k_base *partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags))
			continue;

		/* Transition to INIT state */
		ret = ath12k_umac_reset_state_transition(partner_ab,
							 ATH12K_UMAC_RESET_STATE_INIT);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to INIT state\n");
			spin_unlock_bh(&mlo_umac_reset->lock);
			return -EINVAL;
		}
	}

	spin_unlock_bh(&mlo_umac_reset->lock);

	return 0;
}

void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
						   enum dp_umac_reset_tx_cmd tx_event)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (atomic_read(&mlo_umac_reset->response_chip) >= ab->ag->num_started) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "response chip:%d num_started:%d sending notify\n",
			   atomic_read(&mlo_umac_reset->response_chip), ab->ag->num_started);
		ath12k_umac_reset_notify_target(ab, tx_event);
		atomic_set(&mlo_umac_reset->response_chip, 0);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "response_chip:%d num_started:%d not matching.. hold on notify\n",
			   atomic_read(&mlo_umac_reset->response_chip), ab->ag->num_started);
	}
	return;
}
EXPORT_SYMBOL(ath12k_umac_reset_notify_target_sync_and_send);

void ath12k_umac_reset_notify_pre_reset_done(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;

	dp = ath12k_ab_to_dp(ab);

	if (dp->service_rings_running)
		return;

	ath12k_umac_reset_notify_target_sync_and_send(ab,
						      ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE);
	ab->dp_umac_reset.umac_pre_reset_in_prog = false;
}
EXPORT_SYMBOL(ath12k_umac_reset_notify_pre_reset_done);

void ath12k_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_pre_reset)
		dp->arch_ops->umac_reset_handle_pre_reset(ab);
}

void ath12k_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_post_reset_start)
		dp->arch_ops->umac_reset_handle_post_reset_start(ab);
}

void ath12k_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_post_reset_complete)
		dp->arch_ops->umac_reset_handle_post_reset_complete(ab);
}

static void ath12k_umac_reset_handle_init_recovery(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int tx_event = ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE;

	if (mlo_umac_reset->initiator_chip == ab->device_id)
		ath12k_umac_reset_notify_target(ab, tx_event);
}

/* Task queue management functions */

/**
 * ath12k_umac_reset_enqueue_task - Enqueue a task for multi-core processing
 * @ag: Hardware group
 * @callback: Function to execute
 * @ab: Device context
 * @event: Event type for debugging
 *
 * Returns: 0 on success, negative error code on failure
 */
int ath12k_umac_reset_enqueue_task(struct ath12k_hw_group *ag,
				   umac_reset_handler_fn callback,
				   struct ath12k_base *ab,
				   enum dp_umac_reset_recover_action event)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_umac_reset_task *task;
	unsigned long flags;

	if (!callback || !ab)
		return -EINVAL;

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return -ENOMEM;

	INIT_LIST_HEAD(&task->list);
	task->callback = callback;
	task->ab = ab;
	task->event = event;
	task->task_id = atomic_inc_return(&mlo_umac_reset->task_id);

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);
	list_add_tail(&task->list, &mlo_umac_reset->task_queue);
	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Enqueued task %u for event %d\n", task->task_id, event);

	return 0;
}
EXPORT_SYMBOL(ath12k_umac_reset_enqueue_task);

/**
 * ath12k_umac_reset_dequeue_task - Dequeue a task for processing
 * @ag: Hardware group
 *
 * Returns: Task structure or NULL if queue is empty
 */
struct ath12k_umac_reset_task *ath12k_umac_reset_dequeue_task(struct ath12k_hw_group *ag)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_umac_reset_task *task = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);
	if (!list_empty(&mlo_umac_reset->task_queue)) {
		task = list_first_entry(&mlo_umac_reset->task_queue,
					struct ath12k_umac_reset_task, list);
		list_del(&task->list);
	}
	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	return task;
}

/**
 * ath12k_umac_reset_tasklet_handler_percpu - Per-CPU tasklet handler
 * @t: Tasklet structure
 *
 * Processes tasks from the queue until empty
 */
void ath12k_umac_reset_tasklet_handler_percpu(struct tasklet_struct *t)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_umac_reset_task *task;
	struct ath12k_hw_group *ag;
	int cpu = smp_processor_id();

	/* Get the mlo_umac_reset structure from tasklet */
	mlo_umac_reset = container_of(t, struct ath12k_mlo_dp_umac_reset, tasklet[cpu]);
	ag = container_of(mlo_umac_reset, struct ath12k_hw_group, mlo_umac_reset);

	/* Process tasks until queue is empty */
	while ((task = ath12k_umac_reset_dequeue_task(ag)) != NULL) {
		if (task->callback) {
			ath12k_dbg(task->ab, ATH12K_DBG_DP_UMAC_RESET,
				   "CPU %d processing task %u for event %d\n",
				   cpu, task->task_id, task->event);
			task->callback(task->ab);
		}
		kfree(task);
	}
}

/* Static table mapping rx_event to handler functions */
static const umac_reset_handler_fn umac_reset_handlers[] = {
	[ATH12K_UMAC_RESET_RX_EVENT_NONE] = NULL,
	[ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY] =
					ath12k_umac_reset_handle_init_recovery,
	[ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC] =
					ath12k_umac_reset_handle_init_recovery,
	[ATH12K_UMAC_RESET_DO_PRE_RESET] = ath12k_umac_reset_handle_pre_reset,
	[ATH12K_UMAC_RESET_DO_POST_RESET_START] =
					ath12k_umac_reset_handle_post_reset_start,
	[ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE] =
					ath12k_umac_reset_handle_post_reset_complete,
};

static int
ath12k_dp_umac_reset_check_n_change_state(struct ath12k_base *ab,
					  enum dp_umac_reset_recover_action rx_event)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	bool target_recovery = false;
	enum ath12k_umac_reset_state next_state;
	int ret = 0;

	/* Validate rx_event is within valid range */
	if (rx_event <= ATH12K_UMAC_RESET_RX_EVENT_NONE ||
	    rx_event >= ARRAY_SIZE(umac_reset_handlers)) {
		ath12k_warn(ab, "Invalid UMAC RESET event: %d\n", rx_event);
		return -EINVAL;
	}

	switch(rx_event) {
	case ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC:
		target_recovery = true;
		fallthrough;
	case ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY:
		if (!target_recovery && ab->is_reset)
			return ret;

		ret = ath12k_umac_reset_initiate_recovery(ab, target_recovery);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to initate Umac recovery\n");
			break;
		}
		atomic_set(&mlo_umac_reset->request_chip, ag->num_started);

		break;
	case ATH12K_UMAC_RESET_DO_PRE_RESET:
		/* Transition to PRE_RESET_START state */
		next_state = ATH12K_UMAC_RESET_STATE_PRE_RESET_START;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to PRE_RESET_START state\n");
			break;
		}

		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_START:
		/* Transition to POST_RESET_START state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_START;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to POST_RESET_START state\n");
			break;
		}
		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE:
		/* Transition to POST_RESET_COMPLETE state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to POST_RESET_COMPLETE state\n");
			break;
		}
		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Unknown UMAC RESET event received\n");
		break;
	}

	return ret;
}

irqreturn_t ath12k_umac_reset_interrupt_handler(int irq, void *arg)
{
	struct ath12k_base *ab = arg;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr;
	u32 t2h_msg;
	u64 timestamp;
	bool has_valid_event = false;
	int htt_msg;

	shmem_vaddr = umac_reset->shmem_vaddr_aligned;

	/* Validate shared memory */
	if (!shmem_vaddr || shmem_vaddr->magic_num != umac_reset->magic_num) {
		/* Spurious interrupt - no valid shared memory */
		return IRQ_HANDLED;
	}

	/* Read event message */
	t2h_msg = shmem_vaddr->t2h_msg;

	/* Check if any valid events are present */
	if (!t2h_msg) {
		/* No events - spurious interrupt */
		return IRQ_HANDLED;
	}

	/* Capture timestamp immediately for all events */
	timestamp = jiffies_to_msecs(jiffies);

	/* Check for valid events and record per-event timestamps */
	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY)) {
		umac_reset->ts.event_irq_init_umac_recovery = timestamp;
		has_valid_event = true;
	}

	htt_msg = HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC;
	if (u32_get_bits(t2h_msg, htt_msg)) {
		umac_reset->ts.event_irq_init_target_recovery = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET)) {
		umac_reset->ts.event_irq_pre_reset = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START)) {
		umac_reset->ts.event_irq_post_reset_start = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE)) {
		umac_reset->ts.event_irq_post_reset_complete = timestamp;
		has_valid_event = true;
	}

	/* Only schedule tasklet if we have valid events */
	if (has_valid_event)
		tasklet_schedule(&umac_reset->intr_tq);

	return IRQ_HANDLED;
}

void ath12k_dp_umac_reset_handle(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr;
	struct ath12k_base *partner_ab;
	enum ath12k_umac_reset_state current_state;
	int rx_event, num_event = 0;
	u32 t2h_msg;
	int i;

	shmem_vaddr = umac_reset->shmem_vaddr_aligned;
	if (!shmem_vaddr) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address NULL\n");
		return;
	}

	if (shmem_vaddr->magic_num != umac_reset->magic_num) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address is invalid shmem:0x%x u:0x%x\n",
			   shmem_vaddr->magic_num, umac_reset->magic_num);
		return;
	}

	/* Log current state for debugging */
	current_state = ath12k_umac_reset_get_state(ab);
	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Processing UMAC reset event in state: %s\n",
		   ath12k_umac_reset_state_to_str(current_state));

	t2h_msg = shmem_vaddr->t2h_msg;
	shmem_vaddr->t2h_msg = 0;

	rx_event = ATH12K_UMAC_RESET_RX_EVENT_NONE;

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET)) {
		rx_event |= ATH12K_UMAC_RESET_DO_PRE_RESET;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_START;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE;
		num_event++;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Deduced rx event:%d num:%d\n", rx_event, num_event);

	if (num_event > 1) {
		ath12k_warn(ab, "Multiple events notified in single msg while in state %s\n",
			    ath12k_umac_reset_state_to_str(current_state));
		WARN_ON_ONCE(1);
		return;
	}

	/* First, validate the event */
	if (ath12k_dp_umac_reset_check_n_change_state(ab, rx_event)) {
		ath12k_warn(ab, "UMAC reset event validation failed for event %d\n",
			    rx_event);
		return;
	}

	/* Check if all chips have sent requests */
	if (atomic_read(&mlo_umac_reset->request_chip) < ag->num_started) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Not all chips ready: request_chip=%d num_started=%d, exiting early\n",
			   atomic_read(&mlo_umac_reset->request_chip), ag->num_started);
		return;
	}

	/* Reset request counter after processing all chips */
	atomic_set(&mlo_umac_reset->request_chip, 0);

	/* All chips have sent requests - process event for entire group serially */
	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "All chips ready: request_chip=%d, processing event for entire group\n",
		   atomic_read(&mlo_umac_reset->request_chip));

	/* Process event for all partner devices in the group */
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags))
			continue;

		if (umac_reset_handlers[rx_event])
			umac_reset_handlers[rx_event](partner_ab);
	}

	/* After handlers, schedule tasklets if tasks were enqueued */
	if (!list_empty(&mlo_umac_reset->task_queue)) {
		int cpu;

		for_each_cpu(cpu, cpu_online_mask) {
			smp_call_function_single_async(cpu, &mlo_umac_reset->csd[cpu]);
		}
	}

	return;
}

void ath12k_umac_reset_tasklet_handler(struct tasklet_struct *umac_cntxt)
{
	struct ath12k_dp_umac_reset *umac_reset = from_tasklet(umac_reset, umac_cntxt, intr_tq);
	struct ath12k_base *ab = container_of(umac_reset, struct ath12k_base, dp_umac_reset);

	ath12k_hif_dp_umac_intr_line_reset(ab);
	ath12k_dp_umac_reset_handle(ab);
}

void ath12k_dp_umac_reset_deinit(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;

	if (!ab->hw_params->support_umac_reset || ab->powered_off)
		return;

	umac_reset = &ab->dp_umac_reset;

	if (!umac_reset)
		return;

	ath12k_hif_dp_umac_reset_free_irq(ab);

	if (umac_reset->shmem_vaddr_unaligned) {
		dma_free_coherent(ab->dev,
				  umac_reset->shmem_size,
				  umac_reset->shmem_vaddr_unaligned,
				  umac_reset->shmem_paddr_unaligned);
		umac_reset->shmem_vaddr_unaligned = NULL;

	}
}
