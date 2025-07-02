/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "core.h"
#include "sdwf.h"
#include "dp_peer.h"
#include "debug.h"
#include <linux/module.h>

bool ath12k_sdwf_service_configured(struct ath12k_base *ab, u16 svc_id)
{
	struct ath12k_qos_ctx *qos_ctx;
	bool status;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return false;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return false;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	status = qos_ctx->svc_class[svc_id].configured;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Configured:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].configured);
	spin_unlock_bh(&qos_ctx->profile_lock);
	return status;
}

int ath12k_sdwf_map_service_class(struct ath12k_base *ab, u16 svc_id,
				  u16 dl_qos_id,
				  u16 ul_qos_id)
{
	int ret = -EINVAL;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ret;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ret;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	qos_ctx->svc_class[svc_id].dl_qos_id = dl_qos_id;
	qos_ctx->svc_class[svc_id].ul_qos_id = ul_qos_id;
	qos_ctx->svc_class[svc_id].configured = true;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Config | DL QoS ID:%d | UL QoS ID:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].dl_qos_id,
		   qos_ctx->svc_class[svc_id].ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return 0;
}

int ath12k_sdwf_unmap_service_class(struct ath12k_base *ab, u16 svc_id)
{
	int ret = -EINVAL;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ret;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ret;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	qos_ctx->svc_class[svc_id].dl_qos_id = QOS_ID_INVALID;
	qos_ctx->svc_class[svc_id].ul_qos_id = QOS_ID_INVALID;
	qos_ctx->svc_class[svc_id].configured = false;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Disable| DL QoS ID:%d | UL QoS ID:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].dl_qos_id,
		   qos_ctx->svc_class[svc_id].ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return 0;
}

u16 ath12k_sdwf_get_dl_qos_id(struct ath12k_base *ab, u16 svc_id)
{
	u16 dl_qos_id = QOS_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return dl_qos_id;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return dl_qos_id;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	dl_qos_id = qos_ctx->svc_class[svc_id].dl_qos_id;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Get DL QoS ID:%d",
		   svc_id,
		   dl_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return dl_qos_id;
}

u16 ath12k_sdwf_get_ul_qos_id(struct ath12k_base *ab, u16 svc_id)
{
	u16 ul_qos_id = QOS_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ul_qos_id;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ul_qos_id;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	ul_qos_id = qos_ctx->svc_class[svc_id].ul_qos_id;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Get DL QoS ID:%d",
		   svc_id,
		   ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return ul_qos_id;
}

static struct ath12k_dp_peer_qos*
ath12k_sdwf_get_qos_ctx(struct ath12k_base *ab,
			struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_peer_qos *peer_qos = peer->qos;

	if (!peer_qos)
		peer_qos = ath12k_dp_peer_qos_alloc(ab->dp, peer);

	return peer_qos;
}

static u8 ath12k_sdwf_alloc_msduq(struct ath12k_base *ab, u32 svc_id,
				  u16 peer_id, bool scs)
{
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	u16 qos_id;
	u8 scs_id; u8 qos_tag;
	u16 msduq = QOS_INVALID_MSDUQ;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return msduq;
	}

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(ab->dp, peer_id);
	if (!peer) {
		ath12k_err(ab, "Unable to find peer");
		goto ret;
	}

	qos = ath12k_sdwf_get_qos_ctx(ab, peer->dp_peer);
	if (!qos) {
		ath12k_err(ab, "Unable to find peer qos ctx");
		goto ret;
	}

	if  (scs) {
		qos_tag = u32_get_bits(svc_id, SCS_PROTOCOL_MASK);
		if (qos_tag == QOS_SCS_TAG) {
			scs_id = u32_get_bits(svc_id, SCS_SVC_ID_MASK);
			ath12k_dp_peer_scs_data(ab->dp, qos, scs_id,
						&msduq, &qos_id);
			goto ret;
		} else {
			msduq = u32_get_bits(svc_id, SCS_SVC_ID_MASK);
			goto ret;
		}
	}

	qos_id = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
	if (qos_id == QOS_ID_INVALID)
		goto ret;

	msduq = ath12k_dp_peer_qos_msduq(ab, qos, qos_id);

ret:
	spin_unlock_bh(&ab->dp->dp_lock);
	return msduq;
}

u32 ath_encode_sdwf_metadata(u16 msduq_id)
{
	u32 sawf_metadata = 0;

	sawf_metadata = u32_encode_bits(SDWF_VALID,
					SDWF_VALID_MASK) |
	u32_encode_bits(SDWF_VALID_TAG,
			SDWF_TAG_ID) |
	u32_encode_bits(msduq_id,
			SDWF_PEER_MSDUQ_ID);
	return sawf_metadata;
}

struct ath12k *ath12k_sdwf_get_ar_from_vif(struct wireless_dev *wdev,
					   struct ieee80211_vif *vif,
					   u8 *peer_mac, u16 *peer_id)
{
	struct ath12k_base *ab = NULL;
	struct ath12k *ar = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	u8 mac_addr[ETH_ALEN] = { 0 };
	u8 link_id;
	struct ath12k_dp_link_peer *peer;

	if (!wdev)
		return NULL;

	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	sta = ieee80211_find_sta_by_ifaddr(ahvif->ah->hw, peer_mac, NULL);
	if (!sta) {
		pr_err("Peer:%pM not present", peer_mac);
		sta = wdev_to_ieee80211_vlan_sta(wdev);
		if (!sta)
			return NULL;
	}

	ahsta = (struct ath12k_sta *)sta->drv_priv;
	if (!ahsta)
		return NULL;

	rcu_read_lock();
	if (sta->mlo) {
		link_id = ahsta->primary_link_id;
		memcpy(mac_addr, ahsta->link[link_id]->addr, ETH_ALEN);
	} else if (sta->valid_links) {
		link_id = ahsta->deflink.link_id;
		memcpy(mac_addr, peer_mac, ETH_ALEN);
	} else {
		link_id = 0;
		memcpy(mac_addr, peer_mac, ETH_ALEN);
	}

	arvif = rcu_dereference(ahvif->link[link_id]);

	if (!arvif) {
		rcu_read_unlock();
		return NULL;
	}

	ar = arvif->ar;
	if (!ar) {
		rcu_read_unlock();
		return NULL;
	}
	rcu_read_unlock();

	ab = ar->ab;

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_addr(ab->dp, mac_addr);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Peer: %pM not present\n", mac_addr);
		spin_unlock_bh(&ab->dp->dp_lock);
		return NULL;
	}

	*peer_id = peer->peer_id;

	spin_unlock_bh(&ab->dp->dp_lock);
	return ar;
}

u16 ath12k_sdwf_get_msduq(struct wireless_dev *wdev,
			  u8 *peer_mac, u32 svc_id, bool scs)
{
	struct ath12k *ar;
	struct ieee80211_vif *vif;
	u16 peer_id;
	u16 ret_msduq = SDWF_PEER_MSDUQ_INVALID;
	u8 msduq = QOS_INVALID_MSDUQ;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return ret_msduq;

	ar = ath12k_sdwf_get_ar_from_vif(wdev, vif, peer_mac, &peer_id);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return ret_msduq;
	}

	if (!scs && !ath12k_sdwf_service_configured(ar->ab, svc_id))
		return ret_msduq;

	msduq = ath12k_sdwf_alloc_msduq(ar->ab, svc_id, peer_id, scs);
	if (msduq != QOS_INVALID_MSDUQ)
		ret_msduq = FIELD_PREP(SDWF_PEER_ID, peer_id) |
				FIELD_PREP(SDWF_MSDUQ_ID, msduq);

	return ret_msduq;
}

u16 ath12k_sdwf_get_msduq_peer(struct wireless_dev *wdev,
			       u8 *peer_mac,
			      struct sawf_param *dl_params,
			      bool scs_mscs)
{
	if (!wdev)
		return SDWF_PEER_MSDUQ_INVALID;

	return ath12k_sdwf_get_msduq(wdev, peer_mac,
					dl_params->service_id,
					scs_mscs);
}
