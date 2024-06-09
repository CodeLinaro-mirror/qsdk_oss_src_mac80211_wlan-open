/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_REG_H
#define ATH12K_REG_H

#include <linux/kernel.h>
#include <net/regulatory.h>

struct ath12k_base;
struct ath12k;

#define ATH12K_2GHZ_MAX_FREQUENCY	2495
#define ATH12K_5GHZ_MAX_FREQUENCY	5920

/* DFS regdomains supported by Firmware */
enum ath12k_dfs_region {
	ATH12K_DFS_REG_UNSET,
	ATH12K_DFS_REG_FCC,
	ATH12K_DFS_REG_ETSI,
	ATH12K_DFS_REG_MKK,
	ATH12K_DFS_REG_CN,
	ATH12K_DFS_REG_KR,
	ATH12K_DFS_REG_MKK_N,
	ATH12K_DFS_REG_UNDEF,
};

enum ath12k_afc_power_update_status {
	ath12k_AFC_POWER_UPDATE_IGNORE = 0, /* Used for expiry event */
	ath12k_AFC_POWER_UPDATE_SUCCESS = 1,
	ath12k_AFC_POWER_UPDATE_FAIL = 3,
};

enum ath12k_afc_event_state {
	ATH12K_AFC_EVENT_POWER_INFO   = 1,
};

enum ath12k_afc_power_event_status_code {
	REG_FW_AFC_POWER_EVENT_SUCCESS = 0,
	REG_FW_AFC_POWER_EVENT_RESP_NOT_RECEIVED = 1,
	REG_FW_AFC_POWER_EVENT_RESP_PARSING_FAILURE = 2,
	REG_FW_AFC_POWER_EVENT_FAILURE = 3,
};

enum ath12k_serv_resp_code {
	REG_AFC_SERV_RESP_GENERAL_FAILURE = -1,
	REG_AFC_SERV_RESP_SUCCESS = 0,
	REG_AFC_SERV_RESP_VERSION_NOT_SUPPORTED = 100,
	REG_AFC_SERV_RESP_DEVICE_UNALLOWED = 101,
	REG_AFC_SERV_RESP_MISSING_PARAM = 102,
	REG_AFC_SERV_RESP_INVALID_VALUE = 103,
	REG_AFC_SERV_RESP_UNEXPECTED_PARAM = 106,
	REG_AFC_SERV_RESP_UNSUPPORTED_SPECTRUM = 300,
};

struct ath12k_afc_freq_obj {
	u32 low_freq;
	u32 high_freq;
	s16 max_psd;
};

struct ath12k_chan_eirp_obj {
	u8 cfi;
	u16 eirp_power;
};

struct ath12k_afc_chan_obj {
	u8 global_opclass;
	u8 num_chans;
	struct ath12k_chan_eirp_obj *chan_eirp_info;
};

struct ath12k_afc_sp_reg_info {
	u32 resp_id;
	enum ath12k_afc_power_event_status_code fw_status_code;
	enum ath12k_serv_resp_code serv_resp_code;
	u32 afc_wfa_version;
	u32 avail_exp_time_d;
	u32 avail_exp_time_t;
	u8 num_freq_objs;
	u8 num_chan_objs;
	struct ath12k_afc_freq_obj *afc_freq_info;
	struct ath12k_afc_chan_obj *afc_chan_info;
};

struct ath12k_afc_info {
	enum ath12k_afc_event_state event_type;
	bool is_6ghz_afc_power_event_received;
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	bool afc_regdom_configured;
};

enum ath12k_reg_cc_code {
	REG_SET_CC_STATUS_PASS = 0,
	REG_CURRENT_ALPHA2_NOT_FOUND = 1,
	REG_INIT_ALPHA2_NOT_FOUND = 2,
	REG_SET_CC_CHANGE_NOT_ALLOWED = 3,
	REG_SET_CC_STATUS_NO_MEMORY = 4,
	REG_SET_CC_STATUS_FAIL = 5,
};

struct ath12k_reg_rule {
	u16 start_freq;
	u16 end_freq;
	u16 max_bw;
	u8 reg_power;
	u8 ant_gain;
	u16 flags;
	bool psd_flag;
	s8 psd_eirp;
};

struct ath12k_reg_info {
	enum ath12k_reg_cc_code status_code;
	u8 num_phy;
	u8 phy_id;
	u16 reg_dmn_pair;
	u16 ctry_code;
	u8 alpha2[REG_ALPHA2_LEN + 1];
	u32 dfs_region;
	u32 phybitmap;
	bool is_ext_reg_event;
	u32 min_bw_2g;
	u32 max_bw_2g;
	u32 min_bw_5g;
	u32 max_bw_5g;
	u32 num_2g_reg_rules;
	u32 num_5g_reg_rules;
	struct ath12k_reg_rule *reg_rules_2g_ptr;
	struct ath12k_reg_rule *reg_rules_5g_ptr;
	enum wmi_reg_6g_client_type client_type;
	bool rnr_tpe_usable;
	bool unspecified_ap_usable;
	/* TODO: All 6G related info can be stored only for required
	 * combination instead of all types, to optimize memory usage.
	 */
	u8 domain_code_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u8 domain_code_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 domain_code_6g_super_id;
	u32 min_bw_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 max_bw_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 min_bw_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 max_bw_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 num_6g_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6g_reg_rules_cl[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	struct ath12k_reg_rule *reg_rules_6g_ap_ptr[WMI_REG_CURRENT_MAX_AP_TYPE];
	struct ath12k_reg_rule *reg_rules_6g_client_ptr
		[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
};

/* Phy bitmaps */
enum ath12k_reg_phy_bitmap {
	ATH12K_REG_PHY_BITMAP_NO11AX	= BIT(5),
	ATH12K_REG_PHY_BITMAP_NO11BE	= BIT(6),
};

void ath12k_reg_init(struct ieee80211_hw *hw);
void ath12k_reg_free(struct ath12k_base *ab);
void ath12k_regd_update_work(struct work_struct *work);
struct ieee80211_regdomain *ath12k_reg_build_regd(struct ath12k_base *ab,
						  struct ath12k_reg_info *reg_info);
enum wmi_reg_6g_ap_type
ath12k_ieee80211_ap_pwr_type_convert(enum ieee80211_ap_reg_power power_type);
int ath12k_regd_update(struct ath12k *ar, bool init);
int ath12k_reg_update_chan_list(struct ath12k *ar, bool wait);
int ath12k_reg_get_num_chans_in_band(struct ath12k *ar,
				     struct ieee80211_supported_band *band);
int ath12k_reg_process_afc_power_event(struct ath12k *ar);
#endif
