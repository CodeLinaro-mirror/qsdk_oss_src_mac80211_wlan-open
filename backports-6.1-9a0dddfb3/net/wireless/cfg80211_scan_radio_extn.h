/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __CFG80211_SCAN_RADIO_EXTN_H
#define __CFG80211_SCAN_RADIO_EXTN_H

#include <net/cfg80211.h>

struct cfg80211_registered_device;

struct cfg80211_scan_radio_pwr_nla {
	struct nlattr hdr;
	u8 mode;
	u8 pad[3];
};

void cfg80211_scan_radio_inject_power_mode(struct cfg80211_registered_device *rdev,
					   struct wireless_dev *wdev,
					   struct genl_info *info,
					   struct nlattr *attr_buf);

int cfg80211_scan_radio_check_chandef_pwr(
				struct wiphy *wiphy,
				struct cfg80211_chan_def *chandef,
				u8 reg_6ghz_power_mode);

#endif /* __CFG80211_SCAN_RADIO_EXTN_H */
