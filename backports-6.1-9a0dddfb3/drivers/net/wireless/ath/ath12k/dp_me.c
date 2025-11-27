// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ipv6.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/errno.h>

#include "dp_peer.h"
#include "dp_ext_desc.h"
#include "me.h"
#include "debug.h"

static int ath12k_dp_tx_me5(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *dp_link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx)
{
	/* TODO: Implement ME5 multicast-to-unicast conversion */
	ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "ME5 offload not yet implemented\n");
	return -EOPNOTSUPP;
}

static int ath12k_dp_tx_me6(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *dp_link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx)
{
	/* TODO: Implement ME6 (DMS) multicast-to-unicast conversion */
	ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "ME6 offload not yet implemented\n");
	return -EOPNOTSUPP;
}

static int ath12k_dp_me_check(struct ath12k_dp_vif *dp_vif, struct ath12k_me_ctx *ctx)
{
	struct sk_buff *skb = ctx->skb;
	u32 flags = ctx->me_flags;
	u16 proto = ntohs(skb->protocol);
	u8 l3_proto;

	/*
	 * Only go for M2U conversion when the flags are enabled
	 */
	if (!(flags & ATH12K_ME_OFFLOAD_MASK))
		goto fail;

	if (skb_is_nonlinear(skb)) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "No support for ME with SG pkts");
		goto fail;
	}

	switch (proto) {
	case ETH_P_IP:
		l3_proto = ip_hdr(skb)->protocol;
		break;

	case ETH_P_IPV6:
		l3_proto = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		l3_proto = 0;
		break;
	}

	if (!is_multicast_ether_addr(eth_hdr(skb)->h_dest))
		goto fail;

	if ((l3_proto == IPPROTO_IGMP) && !(flags & ATH12K_ME_FLAGS_BIT_IGMP_EN))
		goto fail;

	return 0;

fail:
	ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "ME offload check failed");
	return -EINVAL;
}

/**
 * ath12k_dp_me_tx_ucast_peer(): Decide and perform the desired MCUC
 * @dp: Data Path ptr
 * @dp_vif: Data Path Virtual Interface
 * @dp_link_vif : Data path Link specific obj
 * @dp_peer: DP Peer Object
 * @app_data: Desired App_data sent
 *
 * Return: Status for MCUC Success/Failure.
 */
int ath12k_dp_me_tx_ucast_peer(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			       struct ath12k_dp_link_vif *dp_link_vif,
			       struct ath12k_dp_peer *dp_peer,
			       void *app_data)
{
	struct ath12k_me_ctx *ctx = app_data;
	int ret = 0;
	u32 flags;

	flags = ctx->me_flags & ATH12K_ME_OFFLOAD_MASK;
	if (!dp_peer->dms_capable)
		flags = ATH12K_ME_FLAGS_BIT_ME5;

	switch (flags) {
	case ATH12K_ME_FLAGS_BIT_ME5:
		ret = ath12k_dp_tx_me5(dp, dp_vif, dp_link_vif, dp_peer, ctx);
		if (ret < 0)
			ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Failed to convert ME5\n");
		break;
	case ATH12K_ME_FLAGS_BIT_ME6:
		ret = ath12k_dp_tx_me6(dp, dp_vif, dp_link_vif, dp_peer, ctx);
		if (ret < 0)
			ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Failed to convert ME6\n");
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "Invalid ME flags(0%x) for TX\n",
			   ctx->me_flags);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * ath12k_dp_me_tx(): Transmit function for Multicast packets
 * @dp_vif - Pointer to Data path virtual interface structure
 * @skb: Pointer to socket buffer
 *
 * Return: Success or Failure.
 */
int ath12k_dp_me_tx(struct ath12k_dp_vif *dp_vif, struct sk_buff *skb)
{
	struct ath12k_me_ctx ctx = {0};
	int ret = 0;

	if (!dp_vif->me_db)
		return -ENOENT;

	ctx.me_flags = dp_vif->me_db->me_flags;
	ctx.skb = skb;

	if (ath12k_dp_me_check(dp_vif, &ctx) < 0)
		return -EINVAL;

	/*
	 * Perform MCUC across all the peers here.
	 */
	rcu_read_lock_bh();
	for (u8 link_id = 0; link_id < ATH12K_NUM_MAX_LINKS; link_id++) {
		struct ath12k_dp_link_vif *dp_link_vif;
		struct ath12k_link_vif *arvif;
		struct ath12k_vif *ahvif;
		struct ath12k_dp *dp;
		struct ath12k *ar;

		ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);

		dp_link_vif = &dp_vif->dp_link_vif[link_id];

		arvif = rcu_dereference(ahvif->link[link_id]);
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar || !ar->ab)
			continue;

		dp = ar->ab->dp;
		if (!dp)
			continue;

		/*
		 * Perform MCUC across all the relevant peers.
		 */
		ret = ath12k_dp_peer_walk_action(dp, dp_vif, dp_link_vif,
						 ath12k_dp_me_tx_ucast_peer, &ctx);
		if (ret) {
			/* TODO:
			 * Can Increment the peer specific stats here.
			 */
		}
	}
	rcu_read_unlock_bh();

	dev_kfree_skb_any(skb);

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_dp_me_tx);
