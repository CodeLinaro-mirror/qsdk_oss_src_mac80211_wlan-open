/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_ME_H
#define __ATH12K_ME_H

#include <net/ipv6.h>
#include <linux/ip.h>
#include <linux/bitfield.h>
#include "core.h"
#include "dp_peer.h"

#define ATH12K_ME_FLAGS_BIT_UNUSED1 BIT(0) /* Available for future use */
#define ATH12K_ME_FLAGS_BIT_UNUSED2 BIT(1) /* Available for future use */
#define ATH12K_ME_FLAGS_BIT_UNUSED3 BIT(2) /* Available for future use */
#define ATH12K_ME_FLAGS_BIT_IGMP_EN BIT(3) /* ME offload for IGMP packets */
#define ATH12K_ME_FLAGS_BIT_BYPASS BIT(4) /* Bypass ME conversion */
#define ATH12K_ME_FLAGS_BIT_ME5 BIT(5) /* ME5 offload enable */
#define ATH12K_ME_FLAGS_BIT_ME6 BIT(6) /* ME6 offload enable */

#define ATH12K_ME_OFFLOAD_MASK (ATH12K_ME_FLAGS_BIT_ME5 | ATH12K_ME_FLAGS_BIT_ME6)

struct ath12k_dp_vif;
struct ath12k_dp_link_vif;

/*
 * ATH12K ME Ctx which holds info for every skb
 */
struct ath12k_me_ctx {
	struct sk_buff *skb;    /* Original skb */
	u32 me_flags;		/* ME flag */
};

/*
 * ATH12K Multicast database
 */
struct ath12k_me_db {
	u32 me_flags;		/* ME Flags  */
	u16 grp_limit;		/* Soft limit for number of groups */
};

/**
 * __skb_get_inet_saddr(): Get source address from an skb
 * @skb - Pointer to Socket Buffer
 * @addr: Source address
 *
 * Return: If a packet is v6 or not.
 */
static inline bool __skb_get_inet_saddr(struct sk_buff *skb, union nf_inet_addr *addr)
{
	bool is_v6 = false;

	switch (skb->protocol) {
	case ETH_P_IP:
		addr->ip = ip_hdr(skb)->saddr;
		break;
	case ETH_P_IPV6:
		addr->in6 = ipv6_hdr(skb)->saddr;
		is_v6 = true;
		break;
	default:
		memset(addr, 0, sizeof(*addr));
		break;
	}

	return is_v6;
}

/**
 * __skb_get_inet_daddr(): Get destination address from an skb
 * @skb - Pointer to Socket Buffer
 * @addr: Destination address
 *
 * Return: If a packet is v6 or not.
 */
static inline bool __skb_get_inet_daddr(struct sk_buff *skb, union nf_inet_addr *addr)
{
	bool is_v6 = false;

	switch (skb->protocol) {
	case ETH_P_IP:
		addr->ip = ip_hdr(skb)->daddr;
		break;
	case ETH_P_IPV6:
		addr->in6 = ipv6_hdr(skb)->daddr;
		is_v6 = true;
		break;
	default:
		memset(addr, 0, sizeof(*addr));
		break;
	}

	return is_v6;
}

int ath12k_dp_me_tx(struct ath12k_dp_vif *dp_vif, struct sk_buff *skb);

void ath12k_me_db_reset(struct ath12k_me_db *db);
int ath12k_me_db_deinit(struct ath12k_dp_vif *dp_vif);
int ath12k_me_db_init(struct ath12k_dp_vif *dp_vif);

#endif /* __ATH12K_ME_H */
