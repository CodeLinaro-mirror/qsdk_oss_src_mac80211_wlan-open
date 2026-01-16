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

#include "me.h"
#include "debug.h"

static int ath12k_dp_me_check(struct ath12k_dp_vif *dp_vif, struct ath12k_me_ctx *ctx)
{
	struct sk_buff *skb = ctx->skb;
	u32 flags = ctx->me_flags;
	u16 proto = skb->protocol;
	int err = 0;
	u16 l3_proto;

	if (skb_is_nonlinear(skb) && (skb_linearize(skb) < 0)) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "No support for ME with SG pkts");
		return -ENOMEM;
	}

	switch (proto) {
	case ETH_P_IP:
		l3_proto = ip_hdr(skb)->protocol;
		break;

	case ETH_P_IPV6:
		l3_proto = ipv6_hdr(skb)->nexthdr;
		break;
	default:
		goto ret;
	}

	err += !is_multicast_ether_addr(eth_hdr(skb)->h_dest);
	err += ((l3_proto == IPPROTO_IGMP) && !(flags & ATH12K_ME_FLAGS_BIT_IGMP_EN));
	err += !(flags & ATH12K_ME_OFFLOAD_MASK);

ret:
	if (err) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "ME offload check failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_dp_me_tx(): Transmit function for Multicast packets
 * @dp_vif - Pointer to Data path virtual interface structure
 * @skb: Pointer to socket buffer
 *
 * Return: status of success or failure.
 */
int ath12k_dp_me_tx(struct ath12k_dp_vif *dp_vif, struct sk_buff *skb)
{
	struct ath12k_me_ctx ctx = {0};

	ctx.me_flags = dp_vif->me_db->me_flags;
	ctx.skb = skb;

	if (ath12k_dp_me_check(dp_vif, &ctx) < 0)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_dp_me_tx);
