// SPDX-License-Identifier: GPL-2.0-only
/*
 * UHR handling
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "ieee80211_i.h"

void
ieee80211_uhr_cap_ie_to_sta_uhr_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const struct ieee80211_uhr_cap_elem *uhr_cap_ie_elem,
				    u8 uhr_cap_len,
				    struct link_sta_info *link_sta)
{
	struct ieee80211_sta_uhr_cap *uhr_cap = &link_sta->pub->uhr_cap;
	u8 *pos = (u8 *)uhr_cap_ie_elem;

	memset(uhr_cap, 0, sizeof(*uhr_cap));

	if (uhr_cap_len < sizeof(uhr_cap->uhr_cap_elem))
		return;

	if (!uhr_cap_ie_elem ||
	    !ieee80211_get_uhr_iftype_cap_vif(sband, &sdata->vif))
		return;

	/* Copy the static portion of the UHR capabilities */
	memcpy(&uhr_cap->uhr_cap_elem, pos, sizeof(uhr_cap->uhr_cap_elem));

	uhr_cap->has_uhr = true;
}
