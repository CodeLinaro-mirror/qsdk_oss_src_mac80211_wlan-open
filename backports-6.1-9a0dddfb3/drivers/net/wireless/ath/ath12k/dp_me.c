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
#include "dp_tx.h"
#include "dp_ext_desc.h"
#include "qcn_extns/me_snoop_extn.h"

static int ath12k_dp_tx_me5(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx)
{
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct ath12k_dp_ext_desc *ext_desc;
	struct sk_buff *skb = me_ctx->skb;
	u8 ring_id = smp_processor_id();
	struct ath12k_pdev_dp *dp_pdev;
	dma_addr_t paddr;
	u8 *mac_addr;

	dp_pdev = ath12k_dp_to_dp_pdev(dp, link_vif->pdev_idx);
	if (!dp_pdev)
		goto fail_no_mem;

	/*
	 * Prepare Tx Descriptor
	 */
	tx_desc = ath12k_dp_tx_assign_buffer(dp, ring_id);
	if (unlikely(!tx_desc))
		goto fail_no_mem;

	ext_desc = kmem_cache_alloc(dp->ext_cache, GFP_DMA | __GFP_ZERO);
	if (!ext_desc) {
		ath12k_warn(dp->ab, "Ext Descriptor not allocated\n");
		goto fail_txbuf_free;
	}

	paddr = ath12k_core_dma_map_single(dp->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (!paddr) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:DMA mapping failed for skb\n", dp);
		goto fail_ext_desc_free;
	}

	/*
	 * Load the mac address in the EXT descriptor
	 */
	mac_addr = ath12k_dp_ext_desc_get_spare(ext_desc, ETH_ALEN);
	ether_addr_copy(mac_addr, dp_peer->addr);
	ath12k_dp_ext_desc_set_buf0(ext_desc, virt_to_phys(mac_addr), ETH_ALEN);

	/*
	 * Load the SKB payload minus MAC address in the EXT descriptor
	 */
	paddr += ETH_ALEN; /* move the payload by the MAC offset */
	ath12k_dp_ext_desc_set_buf1(ext_desc, paddr, skb->len - ETH_ALEN);

	tx_desc->len = skb->len;
	tx_desc->skb = skb_get(skb);
	tx_desc->mac_id = link_vif->pdev_idx;

	tx_desc->ext_desc = ext_desc;
	tx_desc->paddr_ext_desc = ath12k_dp_ext_desc_map(dp, ext_desc);

	tx_desc->ext_desc_len = ATH12K_DP_EXT_DESC_SZ;

	if (ath12k_dp_ext_tx(dp, dp_pdev, dp_vif, link_vif, tx_desc)) {
		ath12k_warn(dp->ab, "DP ME Transmission Failed\n");
		goto fail_desc_unmap;
	}

	return 0;

fail_desc_unmap:
	ath12k_dp_ext_desc_unmap(dp, tx_desc->paddr_ext_desc);
	dev_kfree_skb_any(tx_desc->skb);
	ath12k_core_dma_unmap_single(dp->dev, paddr - ETH_ALEN, skb->len, DMA_TO_DEVICE);
fail_ext_desc_free:
	kmem_cache_free(dp->ext_cache, ext_desc);
fail_txbuf_free:
	ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);
fail_no_mem:
	/*
	 * TODO: stats update
	 */
	return -ENOMEM;
}

static int ath12k_dp_tx_me6(struct ath12k_dp *dp, struct ath12k_dp_vif *dp_vif,
			    struct ath12k_dp_link_vif *link_vif,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_me_ctx *me_ctx)
{
	enum ath12k_dp_tx_enq_error enq_err = DP_TX_ENQ_SUCCESS;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_tx_desc_info *tx_desc;
	struct sk_buff *skb = me_ctx->skb;
	struct ath12k_pdev_dp *dp_pdev;
	u8 ring_id = smp_processor_id();
	dma_addr_t paddr;
	u16 mdata = 0;
	u16 peer_id;

	dp_pdev = ath12k_dp_to_dp_pdev(dp, link_vif->pdev_idx);
	if (unlikely(!dp_pdev)) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:Could not find dp_pdev\n", dp);
		goto fail1;
	}

	link_peer = ath12k_dp_link_peer_find_by_id(dp, dp_peer->peer_id);
	if (!link_peer) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:Link Peer NOT FOUND IN ME6", dp);
		goto fail1;
	}

	/* Assign peer_id of link peer*/
	peer_id = link_peer->peer_id;

	/* Allocate tx_desc and populate */
	tx_desc = ath12k_dp_tx_assign_buffer(dp, ring_id);
	if (unlikely(!tx_desc)) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX,
			   "%p:Unable to assign software tx_desc\n", dp);
		dp->device_stats.tx_err.txbuf_na[ring_id]++;
		goto fail1;
	}

	/*
	 * Prepare metadata with peer_id
	 */
	mdata |= u16_encode_bits(HTT_TCL_META_DATA_TYPE_PEER_BASED,
				HTT_TCL_META_DATA_TYPE);
	mdata |= u16_encode_bits(peer_id, HTT_TCL_META_DATA_PEER_ID);

	/*
	 * Map and get reference on skb
	 */
	paddr = ath12k_core_dma_map_single(dp->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (!paddr) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:DMA mapping failed for skb\n", dp);
		goto fail2;
	}

	/*
	 * Load the TX desc for ME6 offload
	 * Note: Take reference of the SKB for this transmision
	 */
	tx_desc->mac_id = link_vif->pdev_idx;
	tx_desc->skb = skb_get(skb);
	tx_desc->tcl_metadata = mdata;
	tx_desc->paddr = paddr;
	tx_desc->to_fw = 1;
	tx_desc->len = skb->len;

	enq_err = ath12k_dp_ext_tx(dp, dp_pdev, dp_vif, link_vif, tx_desc);
	if (enq_err) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX,
			   "TX enqueue failed for MCUC with error code: %d\n", enq_err);
		goto fail3;
	}

	/* TODO: Update MCUC statistics and return*/
	return 0;

fail3:
	ath12k_core_dma_unmap_single(dp->dev, paddr, skb->len, DMA_TO_DEVICE);
	/*
	 * Drop the SKB reference taken above
	 */
	dev_kfree_skb_any(tx_desc->skb);
fail2:

	/* Free TX descriptor */
	ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);
fail1:
	/* TODO: Update stats */
	return -ENOMEM;
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
	if (dp_peer->dms_disable)
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
	int (*action_fn)(struct ath12k_dp *dp,
			 struct ath12k_dp_vif *dp_vif,
			 struct ath12k_dp_link_vif *dp_link_vif,
			 struct ath12k_dp_peer *dp_peer, void *app_data);
	struct ath12k_me_ctx ctx = {0};
	struct ath12k_me_db *me_db;
	union nf_inet_addr addr = {0};
	int ret = 0;
	int action;
	bool is_v6;

	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db)
		return -ENOENT;

	ctx.me_flags = me_db->me_flags;
	ctx.skb = skb;

	if (ath12k_dp_me_check(dp_vif, &ctx) < 0) {
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	is_v6 = __skb_get_inet_daddr(skb, &addr);

	action = ath12k_me_hmmc_lookup(me_db, (__be32 *)&addr, is_v6);
	ath12k_me_db_put(me_db);

	switch (action) {
	case ATH12K_ME_HMMC_ACTION:
		action_fn = ath12k_dp_me_tx_ucast_peer;
		break;

	case ATH12K_ME_DENYLIST_ACTION:
		return -EINVAL;

	default:
		break;
	}

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
	if (action != ATH12K_ME_HMMC_ACTION) {
		ctx.grp = ath12k_me_snoop_grp_find(dp_vif, skb);
		if (!ctx.grp)
			return -EINVAL;

		bitmap_zero(ctx.tx_bmap, ATH12K_ME_MAX_SNOOP_PEERS);
		action_fn = ath12k_dp_me_tx_ucast_grp_extn;
	}
#endif

	/*
	 * Iterate across all the dp link vifs.
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
						 action_fn, &ctx);
		if (ret) {
			/* TODO:
			 * Can Increment the peer specific stats here.
			 */
		}
	}
	rcu_read_unlock_bh();

	/*
	 * Unconditionally, free the original SKB since the UCAST FN have already
	 * taken the references. This will ensure that intermediate send failures
	 * doesn't leak the SKB.
	 */
	dev_kfree_skb_any(skb);

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_dp_me_tx);
