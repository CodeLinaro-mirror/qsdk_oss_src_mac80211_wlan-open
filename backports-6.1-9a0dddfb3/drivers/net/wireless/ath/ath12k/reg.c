// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/rtnetlink.h>
#include <linux/platform_device.h>
#include "core.h"
#include "debug.h"
#include "ahb.h"
#include "vendor.h"

/* World regdom to be used in case default regd from fw is unavailable */
#define ATH12K_2GHZ_CH01_11      REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0)
#define ATH12K_5GHZ_5150_5350    REG_RULE(5150 - 10, 5350 + 10, 80, 0, 30,\
					  NL80211_RRF_NO_IR)
#define ATH12K_5GHZ_5725_5850    REG_RULE(5725 - 10, 5850 + 10, 80, 0, 30,\
					  NL80211_RRF_NO_IR)

#define ETSI_WEATHER_RADAR_BAND_LOW		5590
#define ETSI_WEATHER_RADAR_BAND_HIGH		5650
#define ETSI_WEATHER_RADAR_BAND_CAC_TIMEOUT	600000

/* The default config is enabling retail AFC */
/* When ath12k_afc_disable_timer_check = false, the AP will enable all
 * the AFC timers in HALPHY.
 */
bool ath12k_afc_disable_timer_check;
module_param_named(afc_disable_timer_check, ath12k_afc_disable_timer_check, bool, 0644);
MODULE_PARM_DESC(afc_disable_timer_check, "Disable AFC expiry timer check for enterprise mode: 1-disable, 0-enable");

/* When ath12k_afc_disable_req_id_check = false, the AP will enable request
 * ID match in the FW.
 */
bool ath12k_afc_disable_req_id_check;
module_param_named(afc_disable_req_id_check, ath12k_afc_disable_req_id_check, bool, 0644);
MODULE_PARM_DESC(afc_disable_req_id_check, "Disable AFC request id check for enterprise mode: 1-disable, 0-enable");

/* Turn off AFC support in the host. The host driver will notify the firmware
 * that AFC is not supported,
 */
bool ath12k_6ghz_sp_pwrmode_supp_enabled;
module_param_named(6ghz_sp_pwrmode_supp_enabled, ath12k_6ghz_sp_pwrmode_supp_enabled, bool, 0644);
MODULE_PARM_DESC(6ghz_sp_pwrmode_supp_enabled, "Enable AFC support to FW: 1-enable, 0-disable");

/* When ath12k_afc_reg_no_action = false, the AP will trigger channel
 * change on AFC response.
 */
bool ath12k_afc_reg_no_action;
module_param_named(afc_reg_no_action, ath12k_afc_reg_no_action, bool, 0644);
MODULE_PARM_DESC(afc_reg_no_action, "Do not trigger channel change on AFC response: 1-enable, 0-disable");

/* Given a global opclass number create the corresponding  array token.
 * Examples:
 *     'CFISARR(132)' expands to  'opcls_132_cfis_arr'
 *     'CFISARR(133)' expands to  'opcls_133_cfis_arr'
 */
#define CFISARR(_g_opcls)  opcls_ ## _g_opcls ## _cfis_arr

#define NULL_CFIS_LST NULL
#define CFISLST(_g_opcls)  opcls_ ## _g_opcls ## _cfis_lst

/* The type of the opclass list objects */
#define CFISLST_TYPE static const struct ath12k_c_freq_lst

#define NELEMS ARRAY_SIZE

#define CREATE_CFIS_LST(_gopcls) \
	CFISLST_TYPE CFISLST(_gopcls) = {NELEMS(CFISARR(_gopcls)), CFISARR(_gopcls)}

/* CFIs for global opclass 131: (start Freq=5925 BW=20MHz) */
static const u8 opcls_131_cfis_arr[] = {
	  1,   5,   9,  13,  17,  21,  25,  29,  33,
	 37,  41,  45,  49,  53,  57,  61,  65,  69,
	 73,  77,  81,  85,  89,  93,  97, 101, 105,
	109, 113, 117, 121, 125, 129, 133, 137, 141,
	145, 149, 153, 157, 161, 165, 169, 173, 177,
	181, 185, 189, 193, 197, 201, 205, 209, 213,
	217, 221, 225, 229, 233,
};

/* CFIs for global opclass 132: (start Freq=5925 BW=40MHz) */
static const u8 opcls_132_cfis_arr[] = {
	  3,  11,  19,  27,  35,  43,  51,  59,  67,
	 75,  83,  91,  99, 107, 115, 123, 131, 139,
	147, 155, 163, 171, 179, 187, 195, 203, 211,
	219, 227,
};

/* CFIs for global opclass 133: (start Freq=5925 BW=80MHz) */
static const u8 opcls_133_cfis_arr[] = {
	  7,  23,  39,  55,  71,  87, 103, 119, 135,
	151, 167, 183, 199, 215,
};

/* CFIs for global opclass 134: (start Freq=5950 BW=160MHz) */
static const u8 opcls_134_cfis_arr[] = {
	15, 47, 79, 111, 143, 175, 207,
};

/* CFIs for global opclass 135: (start Freq=5950 BW=80MHz+80MHz) */
static const u8 opcls_135_cfis_arr[] = {
	  7,  23,  39,  55,  71,  87, 103, 119, 135,
	151, 167, 183, 199, 215,
};

/* CFIs for global opclass 136: (start Freq=5925 BW=20MHz) */
static const u8 opcls_136_cfis_arr[] = {
	2,
};

/* CFIs for global opclass 137: (start Freq=5950 BW=320MHz) */
static const u8 opcls_137_cfis_arr[] = {
	31, 63, 95, 127, 159, 191,
};

/* Create the CFIS static constant lists */
CREATE_CFIS_LST(131);
CREATE_CFIS_LST(132);
CREATE_CFIS_LST(133);
CREATE_CFIS_LST(134);
CREATE_CFIS_LST(135);
CREATE_CFIS_LST(136);
CREATE_CFIS_LST(137);

static const struct ieee80211_regdomain ath12k_world_regd = {
	.n_reg_rules = 3,
	.alpha2 = "00",
	.reg_rules = {
		ATH12K_2GHZ_CH01_11,
		ATH12K_5GHZ_5150_5350,
		ATH12K_5GHZ_5725_5850,
	}
};

enum wmi_reg_6g_ap_type
ath12k_ieee80211_ap_pwr_type_convert(enum ieee80211_ap_reg_power power_type)
{
	switch (power_type) {
	case IEEE80211_REG_LPI_AP:
		return WMI_REG_INDOOR_AP;
	case IEEE80211_REG_SP_AP:
		return WMI_REG_STD_POWER_AP;
	case IEEE80211_REG_VLP_AP:
		return WMI_REG_VLP_AP;
	default:
		return WMI_REG_MAX_AP_TYPE;
	}
}

static struct ath12k_reg_rule
*ath12k_get_active_6g_reg_rule(struct ath12k_reg_info *reg_info,
                              u32 *max_bw_6g, int *max_elements,
                              enum nl80211_regulatory_power_modes *pwr_mode)
{
	struct ath12k_reg_rule *reg_rule = NULL;
	u8 i = 0, j = 0;

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		if (reg_info->num_6g_reg_rules_ap[i]) {
			*max_elements = reg_info->num_6g_reg_rules_ap[i];
			reg_rule = reg_info->reg_rules_6g_ap_ptr[i];
			*max_bw_6g = reg_info->max_bw_6g_ap[i];
			reg_info->num_6g_reg_rules_ap[i] = 0;
			*pwr_mode = i;
			return reg_rule;
		}
	}

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++) {
			if (reg_info->num_6g_reg_rules_cl[j][i]) {
				*max_elements = reg_info->num_6g_reg_rules_cl[j][i];
				reg_rule = reg_info->reg_rules_6g_client_ptr[j][i];
				*max_bw_6g = reg_info->max_bw_6g_client[j][i];
				reg_info->num_6g_reg_rules_cl[j][i] = 0;
				*pwr_mode = WMI_REG_CURRENT_MAX_AP_TYPE * (i + 1)  + j;
				return reg_rule;
			}
		}
	}

       return reg_rule;
}

static bool ath12k_regdom_changes(struct ieee80211_hw *hw, char *alpha2)
{
	const struct ieee80211_regdomain *regd;

	regd = rcu_dereference_rtnl(hw->wiphy->regd);
	/* This can happen during wiphy registration where the previous
	 * user request is received before we update the regd received
	 * from firmware.
	 */
	if (!regd)
		return true;

	return memcmp(regd->alpha2, alpha2, 2) != 0;
}

static void
ath12k_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_wmi_init_country_arg arg;
	struct wmi_set_current_country_arg current_arg = {};
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	int ret, i;

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Regulatory Notification received for %s\n", wiphy_name(wiphy));

	if (request->initiator == NL80211_REGDOM_SET_BY_DRIVER) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "driver initiated regd update\n");
		if (ah->state != ATH12K_HW_STATE_ON)
			return;

		for_each_ar(ah, ar, i) {
			ret = ath12k_reg_update_chan_list(ar, true);
			if (ret) {
				ath12k_warn(ar->ab,
					    "failed to update chan list for pdev %u, ret %d\n",
					    i, ret);
				break;
			}
		}
		return;
	}

	/* Currently supporting only General User Hints. Cell base user
	 * hints to be handled later.
	 * Hints from other sources like Core, Beacons are not expected for
	 * self managed wiphy's
	 */
	if (!(request->initiator == NL80211_REGDOM_SET_BY_USER &&
	      request->user_reg_hint_type == NL80211_USER_REG_HINT_USER)) {
		ath12k_warn(ar->ab, "Unexpected Regulatory event for this wiphy\n");
		return;
	}

	if (!IS_ENABLED(CPTCFG_ATH_REG_DYNAMIC_USER_REG_HINTS)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "Country Setting is not allowed\n");
		return;
	}

	if (!ath12k_regdom_changes(hw, request->alpha2)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Country is already set\n");
		return;
	}

	/* Allow fresh updates to wiphy regd */
	ah->regd_updated = false;

	/* Send the reg change request to all the radios */
	for_each_ar(ah, ar, i) {
		if (ar->ab->hw_params->current_cc_support) {
			memcpy(&current_arg.alpha2, request->alpha2, 2);
			memcpy(&ar->alpha2, &current_arg.alpha2, 2);
			ret = ath12k_wmi_send_set_current_country_cmd(ar, &current_arg);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed set current country code: %d\n", ret);
		} else {
			arg.flags = ALPHA_IS_SET;
			memcpy(&arg.cc_info.alpha2, request->alpha2, 2);
			arg.cc_info.alpha2[2] = 0;

			ret = ath12k_wmi_send_init_country_cmd(ar, &arg);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed set INIT Country code: %d\n", ret);
		}

		wiphy_lock(wiphy);
		ath12k_mac_11d_scan_stop(ar);
		wiphy_unlock(wiphy);

		ar->regdom_set_by_user = true;
	}
}

int ath12k_reg_update_chan_list(struct ath12k *ar, bool wait)
{
	int left;

	if (wait && ar->state_11d != ATH12K_11D_IDLE) {
		left = wait_for_completion_timeout(&ar->completed_11d_scan,
						   ATH12K_SCAN_TIMEOUT_HZ);
		if (!left) {
			ath12k_dbg(ar->ab, ATH12K_DBG_REG,
				   "failed to receive 11d scan complete: timed out\n");
			ar->state_11d = ATH12K_11D_IDLE;
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "reg 11d scan wait left time %d\n", left);
	}

	if (wait &&
	    (ar->scan.state == ATH12K_SCAN_STARTING ||
	    ar->scan.state == ATH12K_SCAN_RUNNING)) {
		left = wait_for_completion_timeout(&ar->scan.completed,
						   ATH12K_SCAN_TIMEOUT_HZ);
		if (!left)
			ath12k_dbg(ar->ab, ATH12K_DBG_REG,
				   "failed to receive hw scan complete: timed out\n");

		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "reg hw scan wait left time %d\n", left);
	}

	if (ar->ah->state == ATH12K_HW_STATE_RESTARTING)
		return 0;

	return ath12k_wmi_update_scan_chan_list(ar, NULL);
}

static void ath12k_copy_regd(struct ieee80211_regdomain *regd_orig,
			     struct ieee80211_regdomain *regd_copy)
{
	u8 i;

	/* The caller should have checked error conditions */
	memcpy(regd_copy, regd_orig, sizeof(*regd_orig));

	for (i = 0; i < regd_orig->n_reg_rules; i++)
		memcpy(&regd_copy->reg_rules[i], &regd_orig->reg_rules[i],
		       sizeof(struct ieee80211_reg_rule));
}

int ath12k_reg_get_num_chans_in_band(struct ath12k *ar,
				     struct ieee80211_supported_band *band)
{
	int i, count = 0;
	u32 center_freq;

	for (i = 0; i < band->n_channels; i++) {
		center_freq = band->channels[i].center_freq;
		if (center_freq >= KHZ_TO_MHZ(ar->freq_range.start_freq) &&
		    center_freq <= KHZ_TO_MHZ(ar->freq_range.end_freq))
			count++;
	}

	return count;
}

int ath12k_regd_update(struct ath12k *ar, bool init)
{
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ieee80211_hw *hw = ah->hw;
	struct ieee80211_regdomain *regd, *regd_copy = NULL;
	int ret, regd_len, pdev_id;
	struct ath12k_base *ab;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *reg_cap;
	u32 phy_id, freq_low, freq_high, supported_bands, band;

	ab = ar->ab;

	supported_bands = ar->pdev->cap.supported_bands;
	if (supported_bands & WMI_HOST_WLAN_2GHZ_CAP)
		band = NL80211_BAND_2GHZ;
	else if(supported_bands & WMI_HOST_WLAN_5GHZ_CAP && !ar->supports_6ghz)
		band = NL80211_BAND_5GHZ;
	else if(supported_bands & WMI_HOST_WLAN_5GHZ_CAP && ar->supports_6ghz)
		band = NL80211_BAND_6GHZ;

	reg_cap = &ab->hal_reg_cap[ar->pdev_idx];

	if (ab->hw_params->single_pdev_only && !ar->supports_6ghz) {
		phy_id = ar->pdev->cap.band[band].phy_id;
		reg_cap = &ab->hal_reg_cap[phy_id];
	}

	/* Possible that due to reg change, current limits for supported
	 * frequency changed. Update that
	 */
	if (supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		freq_low = max(reg_cap->low_2ghz_chan, ab->reg_freq_2g.start_freq);
		freq_high = min(reg_cap->high_2ghz_chan, ab->reg_freq_2g.end_freq);
	} else if(supported_bands & WMI_HOST_WLAN_5GHZ_CAP && !ar->supports_6ghz) {
		freq_low = max(reg_cap->low_5ghz_chan, ab->reg_freq_5g.start_freq);
		freq_high = min(reg_cap->high_5ghz_chan, ab->reg_freq_5g.end_freq);
	} else if(supported_bands & WMI_HOST_WLAN_5GHZ_CAP && ar->supports_6ghz) {
		freq_low = max(reg_cap->low_5ghz_chan, ab->reg_freq_6g.start_freq);
		freq_high = min(reg_cap->high_5ghz_chan, ab->reg_freq_6g.end_freq);
	}

	ar->chan_info.low_freq = freq_low;
	ar->chan_info.high_freq = freq_high;

	ath12k_dbg(ab, ATH12K_DBG_REG, "pdev %u reg updated freq limits %u->%u MHz\n",
		   ar->pdev->pdev_id, ar->chan_info.low_freq,
		   ar->chan_info.high_freq);

	/* If one of the radios within ah has already updated the regd for
	 * the wiphy, then avoid setting regd again
	 */
	if (ah->regd_updated)
		return 0;

	pdev_id = ar->pdev_idx;

	spin_lock_bh(&ab->base_lock);

	if (init) {
		/* Apply the regd received during init through
		 * WMI_REG_CHAN_LIST_CC event. In case of failure to
		 * receive the regd, initialize with a default world
		 * regulatory.
		 */
		if (ab->default_regd[pdev_id]) {
			regd = ab->default_regd[pdev_id];
		} else {
			ath12k_warn(ab,
				    "failed to receive default regd during init\n");
			regd = (struct ieee80211_regdomain *)&ath12k_world_regd;
		}
	} else {
		regd = ab->new_regd[pdev_id];
	}

	if (!regd) {
		ret = -EINVAL;
		spin_unlock_bh(&ab->base_lock);
		goto err;
	}

	/* firmware provides reg rules which are similar for 2 GHz and 5 GHz
	 * pdev but 6 GHz pdev has superset of all rules including rules for
	 * all bands, we prefer 6 GHz pdev's rules to be used for setup of
	 * the wiphy regd.
	 * If 6 GHz pdev was part of the ath12k_hw, wait for the 6 GHz pdev,
	 * else pick the first pdev which calls this function and use its
	 * regd to update global hw regd.
	 * The regd_updated flag set at the end will not allow any further
	 * updates.
	 */
	if (strncmp(regd->alpha2, "00", 2) &&
	    (ah->use_6ghz_regd && !ar->supports_6ghz)) {
		spin_unlock_bh(&ab->base_lock);
		return 0;
	}

	regd_len = sizeof(*regd) + (regd->n_reg_rules *
		sizeof(struct ieee80211_reg_rule));

	regd_copy = kzalloc(regd_len, GFP_ATOMIC);
	if (regd_copy)
		ath12k_copy_regd(regd, regd_copy);

	spin_unlock_bh(&ab->base_lock);

	if (!regd_copy) {
		ret = -ENOMEM;
		goto err;
	}

	ret = regulatory_set_wiphy_regd(hw->wiphy, regd_copy);

	kfree(regd_copy);

	if (ret)
		goto err;

	if (ah->state != ATH12K_HW_STATE_ON)
		goto skip;

	ah->regd_updated = true;
skip:
	return 0;
err:
	ath12k_warn(ab, "failed to perform regd update : %d\n", ret);
	return ret;
}

static enum nl80211_dfs_regions
ath12k_map_fw_dfs_region(enum ath12k_dfs_region dfs_region)
{
	switch (dfs_region) {
	case ATH12K_DFS_REG_FCC:
	case ATH12K_DFS_REG_CN:
		return NL80211_DFS_FCC;
	case ATH12K_DFS_REG_ETSI:
		return NL80211_DFS_ETSI;
	case ATH12K_DFS_REG_MKK:
	case ATH12K_DFS_REG_MKK_N:
	case ATH12K_DFS_REG_KR:
		return NL80211_DFS_JP;
	default:
		return NL80211_DFS_UNSET;
	}
}

static u32 ath12k_update_bw_reg_flags(u16 max_bw)
{
	u32 flags = 0;
	switch (max_bw) {
	case 20:
		flags = (NL80211_RRF_NO_HT40 |
			NL80211_RRF_NO_80MHZ |
			NL80211_RRF_NO_160MHZ |
			NL80211_RRF_NO_320MHZ);
		break;
	case 40:
		flags = (NL80211_RRF_NO_80MHZ |
			NL80211_RRF_NO_160MHZ |
			NL80211_RRF_NO_320MHZ );
		break;
	case 80:
		flags = (NL80211_RRF_NO_160MHZ |
			NL80211_RRF_NO_320MHZ);
		break;
	case 160:
		flags = NL80211_RRF_NO_320MHZ;
		break;
	default:
		break;
	}

	return flags;
}

static u32 ath12k_map_fw_reg_flags(u16 reg_flags)
{
	u32 flags = 0;

	if (reg_flags & REGULATORY_CHAN_NO_IR)
		flags = NL80211_RRF_NO_IR;

	if (reg_flags & REGULATORY_CHAN_RADAR)
		flags |= NL80211_RRF_DFS;

	if (reg_flags & REGULATORY_CHAN_NO_OFDM)
		flags |= NL80211_RRF_NO_OFDM;

	if (reg_flags & REGULATORY_CHAN_INDOOR_ONLY)
		flags |= NL80211_RRF_NO_OUTDOOR;

	if (reg_flags & REGULATORY_CHAN_NO_HT40)
		flags |= NL80211_RRF_NO_HT40;

	if (reg_flags & REGULATORY_CHAN_NO_80MHZ)
		flags |= NL80211_RRF_NO_80MHZ;

	if (reg_flags & REGULATORY_CHAN_NO_160MHZ)
		flags |= NL80211_RRF_NO_160MHZ;

	return flags;
}

static u32 ath12k_map_fw_phy_flags(u32 phy_flags)
{
	u32 flags = 0;

	if (phy_flags & ATH12K_REG_PHY_BITMAP_NO11AX)
		flags |= NL80211_RRF_NO_HE;

	if (phy_flags & ATH12K_REG_PHY_BITMAP_NO11BE)
		flags |= NL80211_RRF_NO_EHT;

	return flags;
}

static void ath12k_reg_intersect_sp_rules(struct ath12k *ar,
					  struct ieee80211_reg_rule *rule1,
					  struct ieee80211_reg_rule *rule2,
					  struct ieee80211_reg_rule *new_rule)
{
	u32 start_freq1, end_freq1;
	u32 start_freq2, end_freq2;
	u32 freq_diff;

	start_freq1 = rule1->freq_range.start_freq_khz;
	start_freq2 = rule2->freq_range.start_freq_khz;

	end_freq1 = rule1->freq_range.end_freq_khz;
	end_freq2 = rule2->freq_range.end_freq_khz;

	new_rule->freq_range.start_freq_khz = max_t(u32, start_freq1,
						    start_freq2);

	new_rule->freq_range.end_freq_khz = min_t(u32, end_freq1, end_freq2);

	freq_diff = new_rule->freq_range.end_freq_khz -
		new_rule->freq_range.start_freq_khz;
	new_rule->freq_range.max_bandwidth_khz =
		min_t(u32, freq_diff, rule1->freq_range.max_bandwidth_khz);
	new_rule->power_rule.max_eirp = rule1->power_rule.max_eirp;
	/* Use the flags of both the rules */
	new_rule->flags = rule1->flags | rule2->flags;

	if ((rule1->flags & NL80211_RRF_PSD) && (rule2->flags & NL80211_RRF_PSD))
		new_rule->psd = min_t(s8, rule1->psd, rule2->psd);
	else
		new_rule->flags &= ~NL80211_RRF_PSD;

	new_rule->mode = NL80211_REG_AP_SP;

	/* To be safe, lets use the max cac timeout of both rules */
	new_rule->dfs_cac_ms = max_t(u32, rule1->dfs_cac_ms,
				     rule2->dfs_cac_ms);
	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Adding sp rule start freq %u end freq %u mac bw %u max eirp %d psd %d flags 0x%x\n",
		   new_rule->freq_range.start_freq_khz,
		   new_rule->freq_range.end_freq_khz,
		   new_rule->freq_range.max_bandwidth_khz,
		   new_rule->power_rule.max_eirp, new_rule->psd, new_rule->flags);
}

/* ath12k_reg_can_intersect() - Check whether two ieee80211_reg_rules can
 * intersect or not based on their frequency range and power mode.
 *
 * @rule1: Pointer to rule1
 * @rule2: Pointer to rule2
 *
 * Return: Return true if 2 rules can be intersected else false
 **/
static bool
ath12k_reg_can_intersect(struct ieee80211_reg_rule *rule1,
			 struct ieee80211_reg_rule *rule2)
{
	u32 start_freq1, end_freq1;
	u32 start_freq2, end_freq2;
 	u8 reg_6ghz_pwr_mode1, reg_6ghz_pwr_mode2;

	start_freq1 = rule1->freq_range.start_freq_khz;
	start_freq2 = rule2->freq_range.start_freq_khz;

	end_freq1 = rule1->freq_range.end_freq_khz;
	end_freq2 = rule2->freq_range.end_freq_khz;

	reg_6ghz_pwr_mode1 = rule1->mode;
	reg_6ghz_pwr_mode2 = rule2->mode;

	/* 6G reg rules can not intersect if power mode is not same.
	 * NOTE: For 2G/5G rules, it will be always 0.
	 */
	if (reg_6ghz_pwr_mode1 != reg_6ghz_pwr_mode2)
		return false;

	if ((start_freq1 >= start_freq2 && start_freq1 < end_freq2) ||
	    (start_freq2 > start_freq1 && start_freq2 < end_freq1))
		return true;

	/* TODO: Should we restrict intersection feasibility
	 *  based on min bandwidth of the intersected region also,
	 *  say the intersected rule should have a  min bandwidth
	 * of 20MHz?
	 */

	return false;
}

static const char *
ath12k_reg_get_regdom_str(enum nl80211_dfs_regions dfs_region)
{
	switch (dfs_region) {
	case NL80211_DFS_FCC:
		return "FCC";
	case NL80211_DFS_ETSI:
		return "ETSI";
	case NL80211_DFS_JP:
		return "JP";
	default:
		return "UNSET";
	}
}

static u16
ath12k_reg_adjust_bw(u16 start_freq, u16 end_freq, u16 max_bw)
{
	u16 bw;

	bw = end_freq - start_freq;
	bw = min_t(u16, bw, max_bw);

	if (bw >= 80 && bw < 160)
		bw = 80;
	else if (bw >= 40 && bw < 80)
		bw = 40;
	else if (bw < 40)
		bw = 20;

	return bw;
}

static void
ath12k_reg_update_rule(struct ieee80211_reg_rule *reg_rule, u32 start_freq,
		       u32 end_freq, u32 bw, u32 ant_gain, u32 reg_pwr,
		       s8 psd, u32 reg_flags,
		       enum nl80211_regulatory_power_modes pwr_mode)
{
	reg_rule->freq_range.start_freq_khz = MHZ_TO_KHZ(start_freq);
	reg_rule->freq_range.end_freq_khz = MHZ_TO_KHZ(end_freq);
	reg_rule->freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw);
	reg_rule->power_rule.max_antenna_gain = DBI_TO_MBI(ant_gain);
	reg_rule->power_rule.max_eirp = DBM_TO_MBM(reg_pwr);
	reg_rule->mode = pwr_mode;
	reg_rule->psd = psd;
	reg_rule->flags = reg_flags;
}

static void
ath12k_reg_update_weather_radar_band(struct ath12k_base *ab,
				     struct ieee80211_regdomain *regd,
				     struct ath12k_reg_rule *reg_rule,
				     u8 *rule_idx, u32 flags, u16 max_bw,
				     u8 *num_rules)
{
	u32 end_freq;
	u16 bw;
	u8 i;

	i = *rule_idx;

	bw = ath12k_reg_adjust_bw(reg_rule->start_freq,
				  ETSI_WEATHER_RADAR_BAND_LOW, max_bw);

	ath12k_reg_update_rule(regd->reg_rules + i, reg_rule->start_freq,
			       ETSI_WEATHER_RADAR_BAND_LOW, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       reg_rule->psd_eirp, flags, 0);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, reg_rule->start_freq, ETSI_WEATHER_RADAR_BAND_LOW,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	if (reg_rule->end_freq > ETSI_WEATHER_RADAR_BAND_HIGH)
		end_freq = ETSI_WEATHER_RADAR_BAND_HIGH;
	else
		end_freq = reg_rule->end_freq;

	bw = ath12k_reg_adjust_bw(ETSI_WEATHER_RADAR_BAND_LOW, end_freq,
				  max_bw);

	i++;

	ath12k_reg_update_rule(regd->reg_rules + i,
			       ETSI_WEATHER_RADAR_BAND_LOW, end_freq, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       reg_rule->psd_eirp, flags, 0);

	regd->reg_rules[i].dfs_cac_ms = ETSI_WEATHER_RADAR_BAND_CAC_TIMEOUT;

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, ETSI_WEATHER_RADAR_BAND_LOW, end_freq,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	if (end_freq == reg_rule->end_freq) {
		(*num_rules)--;
		*rule_idx = i;
		return;
	}

	bw = ath12k_reg_adjust_bw(ETSI_WEATHER_RADAR_BAND_HIGH,
				  reg_rule->end_freq, max_bw);

	i++;

	ath12k_reg_update_rule(regd->reg_rules + i, ETSI_WEATHER_RADAR_BAND_HIGH,
			       reg_rule->end_freq, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       reg_rule->psd_eirp, flags, 0);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, ETSI_WEATHER_RADAR_BAND_HIGH, reg_rule->end_freq,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	*rule_idx = i;
}

static const struct ath12k_op_class_map_t global_op_class[] = {
	{81, 25, BW20, BIT(BEHAV_NONE), 2407,
	  { 1,  2,  3,  4,  5,  6,  7,  8,  9,
	   10, 11, 12, 13},
	  NULL_CFIS_LST },
	{82, 25, BW20, BIT(BEHAV_NONE), 2414,
	  {14},
	  NULL_CFIS_LST },
	{83, 40, BW40_LOW_PRIMARY, BIT(BEHAV_BW40_LOW_PRIMARY), 2407,
	  { 1,  2,  3,  4,  5,  6,  7,  8,  9},
	  NULL_CFIS_LST },
	{84, 40, BW40_HIGH_PRIMARY, BIT(BEHAV_BW40_HIGH_PRIMARY), 2407,
	  { 5,  6,  7,  8,  9, 10, 11, 12, 13},
	  NULL_CFIS_LST },
	{115, 20, BW20, BIT(BEHAV_NONE), 5000,
	  {36, 40, 44, 48},
	  NULL_CFIS_LST },
	{116, 40, BW40_LOW_PRIMARY, BIT(BEHAV_BW40_LOW_PRIMARY), 5000,
	  {36, 44},
	  NULL_CFIS_LST },
	{117, 40, BW40_HIGH_PRIMARY, BIT(BEHAV_BW40_HIGH_PRIMARY), 5000,
	  {40, 48},
	  NULL_CFIS_LST },
	{118, 20, BW20, BIT(BEHAV_NONE), 5000,
	  {52, 56, 60, 64},
	  NULL_CFIS_LST },
	{119, 40, BW40_LOW_PRIMARY, BIT(BEHAV_BW40_LOW_PRIMARY), 5000,
	  {52, 60},
	  NULL_CFIS_LST },
	{120, 40, BW40_HIGH_PRIMARY, BIT(BEHAV_BW40_HIGH_PRIMARY), 5000,
	  {56, 64},
	  NULL_CFIS_LST },
	{121, 20, BW20, BIT(BEHAV_NONE), 5000,
	  {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144},
	  NULL_CFIS_LST },
	{122, 40, BW40_LOW_PRIMARY, BIT(BEHAV_BW40_LOW_PRIMARY), 5000,
	  {100, 108, 116, 124, 132, 140},
	  NULL_CFIS_LST },
	{123, 40, BW40_HIGH_PRIMARY, BIT(BEHAV_BW40_HIGH_PRIMARY), 5000,
	  {104, 112, 120, 128, 136, 144},
	  NULL_CFIS_LST },
	{125, 20, BW20, BIT(BEHAV_NONE), 5000,
	  {149, 153, 157, 161, 165, 169, 173, 177},
	  NULL_CFIS_LST },
	{126, 40, BW40_LOW_PRIMARY, BIT(BEHAV_BW40_LOW_PRIMARY), 5000,
	  {149, 157, 165, 173},
	  NULL_CFIS_LST },
	{127, 40, BW40_HIGH_PRIMARY, BIT(BEHAV_BW40_HIGH_PRIMARY), 5000,
	  {153, 161, 169, 177},
	  NULL_CFIS_LST },
	{128, 80, BW80, BIT(BEHAV_NONE), 5000,
	  { 36,  40,  44,  48,  52,  56,  60,  64, 100,
	   104, 108, 112, 116, 120, 124, 128, 149, 153,
	   157, 161, 165, 169, 173, 177},
	  NULL_CFIS_LST },
	{129, 160, BW80, BIT(BEHAV_NONE), 5000,
	  { 36,  40,  44,  48,  52,  56,  60,  64, 100,
	   104, 108, 112, 116, 120, 124, 128, 149, 153,
	   157, 161, 165, 169, 173, 177},
	  NULL_CFIS_LST },
	{130, 80, BW80, BIT(BEHAV_BW80_PLUS), 5000,
	  { 36,  40,  44,  48,  52,  56,  60,  64, 100,
	   104, 108, 112, 116, 120, 124, 128, 149, 153,
	   157, 161, 165, 169, 173, 177},
	  NULL_CFIS_LST },

	{131, 20, BW20, BIT(BEHAV_NONE), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(131)},

	{132, 40, BW40_LOW_PRIMARY, BIT(BEHAV_NONE), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(132)},

	{133, 80, BW80, BIT(BEHAV_NONE), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(133)},

	{134, 160, BW80, BIT(BEHAV_NONE), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(134)},

	{135, 80, BW80, BIT(BEHAV_BW80_PLUS), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(135)},

	{136, 20, BW20, BIT(BEHAV_NONE), 5925,
	  {2},
	  &CFISLST(136)},
	{137, 320, BW20, BIT(BEHAV_NONE), 5950,
	  {  1,   5,   9,  13,  17,  21,  25,  29,  33,
	    37,  41,  45,  49,  53,  57,  61,  65,  69,
	    73,  77,  81,  85,  89,  93,  97, 101, 105,
	   109, 113, 117, 121, 125, 129, 133, 137, 141,
	   145, 149, 153, 157, 161, 165, 169, 173, 177,
	   181, 185, 189, 193, 197, 201, 205, 209, 213,
	   217, 221, 225, 229, 233},
	  &CFISLST(137)},
	{0, 0, 0, 0, 0, {0},
	  NULL_CFIS_LST },
};

static void ath12k_copy_reg_rule(struct ath12k_reg_freq *ath12k_reg_freq,
				 struct ath12k_reg_rule *reg_rule)
{
	if (!ath12k_reg_freq->start_freq)
		ath12k_reg_freq->start_freq = reg_rule->start_freq;

	if (!ath12k_reg_freq->end_freq || ath12k_reg_freq->end_freq < reg_rule->end_freq)
		ath12k_reg_freq->end_freq = reg_rule->end_freq;
}

struct ieee80211_regdomain *
ath12k_reg_build_regd(struct ath12k_base *ab,
		      struct ath12k_reg_info *reg_info)
{
	struct ieee80211_regdomain *updated_new_regd = NULL;
	struct ieee80211_regdomain *new_regd = NULL;
	struct ath12k_reg_rule *reg_rule, *reg_rule_6g;
	u8 i = 0, j = 0, k = 0, idx = 0;
	u8 num_rules, num_6ghz_sp_rules;
	u16 max_bw = 0;
	int max_elements = 0, sp_idx = 0;
	struct ath12k_6ghz_sp_reg_rule *sp_rule = NULL;
	u32 flags = 0, reg_6g_number = 0, max_bw_6g = 0;
	char alpha2[3];
	bool reg_6g_itr_set = false;
	enum nl80211_regulatory_power_modes pwr_mode;

	num_rules = reg_info->num_5g_reg_rules + reg_info->num_2g_reg_rules;

	/* FIXME: Currently taking reg rules for 6G only from Indoor AP mode list.
	 * This can be updated to choose the combination dynamically based on AP
	 * type and client type, after complete 6G regulatory support is added.
	 */
	if (reg_info->is_ext_reg_event) {
		/* All 6G APs - (LP, SP, VLP) */
		for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
			if (i == WMI_REG_STD_POWER_AP)
				num_6ghz_sp_rules = reg_info->num_6g_reg_rules_ap[i];
			reg_6g_number += reg_info->num_6g_reg_rules_ap[i];
		}
		/* All 6G STAs - (LP_DEF, LP_SUB, SP_DEF, SP_SUB, VLP_DEF, VLP_SUB) */
		for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
			for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++)
				reg_6g_number += reg_info->num_6g_reg_rules_cl[j][i];
		}
	}
	ath12k_dbg(ab, ATH12K_DBG_AFC, "Number of sp rules from target %d\n",
		   num_6ghz_sp_rules);
	num_rules += (reg_6g_number - num_6ghz_sp_rules);

	if (!num_rules)
		goto ret;


	/* 6 GHz standard power rules should not be updated to cfg
	 * until we get afc data with valid rules. Hence save the sp
	 * rule in ab and use this to intersect with afc rules when we get
	 * afc power update
	 */

	sp_rule = kzalloc(sizeof(*sp_rule) +
			  (num_6ghz_sp_rules * sizeof(struct ieee80211_reg_rule)),
			  GFP_ATOMIC);

	if (!sp_rule)
		return new_regd;

	sp_rule->num_6ghz_sp_rule = num_6ghz_sp_rules;

	new_regd = kzalloc(sizeof(*new_regd) +
			   (num_rules * sizeof(struct ieee80211_reg_rule)),
			   GFP_ATOMIC);
	if (!new_regd) {
		kfree(sp_rule);
		goto ret;
}

	memcpy(new_regd->alpha2, reg_info->alpha2, REG_ALPHA2_LEN + 1);
	memcpy(alpha2, reg_info->alpha2, REG_ALPHA2_LEN + 1);
	alpha2[2] = '\0';
	new_regd->dfs_region = ath12k_map_fw_dfs_region(reg_info->dfs_region);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\r\nCountry %s, CFG Regdomain %s FW Regdomain %d, num_reg_rules %d\n",
		   alpha2, ath12k_reg_get_regdom_str(new_regd->dfs_region),
		   reg_info->dfs_region, num_rules);
	/* Update reg_rules[] below. Firmware is expected to
	 * send these rules in order(2G rules first and then 5G)
	 */
	for (i = 0, j = 0, idx = 0; i < num_rules + num_6ghz_sp_rules ; i++) {
		if (reg_info->num_2g_reg_rules &&
		    (i < reg_info->num_2g_reg_rules)) {
			reg_rule = reg_info->reg_rules_2g_ptr + i;
			max_bw = min_t(u16, reg_rule->max_bw,
				       reg_info->max_bw_2g);
			flags = ath12k_update_bw_reg_flags(reg_info->max_bw_2g);
			pwr_mode = 0;
			ath12k_copy_reg_rule(&ab->reg_freq_2g, reg_rule);
		} else if (reg_info->num_5g_reg_rules &&
			   (j < reg_info->num_5g_reg_rules)) {
			reg_rule = reg_info->reg_rules_5g_ptr + j++;
			max_bw = min_t(u16, reg_rule->max_bw,
				       reg_info->max_bw_5g);

			/* FW doesn't pass NL80211_RRF_AUTO_BW flag for
			 * BW Auto correction, we can enable this by default
			 * for all 5G rules here. The regulatory core performs
			 * BW correction if required and applies flags as
			 * per other BW rule flags we pass from here
			 */
			flags = NL80211_RRF_AUTO_BW | ath12k_update_bw_reg_flags(reg_info->max_bw_5g);
			pwr_mode = 0;
			if (reg_rule->end_freq <= ATH12K_MAX_5GHZ_FREQ)
				ath12k_copy_reg_rule(&ab->reg_freq_5g, reg_rule);
			else if (reg_rule->start_freq >= ATH12K_MIN_6GHZ_FREQ)
				ath12k_copy_reg_rule(&ab->reg_freq_6g, reg_rule);
		} else if (reg_info->is_ext_reg_event && reg_6g_number) {
			if (!reg_6g_itr_set) {
				reg_rule_6g = ath12k_get_active_6g_reg_rule(reg_info,
						&max_bw_6g, &max_elements, &pwr_mode);

				if (!reg_rule_6g) {
					ath12k_warn(ab,
							"\nFetching a valid reg_rule_6g_ptr failed."
							"This shouldn't happen normally. Be carefull with"
							"the regulatory domain settings\n");
					break;
				}
				reg_6g_itr_set = true;
			}

			if (reg_6g_itr_set && k < max_elements) {
				reg_rule = reg_rule_6g + k++;
				max_bw = min_t(u16, reg_rule->max_bw, max_bw_6g);
				flags = NL80211_RRF_AUTO_BW | ath12k_update_bw_reg_flags(max_bw_6g);

				if (reg_rule->psd_flag)
					flags |= NL80211_RRF_PSD;

				if (reg_rule->end_freq <= ATH12K_MAX_6GHZ_FREQ)
					ath12k_copy_reg_rule(&ab->reg_freq_6g, reg_rule);
				else if (reg_rule->start_freq >= ATH12K_MIN_6GHZ_FREQ)
					ath12k_copy_reg_rule(&ab->reg_freq_6g, reg_rule);
			}

			if (reg_6g_itr_set && k >= max_elements) {
				reg_6g_itr_set = false;
				reg_rule_6g = NULL;
				max_bw_6g = 0;
				max_elements = 0;
				k = 0;
			}
			reg_6g_number--;
		} else {
			break;
		}

		flags |= ath12k_map_fw_reg_flags(reg_rule->flags);
		flags |= ath12k_map_fw_phy_flags(reg_info->phybitmap);

		if (pwr_mode == NL80211_REG_AP_SP) {
			ath12k_reg_update_rule(sp_rule->sp_reg_rule + sp_idx,
					       reg_rule->start_freq,
					       reg_rule->end_freq, max_bw,
					       reg_rule->ant_gain, reg_rule->reg_power,
					       reg_rule->psd_eirp, flags, pwr_mode);
			ath12k_dbg(ab, ATH12K_DBG_AFC,
				   "Target sp rule freq low: %d high: %d bw: %d psd: %d flag: 0x%x\n",
				   reg_rule->start_freq, reg_rule->end_freq,
				   max_bw, reg_rule->psd_eirp, flags);
		} else {
			ath12k_reg_update_rule(new_regd->reg_rules + idx,
					       reg_rule->start_freq,
					       reg_rule->end_freq, max_bw,
					       reg_rule->ant_gain, reg_rule->reg_power,
					       reg_rule->psd_eirp, flags, pwr_mode);
		}

		/* Update dfs cac timeout if the dfs domain is ETSI and the
		 * new rule covers weather radar band.
		 * Default value of '0' corresponds to 60s timeout, so no
		 * need to update that for other rules.
		 */
		if (flags & NL80211_RRF_DFS &&
		    reg_info->dfs_region == ATH12K_DFS_REG_ETSI &&
		    (reg_rule->end_freq > ETSI_WEATHER_RADAR_BAND_LOW &&
		     reg_rule->start_freq < ETSI_WEATHER_RADAR_BAND_HIGH)){
			num_rules += 2;
			updated_new_regd = krealloc(new_regd, sizeof(*new_regd) +
						    (num_rules * sizeof(struct ieee80211_reg_rule)), GFP_ATOMIC);
			if (!updated_new_regd) {
				ath12k_err(ab, "Failed to realloc new_regd with number of rules as %d\n",
					   num_rules);
				break;
			}

			new_regd = updated_new_regd;
			ath12k_reg_update_weather_radar_band(ab, new_regd,
							     reg_rule, &idx,
							     flags, max_bw, &num_rules);
			idx++;
			continue;
		}

		if (reg_info->is_ext_reg_event) {
			ath12k_dbg(ab, ATH12K_DBG_REG, "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d) (%d, %d) (6G_POWER_MODE: %d)\n",
				   idx + 1, reg_rule->start_freq, reg_rule->end_freq,
				   max_bw, reg_rule->ant_gain, reg_rule->reg_power,
				   new_regd->reg_rules[idx].dfs_cac_ms,
				   flags, reg_rule->psd_flag, reg_rule->psd_eirp,
				   new_regd->reg_rules[idx].mode);
		} else {
			ath12k_dbg(ab, ATH12K_DBG_REG,
				   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
				   idx + 1, reg_rule->start_freq, reg_rule->end_freq,
				   max_bw, reg_rule->ant_gain, reg_rule->reg_power,
				   new_regd->reg_rules[idx].dfs_cac_ms,
				   flags);
		}
		if (pwr_mode == NL80211_REG_AP_SP)
			sp_idx++;
		else
			idx++;
	}

	kfree(ab->sp_rule);
	ab->sp_rule = sp_rule;
	new_regd->n_reg_rules = num_rules;

ret:
	return new_regd;
}

/* ath12k_reg_coalesce_afc_freq_info() - Coalesce the freq info objects received
 * in the AFC power event.
 *
 * @afc_reg_info: Pointer to AFC response
 *
 * When freq info objects are a continuous sequence, coalesce them. The max_psd
 * of the coalesced freq info obj should be the minimum of all psd values of the
 * freq info objects that are being coalesced.
 * Eg: If the freq info objects are as follows
 * {freq_low: 6330, freq_high: 6361, max_psd: 11}
 * {freq_low: 6361, freq_high: 6425, max_psd: 23}
 * {freq_low: 6525, freq_high: 6540, max_psd: 23}
 * {freq_low: 6540, freq_high: 6570, max_psd: 22}
 *
 * These should be coalesced into
 * {freq_low: 6330, freq_high: 6425, max_psd: 11}
 * {freq_low: 6525, freq_high: 6570, max_psd: 22}
 **/
static void
ath12k_reg_coalesce_afc_freq_info(struct ath12k_afc_sp_reg_info *afc_reg_info)
{
	struct ath12k_afc_freq_obj *afc_freq_info = afc_reg_info->afc_freq_info;
	u8 num_freq_objs = afc_reg_info->num_freq_objs;
	int start_idx = 0;
	int next_idx = start_idx + 1;

	while (next_idx < num_freq_objs) {
		if (afc_freq_info[start_idx].high_freq ==
		    afc_freq_info[next_idx].low_freq) {
			afc_freq_info[start_idx].high_freq = afc_freq_info[next_idx].high_freq;
			afc_freq_info[start_idx].max_psd = min(afc_freq_info[start_idx].max_psd,
							       afc_freq_info[next_idx].max_psd);
			next_idx++;
			afc_reg_info->num_freq_objs--;
		} else {
			start_idx++;
			afc_freq_info[start_idx].low_freq = afc_freq_info[next_idx].low_freq;
			afc_freq_info[start_idx].high_freq = afc_freq_info[next_idx].high_freq;
			afc_freq_info[start_idx].max_psd = afc_freq_info[next_idx].max_psd;
			next_idx++;
		}
	}
}

int ath12k_reg_get_6ghz_opclass_from_bw(int bw, int cfi)
{
	int opclass;

	switch (bw) {
	case NL80211_CHAN_WIDTH_20:
		opclass = 131;
		/* According spec Table E-4 Global operating classes */
		if (cfi == 2)
			opclass = 136;
		break;
	case NL80211_CHAN_WIDTH_40:
		opclass = 132;
		break;
	case NL80211_CHAN_WIDTH_80:
		opclass = 133;
		break;
	case NL80211_CHAN_WIDTH_160:
		opclass = 134;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		opclass = 135;
		break;
	case NL80211_CHAN_WIDTH_320:
		opclass = 137;
		break;
	default:
		opclass = 0;
	}

	return opclass;
}

s8 ath12k_reg_get_afc_eirp_power(struct ath12k *ar, enum nl80211_chan_width bw, int cfi)
{
	u16 eirp_pwr = 0;
	struct ath12k_afc_sp_reg_info *reg_info = ar->afc.afc_reg_info;
	struct ath12k_afc_chan_obj *afc_chan;
	struct ath12k_chan_eirp_obj *chan_eirp;
	int op_class, i, j;

	spin_lock_bh(&ar->data_lock);

	if (!ar->afc.afc_reg_info) {
		ath12k_warn(ar->ab, "AFC power info not found\n");
		goto ret;
	}

	op_class = ath12k_reg_get_6ghz_opclass_from_bw(bw, cfi);
	if (!op_class) {
		ath12k_warn(ar->ab, "Invalid opclass for 6 GHz bw\n");
		goto ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Configured BW belong to op_class %d cfi %d\n",
		   op_class, cfi);

	for (i = 0;  i < reg_info->num_chan_objs; i++) {
		afc_chan = reg_info->afc_chan_info + i;

		if (afc_chan->global_opclass != op_class)
			continue;

		for (j = 0; j < afc_chan->num_chans; j++) {
			chan_eirp = afc_chan->chan_eirp_info + j;
			if (chan_eirp->cfi == cfi) {
				eirp_pwr = chan_eirp->eirp_power;
				break;
			}
		}

		if (eirp_pwr)
			break;
	}

	eirp_pwr = eirp_pwr / 100;

ret:
	spin_unlock_bh(&ar->data_lock);
	return eirp_pwr;
}

void ath12k_reg_get_afc_eirp_power_for_bw(struct ath12k *ar, u16 *start_freq,
					  u16 *center_freq, int pwr_level,
					  struct cfg80211_chan_def *chan_def,
					  s8 *tx_power)
{
	int bw = 0, cfi;

	if (chan_def->width == NL80211_CHAN_WIDTH_80P80 && pwr_level == 3)
		*center_freq = (u16)chan_def->center_freq2;
	else
		*center_freq = *start_freq + (10 * (BIT(pwr_level) - 1));

	/* For 20 MHz, no +10 offset is required */
	if (pwr_level != 0)
		*center_freq += 10;

	/* power level is directly correlated to enum nl80211_chan_width
	 * plus one as power level starts from 0
	 */
	if (pwr_level < 3)
		bw = pwr_level + 1;
	else if (pwr_level == 3)
		bw = chan_def->width;

	cfi = ieee80211_frequency_to_channel(*center_freq);
	*tx_power = ath12k_reg_get_afc_eirp_power(ar, bw, cfi);
}

/**
 * ath12k_is_reg_rule_subset_of_chip_range() - Check if the rule range
 * is subset of chip range
 * @rule_range: rule range
 * @chip_range: chip range
 *
 * Return: true if rule range is subset of chip range
 */
static bool
ath12k_is_reg_rule_subset_of_chip_range(struct ieee80211_freq_range rule_range,
					struct ieee80211_freq_range chip_range)
{
	if (rule_range.start_freq_khz >= chip_range.start_freq_khz &&
	    rule_range.end_freq_khz <= chip_range.end_freq_khz)
		return true;

	return false;
}

/**
 * ath12k_get_current_regd() - Get the current regulatory domain
 * @ar: pointer to ath12k
 *
 * Return: pointer to the current regulatory domain or NULL in case of error
 */
static struct ieee80211_regdomain *ath12k_get_current_regd(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_regdomain *regd;

	spin_lock_bh(&ab->base_lock);
	if (ab->new_regd[ar->pdev_idx])
		regd = ab->new_regd[ar->pdev_idx];
	else
		regd = ab->default_regd[ar->pdev_idx];

	if (!regd) {
		ath12k_warn(ab, "Regulatory domain data not present\n");
		goto end;
	}
end:
	spin_unlock_bh(&ab->base_lock);
	return regd;
}

/**
 * ath12k_mark_sp_reg_rules_as_no_ir() - Mark the SP rules as NO_IR
 * @ar: pointer to ath12k
 * @regd: pointer to the regulatory domain
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_mark_sp_reg_rules_as_no_ir(struct ath12k *ar,
				  struct ieee80211_regdomain *regd)
{
	struct ieee80211_freq_range chip_range = {0};
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *reg_cap;
	struct ath12k_base *ab = ar->ab;
	u8 i;

	reg_cap = &ab->hal_reg_cap[ar->pdev_idx];
	if (!reg_cap) {
		ath12k_warn(ab, "Regulatory capabilities not present for pdev_id: %u\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	chip_range.start_freq_khz = MHZ_TO_KHZ(reg_cap->low_5ghz_chan);
	chip_range.end_freq_khz = MHZ_TO_KHZ(reg_cap->high_5ghz_chan);

	for (i = 0; i < regd->n_reg_rules; i++) {
		struct ieee80211_reg_rule *new_rule;

		new_rule =  regd->reg_rules + i;
		if (new_rule->mode != NL80211_REG_AP_SP)
			continue;
		if (ath12k_is_reg_rule_subset_of_chip_range(new_rule->freq_range,
							    chip_range))
			new_rule->flags |= NL80211_RRF_NO_IR;
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Add NO_IR flag SP rule %u, s_freq %u, e_freq %u\n",
			   i, new_rule->freq_range.start_freq_khz,
			   new_rule->freq_range.end_freq_khz);
	}
	return 0;
}

/**
 * ath12k_handle_invalid_afc_payload() - Handle invalid AFC payload by
 * marking all the SP channels as NO_IR.
 * @ar: pointer to ath12k
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_handle_invalid_afc_payload(struct ath12k *ar)
{
	struct ieee80211_regdomain *regd;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_base *ab = ar->ab;
	int ret = 0;
	struct ath12k_6ghz_sp_reg_rule *sp_rule;

	regd = ath12k_get_current_regd(ar);
	if (!regd) {
		ath12k_warn(ab, "Regulatory domain data not present\n");
		return -EINVAL;
	}
	if (!ab->sp_rule) {
		ath12k_warn(ab, "SP rules not present for pdev: %u\n", ar->pdev_idx);
		return -EINVAL;
	}

	sp_rule = ab->sp_rule;
	if (!sp_rule->num_6ghz_sp_rule) {
		ath12k_warn(ab, "No default 6 GHz sp rules present\n");
		return -EINVAL;
	}

	ar->afc.is_6ghz_afc_power_event_received = false;
	ret = ath12k_mark_sp_reg_rules_as_no_ir(ar, regd);
	if (ret) {
		ath12k_warn(ab, "Failed to mark SP rules as NO_IR\n");
		return ret;
	}
	ah->regd_updated = false;
	queue_work(ab->workqueue, &ar->regd_update_work);
	return ret;
}

int ath12k_reg_process_afc_power_event(struct ath12k *ar)
{
	int new_reg_rule_cnt, num_regd_rules, num_afc_rules, num_sp_rules;
	int ret = 0, num_old_sp_rules = 0, num_new_sp_rules = 0;
	struct ieee80211_reg_rule *old_rule, *new_regd_rules;
	struct ath12k_afc_sp_reg_info *afc_reg_info = NULL;
	struct ath12k_6ghz_sp_reg_rule *sp_rule = NULL;
	struct ieee80211_regdomain *new_regd = NULL;
	struct ieee80211_regdomain *old_regd = NULL;
	struct ath12k_afc_freq_obj *afc_freq_info;
	struct ath12k_afc_freq_obj *afc_freq_obj;
	struct ieee80211_reg_rule new_rule = {0};
	struct ieee80211_regdomain *regd = NULL;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw *ah = ar->ah;
	int i, j, k, pdev_idx;
	char alpha2[3] = {0};
	u16 is_afc_chan_or_freq_obj_empty;

	pdev_idx = ar->pdev_idx;

	if (!ab->sp_rule || !ar->afc.afc_reg_info)
		return -EINVAL;

	spin_lock_bh(&ar->data_lock);
	sp_rule = ab->sp_rule;
	afc_reg_info = ar->afc.afc_reg_info;
	afc_freq_info = afc_reg_info->afc_freq_info;
	is_afc_chan_or_freq_obj_empty = (afc_reg_info->num_chan_objs == 0) ||
					(afc_reg_info->num_freq_objs == 0);

	if (afc_reg_info->fw_status_code != REG_FW_AFC_POWER_EVENT_SUCCESS ||
	    is_afc_chan_or_freq_obj_empty) {
		ath12k_warn(ab, "AFC Power event failure status code %d, afc_rule_empty: %d\n",
			    afc_reg_info->fw_status_code, is_afc_chan_or_freq_obj_empty);
		ath12k_handle_invalid_afc_payload(ar);
		ret = -EINVAL;
		goto end;
	}

	if (!sp_rule->num_6ghz_sp_rule) {
		ath12k_warn(ab, "No default 6 GHz sp rules present\n");
		ret = -EINVAL;
		goto end;
	}

	ar->afc.is_6ghz_afc_power_event_received = true;

	if (ab->new_regd[pdev_idx])
		regd = ab->new_regd[pdev_idx];
	else
		regd = ab->default_regd[pdev_idx];

	if (!regd) {
		ath12k_warn(ab, "Regulatory domain data not present\n");
		ret = -EINVAL;
		goto end;
	}

	num_sp_rules = ab->sp_rule->num_6ghz_sp_rule;
	num_regd_rules = regd->n_reg_rules;
	ath12k_reg_coalesce_afc_freq_info(afc_reg_info);
	num_afc_rules = afc_reg_info->num_freq_objs;

	for (i = 0; i < num_regd_rules; i++) {
		old_rule = regd->reg_rules + i;
		if (old_rule->mode == NL80211_REG_AP_SP)
			num_old_sp_rules++;
	}

	for (i = 0; i < num_sp_rules; i++) {
		old_rule = sp_rule->sp_reg_rule + i;

		for (j = 0; j < num_afc_rules; j++) {
			afc_freq_obj = afc_freq_info + j;
			new_rule.freq_range.start_freq_khz =
						MHZ_TO_KHZ(afc_freq_obj->low_freq);
			new_rule.freq_range.end_freq_khz =
						MHZ_TO_KHZ(afc_freq_obj->high_freq);
			new_rule.mode = NL80211_REG_AP_SP;
			if (ath12k_reg_can_intersect(old_rule, &new_rule))
				num_new_sp_rules++;
		}
	}

	/* Remove the old sp rule from regd and add the new intersected sp rules */
	new_reg_rule_cnt = num_regd_rules  - num_old_sp_rules + num_new_sp_rules;
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "Total reg rules %d old sp rules %d new sp rules after intersection %d\n",
		   num_regd_rules, num_old_sp_rules, num_new_sp_rules);

	new_regd = kzalloc(sizeof(*new_regd) +
			   (sizeof(*new_regd_rules) * new_reg_rule_cnt),
			   GFP_ATOMIC);
	if (!new_regd) {
		ret = -ENOMEM;
		goto end;
	}

	new_regd->n_reg_rules = new_reg_rule_cnt;
	memcpy(new_regd->alpha2, regd->alpha2, REG_ALPHA2_LEN + 1);
	memcpy(alpha2, regd->alpha2, REG_ALPHA2_LEN + 1);
	alpha2[2] = '\0';
	new_regd->dfs_region = regd->dfs_region;
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\nAFC: Country %s, CFG Regdomain %s, num_reg_rules %d\n",
		   alpha2, ath12k_reg_get_regdom_str(new_regd->dfs_region),
		   new_reg_rule_cnt);
	k = 0;
	for (i = 0; i < num_regd_rules; i++) {
		old_rule = regd->reg_rules + i;

		if (old_rule->mode == NL80211_REG_AP_SP) {
			continue;
		} else {
			memcpy((new_regd->reg_rules + k), old_rule, sizeof(*new_regd_rules));
			k++;
		}
	}

	for (i = 0; i < num_sp_rules; i++) {
		old_rule = sp_rule->sp_reg_rule + i;
		for (j = 0; j < num_afc_rules; j++) {
			afc_freq_obj = afc_freq_info + j;
				new_rule.freq_range.start_freq_khz =
						MHZ_TO_KHZ(afc_freq_obj->low_freq);
				new_rule.freq_range.end_freq_khz =
						MHZ_TO_KHZ(afc_freq_obj->high_freq);
				new_rule.mode = NL80211_REG_AP_SP;
				if (ath12k_reg_can_intersect(old_rule, &new_rule)) {
					new_rule.psd = (s8)(afc_freq_obj->max_psd / 100);
					new_rule.flags |= NL80211_RRF_PSD;
					ath12k_reg_intersect_sp_rules(ar, old_rule, &new_rule,
								      new_regd->reg_rules + k);
					k++;
				}
			}
	}

	spin_unlock_bh(&ar->data_lock);

	spin_lock_bh(&ab->base_lock);
	old_regd = ab->new_regd[pdev_idx];
	ab->new_regd[pdev_idx] = new_regd;
	spin_unlock_bh(&ab->base_lock);
	kfree(old_regd);

	ah->regd_updated = false;
	queue_work(ab->workqueue, &ar->regd_update_work);
	return ret;
end:
	spin_unlock_bh(&ar->data_lock);
	return ret;
}

int ath12k_copy_afc_response(struct ath12k *ar, char *afc_resp, u32 len)
{
	struct ath12k_base *ab = ar->ab;
	struct target_mem_chunk *target_mem = ab->qmi.target_mem;
	void __iomem *mem = NULL;
	int i;
	int slotid = ar->pdev_idx;
	u32 *status;

	if (len > AFC_SLOT_SIZE) {
		ath12k_warn(ab, "len %d greater than slot size\n", len);
		return -EINVAL;
	}

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		if (target_mem[i].type == AFC_REGION_TYPE) {
			mem = target_mem[i].v.addr;
			status = mem + (slotid * AFC_SLOT_SIZE);
			break;
		}
	}

	if (!mem) {
		ath12k_warn(ab, "AFC mem is not available\n");
		return -ENOMEM;
	}

	status[AFC_AUTH_STATUS_OFFSET] = cpu_to_le32(AFC_AUTH_ERROR);
	if (ab->hif.bus == ATH12K_BUS_HYBRID) {
		memset_io(mem + (slotid * AFC_SLOT_SIZE), 0, AFC_SLOT_SIZE);
		memcpy_toio(mem + (slotid * AFC_SLOT_SIZE), afc_resp, len);
	} else {
		memset(mem + (slotid * AFC_SLOT_SIZE), 0, AFC_SLOT_SIZE);
		memcpy(mem + (slotid * AFC_SLOT_SIZE), afc_resp, len);
	}

	status[AFC_AUTH_STATUS_OFFSET] = cpu_to_le32(AFC_AUTH_SUCCESS);

	return 0;
}

/**
 * ath12k_is_6ghz_op_class - Check if operating class is 6GHz
 * @op_class: Operating class
 *
 * Return: true if 6GHz, false otherwise
 */
static bool ath12k_is_6ghz_op_class(u8 op_class)
{
	return ((op_class >= ATH12K_MIN_6GHZ_OPER_CLASS) &&
		(op_class <= ATH12K_MAX_6GHZ_OPER_CLASS));
}

/**
 * ath12k_is_range_valid - Check if frequency range is valid
 * @range: Pointer to frequency range
 *
 * Return: true if valid, false otherwise
 */
static bool ath12k_is_range_valid(struct ieee80211_freq_range *range)
{
	return (range->end_freq_khz > range->start_freq_khz);
}

/**
 * ath12k_is_subrange - Check if range_first is a subrange of range_second
 * @range_first: First frequency range
 * @range_second: Second frequency range
 *
 * Return: true if subrange, false otherwise
 */
static bool ath12k_is_subrange(struct ieee80211_freq_range *range_first,
			       struct ieee80211_freq_range *range_second)
{
	bool is_subrange;
	bool is_valid;

	is_valid = ath12k_is_range_valid(range_first) &&
		ath12k_is_range_valid(range_second);

	if (!is_valid)
		return false;

	is_subrange = (range_first->start_freq_khz >= range_second->start_freq_khz) &&
			(range_first->end_freq_khz <= range_second->end_freq_khz);

	return is_subrange;
}

/**
 * ath12k_is_cfi_freq_in_ranges - Check if CFI frequency is in ranges
 * @cfi_freq: CFI frequency
 * @bw: Bandwidth
 * @p_frange_lst: Pointer to frequency range list
 *
 * This function checks if the CFI frequency is within the given ranges.
 *
 * Return: true if in ranges, false otherwise
 */
static bool ath12k_is_cfi_freq_in_ranges(u16 cfi_freq, u16 bw,
					 struct ath12k_afc_frange_list *p_frange_lst)
{
	struct ath12k_afc_freq_range_obj *p_range_objs;
	struct ieee80211_freq_range range_cfi;
	u16 cfi_band_left, cfi_band_right;
	bool is_cfi_supported = false;
	u32 num_ranges;
	u8 i;

	num_ranges = p_frange_lst->num_ranges;
	p_range_objs = &p_frange_lst->range_objs[0];
	cfi_band_left = cfi_freq - bw / 2;
	cfi_band_right = cfi_freq + bw / 2;
	range_cfi.start_freq_khz = MHZ_TO_KHZ(cfi_band_left);
	range_cfi.end_freq_khz = MHZ_TO_KHZ(cfi_band_right);

	for (i = 0; i <  num_ranges; i++) {
		struct ieee80211_freq_range range_chip;

		range_chip.start_freq_khz = MHZ_TO_KHZ(p_range_objs->lowfreq);
		range_chip.end_freq_khz = MHZ_TO_KHZ(p_range_objs->highfreq);
		is_cfi_supported = ath12k_is_subrange(&range_cfi, &range_chip);

		if (is_cfi_supported)
			return true;

		p_range_objs++;
	}

	return is_cfi_supported;
}

/**
 * ath12k_fill_cfis - Fill CFIs
 * @op_class_tbl: Pointer to operating class table
 * @p_lst: Pointer to frequency list
 * @p_frange_lst: Pointer to frequency range list
 * @dst: Destination buffer
 *
 * This function fills the CFIs (Channel Frequency Indicators) for a given
 * operating class. It iterates over the CFIs in the frequency list and checks
 * if each CFI frequency is within the ranges specified in the frequency range
 * list. If the CFI frequency is within the ranges, it is added to the
 * destination buffer.
 *
 * Return: Number of valid CFIs
 */
static u8 ath12k_fill_cfis(const struct ath12k_op_class_map_t *op_class_tbl,
			   const struct ath12k_c_freq_lst *p_lst,
			   struct ath12k_afc_frange_list *p_frange_lst,
			   u8 *dst)
{
	u8 j;
	u8 cfi_idx = 0;

	for (j = 0; j < p_lst->num_cfis; j++) {
		u8 cfi;
		u16 cfi_freq;
		u16 start_freq = op_class_tbl->start_freq;
		u16 bw = op_class_tbl->chan_spacing;

		cfi = p_lst->p_cfis_arr[j];
		cfi_freq = start_freq + ATH12K_FREQ_TO_CHAN_SCALE * cfi;

		if (ath12k_is_cfi_freq_in_ranges(cfi_freq, bw, p_frange_lst))
			dst[cfi_idx++] = cfi;
	}
	return cfi_idx;
}

/**
 * ath12k_fill_6g_opcls_chan_lists - Fill 6GHz operating class channel lists
 * @p_frange_lst: Pointer to frequency range list
 * @chansize_lst: Channel size list
 * @channel_lists: Channel lists
 *
 */
static void ath12k_fill_6g_opcls_chan_lists(struct ath12k_afc_frange_list *p_frange_lst,
					    u8 chansize_lst[], u8 *channel_lists[])
{
	const struct ath12k_op_class_map_t *op_class_tbl;
	u8 i = 0;

	op_class_tbl = global_op_class;
	while (op_class_tbl && op_class_tbl->op_class) {
		const struct ath12k_c_freq_lst *p_lst;

		p_lst = op_class_tbl->p_cfi_lst_obj;
		if (p_lst && ath12k_is_6ghz_op_class(op_class_tbl->op_class)) {
			u8 *dst;
			u8 num_valid_cfi = 0;

			dst = channel_lists[i];
			if (!dst)
				return;

			num_valid_cfi = ath12k_fill_cfis(op_class_tbl, p_lst,
							 p_frange_lst, dst);
			if (num_valid_cfi)
				i++;
		}
		op_class_tbl++;
	}
}

/**
 * ath12k_get_num_6g_opclasses - Get number of 6GHz operating classes
 *
 * Return: Number of 6GHz operating classes
 */
static u8 ath12k_get_num_6g_opclasses(void)
{
	const struct ath12k_op_class_map_t *op_class_tbl;
	u8 count;

	op_class_tbl = global_op_class;
	count = 0;
	while (op_class_tbl && op_class_tbl->op_class) {
		const struct ath12k_c_freq_lst *p_lst;

		p_lst = op_class_tbl->p_cfi_lst_obj;
		if (p_lst && ath12k_is_6ghz_op_class(op_class_tbl->op_class))
			count++;

		op_class_tbl++;
	}

	return count;
}

/**
 * ath12k_get_6g_opclasses_and_channels - Get 6GHz operating classes and channels
 * @p_frange_lst: Pointer to frequency range list
 * @num_opclasses: Pointer to number of operating classes
 * @opclass_lst: Pointer to operating class list
 * @chansize_lst: Pointer to channel size list
 * @channel_lists: Pointer to channel lists
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_get_6g_opclasses_and_channels(struct ath12k_afc_frange_list *p_frange_lst,
						u8 *num_opclasses,
						u8 **opclass_lst,
						u8 **chansize_lst,
						u8 **channel_lists[])
{
	u16 total_alloc_size, opcls_lst_size, chansize_lst_size, arr_chan_lists_size;
	u8 **arr_chan_lists, *p_total_alloc2, *p_temp_alloc = NULL;
	u8 *p_total_alloc1 = NULL, *l_opcls_lst, *l_chansize_lst;
	const struct ath12k_op_class_map_t *op_class_tbl;
	u8 count, i, n_tot_opclss;

	*opclass_lst = NULL;
	*chansize_lst =  NULL;
	*channel_lists = NULL;
	*num_opclasses = 0;

	op_class_tbl = global_op_class;
	n_tot_opclss = ath12k_get_num_6g_opclasses();
	opcls_lst_size = n_tot_opclss * sizeof(u8);
	chansize_lst_size = n_tot_opclss * sizeof(u8);
	arr_chan_lists_size = n_tot_opclss * sizeof(u8 *);

	total_alloc_size = 0;
	total_alloc_size += opcls_lst_size
		+ chansize_lst_size
		+ arr_chan_lists_size;

	if (!total_alloc_size)
		return -EINVAL;

	p_total_alloc1 = kzalloc(total_alloc_size, GFP_ATOMIC);
	if (!p_total_alloc1)
		return -ENOMEM;

	/* Assign memory locations to each pointers */
	p_temp_alloc = p_total_alloc1;

	l_opcls_lst = p_temp_alloc;
	p_temp_alloc += opcls_lst_size;

	l_chansize_lst = p_temp_alloc;
	p_temp_alloc += chansize_lst_size;

	arr_chan_lists = (u8 **)p_temp_alloc;

	/* Fill arrays with opclasses and chanlist sizes */
	count = 0;
	while (op_class_tbl && op_class_tbl->op_class) {
		const struct ath12k_c_freq_lst *p_lst;

		p_lst = op_class_tbl->p_cfi_lst_obj;
		if (p_lst && ath12k_is_6ghz_op_class(op_class_tbl->op_class)) {
			u8 n_supp_cfis = 0;
			u8 j;

			for (j = 0; j < p_lst->num_cfis; j++) {
				u8 cfi;
				u16 cfi_freq;
				u16 start_freq = op_class_tbl->start_freq;
				u16 bw = op_class_tbl->chan_spacing;

				cfi = p_lst->p_cfis_arr[j];
				cfi_freq = start_freq +
					ATH12K_FREQ_TO_CHAN_SCALE * cfi;
				if (ath12k_is_cfi_freq_in_ranges(cfi_freq, bw,
								 p_frange_lst)) {
					n_supp_cfis++;
				}
			}
			/* Fill opclass number, num cfis and increment
			 * num_opclasses only if the cfi of the opclass
			 * is within the frequency range of interest.
			 */
			if (n_supp_cfis) {
				l_chansize_lst[count] = n_supp_cfis;
				l_opcls_lst[count] = op_class_tbl->op_class;
				(*num_opclasses)++;
				count++;
			}
		}
		op_class_tbl++;
	}

	/* Calculate total allocation size for the array */
	total_alloc_size = 0;
	for (i = 0; i < *num_opclasses; i++)
		total_alloc_size += l_chansize_lst[i] * sizeof(uint8_t *);

	if (!total_alloc_size) {
		kfree(p_total_alloc1);
		return -EINVAL;
	}

	p_total_alloc2 = kzalloc(total_alloc_size, GFP_ATOMIC);
	if (!p_total_alloc2) {
		kfree(p_total_alloc1);
		return -ENOMEM;
	}

	/* Assign memory locations to each list pointers */
	p_temp_alloc = p_total_alloc2;
	for (i = 0; i < *num_opclasses; i++) {
		if (!l_chansize_lst[i])
			arr_chan_lists[i] = NULL;
		else
			arr_chan_lists[i] = p_temp_alloc;
		p_temp_alloc += l_chansize_lst[i] * sizeof(uint8_t *);
	}

	/* Fill the array with channel lists */
	ath12k_fill_6g_opcls_chan_lists(p_frange_lst, l_chansize_lst, arr_chan_lists);

	*opclass_lst = l_opcls_lst;
	*chansize_lst = l_chansize_lst;
	*channel_lists = arr_chan_lists;

	return 0;
}

/**
 * ath12k_free_6g_opclasses_and_channels - Free 6GHz operating classes and channels
 * @num_opclasses: Number of operating classes
 * @opclass_lst: Pointer to operating class list
 * @chansize_lst: Pointer to channel size list
 * @channel_lists: Pointer to channel lists
 */
void ath12k_free_6g_opclasses_and_channels(u8 num_opclasses, u8 *opclass_lst,
					   u8 *chansize_lst, u8 *channel_lists[])
{
	/* All the elements of channel_lists were allocated as a single
	 * allocation with 'channel_lists[0]' holding the first location of the
	 * allocation. Therefore, freeing only 'channel_lists[0]' is enough.
	 * Freeing any other 'channel_lists[i]' will result in error of freeing
	 * unallocated memory.
	 */
	if (channel_lists)
		kfree(channel_lists[0]);

	/* opclass_lst, chansize_lst and channel_lists were allocated as a
	 * single allocation with 'opclass_lst' holding the first location of
	 * allocation. Therefore, freeing only 'opclass_lst' is enough.
	 * Freeing chansize_lst, channel_lists will result in error of freeing
	 * unallocated memory.
	 */
	kfree(opclass_lst);
}

/**
 * ath12k_free_afc_opclass_objs - Free AFC operating class objects
 * @opclass_objs: Pointer to array of operating class objects
 * @num_opclass_objs: Number of operating class objects
 *
 */
static void
ath12k_free_afc_opclass_objs(struct ath12k_afc_opclass_obj *opclass_objs,
			     u8 num_opclass_objs)
{
	u8 i;

	for (i = 0; i < num_opclass_objs; i++) {
		struct ath12k_afc_opclass_obj *opclass_obj;

		opclass_obj = &opclass_objs[i];
		kfree(opclass_obj->cfis);
	}

	kfree(opclass_objs);
}

/**
 * ath12k_free_afc_opclass_list - Free AFC operating class list
 * @opclass_obj_lst: Pointer to operating class object list
 *
 */
static void
ath12k_free_afc_opclass_list(struct ath12k_afc_opclass_obj_list *opclass_obj_lst)
{
	if (!opclass_obj_lst)
		return;

	if (opclass_obj_lst->opclass_objs)
		ath12k_free_afc_opclass_objs(opclass_obj_lst->opclass_objs,
					     opclass_obj_lst->num_opclass_objs);
	kfree(opclass_obj_lst);
}

/**
 * ath12k_fill_afc_opclass_obj - Fill AFC operating class object
 * @p_obj_opclass_obj: Pointer to operating class object
 * @opclass: Operating class
 * @num_chans: Number of channels
 * @p_chan_lst: Pointer to channel list
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_fill_afc_opclass_obj(struct ath12k_afc_opclass_obj *p_obj_opclass_obj,
			    u8 opclass, u8 num_chans, u8 *p_chan_lst)
{
	u8 *src, *dst;
	u8 copy_len;

	p_obj_opclass_obj->opclass_num_cfis = num_chans;
	p_obj_opclass_obj->opclass = opclass;
	/* Zero CFIs(opclass_num_cfis / num_chans) is a valid case */
	if (!num_chans)
		return 0;

	src = p_chan_lst;
	copy_len = num_chans * sizeof(uint8_t);
	dst = kzalloc(copy_len, GFP_ATOMIC);
	if (!dst)
		return -ENOMEM;

	memcpy(dst, src, copy_len);
	p_obj_opclass_obj->cfis = dst;

	return 0;
}

/**
 * ath12k_fill_afc_opclasses_arr - Fill array of AFC operating class objects
 * @num_opclasses: Number of operating classes
 * @opclass_lst: Pointer to operating class list
 * @chansize_lst: Pointer to channel size list
 * @channel_lists: Pointer to channel lists
 * @p_opclass_obj_arr: Pointer to array of operating class objects
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_fill_afc_opclasses_arr(u8 num_opclasses, u8 *opclass_lst,
			      u8 *chansize_lst, u8 *channel_lists[],
			      struct ath12k_afc_opclass_obj *p_opclass_obj_arr)
{
	u16 i;

	for (i = 0; i < num_opclasses; i++) {
		struct ath12k_afc_opclass_obj *p_opclass_obj;
		int ret;

		p_opclass_obj = &p_opclass_obj_arr[i];
		ret = ath12k_fill_afc_opclass_obj(p_opclass_obj, opclass_lst[i],
						  chansize_lst[i],
						  channel_lists[i]);
		if (ret) {
			ath12k_free_afc_opclass_objs(p_opclass_obj_arr,
						     num_opclasses);
			return ret;
		}
	}
	return 0;
}

/**
 * ath12k_reg_fill_afc_opclass_obj_lst - Fill AFC operating class object list
 * @p_afc_req: Pointer to AFC host request
 *
 * The function first calls `ath12k_get_6g_opclasses_and_channels` to
 * retrieve the number of 6GHz operating classes, their list, channel sizes,
 * and channel lists. It then allocates memory for the
 * `ath12k_afc_opclass_obj_list` structure and the array of
 * `ath12k_afc_opclass_obj` structures. The function iterates over the
 * retrieved operating classes and channels, filling each `ath12k_afc_opclass_obj`
 * structure with the corresponding operating class and channel information.
 * Finally, it assigns the filled array of operating class objects to the
 * `opclass_objs` field of the `ath12k_afc_opclass_obj_list` structure and
 * returns the filled list.
 *
 * Return: Pointer to filled operating class object list, NULL on failure
 */
static struct ath12k_afc_opclass_obj_list *
ath12k_reg_fill_afc_opclass_obj_lst(struct ath12k_afc_host_request *p_afc_req)
{
	u8 num_opclasses;
	u8 *opclass_lst;
	u8 *chansize_lst;
	u8 **channel_lists;
	struct ath12k_afc_opclass_obj_list *opclass_obj_lst;
	struct ath12k_afc_opclass_obj *l_opclass_objs;
	int ret;

	ret = ath12k_get_6g_opclasses_and_channels(p_afc_req->freq_lst,
						   &num_opclasses, &opclass_lst,
						   &chansize_lst,
						   &channel_lists);
	if (ret)
		return NULL;

	opclass_obj_lst =  kzalloc(sizeof(*opclass_obj_lst), GFP_ATOMIC);
	if (!opclass_obj_lst) {
		ath12k_free_6g_opclasses_and_channels(num_opclasses,
						      opclass_lst, chansize_lst,
						      channel_lists);
		return NULL;
	}

	opclass_obj_lst->num_opclass_objs = num_opclasses;
	l_opclass_objs = kcalloc(num_opclasses, sizeof(struct ath12k_afc_opclass_obj),
				 GFP_ATOMIC);
	if (!l_opclass_objs) {
		ath12k_free_6g_opclasses_and_channels(num_opclasses,
						      opclass_lst, chansize_lst,
						      channel_lists);
		ath12k_free_afc_opclass_list(opclass_obj_lst);
		return NULL;
	}

	ret = ath12k_fill_afc_opclasses_arr(num_opclasses, opclass_lst,
					    chansize_lst, channel_lists,
					    l_opclass_objs);
	if (ret) {
		ath12k_free_6g_opclasses_and_channels(num_opclasses,
						      opclass_lst, chansize_lst,
						      channel_lists);
		ath12k_free_afc_opclass_list(opclass_obj_lst);
		return NULL;
	}

	opclass_obj_lst->opclass_objs = l_opclass_objs;
	ath12k_free_6g_opclasses_and_channels(num_opclasses, opclass_lst,
					      chansize_lst, channel_lists);

	return opclass_obj_lst;
}

/**
 * ath12k_reg_intersect_freq_ranges - Intersect two frequency ranges
 * @ab: Pointer to ath12k_base structure
 * @first_range: First frequency range
 * @second_range: Second frequency range
 *
 * This function intersects two frequency ranges and returns the resulting range.
 *
 * Return: Intersected frequency range
 */
static struct ieee80211_freq_range
ath12k_reg_intersect_freq_ranges(struct ath12k_base *ab,
				 struct ieee80211_freq_range first_range,
				 struct ieee80211_freq_range second_range)
{
	struct ieee80211_freq_range out_range;
	u32 l_freq;
	u32 r_freq;

	l_freq = max(first_range.start_freq_khz, second_range.start_freq_khz);
	r_freq = min(first_range.end_freq_khz, second_range.end_freq_khz);

	if (l_freq > r_freq) {
		l_freq = 0;
		r_freq = 0;

		ath12k_dbg(ab, ATH12K_DBG_AFC, "Ranges do not overlap first= [%u, %u], second = [%u, %u]\n",
			   first_range.start_freq_khz,
			   first_range.end_freq_khz,
			   second_range.start_freq_khz,
			   second_range.end_freq_khz);
	}
	out_range.start_freq_khz = l_freq;
	out_range.end_freq_khz = r_freq;

	return out_range;
}

/**
 * ath12k_afc_get_intersected_ranges - Get intersected frequency ranges
 * @in_range: Input frequency range
 * @out_arg: Output argument
 *
 * This function gets the intersected frequency ranges.
 */
static void ath12k_afc_get_intersected_ranges(struct ieee80211_freq_range *in_range,
					      void *out_arg)
{
	struct ath12k_afc_freq_range_obj *p_range;
	struct ath12k_afc_freq_range_obj **pp_range;
	u32 low, high;

	pp_range = (struct ath12k_afc_freq_range_obj **)out_arg;
	p_range = *pp_range;

	if (in_range->start_freq_khz && in_range->end_freq_khz) {
		low = KHZ_TO_MHZ(in_range->start_freq_khz);
		high = KHZ_TO_MHZ(in_range->end_freq_khz);
		p_range->lowfreq = (uint16_t)low;
		p_range->highfreq = (uint16_t)high;
		(*pp_range)++;
	}
}

typedef void (*ath12k_act_sp_rule_cb)(struct ieee80211_freq_range *out_range,
				      void *arg);

/**
 * ath12k_iterate_sp_rules - Iterate special rules
 * @ar: Pointer to ath12k structure
 * @sp_rule_action: Special rule action callback
 * @arg: Argument for the callback
 *
 * This function iterates over special rules and applies the given action.
 */
static void ath12k_iterate_sp_rules(struct ath12k *ar,
				    ath12k_act_sp_rule_cb sp_rule_action,
				    void *arg)
{
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *reg_cap;
	struct ieee80211_freq_range chip_range;
	struct ieee80211_reg_rule *sp_rule;
	struct ath12k_base *ab;
	int num_6ghz_sp_rules;
	u16 i;

	ab = ar->ab;
	reg_cap = &ab->hal_reg_cap[ar->pdev_idx];
	chip_range.start_freq_khz = MHZ_TO_KHZ(reg_cap->low_5ghz_chan);
	chip_range.end_freq_khz = MHZ_TO_KHZ(reg_cap->high_5ghz_chan);

	sp_rule = ab->sp_rule->sp_reg_rule;
	num_6ghz_sp_rules = ab->sp_rule->num_6ghz_sp_rule;
	for (i = 0; i < num_6ghz_sp_rules; i++) {
		struct ieee80211_freq_range in_range;

		in_range = ath12k_reg_intersect_freq_ranges(ab,
							    sp_rule->freq_range,
							    chip_range);

		if (sp_rule_action)
			sp_rule_action(&in_range, arg);

		sp_rule++;
	}
}

/**
 * ath12k_cp_freq_ranges - Copy frequency ranges
 * @ar: Pointer to ath12k structure
 * @num_6ghz_sp_rules: Number of 6GHz standard power rules
 * @p_range_obj: Pointer to frequency range object
 *
 * This function copies the frequency ranges.
 */
static void ath12k_cp_freq_ranges(struct ath12k *ar,
				  int num_6ghz_sp_rules,
				  struct ath12k_afc_freq_range_obj *p_range_obj)
{
	struct ath12k_afc_freq_range_obj *p_range;

	p_range = p_range_obj;
	ath12k_iterate_sp_rules(ar, ath12k_afc_get_intersected_ranges, &p_range);
}

/**
 * ath12k_reg_afc_incr_num_ranges - Increment number of AFC frequency ranges
 * @p_range: Pointer to frequency range
 * @num_freq_ranges: Pointer to number of frequency ranges
 *
 * This function increments the count of AFC frequency ranges for valid ranges.
 */
static void
ath12k_reg_afc_incr_num_ranges(struct ieee80211_freq_range *p_range,
			       void *num_freq_ranges)
{
	if (p_range->start_freq_khz && p_range->end_freq_khz) {
		(*(u8 *)num_freq_ranges)++;
	}
}

/**
 * ath12k_reg_get_num_sp_freq_ranges - Get number of SP frequency ranges
 * @ar: Pointer to ath12k structure
 *
 * This function iterates over the intersected SP rules and counts the
 * number of frequency ranges that are valid.
 *
 * Return: Number of SP frequency ranges
 */
static u8 ath12k_reg_get_num_sp_freq_ranges(struct ath12k *ar)
{
	u8 num_freq_ranges;

	num_freq_ranges = 0;
	ath12k_iterate_sp_rules(ar,
				ath12k_reg_afc_incr_num_ranges,
				&num_freq_ranges);

	return num_freq_ranges;
}

/**
 * ath12k_reg_fill_afc_freq_ranges - Fill AFC frequency ranges
 * @ar: Pointer to ath12k structure
 * @p_frange_lst: Pointer to frequency range list
 * @num_6ghz_sp_rules: Number of 6 GHz standard power rules
 *
 * This function fills the AFC frequency ranges. It iterates over the
 * standard power rules* and intersects them with the chip's frequency range,
 * then fills the frequency range  list with the intersected ranges.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_reg_fill_afc_freq_ranges(struct ath12k *ar,
				struct ath12k_afc_frange_list *p_frange_lst,
				u8 num_6ghz_sp_rules)
{
	struct ath12k_afc_freq_range_obj *p_range_obj;

	p_frange_lst->num_ranges = num_6ghz_sp_rules;
	if (!num_6ghz_sp_rules)
		return -EINVAL;

	p_range_obj = kcalloc(num_6ghz_sp_rules, sizeof(*p_range_obj),
			      GFP_ATOMIC);
	if (!p_range_obj)
		return -ENOMEM;

	ath12k_cp_freq_ranges(ar, num_6ghz_sp_rules, p_range_obj);
	p_frange_lst->range_objs = p_range_obj;
	return 0;
}

/**
 * ath12k_free_afc_freq_list - Free AFC frequency list
 * @freq_lst: Pointer to frequency list
 *
 */
static void ath12k_free_afc_freq_list(struct ath12k_afc_frange_list *freq_lst)
{
	if (freq_lst) {
		kfree(freq_lst->range_objs);
		kfree(freq_lst);
	}
}

/**
 * ath12k_reg_fill_freq_lst - Fill frequency list
 * @ar: Pointer to ath12k structure
 *
 * Return: Pointer to filled frequency list, NULL on failure
 */
static struct ath12k_afc_frange_list *ath12k_reg_fill_freq_lst(struct ath12k *ar)
{
	struct ath12k_afc_frange_list *p_frange_lst_local;
	int status;
	u8 num_freq_ranges;

	num_freq_ranges = ath12k_reg_get_num_sp_freq_ranges(ar);
	if (!num_freq_ranges) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "No AFC frequency ranges found\n");
		return NULL;
	}

	p_frange_lst_local = kzalloc(sizeof(*p_frange_lst_local), GFP_ATOMIC);
	if (!p_frange_lst_local)
		return NULL;

	status = ath12k_reg_fill_afc_freq_ranges(ar, p_frange_lst_local,
						 num_freq_ranges);
	if (status) {
		ath12k_free_afc_freq_list(p_frange_lst_local);
		return NULL;
	}

	return p_frange_lst_local;
}

/**
 * ath12k_reg_fill_afc_location_obj - Fill AFC location object
 * @ar: Pointer to ath12k structure
 *
 * Return: Pointer to filled AFC location object, NULL on failure
 */
static struct ath12k_afc_location *ath12k_reg_fill_afc_location_obj(struct ath12k *ar)
{
	struct ath12k_afc_location *p_afc_location;

	p_afc_location = kzalloc(sizeof(*p_afc_location), GFP_ATOMIC);
	if (!p_afc_location)
		return NULL;

	p_afc_location->deployment_type = ar->ab->afc_dev_deployment;

	return p_afc_location;
}

/**
 * ath12k_free_afc_req - Free AFC request
 * @afc_req: Pointer to AFC host request
 *
 */
static void ath12k_free_afc_req(struct ath12k_afc_host_request *afc_req)
{
	if (!afc_req)
		return;

	ath12k_free_afc_freq_list(afc_req->freq_lst);
	ath12k_free_afc_opclass_list(afc_req->opclass_obj_lst);
	kfree(afc_req->afc_location);
	kfree(afc_req);
}

int ath12k_get_afc_req_info(struct ath12k *ar,
			    struct ath12k_afc_host_request **afc_req,
			    u64 request_id)
{
	struct ath12k_afc_host_request *p_afc_req;

	*afc_req = kzalloc(sizeof(*afc_req), GFP_ATOMIC);
	if (!*afc_req)
		return -ENOMEM;

	p_afc_req = *afc_req;
	p_afc_req->req_id = request_id;
	p_afc_req->min_des_power = DEFAULT_MIN_POWER;

	p_afc_req->freq_lst = ath12k_reg_fill_freq_lst(ar);
	if (!p_afc_req->freq_lst) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "Allocation and filling of freq_lst failed\n");
		ath12k_free_afc_req(p_afc_req);
		return -ENOMEM;
	}

	p_afc_req->opclass_obj_lst = ath12k_reg_fill_afc_opclass_obj_lst(p_afc_req);
	if (!p_afc_req->opclass_obj_lst) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "Allocation and filling of opclass_obj_lst failed\n");
		ath12k_free_afc_req(p_afc_req);
		return -ENOMEM;
	}

	p_afc_req->afc_location = ath12k_reg_fill_afc_location_obj(ar);
	if (!p_afc_req->afc_location) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "Allocation and filling of afc_location failed\n");
		ath12k_free_afc_req(p_afc_req);
		return -ENOMEM;
	}

	return 0;
}

/**
 * ath12k_reg_print_afc_req_info_header_params - Print AFC request header parameters
 * @ab: Pointer to ath12k_base structure
 * @afc_req: Pointer to ath12k_afc_host_request structure
 *
 */
static void
ath12k_reg_print_afc_req_info_header_params(struct ath12k_base *ab,
					    struct ath12k_afc_host_request *afc_req)
{
	ath12k_dbg(ab, ATH12K_DBG_AFC, "req_id=%llu\n", afc_req->req_id);
	ath12k_dbg(ab, ATH12K_DBG_AFC, "version_minor=%u\n", afc_req->version_minor);
	ath12k_dbg(ab, ATH12K_DBG_AFC, "version_major=%u\n", afc_req->version_major);
	ath12k_dbg(ab, ATH12K_DBG_AFC, "min_des_power=%d\n", afc_req->min_des_power);
}

/**
 * ath12k_reg_print_afc_req_info_frange_list - Print AFC request frequency range list
 * @ab: Pointer to ath12k_base structure
 * @afc_req: Pointer to ath12k_afc_host_request structure
 *
 */
static void
ath12k_reg_print_afc_req_info_frange_list(struct ath12k_base *ab,
					  struct ath12k_afc_host_request *afc_req)
{
	struct ath12k_afc_frange_list *p_frange_lst;
	u8 i;

	p_frange_lst = afc_req->freq_lst;
	if (!p_frange_lst) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "p_frange_lst is NULL\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "num_ranges=%u\n", p_frange_lst->num_ranges);
	for (i = 0; i < p_frange_lst->num_ranges; i++) {
		struct ath12k_afc_freq_range_obj *p_range_obj;

		p_range_obj = &p_frange_lst->range_objs[i];
		ath12k_dbg(ab, ATH12K_DBG_AFC, "lowfreq=%u\n", p_range_obj->lowfreq);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "highfreq=%u\n", p_range_obj->highfreq);
	}
}

/**
 * ath12k_reg_print_afc_req_info_opclass_list - Print AFC request operating class list
 * @ab: Pointer to ath12k_base structure
 * @afc_req: Pointer to ath12k_afc_host_request structure
 *
 */
static void
ath12k_reg_print_afc_req_info_opclass_list(struct ath12k_base *ab,
					   struct ath12k_afc_host_request *afc_req)
{
	u8 i;
	u8 num_opclasses;
	struct ath12k_afc_opclass_obj_list *p_opclass_obj_lst;
	struct ath12k_afc_opclass_obj *p_opclass_obj;

	p_opclass_obj_lst = afc_req->opclass_obj_lst;
	if (!p_opclass_obj_lst) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "p_opclass_obj_lst is NULL\n");
		return;
	}

	num_opclasses = p_opclass_obj_lst->num_opclass_objs;
	ath12k_dbg(ab, ATH12K_DBG_AFC, "num_opclasses=%u\n", num_opclasses);
	p_opclass_obj = p_opclass_obj_lst->opclass_objs;
	for (i = 0; i < num_opclasses; i++) {
		u8 opclass = p_opclass_obj[i].opclass;
		u8 num_cfis = p_opclass_obj[i].opclass_num_cfis;
		u8 *cfis = p_opclass_obj[i].cfis;
		u8 j;

		ath12k_dbg(ab, ATH12K_DBG_AFC, "opclass[%u]=%u\n", i, opclass);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "num_cfis[%u]=%u\n", i, num_cfis);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "[\n");
		for (j = 0; j < num_cfis; j++)
			ath12k_dbg(ab, ATH12K_DBG_AFC, "%u,\n", cfis[j]);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "]\n");
	}
}

/**
 * ath12k_reg_print_afc_req_info_location - Print AFC request location information
 * @ab: Pointer to ath12k_base structure
 * @afc_req: Pointer to ath12k_afc_host_request structure
 *
 */
static void ath12k_reg_print_afc_req_info_location(struct ath12k_base *ab,
						   struct ath12k_afc_host_request *afc_req)
{
	struct ath12k_afc_location *p_afc_location;
	u8 *deployment_type_str;

	p_afc_location = afc_req->afc_location;
	if (!p_afc_location) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "p_afc_location is NULL\n");
		return;
	}

	switch (p_afc_location->deployment_type) {
	case ATH12K_AFC_DEPLOYMENT_INDOOR:
		deployment_type_str = "Indoor";
		break;
	case ATH12K_AFC_DEPLOYMENT_OUTDOOR:
		deployment_type_str = "Outdoor";
		break;
	default:
		deployment_type_str = "Unknown";
		break;
	}
	ath12k_dbg(ab, ATH12K_DBG_AFC, "AFC location=%s\n", deployment_type_str);
}

/**
 * ath12k_reg_print_afc_req_info - Print AFC request information
 * @ab: Pointer to ath12k_base structure
 * @afc_req: Pointer to ath12k_afc_host_request structure
 *
 */
void ath12k_reg_print_afc_req_info(struct ath12k_base *ab, struct ath12k_afc_host_request *afc_req)
{
	if (!afc_req) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "afc_req is NULL\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Printing AFC request\n");
	ath12k_reg_print_afc_req_info_header_params(ab, afc_req);
	ath12k_reg_print_afc_req_info_frange_list(ab, afc_req);
	ath12k_reg_print_afc_req_info_opclass_list(ab, afc_req);
	ath12k_reg_print_afc_req_info_location(ab, afc_req);
}

/**
 * ath12k_reg_afc_start - Get AFC request info to send the AFC request
 * @ab: Pointer to ath12k_base structure
 * @afc: Pointer to AFC information structure
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_reg_afc_start(struct ath12k_base *ab,
				struct ath12k_afc_info *afc)
{
	struct ath12k *ar = container_of(afc, struct ath12k, afc);
	struct ath12k_afc_host_request *afc_req = afc->afc_req;
	int ret;

	ret = ath12k_get_afc_req_info(ar, &afc_req, afc->request_id);

	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Creating AFC Request failed\n");
		return -EINVAL;
	}

	ath12k_reg_print_afc_req_info(ab, afc_req);
	ret = ath12k_send_afc_request(ar, afc_req);
	ath12k_free_afc_req(afc_req);

	return ret;
}

void ath12k_free_afc_power_event_info(struct ath12k_afc_info *afc)
{
	struct ath12k *ar = container_of(afc, struct ath12k, afc);
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	struct ath12k_afc_chan_obj *afc_chan_info;
	struct ath12k_base *ab = ar->ab;
	int num_chan_objs;
	int i;

	if (!afc->afc_reg_info)
		return;

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Freeing afc info\n");
	afc_reg_info = afc->afc_reg_info;
	num_chan_objs = afc_reg_info->num_chan_objs;
	kfree(afc_reg_info->afc_freq_info);

	for (i = 0; i < num_chan_objs; i++) {
		afc_chan_info = afc_reg_info->afc_chan_info + i;
		kfree(afc_chan_info->chan_eirp_info);
	}

	kfree(afc_reg_info->afc_chan_info);
	kfree(afc_reg_info);
	afc->afc_reg_info = NULL;
}

int ath12k_process_expiry_event(struct ath12k *ar)
{
	struct ath12k_afc_info *afc = &ar->afc;
	int ret;

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC expiry event subtype %d\n",
		   afc->event_subtype);
	afc->is_6ghz_afc_power_event_received = false;

	switch (afc->event_subtype) {
	case REG_AFC_EXPIRY_EVENT_START:
	case REG_AFC_EXPIRY_EVENT_RENEW:
		afc->is_6g_afc_expiry_event_received = true;
		ret = ath12k_reg_afc_start(ar->ab, afc);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to notify expiry event\n");
			return ret;
		}
		break;
	case REG_AFC_EXPIRY_EVENT_SWITCH_TO_LPI:
		ret = ath12k_handle_invalid_afc_payload(ar);
		ath12k_free_afc_power_event_info(afc);
		if (ret) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Failed to process switch to LPI event\n");
			return ret;
		}
		ath12k_send_afc_payload_reset(ar);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Invalid AFC expiry event subtype %d\n",
			   afc->event_subtype);
		break;
	}
	return 0;
}

u8 ath12k_reg_get_nsubchannels_for_opclass(u8 opclass)
{
	u8 i, n_opclasses = ARRAY_SIZE(ath12k_opclass_nchans_map);

	for (i = 0; i < n_opclasses; i++)
		if (opclass == ath12k_opclass_nchans_map[i].opclass)
			return ath12k_opclass_nchans_map[i].nchans;

	return 0;
}

void ath12k_reg_fill_subchan_centers(u8 nchans, u8 cfi, u8 *subchannels)
{
#define HALF_IEEE_CH_SEP  2
#define IEEE_20MHZ_CH_SEP 4
	u8 offset = HALF_IEEE_CH_SEP;
	u8 last_idx = nchans - 1;
	u8 i;

	if (nchans == 1) {
		subchannels[0] = cfi;
		return;
	}

	for (i = nchans / 2; i < nchans; i++) {
		subchannels[i] = cfi + offset;
		subchannels[last_idx - i] = cfi - offset;
		offset += IEEE_20MHZ_CH_SEP;
	}
}

u8 ath12k_reg_get_opclass_from_bw(enum nl80211_chan_width bw)
{
	u8 i, n_opclass = ARRAY_SIZE(ath12k_opclass_bw_map);

	for (i = 0; i < n_opclass; i++)
		if (bw == ath12k_opclass_bw_map[i].bw)
			return ath12k_opclass_bw_map[i].opclass;

	return 0;
}

s16 ath12k_reg_psd_2_eirp(s16 psd, uint16_t ch_bw)
{
	s16 eirp = ATH12K_MAX_TX_POWER;
	s16 ten_log10_bw;
	u8 num_bws;
	u8 i;

	/* EIRP = PSD + (10 * log10(CH_BW)) */
	num_bws = ARRAY_SIZE(ath12k_bw_to_10log10_map);
	for (i = 0; i < num_bws; i++) {
		if (ch_bw == ath12k_bw_to_10log10_map[i].bw) {
			ten_log10_bw = ath12k_bw_to_10log10_map[i].ten_l_ten;
			eirp = psd + ten_log10_bw;
			break;
		}
	}

	return eirp;
}

void ath12k_reg_get_sp_regulatory_pwrs(struct ath12k_base *ab,
				       u32 freq,
				       s8 *max_reg_eirp,
				       s8 *reg_psd)
{
	struct ath12k_6ghz_sp_reg_rule *sp_rule = NULL;
	struct ieee80211_reg_rule *sp_reg_rule;
	int num_sp_rules;
	int i;

	if (!ab->sp_rule)
		return;

	sp_rule = ab->sp_rule;

	if (!sp_rule->num_6ghz_sp_rule) {
		ath12k_warn(ab, "No default 6 GHz sp rules present\n");
		return;
	}

	num_sp_rules = sp_rule->num_6ghz_sp_rule;

	for (i = 0; i < num_sp_rules; i++) {
		sp_reg_rule = sp_rule->sp_reg_rule + i;
		if (freq >= sp_reg_rule->freq_range.start_freq_khz &&
		    freq <= sp_reg_rule->freq_range.end_freq_khz) {
			*max_reg_eirp = MBM_TO_DBM(sp_reg_rule->power_rule.max_eirp);
			*reg_psd = sp_reg_rule->psd;
		}
	}
}

void ath12k_reg_get_regulatory_pwrs(struct ath12k *ar,
				    u32 freq,
				    u8 reg_6g_power_mode,
				    s8 *max_reg_eirp,
				    s8 *reg_psd)
{
	struct ieee80211_regdomain *regd = NULL;
	struct ath12k_base *ab = ar->ab;
	int pdev_id;
	int i;

	pdev_id = ar->pdev_idx;

	if (ab->new_regd[pdev_id])
		regd = ab->new_regd[pdev_id];
	else
		regd = ab->default_regd[pdev_id];

	if (!regd)
		return;

	if (reg_6g_power_mode == NL80211_REG_AP_SP) {
		ath12k_reg_get_sp_regulatory_pwrs(ab, freq, max_reg_eirp, reg_psd);
	} else {
		for (i = 0; i < regd->n_reg_rules; i++) {
			struct ieee80211_reg_rule reg_rule = regd->reg_rules[i];

			if (reg_rule.mode == reg_6g_power_mode &&
			    freq >= reg_rule.freq_range.start_freq_khz &&
			    freq <= reg_rule.freq_range.end_freq_khz) {
				*max_reg_eirp = MBM_TO_DBM(reg_rule.power_rule.max_eirp);
				*reg_psd = reg_rule.psd;
			}
		}
	}
}

void ath12k_regd_update_work(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 regd_update_work);
	int ret;

	ret = ath12k_regd_update(ar, false);
	if (ret) {
		/* Firmware has already moved to the new regd. We need
		 * to maintain channel consistency across FW, Host driver
		 * and userspace. Hence as a fallback mechanism we can set
		 * the prev or default country code to the firmware.
		 */
		/* TODO: Implement Fallback Mechanism */
	}
}

void ath12k_reg_init(struct ieee80211_hw *hw)
{
	hw->wiphy->regulatory_flags = REGULATORY_WIPHY_SELF_MANAGED;
	hw->wiphy->flags |= WIPHY_FLAG_NOTIFY_REGDOM_BY_DRIVER;
	hw->wiphy->reg_notifier = ath12k_reg_notifier;
}

void ath12k_reg_free(struct ath12k_base *ab)
{
	int i;

	if (ab->regd_freed)
		return;

	for (i = 0; i < ab->hw_params->max_radios; i++) {
		kfree(ab->default_regd[i]);
		kfree(ab->new_regd[i]);
		ab->default_regd[i] = NULL;
		ab->new_regd[i] = NULL;
		ab->regd_freed = true;
	}
}
