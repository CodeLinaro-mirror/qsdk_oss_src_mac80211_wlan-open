/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_VENDOR_H
#define ATH12K_VENDOR_H

#define QCA_NL80211_VENDOR_ID 0x001374

#define INVALID_LINK_ID 0xFF
#define is_valid_link_id(X) !((X) >= INVALID_LINK_ID)

#define QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE          5000
#define QCA_WLAN_AFC_RESP_DESC_FIELD_START_OCTET       14
#define QCA_WLAN_AFC_RESP_DESC_FIELD_END_OCTET         30
#define ATF_OFFLOAD_MAX_PAYLOAD                                2048
#define ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT              30
#define ATH12K_ATF_MAX_PEERS				512

#define QCA_VENDOR_WLAN_TELEMETRY_LEGACY_MCS_MAX     12
#define QCA_VENDOR_WLAN_TELEMETRY_VHT_MCS_MAX        10
#define QCA_VENDOR_WLAN_TELEMETRY_HT_MCS_MAX         32
#define QCA_VENDOR_WLAN_TELEMETRY_HE_MCS_MAX         14
#define QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX        16
#define QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX         24	/* UHR (11BN) */
#define QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX \
	(QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX + 1)

#define QCA_VENDOR_WLAN_OEM_DATA_BUF_MAX_SIZE		1024

#define QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS 9

#define INVALID_RADIO_INDEX 0xFF

#define ATH12K_MGMT_TX_RETRY_LIMIT_MIN 1
#define ATH12K_MGMT_TX_RETRY_LIMIT_MAX 14

#define ATH12K_IPV4_ADDR_LEN         4
#define ATH12K_IPV6_ADDR_LEN         16

#define ATH12K_MAX_PCP	7
#define ATH12K_MAX_TID	7

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
extern unsigned int ath12k_ppe_ds_enabled;
#endif
struct ath12k;
struct ath12k_hw;
struct ath12k_afc_info;
struct ath12k_afc_host_request;
struct ath12k_base;
struct ath12k_hw_group;

struct ath12k_wifi_generic_params {
	u32 command;
	u32 value;
	void *data;
	u32 data_len;
	u32 length;
	u32 flags;
	u32 ifindex;
	u8 link_id;
	u8 radio_idx;
};

struct atf_peer_stat {
	u8 addr[ETH_ALEN];
	u32 atf_actual_airtime;
	u32 atf_peer_conf_airtime;
	u8 atf_group_index;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;
};

/* ME List Types */
enum ieee80211_me_list {
	IEEE80211_HMMC_LIST = 0,
	IEEE80211_DENY_LIST = 1,
	IEEE80211_HMMC_LIST_V6 = 2,
	IEEE80211_DENY_LIST_V6 = 3,
	IEEE80211_ME_LIST_ALL = 4,
};

/* ME List Operations */
enum ieee80211_wlanconfig_me_op {
	IEEE80211_WLANCONFIG_ME_LIST_ADD = 0,
	IEEE80211_WLANCONFIG_ME_LIST_DEL = 1,
	IEEE80211_WLANCONFIG_ME_LIST_DUMP = 2,
};

/* IP Address Types */
enum qca_wlan_vendor_me_ip_type {
	QCA_WLAN_VENDOR_ME_IP_TYPE_IPV4 = 0,
	QCA_WLAN_VENDOR_ME_IP_TYPE_IPV6 = 1,
};

/* ME List Entry Structure */
struct ieee80211_wlanconfig_me_list {
	union {
		u_int32_t ip;			/* IPv4 address */
		u_int32_t ipv6[4];		/* IPv6 address (128-bit) */
	};
	u_int32_t mask;				/* Mask or Prefix for IPv4 or IPv6 */
	enum ieee80211_me_list me_list_type;	/* ME List type */
};

struct oem_vendor_build {
	u8 l_radio_id;
	__le32 l_content_type;
	__le32 l_num_bytes_valid;
	u8 l_data[];
} __packed;

struct ath12k_vendor_ch_switch_attrs {
	u32 freq;
	u32 width;
};

/**
 * @QCA_NL80211_VENDOR_SUBCMD_WLAN_CTL_TABLE: This vendor subcommand is used to
 *     configure the CTL (Conformance Test Limit) table for a specific band.
 *     The attributes used with this subcommand
 *     are defined in enum qca_wlan_vendor_attr_ctl_table.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS: Vendor subcommand/event used
 *     to query hardware blocked channel information from the driver or firmware.
 *     Request attributes are defined in enum
 *     qca_wlan_vendor_attr_hw_blocked_chans_req.
 *     Response/event attributes are defined in enum
 *     qca_wlan_vendor_attr_hw_blocked_chans_resp,
 *     qca_wlan_vendor_attr_hw_blocked_chans_radio and
 *     qca_wlan_vendor_attr_hw_blocked_chans_band.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON: Vendor event used to notify
 *     userspace about the reason for a channel switch. Event attributes are
 *     defined in enum qca_wlan_vendor_attr_ch_switch_reason. Reason codes are
 *     defined in enum qca_wlan_vendor_ch_switch_reason.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED: Vendor event used to notify
 *     userspace whenever the TX or RX chain mask is updated in the firmware.
 *     This event carries no attributes; the updated chain mask values can be
 *     retrieved via the standard wiphy configuration interface after the event
 *     is received by the userspace.
 *
 * @QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM: Vendor subcommand used to
 *	indicate parameters update for multiple BSS to the driver to perform
 *	driver internals based on the parameter change. The attributes used
 *	with this command are defined in &enum qca_wlan_vendor_attr_set_multi_bss_param.
 */
enum qca_nl80211_vendor_subcmds {
	/* Wi-Fi configuration subcommand */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION = 74,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION = 75,
	QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN = 106,
	QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE = 107,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START = 154,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP = 155,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG = 158,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS = 159,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO = 160,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS = 161,
	QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO = 163,
	QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG = 173,
	QCA_NL80211_VENDOR_SUBCMD_OEM_DATA = 182,
	QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO = 186,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC = 206,
	QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG = 218,
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT = 222,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE = 223,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS = 235,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS = 236,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE = 242,
	QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE = 256,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY = 262,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV = 263,
	QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS = 268,
	QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG = 269,
	QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR = 272,
	QCA_NL80211_VENDOR_SUBCMD_ME_LIST = 374,
	QCA_NL80211_VENDOR_SUBCMD_ME_CONFIG = 375,

	/* Yet to upstream */
	QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE = 500,
	QCA_NL80211_VENDOR_SUBCMD_POWER_MODE_CHANGE_COMPLETED = 501,
	QCA_NL80211_VENDOR_SUBCMD_AFC_CLEAR_PAYLOAD = 502,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESET = 503,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD = 504,
	QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION = 505,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION = 506,
	QCA_NL80211_VENDOR_SUBCMD_AFC_GET_REG_EIRP = 507,
	QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO = 508,
	QCA_NL80211_VENDOR_SUBCMD_AFC_FETCH_POWER_EVENT = 510,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_EVENT = 511,
	QCA_NL80211_VENDOR_SUBCMD_DERIVE_LINK_BSS_ADDR = 512,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX = 513,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_CTL_TABLE = 514,
	QCA_NL80211_VENDOR_SUBCMD_GET_CHANNEL_SWITCH_TIME = 515,
	QCA_NL80211_VENDOR_SUBCMD_REPURPOSE_LINK_INDICATION = 516,
	QCA_NL80211_VENDOR_SUBCMD_DCS_SIM = 517,
	QCA_NL80211_VENDOR_SUBCMD_REG_PARAMS = 518,
	QCA_NL80211_VENDOR_SUBCMD_TPC_EIRP_EVENT = 519,
	QCA_NL80211_VENDOR_SUBCMD_TDMA_SCHEDULE_CONFIG = 520,
	QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP = 521,
	QCA_NL80211_VENDOR_SUBCMD_SET_PCP_TID_MAP = 522,
	QCA_NL80211_VENDOR_SUBCMD_GET_PCP_TID_MAP = 523,
	QCA_NL80211_VENDOR_SUBCMD_SET_TID_MAP_PRECEDENCE = 524,
	QCA_NL80211_VENDOR_SUBCMD_GET_TID_MAP_PRECEDENCE = 525,
	QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS = 526,
	QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON = 527,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT = 528,
	QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED = 529,
	QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM = 530,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT = 531,
	QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE = 532,
	QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS = 533,
};

/**
 * enum qca_wlan_vendor_attr_scan - Specifies vendor scan attributes
 *
 * @QCA_WLAN_VENDOR_ATTR_SCAN_IE: IEs that should be included as part of scan
 * @QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES: Nested unsigned 32-bit attributes
 *	with frequencies to be scanned (in MHz)
 * @QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS: Nested attribute with SSIDs to be scanned
 * @QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES: Nested array attribute of supported
 *	rates to be included
 * @QCA_WLAN_VENDOR_ATTR_SCAN_TX_NO_CCK_RATE: flag used to send probe requests
 *	at non CCK rate in 2GHz band
 * @QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS: Unsigned 32-bit scan flags
 * @QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE: Unsigned 64-bit cookie provided by the
 *	driver for the specific scan request
 * @QCA_WLAN_VENDOR_ATTR_SCAN_STATUS: Unsigned 8-bit status of the scan
 *	request decoded as in enum scan_status
 * @QCA_WLAN_VENDOR_ATTR_SCAN_MAC: 6-byte MAC address to use when randomisation
 *	scan flag is set
 * @QCA_WLAN_VENDOR_ATTR_SCAN_MAC_MASK: 6-byte MAC address mask to be used with
 *	randomisation
 * @QCA_WLAN_VENDOR_ATTR_SCAN_BSSID: 6-byte MAC address representing the
 *	specific BSSID to scan for.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME: Unsigned 64-bit dwell time in
 *	microseconds. This is a common value which applies across all
 *	frequencies specified by QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_PRIORITY: Priority of vendor scan relative to
 *	other scan requests. It is a u32 attribute and takes values from enum
 *	qca_wlan_vendor_scan_priority. This is an optional attribute.
 *	If this attribute is not configured, the driver shall use
 *	QCA_WLAN_VENDOR_SCAN_PRIORITY_HIGH as the priority of vendor scan.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_PAD: Attribute used for padding for 64-bit
 *	alignment.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID: This u8 attribute is used for OBSS scan
 *	when AP is operating as MLD to specify which link is requesting the
 *	scan or which link the scan result is for. No need of this attribute
 *	in other cases.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_SKIP_CHANNEL_RECENCY_PERIOD: Optional (u32). Skip
 *	scanning channels which are scanned recently within configured time
 *	(in ms).
 * @QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN: Optional (nested attribute). To enable
 * and config Split scan. Split scan is a Continuous Background Scan which splits single
 * scan into multiple smaller scans by with included rest times in between.
 * This is an AP mode scan, to scan the whole channel without degrading service
 * quality by adding rest time and wait time in between the scans. The AP sends
 * configured values from Application to driver, and the driver takes care of
 * algorithm to schedule the split scans and rest times.
 * Its sub-attributes are mentioned in enum qca_wlan_vendor_attr_split_scan_params.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME: Optional u32 rest time in milliseconds
 *	This represents the time to wait between scans on different channels.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME: Optional u32 wait time in milliseconds
 *	This is added for a continuous background scan feature. This represents
 *	the time to wait after scanning all channels and
 *	before starting over again next scan.
 */
enum qca_wlan_vendor_attr_scan {
	QCA_WLAN_VENDOR_ATTR_SCAN_INVALID_PARAM = 0,
	QCA_WLAN_VENDOR_ATTR_SCAN_IE = 1,
	QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES = 2,
	QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS = 3,
	QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES = 4,
	QCA_WLAN_VENDOR_ATTR_SCAN_TX_NO_CCK_RATE = 5,
	QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS = 6,
	QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE = 7,
	QCA_WLAN_VENDOR_ATTR_SCAN_STATUS = 8,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAC = 9,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAC_MASK = 10,
	QCA_WLAN_VENDOR_ATTR_SCAN_BSSID = 11,
	QCA_WLAN_VENDOR_ATTR_SCAN_DWELL_TIME = 12,
	QCA_WLAN_VENDOR_ATTR_SCAN_PRIORITY = 13,
	QCA_WLAN_VENDOR_ATTR_SCAN_PAD = 14,
	QCA_WLAN_VENDOR_ATTR_SCAN_LINK_ID = 15,
	QCA_WLAN_VENDOR_ATTR_SCAN_SKIP_CHANNEL_RECENCY_PERIOD = 16,
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN = 17,
	QCA_WLAN_VENDOR_ATTR_SCAN_RESTTIME = 18,
	QCA_WLAN_VENDOR_ATTR_SCAN_WAITTIME = 19,
	QCA_WLAN_VENDOR_ATTR_SCAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SCAN_MAX =
	QCA_WLAN_VENDOR_ATTR_SCAN_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_split_scan_params - These sub-attributes are added for
 * the Split Scan feature. This feature allows to split scan into multiple dwell
 * splits with included rest times in between.
 * These attr are nested inside QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN used under
 * the vendor sub-cmd QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN.
 *
 * @QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL: u32 dwell split value in milliseconds
 *	This is the dwell split time in milliseconds on a foreign channel for one
 *	single scan. For example, if total dwell time of 200msec is split into
 *	two split scans. Then the ATTR_SPLIT_SCAN_DWELL value will be 100msec.
 * @QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST: u32 dwell rest time in milliseconds
 *	Minimum time to rest before issuing the next consecutive
 *	scan on the same channel.
 * @QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE: u8 type of the split scan to run
 *	 0: disable/cancel the split scan,
 *	 1: run split scan only once in the background,
 *	 2: run split scan continuously in the background.
 */
enum qca_wlan_vendor_attr_split_scan_params {
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELL = 1,
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_DWELLREST = 2,
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_TYPE = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_MAX =
	QCA_WLAN_VENDOR_ATTR_SPLIT_SCAN_AFTER_LAST - 1
};

/**
 * enum scan_status - Specifies the valid values the vendor scan attribute
 *	QCA_WLAN_VENDOR_ATTR_SCAN_STATUS can take
 *
 * @VENDOR_SCAN_STATUS_NEW_RESULTS: implies the vendor scan is successful with
 *	new scan results
 * @VENDOR_SCAN_STATUS_ABORTED: implies the vendor scan was aborted in-between
 * @VENDOR_SPLIT_SCAN_COMPLETE_PER_CHANNEL: implies the vendor split scan is
 *	completed for a given channel frequency represented by attribute
 *	QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES.
 */
enum scan_status {
	VENDOR_SCAN_STATUS_NEW_RESULTS,
	VENDOR_SCAN_STATUS_ABORTED,
	VENDOR_SPLIT_SCAN_COMPLETE_PER_CHANNEL,
	VENDOR_SCAN_STATUS_MAX,
};

enum qca_nl80211_vendor_events {
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX = 0,
	QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX = 1,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX = 2,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT = 3,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT = 4,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX = 5,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX = 6,
	QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX = 7,
	QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE_INDEX = 8,
	QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX = 9,
	QCA_NL80211_VENDOR_SUBCMD_ESP_ESTIMATE_INDEX = 10,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_INDEX = 11,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX_INDEX = 12,
	QCA_NL80211_VENDOR_SUBCMD_DCS_INTERFERENCE_COMPUTE_INDEX = 13,
	QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR_INDEX = 14,
	QCA_NL80211_VENDOR_SUBCMD_TPC_EIRP_EVENT_INDEX = 15,
	QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE_INDEX = 16,
	QCA_NL80211_VENDOR_SUBCMD_OEM_DATA_INDEX = 17,
	QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS_EVENT_INDEX = 18,
	/**
	 * @QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX:
	 * Vendor event index used for notifications associated with
	 * %QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION.
	 *
	 * @ATTR - qca_wlan_vendor_attr_set_wifi
	 */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX = 19,
	QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON_INDEX = 20,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT_INDEX = 21,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION_INDEX = 22,
	/**
	 * @QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_INDEX:
	 * Vendor event index used for notifications associated with
	 * %QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED.
	 */
	QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_INDEX = 23,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT_INDEX = 24,
	QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE_INDEX = 25,
	QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS_INDEX = 26,
};

/**
 * ath12k_vendor_send_power_update_complete - Send 6 GHz power update complete
 * vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @afc - Pointer to AFC info
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_vendor_send_power_update_complete(struct ath12k *ar,
					 struct ath12k_afc_info *afc);

/**
 * ath12k_send_afc_request - Send AFC request vendor NL event to the application
 * @ar: Pointer to ath12k structure
 * @afc_req: Pointer to AFC host request
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_send_afc_request(struct ath12k *ar, struct ath12k_afc_host_request *afc_req);

/**
 * ath12k_send_afc_payload_reset - Send AFC payload reset vendor NL event
 * to userspace.
 *
 * @ar: Pointer to ar
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_send_afc_payload_reset(struct ath12k *ar);

/**
 * Opclass, channel and EIRP information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO
 * to understand the minimum length calculation.
 */
#define QCA_WLAN_AFC_RESP_OPCLASS_CHAN_EIRP_INFO_MIN_LEN       \
	NLA_ALIGN((NLA_HDRLEN + sizeof(uint8_t)) +              \
		  ((3 * NLA_HDRLEN) + sizeof(uint8_t) +         \
		   sizeof(uint32_t)))

/**
 * Frequency/PSD information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO
 * to understand the minimum length calculation.
 */
#define QCA_WLAN_AFC_RESP_FREQ_PSD_INFO_INFO_MIN_LEN           \
	NLA_ALIGN((3 * NLA_HDRLEN) +                            \
		  (2 * sizeof(uint16_t)) +                      \
		  sizeof(uint32_t))

/* enum qca_nl_afc_resp_type: Defines the format in which user space
 * application will send over the AFC response to driver.
 * @QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP: Payload in JSON format
 * @QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP: Payload in binary format
 * @QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP: Invalid payload format
 */
enum ath12k_nl_afc_resp_type {
	QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP,
	QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP,
	QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
};

enum qca_wlan_vendor_afc_response_attr {
	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_DATA_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_DATA,

	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_MAX,
};

enum qca_nl_afc_event_type {
	QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT,
	QCA_WLAN_VENDOR_AFC_POWER_UPDATE_COMPLETE_EVENT,
};

/**
 * enum qca_wlan_vendor_attr_afc_freq_psd_info: This enum is used with
 * nested attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST to update the frequency range
 * and PSD information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START: Required and type is
 * u32. This attribute is used to indicate the start of the queried frequency
 * range in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END: Required and type is u32.
 * This attribute is used to indicate the end of the queried frequency range
 * in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD: Required and type is u32.
 * This attribute will contain the PSD information for a single range as
 * specified by the QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START and
 * QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END attributes.
 *
 * The PSD power info (dBm/MHz) from user space should be multiplied
 * by a factor of 100 when sending to the driver to preserve granularity
 * up to 2 decimal places.
 * Example:
 *     PSD power value: 10.21 dBm/MHz
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_PSD_INFO: 1021.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD attribute will be used only
 * with nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and with
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * The following set of attributes will be used to exchange frequency and
 * corresponding PSD information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_freq_psd_info {
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD = 3,

	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_scan - Spectral scan config parameters
 */
enum qca_wlan_vendor_attr_spectral_scan {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INVALID = 0,
	/*
	 * Number of times the chip enters spectral scan mode before
	 * deactivating spectral scans. When set to 0, chip will enter spectral
	 * scan mode continuously. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT = 1,
	/*
	 * Spectral scan period. Period increment resolution is 256*Tclk,
	 * where Tclk = 1/44 MHz (Gmode), 1/40 MHz (Amode). u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD = 2,
	/* Spectral scan priority. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY = 3,
	/* Number of FFT data points to compute. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE = 4,
	/*
	 * Enable targeted gain change before starting the spectral scan FFT.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA = 5,
	/* Restart a queued spectral scan. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA = 6,
	/*
	 * Noise floor reference number for the calculation of bin power.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF = 7,
	/*
	 * Disallow spectral scan triggers after TX/RX packets by setting
	 * this delay value to roughly SIFS time period or greater.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY = 8,
	/*
	 * Number of strong bins (inclusive) per sub-channel, below
	 * which a signal is declared a narrow band tone. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR = 9,
	/*
	 * Specify the threshold over which a bin is declared strong (for
	 * scan bandwidth analysis). u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR = 10,
	/* Spectral scan report mode. u32 attribute. */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE = 11,
	/*
	 * RSSI report mode, if the ADC RSSI is below
	 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR,
	 * then FFTs will not trigger, but timestamps and summaries get
	 * reported. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE = 12,
	/*
	 * ADC RSSI must be greater than or equal to this threshold (signed dB)
	 * to ensure spectral scan reporting with normal error code.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR = 13,
	/*
	 * Format of frequency bin magnitude for spectral scan triggered FFTs:
	 * 0: linear magnitude, 1: log magnitude (20*log10(lin_mag)).
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT = 14,
	/*
	 * Format of FFT report to software for spectral scan triggered FFTs.
	 * 0: No FFT report (only spectral scan summary report)
	 * 1: 2-dword summary of metrics for each completed FFT + spectral scan
	 * report
	 * 2: 2-dword summary of metrics for each completed FFT + 1x-oversampled
	 * bins (in-band) per FFT + spectral scan summary report
	 * 3: 2-dword summary of metrics for each completed FFT + 2x-oversampled
	 * bins (all) per FFT + spectral scan summary report
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE = 15,
	/*
	 * Number of LSBs to shift out in order to scale the FFT bins.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE = 16,
	/*
	 * Set to 1 (with spectral_scan_pwr_format=1), to report bin magnitudes
	 * in dBm power. u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ = 17,
	/*
	 * Per chain enable mask to select input ADC for search FFT.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK = 18,
	/*
	 * An unsigned 64-bit integer provided by host driver to identify the
	 * spectral scan request. This attribute is included in the scan
	 * response message for @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START
	 * and used as an attribute in
	 * @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP to identify the
	 * specific scan to be stopped.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE = 19,
	/* Skip interval for FFT reports. u32 attribute */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD = 20,
	/* Set to report only one set of FFT results.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT = 21,
	/* Debug level for spectral module in driver.
	 * 0 : Verbosity level 0
	 * 1 : Verbosity level 1
	 * 2 : Verbosity level 2
	 * 3 : Matched filterID display
	 * 4 : One time dump of FFT report
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DEBUG_LEVEL = 22,
	/* Type of spectral scan request. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_attr_spectral_scan_request_type.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE = 23,
	/* This specifies the frequency span over which spectral
	 * scan would be carried out. Its value depends on the
	 * value of QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE and
	 * the relation is as follows.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL
	 *    Not applicable. Spectral scan would happen in the
	 *    operating span.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE
	 *    Center frequency (in MHz) of the span of interest or
	 *    for convenience, center frequency (in MHz) of any channel
	 *    in the span of interest. For 80+80 MHz agile spectral scan
	 *    request it represents center frequency (in MHz) of the primary
	 *    80 MHz span or for convenience, center frequency (in MHz) of any
	 *    channel in the primary 80 MHz span. If agile spectral scan is
	 *    initiated without setting a valid frequency it returns the
	 *    error code
	 *    (QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED).
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY = 24,
	/* Spectral scan mode. u32 attribute.
	 * It uses values defined in enum qca_wlan_vendor_spectral_scan_mode.
	 * If this attribute is not present, it is assumed to be
	 * normal mode (QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL).
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE = 25,
	/* Spectral scan error code. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_error_code.
	 * This attribute is included only in failure scenarios.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE = 26,
	/* 8-bit unsigned value to enable/disable debug of the
	 * Spectral DMA ring.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_RING_DEBUG = 27,
	/* 8-bit unsigned value to enable/disable debug of the
	 * Spectral DMA buffers.
	 * 1-enable, 0-disable
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_BUFFER_DEBUG = 28,
	/* This specifies the frequency span over which spectral scan would be
	 * carried out. Its value depends on the value of
	 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE and the relation is as
	 * follows.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL
	 *    Not applicable. Spectral scan would happen in the operating span.
	 * QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE
	 *    This attribute is applicable only for agile spectral scan
	 *    requests in 80+80 MHz mode. It represents center frequency (in
	 *    MHz) of the secondary 80 MHz span or for convenience, center
	 *    frequency (in MHz) of any channel in the secondary 80 MHz span.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2 = 29,
	/* This attribute specifies the bandwidth to be used for Spectral scan
	 * operation. This is an u8 attribute and uses the values in enum
	 * nl80211_chan_width.  This is an optional attribute.
	 * If this attribute is not populated, the driver should configure the
	 * Spectral scan bandwidth to the maximum value supported by the target
	 * for the current operating bandwidth.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH = 30,
	/* Spectral FFT recapture flag attribute, to enable/disable FFT
	 * recapture. Recapture can only be enabled for Scan period greater
	 * than 52us.
	 * If this attribute is enabled, re-triggers will be enabled in uCode
	 * when AGC gain changes.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_RECAPTURE = 31,
	/* Spectral data transport mode. u32 attribute. It uses values
	 * defined in enum qca_wlan_vendor_spectral_data_transport_mode.
	 * This is an optional attribute. If this attribute is not populated,
	 * the driver should configure the default transport mode to netlink.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_DATA_TRANSPORT_MODE = 33,
	/* Spectral scan completion timeout. u32 attribute. This
	 * attribute is used to configure a timeout value (in us). The
	 * timeout value would be from the beginning of a spectral
	 * scan. This is an optional attribute. If this attribute is
	 * not populated, the driver would internally derive the
	 * timeout value.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETION_TIMEOUT = 34,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_diag_stats - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS.
 */
enum qca_wlan_vendor_attr_spectral_diag_stats {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_INVALID = 0,
	/* Number of spectral TLV signature mismatches.
	 * u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SIG_MISMATCH = 1,
	/* Number of spectral phyerror events with insufficient length when
	 * parsing for secondary 80 search FFT report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SEC80_SFFT_INSUFFLEN = 2,
	/* Number of spectral phyerror events without secondary 80
	 * search FFT report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_NOSEC80_SFFT = 3,
	/* Number of spectral phyerror events with vht operation segment 1 id
	 * mismatches in search fft report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG1ID_MISMATCH = 4,
	/* Number of spectral phyerror events with vht operation segment 2 id
	 * mismatches in search fft report. u64 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG2ID_MISMATCH = 5,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_cap - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO.
 */
enum qca_wlan_vendor_attr_spectral_cap {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_INVALID = 0,
	/* Flag attribute to indicate phydiag capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_PHYDIAG = 1,
	/* Flag attribute to indicate radar detection capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RADAR = 2,
	/* Flag attribute to indicate spectral capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_SPECTRAL = 3,
	/* Flag attribute to indicate advanced spectral capability */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_ADVANCED_SPECTRAL = 4,
	/* Spectral hardware generation. u32 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_cap_hw_gen.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN = 5,
	/* Spectral bin scaling formula ID. u16 attribute.
	 * It uses values defined in enum
	 * qca_wlan_vendor_spectral_scan_cap_formula_id.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_FORMULA_ID = 6,
	/* Spectral bin scaling param - low level offset.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_LOW_LEVEL_OFFSET = 7,
	/* Spectral bin scaling param - high level offset.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HIGH_LEVEL_OFFSET = 8,
	/* Spectral bin scaling param - RSSI threshold.
	 * s16 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RSSI_THR = 9,
	/* Spectral bin scaling param - default AGC max gain.
	 * u8 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_DEFAULT_AGC_MAX_GAIN = 10,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 20/40/80 MHz modes.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL = 11,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 160 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_160 = 12,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 80+80 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_80_80 = 13,
	/* Number of spectral detectors used for scan in 20 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_20_MHZ = 14,
	/* Number of spectral detectors used for scan in 40 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_40_MHZ = 15,
	/* Number of spectral detectors used for scan in 80 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80_MHZ = 16,
	/* Number of spectral detectors used for scan in 160 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_160_MHZ = 17,
	/* Number of spectral detectors used for scan in 80+80 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80P80_MHZ = 18,
	/* Flag attribute to indicate agile spectral scan capability
	 * for 320 MHz mode.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_320 = 19,
	/* Number of spectral detectors used for scan in 320 MHz.
	 * u32 attribute.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_320_MHZ = 20,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_spectral_scan_status - used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS.
 */
enum qca_wlan_vendor_attr_spectral_scan_status {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_INVALID = 0,
	/* Flag attribute to indicate whether spectral scan is enabled */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ENABLED = 1,
	/* Flag attribute to indicate whether spectral scan is in progress*/
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ACTIVE = 2,
	/* Spectral scan mode. u32 attribute.
	 * It uses values defined in enum qca_wlan_vendor_spectral_scan_mode.
	 * If this attribute is not present, normal mode
	 * (QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL is assumed to be
	 * requested.
	 */
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE = 3,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_AFTER_LAST - 1,
};

/**
 * qca_wlan_vendor_attr_spectral_scan_request_type: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START. This represents the
 * spectral scan request types.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN_AND_CONFIG: Request to
 * set the spectral parameters and start scan.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN: Request to
 * only set the spectral parameters.
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG: Request to
 * only start the spectral scan.
 */
enum qca_wlan_vendor_attr_spectral_scan_request_type {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN_AND_CONFIG,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG,
};

/**
 * qca_wlan_vendor_spectral_scan_mode: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START and
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS. This represents the
 * spectral scan modes.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL: Normal spectral scan:
 * spectral scan in the current operating span.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE: Agile spectral scan:
 * spectral scan in the configured agile span.
 */
enum qca_wlan_vendor_spectral_scan_mode {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE = 1,
};

/**
 * qca_wlan_vendor_spectral_scan_error_code: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED: Changing the value
 * of a parameter is not supported.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED: Requested spectral scan
 * mode is not supported.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE: A parameter
 * has invalid value.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED: A parameter
 * is not initialized.
 */
enum qca_wlan_vendor_spectral_scan_error_code {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED = 1,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE = 2,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED = 3,
};

/**
 * qca_wlan_vendor_spectral_scan_cap_hw_gen: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN to the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO. This represents the
 * spectral hardware generation.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_1: generation 1
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_2: generation 2
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_3: generation 3
 */
enum qca_wlan_vendor_spectral_scan_cap_hw_gen {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_1 = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_2 = 1,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_3 = 2,
};

/**
 * qca_wlan_vendor_spectral_scan_cap_formula_id: Attribute values for
 * QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_FORMULA_ID in the vendor subcmd
 * QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO. This represents the
 * Spectral bin scaling formula ID.
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_NO_SCALING: No scaling
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_AGC_GAIN_RSSI_CORR_BASED: AGC gain
 * and RSSI threshold based formula.
 */
enum qca_wlan_vendor_spectral_scan_cap_formula_id {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_NO_SCALING = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_AGC_GAIN_RSSI_CORR_BASED = 1,
};

/**
 * enum qca_wlan_vendor_spectral_data_transport_mode - Attribute
 * values for QCA_WLAN_VENDOR_ATTR_SPECTRAL_DATA_TRANSPORT_MODE.
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_NETLINK: Use netlink to
 * send spectral data to userspace applications.
 * @QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_RELAY: Use relay interface
 * to send spectral data to userspace applications.
 */
enum qca_wlan_vendor_spectral_data_transport_mode {
	QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_NETLINK = 0,
	QCA_WLAN_VENDOR_SPECTRAL_DATA_TRANSPORT_RELAY = 1,
};

/* enum qca_wlan_vendor_spectral_scan_complete_status - Attribute
 * values for QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS to
 * indicate the completion status for a spectral scan.
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_SUCCESSFUL:
 * Indicates a successful completion of the scan
 *
 * @QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_TIMEOUT: Indicates
 * a timeout has occurred while processing the spectral reports.
 */
enum qca_wlan_vendor_spectral_scan_complete_status {
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_SUCCESSFUL = 0,
	QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_TIMEOUT = 1,
};

/* enum qca_wlan_vendor_attr_spectral_scan_complete- Definition of
 * attributes for @QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE
 * to indicate scan status and samples received from hardware.
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS: u32 attribute.
 * Indicates completion status, either the scan is successful or a timeout
 * is issued by the driver.
 * See enum qca_wlan_vendor_spectral_scan_complete_status
 *
 * @QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_RECEIVED_SAMPLES: u32
 * attribute.  Number of spectral samples received after the scan has
 * started.
 */
enum qca_wlan_vendor_attr_spectral_scan_complete {
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS = 1,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_RECEIVED_SAMPLES = 2,

	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_MAX =
	QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info: This enum is used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_CHAN_LIST_INFO to update the
 * channel list and corresponding EIRP information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM: Required and type is u8.
 * This attribute is used to indicate queried channel from
 * the operating class indicated in QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP: Optional and type is u32.
 * This attribute is used to configure the EIRP power info corresponding
 * to the channel number indicated in QCA_WLAN_VENDOR_ATTR_AFC_CHAN_NUM.
 * The EIRP power info(dBm) from user space should be multiplied
 * by a factor of 100 when sending to Driver to preserve granularity up to
 * 2 decimal places.
 * Example:
 *     EIRP power value: 34.23 dBm
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO: 3423.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO attribute will only be used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * with QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE:
 *
 * The following set of attributes will be used to exchange Channel and
 * corresponding EIRP information for AFC between the user space and Driver.
 */
enum qca_wlan_vendor_attr_afc_chan_eirp_info {
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_opclass_info: This enum is used with nested
 * attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_REQ_OPCLASS_CHAN_INFO to update the operating class,
 * channel, and EIRP related information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS: Required and type is u8.
 * This attribute is used to indicate the operating class, as listed under
 * IEEE Std 802.11-2020 Annex E Table E-4, for the queried channel list.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST: Array of nested attributes
 * for updating the channel number and EIRP power information.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info.
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE_EXPIRY.
 *
 * m - Total number of operating classes.
 * n, j - Number of queried channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * m - Total number of operating classes.
 * n, j - Number of channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[j - 1]
 *
 * The following set of attributes will be used to exchange operating class
 * information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_opclass_info {
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_afc_event_type: Defines values for AFC event type.
 * Attribute used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY: AFC expiry event sent from the
 * driver to userspace in order to query the new AFC power values.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE: Power update
 * complete event will be sent from the driver to userspace to indicate
 * processing of the AFC response.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET: AFC payload reset event
 * will be sent from the driver to userspace to indicate last received
 * AFC response data has been cleared on the AP due to invalid data
 * in the QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE.
 *
 * The following enum defines the different event types that will be
 * used by the driver to help trigger corresponding AFC functionality in user
 * space.
 */
enum qca_wlan_vendor_afc_event_type {
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY = 0,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE = 1,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET = 2,
};

/**
 * enum qca_wlan_vendor_afc_evt_status_code: Defines values AP will use to
 * indicate AFC response status.
 * Enum used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS: Success
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT: Indicates AFC indication
 * command was not received within the expected time of the AFC expiry event
 * being triggered.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR: Indicates AFC data
 * parsing error by the driver.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR: Indicates any other local
 * error.
 *
 * The following enum defines the status codes that the driver will use to
 * indicate whether the AFC data is valid or not.
 */
enum qca_wlan_vendor_afc_evt_status_code {
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS = 0,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT = 1,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR = 2,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR = 3,
};

/**
 * enum qca_wlan_vendor_attr_afc_event: Defines attributes to be used with
 * vendor event QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT. These attributes will
 * support sending only a single request to the user space at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE: Required u8 attribute.
 * Used with event to notify the type of AFC event received.
 * Valid values are defined in enum qca_wlan_vendor_afc_event_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 *
 * This attribute is used to indicate the AP deployment type in the AFC request.
 * Valid values are defined in enum qca_wlan_vendor_afc_ap_deployment_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID: Required u32 attribute.
 * Unique request identifier generated by the AFC client for every
 * AFC expiry event trigger. See also QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID.
 * The user space application is responsible for ensuring no duplicate values
 * are in-flight with the server, e.g., by delaying a request, should the same
 * value be received from different radios in parallel.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION: u32 attribute. Optional.
 * It is used when the QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY, otherwise unused.
 *
 * This attribute indicates the AFC spec version information. This will
 * indicate the AFC version AFC client must use to query the AFC data.
 * Bits 15:0  - Minor version
 * Bits 31:16 - Major version
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER: u16 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 * This attribute indicates the minimum desired power (in dBm) for
 * the queried spectrum.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * Valid values are defined in enum qca_wlan_vendor_afc_evt_status_code.
 * This attribute is used to indicate if there were any errors parsing the
 * AFC response.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE: s32 attribute. Required
 * when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST: Array of nested attributes
 * for updating the list of frequency ranges to be queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_freq_psd_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST: Array of nested attributes
 * for updating the list of operating classes and corresponding channels to be
 * queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX: Required u32 attribute.
 * It notifies the hardware index for which the AFC request event is sent
 * to the AFC application.
 */
enum qca_wlan_vendor_attr_afc_event {
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE = 8,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME = 9,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST = 10,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST = 11,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX = 12,

	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_reg_params {
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CMD = 1,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS_CHAN = 2,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_LINKID = 3,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_DISABLE = 4,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_OPCLASS = 5,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_CHAN_LIST = 6,

	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_MAX =
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_fb_chan - Nested attributes for a
 * full-bandwidth blocked channel entry.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW: u32 attribute. Required.
 * Blocked channel width encoded using enum nl80211_chan_width.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ: u16 attribute.
 * Required. Blocked bandwidth center frequency in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_PRI20_BITMAP: u16 bitmap.
 * Required.
 * Bitmap of blocked primary 20 MHz subchannels for
 * QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW centered at
 * QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ.
 * Each bit N in the bitmap corresponds to the Nth 20 MHz subchannel
 * from the lower edge (low edge = center_freq - BW / 2 + 10 MHz).
 * If bit N is set, then that 20 MHz subchannel is blocked from being used as
 * primary 20 MHz channel. More than one channel bit can be set.
 * Valid bits by width:
 * 20 MHz: bit [0]
 * 40 MHz: bits [0..1]
 * 80 MHz: bits [0..3]
 * 160 MHz: bits [0..7]
 * 320 MHz: bits [0..15]
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_fb_chan {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_BW = 1,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_CENTER_FREQ = 2,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_PRI20_BITMAP = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_FB_CHAN_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_pc_chan - Nested attributes for
 * a punctured blocked channel entry.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW: u32 attribute.
 * Required. Blocked channel width encoded using enum nl80211_chan_width.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ: u16 attribute.
 * Required. Blocked bandwidth center frequency in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_PUNC_PATTERN_BITMAP:
 * u32 bitmap. Required.
 * Bitmap of blocked puncturing patterns for
 * QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW centered at
 * QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ.
 * Each bit in the bitmap corresponds to a puncture pattern.
 * If bit N is set, the puncture pattern corresponding to bit N for the
 * width specified by QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW is
 * blocked. More than one puncture pattern can be blocked.
 * Each bit N corresponds to a puncture pattern for a particular BW as:
 * 80 MHz:
 *     idx 0->0x0001, 1->0x0002, 2->0x0004, 3->0x0008
 * 160 MHz:
 *     idx 0->0x0001, 1->0x0002, 2->0x0004, 3->0x0008
 *     idx 4->0x0010, 5->0x0020, 6->0x0040, 7->0x0080
 *     idx 8->0x000C, 9->0x0003, 10->0x00C0, 11->0x0030
 * 320 MHz:
 *     idx 0->0x000C, 1->0x0003, 2->0x00C0, 3->0x0030
 *     idx 4->0x0C00, 5->0x0300, 6->0xC000, 7->0x3000
 *     idx 8->0x000F, 9->0x00F0, 10->0x0F00, 11->0xF000
 *     idx 12->0xF003, 13->0xF00C, 14->0xF030, 15->0xF0C0
 *     idx 16->0xF300, 17->0xFC00, 18->0x003F, 19->0x00CF
 *     idx 20->0x030F, 21->0x0C0F, 22->0x300F, 23->0xC00F
 *
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_pc_chan {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_BW = 1,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_CENTER_FREQ = 2,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_PUNC_PATTERN_BITMAP = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PC_CHAN_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_pwr_mode - Nested attributes for
 * one 6 GHz power-mode entry in
 * QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST.
 * Valid only for NL80211_BAND_6GHZ
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_ID: u8 attribute. Required.
 * 6 GHz AP power mode value:
 * 0 = LPI, 1 = SP, 2 = VLP.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_FB_CHAN_LIST: Nested array.
 * Optional. Uses enum qca_wlan_vendor_attr_hw_blocked_chans_fb_chan.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_PC_CHAN_LIST: Nested array.
 * Optional. Uses enum qca_wlan_vendor_attr_hw_blocked_chans_pc_chan.
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_pwr_mode {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_ID = 1,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_FB_CHAN_LIST = 2,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_PC_CHAN_LIST = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_PWR_MODE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_band - Nested per-band blocked
 * channel attributes in response/event payload.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_ID: u32 attribute.
 * Required. Band identifier from enum nl80211_band.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_FB_CHAN_LIST: Nested array.
 * Optional. Uses enum qca_wlan_vendor_attr_hw_blocked_chans_fb_chan.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PC_CHAN_LIST: Nested array.
 * Optional. Uses enum qca_wlan_vendor_attr_hw_blocked_chans_pc_chan.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST: Nested array.
 * Optional. For NL80211_BAND_6GHZ only. Uses enum
 * qca_wlan_vendor_attr_hw_blocked_chans_pwr_mode.
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_band {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_ID = 1,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_FB_CHAN_LIST = 2,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PC_CHAN_LIST = 3,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_PWR_MODE_LIST = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_BAND_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_radio - Nested per-radio
 * blocked-channel attributes in response/event payload.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_RADIO_INDEX: s32 attribute.
 * Required. Non-negative radio index of this radio entry (same semantics as
 * NL80211_ATTR_WIPHY_RADIO_INDEX).
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_BAND_LIST: Nested array of
 * per-band entries. Required.
 * Uses enum qca_wlan_vendor_attr_hw_blocked_chans_band.
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_radio {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_RADIO_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_BAND_LIST = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RADIO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_resp - Response/event attributes
 * used with QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST: Nested array of
 * per-radio entries. Required. Each radio entry uses
 * enum qca_wlan_vendor_attr_hw_blocked_chans_radio.
 * If requested for a particular radio index, the response shall have a single
 * radio in the array corresponding to the requested radio index.
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_resp {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_RADIO_LIST = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_RESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_hw_blocked_chans_req - Request attributes used with
 * QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS.
 *
 * @QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX: s32 attribute.
 * Optional. Radio index with the same semantics as
 * NL80211_ATTR_WIPHY_RADIO_INDEX. When absent or set to -1, driver returns
 * blocked-channel data for all radios.
 */
enum qca_wlan_vendor_attr_hw_blocked_chans_req {
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX = 1,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX =
	QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_AFTER_LAST - 1,
};

enum qca_wlan_vendor_reg_params {
	QCA_WLAN_VENDOR_REG_PARAMS_NUM_OPCLASS = 0,
	QCA_WLAN_VENDOR_REG_PARAMS_CHAN_NUM = 1,
	QCA_WLAN_VENDOR_REG_PARAMS_TXPOWER = 2,
	QCA_WLAN_VENDOR_REG_PARAMS_OPCLASS_LIST = 3,
	QCA_WLAN_VENDOR_REG_PARAMS_DISABLE_OPCLASS_CHANS = 4,
};

enum qca_wlan_vendor_attr_reg_params_update {
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_NUM_OPCLASS = 1,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_CHAN_NUM = 2,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_TXPOWER = 3,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_OPCLASS_LIST = 4,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_OPCLASS_CHAN = 5,

	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_MAX =
	QCA_WLAN_VENDOR_ATTR_REG_PARAMS_UPDATE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_vap_submode_type - VAP submode types for QCA WLAN vendor
 *
 * This enumeration defines different submodes that a Virtual Access Point (VAP)
 * can operate in. These submodes are used to configure specific behaviors or
 * roles for the VAP.
 *
 * @QCA_WLAN_VENDOR_VAP_SUBMODE_NONE: Default mode, no special submode applied.
 * @QCA_WLAN_VENDOR_VAP_SUBMODE_MESH: VAP operates in mesh mode for mesh networking.
 * @QCA_WLAN_VENDOR_VAP_SUBMODE_SCAN: VAP operates in scan mode, typically for
 *     off-channel scanning / scan radio specific operations.
 *
 * @QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_AFTER_LAST: Internal marker for the end of
 *     valid submode attributes. Not to be used directly.
 * @QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MAX: Maximum valid attribute value for VAP
 *     submode. Used for bounds checking.
 */
enum qca_wlan_vendor_vap_submode_type {
	QCA_WLAN_VENDOR_VAP_SUBMODE_NONE = 0,
	QCA_WLAN_VENDOR_VAP_SUBMODE_MESH = 1,
	QCA_WLAN_VENDOR_VAP_SUBMODE_SCAN = 2,

	QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MAX =
	QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_config {
	QCA_WLAN_VENDOR_ATTR_CONFIG_INVALID = 0,
	/* Unsigned 32-bit attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND = 17,
	/* Unsigned 32-bit value attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE = 18,
	/* Unsigned 32-bit data attribute for generic command response */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA = 19,
	/* Unsigned 32-bit length attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH = 20,
	/* Unsigned 32-bit flags attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS = 21,
	/* Unsigned 32-bit, specifies the interface index (netdev) for which the
	 * corresponding configurations are applied. If the interface index is
	 * not specified, the configurations are attributed to the respective
	 * wiphy.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX = 24,
	/* 8-bit unsigned value. Used to specify the MLO link ID of a link
	 * that is being configured. This attribute must be included in each
	 * record nested inside %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS, and
	 * may be included without nesting to indicate the link that is the
	 * target of other configuration attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID = 99,
	/* 8-bit unsigned value to configure the interface offload type
	 *
	 * This attribute is used to configure the interface offload capability.
	 * User can configure software based acceleration, hardware based
	 * acceleration, or a combination of both using this option. More
	 * details on each option is described under the enum definition below.
	 * Uses enum qca_wlan_intf_offload_type for values.
	 */
	QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE = 120,

	/* Nested attribute to configure or report Estimated Service Parameters
	 * (ESP). This contains nested attributes defined in
	 * enum qca_wlan_vendor_attr_config_esp_param.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS = 143,

	/* 8-bit unsigned value to configure VAP submodes, This enables to
	 * have QCA Proprietary VAP modes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE = 144,

	/* 8-bit unsigned value to enable/disable dynamic VLAN handling
	 * for AP mode.
	 * 1 - Enable, 0 - Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_VLAN = 145,

	/* 8-bit unsigned value. Used to specify the HW Radio Index of a wiphy
	 * device that is being configured. This attribute may be included in
	 * %QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION or
	 * %QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION subcmds to
	 * specify a particular Radio of the wiphy device.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX = 150,

	/* Attribute to query/report 6 GHz VLP priority threshold frequency in MHz.
	 * This can be queried using
	 * %QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION.
	 *
	 * For GET requests, userspace includes this attribute as a selector.
	 * The driver replies with the threshold frequency for the given radio.
	 *
	 * This threshold is used by Automatic Channel Selection (ACS):
	 * 6 GHz frequencies above this threshold are given priority during
	 * channel selection.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ = 151,

	/* 8-bit unsigned value to enable/disable the driver to allow forwarding
	 * 3-address multicast frames in WDS mode
	 * 1-Enable, 0-Disable.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC = 152,

	/* Indicates whether the current chainmask on the radio specified by
	 * %QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX supports Agile DFS.
	 * 1 = agile DFS capable, 0 = not capable. Used with
	 * %QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION as a query
	 * selector and in the unsolicited vendor event sent on dynamic
	 * chainmask changes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE = 153,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_mac_config {
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_RADIO_BSS_ID,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_IFTYPE,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_FLAGS,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_GRP_ID,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_GRP_SIZE,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MBSSID_ENABLED,

	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_tele_delay_hist_bucket {
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_0 = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_1,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_2,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_3,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_4,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_5,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_6,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_7,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_8,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_9,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_10,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_11,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_12,

	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay_hist {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_SW_ENQEUE_DELAY = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_REAP_STACK,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_TX_COMP_DELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_DELAY_PERCENTILE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY_TSF,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY_JITTER_TSF,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay_hwdelay {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAXIMUM = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MINIMUM,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AVERAGE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_SUCCESS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_FAILURE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_INVALID_PKTS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_HISTOGRAM,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AFTER_LAST,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_NETWORK_DELAY_AVG = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_SOFTWARE_DELAY_AVG,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HARDWARE_DELAY_AVG,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_TID,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_QUEUE_ID,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_AFTER_LAST -1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_advance_stats {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_AFTER_LAST -1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_drop_res {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_MPDU = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_TX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_NOTX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_AGED_FRAMES,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON2,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON3,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_DISABLE_QUEUE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_TILL_NONMATCHING,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_THRESHOLD_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_LINK_DESC_UNAVAIL_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_MSDU_OR_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MULTICAST_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_RR,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_AFTER_LAST - 1,
};


enum qca_wlan_vendor_attr_tele_sdwftx_pkt_type_mcs {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX2,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX3,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX4,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX5,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX6,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX7,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX8,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX9,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX10,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX11,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX12,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX13,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX14,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX15,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX16,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX17,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX18,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX19,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX20,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX21,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX22,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX23,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX24,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_pkt_type {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_B,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_N,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_AC,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_AX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_BA,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_BE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_AZ,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_N_GF,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_BN,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwftx {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SUCCESS = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_DEPTH,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS_RATE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MIN_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AVG_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_ERROR_RATE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PERCENTAGE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PKTS_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TOTAL_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MULTIPLE_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_REINJECT_PKTS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SERVICE_INTERVAL,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_BURST_SIZE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TID,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_ID,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AFTER_LAST - 1,
};

/* SDWF MSDUQ dimension constants.
 * QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX: number of QoS TIDs (= QOS_TID_MAX).
 * QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX: number of MSDUQs per TID
 *     (= QOS_TID_MDSUQ_MAX).
 */
#define QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX		8
#define QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX	2

/**
 * enum qca_wlan_vendor_attr_afc_response: Defines attributes to be used
 * with vendor command QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE. These attributes
 * will support sending only a single AFC response to the driver at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA: Type is NLA_STRING. Required attribute.
 * This attribute will be used to send a single Spectrum Inquiry response object
 * from the 'availableSpectrumInquiryResponses' array object from the response
 * JSON.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE: Required u32 attribute.
 *
 * This attribute indicates the period (in seconds) for which the response
 * data received is valid for.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID: Required u32 attribute.
 *
 * This attribute indicates the request ID for which the corresponding
 * response is being sent for. See also QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE: Required u32 attribute.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME: Required u32 attribute.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE: Required s32 attribute.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO: Array of nested attributes
 * for PSD info of all the queried frequency ranges. It uses the attributes
 * defined in enum qca_wlan_vendor_attr_afc_freq_psd_info. Required attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO: Array of nested
 * attributes for EIRP info of all queried operating class/channels. It uses
 * the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info and
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info. Required attribute.
 *
 */
enum qca_wlan_vendor_attr_afc_response {
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO = 8,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX = 9,

	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tpc_eirp_event {
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_EIRP_DBM = 2,

	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_TPC_EIRP_EVENT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_6ghz_power_modes: Defines values of power modes a 6GHz
 * radio can operate in.
 * Enum used by QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE attribute.
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI: LPI AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_SP: SP AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP: VLP AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_LPI: LPI Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_SP: SP Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_VLP: VLP Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_LPI: LPI Subordinate Client
 *
 */
enum qca_wlan_vendor_6ghz_power_modes {
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI = 0,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_SP = 1,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP = 2,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_LPI = 3,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_SP = 4,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_VLP = 5,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_LPI = 6,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_SP = 7,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_VLP = 8,
};

/**
 * enum qca_wlan_vendor_client_type - Client types for 6 GHz operation
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_DEFAULT: Default client type operating under standard rules.
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_SUBORDINATE: Subordinate client type, typically operating under
 *                                           a controlling AP.
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_MAX: Maximum value placeholder for bounds checking and validation.
 */
enum qca_wlan_vendor_client_type {
	QCA_WLAN_VENDOR_CLIENT_TYPE_DEFAULT = 0,
	QCA_WLAN_VENDOR_CLIENT_TYPE_SUBORDINATE = 1,
	QCA_WLAN_VENDOR_CLIENT_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_set_6ghz_power_mode - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE command to configure the
 * 6 GHz power mode.
 *
 * QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE: Unsigned 8-bit integer representing
 * the 6 GHz power mode
 */
enum qca_wlan_vendor_set_6ghz_power_mode {
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE = 1,
	QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID = 2,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX =
		QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_reg_eirp - Vendor attributes for regulatory EIRP handling
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_INVALID: Invalid attribute (placeholder).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE: Attribute indicating the AP power mode type
 *                                            (e.g., LPI, SP, VLP) for 6 GHz regulatory
 *                                            configuration.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE: Attribute indicating the client type
 *                                             (e.g., default or subordinate) for 6 GHz
 *                                             regulatory configuration.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX: Maximum attribute index (internal use for bounds checking).
 */
enum qca_wlan_vendor_attr_reg_eirp {
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX,
};

/**
 * enum qca_wlan_vendor_attr_reg_eirp_update - Vendor attributes for EIRP update
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_INVALID: Invalid attribute (placeholder).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ: Attribute representing the center
 *                                                    frequency (in MHz) of the channel.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM: Attribute representing the hardware channel
 *                                                 number.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER: Attribute representing the maximum regulatory
 *                                                 transmit power (in dBm).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_MAX: Maximum attribute index (used for validation and
 *                                            bounds checking).
 */
enum qca_wlan_vendor_attr_reg_eirp_update {
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_240mhz_info - Represents the vendor specific
 * 240MHz information. This enum is used by
 * %QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS: u8 mandatory attribute.
 * This is for the beamformee SS capability to indicate the maximum number of
 * spatial streams that the STA can receive in an EHT sounding NDP for 240 MHz.
 * The range of the vale is from 3 to 7.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS: u8 mandatory attribute.
 * This indicates the maximum value of the TXVECTOR parameter NUM_STS
 * supported by the beamformer for an EHT sounding NDP.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO: flag optional attribute.
 * If present, this indicates the support for non-OFDMA UL MU-MIMO reception of
 * an EHT TB PPDU, for PPDU with 240MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER: flag optional attribute.
 * If present, this indicates the support for non-OFDMA DL MU-MIMO transmission
 * and the required MU sounding, for PPDU 240MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP: u8 array of size 3, mandatory
 * attribute. This indicates the maximum number of spatial streams supported for
 * reception and the maximum number of spatial streams that the STA can
 * transmit, for each MCS value, in a PPDU 240MHz.
 */
enum qca_wlan_vendor_attr_240mhz_info {
	QCA_WLAN_VENDOR_ATTR_240MHZ_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS = 1,
	QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS = 2,
	QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO = 3,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER = 4,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_240MHZ_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MAX =
	QCA_WLAN_VENDOR_ATTR_240MHZ_AFTER_LAST - 1,
};

/**
 * enum qca_nl80211_vendor_fw_recovery_event_type - Vendor event types for
 * firmware recovery notifications.
 *
 * @QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_RECOVERY_INVALID:
 *     Invalid event type (placeholder).
 *
 * @QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_RECOVERY_DONE:
 *     Indicates that firmware recovery has completed successfully and the
 *     device is operational again.
 *
 * @QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_DUMP_READY:
 *     Indicates that the firmware crash dump or diagnostic data collection
 *     is ready.
 *
 * @QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_DUMP_COMPLETED:
 *     Indicates that the firmware crash dump or diagnostic data collection
 *     has finished.
 *
 * @QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_FW_ASSERT:
 *     Indicates that the firmware has asserted (crashed). This is typically
 *     the first event in the recovery sequence.
 */
enum qca_nl80211_vendor_fw_recovery_event_type {
	QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_RECOVERY_INVALID = 0,
	QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_RECOVERY_DONE = 1,
	QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_DUMP_READY = 2,
	QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_DUMP_COMPLETED = 3,
	QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_FW_ASSERT = 4,
};

/**
 * enum qca_nl80211_vendor_fw_recovery_attr - Vendor attributes for firmware
 * recovery event.
 *
 * @QCA_VENDOR_ATTR_FW_RECOVERY_INVALID:
 *     Invalid attribute (placeholder).
 *
 * @QCA_VENDOR_ATTR_FW_RECOVERY_EVENT_TYPE:
 *     Mandatory attribute (u8) specifying the event type. Valid values are
 *     defined in enum qca_nl80211_vendor_fw_recovery_event_type.
 *
 * @QCA_WLAN_VENDOR_FW_RECOVERY_HW_LINK_ID:
 *     Mandatory attribute (u8) representing the hardware link ID (HW_LINK_ID)
 *     or SoC identifier where the crash or recovery event occurred.
 *
 * @QCA_VENDOR_ATTR_FW_RECOVERY_AFTER_LAST:
 *     Internal marker for the end of attributes.
 *
 * @QCA_VENDOR_ATTR_FW_RECOVERY_MAX:
 *     Maximum attribute index (for bounds checking).
 */
enum qca_nl80211_vendor_fw_recovery_attr {
	QCA_VENDOR_ATTR_FW_RECOVERY_INVALID = 0,
	QCA_VENDOR_ATTR_FW_RECOVERY_EVENT_TYPE = 1,
	QCA_WLAN_VENDOR_FW_RECOVERY_HW_LINK_ID = 2,
	QCA_VENDOR_ATTR_FW_RECOVERY_AFTER_LAST,
	QCA_VENDOR_ATTR_FW_RECOVERY_MAX = QCA_VENDOR_ATTR_FW_RECOVERY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_nfcal_power_event - Vendor attributes for the
 * %QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT vendor event.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_INVALID:
 *     Invalid attribute (placeholder).
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_HW_LINK_ID:
 *     Mandatory attribute (u8) representing the hardware link ID on which
 *     the noise floor calibration was performed.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBR:
 *     Mandatory attribute (NLA_BINARY) containing an array of s8 values
 *     representing the noise floor in dBr for each calibrated channel/chain.
 *     The number of valid entries is indicated by
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_NFDBR_DBM.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBM:
 *     Mandatory attribute (NLA_BINARY) containing an array of s8 values
 *     representing the noise floor in dBm for each calibrated channel/chain.
 *     The number of valid entries is indicated by
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_NFDBR_DBM.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_FREQNUM:
 *     Mandatory attribute (NLA_BINARY) containing an array of u32 values
 *     representing the frequency numbers (in MHz) for each calibrated channel.
 *     The number of valid entries is indicated by
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_FREQ.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_NFDBR_DBM:
 *     Mandatory attribute (u16) indicating the number of valid entries in the
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBR and
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBM arrays.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_FREQ:
 *     Mandatory attribute (u16) indicating the number of valid entries in the
 *     %QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_FREQNUM array.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_AFTER_LAST:
 *     Internal marker for the end of attributes.
 *
 * @QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_MAX:
 *     Maximum attribute index (for bounds checking).
 */
enum qca_wlan_vendor_attr_nfcal_power_event {
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_HW_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBR = 2,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NFDBM = 3,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_FREQNUM = 4,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_NFDBR_DBM = 5,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_NUM_FREQ = 6,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_MAX =
		QCA_WLAN_VENDOR_ATTR_NFCAL_POWER_AFTER_LAST - 1,
};

/**
 * ath12k_vendor_send_6ghz_power_mode_update_complete - Send 6 GHz power mode
 * update vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @wdev - Pointer to wdev
 * @link_id - Link ID for which the power mode update is complete
 */
int
ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						   struct wireless_dev *wdev,
						   u8 link_id);

enum qca_wlan_vendor_attr_sdwf_phy {
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_phy_oper {
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_SET = 0,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_DEL = 1,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_GET = 2,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_SAMPLES_SET = 3,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_BREACH_DETECTION_SET = 4,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_THRESHOLD_SET = 5,
};

enum qca_wlan_vendor_attr_sdwf_svc {
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT = 12,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY = 13,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE = 14,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE = 15,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE = 16,
	/* The below are used by MCC */
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BUFFER_LATENCY_TOLERANCE = 17,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_TRIGGER_DSCP = 18,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_REPLACE_DSCP = 19,

	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_offload_ops {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG = 9,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY = 10,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG = 11,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT = 12,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS = 13,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST - 1
};

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
enum qca_vendor_wlan_telemetry_txstats_mmesh {
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_NOQOS = 1,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_NOENC = 2,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_TXINFO = 3,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_AUTORATE = 4,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_TOFW = 5,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_DIRECT = 6,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_TX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rxstats_mmesh {
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXHDR_UPDT = 1,
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXFILTDROP = 2,
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXKEY_LOOKUP_FAIL = 3,
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXKEY_LOOKUP_SUCC = 4,
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXHDR_ALLOC_FAIL = 5,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_AFTER_LAST - 1,
};
#endif

enum qca_wlan_vendor_attr_atf_stats {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BE_AIRTIME = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BK_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VI_AIRTIME = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VO_AIRTIME = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BE_AIRTIME = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BK_AIRTIME = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VI_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VO_AIRTIME = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS = 9,

	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_peer_stats {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BE_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BK_AIRTIME = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VI_AIRTIME = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VO_AIRTIME = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BE_AIRTIME = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BK_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VI_AIRTIME = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VO_AIRTIME = 9,

	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_atf_offload_ssid_group_config -
 * Defines attributes to be used with vendor attribute
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX: Mandatory
 * u8 attribute. Indicates the unique index of the SSID group.
 * These indexes are assigned based on the order in which the SSID
 * groups are configured.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED:
 * Mandatory u16 attribute. Indicates the percentage of airtime
configured by
 * the user for the SSID group. The value is represented as a
fixed-point
 * integer with one digit after the decimal point.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY: Mandatory
 * u8 attribute. Indicates the scheduling policy of the SSID group.
 * Attribute value range is 0 to 2.
 * 0 - Fair scheduling, the SSID group can contribute its
 * unused airtime with other SSID groups and can also borrow from them.
 * 1 - Strict scheduling, the SSID group can contribute
 * its unused airtime with other SSID groups but cannot be borrowed.
 * 2 - Fair with upper bound, the SSID group can only contribute
 * its unused airtime, but cannot borrow airtime from other SSID groups.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS:
 * Mandatory u16 attribute. Indicates the number of peers that are
associated
 * but do not have airtime configured by user.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS:
 * Mandatory u16 attribute. Indicates the number of peers that are
 * associated and have airtime configured by user.
 * Note: The total number of associated peers to the SSID group is
 * the sum of configured and unconfigured peers.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME:
 * Mandatory u16 attribute. Indicates the percentage of total airtime
allocated
 * for unconfigured peers within the SSID group. The value is
represented as
 * a fixed-point integer with one digit after the decimal point.
 *
 */
enum qca_wlan_vendor_attr_atf_offload_ssid_group_config {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_LAST - 1,
};


/**
 * enum qca_wlan_vendor_attr_atf_offload_wmm_ac_config -
 * Defines attributes to be used with vendor attribute
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_GROUP_INDEX: Mandatory
 * u8 attribute. Indicates the unique index of the SSID group.
 * These indexes are assigned based on the order in which the SSID
 * groups are configured.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_BE_AIRTIME: Mandatory u16
 * attribute. Indicates the airtime percentage configured for the Best
 * Effort (BE) WMM Access Category of SSID group. The value is represented
 * as a fixed-point integer with one digit after the decimal point.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_BK_AIRTIME: Mandatory u16
 * attribute. Indicates the airtime percentage configured for the
 * Background (BK) WMM Access Category of SSID group. The value is represented
 * as a fixed-point integer with one digit after the decimal point.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_VI_AIRTIME: Mandatory u16
 * attribute. Indicates the airtime percentage configured for
 * the Video (VI) WMM Access Category of SSID group. The value is represented
 * as a fixed-point integer with one digit after the decimal point.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_VO_AIRTIME: Mandatory u16
 * attribute. Indicates the airtime percentage configured for the
 * Voice (VO) WMM Access Category of SSID group. The value is represented
 * as a fixed-point integer with one digit after the decimal point.
 *
 */
enum qca_wlan_vendor_attr_atf_offload_wmm_ac_config {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_NUM_WMM_AC_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_GROUP_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_BE_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_BK_AIRTIME = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_VI_AIRTIME = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_VO_AIRTIME = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_NUM_WMM_AC_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_NUM_WMM_AC_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_NUM_WMM_AC_LAST - 1, };

/**
 * enum qca_wlan_vendor_attr_atf_offload_peer_config - Defines
 * attributes to be used with vendor attribute
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE: Mandatory
 * NLA_FLAG attribute. When included, the configuration update applies to
 * all currently connected peers. If not include, the update applies only
 * to newly connected peers.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE: Mandatory
 * NLA_FLAG attribute. Indicates that the current
 * QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS command does not include
 * configuration data for all connected peers. When this flag is included,
 * it signals that additional command(s) will follow, each carrying the
 * remaining peer configurations using the
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD: Mandatory nested
 * attribute. Indicates each peer's configuration associated with the radio.
 * The attributes defined in enum qca_wlan_vendor_attr_atf_offload_peer are
 * nested in this attribute.
 */
enum qca_wlan_vendor_attr_atf_offload_peer_config {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_atf_offload_peer - Defines
 * attributes to be used with vendor attribute
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_PAYLOAD.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC: MAC address mandatory
 * attribute. Indicates the MAC address of the peer or link peer
 * in case of MLO.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME: Mandatory u16 attribute.
 * Indicates the percentage of airtime configured for the peer. The airtime
 * assigned to a peer is relative to the SSID group's total airtime allocation.
 * The value is represented as a fixed-point integer with one digit after the
 * decimal point.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX: Mandatory u8 attribute.
 * Indicates the index of the SSID group to which the peer belongs.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED: Mandatory NLA_FLAG
 * attribute. This flag is included if the peer has explicitly configured
 * airtime by user.
 */
enum qca_wlan_vendor_attr_atf_offload_peer {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_LAST =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX - 1,
};

/**
 * enum qca_wlan_vendor_attr_atf_offload_ssid_scheduling_policy -
 * Defines attributes to be used with vendor attribute
 * QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID: u8 attribute.
 * Mandatory attribute when interface is configured in
 * Multi Link Operation (MLO). This attribute must not be included
 * in non-MLO scenarios. This is the link ID of the interface in
 * the MLO case. Possible values are 0 to 14.
 *
 * @QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING: Mandatory u8 attribute.
 * An attribute used to define the scheduling policy.
 * The accepted values for the attribute are
 * 0 - fair, 1 - strict, 2 - fair with upper bound.
 *
 */
enum qca_wlan_vendor_attr_atf_offload_ssid_scheduling_policy {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_sched_duration {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_AC = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_sla_samples {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_sla_detect {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_sla_detect_param {
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_NUM_PACKET,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_PER_SECOND,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_MOV_AVG,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_NUM_SECOND,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_MAX,
};

enum qca_wlan_vendor_attr_sdwf_sla_threshold {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_rm_generic - Attributes required for vendor
 * command %QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC to register a Resource Manager
 * with the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP: Nested attribute used for commands
 * and events related to ErP (Energy related Products),
 * see @enum qca_wlan_vendor_attr_erp_ath for details.
 */
enum qca_wlan_vendor_attr_rm_generic {
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION = 1,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION = 2,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES = 3,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO = 4,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_TTLM_MAPPING = 5,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_PMLO = 6,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_LINK_BW_NSS_CHANGE = 7,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_DETSCHED = 8,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY = 9,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS = 10,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY = 11,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_TTLM_INFO = 12,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP = 13,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR = 14,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID = 15,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA = 16,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF = 17,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX =
		QCA_WLAN_VENDOR_ATTR_RM_GENERIC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_erp_ath - Parameters to support ErP in ath driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START: Flag, set to true will trigger
 * driver's entry into ErP mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE: Flag to indicate that ErP
 * parameter configuration is complete. This can be included along with flags
 * QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START and
 * QCA_WLAN_VENDOR_ATTR_ERP_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG: Optional nested attribute for ErP
 * parameters. Flag QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START must be sent
 * either before or when the first time this flag is included. See
 * @enum qca_wlan_vendor_attr_erp_ath_config for details.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_EXIT: Flag, set to true will trigger exit from
 * ErP mode. Driver uses this flag to send vendor event
 * %QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC to userspace upon receiving a packet
 * matching a previously configured filter. Userspace can also trigger driver's
 * exit from ErP using this flag.
 */
enum qca_wlan_vendor_attr_erp_ath {
	QCA_WLAN_VENDOR_ATTR_ERP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START = 1,
	QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE = 2,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_ERP_EXIT = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ERP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ERP_MAX = QCA_WLAN_VENDOR_ATTR_ERP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_trigger_types - Types of ErP wake up trigger
 */
enum qca_wlan_vendor_trigger_types {
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ARP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_NS_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IGMP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_MLD_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_EAP,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_erp_ath_config - Parameters to support ErP.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX: (u32) Interface index. This is
 * a mandatory attribute for setting packet trigger for the designated wake up
 * interface along with %QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER).
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER: (u32) Attribute used
 * to set wake-up trigger to bring the device out of ErP mode. This is bitmap
 * where each bit corresponds to the values defined in
 * enum qca_wlan_vendor_trigger_types.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE: flag, set if the driver should
 * remove PCIe slot.
 */
enum qca_wlan_vendor_attr_erp_ath_config {
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER = 2,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE = 3,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_feat_attr {
	QCA_VENDOR_ATTR_WLAN_FEAT_TX = 1,
	QCA_VENDOR_ATTR_WLAN_FEAT_RX,
	QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX,
	QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY,
	QCA_VENDOR_ATTR_WLAN_FEAT_PROTO,
	QCA_VENDOR_ATTR_WLAN_FEAT_TID,
	QCA_VENDOR_ATTR_WLAN_FEAT_DELAY,
	QCA_VENDOR_ATTR_WLAN_FEAT_JITTER,
	QCA_VENDOR_ATTR_WLAN_FEAT_SOJOURN,
	QCA_VENDOR_ATTR_WLAN_FEAT_MON_STATS,
	QCA_VENDOR_ATTR_WLAN_FEAT_TX_MON_STATS,

	QCA_VENDOR_ATTR_WLAN_FEAT_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_FEAT_MAX =
		QCA_VENDOR_ATTR_WLAN_FEAT_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_event_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_OBJECT_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFTX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFDELAY_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPEDS_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_VAP_CP_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_DELAY_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_JITTER_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SOJOURN_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_MON_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_MON_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST - 1,
};

enum qca_vendor_mgmt_stats {
	QCA_VENDOR_MGMT_STATS_ASSOC_REQ = 0,
	QCA_VENDOR_MGMT_STATS_ASSOC_RESP,
	QCA_VENDOR_MGMT_STATS_REASSOC_REQ,
	QCA_VENDOR_MGMT_STATS_REASSOC_RESP,
	QCA_VENDOR_MGMT_STATS_PROBE_REQ,
	QCA_VENDOR_MGMT_STATS_PROBE_RESP,
	QCA_VENDOR_MGMT_STATS_TIMING_ADV,
	QCA_VENDOR_MGMT_STATS_RESERVED,
	QCA_VENDOR_MGMT_STATS_BEACON,
	QCA_VENDOR_MGMT_STATS_ATIM,
	QCA_VENDOR_MGMT_STATS_DISASSOC,
	QCA_VENDOR_MGMT_STATS_AUTH,
	QCA_VENDOR_MGMT_STATS_DEAUTH,
	QCA_VENDOR_MGMT_STATS_ACTION,
	QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK,

	QCA_VENDOR_MGMT_STATS_AFTER_LAST,
	QCA_VENDOR_MGMT_STATS_MAX = QCA_VENDOR_MGMT_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_cp_stats_attr {
	QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID = 0,
	QCA_VENDOR_ATTR_TELEMETRY_CP_VDEV_ID,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_BEACON_COUNT,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_BEACON_OUTAGE_COUNT,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_PROBE_REQUEST,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_PROBE_RESPONSE,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_REQUEST_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_REQUEST_FAILURE,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_RESPONSE_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_RESPONSE_FAILURE,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_SENT_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_SENT_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_SENT_FAIL,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_SENT_FAIL,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_COMPLETION_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_COMPLETION_SUCCESS,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_COMPLETION_FAIL,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_COMPLETION_FAIL,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_FRAMES,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_SUCCESS_COUNT,
	QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_FAILURE_COUNT,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_BEACON_COUNT,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_PROBE_REQUEST_UCAST,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_PROBE_REQUEST_BCAST,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_NO_RATE_MATCH,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_BAD_WPAIE,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_CAP_MISMATCH,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ACTION,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ACTION_NO_ACK,
	QCA_VENDOR_ATTR_TELEMETRY_CP_RX_MGMT_FRAMES,
	QCA_VENDOR_ATTR_TELEMETRY_CP_AFTER_LAST,

	QCA_VENDOR_ATTR_TELEMETRY_CP_MAX =
		QCA_VENDOR_ATTR_TELEMETRY_CP_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_radio_cp_stats_attr - Attributes for
 * radio-level control path statistics, nested under
 * QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_EVENT.
 */
enum qca_vendor_wlan_telemetry_radio_cp_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_INVALID = 0,

	/* Radio CP TX stats */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_FAILED,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_RTS_SUCCESS,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_RTS_FAIL,
	/* Radio CP RX stats */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_MGMT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_CTRL,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_DECRYPT_ERR,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_MIC_ERR,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_OVER_RUN,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_CRC_ERR,
	/* NLA_NESTED: static (characterized/BDF) NF per chain from HTT PHY stats
	 * type 37 bdf_nf_chain[]. Each nested entry is an NLA_S32 indexed by
	 * chain number (1-based). Only valid chains (value != 1) are included.
	 */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_CHAN_NF_STATIC,
	/* NLA_S32: dynamic (runtime) NF — first valid runtime_nf_chain[] value
	 * from HTT PHY stats type 37, refreshed by triggering the survey NL
	 * command (same path as iw dev <iface> survey dump) before reading.
	 */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_CHAN_NF_DYNAMIC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_tid_stats_attr - Top-level TID stats container
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_TX_STATS: Nested array of per-TID TX stats
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_RX_STATS: Nested array of per-TID RX stats
 */
enum qca_vendor_wlan_telemetry_tid_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_TX_STATS,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_RX_STATS,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_tid_delay_attr - Sub-attributes for per-TID
 * TX delay histogram statistics. Used as nested attributes under
 * QCA_VENDOR_ATTR_TID_TX_SWQ_DELAY, QCA_VENDOR_ATTR_TID_TX_HWTX_DELAY, and
 * QCA_VENDOR_ATTR_TID_TX_INTFRM_DELAY.
 *
 * @QCA_VENDOR_ATTR_TID_DELAY_MAX_VAL: u32 - Maximum observed delay value
 * @QCA_VENDOR_ATTR_TID_DELAY_MIN_VAL: u32 - Minimum observed delay value
 * @QCA_VENDOR_ATTR_TID_DELAY_AVG_VAL: u32 - Average delay value
 * @QCA_VENDOR_ATTR_TID_DELAY_HIST: Nested u64 array - Histogram bucket
 *     frequencies indexed 1..HIST_BUCKET_MAX
 */
enum qca_vendor_wlan_telemetry_tid_delay_attr {
	QCA_VENDOR_ATTR_TID_DELAY_INVALID = 0,
	QCA_VENDOR_ATTR_TID_DELAY_MAX_VAL,
	QCA_VENDOR_ATTR_TID_DELAY_MIN_VAL,
	QCA_VENDOR_ATTR_TID_DELAY_AVG_VAL,
	QCA_VENDOR_ATTR_TID_DELAY_HIST,

	QCA_VENDOR_ATTR_TID_DELAY_AFTER_LAST,
	QCA_VENDOR_ATTR_TID_DELAY_MAX_ATTR =
		QCA_VENDOR_ATTR_TID_DELAY_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_tid_tx_attr - Per-TID TX statistics
 * @QCA_VENDOR_ATTR_TID_TX_TQM_STATUS_CNT: TQM release reason counters (nested u32 array)
 * @QCA_VENDOR_ATTR_TID_TX_HTT_STATUS_CNT: HTT completion counters (nested u32 array)
 * @QCA_VENDOR_ATTR_TID_TX_SW_DROP_CNT: Software drop reason counters (nested u32 array)
 * @QCA_VENDOR_ATTR_TID_TX_SWQ_DELAY: Software queue delay histogram (nested)
 * @QCA_VENDOR_ATTR_TID_TX_HWTX_DELAY: HW TX completion delay histogram (nested)
 * @QCA_VENDOR_ATTR_TID_TX_INTFRM_DELAY: Inter-frame delay histogram (nested)
 */
enum qca_vendor_wlan_telemetry_tid_tx_attr {
	QCA_VENDOR_ATTR_TID_TX_INVALID = 0,
	QCA_VENDOR_ATTR_TID_TX_TQM_STATUS_CNT,
	QCA_VENDOR_ATTR_TID_TX_HTT_STATUS_CNT,
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_CNT,
	QCA_VENDOR_ATTR_TID_TX_SWQ_DELAY,
	QCA_VENDOR_ATTR_TID_TX_HWTX_DELAY,
	QCA_VENDOR_ATTR_TID_TX_INTFRM_DELAY,

	QCA_VENDOR_ATTR_TID_TX_AFTER_LAST,
	QCA_VENDOR_ATTR_TID_TX_MAX =
		QCA_VENDOR_ATTR_TID_TX_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_tid_tx_sw_drop_attr - SW drop reason counters
 * for per-TID TX statistics. Used as nested attributes under
 * %QCA_VENDOR_ATTR_TID_TX_SW_DROP_CNT.
 *
 * @QCA_VENDOR_ATTR_TID_TX_SW_DROP_DESC_ERR: TX descriptor allocation error
 *     drop count. Corresponds to %DP_TID_TX_DESC_ERR.
 * @QCA_VENDOR_ATTR_TID_TX_SW_DROP_DMA_MAP_ERR: DMA mapping error drop count.
 *     Corresponds to %DP_TID_TX_DMA_MAP_ERR.
 * @QCA_VENDOR_ATTR_TID_TX_SW_DROP_HW_ENQUEUE: HW enqueue failure drop count.
 *     Corresponds to %DP_TID_TX_HW_ENQUEUE.
 */
enum qca_vendor_wlan_telemetry_tid_tx_sw_drop_attr {
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_INVALID = 0,
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_DESC_ERR,
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_DMA_MAP_ERR,
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_HW_ENQUEUE,

	QCA_VENDOR_ATTR_TID_TX_SW_DROP_AFTER_LAST,
	QCA_VENDOR_ATTR_TID_TX_SW_DROP_MAX =
		QCA_VENDOR_ATTR_TID_TX_SW_DROP_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_tid_rx_attr - Per-TID RX statistics
 * @QCA_VENDOR_ATTR_TID_RX_DELIVERED_TO_STACK: MSDUs delivered to stack
 * @QCA_VENDOR_ATTR_TID_RX_MSDU_CNT: Total MSDU count
 * @QCA_VENDOR_ATTR_TID_RX_MCAST_MSDU_CNT: Multicast MSDU count
 * @QCA_VENDOR_ATTR_TID_RX_BCAST_MSDU_CNT: Broadcast MSDU count
 * @QCA_VENDOR_ATTR_TID_RX_FAIL_CNT: RX failure counters per reason
 * @QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODE_INV: Unknown REO error code count
 * @QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODES: Per-REO-error-code counters
 * @QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODE_INV: Unknown RXDMA error code count
 * @QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODES: Per-RXDMA-error-code counters
 * @QCA_VENDOR_ATTR_TID_RX_TO_STACK_DELAY: To-stack delay histogram
 * @QCA_VENDOR_ATTR_TID_RX_INTFRM_DELAY: RX interframe delay histogram
 */
enum qca_vendor_wlan_telemetry_tid_rx_attr {
	QCA_VENDOR_ATTR_TID_RX_INVALID = 0,
	QCA_VENDOR_ATTR_TID_RX_DELIVERED_TO_STACK,
	QCA_VENDOR_ATTR_TID_RX_MSDU_CNT,
	QCA_VENDOR_ATTR_TID_RX_MCAST_MSDU_CNT,
	QCA_VENDOR_ATTR_TID_RX_BCAST_MSDU_CNT,
	QCA_VENDOR_ATTR_TID_RX_FAIL_CNT,
	QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODE_INV,
	QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODES,
	QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODE_INV,
	QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODES,
	QCA_VENDOR_ATTR_TID_RX_TO_STACK_DELAY,
	QCA_VENDOR_ATTR_TID_RX_INTFRM_DELAY,

	QCA_VENDOR_ATTR_TID_RX_AFTER_LAST,
	QCA_VENDOR_ATTR_TID_RX_MAX =
		QCA_VENDOR_ATTR_TID_RX_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_delay_stats_attr - Per-TID delay statistics
 *
 * These attributes are nested inside each TID entry within the
 * QCA_VENDOR_ATTR_WLAN_TELEMETRY_DELAY_EVENT vendor event.
 * One TID entry is emitted per TID (0..DP_TID_MAX-1), each wrapped in a
 * nested attribute indexed by (tid + 1).
 *
 * @QCA_VENDOR_ATTR_DELAY_STATS_TX_SWQ: Nested histogram (see
 *     qca_vendor_wlan_telemetry_delay_hist_attr). TX software-queue
 *     enqueue-to-dequeue delay histogram.
 * @QCA_VENDOR_ATTR_DELAY_STATS_TX_HW: Nested histogram. TX hardware
 *     (TCL enqueue to WBM completion) delay histogram.
 * @QCA_VENDOR_ATTR_DELAY_STATS_RX_TO_STACK: Nested histogram. RX
 *     REO-dequeue to network-stack delivery delay histogram.
 */
enum qca_vendor_wlan_telemetry_delay_stats_attr {
	QCA_VENDOR_ATTR_DELAY_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_DELAY_STATS_TX_SWQ = 1,
	QCA_VENDOR_ATTR_DELAY_STATS_TX_HW = 2,
	QCA_VENDOR_ATTR_DELAY_STATS_RX_TO_STACK = 3,

	QCA_VENDOR_ATTR_DELAY_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_DELAY_STATS_MAX =
		QCA_VENDOR_ATTR_DELAY_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_jitter_stats_attr {
	QCA_VENDOR_ATTR_JITTER_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_JITTER = 1,
	QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_DELAY = 2,
	QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_ERR = 3,
	QCA_VENDOR_ATTR_JITTER_STATS_TX_TOTAL_SUCCESS = 4,
	QCA_VENDOR_ATTR_JITTER_STATS_TX_DROP = 5,

	QCA_VENDOR_ATTR_JITTER_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_JITTER_STATS_MAX =
		QCA_VENDOR_ATTR_JITTER_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_sojourn_stats_attr {
	QCA_VENDOR_ATTR_SOJOURN_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_SOJOURN_STATS_SUM_SOJOURN_MSDU = 1,
	QCA_VENDOR_ATTR_SOJOURN_STATS_NUM_MSDUS = 2,
	QCA_VENDOR_ATTR_SOJOURN_STATS_AVG_SOJOURN_MSDU = 3,

	QCA_VENDOR_ATTR_SOJOURN_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_SOJOURN_STATS_MAX =
		QCA_VENDOR_ATTR_SOJOURN_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_mon_stats_attr {
	QCA_VENDOR_ATTR_TX_MON_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_REPLENISHED = 1,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_IN_REAP,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_IN_HARDWARE,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_FREE,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_REPLENISH_ERR,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_PROC_ERR,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_ALLOC_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_DMA_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_NUM_BUFS_REAPED,
	QCA_VENDOR_ATTR_TX_MON_STATS_TRUNCATED_BUF,
	QCA_VENDOR_ATTR_TX_MON_STATS_FLUSHED_BUF,
	QCA_VENDOR_ATTR_TX_MON_STATS_NULL_BUF,
	QCA_VENDOR_ATTR_TX_MON_STATS_MON_DESC_FREE,
	QCA_VENDOR_ATTR_TX_MON_STATS_PKT_BUF_NULL,
	QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_BUF_NULL,
	QCA_VENDOR_ATTR_TX_MON_STATS_PREP_WQ_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_EMPTY_DESCRIPTORS,
	QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_PROCESSED,
	QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_DESC_PROCESSED,
	QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_DESC_OVERFLOW,
	QCA_VENDOR_ATTR_TX_MON_STATS_ZERO_STATUS_DESC,
	QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_PREP_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_TLV_PROCESS_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_DATA_GEN_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_BUF_EXTRACT_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_MAGIC_VALUE_ERROR,
	QCA_VENDOR_ATTR_TX_MON_STATS_PKT_TLV_FREE,
	QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_BUF_FREE,
	QCA_VENDOR_ATTR_TX_MON_STATS_MU_USER_FRAME,
	QCA_VENDOR_ATTR_TX_MON_STATS_DATA_PPDU_DELIVERED,
	QCA_VENDOR_ATTR_TX_MON_STATS_PROT_PPDU_DELIVERED,
	QCA_VENDOR_ATTR_TX_MON_STATS_SELF_GEN_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_SKB_ALLOC_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_RING_EXTRACT_FAILED,
	QCA_VENDOR_ATTR_TX_MON_STATS_GET_NUM_USERS_FAILED,

	QCA_VENDOR_ATTR_TX_MON_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_MON_STATS_MAX =
		QCA_VENDOR_ATTR_TX_MON_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_WBM_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT,

	/* New RX monitor stats block - nested attributes */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_STATS,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_SCAN_STATS,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MMESH_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_mon_stats {
	QCA_VENDOR_ATTR_MON_STATS_INVALID = 0,

	QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_REAPED = 1,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_PROCESSED,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_FREE,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_ERROR_FREE,
	QCA_VENDOR_ATTR_MON_STATS_RING_DESC_EMPTY,
	QCA_VENDOR_ATTR_MON_STATS_RING_DESC_FLUSH,
	QCA_VENDOR_ATTR_MON_STATS_RING_DESC_TRUNC,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_PROCESSED,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_FREE,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_ERROR_FREE,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_TO_MAC80211,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_TRUNCATED,
	QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_REAPED,
	QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_ALLOC,
	QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_FREE,
	QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_TO_MAC80211,
	QCA_VENDOR_ATTR_MON_STATS_NUM_PPDU_REAPED,
	QCA_VENDOR_ATTR_MON_STATS_NUM_PPDU_PROCESSED,
	QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_RAW,
	QCA_VENDOR_ATTR_MON_STATS_NUM_FRAG_RAW,
	QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_ETH,
	QCA_VENDOR_ATTR_MON_STATS_NUM_FRAG_ETH,
	QCA_VENDOR_ATTR_MON_STATS_DROP_TLV,
	QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_USED,
	QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_PROC,
	QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_FREE,
	QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_FREE_LIST_EMPTY_CNT,
	QCA_VENDOR_ATTR_MON_STATS_RESTITCH_INSUFF_FRAGS_CNT,
	QCA_VENDOR_ATTR_MON_STATS_INVALID_STATUS_MAGIC_NUM,
	QCA_VENDOR_ATTR_MON_STATS_INVALID_PKT_MAGIC_NUM,
	QCA_VENDOR_ATTR_MON_STATS_NULL_MPDU_Q,
	QCA_VENDOR_ATTR_MON_STATS_SKB_ALLOC_FAIL,
	QCA_VENDOR_ATTR_MON_STATS_RX_HDR_NOT_RCVD,
	QCA_VENDOR_ATTR_MON_STATS_MIN_FRAGS_UNAVAILABLE,
	QCA_VENDOR_ATTR_MON_STATS_INVALID_MPDU_HDR_LEN,
	QCA_VENDOR_ATTR_MON_STATS_INVALID_IN_USE,
	QCA_VENDOR_ATTR_MON_STATS_INVALID_END_OFFSET,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_FRAG_ADD_TO_SKB,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_NUM_FRAG_REPLENISH,
	QCA_VENDOR_ATTR_MON_STATS_STATUS_NUM_FRAG_FREE,

	QCA_VENDOR_ATTR_MON_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_MON_STATS_MAX =
		QCA_VENDOR_ATTR_MON_STATS_AFTER_LAST - 1,
};

/* Nested attributes under QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_SIGNAL_STATS */
enum qca_vendor_wlan_attr_rx_mon_signal_stats {
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_AVG,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP_AVG,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_AVG,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP_AVG,

	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_MAX =
		QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_AFTER_LAST - 1
};

/**
 * enum qca_vendor_wlan_telemetry_rx_mon_stats - RX Monitor Statistics
 *
 * Nested attributes for per-peer RX monitor statistics.
 * These attributes mirror the structure of ath12k_rx_peer_stats.
 *
 * All counter attributes use NLA_U64 type for consistency and to prevent
 * overflow issues. Array attributes use NLA_NESTED type with indexed elements.
 */
enum qca_vendor_wlan_telemetry_rx_mon_stats {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_INVALID = 0,

	/* Basic counters - u64 values */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_OK = 2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_ERR = 3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCP_MSDU_COUNT = 4,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_UDP_MSDU_COUNT = 5,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_OTHER_MSDU_COUNT = 6,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_AMPDU_MSDU_COUNT = 7,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NON_AMPDU_MSDU_COUNT = 8,

	/* Feature flags - u64 counters */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_STBC_COUNT = 9,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_BEAMFORMED_COUNT = 10,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_DCM_COUNT = 11,

	/* Duration - u64 value */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_DURATION = 12,

	/* Array attributes - nested */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_CODING_COUNT = 13,  /* HAL_RX_SU_MU_CODING_MAX */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_COUNT = 14,     /* IEEE80211_NUM_TIDS + 1 */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PREAMBLE_COUNT = 15,/* HAL_RX_PREAMBLE_MAX */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RECEP_TYPE = 16,	   /* HAL_RX_RECEPTION_TYPE_MAX */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RU_ALLOC_COUNT = 17,/* HAL_RX_RU_ALLOC_TYPE_MAX */

	/* Rate statistics - nested structures */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKT_STATS = 18,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_BYTE_STATS = 19,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU_BYTES = 20,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU_RETRY_COUNT = 21,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU = 22,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_RETRY_COUNT = 23,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_PPDU = 24,
	/* Advance stats */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_BAR = 25,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_NDPA = 26,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_RECEPTION = 27,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_NSS = 28,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_TYPE = 29,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE = 30,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SU_PPDU_COUNT = 31,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PUNC_BW = 32,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MU = 33,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_LAST_RX_RATE = 34,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RND_AVG_RX_RATE = 35,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_AVG_RX_RATE = 36,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RATECODE = 37,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_SIGNAL_STATS = 38,

	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_rx_coding - RX Coding Type
 *
 * Nested attributes for coding_count array in RX monitor statistics.
 * Maps to hal_rx_su_mu_coding enum from hal.h.
 */
enum qca_vendor_wlan_telemetry_rx_coding {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_BCC = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_LDPC = 2,

	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_rx_tid - RX TID Values
 *
 * Nested attributes for tid_count array in RX monitor statistics.
 * Covers TIDs 0-15 plus non-QoS traffic (TID 16).
 */
enum qca_vendor_wlan_telemetry_rx_tid {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_0 = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_1 = 2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_2 = 3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_3 = 4,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_4 = 5,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_5 = 6,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_6 = 7,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_7 = 8,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_8 = 9,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_9 = 10,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_10 = 11,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_11 = 12,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_12 = 13,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_13 = 14,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_14 = 15,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_15 = 16,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_NON_QOS = 17,

	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_rx_preamble - RX Preamble Type
 *
 * Nested attributes for pream_cnt array in RX monitor statistics.
 * Maps to hal_rx_preamble enum from hal.h.
 */
enum qca_vendor_wlan_telemetry_rx_preamble {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11A = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11B = 2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11N = 3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11AC = 4,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11AX = 5,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11BA = 6,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11BE = 7,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11AZ = 8,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11N_GF = 9,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_11BN = 10,	/* UHR */

	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_rx_reception - RX Reception Type
 *
 * Nested attributes for reception_type array in RX monitor statistics.
 * Maps to hal_rx_reception_type enum from hal.h.
 */
enum qca_vendor_wlan_telemetry_rx_reception {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_SU = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MU_MIMO = 2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MU_OFDMA = 3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MU_OFDMA_MIMO = 4,

	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_wlan_telemetry_rx_pkt_type {
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_A = 1,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_B,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_N,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_AC,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_AX,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_BA,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_BE,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_AZ,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_N_GF,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_80211_BN,

	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX =
		QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_proto_stats_event {
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_TX,
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_RX,
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_VAP,
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_EVENT_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_EVENT_AFTER_LAST - 1,
};

enum qca_vendor_wlan_proto_stats_tx_enq {
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_RECV_FROM_STACK,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_RECV_FROM_STACK_FP,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_ENQUEUE_HW,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_ENQUEUE_HW_FP,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_AFTER_LAST - 1,
};

enum qca_vendor_wlan_proto_stats_tx_comp {
	QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_proto_stats_rx {
	QCA_VENDOR_ATTR_PROTO_STATS_RX_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_RX_RECV_FROM_HW,
	QCA_VENDOR_ATTR_PROTO_STATS_RX_SENT_TO_STACK,
	QCA_VENDOR_ATTR_PROTO_STATS_RX_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_RX_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_RX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_proto_stats_level_attr {
	QCA_VENDOR_ATTR_PROTO_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_L3,
	QCA_VENDOR_ATTR_PROTO_STATS_L4,
	QCA_VENDOR_ATTR_PROTO_STATS_L5,
	QCA_VENDOR_ATTR_PROTO_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_l3_proto_stats_attr {
	QCA_VENDOR_ATTR_PROTO_STATS_L3_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_ARP,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_IPV4,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_IPV6,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_M1,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_M2,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_M3,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_M4,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_G1,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_EAPOL_G2,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_NS,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX =
	QCA_VENDOR_ATTR_PROTO_STATS_L3_AFTER_LAST - 1,
};

enum qca_vendor_wlan_l4_proto_stats_attr {
	QCA_VENDOR_ATTR_PROTO_STATS_L4_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_TCP,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_UDP,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_ICMP,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_ICMP_REQ,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_ICMP_RSP,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_IGMP,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_NS,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_L4_AFTER_LAST - 1,
};

enum qca_vendor_wlan_l5_proto_stats_attr {
	QCA_VENDOR_ATTR_PROTO_STATS_L5_INVALID = 0,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DHCP,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DHCP_DIS,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DHCP_REQ,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DHCP_OFR,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DHCP_ACK,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DNS_QUERY,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_DNS_RSP,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_NS,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_AFTER_LAST,
	QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX =
		QCA_VENDOR_ATTR_PROTO_STATS_L5_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_ppeds_stats_attr {
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TCL_PROD_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TCL_CONS_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_REO_PROD_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_REO_CONS_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_GET_TX_DESC_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_ALLOCATED,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_ALLOC_FAILS,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_FREED,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_FW2WBM_PKT_DROPS,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_ENABLE_INTR_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_DISABLE_INTR_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_RELEASE_TX_SINGLE_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_RELEASE_RX_DESC_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_NUM_RX_DESC_FREED,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_NUM_RX_DESC_REALLOC,
	QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TQM_REL_REASON,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPEDS_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPEDS_STATS_MAX_EVENT =
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPEDS_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PPDU_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_MMESH_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_MAX_EVENT =
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_stats_attr {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_UCAST_SUCC,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PPDUS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDUS_SUCCESS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDUS_TRIED,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RETRIES_MPDU,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LAST_ACK_RSSI,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AVG_ACK_RSSI,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RSSI_CHAIN,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_RATE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PKT_TYPE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_GI_COUNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NSS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_BW,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_START,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_TONES,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_GROUP,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_STBC,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LDPC,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_WME_AC_TYPE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_WME_AC_BYTES,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_EXCESS_RETRY_AC,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AMPDU_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NON_AMPDU_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NUM_PPDU_COOKIE_VALID,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AVG_RATE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_RATECODE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LAST_RATE_MCS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MCAST_LAST_TX_RATE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MCAST_LAST_TX_RATE_MCS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PREAM_PUNCT_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_MPDU_SUC_TRD,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDU_SUC_TRD,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_SU_BE_PPDU_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_BE_PPDU_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_SU_BN_PPDU_CNT, /* UHR (11BN) */
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_BN_PPDU_CNT, /* UHR (11BN) */
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PUNC_BW,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RTS_SUCCESS,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RTS_FAILURE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_BAR_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NDPA_CNT,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PPDU_DURATION,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PWR,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MSDU_FLUSH_RSN,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_ATTR_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_ATTR_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_ATTR_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_pkt_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_BYTES,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_PKT,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PKT_INFO_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_tx_pkt_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_NUM_MPDU,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_MPDU_TRD,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_AFTER_LAST - 1,
};
enum qca_vendor_wlan_telemetry_mcs_info {
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_0,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_1,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_2,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_3,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_4,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_5,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_6,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_7,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_8,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_9,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_10,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_11,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_12,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_13,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_14,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_15,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_16,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_17,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_18,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_19,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_20,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_21,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_22,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_23,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_24,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_25,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_26,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_27,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_28,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_29,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_30,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_31,

	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_MCS_IDX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_bw_info {
	QCA_VENDOR_WLAN_TELEMETRY_BW_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_BW_20_MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_BW_40_MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_BW_80_MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_BW_160_MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_BW_240_MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_BW_320_MHZ,

	QCA_VENDOR_WLAN_TELEMETRY_BW_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_BW_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_BW_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_nss_info {
	QCA_VENDOR_WLAN_TELEMETRY_NSS_INVALIS = 0,
	QCA_VENDOR_WLAN_TELEMETRY_NSS_0,
	QCA_VENDOR_WLAN_TELEMETRY_NSS_1,
	QCA_VENDOR_WLAN_TELEMETRY_NSS_2,
	QCA_VENDOR_WLAN_TELEMETRY_NSS_3,

	QCA_VENDOR_WLAN_TELEMETRY_NSS_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_NSS_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_NSS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_gi_info {
	QCA_VENDOR_WLAN_TELEMETRY_GI_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_GI_NUM_0,
	QCA_VENDOR_WLAN_TELEMETRY_GI_NUM_1,
	QCA_VENDOR_WLAN_TELEMETRY_GI_NUM_2,
	QCA_VENDOR_WLAN_TELEMETRY_GI_NUM_3,

	QCA_VENDOR_WLAN_TELEMETRY_GI_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_GI_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_GI_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_ppdu_ru_alloc_type_info {
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_INVALIS = 0,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_26,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_96,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_106,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_242,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_484,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996x2,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996x4,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_52_26,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_106_26,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_484_242,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996_484,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996_484_242,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996x2_484,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996x3,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_RU_996x3_484,

	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_transmit_type_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_SU,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_MU_MIMO,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_MU_OFDMA,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_MU_MIMO_OFMDA,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_UL_TRIG,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_BURST_BCN,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_UL_BSRP_RESP,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_UL_BSRP_TRIG,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_UL_RESP,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_mu_grp_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_1,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_2,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_3,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_4,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_5,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_6,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_7,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_8,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_9,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_10,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_11,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_12,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_13,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_14,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_15,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_16,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_17,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_18,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_19,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_20,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_21,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_22,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_23,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_24,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_25,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_26,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_27,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_28,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_29,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_30,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_31,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_32,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_33,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_34,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_35,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_36,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_37,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_38,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_39,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_40,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_41,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_42,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_43,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_44,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_45,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_46,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_47,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_48,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_49,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_50,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_51,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_52,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_53,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_54,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_55,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_56,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_57,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_58,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_59,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_60,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_61,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_62,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_63,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_wme_ac_type_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_BE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_BK,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_VI,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_VO,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_punc_bw_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_NO_PUNCTURE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_20,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_40,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_80,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_120,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_txrx_mu_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_MIMO,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_OFDMA,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_htt_flush_info {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_PEER_DELETE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_TID_DELETE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_TTL_EXCEEDED,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_EXCESS_RETRIES,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_REINJECT,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ppdu_dot11_type {
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_A,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_B,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_N,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_AC,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_AX,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_BA,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_BE,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_AZ,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_N_GF,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_BN,

	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_attr {
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_ADDR_0 = 1,
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_INVALID,
	QCA_VENDOR_ATTR_REO_ERR_AMPDU_IN_NON_BA,
	QCA_VENDOR_ATTR_REO_ERR_NON_BA_DUPLICATE,
	QCA_VENDOR_ATTR_REO_ERR_BA_DUPLICATE,
	QCA_VENDOR_ATTR_REO_ERR_REGULAR_FRAME_2K_JUMP,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_2K_JUMP,
	QCA_VENDOR_ATTR_REO_ERR_REGULAR_FRAME_OOR,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_OOR,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_NO_BA_SESSION,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_SN_EQUALS_SSN,
	QCA_VENDOR_ATTR_REO_ERR_PN_CHECK_FAILED,
	QCA_VENDOR_ATTR_REO_ERR_2K_ERROR_HANDLING_FLAG_SET,
	QCA_VENDOR_ATTR_REO_ERR_PN_ERROR_HANDLING_FLAG_SET,
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_BLOCKED_SET,

	QCA_VENDOR_ATTR_REO_ERR_AFTER_LAST,
	QCA_VENDOR_ATTR_REO_ERR_MAX =
		QCA_VENDOR_ATTR_REO_ERR_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rxdma_attr {
	QCA_VENDOR_ATTR_RXDMA_ERR_OVERFLOW = 1,
	QCA_VENDOR_ATTR_RXDMA_ERR_MPDU_LENGTH,
	QCA_VENDOR_ATTR_RXDMA_ERR_FCS,
	QCA_VENDOR_ATTR_RXDMA_ERR_DECRYPT,
	QCA_VENDOR_ATTR_RXDMA_ERR_TKIP_MIC,
	QCA_VENDOR_ATTR_RXDMA_ERR_UNENCRYPTED,
	QCA_VENDOR_ATTR_RXDMA_ERR_MSDU_LEN,
	QCA_VENDOR_ATTR_RXDMA_ERR_MSDU_LIMIT,
	QCA_VENDOR_ATTR_RXDMA_ERR_WIFI_PARSE,
	QCA_VENDOR_ATTR_RXDMA_ERR_AMSDU_PARSE,
	QCA_VENDOR_ATTR_RXDMA_ERR_SA_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_DA_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_FLOW_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_FLUSH_REQUEST,
	QCA_VENDOR_ATTR_RXDMA_AMSDU_FRAGMENT,
	QCA_VENDOR_ATTR_RXDMA_MULTICAST_ECHO,
	QCA_VENDOR_ATTR_RXDMA_AMSDU_ADDR_MISMATCH,
	QCA_VENDOR_ATTR_RXDMA_UNAUTH_WDS,
	QCA_VENDOR_ATTR_RXDMA_GROUPCAST_AMSDU_OR_WDS,
	QCA_VENDOR_ATTR_RXDMA_CFP_MIC_ERR,
	QCA_VENDOR_ATTR_RXDMA_CFP_PN_CHK_ERR,

	QCA_VENDOR_ATTR_RXDMA_AFTER_LAST,
	QCA_VENDOR_ATTR_RXDMA_ERR_MAX =
		QCA_VENDOR_ATTR_RXDMA_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rx_wbm_sw_drop_reason {
	QCA_VENDOR_ATTR_WBM_SW_DROP_GET_SW_DESC_ERROR = 1,
	QCA_VENDOR_ATTR_WBM_SW_DROP_GET_SW_DESC_FROM_CK_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_PEER_ID_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_DESC_PARSE_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_COOKIE,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_PUSH_REASON,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_HW_ID,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_PARTNER_DP,
	QCA_VENDOR_ATTR_WBM_SW_DROP_PROCESS_NULL_PARTNER_DP,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_PDEV,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_AR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_CAC_RUNNING,
	QCA_VENDOR_ATTR_WBM_SW_DROP_SCATTER_GATHER,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_NWIFI_HDR_LEN,
	QCA_VENDOR_ATTR_WBM_SW_DROP_REO_GENERIC,
	QCA_VENDOR_ATTR_WBM_SW_DROP_RXDMA_GENERIC,

	QCA_VENDOR_ATTR_WBM_SW_DROP_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_SW_DROP_MAX =
		QCA_VENDOR_ATTR_WBM_SW_DROP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_sw_drop_reason {
	QCA_VENDOR_ATTR_REO_SUCCESS  = 1,
	QCA_VENDOR_ATTR_REO_SW_DROP_MISCELLANEOUS,
	QCA_VENDOR_ATTR_REO_SW_DROP_GET_SW_DESC_FROM_CK_ERROR,
	QCA_VENDOR_ATTR_REO_SW_DROP_GET_SW_DESC_ERROR,
	QCA_VENDOR_ATTR_REO_SW_DROP_REPLENISH,
	QCA_VENDOR_ATTR_REO_SW_DROP_PARTNER_DP_NA,
	QCA_VENDOR_ATTR_REO_SW_DROP_PDEV_NA,
	QCA_VENDOR_ATTR_REO_SW_DROP_LAST_MSDU_NOT_FOUND,
	QCA_VENDOR_ATTR_REO_SW_DROP_NWIFI_HDR_LEN_INVALID,
	QCA_VENDOR_ATTR_REO_SW_DROP_INVALID_MSDU_LEN,
	QCA_VENDOR_ATTR_REO_SW_DROP_MSDU_COALESCE_FAIL,
	QCA_VENDOR_ATTR_REO_SW_DROP_MPDU,
	QCA_VENDOR_ATTR_REO_SW_DROP_PPDU,
	QCA_VENDOR_ATTR_REO_SW_DROP_INVALID_PEER,

	QCA_VENDOR_ATTR_REO_SW_DROP_AFTER_LAST,
	QCA_VENDOR_ATTR_REO_SW_DROP_MAX =
		QCA_VENDOR_ATTR_REO_SW_DROP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tcl_ring_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_4,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_ring_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_4,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_comp_err_types_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_MISC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_DESC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PDEV,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_VIF,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PEER,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_LINK_PEER,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_DESC_INUSE,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST - 1
};

/**
 * enum qca_vendor_wlan_telemetry_rate_stats_type - Generic rate statistics types
 *
 * These attributes describe transmission rate characteristics and are used by
 * both TX HTT statistics and RX monitor statistics. They represent the type
 * of rate data being reported (e.g., HT, VHT, HE, bandwidth, NSS, etc.).
 *
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_LEGACY_CNT: Legacy rate counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HT_CNT: HT MCS counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_VHT_CNT: VHT MCS counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HE_CNT: HE MCS counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_EHT_CNT: EHT/BE MCS counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_BW_CNT: Bandwidth counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_NSS_CNT: NSS counts
 * @QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_GI_CNT: Guard interval counts
 * @QCA_VENDOR_ATTR_RX_RATE_STATS_RX_RATE: 4D rate array [BW][GI][NSS][MCS]
 */
enum qca_vendor_wlan_telemetry_rate_stats_type {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_LEGACY_CNT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HT_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_VHT_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HE_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_EHT_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_AZ_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_N_GF_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_BN_CNT,	/* UHR (11BN) MCS counts */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_BW_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_NSS_CNT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_GI_CNT,
	QCA_VENDOR_ATTR_RX_RATE_STATS_RX_MON_RATE,
	/* keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_TYPE_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rate_wme_ac_type {
	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_BE = 1,
	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_BK = 2,
	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_VI = 3,
	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_VO = 4,

	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_mu_user_pkt_type {
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MU_MIMO = 1,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MU_OFDMA = 2,

	QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_AFTER_LAST - 1,
};

/**
 ** enum qca_vendor_wlan_telemetry_rx_mu - rx mu counters
 **/
enum qca_vendor_wlan_telemetry_attr_rx_mu {
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_OK,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_ERR,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_PPDU_NSS,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MCS_COUNTS,

	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_AFTER_LAST - 1
};

/**
 ** enum qca_vendor_wlan_telemetry_punc_bw_modes - Puncture bw modes
 **/
enum qca_vendor_wlan_telemetry_punc_bw_modes {
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNCTURE_INVALID = 0,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_NO_PUNCTURE,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNCTURED_20MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNCTURED_40MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNCTURED_80MHZ,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNCTURED_120MHZ,

	/* keep last */
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNC_BW_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNC_BW_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNC_BW_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_telemetry_rx_nss - NSS values
 *
 * Attributes for spatial stream values
 */
enum qca_vendor_wlan_telemetry_nss {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_1 = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_2 = 2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_3 = 3,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_4 = 4,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_5,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_6,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_7,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_8,
	/* Keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_AFTER_LAST - 1,
};

/**
 * Alias for BE (802.11be final standard name) to EHT (draft name).
 * Both refer to the same technology.
 */
#define QCA_VENDOR_ATTR_RATE_STATS_BE_CNT \
	QCA_VENDOR_ATTR_RATE_STATS_EHT_CNT

enum qca_vendor_wlan_telemetry_pkt_info {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,

	/* keep last */
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_per_pkt_stats_rx_attr {
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO = 1,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_GCAST_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_GCAST_PKTS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED_MPDU_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED_MPDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP1_UCAST_PKTS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_PPDU_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUM_RSSI,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUM_PHY_RATE,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_UCAST_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP2_UCAST_PKTS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_GCAST_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_GCAST_PKTS,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_STATS_RX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_stats_tx {
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT = 1,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_ACKED_PPDU_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_SUM_ACK_RSSI,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_SUM_PHY_RATE,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP_BYTES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP1_PKTS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP2_PKTS,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_htt_tx_comp_status {
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_OK = 1,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_DROP,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_TTL,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_REINJ,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_INSPECT,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH,

	/* keep last */
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX =
		QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_wbm_tqm_rel_reason {
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_FRAME_ACKED = 1,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_TX,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_THRESHOLD,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MULTICAST_DROP,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_VDEV_MISMATCH_DROP,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_GEN_CMD_USED_TREE_EXT,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_DROP_FROM_PEER_CCE_OR_FLOW_TABLE,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_MULTICAST_REINJECT_FOR_VDEV,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_MEC_SEARCH_FAIL_FOR_VDEV,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_ASE_SEARCH_FAIL,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_SMD_ROAMING_DROP,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_STRIP_VLAN_TCI_MISMATCH_DROP,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_MEC_KEEP_ALIVE_FOR_VDEV,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_RESERVED_DROP_REASON1,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TCL_RESERVED_DROP_REASON2,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TQM_REM_MSDU_SMD_ROAMING,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TQM_REM_MPDU_SMD_ROAMING,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TQM_RESERVED_DROP_REASON3,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_TQM_RESERVED_DROP_REASON4,

	/* keep last */
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX =
		QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wbm_tqm_rel_reason_ext - wifi8 HW TX reasons
 * for pdev TID stats. Only populated when HW peer telemetry (wifi8) is active.
 *
 * @QCA_VENDOR_ATTR_TASC_REASON_HW_COMPLETION: HW TX completion (acked)
 * @QCA_VENDOR_ATTR_TASC_REASON_HW_DROP1: HW TX drop reason 1
 * @QCA_VENDOR_ATTR_TASC_REASON_HW_DROP2: HW TX drop reason 2
 * @QCA_VENDOR_ATTR_TASC_REASON_HW_FAILED: HW TX failed
 * @QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX_EXT: total size including HW stats
 */
enum qca_vendor_wbm_tqm_rel_reason_ext {
	QCA_VENDOR_ATTR_TASC_REASON_HW_COMPLETION =
		QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST,
	QCA_VENDOR_ATTR_TASC_REASON_HW_DROP1,
	QCA_VENDOR_ATTR_TASC_REASON_HW_DROP2,
	QCA_VENDOR_ATTR_TASC_REASON_HW_FAILED,

	/* keep last */
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST_EXT,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX_EXT =
		QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST_EXT - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_stats {
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_SG_PKT,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_SG_DMA_MAP_ERR,

	QCA_VENDOR_ATTR_TX_INGRESS_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_MAX =
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_encap_type {
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_RAW = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_NATIVE_WIFI,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_ETHERNET,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_802_3,

	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_encrypt_type {
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_40 = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_104,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_TKIP_NO_MIC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_TKIP_MIC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WAPI,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_CCMP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_OPEN,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_CCMP_256,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_GCMP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AES_GCMP_256,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WAPI_GCM_SM4,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_desc_type {
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_BUFFER = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_EXT_DESC,

	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_enq_error {
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_SUCCESS = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MISC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_VIF_TYPE_MON,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_LINK,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ARVIF,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MGMT_FRAME,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MAX_TX_LIMIT,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PDEV,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PEER,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_CRASH_FLUSH,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_NON_DATA_FRAME,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_SW_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_ENCAP_RAW,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_ENCAP_802_3,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_DMA_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_EXT_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_HTT_MDATA_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_TCL_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_TCL_DESC_RETRY,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ARVIF_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PDEV_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MAX_TX_LIMIT_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ENCAP_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_BRIDGE_VDEV,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_ARSTA_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_CLONE,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MHDR_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_FEAT_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_QUEUE_STOP,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_HW_ENQ_FAIL,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MCBC_ENCRY_FAIL,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MCBC_MSDU_INFO,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MCAST_NO_LINK,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_FW_RECOVERY,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_SKB_NO_LINEAR,

	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_ERR_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENQ_AFTER_LAST - 1,
};

/* TODO: qca_wlan_genric_data, qca_wlan_set_params,
 * qca_wlan_get_params
 * These should be align with qca_wlan_vendor_attr_config
 * in qca-vendor.h
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
 *
 * It requires qca_nl80211_lib changes also in reading
 * responses
 */
enum qca_wlan_genric_data {
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
	QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
	QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST,
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST - 1
};

enum qca_wlan_vendor_attr_iface_reload {
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_LINKID = 1,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_set_wifi - Attributes used with radio parameter
 *	set operation events.
 *
 * These attributes can be used in event notification
 * contexts for vendor operations that set a radio parameter.
 *
 * @QCA_WLAN_VENDOR_ATTR_SET_RADIO_PARAM: u32 attribute specifying the exact
 *	radio parameter/command being configured.
 * @QCA_WLAN_VENDOR_ATTR_SET_RADIO_VALUE: u32 attribute providing the value
 *	updated for %QCA_WLAN_VENDOR_ATTR_SET_RADIO_PARAM.
 * @QCA_WLAN_VENDOR_ATTR_SET_RADIO_STATUS: u32 attribute providing the status
 *	of the operation (e.g. success/failure) in the response/event.
 */
enum qca_wlan_vendor_attr_set_wifi {
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_PARAM = 0,
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_VALUE = 1,
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_STATUS = 2,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_MAX =
	QCA_WLAN_VENDOR_ATTR_SET_RADIO_AFTER_LAST - 1,
};

enum qca_vendor_vdev_param {
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_VDEV_PARAM_TEST,
	QCA_WLAN_VENDOR_VDEV_PARAM_DYN_BW_RTS = 1,
	QCA_WLAN_VENDOR_VDEV_PARAM_CWM_ENABLE = 2,
	QCA_WLAN_VENDOR_VDEV_PARAM_RATE_DROPDOWN = 3,
	QCA_WLAN_VENDOR_VDEV_PARAM_CTSPROT_DTIM_BCN = 4,
	QCA_WLAN_VENDOR_VDEV_PARAM_CABQ_MAXDUR = 5,
	QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RC_STALE_PERIOD = 6,
	QCA_WLAN_VENDOR_VDEV_PARAM_ENABLE_MCAST_RC = 7,
	QCA_WLAN_VENDOR_VDEV_PARAM_RC_NUM_RETRIES = 8,
	QCA_WLAN_VENDOR_VDEV_PARAM_DISABLE_CABQ = 9,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_SOUNDING_MODE = 10,
	QCA_WLAN_VENDOR_VDEV_PARAM_MAX_MTU_SIZE = 11,
	QCA_WLAN_VENDOR_VDEV_PARAM_GTX_ENABLE = 12,
	QCA_WLAN_VENDOR_VDEV_PARAM_HWCTS2SELF_OFDMA = 13,
	QCA_WLAN_VENDOR_VDEV_PARAM_VDEV_TSF = 14,
	QCA_WLAN_VENDOR_VDEV_PARAM_BCN_TX_POWER = 15,
	QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MIN_THRESH = 16,
	QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_MAX_THRESH = 17,
	QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MIN_THRESH = 18,
	QCA_WLAN_VENDOR_VDEV_PARAM_ACKRSSI_MAX_THRESH = 19,
	QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MIN_THRESH = 20,
	QCA_WLAN_VENDOR_VDEV_PARAM_TXRATE_MAX_THRESH = 21,
	QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MIN_THRESH = 22,
	QCA_WLAN_VENDOR_VDEV_PARAM_RXRATE_MAX_THRESH = 23,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_RSSI_RATE_THRESHOLDS = 24,
	QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_RATE_BREACH_MASK = 25,
	QCA_WLAN_VENDOR_VDEV_PARAM_AMPDU = 26,
	QCA_WLAN_VENDOR_VDEV_PARAM_AMSDU = 27,
	QCA_WLAN_VENDOR_VDEV_PARAM_BA_BUFSIZE = 28,
	QCA_WLAN_VENDOR_VDEV_PARAM_TX_ENCAP_TYPE = 29,
	QCA_WLAN_VENDOR_VDEV_PARAM_RX_DECAP_TYPE = 30,
	QCA_WLAN_VENDOR_VDEV_PARAM_ME = 31,
	QCA_WLAN_VENDOR_VDEV_PARAM_IGMP_ME = 32,
	QCA_WLAN_VENDOR_VDEV_PARAM_ME_GRP_LIMIT = 33,
	QCA_WLAN_VENDOR_VDEV_PARAM_MCAST_RATE = 34,
	QCA_WLAN_VENDOR_VDEV_PARAM_BCAST_RATE = 35,
	QCA_WLAN_VENDOR_VDEV_PARAM_RSSI_HYSTERESIS = 36,
	QCA_WLAN_VENDOR_VDEV_PARAM_RATE_HYSTERESIS = 37,
	QCA_WLAN_VENDOR_VDEV_PARAM_PN_MGMT_RX_FILTER = 38,
	QCA_WLAN_VENDOR_VDEV_MESH_MODE_HDR = 39,
	QCA_WLAN_VENDOR_VDEV_MESH_MODE_DBG = 40,
	QCA_WLAN_VENDOR_VDEV_RX_FILTER = 41,
	QCA_WLAN_VENDOR_VDEV_TX_MESH = 42,
	QCA_WLAN_VENDOR_VDEV_PARAM_PROTECTION_MODE = 43,
	QCA_WLAN_VENDOR_VDEV_PARAM_BW_NSS_RATE = 44,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LDPC = 45,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_LTF = 46,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_NSS = 47,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_PPDU_BW = 48,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_SHORTGI = 49,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_STBC = 50,
	QCA_WLAN_VENDOR_VDEV_PARAM_ENABLERTSCTS = 51,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_DCM = 52,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_EXTRANGE = 53,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LDPC = 54,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_NSS = 55,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_PPDU_BW = 56,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_SHORTGI = 57,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_STBC = 58,
	QCA_WLAN_VENDOR_VDEV_PARAM_RTSCTS_RATE = 59,
	QCA_WLAN_VENDOR_VDEV_PARAM_VHT_SGIMASK = 60,
	QCA_WLAN_VENDOR_VDEV_PARAM_VHT80_RATE = 61,
	QCA_WLAN_VENDOR_VDEV_PARAM_DIS_LPI_ANT_OPTIMIZE = 62,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_MINTXPOWER = 63,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_MAXTXPOWER = 64,
	QCA_WLAN_VENDOR_VDEV_PARAM_REGTXPOWER = 65,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_TXPOWER_RESOLUTION = 66,
	QCA_WLAN_VENDOR_VDEV_PARAM_ENABLE_RX_AMSDU = 67,
	QCA_WLAN_VENDOR_VDEV_PARAM_DISABLE_RX_AMSDU = 68,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_RX_AMSDU_BITMAP = 69,
	QCA_WLAN_VENDOR_VDEV_PARAM_SUPPORTED_BANDS = 70,
	QCA_WLAN_VENDOR_VDEV_PARAM_LIST_CHAN = 71,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_MAXRATE = 72,
	QCA_WLAN_VENDOR_VDEV_PARAM_NSS = 73,
	QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_RATE = 74,
	QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_VHT_MCS = 75,
	QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_HE_MCS = 76,
	QCA_WLAN_VENDOR_VDEV_PARAM_FIXED_EHT_MCS = 77,
	QCA_WLAN_VENDOR_VDEV_PARAM_UL_FIXED_RATE = 78,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_FIXED_RATE = 79,
	QCA_WLAN_VENDOR_VDEV_PARAM_CHWIDTH = 80,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_LTF = 81,
	/* Per-VAP EHT OFDMA/TXBF config (value: 0=disable, 1=enable) */
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA_TXBF = 82,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_DL_OFDMA = 83,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_OFDMA = 84,
	/* Per-VAP HE OFDMA/TXBF config (value: 0=disable, 1=enable) */
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA_TXBF = 85,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_DL_OFDMA = 86,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_OFDMA = 87,
	QCA_WLAN_VENDOR_VDEV_PARAM_TLV_LOGGER_MODE = 88,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_LTF = 89,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_LTF = 90,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_GI_LTF = 91,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_AR_LDPC = 92,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_RTSTHRSHLD = 93,
	QCA_WLAN_VENDOR_VDEV_PARAM_HE_UL_MCS = 94,
	QCA_WLAN_VENDOR_VDEV_PARAM_EHT_UL_MCS = 95,
	QCA_WLAN_VENDOR_VDEV_PARAM_GET_MONITOR_VERSION = 96,

	/* Add new params above */
	QCA_WLAN_VENDOR_VDEV_PARAM_LAST,
	QCA_WLAN_VENDOR_VDEV_PARAM_MAX = QCA_WLAN_VENDOR_VDEV_PARAM_LAST - 1,
};

enum qca_vendor_radio_param {
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_RADIO_PARAM_TEST,
	QCA_WLAN_VENDOR_RADIO_PARAM_MGMT_RETRY_LIMIT = 1,
	QCA_WLAN_VENDOR_RADIO_PARAM_RTS_CTS_RATE = 2,
	QCA_WLAN_VENDOR_RADIO_PARAM_PS_STATE_CHANGE = 3,
	QCA_WLAN_VENDOR_RADIO_PARAM_NON_AGG_SW_RETRY_TH = 4,
	QCA_WLAN_VENDOR_RADIO_PARAM_AGG_SW_RETRY_TH = 5,
	QCA_WLAN_VENDOR_RADIO_PARAM_STA_KICKOUT_TH = 6,
	QCA_WLAN_VENDOR_RADIO_PARAM_ARPDHCP_AC_OVERRIDE = 7,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANI_ENABLE = 8,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANI_POLL_PERIOD = 9,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANI_LISTEN_PERIOD = 10,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANI_OFDM_LEVEL = 11,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANI_CCK_LEVEL = 12,
	QCA_WLAN_VENDOR_RADIO_PARAM_CCA_THRESHOLD = 13,
	QCA_WLAN_VENDOR_RADIO_PARAM_DYN_TX_CHAINMASK = 14,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_ENABLE = 15,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_BE = 16,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_BK = 17,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_VI = 18,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_VO = 19,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_AC_LATENCY_TIMEOUT = 20,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_TX_ACTIVITY_TIMEOUT = 21,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_SLEEP_OVERRIDE = 22,
	QCA_WLAN_VENDOR_RADIO_PARAM_LTR_RX_OVERRIDE = 23,
	QCA_WLAN_VENDOR_RADIO_PARAM_L1SS_ENABLE = 24,
	QCA_WLAN_VENDOR_RADIO_PARAM_DSLEEP_ENABLE = 25,
	QCA_WLAN_VENDOR_RADIO_PARAM_SENS_LEVEL = 26,
	QCA_WLAN_VENDOR_RADIO_PARAM_DYN_GROUPING = 27,
	QCA_WLAN_VENDOR_RADIO_PARAM_DPD_ENABLE = 28,
	QCA_WLAN_VENDOR_RADIO_PARAM_BURST_DUR = 29,
	QCA_WLAN_VENDOR_RADIO_PARAM_BURST_ENABLE = 30,
	QCA_WLAN_VENDOR_RADIO_PARAM_DISABLE_LPI_ANT = 31,
	QCA_WLAN_VENDOR_RADIO_PARAM_EN_PROBE_ALL_BW = 32,
	QCA_WLAN_VENDOR_RADIO_PARAM_UL_OFDMA_RTD = 33,
	QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_SMALL_MRU = 34,
	QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_LARGE_MRU = 35,
	QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_RESET = 36,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_HW_MODE_CMDID = 37,
	QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT2G = 38,
	QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_LIMIT5G = 39,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_2G = 40,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_5G = 41,
	QCA_WLAN_VENDOR_RADIO_PARAM_OFDM_LEVEL = 42,
	QCA_WLAN_VENDOR_RADIO_PARAM_TXPOWER_SCALE = 43,
	QCA_WLAN_VENDOR_RADIO_PARAM_RX_FILTER = 44,
	QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_INTERBSS = 45,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_DISABLE_RESET_CMDID = 46,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_PPDU_DURATION_CMDID = 47,
	QCA_WLAN_VENDOR_RADIO_PARAM_TXBF_SOUND_PERIOD_CMDID = 48,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROMISC_MODE_CMDID = 49,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_BURST_MODE_CMDID = 50,
	QCA_WLAN_VENDOR_RADIO_PARAM_MCAST_BCAST_ECHO = 51,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANT_PLZN = 52,
	QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMSDU = 53,
	QCA_WLAN_VENDOR_RADIO_PARAM_ENABLE_AMPDU = 54,
	QCA_WLAN_VENDOR_RADIO_PARAM_HE_MBSSID_CTRL_FRAME_CONFIG = 55,
	QCA_WLAN_VENDOR_RADIO_PARAM_PROBE_RESP_RETRY_LIMIT = 56,
	QCA_WLAN_VENDOR_RADIO_PARAM_CTS_TIMEOUT = 57,
	QCA_WLAN_VENDOR_RADIO_PARAM_SLOT_TIME = 58,
	QCA_WLAN_VENDOR_RADIO_PARAM_ACK_TIMEOUT = 59,
	QCA_WLAN_VENDOR_RADIO_PARAM_CCK_TX_ENABLE = 60,
	QCA_WLAN_VENDOR_RADIO_PARAM_EQUAL_RU_ALLOCATION_ENABLE = 61,
	QCA_WLAN_VENDOR_RADIO_PARAM_ANTENNA_GAIN_HALF_DB = 62,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_MGMT_TTL = 63,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_PROBE_RESP_TTL = 64,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_MU_PPDU_DURATION = 65,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_TBTT_CTRL = 66,
	QCA_WLAN_VENDOR_RADIO_PARAM_SET_PREAM_PUNCT_BW = 67,
	QCA_WLAN_VENDOR_RADIO_PARAM_LOW_LATENCY_SCHED_MODE = 68,
	QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_CONFIG = 69,
	QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_OUTPUT = 70,
	QCA_WLAN_VENDOR_RADIO_PARAM_GPIO_INPUT  = 71,
	QCA_WLAN_VENDOR_RADIO_PARAM_GET_TEMPERATURE = 72,
	QCA_WLAN_VENDOR_RADIO_PARAM_MSDU_TTL = 73,
	QCA_WLAN_VENDOR_RADIO_PARAM_DFS_NOL_SUBCHANNEL_MARKING = 74,
	QCA_WLAN_VENDOR_RADIO_PARAM_RADAR_DETECT_COUNT = 75,
	QCA_WLAN_VENDOR_RADIO_PARAM_CTLPWRSCALE = 76,
	QCA_WLAN_VENDOR_RADIO_PARAM_EN_CHAN_144 = 77,
	QCA_WLAN_VENDOR_RADIO_PARAM_PDEV_TO_REO_DEST = 78,
	QCA_WLAN_VENDOR_RADIO_PARAM_NOL_CHAN_LIST = 79,
	/* Configure to put device in WSI bypass state.
	 * 1 - Bypass device
	 * 2 - Readd device
	 */
	QCA_WLAN_VENDOR_RADIO_PARAM_WSI_BYPASS = 80,
	QCA_WLAN_VENDOR_RADIO_PARAM_BAND_INFO = 81,
	QCA_WLAN_VENDOR_RADIO_PARAM_DISPLAY_BAND_CHANS = 82,
	QCA_WLAN_VENDOR_RADIO_PARAM_DISPLAY_SUPER_CHANNEL_LIST = 83,
	QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ALPHA2 = 84,
	QCA_WLAN_VENDOR_RADIO_PARAM_COUNTRY_ID = 85,
	QCA_WLAN_VENDOR_RADIO_PARAM_REGDOMAIN = 86,
	QCA_WLAN_VENDOR_RADIO_PARAM_CAC_TIMEOUT = 87,
	QCA_WLAN_VENDOR_RADIO_PARAM_LIST_5GHZ_CHAN_INFO = 88,
	QCA_WLAN_VENDOR_RADIO_PARAM_BGCAC_TIMEOUT = 89,
	QCA_WLAN_VENDOR_RADIO_PARAM_BLOCK_DFS_LIST = 90,
	QCA_WLAN_VENDOR_RADIO_PARAM_RX_FLOW_TAG_OP = 91,
	QCA_WLAN_VENDOR_RADIO_PARAM_GET_CAC_STATE = 92,
	QCA_WLAN_VENDOR_RADIO_PARAM_SCAN_STRICT_PASSIVE_PCH = 93,
	QCA_WLAN_VENDOR_RADIO_PARAM_GET_NFCAL_POWER = 94,
	QCA_WLAN_VENDOR_RADIO_PARAM_CAL_VER_CHECK = 95,

	/* Add new params above */
	QCA_WLAN_VENDOR_RADIO_PARAM_LAST,
	QCA_WLAN_VENDOR_RADIO_PARAM_MAX = QCA_WLAN_VENDOR_RADIO_PARAM_LAST - 1,
};

enum qca_nl80211_vendor_config_generic_command {
	/*Although named SUBCMD_WIFI_PARAMS,
	 *this is not a subcommand.
	 *The name is retained unchanged for compatibility purposes.
	 */
	QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS = 200,
	QCA_NL80211_VENDOR_RADIO_CONFIG_HWADDR = 237,
	QCA_NL80211_VENDOR_RADIO_SR_SELF_CONFIG = 310,
	QCA_NL80211_VENDOR_RADIO_SMART_ANT_CONFIG = 312,
	QCA_WLAN_VENDOR_WIFI_PARAM_ENABLE_SOUNDING = 601,
	QCA_WLAN_VENDOR_WIFI_PARAM_SU_SND_INTERVAL = 602,
	QCA_WLAN_VENDOR_WIFI_PARAM_MU_SND_INTERVAL = 603,
	QCA_WLAN_VENDOR_WIFI_PARAM_SCHED_MU_ENABLE = 604,
	QCA_WLAN_VENDOR_WIFI_PARAM_SCHED_OFDMA_ENABLE = 605,
	QCA_WLAN_VENDOR_WIFI_PARAM_SET_NAV_OVERRIDE_CONFIG = 606,
	QCA_WLAN_VENDOR_WIFI_PARAM_GET_NAV_OVERRIDE_CONFIG = 607,
	QCA_WLAN_VENDOR_WIFI_PARAM_UL_OFDMA_RTD = 608,
	QCA_WLAN_VENDOR_WIFI_PARAM_ALLOW_SCAN_ON_DFS_CHAN = 609,
	QCA_WLAN_VENDOR_WIFI_PARAM_BSSID = 610,
};

enum qca_wlan_vendor_attr_sdwf_dev {
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_DEF_Q_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_SLA_BREACHED_PARAMS = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RSSI_RATE_BREACH_PARAMS = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_dev_oper {
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_MAP = 0,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_UNMAP = 1,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_MAP_GET = 2,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_STREAMING_STATS = 3,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_RESET_STATS = 4,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_BREACH_DETECTED = 5,
};

enum qca_wlan_vendor_attr_sdwf_streaming_stats {
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_sla_breach_param {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC = 6,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AFTER_LAST - 1
};

enum qca_wlan_vendor_sdwf_sla_breach_type {
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_INVALID = 0,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MIN_THROUGHPUT,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MAX_THROUGHPUT,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_BURST_SIZE,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_SERVICE_INTERVAL,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_DELAY_BOUND,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MSDU_TTL,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MSDU_LOSS,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_rssi_rate_breach - RSSI/Rate breach attributes
 *
 * Attributes for RSSI and rate threshold breach notifications.
 * These are nested inside QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PARAMS.
 *
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MAC: Peer MAC address (6 bytes)
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_TYPE: Breach type (u8)
 *     Values from enum breach_type (RSSI_MIN=0, RSSI_MAX=1, etc.)
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD: Config thresh (u32/s32)
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE: Detected value (u32/s32)
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_SET_CLEAR: Breach state (u8)
 *     1 = breach detected, 0 = breach cleared
 * @QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MLD_MAC: MLD MAC
 *     address (6 bytes, optional)
 */
enum qca_wlan_vendor_attr_rssi_rate_breach {
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD = 3,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE = 4,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_SET_CLEAR = 5,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MLD_MAC = 6,

	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_MAX =
		QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_AFTER_LAST - 1,
};

/**
 *Add GPIO helper enums

 *Add direction, pull type and interrupt mode enums
 */
/* GPIO direction */
enum qca_wlan_gpio_direction {
	QCA_WLAN_GPIO_INPUT = 0,
	QCA_WLAN_GPIO_OUTPUT = 1,
};

/* GPIO pull type */
enum qca_wlan_gpio_pull_type {
	QCA_WLAN_GPIO_PULL_NONE = 0,
	QCA_WLAN_GPIO_PULL_UP = 1,
	QCA_WLAN_GPIO_PULL_DOWN = 2,
};

/* GPIO interrupt mode */
enum qca_wlan_gpio_intr_mode {
	QCA_WLAN_GPIO_INTMODE_DISABLE      = 0,
	QCA_WLAN_GPIO_INTMODE_RISING_EDGE  = 1,
	QCA_WLAN_GPIO_INTMODE_FALLING_EDGE = 2,
	QCA_WLAN_GPIO_INTMODE_BOTH_EDGE    = 3,
	QCA_WLAN_GPIO_INTMODE_LEVEL_LOW    = 4,
	QCA_WLAN_GPIO_INTMODE_LEVEL_HIGH   = 5,
};

/**
 * enum qca_wlan_vendor_attr_pri_link_migrate: Attributes used by the vendor
 *     subcommand/event %QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE.
 *
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR: 6 byte MAC address.
 *	(a) Used in subcommand to indicate that primary link migration
 * will occur only for the ML client with the given MLD MAC address.
 *	(b) Used in event to specify the MAC address of the peer for which
 * the primary link has been modified.
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_CURRENT_PRI_LINK_ID: Optional u8
 *     attribute. When specified, all ML clients having their current primary
 *     link as specified will be considered for migration.
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID: u8 attribute.
 *	(a) Optional attribute used in subcommand, to indicate the new
 * primary link to which the selected ML clients should be migrated to.
 * If not provided, the driver will select a suitable primary link
 * on its own.
 *	(b) Used in event, to indicate the new link ID which is set
 * as primary link.
 */
enum qca_wlan_vendor_attr_pri_link_migrate {
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_INVALID = 0,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_CURRENT_PRI_LINK_ID,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID,

       /* keep this last */
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_AFTER_LAST,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX =
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_dynamic_init_conf - This enum defines the different
 * dynamic app init/deinit configurations.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START: Indicates to driver this
 * is the initial app init and not an individual service dynamic init/de-init.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START: Indicates to driver this
 * is dynamic service init/start.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP: Indicates to driver this
 * is dynamic service de-init/stop.
 */
enum qca_wlan_vendor_dynamic_init_conf {
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START = 0,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START = 1,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP = 2,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_START = 3,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_STOP = 4,
};
/**
 * enum qca_wlan_vendor_attr_soc_device_info - Represents the SOC device
 * information available in the driver. The driver will send this information
 * to the userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID: u8, represents the SOC device ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS: u8, represents the number of
 * links present in the SOC device.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO: represents the link level
 * information. Array of nested attributes are defined in enum
 * qca_wlan_vendor_attr_link_info
 */
enum qca_wlan_vendor_attr_soc_device_info {
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS = 2,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_link_info - Represents the link level information
 * available in the driver. The driver will send this information to the
 * userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID: u16, represents the hardware link
 * ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_MAC: 6 byte MAC address represents the Link
 * MAC address.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW: u8, represents the channel bandwidth,
 * values are defined in enum qca_wlan_vendor_channel_width
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ: u16, represents the channel frequency
 * in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_BAND_CAP: Channel band capability, values are
 * defined in enum qca_wlan_vendor_link_band_caps
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK: u8, represents the max tx chainmask
 * value.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK: u8, represents the max rx chainmask
 * value.
 */
enum qca_wlan_vendor_attr_link_info {
	QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW = 3,
	QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ = 4,
	QCA_WLAN_VENDOR_ATTR_LINK_BAND_CAP = 5,
	QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK = 6,
	QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_MAX =
		QCA_WLAN_VENDOR_ATTR_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_app_generic_category: Represents the Generic
 * mapping frame category value.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_INVALID: Generic mapping catefory
 * invalid.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT: The driver includes this
 * category in the event  sent to the userspace when it receives a APP INIT
 * request frame.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_LINK_BW_NSS_CHANGE: Represents the
 * notification message that will be sent to the RM APP for changes observed
 * in the BW and NSS values at AP side. Array of nested attributes are defined
 * in enum qca_wlan_vendor_attr_link_bw_nss_change_info
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO: The driver
 * includes this category in the event sent to the userspace when it receives a
 * Assoc request frame from the STA without T2LM IE.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_WITH_T2LM_INFO: The driver
 * includes this category in the event sent to the userspace when it receives a
 * Assoc request frame from the STA with T2LM IE.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO: The driver includes
 * this category in the event sent to the userspace when it receives a Operatin
 * mode change notification from the STA.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC: The driver includes this
 * category in the event sent to the userspace when it receives a disassoc from
 * the connected STA.
 */
enum qca_wlan_vendor_attr_app_generic_category {
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT = 1,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_LINK_BW_NSS_CHANGE = 2,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO = 3,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_WITH_T2LM_INFO = 4,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO = 5,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC = 6,
};

/**
 * enum qca_wlan_vendor_attr_t2lm_mlo_peer_link_info - Represents the MLO peer
 * link inforamtion.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID: u16, represents the hardware
 * link id of the MLO peer link. This is included in the commands sent from the
 * userspace to the driver and used in the events sent from the driver to the
 * userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC: 6 byte MAC address represents
 * the MLO peer mac address. This is included in the commands sent from the
 * userspace to the driver and used in the events sent from the driver to the
 * userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MLO_LINK_ID: u8, represents the mlo peer
 * link index. This is included in the commands sent from the userspace to the
 * driver and used in the events sent from the driver to the userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK: u8, this is included in
 * the event sent from the driver to the userspace to identify the Assoc request
 * received link. Userspace includes this in all the commands sent to the driver
 * to identify the Assoc request received list.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_BW: u8, this is included in the
 * event sent from the driver to the userspace to indicate the STA's channel
 * bandwidth. The values are defined in enum qca_wlan_vendor_channel_width.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_FREQ: u16, this is included in the
 * event sent from the driver to the userspace to indicate the STA's channel
 * frequency in MHz. The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AVAILABLE_AIRTIME: u16, this is included
 * in the event sent from the driver to the userspace to indicate MLO peer
 * link's available airtime value (unit is percentage). The driver includes this
 * attribute in the event sent for QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_RSSI: s8, this is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's (Assoc
 * request received link) RSSI value in dBm. The driver includes this attribute
 * in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EHT_CAPS: This is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's EHT
 * capabilities. Values are defined in enum
 * qca_wlan_vendor_attr_eht_peer_capabilities.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_BAND_CAP: This is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's band
 * capabilities. Values are defined in enum qca_wlan_vendor_link_band_caps.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_VDEV_ID: u8, represents the vdev id.
 * This is included in the commands sent from the userspace to the driver
 * and used in the events sent from the driver to the userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AP_MLD_MAC: 6 byte MAC address represents
 * the vdev mld mac address.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CAPS: u16, represents peer capabilities.
 */
enum qca_wlan_vendor_attr_t2lm_mlo_peer_link_info {
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MLO_LINK_ID = 3,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK = 4,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_BW = 5,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_FREQ = 6,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AVAILABLE_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_RSSI = 8,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EHT_PEER_CAPS = 9,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_BAND_CAP = 10,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EFF_CHAN_BW = 11,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_VDEV_ID = 12,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AP_MLD_MAC = 13,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CAPS = 14,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MAX =
		QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_channel_width - Represents the channel bandwidth in MHz
 * available in the driver. The driver will send this information to the
 * userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID: Invalid channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ: 20 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ: 40 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ: 80 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ: 160 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ: 80+80 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ: 320 MHz channel bandwidth
 */
enum qca_wlan_vendor_channel_width {
	QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID = 0,
	QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ = 1,
	QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ = 2,
	QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ = 3,
	QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ = 4,
	QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ = 5,
	QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ = 6,
};

enum qca_wlan_vendor_attr_scs_rule_config {
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_INVALID = 0,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID = 1,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE = 2,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID = 3,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE = 4,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION = 5,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR = 6,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR = 7,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR = 8,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR = 9,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT = 10,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT = 11,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP = 12,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER = 13,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL = 14,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE = 15,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER = 16,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK = 17,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE = 18,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID = 19,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR = 20,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX = 21,

		/* Keep last */
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST -1,
};

/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_func_type - function types
 *
 * This enum defines the function/command types used with attribute
 * %QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC.
 *
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_TX_MGMT: Transmit management frame
 *	on home or off-channel
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_TX_DATA: Transmit data frame
 *	on home or off-channel
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_CANCEL: Cancel ongoing off-channel
 *	operation
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX: Start receive operation
 */
enum qca_vendor_wlan_home_offchan_tx_rx_func_type {
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_INVALID = 0,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_TX_MGMT = 1,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_TX_DATA = 2,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_CANCEL = 3,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX = 4,
};

/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_bw_mode - Bandwidth modes
 *
 * This enum defines the bandwidth modes used with attribute
 * %QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_BW_MODE.
 *
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_20MHZ: 20 MHz bandwidth
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_40MHZ: 40 MHz bandwidth
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_80MHZ: 80 MHz bandwidth
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_160MHZ: 160 MHz bandwidth
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_320MHZ: 320 MHz bandwidth
 */
enum qca_vendor_wlan_home_offchan_tx_rx_bw_mode {
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_20MHZ = 0,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_40MHZ = 1,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_80MHZ = 2,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_160MHZ = 3,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_BW_320MHZ = 4,
};

/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_pkt_status - Per-packet TX status
 *
 * This enum defines the per-packet transmission status used with attribute
 * %QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_RESULT.
 *
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_SUCCESS: Packet transmitted
 *	successfully
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ERROR: Packet transmission
 *	failed with error
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_XRETRY: Packet transmission
 *	failed due to excessive retries
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_UNKNOWN: Unknown status
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_TIMEOUT: Packet transmission
 *	timed out
 * @QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_BAD: Bad/invalid packet or
 *	parameters
 */
enum qca_vendor_wlan_home_offchan_tx_rx_pkt_status {
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_SUCCESS = 0,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ERROR = 1,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_XRETRY = 2,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_UNKNOWN = 3,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_TIMEOUT = 4,
	QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_BAD = 5,
};

/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_event_tx_pkt_status_attr -
 * Per-packet TX status
 *
 * This enum defines attributes for per-packet transmission status, used
 * with nested attribute
 * %QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_PKT_STATUS.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ID: u8 attribute.
 *	Frame identifier supplied by userspace in the TX command (0-255),
 *	echoed back so the application can correlate per-packet status with
 *	the original request.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_RESULT:
 * u8 attribute.
 *	Transmission status result for this packet.
 *	Uses enum qca_vendor_wlan_home_offchan_tx_rx_pkt_status.
 */
enum qca_vendor_wlan_home_offchan_tx_rx_event_tx_pkt_status_attr {
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ID,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_RESULT,

	/* keep last */
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_MAX =
		QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_AFTER_LAST - 1,
};


/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_attr - Attributes for
 * home/off-channel operations
 *
 * This enum defines attributes used with vendor subcommand
 * %QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX for configuring
 * home and off-channel transmission and reception operations, and for reporting
 * statistics and events.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC: u8 attribute.
 *	Specifies the off-channel function/command to perform.
 *	Uses enum qca_vendor_wlan_home_offchan_tx_rx_func_type.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN: u16 attribute.
 *	Specifies the channel frequency in MHz for the home/off-channel
 *	operation. Valid range depends on regulatory domain and band
 *	capabilities.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN_BAND: u8 attribute.
 *	Specifies the channel band. Valid values:
 *	0 - 2.4 GHz band
 *	1 - 5 GHz band
 *	2 - 6 GHz band
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SCAN_DUR: u32 attribute.
 *	Specifies the scan duration in milliseconds for operations.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME: Nested attribute.
 *	Contains frame transmission parameters. Uses attributes defined
 *	in enum qca_vendor_wlan_home_offchan_tx_rx_frame_attr. This
 *	attribute is mandatory when transmitting frames.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID: u32 attribute.
 *	Unique transaction identifier for correlating commands with
 *	events. The driver includes this ID in corresponding event
 *	notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_BW_MODE: u8 attribute.
 *	Specifies the bandwidth mode for the off-channel operation.
 *	Uses enum qca_vendor_wlan_home_offchan_tx_rx_bw_mode.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SEC_CHAN_OFFSET:
 *	u8 attribute. Specifies the secondary channel offset for 40 MHz
 *	operation.
 *	Valid values:
 *	0 - No secondary channel
 *	1 - Secondary channel above primary
 *	3 - Secondary channel below primary
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES: u8 attribute.
 *	Specifies the number of frames to transmit in the operation.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_IS_MLD: flag attribute
 *	Indicates if this is a Multi-Link Device (MLD) operation.
 *	Valid values:
 *	Attribute present   : MLD operation
 *	Attribute absent    : Non-MLD operation
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_LINK_ID: u8 attribute.
 *	Specifies the link ID for Multi-Link Device operations.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_STATUS:
 *	u8 attribute. Reports the overall operation status. Used in
 *	event notifications.
 *	Valid values:
 *	0 - Success: Operation completed successfully
 *	1 - Failure: Operation failed
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_NOISE_FLOOR:
 *	s16 attribute. Reports the noise floor in dBm measured during
 *	the operation. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_VALID:
 *	u8 attribute. Indicates whether blanking statistics are valid.
 *	Used in event notifications. Valid values:
 *	0 - Blanking statistics not valid
 *	1 - Blanking statistics valid
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_FRAME_COUNT:
 *	u32 attribute. Reports the number of frames transmitted during
 *	the operation. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_FRAME_COUNT:
 *	u32 attribute. Reports the number of frames received during
 *	off-channel operation. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_CLEAR_COUNT:
 *	u32 attribute. Reports the RX clear count in microseconds. This
 *	represents the time the medium was sensed idle during
 *	off-channel operation. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CYCLE_COUNT:
 *	u32 attribute. Reports the cycle count in microseconds. This
 *	represents the total time spent in off-channel operation.
 *	Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_DWELL_TIME:
 *	u32 attribute. Reports the actual dwell time in milliseconds
 *	spent on the off-channel. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_HTOF:
 *	u32 attribute. Reports the number of channel switches from home
 *	channel to off-channel. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_FTOH:
 *	u32 attribute. Reports the number of channel switches from
 *	off-channel to home channel. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_COUNT:
 *	u32 attribute. Reports the number of times the off-channel
 *	operation was blanked (interrupted) due to home channel
 *	activity. Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_DURATION:
 *	u32 attribute. Reports the total duration in microseconds that
 *	off-channel operation was blanked due to home channel activity.
 *	Used in event notifications.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_PKT_STATUS:
 *	Array of nested attributes. Contains per-packet transmission
 *	status information. Each array element is a nested attribute
 *	containing packet ID and status, using attributes from enum
 *	qca_vendor_wlan_home_offchan_tx_rx_event_tx_pkt_status_attr.
 *	The array can contain up to the number of frames specified in
 *	%QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_IFINDEX: u32 attribute.
 *	Interface index of the transmitting interface for the home/off-channel
 *	TX/RX event. Used by the driver to indicate which netdev sent the frame.
 */
enum qca_vendor_wlan_home_offchan_tx_rx_attr {
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_INVALID = 0,

	/* Command attributes */
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN_BAND,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SCAN_DUR,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_BW_MODE,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SEC_CHAN_OFFSET,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_IS_MLD,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_LINK_ID,

	/* Event/Statistics attributes */
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_STATUS,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_NOISE_FLOOR,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_VALID,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_FRAME_COUNT,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_FRAME_COUNT,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_CLEAR_COUNT,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CYCLE_COUNT,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_DWELL_TIME,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_HTOF,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_FTOH,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_COUNT,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_DURATION,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_PKT_STATUS,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_IFINDEX,

	/* keep last */
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX =
		QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_wlan_home_offchan_tx_rx_frame_attr - Frame
 * parameters for home and off-channel TX
 *
 * This enum defines attributes used with nested attribute
 * %QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME to specify frame
 * transmission parameters for off-channel operations.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS: u8 attribute.
 *	Specifies the number of spatial streams for frame transmission.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE:
 *	u8 attribute. Specifies the preamble type for frame
 *	transmission. Valid values:
 *	0 - Legacy preamble (OFDM/CCK)
 *	1 - HT preamble (802.11n)
 *	2 - VHT preamble (802.11ac)
 *	3 - HE preamble (802.11ax)
 *	4 - EHT preamble (802.11be)
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS: u8 attribute.
 *	Specifies the Modulation and Coding Scheme (MCS) index for frame
 *	transmission. Valid range depends on the preamble type:
 *	- Legacy: 0-7 (OFDM rates)
 *	- HT/VHT: 0-9
 *	- HE: 0-11
 *	- EHT: 0-13
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_RETRY:
 *	u8 attribute. Specifies the number of retries for frame
 *	transmission.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_POWER:
 *	u8 attribute. Specifies the transmit power in dBm for frame
 *	transmission.
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_TX_BEAMFORMING:
 *	u8 attribute. Specifies whether transmit beamforming is enabled
 *	for frame transmission.
 *	Valid values:
 *	0 - Beamforming disabled
 *	1 - Beamforming enabled
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA:
 *	Binary attribute. Contains the frame data to be transmitted.
 *
 * @QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_ID: u8 attribute.
 *	Application-assigned identifier for this frame (0-255). The driver
 *	caches this value and echoes it back in the per-packet TX status event
 *	(%QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ID) so the
 *	application can correlate completion status with the original request.
 */
enum qca_vendor_wlan_home_offchan_tx_rx_frame_attr {
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_RETRY,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_POWER,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_TX_BEAMFORMING,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_ID,

	/* keep last */
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX =
		QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_scan_radio_chan_stats - Attributes for
 * QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS event.
 *
 * Sent by the driver per WMI chan_info event on a scan-radio pdev.
 *
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_FREQ: u32, channel freq in MHz.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_VALID: u8, non-zero
 *	when blanking parameters below are valid.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_COUNT: u32, number of
 *	blanking events during the measurement period.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_DURATION: u32, total
 *	blanking duration in microseconds.
 * @QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_PHYMODE: NUL-terminated string,
 *	current PHY mode of the scan radio home channel (e.g. "11AXHE80").
 */
enum qca_wlan_vendor_attr_scan_radio_chan_stats {
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_VALID = 2,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_COUNT = 3,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_DURATION = 4,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_PHYMODE = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_config_esp_param - Parameters for ESP configuration
 *
 * Attributes used to configure (set command) or report (get command)
 * Estimated Service Parameters (ESP). ESP describes predicted service
 * characteristics such as airtime availability, PPDU duration, and Block Ack
 * window size per access category and it is advertised in the Estimated Service
 * Parameters Inbound element (see IEEE Std 802.11-2024, 9.4.2.172).
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_ENABLE: u8. Enable (1) or disable (0)
 * the advertisement of the Estimated Service Parameters Inbound element
 * in Beacon and Probe Response frames.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE: u8. Predicted percentage of
 * airtime available for AC_BE. This is the exact 8-bit value used in the ESP
 * Information field as defined in IEEE Std 802.11-2024, 9.4.2.172. The
 * value is linearly scaled: 0 represents 0% airtime, 255 represents 100%
 * airtime.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BE: u8. BE Access Category "target"
 * duration for PPDUs that carry Data MPDUs, i.e., how long the AP expects a
 * typical data transmission PPDU to last for the BE AC. Encoded in unts of 50
 * microseconds (actual_duration_us = value × 50) as defined in IEEE Std
 * 802.11-2024, 9.4.2.172.
 * Range: 0..255 units (0..12.75 ms). Example: value 16 -> 800 microseconds.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BE: u8. BA Window Size subfield
 * for AC_BE as defined in IEEE Std 802.11-2024, Table 9-334 (BA Window Size
 * subfield encoding).
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BK: u8. Airtime for AC_BK.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BK: u8. PPDU duration for AC_BK.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BK: u8. BA window for AC_BK.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_VI: u8. Airtime for AC_VI.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_VI: u8. PPDU duration for AC_VI.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_VI: u8. BA window for AC_VI.
 *
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_VO: u8. Airtime for AC_VO.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_VO: u8. PPDU duration for AC_VO.
 * @QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_VO: u8. BA window for AC_VO.
 */
enum qca_wlan_vendor_attr_config_esp_param {
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_ENABLE = 1,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE = 2,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BE = 3,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BE = 4,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BK = 5,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BK = 6,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BK = 7,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_VI = 8,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_VI = 9,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_VI = 10,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_VO = 11,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_VO = 12,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_VO = 13,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_MAX =
	QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_ch_switch_reason - Reason for channel switch
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_UNKNOWN: Reason unknown
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_DFS_RADAR: DFS radar detected
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_AWGN_INTERFERENCE: AWGN interference detected
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_CSA: Channel Switch Announcement
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_BW_REDUCTION: Channel switch to reduce BW
 * @QCA_WLAN_VENDOR_CH_SWITCH_REASON_USER_REQUEST: User-initiated channel change
 */
enum qca_wlan_vendor_ch_switch_reason {
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_UNKNOWN,
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_DFS_RADAR,
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_AWGN_INTERFERENCE,
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_CSA,
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_BW_REDUCTION,
	QCA_WLAN_VENDOR_CH_SWITCH_REASON_USER_REQUEST,
};

/**
 * enum qca_wlan_vendor_attr_ch_switch_reason - Attributes for channel switch
 * reason vendor event
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_UNSPEC: Reserved attribute
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CODE: reason enum value
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_FREQ: primary channel freq (MHz)
 *     of the current/old channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_WIDTH: channel width of the
 *     current/old channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ1: center frequency
 *     of the first segment of the current/old channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ2: center frequency
 *     of the second segment of the current/old channel for 80+80 MHz operation;
 *     0 if not applicable
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_PUNCTURED: punctured subchannel
 *     bitmap of the current/old channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_RADAR_BITMAP: radar subchannel
 *     bitmap of the current/old channel; each set bit represents a
 *     20 MHz subchannel on which radar has been detected; 0 if no
 *     radar subchannels are flagged
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_FREQ: primary channel freq (MHz)
 *     of the new channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_WIDTH: channel width of the new
 *     channel
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_AFTER_LAST: Internal use
 * @QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_MAX: Maximum attribute value
 */
enum qca_wlan_vendor_attr_ch_switch_reason {
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_UNSPEC,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CODE,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_FREQ,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_WIDTH,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ1,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ2,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_PUNCTURED,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_RADAR_BITMAP,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_FREQ,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_WIDTH,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_MAX =
	QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ctl_table - Attributes for CTL table vendor command
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_INVALID: Invalid attribute
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_BAND: Band for CTL table (u32)
 *      0 = 5GHz, 1 = 2.4GHz, 2 = 6GHz
 * @QCA_WLAN_VENDOR_ATTR_CTL_RADIO_INDEX: Radio index
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_LENGTH: Length of CTL table data (u32)
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA: CTL table data buffer (binary)
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_AFTER_LAST: Last attribute
 * @QCA_WLAN_VENDOR_ATTR_CTL_TABLE_MAX: Maximum attribute value
 */
enum qca_wlan_vendor_attr_ctl_table {
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_BAND = 1,
	QCA_WLAN_VENDOR_ATTR_CTL_RADIO_INDEX = 2,
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_LENGTH = 3,
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_DATA = 4,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_MAX =
	QCA_WLAN_VENDOR_ATTR_CTL_TABLE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcs - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID: 8-bit unsigned value for link ID.
 * Specifies which link to set/get in a multi-link setup.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE: 8-bit unsigned value for DCS command type
 *     0: GET – Retrieve the current DCS configuration.
 *        Userspace must provide QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE.
 *        QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID is also required in multi-link AP
 *        scenarios.
 *        The driver will return the following attributes:
 *             QCA_WLAN_VENDOR_ATTR_DCS_ENABLE
 *             QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD
 *             QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY
 *             QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD
 *             QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD
 *             QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD
 *             QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW
 *             QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD
 *             QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU
 *     1: SET – Update the DCS configuration.
 *        Userspaxce must provide QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE.
 *        QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID is also required in multi-link AP
 *        scenarios.
 *        One or more of the above attributes must be included with new
 *        values to apply the configuration update.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_ENABLE: 16-bit bitmap to set/get enable/disable DCS
 *     bit 0: Enable Continuous Wave Interference Management (CW IM)
 *     bit 1: Enable WLAN Interference Management (WLAN IM)
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD: 32-bit unsigned
 * value to set/get interference detection threshold. This attribute specifies
 * the number of interference events required to trigger a channel switch.
 * Higher values decrease sensitivity, making DCS less likely to switch.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY: 32-bit unsigned value to set/get
 * the PHY error penalty. This value specifies the amount of channel time
 * (in microseconds) counted as wasted for each PHY error when estimating
 * interference.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD: 32-bit unsigned value to set/get
 * the PHY error count threshold.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD: 32-bit unsigned value to
 * set/get radar error count threshold.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD: 32-bit unsigned value to set/get
 * TX error count threshold.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW: 32-bit unsigned
 * value to set/get the interference detection sampling window. The unit is a
 * count of sampling intervals, where each interval corresponds to one second.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD: 8-bit unsigned
 * value to set/get the co-channel interference threshold level, interpreted as
 * a percentage of channel time affected by same-channel interference.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU: 8-bit unsigned value to set/get the maximum
 * channel utilization percentage allowed. If the combined TX and RX channel
 * utilization exceeds this configured maximum CU, treats the condition as WLAN
 * interference.
 */
enum qca_wlan_vendor_attr_dcs {
	QCA_WLAN_VENDOR_ATTR_DCS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_DCS_CMD_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_DCS_ENABLE = 3,
	QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_THRESHOLD = 4,
	QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_PENALTY = 5,
	QCA_WLAN_VENDOR_ATTR_DCS_PHY_ERR_THRESHOLD = 6,
	QCA_WLAN_VENDOR_ATTR_DCS_RADAR_ERR_THRESHOLD = 7,
	QCA_WLAN_VENDOR_ATTR_DCS_TX_ERR_THRESHOLD = 8,
	QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_DETECTION_WINDOW = 9,
	QCA_WLAN_VENDOR_ATTR_DCS_COCHANNEL_INTERFERENCE_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_DCS_MAX_CU = 11,
	QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DCS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCS_MAX =
		QCA_WLAN_VENDOR_ATTR_DCS_AFTER_LAST - 1
};

/*
 * enum qca_wlan_vendor_attr_rrop_info - Specifies vendor specific
 * Representative RF Operating Parameter (RROP) information. It is sent for the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO. This information is
 * intended for use by external Auto Channel Selection applications. It provides
 * guidance values for some RF parameters that are used by the system during
 * operation. These values could vary by channel, band, radio, and so on.
 */
enum qca_wlan_vendor_attr_rrop_info {
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_INVALID = 0,

	/* Representative Tx Power List (RTPL) which has an array of nested
	 * values as per attributes in enum qca_wlan_vendor_attr_rtplinst.
	 */
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL = 1,

	QCA_WLAN_VENDOR_ATTR_RROP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_RROP_INFO_AFTER_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_rtplinst - Specifies attributes for individual list
 * entry instances in the Representative Tx Power List (RTPL). It provides
 * simplified power values intended for helping external Auto channel Selection
 * applications compare potential Tx power performance between channels, other
 * operating conditions remaining identical. These values are not necessarily
 * the actual Tx power values that will be used by the system. They are also not
 * necessarily the max or average values that will be used. Instead, they are
 * relative, summarized keys for algorithmic use computed by the driver or
 * underlying firmware considering a number of vendor specific factors.
 */
enum qca_wlan_vendor_attr_rtplinst {
	QCA_WLAN_VENDOR_ATTR_RTPLINST_INVALID = 0,

	/* Primary channel number (u8).
	 * Note: If both the driver and user space application support the
	 * 6 GHz band, this attribute is deprecated and
	 * QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY should be used. To
	 * maintain backward compatibility,
	 * QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY is still used if either the
	 * driver or user space application or both do not support the 6 GHz
	 * band.
	 */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY = 1,
	/* Representative Tx power in dBm (s32) with emphasis on throughput. */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT = 2,
	/* Representative Tx power in dBm (s32) with emphasis on range. */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE = 3,
	/* Primary channel center frequency (u32) in MHz */
	QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY_FREQUENCY = 4,

	QCA_WLAN_VENDOR_ATTR_RTPLINST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX =
		QCA_WLAN_VENDOR_ATTR_RTPLINST_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_channel_switch_time - Attributes for
 * %QCA_NL80211_VENDOR_SUBCMD_GET_CHANNEL_SWITCH_TIME.
 *
 * This vendor command is used to query the driver for the estimated time
 * required to complete a channel switch operation.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ: Required (u32).
 * Center frequency of the target channel in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH: Optional (u32).
 * Channel bandwidth. Uses values from enum nl80211_chan_width.
 *
 * @QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1: Optional (u32).
 * Center frequency of the first segment in MHz (for VHT/HE).
 *
 * @QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2: Optional (u32).
 * Center frequency of the second segment in MHz (for 80+80 MHz).
 *
 * @QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL: Response attribute (u32).
 * Total estimated channel switch time in microseconds.
 */
enum qca_wlan_vendor_attr_channel_switch_time {
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH = 2,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1 = 3,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2 = 4,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_PUNCT_BMAP = 5,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX =
		QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_mon_scan_stats {
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_PKTS,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_OK_BYTES,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_PKTS,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_ERR_BYTES,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_MGMT_PKTS,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_CTRL_PKTS,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_RX_DATA_PKTS,

	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_RX_MON_SCAN_STATS_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcs_sim - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_DCS_SIM.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID: 8-bit unsigned value for link ID.
 * Specifies which link to simulate in a multi-link setup.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE: 16-bit unsigned value for DCS simulation
 * type. Selects the interference simulation mode the driver should execute.
 * Userspace must provide this attribute. QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID
 * is also required in multi-link AP scenarios.
 *
 * @QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP: 32-bit bitmap to get the
 * channel bandwidth interference bitmap value from userspace.
 */
enum qca_wlan_vendor_attr_dcs_sim {
	QCA_WLAN_VENDOR_ATTR_DCS_SIM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCS_SIM_LINK_ID,
	QCA_WLAN_VENDOR_ATTR_DCS_SIM_TYPE,
	QCA_WLAN_VENDOR_ATTR_DCS_SIM_INTERFERENCE_BITMAP,

	QCA_WLAN_VENDOR_ATTR_DCS_SIM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCS_SIM_MAX =
		QCA_WLAN_VENDOR_ATTR_DCS_SIM_AFTER_LAST - 1
};

/*
 * enum qca_wlan_vendor_attr_me_config - Attributes for ME vendor command
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_INVALID: Invalid attribute
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_PARAM: Parameter selector (u32)
 *      One of the vdev_param IDs:
 *      - QCA_WLAN_VENDOR_VDEV_PARAM_ME
 *      - QCA_WLAN_VENDOR_VDEV_PARAM_IGMP_ME
 *      - QCA_WLAN_VENDOR_VDEV_PARAM_ME_GRP_LIMIT
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_VALUE: Value to set for the parameter (u32)
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_GET: Flag to indicate a get/query operation
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_AFTER_LAST: Last attribute
 * @QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX: Maximum attribute value
 */
enum qca_wlan_vendor_attr_me_config {
	QCA_WLAN_VENDOR_ATTR_ME_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ME_CONFIG_PARAM = 1,
	QCA_WLAN_VENDOR_ATTR_ME_CONFIG_VALUE = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ME_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_ME_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_me_list: Represents the List of attributes
 * used for HMMC/DENY Lists for ME Enhancements.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_INVALID: Information passed for HMMC/DENY list
 * is invalid.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_OPERATION: u8, represents the exact list operation
 * being performed (add, delete, dump). This is included in the commands sent from
 * userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_TYPE: u8, represents the type of the list for
 * which the relevant operation is being perfomed(hmmc, deny). This is included
 * in the commands sent from userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_IP_TYPE: u8, represents the IP version used
 * for the operation being performed (ipv4/ipv6). This is included in the commands
 * sent from userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR: 4 Bytes, represents the IPv4 address used
 * for the operation being performed. This is included in the commands
 * sent from userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR: 16 Bytes, represents the IPv6 address used
 * for the operation being performed. This is included in the commands
 * sent from userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_MASK: u32, represents the IPv4 Mask used
 * for the operation being performed. This is included in the commands
 * sent from userspace to the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ME_LIST_PREFIX: u32, represents the IPv6 Prefix used
 * for the operation being performed. This is included in the commands
 * sent from userspace to the driver.
 */
enum qca_wlan_vendor_attr_me_list {
	QCA_WLAN_VENDOR_ATTR_ME_LIST_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ME_LIST_OPERATION = 1,    /* u8: ADD/DEL/DUMP */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_TYPE = 2,         /* u8: List type */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_IP_TYPE = 3,      /* u8: IPv4/IPv6 */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR = 4,    /* binary: 4 bytes */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR = 5,    /* binary: 16 bytes */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_MASK = 6,         /* u32: IPv4 mask */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_PREFIX = 7,       /* u32: IPv6 prefix */

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ME_LIST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ME_LIST_MAX =
		QCA_WLAN_VENDOR_ATTR_ME_LIST_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_extended_monitor - Attributes used by
 * QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR.
 * Same subcommand is used by both user application to send request and driver
 * to send the response back.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE: u8 attribute.
 *     Mandatory attribute defining the type of operation.
 *     The possible types are defined in
 *     enum qca_vendor_extended_monitor_cmd_type.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_DIRECTION: u8 attribute.
 *     Mandatory attribute defining the direction for which the configuration is
 *     to be applied or retrieved. The possible directions are defined in
 *     enum qca_vendor_extended_monitor_direction.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_STATUS_CODE: u8 attribute.
 *    Reports the result of an extended monitor operation.
 *    For set commands, this field indicates whether the requested configuration
 *    was applied successfully. On failure, it contains an error code.
 *    For get commands, this field is populated with an error code if retrieving
 *    the configuration fails.
 *    The possible error codes are defined in the
 *    enum qca_vendor_extended_monitor_status_code.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG: Nested attribute.
 *    Contains direction-specific configuration for extended monitor operations.
 *    This attribute is valid only when %QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE is
 *    %QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER or
 *    %QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER.
 *
 *    The nested data encapsulates the full set of monitor parameters as
 *    defined in enum qca_vendor_attr_extended_monitor_filter_config.
 *
 *    For SET_FILTER operations, the contents of this attribute are used to
 *    program user-requested monitoring behavior (filters, reporting
 *    controls, and any hardware/firmware-specific monitoring options).
 *    For GET_FILTER operations, the driver populates this attribute with the
 *    currently active monitor configuration for the requested direction.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG: Nested attribute.
 *    Contains peer-specific configuration for extended monitor operations.
 *    This attribute is valid only when %QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE is
 *    %QCA_VENDOR_EXT_MON_CMD_TYPE_SET_PEER or
 *    %QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER.
 *
 *    The nested attributes follow the layout defined in
 *    enum qca_vendor_attr_extended_monitor_peer_config, describing the
 *    peer configuration for SET_PEER or GET_PEER commands.
 *    configuration maintained by the driver (for GET_PEER).
 *
 *    The interpretation of the peer configuration depends on the value of
 *    QCA_VENDOR_ATTR_EXT_MON_DIRECTION. When the direction is set to
 *    RX, this attribute carries or returns RX monitor specific peer
 *    configuration. When the direction is set to TX, it carries or returns
 *    TX monitor specific peer configuration.
 */
enum qca_vendor_attr_extended_monitor {
	QCA_VENDOR_ATTR_EXT_MON_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE = 1,
	QCA_VENDOR_ATTR_EXT_MON_DIRECTION = 2,
	QCA_VENDOR_ATTR_EXT_MON_STATUS_CODE = 3,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG = 4,
	QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG = 5,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_MAX =
		QCA_VENDOR_ATTR_EXT_MON_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_extended_monitor_cmd_type - Operation type for
 * QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR.
 *
 * Defines the type of configuration operation requested by the user.
 * These values indicate whether the command intends to set or
 * retrieve filter-based monitor configuration or peer-specific
 * monitor configuration.
 *
 * @QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER:
 *    Set request for filter-based monitor configuration.
 *    The corresponding filter configuration is supplied through
 *    %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG, depending on the
 *    %QCA_VENDOR_ATTR_EXT_MON_DIRECTION.
 *
 * @QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER:
 *    Get request for retrieving the currently active filter-based monitor
 *    configuration for a direction. Direction is indicated in
 *    %QCA_VENDOR_ATTR_EXT_MON_DIRECTION. Driver returns the configuration
 *    via %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG.
 *
 * @QCA_VENDOR_EXT_MON_CMD_TYPE_SET_PEER:
 *    Set request for programming peer-specific monitor configuration.
 *    The peer configuration must be provided through the nested attribute
 *    %QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG. The interpretation of the
 *    peer configuration (RX or TX) depends on
 *    %QCA_VENDOR_ATTR_EXT_MON_DIRECTION.
 *
 * @QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER:
 *    Get request for retrieving peer-specific monitor configuration already
 *    stored by the driver. The driver returns the peer configuration through
 *    %QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG, with RX/TX semantics governed
 *    by %QCA_VENDOR_ATTR_EXT_MON_DIRECTION.
 */
enum qca_vendor_extended_monitor_cmd_type {
	QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER = 1,
	QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER = 2,
	QCA_VENDOR_EXT_MON_CMD_TYPE_SET_PEER = 3,
	QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER = 4,
};

/**
 * enum qca_vendor_extended_monitor_direction - Direction selector for
 * extended monitor mode capture.
 * It is used to indicate the direction in %QCA_VENDOR_ATTR_EXT_MON_DIRECTION
 * which is a mandatory attribute alongwith %QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE
 * for application requests. This enum defines the direction for all the cmd
 * types defined in enum qca_vendor_extended_monitor_cmd_type.
 *
 * @QCA_VENDOR_EXT_MON_DIRECTION_RX: Capture RX frames only.
 * @QCA_VENDOR_EXT_MON_DIRECTION_TX: Capture TX frames only.
 */
enum qca_vendor_extended_monitor_direction {
	QCA_VENDOR_EXT_MON_DIRECTION_RX = 1,
	QCA_VENDOR_EXT_MON_DIRECTION_TX = 2,
};

/**
 * enum qca_vendor_extended_monitor_status_code - Error codes for extended monitor
 * tool failure in driver. Each error code corresponds to a failure in driver,
 * which is sent to the application in the response using
 * %QCA_VENDOR_ATTR_EXT_MON_STATUS_CODE.
 *
 * @QCA_VENDOR_EXT_MON_SUCCESS: Application request is processed successfully.
 * @QCA_VENDOR_EXT_MON_VALIDATION_FAIL: Request failure indicating that the configs
 * are incorrect as per the driver.
 * @QCA_VENDOR_EXT_MON_FILTER_SETUP_FAIL: Request failure indicating failure in
 * sending the HTT message to set the requested filter.
 * @QCA_VENDOR_EXT_MON_PEER_SETUP_FAIL: Request failure indicating failure in
 * configuring the requested peers.
 */
enum qca_vendor_extended_monitor_status_code {
	QCA_VENDOR_EXT_MON_SUCCESS = 0,
	QCA_VENDOR_EXT_MON_VALIDATION_FAIL = 1,
	QCA_VENDOR_EXT_MON_FILTER_SETUP_FAIL = 2,
	QCA_VENDOR_EXT_MON_PEER_SETUP_FAIL = 3,
};

/**
 * enum qca_vendor_attr_extended_monitor_filter_config - Nested attributes for filter
 * configuration used with %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL: u8 attribute.
 *     Level of filtering applied. This controls the granularity of frame capture.
 *     Uses enum qca_vendor_extended_monitor_filter_level.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE: Flag attribute.
 *     If set, disables extended monitor filter settings for direction set in
 *     %QCA_VENDOR_ATTR_EXT_MON_DIRECTION
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER: Nested attribute.
 *     Filter settings for frames from connected clients.
 *     The filter settings are provided using
 *     enum qca_vendor_attr_extended_monitor_pkt_config_filter.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR: Nested attribute.
 *     Filter settings for frames from non-associated clients (neighbors).
 *     The filter settings are provided using
 *     enum qca_vendor_attr_extended_monitor_pkt_config_filter.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER: Nested attribute.
 *     Filter settings for frames from clients that are added using
 *     the set peer command. The filter settings are provided using
 *     enum qca_vendor_attr_extended_monitor_pkt_config_filter.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR: Nested attribute.
 *     Filter settings for frames from neighbors added using
 *     the set peer command. The filter settings are provided using
 *     enum qca_vendor_attr_extended_monitor_pkt_config_filter.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA: u8 attribute.
 *     Indicates packet metadata bitmap configured by the user.
 *     The bitmap definition is application-specific and should be agreed upon
 *     between the driver and the user space application.
 */
enum qca_vendor_attr_extended_monitor_filter_config {
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL = 1,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE = 2,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER = 3,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR = 4,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER = 5,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR = 6,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA = 7,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MAX =
		QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_extended_monitor_filter_level - Filter level selector
 * Enum defining the supported filter levels. These values control the
 * granularity of frame capture.
 *
 * @QCA_VENDOR_EXT_MON_FILTER_LEVEL_MSDU: Capture all MSDUs.
 *     This level captures every MSDU (MAC Service Data Unit) individually.
 *
 * @QCA_VENDOR_EXT_MON_FILTER_LEVEL_MPDU: Capture the first MSDU of every MPDU.
 *     This level captures only the first MSDU from each MPDU (MAC Protocol
 *     Data Unit), reducing the capture volume while maintaining frame flow
 *     visibility.

 * @QCA_VENDOR_EXT_MON_FILTER_LEVEL_PPDU: Capture the first MSDU of the first
 *     MPDU of every PPDU. This level captures only the first MPDU from each PPDU
 *     (Physical Layer Protocol Data Unit), providing the most coarse-grained
 *     capture with minimal overhead.
 */
enum qca_vendor_extended_monitor_filter_level {
	QCA_VENDOR_EXT_MON_FILTER_LEVEL_MSDU = 1,
	QCA_VENDOR_EXT_MON_FILTER_LEVEL_MPDU = 2,
	QCA_VENDOR_EXT_MON_FILTER_LEVEL_PPDU = 3,
};

/**
 * enum qca_vendor_attr_extended_monitor_packet_config - Nested attribute.
 * Defines the packet configuration for a particular type for the below attributes:
 * %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER,
 * %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR,
 * %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER
 * %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER: Nested attribute.
 *     Defines the subtype bitmask for different frame types.
 *     This bitmask controls which frame subtypes are allowed for a
 *     particular frame type.
 *     See enum qca_vendor_attr_extended_monitor_pkt_config_filter.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN: Nested attribute.
 *     Defines the packet length configured for different frame types.
 *     See enum qca_vendor_attr_extended_monitor_pkt_config_len
 */
enum qca_vendor_attr_extended_monitor_packet_config {
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER = 1,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN = 2,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_MAX =
		QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_extended_monitor_pkt_config_filter - Nested attribute.
 * Defines subtype frame masks for management, control, and data frames.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT: u32 attribute.
 *     Subtype frame mask for management frames.
 *     Subtype bit numbering follows IEEE 802.11 specification.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL: u32 attribute.
 *     Subtype frame mask for control frames.
 *     Subtype bit numbering follows IEEE 802.11 specification.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA: u32 attribute.
 *     Frame mask for data frames (extended monitor specific).
 *     The following are examples of supported masks:
 *       - 0xFFFF : ALL data frames
 *       - 0x0008 : Subtype Null
 *       - 0x4000 : Multicast frames
 *       - 0x8000 : Unicast frames
 */
enum qca_vendor_attr_extended_monitor_pkt_config_filter {
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT = 1,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL = 2,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA = 3,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MAX =
		QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_extended_monitor_pkt_config_len - Nested attribute.
 * Each attribute's possible values are defined as per
 * enum qca_vendor_extended_monitor_len.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT: u8 attribute.
 *     Frame length limit for management frames.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL: u8 attribute.
 *     Frame length limit for control frames.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA: u8 attribute.
 *     Frame length limit for data frames.
 */
enum qca_vendor_attr_extended_monitor_pkt_config_len {
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT = 1,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL = 2,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA = 3,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MAX =
		QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_extended_monitor_len - Length selector
 * Enum defining the supported frame length presets.
 * The enum values are selectors that map to specific hardware capture lengths.
 * User space and driver must use a common mapping:
 *   QCA_VENDOR_EXT_MON_LEN_64B      -> 64 bytes
 *   QCA_VENDOR_EXT_MON_LEN_128B     -> 128 bytes
 *   QCA_VENDOR_EXT_MON_LEN_256B     -> 256 bytes
 *   QCA_VENDOR_EXT_MON_LEN_FULL_PKT -> Full packet capture
 *
 * These selector values are passed through the u8 netlink attributes defined in
 * enum qca_vendor_attr_extended_monitor_pkt_config_len.
 */
enum qca_vendor_extended_monitor_len {
	QCA_VENDOR_EXT_MON_LEN_INVALID = 0,
	QCA_VENDOR_EXT_MON_LEN_64B = 1,
	QCA_VENDOR_EXT_MON_LEN_128B = 2,
	QCA_VENDOR_EXT_MON_LEN_256B = 3,
	QCA_VENDOR_EXT_MON_LEN_FULL_PKT = 4,
};

/**
 * enum qca_vendor_attr_extended_monitor_peer_config - Nested attributes for peer
 * management, used in both Rx/Tx peer settings.
 * When cmd type is set to QCA_VENDOR_EXT_MON_CMD_TYPE_SET_PEER,
 * it contains per-peer information provided by user.
 * When cmd type is set to %QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER,
 * it contains information on all currently configured peers in driver.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_ACTION: u8 attribute.
 *    See enum qca_vendor_extended_monitor_peer_action.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT: u8 attribute.
 *    Number of peer entries present in %QCA_VENDOR_ATTR_EXT_MON_PEER_INFO.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_INFO: Array of Nested attribute.
 *    Each array element is a nested container encoded with
 *    enum qca_vendor_attr_extended_monitor_peer_info:
 *      - QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR (6 bytes)
 *      - QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP (u8)
 *    Array encoding: QCA_VENDOR_ATTR_EXT_MON_PEER_INFO contains N nested
 *    elements (indices 0..N-1). Each element in turn contains the MAC/bitmap
 *    fields.
 */
enum qca_vendor_attr_extended_monitor_peer_config {
	QCA_VENDOR_ATTR_EXT_MON_PEER_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_PEER_ACTION = 1,
	QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT = 2,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO = 3,

	/* keep last */
	QCA_VENDOR_ATTR_EXT_MON_PEER_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_PEER_MAX =
		QCA_VENDOR_ATTR_EXT_MON_PEER_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_extended_monitor_peer_action - Action for extended monitor
 * peer entry.
 *
 * @QCA_VENDOR_EXT_MON_PEER_ACTION_ADD: Add peer entry.
 *
 * @QCA_VENDOR_EXT_MON_PEER_ACTION_REMOVE: Remove peer entry.
 */
enum qca_vendor_extended_monitor_peer_action {
	QCA_VENDOR_EXT_MON_PEER_ACTION_ADD = 1,
	QCA_VENDOR_EXT_MON_PEER_ACTION_REMOVE = 2,
};

/**
 * enum qca_vendor_attr_extended_monitor_snr_info - Nested attribute.
 * Consists of peer's snr related information
 *
 * @QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_SNR: u8 attribute
 *     Latest value of SNR for the peer.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AVG_SNR: u8 attribute
 *     Average value of SNR for the peer so far
 *
 * @QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_TSTAMP: u64 attribute
 *     Timestamp at which the latest SNR was populated for the peer.
 */
enum qca_vendor_attr_extended_monitor_snr_info {
	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_SNR = 1,
	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AVG_SNR = 2,
	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_TSTAMP = 3,

	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_MAX =
		QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_attr_extended_monitor_peer_info - Nested attribute.
 * Consists of per-peer information used in extended monitor peer
 * configuration.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR: 6-Byte MAC Address.
 *     Peer MAC address, either requested by user or reported by driver.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA: Flag attribute.
 *    Set if the peer mac address present in
 *    %QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR is a receiving mac address.
 *    If this flag is set then the filters are applied to all the frames received
 *    by this peer, else the filter are applied to all the frames transmitted by
 *    this peer, which is the default behavior.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP: u8 attribute.
 *     Bitmap controlling which major frame types are enabled for this peer.
 *     Bits may be independently enabled or combined.
 *     Bit definitions (LSB = bit 0):
 *       Bit 0 – Management frame enable
 *       Bit 1 – Control frame enable
 *       Bit 2 – Data frame enable
 *
 *     When a bit is enabled for a peer, actual frame capture is further
 *     qualified by the subtype mask defined in the global filter
 *     configuration:
 *       - For connected peers, subtype filtering is controlled by
 *         %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER.
 *       - For non‑connected peers, subtype filtering is controlled by
 *         %QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR.
 *
 *     These subtype masks apply globally to all peers whose MAC addresses
 *     are included in the extended monitor peer configuration, but the
 *     per‑peer bitmap determines which major frame categories are enabled
 *     for each individual peer.
 *
 * @QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_SNR_INFO: Nested attribute
 *     Defines peer's snr related information.
 *     See enum qca_vendor_attr_extended_monitor_snr_info
 */
enum qca_vendor_attr_extended_monitor_peer_info {
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_INVALID = 0,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR = 1,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA = 2,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP = 3,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_SNR_INFO = 4,

	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_AFTER_LAST,
	QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAX =
		QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_attr_get_sta_info - Defines attributes
 * used by QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC:
 * Required attribute in request for AP mode only, 6-byte MAC address,
 * corresponding to the station's MAC address for which information is
 * requested. For STA mode this is not required as the info always correspond
 * to the self STA and the current/last association.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS:
 * Optionally used in response, u32 attribute, contains a bitmap of different
 * fields defined in enum qca_vendor_wlan_sta_flags, used in AP mode only.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL:
 * Optionally used in response, u32 attribute, possible values are defined in
 * enum qca_vendor_wlan_sta_guard_interval, used in AP mode only.
 * Guard interval used by the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Value indicates the number of data frames received from station with retry
 * bit set to 1 in FC.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Counter for number of data frames with broadcast or multicast address in
 * the destination address received from the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED:
 * Optionally used in response, u32 attribute, used in both STA and AP modes.
 * Value indicates the number of data frames successfully transmitted only
 * after retrying the packets and for which the TX status has been updated
 * back to host from target.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED:
 * Optionally used in response, u32 attribute, used in both STA and AP mode.
 * Value indicates the number of data frames not transmitted successfully even
 * after retrying the packets for the number of times equal to the total number
 * of retries allowed for that packet and for which the TX status has been
 * updated back to host from target.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Counter in the target for the number of data frames successfully transmitted
 * to the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY:
 * Optionally used in response, u32 attribute, used in AP mode only.
 * Value indicates the number of data frames successfully transmitted only
 * after retrying the packets.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED:
 * Optionally used in response, u32 attribute, used in both STA & AP mode.
 * Value indicates the number of data frames not transmitted successfully even
 * after retrying the packets for the number of times equal to the total number
 * of retries allowed for that packet.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_PROBE_REQ_BMISS_COUNT: u32, used in
 * the STA mode only. Represent the number of probe requests sent by the STA
 * while attempting to roam on missing certain number of beacons from the
 * connected AP. If queried in the disconnected state, this represents the
 * count for the last connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_PROBE_RESP_BMISS_COUNT: u32, used in
 * the STA mode. Represent the number of probe responses received by the station
 * while attempting to roam on missing certain number of beacons from the
 * connected AP. When queried in the disconnected state, this represents the
 * count when in last connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_ALL_COUNT: u32, used in the
 * STA mode only. Represents the total number of frames sent out by STA
 * including Data, ACK, RTS, CTS, Control Management. This data is maintained
 * only for the connect session. Represents the count of last connected session,
 * when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_COUNT: u32, used in the STA mode.
 * Total number of RTS sent out by the STA. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_RETRY_FAIL_COUNT: u32, used in the
 * STA mode.Represent the number of RTS transmission failure that reach retry
 * limit. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_NON_AGGREGATED_COUNT: u32, used in
 * the STA mode. Represent the total number of non aggregated frames transmitted
 * by the STA. This data is maintained per connect session. Represents the count
 * of last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_AGGREGATED_COUNT: u32, used in the
 * STA mode. Represent the total number of aggregated frames transmitted by the
 * STA. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_GOOD_PLCP_COUNT: u32, used in
 * the STA mode. Represents the number of received frames with a good PLCP. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_INVALID_DELIMITER_COUNT: u32,
 * used in the STA mode. Represents the number of occasions that no valid
 * delimiter is detected by A-MPDU parser. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT: u32, used in the
 * STA mode. Represents the number of frames for which CRC check failed in the
 * MAC. This data is maintained per connect session. Represents the count of
 * last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_ACKS_GOOD_FCS_COUNT: u32, used in the
 * STA mode. Represents the number of unicast ACKs received with good FCS. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BLOCKACK_COUNT: u32, used in the STA
 * mode. Represents the number of received Block Acks. This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BEACON_COUNT: u32, used in the STA
 * mode. Represents the number of beacons received from the connected BSS. This
 * data is maintained per connect session. Represents the count of last
 * connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_OTHER_BEACON_COUNT: u32, used in the
 * STA mode. Represents the number of beacons received by the other BSS when in
 * connected state (through the probes done by the STA). This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_UCAST_DATA_GOOD_FCS_COUNT: u64, used in
 * the STA mode. Represents the number of received DATA frames with good FCS and
 * matching Receiver Address when in connected state. This data is maintained
 * per connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_DATA_BC_MC_DROP_COUNT: u32, used in the
 * STA mode. Represents the number of RX Data multicast frames dropped by the HW
 * when in the connected state. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS: u32, used in the
 * STA mode. This represents the target power in dBm for the transmissions done
 * to the AP in 2.4 GHz at 1 Mbps (DSSS) rate. This data is maintained per
 * connect session. Represents the count of last connected session, when
 * queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 2.4 GHz at 6 Mbps (OFDM) rate. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_MCS0: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 2.4 GHz at MCS0 rate. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done to
 * the AP in 5 GHz at 6 Mbps (OFDM) rate. This data is maintained per connect
 * session. Represents the count of last connected session, when queried in
 * the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_MCS0: u32, used in the
 * STA mode. This represents the Target power in dBm for transmissions done
 * to the AP in 5 GHz at MCS0 rate. This data is maintained per connect session.
 * Represents the count of last connected session, when queried in the
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_HW_BUFFERS_OVERFLOW_COUNT: u32, used
 * in the STA mode. This represents the Nested attribute representing the
 * overflow counts of each receive buffer allocated to the hardware during the
 * STA's connection. The number of hw buffers might vary for each WLAN
 * solution and hence this attribute represents the nested array of all such
 * HW buffer count. This data is maintained per connect session. Represents
 * the count of last connected session, when queried in the disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_TX_POWER: u32, Max TX power (dBm)
 * allowed as per the regulatory requirements for the current or last connected
 * session. Used in the STA mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER: u32, Latest TX power
 * (dBm) used by the station in its latest unicast frame while communicating
 * to the AP in the connected state. When queried in the disconnected state,
 * this represents the TX power used by the STA with last AP communication
 * when in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL: u32, Adaptive noise immunity
 * level used to adjust the RX sensitivity. Represents the current ANI level
 * when queried in the connected state. When queried in the disconnected
 * state, this corresponds to the latest ANI level at the instance of
 * disconnection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_IES: Binary attribute containing
 * the raw information elements from Beacon frames. Represents the Beacon frames
 * of the current BSS in the connected state. When queried in the disconnected
 * state, these IEs correspond to the last connected BSSID.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PROBE_RESP_IES: Binary attribute
 * containing the raw information elements from Probe Response frames.
 * Represents the Probe Response frames of the current BSS in the connected
 * state. When queried in the disconnected state, these IEs correspond to the
 * last connected BSSID.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_DRIVER_DISCONNECT_REASON: u32, Driver
 * disconnect reason for the last disconnection if the disconnection is
 * triggered from the host driver. The values are referred from
 * enum qca_disconnect_reason_codes.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT: u32, used in STA mode
 * only. This represents the number of group addressed robust management frames
 * received from this station with an invalid MIC or a missing MME when PMF is
 * enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT: u32, used in STA mode
 * only. This represents the number of group addressed robust management frames
 * received from this station with the packet number less than or equal to the
 * last received packet number when PMF is enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT: u32, used in STA
 * mode only. This represents the number of Beacon frames received from this
 * station with an invalid MIC or a missing MME when beacon protection is
 * enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT: u32, used in STA mode
 * only. This represents number of Beacon frames received from this station with
 * the packet number less than or equal to the last received packet number when
 * beacon protection is enabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE: u32, used in
 * STA mode only. The driver uses this attribute to populate the connection
 * failure reason codes and the values are defined in
 * enum qca_sta_connect_fail_reason_codes. Userspace applications can send
 * QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO vendor command after receiving
 * a connection failure indication from the driver. The driver shall not
 * include this attribute in response to the
 * QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO command if there is no connection
 * failure observed in the last attempted connection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE: u32, latest TX rate (Kbps)
 * used by the station in its last TX frame while communicating to the AP in the
 * connected state. When queried in the disconnected state, this represents the
 * rate used by the STA in the last TX frame to the AP when it was connected.
 * This attribute is used for STA mode only.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX: u32, used in STA mode only.
 * This represents the rate index used by the STA for the last TX frame to the
 * AP. When queried in the disconnected state, this gives the last RIX used by
 * the STA in the last TX frame to the AP when it was connected.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT: u32, used in STA
 * mode only. This represents the number of times the STA TSF goes out of sync
 * from the AP after the connection. If queried in the disconnected state, this
 * gives the count of TSF out of sync for the last connection.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON: u32, used in STA
 * mode only. This represents the roam trigger reason for the last roaming
 * attempted by the firmware. This can be queried either in connected state or
 * disconnected state. Each bit of this attribute represents the different
 * roam trigger reason code which are defined in enum qca_vendor_roam_triggers.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON: u32, used in STA mode
 * only. This represents the roam fail reason for the last failed roaming
 * attempt by the firmware. Different roam failure reason codes are specified
 * in enum qca_vendor_roam_fail_reasons. This can be queried either in
 * connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON: u32, used in
 * STA mode only. This represents the roam invoke fail reason for the last
 * failed roam invoke. Different roam invoke failure reason codes
 * are specified in enum qca_vendor_roam_invoke_fail_reasons. This can be
 * queried either in connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY: u32, used in STA mode only.
 * This represents the average congestion duration of uplink frames in MAC
 * queue in unit of ms. This can be queried either in connected state or
 * disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_TX_PACKETS: Array of u32 nested
 * values, used in AP mode. This represents the MPDU packet count per MCS
 * rate value of TX packets. Every index of this nested attribute corresponds
 * to MCS index, e.g., Index 0 represents MCS0 TX rate. This can be
 * queried in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_RX_PACKETS: Array of u32 nested
 * values, used in AP mode. This represents the MPDU packet count per MCS
 * rate value of RX packets. Every index of this nested attribute corresponds
 * to MCS index, e.g., Index 0 represents MCS0 RX rate. This can be
 * queried in connected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PAD: Attribute used for padding for
 * 64-bit alignment.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER: u32, used in STA mode
 * only. This represents the average of the delta between successive uplink
 * frames congestion duration in MAC queue in unit of ms. This can be queried
 * either in connected state or disconnected state.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT: Array of nested attributes,
 * used in STA mode. This represents the number of MSDU packets
 * (unicast/multicast/broadcast) transmitted/received with each NSS value. See
 * enum qca_wlan_vendor_attr_nss_pkt.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MCS_PKT_COUNT: Array of nested attributes,
 * used in STA mode. This represents the number of PPDUs
 * (unicast/multicast/broadcast) transmitted/received with each MCS value. See
 * enum qca_wlan_vendor_attr_mcs_pkt.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BW_PKT_COUNT: Array of nested attributes,
 * used in STA mode. This represents the number of PPDUs
 * (unicast/multicast/broadcast) transmitted/received on each bandwidth value.
 * See enum qca_wlan_vendor_attr_bw_pkt.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CCA_STAT: Array of nested attributes
 * representing the CCA statistics for the affiliated AP(s) in STA mode. This
 * uses attributes defined in enum qca_wlan_vendor_attr_cca_stat.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MISS_STAT: Array of nested
 * attributes representing the beacon miss data for the affiliated AP(s) in STA
 * mode. This uses attributes defined in
 * enum qca_wlan_vendor_attr_beacon_miss_stat.
 *
 * @QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LINK_ID:  u8 attribute.
 * Mandatory attribute when interface is configured in Multi-Link Operation
 * (MLO). This attribute must not be included in non-MLO scenarios. Possible
 * values are 0 to 14.
 *
 * @QCA_WLAN_VENDOR_ATTR_STA_INFO_MAX_RSSI: s8 attribute.
 * Maximum received signal strength indicator (RSSI) value observed after association
 * for the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_STA_INFO_MIN_RSSI: s8 attribute.
 * Minimum received signal strength indicator (RSSI) value observed after association
 * for the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_STA_INFO_PS_STATE: u8 attribute.
 * Indicates the current power‑save state of the station as reported by
 * firmware (ON, OFF, or DISABLED). Possible values are 0, 1 or 2.
 */
enum qca_wlan_vendor_attr_get_sta_info {
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS = 2,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL = 3,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT = 4,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT = 5,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED = 6,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED = 7,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL = 8,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY = 9,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED = 10,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_PROBE_REQ_BMISS_COUNT = 11,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_PROBE_RESP_BMISS_COUNT = 12,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_ALL_COUNT = 13,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_COUNT = 14,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_RETRY_FAIL_COUNT = 15,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_NON_AGGREGATED_COUNT = 16,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_AGGREGATED_COUNT = 17,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_GOOD_PLCP_COUNT = 18,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_INVALID_DELIMITER_COUNT = 19,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT = 20,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_ACKS_GOOD_FCS_COUNT = 21,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BLOCKACK_COUNT = 22,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BEACON_COUNT = 23,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_OTHER_BEACON_COUNT = 24,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_UCAST_DATA_GOOD_FCS_COUNT = 25,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_DATA_BC_MC_DROP_COUNT = 26,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS = 27,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS = 28,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_MCS0 = 29,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS = 30,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_MCS0 = 31,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_HW_BUFFERS_OVERFLOW_COUNT = 32,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_TX_POWER = 33,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER = 34,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL = 35,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_IES = 36,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PROBE_RESP_IES = 37,
	QCA_WLAN_VENDOR_ATTR_GET_STA_DRIVER_DISCONNECT_REASON = 38,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT = 39,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT = 40,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT = 41,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT = 42,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE = 43,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE = 44,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX = 45,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT = 46,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON = 47,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON = 48,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON = 49,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY = 50,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_TX_PACKETS = 51,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_RX_PACKETS = 52,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PAD = 53,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER = 54,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT = 55,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MCS_PKT_COUNT = 56,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BW_PKT_COUNT = 57,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CCA_STAT = 58,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MISS_STAT = 59,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LINK_ID = 60,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_RSSI = 61,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MIN_RSSI = 62,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PS_STATE = 63,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_vendor_oem_device_type - Represents the target device in firmware.
 * It is used by QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO.
 *
 * @QCA_VENDOR_OEM_DEVICE_VIRTUAL: The command is intended for
 * a virtual device.
 *
 * @QCA_VENDOR_OEM_DEVICE_PHYSICAL: The command is intended for
 * a physical device.
 */
enum qca_vendor_oem_device_type {
	QCA_VENDOR_OEM_DEVICE_VIRTUAL = 0,
	QCA_VENDOR_OEM_DEVICE_PHYSICAL = 1,
};

/**
 * enum qca_wlan_vendor_attr_oem_data_params - Used by the vendor command/event
 * QCA_NL80211_VENDOR_SUBCMD_OEM_DATA.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA: This NLA_BINARY attribute is
 * used to set/query the data to/from the firmware. On query, the same
 * attribute is used to carry the respective data in the reply sent by the
 * driver to userspace. The request to set/query the data and the format of the
 * respective data from the firmware are embedded in the attribute. The
 * maximum size of the attribute payload is 1024 bytes.
 * Userspace has to set the QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED
 * attribute when the data is queried from the firmware.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO: The binary blob will be routed
 * based on this field. This optional attribute is included to specify whether
 * the device type is a virtual device or a physical device for the
 * command/event. This attribute can be omitted for a virtual device (default)
 * command/event.
 * This u8 attribute is used to carry information for the device type using
 * values defined by enum qca_vendor_oem_device_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED: This NLA_FLAG attribute
 * is set when the userspace queries data from the firmware. This attribute
 * should not be set when userspace sets the OEM data to the firmware.
 */
enum qca_wlan_vendor_attr_oem_data_params {
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA = 1,
	QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO = 2,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX =
		QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_AFTER_LAST - 1,
};

/*
 * enum qca_wlan_vendor_attr_pcp_tid_entry - Inner attributes for each
 * PCP-TID mapping entry nested inside QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE.
 *
 * @QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP: NLA_U8. PCP value (0-7).
 * @QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID: NLA_U8. TID value (0-7).
 */
enum qca_wlan_vendor_attr_pcp_tid_entry {
	QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_MAX =
		QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_pcp_tid_map - Attributes for PCP-TID mapping
 * vendor commands (QCA_NL80211_VENDOR_SUBCMD_SET/GET_PCP_TID_MAP).
 *
 * @QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE: NLA_NESTED.
 *   Array of qca_wlan_vendor_attr_pcp_tid_entry nested attributes.
 *   Each entry specifies one {PCP, TID} mapping.
 *   Partial updates are supported: only the PCPs present in the message
 *   are updated; the rest retain their current values.
 *   1-8 entries per SET command.
 */

enum qca_wlan_vendor_attr_pcp_tid_map {
	QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX =
		QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_tid_map_precedence - Attributes for TID map
 * precedence vendor commands
 * (QCA_NL80211_VENDOR_SUBCMD_SET/GET_TID_MAP_PRECEDENCE).
 *
 * @QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL: NLA_U8.
 *   Precedence order value (0-11); maps to UMAC_TCL_R0_TID_MAP_PRTY.VAL.
 *   Values 12-15 are reserved by hardware and will be rejected.
 * @QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_TID_DEF: NLA_U8 (optional).
 *   Default TID (0-7) for MSDUs with no valid TID; maps to register [7:5].
 *   If omitted, the existing default TID is preserved.
 */
enum qca_wlan_vendor_attr_tid_map_precedence {
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL,
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_TID_DEF,
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX =
	QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_sdwf_peer_msduq_params - Attributes used in
 * Peer MSDUQ event NL Msg (QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_HW_LINK_ID: u16 attribute
 * Represents hardware link id on which the station is connected.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MAC: 6bytes mac address attribute
 * Represents link mac address of the station.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MLD_MAC: 6bytes mac address attribute
 * Represents mld mac address of the station if it is MLO capable.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_ID: u8 attribute
 * Represents msduq id for which notification is being sent.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_EVENT_TYPE: u8 attribute
 * Represents event type (add/delete/update)
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_ID: u8 attribute
 * Represents service class id.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_TYPE: u8 attribute
 * Represents service class type.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_PRIORITY: u8 attribute
 * Represents service class priority.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_TID: u8 attribute
 * Represents service class traffic identifier (TID).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_AC: u8 attribute
 * Represents service class access category (AC).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MARK_METADATA: u32 attribute
 * Represents mark metadata.
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SERVICE_INTERVAL: u32 attribute
 * Represents service interval (in milliseconds).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_BURST_SIZE: u32 attribute
 * Represents burst size (in bytes).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_DELAY_BOUND: u32 attribute
 * Represents delay bound (in milliseconds).
 *
 * @QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MIN_THROUGHPUT: u32 attribute
 * Represents minimum throughput (in kbps).
 */
enum qca_wlan_vendor_attr_sdwf_peer_msduq_params {
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_HW_LINK_ID,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MAC,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MLD_MAC,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_ID,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_EVENT_TYPE,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_ID,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_TYPE,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SVC_PRIORITY,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_TID,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_AC,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MARK_METADATA,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_SERVICE_INTERVAL,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_BURST_SIZE,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_DELAY_BOUND,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MIN_THROUGHPUT,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_MAX =
		QCA_WLAN_VENDOR_ATTR_SDWF_PEER_MSDUQ_LAST - 1,
};

enum qca_wlan_vendor_sawf_peer_msduq_event_type {
	QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_EVENT_ADD = 0,
	QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_EVENT_DELETE = 1,
	QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_EVENT_UPDATE = 2,
};

enum qca_wlan_vendor_sdwf_peer_msduq_svc_type {
	QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_SVC_TYPE_DL = 0,
	QCA_WLAN_VENDOR_SDWF_PEER_MSDUQ_SVC_TYPE_UL = 1,
};

#define ATH12K_VENDOR_PUT(vendor_event, type, attr, param)             \
	do {                                                            \
		if (nla_put_##type(vendor_event, attr, param)) {        \
			ath12k_err(NULL, "Fails to put " #attr "\n");   \
			return -1;                                      \
		}                                                       \
	} while (0)

#define ATH_PARAM_MASK     0x1000
enum ath_cfg_param_radio {
	ACFG_PARAM_RADIO_TXCHAINMASK	      = 1   | ATH_PARAM_MASK,
	ACFG_PARAM_RADIO_RXCHAINMASK	      = 2   | ATH_PARAM_MASK,
	PARAM_RADIO_TXCHAINSOFT               = 361 | ATH_PARAM_MASK,
	ACFG_PARAM_RADIO_SCAN_BLANKING_MODE   = 525 | ATH_PARAM_MASK,
};

enum qca_wlan_vendor_channel_width
ath12k_nl_chan_bw_to_qca_vendor_chan_bw(enum nl80211_chan_width chan_bw);

int ath12k_vendor_put_ar_hw_link_id(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ar_link_mac_addr(struct sk_buff *vendor_event,
				       struct ath12k *ar);
int ath12k_vendor_put_ar_chan_info(struct sk_buff *vendor_event,
				   struct ath12k *ar);
int ath12k_vendor_put_ar_nss_chains(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ab_soc_id(struct sk_buff *vendor_event,
				struct ath12k_base *ab);
int ath12k_vendor_put_ab_num_links(struct sk_buff *vendor_event,
				   struct ath12k_base *ab,
				   const int num_active_links);
int ath12k_vendor_put_ag_num_socs(struct sk_buff *vendor_event,
				  struct ath12k_hw_group *ag);
void ath12k_vendor_telemetry_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 svc_id, u8 param, bool set_clear,
					   u8 tid, u8 *mld_addr);
void ath12k_vendor_rssi_rate_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 breach_type, u32 threshold_value,
					   u32 detected_value, bool set_clear,
					   u8 *mld_addr);
int ath12k_vendor_send_es_oem_data(struct ieee80211_hw *hw, u8 radio_id, u32 content_type,
				   u32 num_bytes_valid, const u8 *data);
int ath12k_vendor_register(struct ath12k_hw *ah);
int ath12k_vendor_put_umac_migration_notif(struct ieee80211_vif *vif,
					   u8 *mld_addr, u8 link_id);
int ath12k_vendor_ch_switch_reason_notify(struct ath12k *ar,
					  enum qca_wlan_vendor_ch_switch_reason reason,
					  const struct cfg80211_chan_def *old_chandef,
					  const struct cfg80211_chan_def *new_chandef);

/**
 * enum qca_wlan_vendor_attr_tdma_schedule - Attributes for
 * %QCA_NL80211_VENDOR_SUBCMD_TDMA_SCHEDULE_CONFIG.
 *
 * These attributes correspond to the fields of struct tdma_sched_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_RADIO_INDEX: u32. Physical device (pdev)
 * index identifying the radio to which this TDMA schedule applies.
 * Corresponds to tdma_sched_info.pdev_id.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_TYPE: u8. Schedule type selector.
 * Corresponds to tdma_sched_info.sched_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_ID: u8. Schedule instance identifier.
 * Corresponds to tdma_sched_info.sched_id.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID: 6-byte MAC address. BSSID of the
 * BSS to which this schedule is associated.
 * Corresponds to tdma_sched_info.bssid[IEEE80211_ADDR_LEN].
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_START_TIME_TSF_LOW: u32. Lower 32 bits
 * of the TSF timestamp at which the schedule starts.
 * Corresponds to tdma_sched_info.start_time_tsf_low.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_NUM_BUSY_SLOTS: u16. Number of busy
 * (transmission-restricted) slots in the TDMA schedule.
 * Corresponds to tdma_sched_info.num_busy_slots.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_DUR: u16. Duration of each
 * busy slot in milliseconds.
 * Corresponds to tdma_sched_info.busy_slot_dur_ms.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_INTVL: u16. Interval between
 * consecutive busy slots in milliseconds.
 * Corresponds to tdma_sched_info.busy_slot_intvl_ms.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID: flag. When present,
 * indicates that the EDCA parameters (AIFSN, CWmin, CWmax) carried in this
 * command are valid and should be applied.
 * Corresponds to tdma_sched_info.edca_params_valid.
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN: Array of u16 values (one per AC,
 * WLAN_MAX_AC = 4 elements). Arbitration Inter-Frame Space Number per access
 * category. Valid only when %QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID
 * is present.
 * Corresponds to tdma_sched_info.aifsn[WLAN_MAX_AC].
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN: Array of u16 values (one per AC,
 * WLAN_MAX_AC = 4 elements). Minimum contention window per access category.
 * Valid only when %QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID is
 * present.
 * Corresponds to tdma_sched_info.cwmin[WLAN_MAX_AC].
 *
 * @QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX: Array of u16 values (one per AC,
 * WLAN_MAX_AC = 4 elements). Maximum contention window per access category.
 * Valid only when %QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID is
 * present.
 * Corresponds to tdma_sched_info.cwmax[WLAN_MAX_AC].
 */
enum qca_wlan_vendor_attr_tdma_schedule {
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_RADIO_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_TYPE = 2,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_ID = 3,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BSSID = 4,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_START_TIME_TSF_LOW = 5,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_NUM_BUSY_SLOTS = 6,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_DUR = 7,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_BUSY_SLOTS_INTVL = 8,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_EDCA_PARAMS_VALID = 9,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AIFSN = 10,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMIN = 11,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_CWMAX = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_MAX =
		QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_cfr_data_transport_modes - Defines QCA vendor CFR data
 * transport modes and is used by the attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE as a part of the vendor
 * command QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG.
 * @QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS: Use relayfs to send CFR data.
 * @QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS: Use netlink events to send CFR
 * data. The data shall be encapsulated within
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA along with the vendor sub command
 * QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG as an asynchronous event.
 */
enum qca_wlan_vendor_cfr_data_transport_modes {
	QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS = 0,
	QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS = 1,
};

/**
 * enum qca_wlan_vendor_cfr_method - QCA vendor CFR methods used by
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD as part of vendor
 * command QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG.
 * @QCA_WLAN_VENDOR_CFR_METHOD_QOS_NULL: CFR method using QoS Null frame
 * @QCA_WLAN_VENDOR_CFR_QOS_NULL_WITH_PHASE: CFR method using QoS Null frame
 * with phase
 * @QCA_WLAN_VENDOR_CFR_PROBE_RESPONSE: CFR method using Probe Response frame
 */
enum qca_wlan_vendor_cfr_method {
	QCA_WLAN_VENDOR_CFR_METHOD_QOS_NULL = 0,
	QCA_WLAN_VENDOR_CFR_QOS_NULL_WITH_PHASE = 1,
	QCA_WLAN_VENDOR_CFR_PROBE_RESPONSE = 2,
};

/**
 * enum qca_wlan_vendor_cfr_capture_type - QCA vendor CFR capture type used by
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE.
 * @QCA_WLAN_VENDOR_CFR_DIRECT_FTM: Filter directed FTM ACK frames.
 * @QCA_WLAN_VENDOR_CFR_ALL_FTM_ACK: Filter all FTM ACK frames.
 * @QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP: Filter NDPA NDP directed frames.
 * @QCA_WLAN_VENDOR_CFR_TA_RA: Filter frames based on TA/RA/Subtype which
 * is provided by one or more of below attributes:
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER
 * @QCA_WLAN_CFR_ALL_PACKET: Filter all packets.
 * @QCA_WLAN_VENDOR_CFR_NDPA_NDP_ALL: Filter all NDPA NDP frames.
 */
enum qca_wlan_vendor_cfr_capture_type {
	QCA_WLAN_VENDOR_CFR_DIRECT_FTM = 0,
	QCA_WLAN_VENDOR_CFR_ALL_FTM_ACK = 1,
	QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP = 2,
	QCA_WLAN_VENDOR_CFR_TA_RA = 3,
	QCA_WLAN_VENDOR_CFR_ALL_PACKET = 4,
	QCA_WLAN_VENDOR_CFR_NDPA_NDP_ALL = 5,
};


/**
 * enum qca_wlan_vendor_cfr_ltf_type - CSI/LTF type for CFR events.
 *
 * @QCA_WLAN_VENDOR_CFR_LTF_TYPE_HT: CSI from HT-LTF (IEEE 802.11n)
 * @QCA_WLAN_VENDOR_CFR_LTF_TYPE_VHT: CSI from VHT-LTF (IEEE 802.11ac)
 * @QCA_WLAN_VENDOR_CFR_LTF_TYPE_HE: CSI from HE-LTF (IEEE 802.11ax)
 * @QCA_WLAN_VENDOR_CFR_LTF_TYPE_EHT: CSI from EHT-LTF (IEEE 802.11be)
 */
enum qca_wlan_vendor_cfr_ltf_type {
	QCA_WLAN_VENDOR_CFR_LTF_TYPE_HT = 0,
	QCA_WLAN_VENDOR_CFR_LTF_TYPE_VHT = 1,
	QCA_WLAN_VENDOR_CFR_LTF_TYPE_HE = 2,
	QCA_WLAN_VENDOR_CFR_LTF_TYPE_EHT = 3,
	QCA_WLAN_VENDOR_CFR_LTF_TYPE_LEGACY = 4,
};

/**
 * enum qca_wlan_vendor_chip_id - WLAN Chip identifier code.
 */
enum qca_wlan_vendor_chip_id {
	QCA_WLAN_VENDOR_CHIP_ID_WCN3680B = 0,
	QCA_WLAN_VENDOR_CHIP_ID_WCN3910 = 1,
	QCA_WLAN_VENDOR_CHIP_ID_WCN3950 = 2,
	QCA_WLAN_VENDOR_CHIP_ID_WCN3988 = 3,
	QCA_WLAN_VENDOR_CHIP_ID_WCN3991 = 4,
	QCA_WLAN_VENDOR_CHIP_ID_WCN3998 = 5,
	QCA_WLAN_VENDOR_CHIP_ID_QCA639x = 6,
	QCA_WLAN_VENDOR_CHIP_ID_WCN685x = 7,
	QCA_WLAN_VENDOR_CHIP_ID_WCN6750 = 8,
	QCA_WLAN_VENDOR_CHIP_ID_WCN785x = 9,
	QCA_WLAN_VENDOR_CHIP_ID_WCN7750 = 10,
	QCA_WLAN_VENDOR_CHIP_ID_WCN7950 = 11,
	QCA_WLAN_VENDOR_CHIP_ID_WCN7880 = 12,
	QCA_WLAN_VENDOR_CHIP_ID_WCN7881 = 13,
	QCA_WLAN_VENDOR_CHIP_ID_WCN8850 = 14,
};


/**
 * qca_wlan_vendor_cfr_stop_reason - Reason codes for CFR stop indication used
 * by attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_STOP_REASON.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_UNSPEC: Unspecified or unknown reason.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_COMPLETED: CFR collection completed
 * successfully as planned.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_USER_ABORTED: CFR collection stopped
 * explicitly upon userspace abort/stop request.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_PEER_UNAVAILABLE: Peer disconnected or
 * unavailable.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_CONCURRENCY: Stopped to accommodate a
 * higher-priority concurrency operation.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_ROAMING: Stopped due to roaming activity.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_FW_ERROR: Stopped because of a firmware or
 * internal error.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_CHANNEL_SWITCHED: Channel changed (CSA).
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_LINK_SWITCHED: Stopped due to MLO link
 * switch.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_LINK_RECONFIG: Stopped due to MLO link
 * reconfiguration.
 *
 * @QCA_WLAN_VENDOR_CFR_STOP_REASON_RECOVERY: Stopped as part of driver
 * recovery, restart, or assert handling.
 */
enum qca_wlan_vendor_cfr_stop_reason {
	QCA_WLAN_VENDOR_CFR_STOP_REASON_UNSPEC = 0,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_COMPLETED = 1,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_USER_ABORTED = 2,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_PEER_UNAVAILABLE = 3,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_CONCURRENCY = 4,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_ROAMING = 5,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_FW_ERROR = 6,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_CHANNEL_SWITCHED = 7,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_LINK_SWITCHED = 8,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_LINK_RECONFIG = 9,
	QCA_WLAN_VENDOR_CFR_STOP_REASON_RECOVERY = 10,
};

/**
 * enum qca_wlan_vendor_peer_cfr_capture_attr - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG to configure peer
 * Channel Frequency Response capture parameters and enable periodic CFR
 * capture.
 *
 * @QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR: Optional (6-byte MAC address)
 * MAC address of peer. This is for CFR version 1 and in peer CFR event for
 * version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE: Required (flag)
 * Enable peer CFR capture. This attribute is mandatory to enable peer CFR
 * capture. If this attribute is not present, peer CFR capture is disabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH: Optional (u8)
 * BW of measurement, attribute uses the values in enum nl80211_chan_width
 * Supported bandwidth: 20, 40, 80, 80+80, 160, 320.
 * Note that all targets may not support all bandwidths.
 * This attribute is mandatory for version 1 and version 3 if attribute
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 * This attribute is also applicable for peer CFR event with CFR data format
 * version 3
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY: Optional (u32)
 * Periodicity of CFR measurement in milliseconds.
 * Periodicity should be a multiple of Base timer.
 * Current Base timer value supported is 10 milliseconds (default).
 * 0 for one shot capture.
 * This attribute is mandatory for version 1 and optional for version 3 if
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD: Optional (u8)
 * Method used to capture Channel Frequency Response.
 * Attribute uses the values defined in enum qca_wlan_vendor_cfr_method.
 * This attribute is mandatory for version 1 and optional for version 3 if
 * attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE: Optional (flag)
 * Enable periodic CFR capture.
 * This attribute is mandatory for version 1 to enable Periodic CFR capture.
 * If this attribute is not present, periodic CFR capture is disabled.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION: Optional (u8)
 * Value is 1, 2, or 3 since there are three versions of CFR capture.
 * Only one version can be enabled at a time. This attribute is mandatory
 * if the target supports multiple versions and uses one of the versions.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP: Optional (u32)
 * This attribute is mandatory for version 2 if
 * QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY is used.
 * Bits 15:0 bitfield indicates which group is to be enabled.
 * Bits 31:16 Reserved for future use.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION: Optional (u32)
 * CFR capture duration in microsecond. This attribute is mandatory for
 * version 2 if attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL: Optional (u32)
 * CFR capture interval in microsecond. This attribute is mandatory for
 * version 2 if attribute QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION is used.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE: Optional (u32)
 * CFR capture type is defined in enum qca_wlan_vendor_cfr_capture_type.
 * This attribute is mandatory for version 2.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK: Optional (u64)
 * Bitfield indicating which user in the current UL MU transmissions are
 * enabled for CFR capture. Bits 36 to 0 indicate user indexes for 37 users in
 * a UL MU transmission. If bit 0 is set, the CFR capture will happen for user
 * index 0 in the current UL MU transmission. If bits 0 and 2 are set, CFR
 * capture for UL MU TX corresponds to user indices 0 and 2. Bits 63:37 are
 * reserved for future use. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT: Optional (u32)
 * Indicates the number of consecutive RX frames to be skipped before CFR
 * capture is enabled again. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE: Nested attribute containing
 * one or more %QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY attributes.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY: Nested attribute containing
 * the following group attributes:
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER: Optional (u32)
 * Target supports multiple groups for some configurations. The group number
 * can be any value between 0 and 15. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA: Optional (6-byte MAC address)
 * Transmitter address which is used to filter frames. This MAC address takes
 * effect with QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK. This is for CFR
 * version 2 and version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA: Optional (6-byte MAC address)
 * Receiver address which is used to filter frames. This MAC address takes
 * effect with QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK: Optional (6-byte MAC address)
 * Mask of transmitter address which is used to filter frames. This is for CFR
 * version 2 and version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK: Optional (6-byte MAC address)
 * Mask of receiver address which is used to filter frames. This is for CFR
 * version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS: Optional (u32)
 * Indicates frames with a specific NSS will be filtered for CFR capture.
 * This is for CFR version 2 and version 3. This is a bitmask. Bits 7:0 request
 * CFR capture to be done for frames matching the NSS specified within this
 * bitmask.
 * Bits 31:8 are reserved for future use. Bits 7:0 map to NSS:
 *     bit 0 : NSS 1
 *     bit 1 : NSS 2
 *     ...
 *     bit 7 : NSS 8
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW: Optional (u32)
 * Indicates frames with a specific bandwidth will be filtered for CFR capture.
 * This is for CFR version 2 only. This is a bitmask. Bits 4:0 request CFR
 * capture to be done for frames matching the bandwidths specified within this
 * bitmask. Bits 31:5 are reserved for future use. Bits 4:0 map to bandwidth
 * numerated in enum nl80211_band (although not all bands may be supported
 * by a given device).
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER: Optional (u32)
 * Management frames matching the subtype filter categories will be filtered in
 * by MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Management frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. For example, Beacon frame control type
 * is 8 and its value is 1 << 8 = 0x100. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER: Optional (u32)
 * Control frames matching the subtype filter categories will be filtered in by
 * MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Control frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER: Optional (u32)
 * Data frames matching the subtype filter categories will be filtered in by
 * MAC for CFR capture. This is a bitmask in which each bit represents the
 * corresponding Data frame subtype value per IEEE Std 802.11-2016,
 * 9.2.4.1.3 Type and Subtype subfields. This is for CFR version 2 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE: Optional (u8)
 * Userspace can use this attribute to specify the driver about which transport
 * mode shall be used by the driver to send CFR data to userspace. Uses values
 * from enum qca_wlan_vendor_cfr_data_transport_modes. When this attribute is
 * not present, the driver shall use the default transport mechanism which is
 * QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID: Optional (u32)
 * Userspace can use this attribute to specify the nl port id of the application
 * which receives the CFR data and processes it further so that the drivers can
 * unicast the netlink events to a specific application. Optionally included
 * when QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE is set to
 * QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS, not required otherwise. The drivers
 * shall multicast the netlink events when this attribute is not included.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA: Required (NLA_BINARY).
 * This attribute will be used by the driver to encapsulate and send CFR data
 * to userspace along with QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG as an
 * asynchronous event when the driver is configured to send CFR data using
 * netlink events with %QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_PAD: Attribute used for padding for 64-bit
 * alignment.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREQ: Optional (u32)
 * Frequency (in MHz) used for CFR capture.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_TYPE: Optional (u8)
 * IEEE 802.11 WLAN frame type configuration.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SUBTYPE: Optional (u8)
 * IEEE 802.11 WLAN frame subtype configuration.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_INTERVAL: Optional (u32)
 * Interval (in milliseconds) at which CSI reports are generated
 * and delivered to the user.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_OUI: Optional (u8 array, 3 or
 * 5 bytes) Organizationally Unique Identifier (OUI) for the CFR data format.
 * The OUI is assigned by IEEE and uniquely identifies the vendor or
 * organization.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_VERSION: Optional (u8)
 * Version of the CFR data format associated with the specified OUI.
 * If not set, version 1 is used by default.
 * OUI and version together define the vendor-specific format to interpret CFR
 * data.
 * Applicable only for CFR version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_TIMESTAMP_US: Optional (u64)
 * Timestamp (in microseconds) indicating when the packet was received,
 * based on the receiver's internal clock. This value represents the local
 * timing reference for the captured frame.
 * Applicable for peer CFR event and CFR data format version 3 only.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INFO: Optional (nested)
 * Nested attribute containing one or more antenna entries. Each entry is a
 * nested attribute that includes:
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_RSSI,
 *	%QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_AGC,
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX: Optional (u8)
 * Index of the receiving antenna corresponding to each entry.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_RSSI: Optional (s8)
 * RSSI value (in dBm) measured on the antenna specified by
 * %QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_AGC: Optional (u8)
 * AGC (Automatic Gain Control) value in dB for the antenna
 * specified by %QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_IS_LAST_REPORT: Optional (flag)
 * Indicates that this event is the last entry in the current CSI reporting
 * period. This flag is used when %QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_INTERVAL
 * is configured to help the receiver determine the end of a CSI report batch.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SEQUENCE_NUMBER: Optional (u16)
 * Sequence number of the IEEE 802.11 frame (without the fragment number) that
 * triggered the CSI capture.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CHIP_ID: Optional (u16)
 * Vendor-specific chip identifier of the reporting device. Values are defined
 * in enum qca_wlan_vendor_chip_id.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TSF: Optional (u64)
 * TSF (Timing Synchronization Function) timestamp in microseconds.
 * This value is derived at the receiver of the frame by aligning with
 * the TSF provided by the AP through periodic Beacon or Probe Response frames.
 * Applicable for peer CFR events using CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CFO: Optional (s16)
 * Carrier Frequency Offset (in 0.01 ppm) indicating frequency drift between the
 * transmitter and receiver for the captured frame.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CSI_LTF_TYPE: Optional (u8)
 * CSI LTF type for the CFR event. Values are defined by
 * enum qca_wlan_vendor_cfr_ltf_type.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_CSI_NUM_SPATIAL_STREAMS: Optional (u8)
 * Number of spatial streams used to capture the CFR data.
 * Applicable for peer CFR event with CFR data format version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_STOP_REASON: Optional (u32)
 * This attribute signifies that CFR collection for a peer has been stopped
 * and provides the corresponding reason code. The reason codes are defined
 * in enum qca_wlan_vendor_cfr_stop_reason.
 * Applicable for peer CFR events when CFR data format version is 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_FIXED_AGC: Optional (flag)
 * This attribute indicates that the Wi-Fi firmware should fix RX antenna gain
 * during CSI capturing. This is for CFR version 2 and version 3.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_ONLY_LAST_FRAME: Optional (flag)
 * Report only the last captured frame per MAC address in each reporting
 * interval configured by %QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_INTERVAL,
 * i.e., one report per interval per MAC address. When this flag is not
 * included, all captured frames in the reporting interval are reported.
 * Applicable only for CFR version 3.
 */
enum qca_wlan_vendor_peer_cfr_capture_attr {
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR = 1,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE = 2,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH = 3,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY = 4,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD = 5,
	QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE = 6,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION = 7,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP = 8,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION = 9,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL = 10,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE = 11,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK = 12,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT = 13,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE = 14,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY = 15,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER = 16,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA = 17,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA = 18,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK = 19,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK = 20,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS = 21,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW = 22,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER = 23,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER = 24,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER = 25,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE = 26,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID = 27,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA = 28,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_PAD = 29,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREQ = 30,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_TYPE = 31,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SUBTYPE = 32,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_INTERVAL = 33,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_OUI = 34,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_VERSION = 35,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_TIMESTAMP_US = 36,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INFO = 37,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX = 38,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_RSSI = 39,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_AGC = 40,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_IS_LAST_REPORT = 41,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SEQUENCE_NUMBER = 42,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CHIP_ID = 43,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TSF = 44,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CFO = 45,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_CSI_LTF_TYPE = 46,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_NUM_SPATIAL_STREAMS = 47,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_STOP_REASON = 48,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_FIXED_AGC = 49,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_ONLY_LAST_FRAME = 50,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX =
		QCA_WLAN_VENDOR_ATTR_PEER_CFR_AFTER_LAST - 1,
};

/**
 * struct ath12k_sdwf_msduq_evt_data - Per-peer MSDUQ vendor event payload.
 *
 * @hw_link_id:       Hardware link ID of the radio the peer is associated on.
 * @link_mac:         Link MAC address of the peer.
 * @mld_mac:          MLD MAC address (valid only when @mlo is true).
 * @mlo:              True when the peer is an MLO peer.
 * @msduq_id:         MSDUQ identifier within the peer's QoS context.
 * @svc_id:           Service class ID assigned to this MSDUQ.
 * @svc_type:         Service class type (DL=0, UL=1).
 * @priority:         Service class priority.
 * @tid:              Traffic identifier mapped to this MSDUQ.
 * @ac:               Access category mapped to this MSDUQ.
 * @mark_metadata:    SKB mark/metadata value for this flow.
 * @service_interval: Service interval in milliseconds.
 * @burst_size:       Burst size in bytes.
 * @delay_bound:      Delay bound in milliseconds.
 * @min_throughput:   Minimum throughput in kbps.
 * @peer_id:          Peer ID used to look up the peer.
 */
struct ath12k_sdwf_msduq_evt_data {
	u16 hw_link_id;
	u8 link_mac[ETH_ALEN];
	u8 mld_mac[ETH_ALEN];
	bool mlo;
	u8 msduq_id;
	u8 svc_id;
	u8 svc_type;
	u8 priority;
	u8 tid;
	u8 ac;
	u32 mark_metadata;
	u32 service_interval;
	u32 burst_size;
	u32 delay_bound;
	u32 min_throughput;
	u16 peer_id;
};

void
ath12k_vendor_sdwf_msduq_send_event(struct ath12k *ar,
				    const struct ath12k_sdwf_msduq_evt_data *e);
struct ath12k_wmi_nfcal_power_event;
int ath12k_vendor_nfcal_power_event(struct ath12k *ar,
				    const struct ath12k_wmi_nfcal_power_event *param);

/**
 * enum qca_wlan_vendor_multi_bss_param_id - multi bss parameters IDs
 * This will be used by %QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM
 */
enum qca_wlan_vendor_multi_bss_param_id {
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_INVALID = 0,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_BEACON_INT,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_MU_BFMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMEE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SOUNDING_DIM,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_BFMEE_STS,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_MCS_NSS_SET,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMEE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_DL_MU_OFDMA,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_DL_MU_OFDMA_BFER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_UL_MU_OFDMA,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MU_BEAMFORMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_UL_MUMIMO,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_BASIC_MCS_NSS_SET,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_RTS_THRESHOLD,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_SPP_AMSDU,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_TWT_RESPONDER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_6GHZ_MAX_AMPDU_LEN_EXP,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_ER_SU_DISABLE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_BFEE_STS,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MULTI_TID_AGGR,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MULTI_TID_AGGR_RX,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MULTI_TID_AGGR_TX,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MAX_AMPDU_LEN_EXP,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_PPDU_1X_LTF_800NS_GI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_MU_PPDU_4X_LTF_800NS_GI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MAX_FRAG_MSDU,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MIN_FRAG_SIZE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_OMI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_NDP_4X_LTF_3200NS_GI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_FRAGMENTATION,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_AMSDU_IN_AMPDU_SUPRT,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SUBFEE_STS_SUPRT,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MAX_NC_SUPRT,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_ER_SU_PPDU_1X_LTF_800NS_GI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_ER_SU_PPDU_4X_LTF_800NS_GI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_BSR_SUPPORT,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_6GHZ_MIN_RATE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMEE,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_MU_BFMER,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_DL_MU_OFDMA,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_UL_MU_OFDMA,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_DL_OFDMA_MUMIMO,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_UL_OFDMA_MUMIMO,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_80,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_160,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_320,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_LTF,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_ENABLE_MCS15,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_NDP_4X_EHT_LTF_AND_320NSGI,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_RX_1024_AND_4096_QAM_LS_242_TONE_RU,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_DL_OFDMA_TXBF,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SUP_MCS15_IN_MRU,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_MCS14_DUP_IN_6GHZ,
	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_ECSA_IE_ONLY,

	QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_MAX
};

/**
 * enum qca_wlan_vendor_attr_multi_bss_params_info - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_ID: Required (u32)
 *	ID for the parameter being set
 *	(values from &enum qca_wlan_vendor_multi_bss_param_id)
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_1: Required (u32)
 *	Parameter's user configure first value
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_2: Optional (u32)
 *	Parameter's user configure second value if applicable
 */
enum qca_wlan_vendor_attr_multi_bss_params_info {
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_ID = 1,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_1 = 2,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_2 = 3,

	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_set_multi_bss_param - Attributes used by
 * %QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_IFINDEX: Required (u32)
 *	IFINDEX of BSS of reference  BSS and Tx BSS's in case of MBSSID set
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_LINK_ID: Optional (u8)
 *	Link ID of reference BSS or TX BSS's in case of MBSSID set
 *
 * @QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_CONFIGS: Required (u32)
 *	Nested parameter for parameters ID and values
 *	(values from &enum qca_wlan_vendor_attr_multi_bss_params_info)
 */

enum qca_wlan_vendor_attr_set_multi_bss_param {
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_IFINDEX = 1,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_LINKID = 2,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_CONFIGS = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_MAX =
		QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_AFTER_LAST - 1,
};

/**
 * ath12k_vendor_event_chain_mask_changed() - Notify userspace of a chain mask change.
 * @ar: per-radio ath12k context for which the chain mask was modified.
 *
 * Allocates and sends a %QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED vendor
 * event over nl80211 so that userspace applications are notified whenever
 * the TX or RX chain mask is updated
 *
 * Context: Any context. GFP_ATOMIC allocation; safe to call from non-sleepable
 *          paths.
 */
void ath12k_vendor_event_chain_mask_changed(struct ath12k *ar);

/**
 * enum qca_wlan_vendor_rf_path_mode - RF path configuration modes used as
 * values for QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX.
 *
 * @QCA_WLAN_VENDOR_RF_PATH_MODE_5G_FULL_RANGE: Full 5G operating range
 * (4890–5930 MHz, channels 36–177). The radio operates across the complete
 * 5G band.
 *
 * @QCA_WLAN_VENDOR_RF_PATH_MODE_5G_HIGH_RANGE: High 5G operating range
 * (5490–5930 MHz, channels 100–177). The radio operates on the upper
 * portion of the 5G band only.
 */
enum qca_wlan_vendor_rf_path_mode {
	QCA_WLAN_VENDOR_RF_PATH_MODE_5G_FULL_RANGE = 0,
	QCA_WLAN_VENDOR_RF_PATH_MODE_5G_HIGH_RANGE = 1,
};

/**
 * enum qca_wlan_vendor_attr_rf_path_mode - Vendor attributes for
 * QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE.
 *
 * @QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX: u32 attribute.
 * Value is one of enum qca_wlan_vendor_rf_path_mode.
 * SET: desired RF path configuration for all eligible 5G non-6GHz radios.
 * GET (attribute absent in command): driver returns the current active
 *   mode via this attribute in the reply.
 * EVENT: carried in the completion vendor event after a SET attempt,
 *   regardless of success or failure. Indicates the RF path that was
 *   requested. Check QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_STATUS for outcome.
 *
 * @QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_STATUS: u32 attribute.
 * Carried in the completion vendor event after a SET attempt.
 * 0 = switch completed successfully.
 * Non-zero = switch failed; the RF path remains unchanged.
 * Userspace should check this attribute to determine whether the
 * requested RF path switch completed successfully before updating
 * any local state.
 */
enum qca_wlan_vendor_attr_rf_path_mode {
	QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX   = 1,
	QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_STATUS  = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_MAX =
		QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_AFTER_LAST - 1,
};

#endif
