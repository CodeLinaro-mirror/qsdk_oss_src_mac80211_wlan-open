// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include <net/genetlink.h>
#include "core.h"
#include "cfg80211_scan_radio_extn.h"

static const u8 scan_radio_pwr_prio[] = {
	NL80211_REG_AP_SP,
	NL80211_REG_AP_LPI,
	NL80211_REG_AP_VLP,
};

/**
 * cfg80211_scan_radio_inject_power_mode - inject 6 GHz power mode attr for scan radio
 *
 * Selects the best available power mode for the scan radio and injects it into
 * info->attrs so that _nl80211_parse_chandef sees it as if user space set it.
 *
 * Selection order: SP → LPI → VLP.
 * Pass 1: first mode whose channel has neither DISABLED nor NO_IR.
 * Pass 2: if none found, use SP as long as the channel is not DISABLED
 *         (NO_IR accepted — scan radio may start before AFC completes).
 *
 * @attr_buf must be a heap-allocated buffer; the caller is responsible for
 * freeing it after nl80211_parse_chandef and any subsequent attr access.
 */
void cfg80211_scan_radio_inject_power_mode(struct cfg80211_registered_device *rdev,
					   struct wireless_dev *wdev,
					   struct genl_info *info,
					   struct nlattr *attr_buf)
{
	struct ieee80211_channel *chan;
	u32 freq_khz;
	u8 mode;
	int i;

	if (!wdev_is_scan_radio(wdev))
		return;

	if (!info->attrs[NL80211_ATTR_WIPHY_FREQ])
		return;

	if (info->attrs[NL80211_ATTR_6G_REG_POWER_MODE])
		return;

	freq_khz = MHZ_TO_KHZ(nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ]));
	if (info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET])
		freq_khz += nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET]);

	/* Pass 1: SP → LPI → VLP, require no DISABLED and no NO_IR */
	for (i = 0; i < ARRAY_SIZE(scan_radio_pwr_prio); i++) {
		struct ieee80211_supported_band *sband6;

		mode = scan_radio_pwr_prio[i];

		sband6 = rdev->wiphy.bands[NL80211_BAND_6GHZ];
		if (!sband6 || !sband6->chan_6g[mode])
			continue;

		chan = ieee80211_get_6g_channel_khz(&rdev->wiphy, freq_khz, mode);
		if (chan &&
		    !(chan->flags & (IEEE80211_CHAN_DISABLED | IEEE80211_CHAN_NO_IR)))
			goto inject;
	}

	/* Pass 2: fall back to SP, accept NO_IR but not DISABLED */
	mode = NL80211_REG_AP_SP;
	chan = ieee80211_get_6g_channel_khz(&rdev->wiphy, freq_khz, mode);
	if (!chan || (chan->flags & IEEE80211_CHAN_DISABLED))
		return;

inject:
	attr_buf->nla_len  = nla_attr_size(sizeof(u8));
	attr_buf->nla_type = NL80211_ATTR_6G_REG_POWER_MODE;
	*(u8 *)nla_data(attr_buf) = mode;

	info->attrs[NL80211_ATTR_6G_REG_POWER_MODE] = attr_buf;
}

int cfg80211_scan_radio_check_chandef_pwr(
				struct wiphy *wiphy,
				struct cfg80211_chan_def *chandef,
				u8 reg_6ghz_power_mode)
{
	u32 prohibited_flags = IEEE80211_CHAN_DISABLED;

	return cfg80211_validate_freq_width_for_pwr_mode(wiphy, chandef,
							 reg_6ghz_power_mode,
							 prohibited_flags);
}
