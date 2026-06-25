// SPDX-License-Identifier: GPL-2.0-only
/*
 * UHR handling
 *
 * Copyright(c) 2025-2026 Intel Corporation
 */

#include "ieee80211_i.h"

void
ieee80211_uhr_cap_ie_to_sta_uhr_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const struct ieee80211_uhr_cap_elem *uhr_cap,
				    u8 uhr_cap_len,
				    struct link_sta_info *link_sta)
{
	struct ieee80211_sta_uhr_cap *sta_uhr_cap = &link_sta->pub->uhr_cap;

	memset(sta_uhr_cap, 0, sizeof(*sta_uhr_cap));

	if (!ieee80211_get_uhr_iftype_cap_vif(sband, &sdata->vif))
		return;

	sta_uhr_cap->has_uhr = true;

	sta_uhr_cap->mac = uhr_cap->fixed.mac;
	sta_uhr_cap->phy = uhr_cap->fixed.phy;
}

void
ieee80211_uhr_npca_elem_to_sta_uhr_npca_info(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_supported_band *sband,
					   const struct ieee80211_uhr_operation *uhr_oper,
					   struct link_sta_info *link_sta)
{
	struct ieee80211_sta_uhr_npca_info *npca_info = &link_sta->pub->npca_info;
	const struct ieee80211_sta_uhr_cap *uhr_cap;
	const struct ieee80211_uhr_npca_info *npca;

	memset(npca_info, 0, sizeof(*npca_info));
	link_sta->pub->npca_offset = 0;
	link_sta->pub->npca_puncture_bitmap = 0;

	uhr_cap = ieee80211_get_uhr_iftype_cap_vif(sband, &sdata->vif);
	if (!uhr_cap)
		return;

	if (!(uhr_cap->mac.mac_cap[0] & IEEE80211_UHR_MAC_CAP0_NPCA_SUPP))
		return;

	if (!uhr_oper)
		return;

	npca = ieee80211_uhr_npca_info(uhr_oper);
	if (!npca)
		return;

	npca_info->npca_enabled = true;
	npca_info->npca_min_dur_threshold =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_MIN_DUR_THRESH);
	npca_info->npca_switch_delay =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_SWITCH_DELAY);
	npca_info->npca_switch_back_delay =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_SWITCH_BACK_DELAY);
	npca_info->npca_initial_qsrc =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_INIT_QSRC);
	npca_info->npca_moplen =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_MOPLEN);

	link_sta->pub->npca_offset =
		le32_get_bits(npca->params,
			      IEEE80211_UHR_NPCA_PARAMS_PRIMARY_CHAN_OFFS);
	if (npca->params &
	    cpu_to_le32(IEEE80211_UHR_NPCA_PARAMS_DIS_SUBCH_BMAP_PRES))
		link_sta->pub->npca_puncture_bitmap =
			le16_to_cpu(npca->dis_subch_bmap[0]);
}
