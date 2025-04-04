// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "debug.h"
#include "fse.h"
#include "dp_rx.h"
#include "peer.h"
#include <linux/module.h>

bool ath12k_fse_enable;
module_param_named(fse, ath12k_fse_enable, bool, 0444);
MODULE_PARM_DESC(fse, "Enable FSE feature (Default: false)");

static const struct ath_fse_ops ath_fse_ops_obj = {
	.fse_rule_add = ath12k_sfe_add_flow_entry,
	.fse_rule_delete = ath12k_sfe_delete_flow_entry,
	.fse_get_ab = ath12k_sfe_get_ab_from_vif,
};

void ath12k_fse_init(struct ath12k_base *ab)
{
	const struct ath_fse_ops *fse_ops_ptr;

	fse_ops_ptr = &ath_fse_ops_obj;
	if (!ath12k_fse_enable)
		return;

	if (ath_fse_ops_callback_register(fse_ops_ptr)) {
		ath12k_err(ab, "ath12k callback register fail\n");
		return;
	}
	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "FSE context initialized\n");
}

void ath12k_fse_deinit(struct ath12k_base *ab)
{
	if (!ath12k_fse_enable)
		return;

	ath_fse_ops_callback_unregister();

	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "FSE context deinitialized\n");
}

void *ath12k_sfe_get_ab_from_vif(struct ieee80211_vif *vif,
				 const u8 *peer_mac)
{
	struct ath12k_base *ab = NULL;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	unsigned long links;
	u8 link_id;

	if (!vif)
		return NULL;

	ahvif = ath12k_vif_to_ahvif(vif);
	links = ahvif->links_map;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ahvif->link[link_id];
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar)
			continue;
		ab = ar->ab;
		spin_lock_bh(&ab->base_lock);
		peer = ath12k_dp_link_peer_find_by_addr(ath12k_ab_to_dp(ab), peer_mac);
		spin_unlock_bh(&ab->base_lock);
		if (peer)
			return ab;
	}
	return ab;
}

int ath12k_sfe_add_flow_entry(void *ptr,
			      u32 *src_ip, u32 src_port,
			      u32 *dest_ip, u32 dest_port,
			      u8 protocol, u8 version)

{
	int ret = 0;

	return ret;
}

int ath12k_sfe_delete_flow_entry(void *ptr,
				 u32 *src_ip, u32 src_port,
				 u32 *dest_ip, u32 dest_port,
				 u8 protocol, u8 version)
{
	int ret = 0;

	return ret;
}
