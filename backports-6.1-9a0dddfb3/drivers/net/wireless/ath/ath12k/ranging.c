// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/slab.h>
#include "core.h"
#include "wmi.h"
#include "mac.h"
#include "debug.h"
#include "ranging.h"

u32 ath12k_supported_rtt_responder_roles(struct ath12k_base *ab)
{
	u32 role = ATH12K_RTT_11MC_RESPONDER_ROLE;

	if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT,
		     ab->wmi_ab.svc_map))
		role |= ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE;

	if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT,
		     ab->wmi_ab.svc_map))
		role |= ATH12K_RTT_11AZ_TB_RESPONDER_ROLE;

	return role;
}

static bool ath12k_11az_rsta_supported(struct ath12k_base *ab)
{
	return test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT, ab->wmi_ab.svc_map) ||
		test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT, ab->wmi_ab.svc_map);
}

static bool ath12k_11az_mac_sec_supported(struct ath12k_base *ab)
{
	return ath12k_11az_rsta_supported(ab) &&
		(test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_SEC_SUPPORT, ab->wmi_ab.svc_map) ||
		 test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_PHY_SEC_SUPPORT,
			  ab->wmi_ab.svc_map));
}

static bool ath12k_11az_mac_phy_sec_supported(struct ath12k_base *ab)
{
	return ath12k_11az_rsta_supported(ab) &&
		test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_PHY_SEC_SUPPORT,
			 ab->wmi_ab.svc_map);
}

void ath12k_mac_set_ranging_ext_features(struct wiphy *wiphy, struct ath12k *ar)
{
	if (ath12k_11az_mac_sec_supported(ar->ab)) {
		wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SECURE_RTT);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE);
	}

	if (ath12k_11az_mac_phy_sec_supported(ar->ab))
		wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SECURE_LTF);
}

/**
 * ath12k_mac_setup_iftype_11az_ranging() - Advertise AP 11az ext-caps.
 * @ah: ath12k hardware group.
 *
 * The static ath12k iftype capability table is shared by all hardware
 * instances and also carries immutable template data used by registration.
 * 11az NTB/TB responder bits depend on firmware service bits, so update a
 * per-wiphy copy instead of modifying the global template in place.
 *
 * Return: 0 on success or a negative errno on allocation failure.
 */
int ath12k_mac_setup_iftype_11az_ranging(struct ath12k_hw *ah)
{
	const struct wiphy_iftype_ext_capab *tmpl, *ap_tmpl;
	struct wiphy_iftype_ext_capab *iftype_ext_capa;
	struct ath12k *ar, *tmp_ar;
	struct wiphy *wiphy;
	u8 *ap_ext_capa;
	unsigned int n_capab;
	u8 ranging = 0;
	u8 ap_len;
	int i, ap_idx = -1;

	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar || ar->rtt_capab.rtt_iftype_ext_capab)
		return 0;

	for_each_ar(ah, tmp_ar, i) {
		if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT,
			     tmp_ar->ab->wmi_ab.svc_map))
			ranging |= ATH12K_EXT_CAPA12_NTB_RANGING_RESPONDER;

		if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT,
			     tmp_ar->ab->wmi_ab.svc_map))
			ranging |= ATH12K_EXT_CAPA12_TB_RANGING_RESPONDER;
	}

	if (!ranging)
		return 0;

	wiphy = ah->hw->wiphy;
	tmpl = wiphy->iftype_ext_capab;
	n_capab = wiphy->num_iftype_ext_capab;
	if (!tmpl || !n_capab)
		return 0;

	for (i = 0; i < n_capab; i++) {
		if (tmpl[i].iftype == NL80211_IFTYPE_AP) {
			ap_idx = i;
			break;
		}
	}

	if (ap_idx < 0)
		return 0;

	ap_tmpl = &tmpl[ap_idx];
	ap_len = max_t(u8, ap_tmpl->extended_capabilities_len,
		       ATH12K_EXT_CAPA_11AZ_RANGING_IDX + 1);
	iftype_ext_capa = kmemdup(tmpl, sizeof(*tmpl) * n_capab, GFP_KERNEL);
	if (!iftype_ext_capa)
		return -ENOMEM;

	ap_ext_capa = kzalloc(ap_len, GFP_KERNEL);
	if (!ap_ext_capa) {
		kfree(iftype_ext_capa);
		return -ENOMEM;
	}

	if (ap_tmpl->extended_capabilities)
		memcpy(ap_ext_capa, ap_tmpl->extended_capabilities,
		       ap_tmpl->extended_capabilities_len);

	ap_ext_capa[ATH12K_EXT_CAPA_11AZ_RANGING_IDX] |= ranging;
	iftype_ext_capa[ap_idx].extended_capabilities = ap_ext_capa;
	iftype_ext_capa[ap_idx].extended_capabilities_mask = ap_ext_capa;
	iftype_ext_capa[ap_idx].extended_capabilities_len = ap_len;

	ar->rtt_capab.rtt_iftype_ext_capab_orig = tmpl;
	ar->rtt_capab.rtt_num_iftype_ext_capab_orig = n_capab;
	ar->rtt_capab.rtt_iftype_ext_capab = iftype_ext_capa;
	ar->rtt_capab.rtt_ap_ext_capab = ap_ext_capa;
	wiphy->iftype_ext_capab = iftype_ext_capa;
	wiphy->num_iftype_ext_capab = n_capab;

	return 0;
}

/**
 * ath12k_mac_cleanup_iftype_11az_ranging() - Release RTT ext-cap copy.
 * @ah: ath12k hardware group.
 *
 * Restore the wiphy back to the original iftype capability table before
 * freeing the dynamically allocated AP extended capability buffer.
 */
void ath12k_mac_cleanup_iftype_11az_ranging(struct ath12k_hw *ah)
{
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	struct wiphy *wiphy = ah->hw->wiphy;

	if (!ar || !ar->rtt_capab.rtt_iftype_ext_capab)
		return;

	if (wiphy->iftype_ext_capab == ar->rtt_capab.rtt_iftype_ext_capab) {
		wiphy->iftype_ext_capab = ar->rtt_capab.rtt_iftype_ext_capab_orig;
		wiphy->num_iftype_ext_capab =
			ar->rtt_capab.rtt_num_iftype_ext_capab_orig;
	}

	kfree(ar->rtt_capab.rtt_ap_ext_capab);
	kfree(ar->rtt_capab.rtt_iftype_ext_capab);
	ar->rtt_capab.rtt_ap_ext_capab = NULL;
	ar->rtt_capab.rtt_iftype_ext_capab = NULL;
	ar->rtt_capab.rtt_iftype_ext_capab_orig = NULL;
	ar->rtt_capab.rtt_num_iftype_ext_capab_orig = 0;
}

/* RTT FW recovery reconfig and link_vif lifecycle */
void ath12k_rtt_reconfig_responder_role(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	u32 role;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		struct ath12k_vif *ahvif = arvif->ahvif;

		if (!arvif->is_started || !ahvif)
			continue;

		if (ahvif->vdev_type != WMI_VDEV_TYPE_AP)
			continue;

		if (!arvif->ftm_responder)
			continue;

		role = arvif->rtt_ctx.rtt_responder_role;
		if (!role)
			continue;

		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
			WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_RESPONDER_ROLE,
			role);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to restore RTT responder role 0x%x on vdev %i after FW recovery: %d\n",
				    role, arvif->vdev_id, ret);
		else
			ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
				   "restored RTT responder role 0x%x on vdev %i link %u after FW recovery\n",
				   role, arvif->vdev_id, arvif->link_id);
	}
}
