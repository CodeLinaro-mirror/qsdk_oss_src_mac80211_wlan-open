/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_VENDOR_H
#define ATH12K_VENDOR_H

#define QCA_NL80211_VENDOR_ID 0x001374

#define QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE          5000
#define QCA_WLAN_AFC_RESP_DESC_FIELD_START_OCTET       14
#define QCA_WLAN_AFC_RESP_DESC_FIELD_END_OCTET         30
#define ATF_OFFLOAD_MAX_PAYLOAD                                2048

#define INVALID_LINK_ID 0xFF
#define is_valid_link_id(X) !((X) >= INVALID_LINK_ID)

#define INVALID_RADIO_INDEX 0xFF

extern unsigned int ath12k_ppe_ds_enabled;

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

enum qca_nl80211_vendor_subcmds {
	/* Wi-Fi configuration subcommand */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION = 74,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION = 75,
	QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS = 200,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC = 206,
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT = 222,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE = 223,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS = 235,
	QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE = 256,
	QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE = 258,
	QCA_NL80211_VENDOR_SUBCMD_POWER_MODE_CHANGE_COMPLETED = 259,
	QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS = 261,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY = 262,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV = 263,
	QCA_NL80211_VENDOR_SUBCMD_AFC_CLEAR_PAYLOAD = 264,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESET = 265,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD = 267,
	QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION = 268,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION = 269,
};

enum qca_nl80211_vendor_events {
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX = 0,
	QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX = 1,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX = 2,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT = 3,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT = 4,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX = 5,
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
+ */
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
        /* 8-bit unsigned value. Used to specify the HW Radio Index of a wiphy
         * device that is being configured. This attribute may be included in
         * %QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION or
         * %QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION subcmds to
         * specify a particular Radio of the wiphy device.
         */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX = 135,
	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST - 1
};

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

enum qca_wlan_vendor_attr_atf_offload_oper {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_ID = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STATS_CONFIG = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STRICT_SCH_CONFIG = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG = 9,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG = 10,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG = 11,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST - 1
};

enum qca_wlan_vendor_atf_offload_peer_config {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_INVALID = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_NUMBER_OF_PEERS = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_FLAGS = 2,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_PAYLOAD = 3,

	/*keep last */
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_LAST,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_sched_duration {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_INVALID = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_AC = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION = 2,

	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_LAST,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_MAX =
		QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_ssid_scheduling {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_INVALID = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED = 2,

	/*keep last */
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_LAST,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_MAX =
		QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_operations {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_ENABLE_DISABLE = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_GROUP = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_WMM_AC = 2,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER = 3,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_STATS_ENABLE_DISABLE = 4,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_STRICT_SCH = 5,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_VO_TIME = 6,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_VI_TIME = 7,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED = 8,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHEDULING = 9,
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
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP = 13,

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

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_feat_attr {
	QCA_VENDOR_ATTR_WLAN_FEAT_TX = 1,
	QCA_VENDOR_ATTR_WLAN_FEAT_RX,

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

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_WBM_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_MAX_EVENT =
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST - 1,
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

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_comp_err_types_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_DESC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PDEV,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_VIF,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PEER,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_LINK_PEER,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST - 1
};

enum qca_vendor_wlan_telemetry_pkt_info {
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS = 1,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,

	/* keep last */
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_per_pkt_stats_rx_attr {
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO = 1,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,

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
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,

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

	/* keep last */
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX =
		QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_stats {
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST,

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

enum qca_vendor_vdev_param {
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_VDEV_PARAM_TEST,

	/* Add new params above */
	QCA_WLAN_VENDOR_VDEV_PARAM_LAST,
	QCA_WLAN_VENDOR_VDEV_PARAM_MAX = QCA_WLAN_VENDOR_VDEV_PARAM_LAST - 1,
};

enum qca_vendor_radio_param {
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_RADIO_PARAM_TEST,

	/* Add new params above */
	QCA_WLAN_VENDOR_RADIO_PARAM_LAST,
	QCA_WLAN_VENDOR_RADIO_PARAM_MAX = QCA_WLAN_VENDOR_RADIO_PARAM_LAST - 1,
};

int ath12k_vendor_register(struct ath12k_hw *ah);
#endif
