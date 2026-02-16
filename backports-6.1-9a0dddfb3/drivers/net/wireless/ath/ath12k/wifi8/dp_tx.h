/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_WIFI8_H
#define ATH12K_DP_TX_WIFI8_H

#include "../dp.h"
#include "hal_desc.h"

#define DP_TX_SFE_BUFFER_SIZE           256

struct ath12k_dp_tx_queue {
	u16 peer_id;
	u8 hw_link_id;
};
struct ath12k_dp_tqm_cmd {
	struct list_head list;
	struct ath12k_dp_tx_queue data;
	enum hal_tlv_tag_be cmd_type;
	int cmd_num;
	void (*handler)(struct ath12k_dp *dp, void *ctx,
			enum hal_tqm_cmd_execution_status status);
};

int ath12k_wifi8_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget);
enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx(struct ath12k_pdev_dp *dp_pdev,
		   struct ath12k_link_vif *arvif,
		   struct sk_buff *skb, bool gsn_valid, int mcbc_gsn,
		   bool is_mcast, struct ath12k_link_sta *arsta,
		   u8 ring_id, u32 qos_nw_delay);
enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx_fast(struct ath12k_pdev_dp *dp_pdev,
			struct ath12k_link_vif *arvif,
			struct ath12k_vif *vlan_vif,
			struct sk_buff *skb,
			u32 qos_nw_delay);
u32 ath12k_wifi8_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable);
bool ath12k_mac_tx_check_max_limit(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *skb);
int ath12k_wifi8_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb,
				       struct ath12k_link_sta *arsta);
int ath12k_wifi8_dp_tx_ring_setup(struct ath12k_base *ab);
void ath12k_wifi8_dp_tx_ring_cleanup(struct ath12k_base *ab);
int ath12k_wifi8_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget);
int ath12k_wifi8_dp_tx_exception_handler(struct ath12k_dp *dp, int budget);
int ath12k_wifi8_dp_tqm_cmd_send(struct ath12k_base *ab,
				 enum hal_tlv_tag_be type,
				 struct ath12k_hal_tqm_cmd *cmd,
				 struct ath12k_dp_tx_queue *data,
				 void (*callback_fn)(
					 struct ath12k_dp *dp,
					 void *ctx,
					 enum hal_tqm_cmd_execution_status status));
void ath12k_wifi8_dp_tx_process_tqm_status(struct ath12k_dp *dp);
void ath12k_dp_peer_cleanup_tqm_sync(struct ath12k_dp *dp, void *ctx,
				     enum hal_tqm_cmd_execution_status status);
void ath12k_wifi8_dp_tx_tqm_cmd_list_cleanup(struct ath12k_base *ab);
#endif
