// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
 */
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <net/netlink.h>
#include <net/mac80211.h>
#include "core.h"
#include "debug.h"
#include "mac.h"
#include "ppe.h"
#include "vendor.h"

static const struct nla_policy
ath12k_wifi_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA] = {.type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_cfg80211_afc_response_policy[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA] = { .type = NLA_BINARY,
						 .len = QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE },
};

static void
ath12k_afc_response_buffer_display(struct ath12k_base *ab,
				   struct ath12k_afc_host_resp *afc_rsp)
{
	struct ath12k_afc_bin_resp_data *afc_bin = NULL;
	struct ath12k_afc_resp_freq_psd_info *freq_obj = NULL;
	struct ath12k_afc_resp_opclass_info *opclass_obj = NULL;
	struct ath12k_afc_resp_eirp_info *eirp_obj = NULL;
	u8 *tmp_ptr = NULL;
	int iter, iter_j;

	/* Display the AFC Response Fixed parameters */
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         AFC Response Fixed params\n"
		   "---------------------------\n");
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\nTLV header: %u\nStatus: %u\nTTL: %u\n"
		   "Length: %u\nResponse format: %u\n"
		   "---------------------------\n",
		   afc_rsp->header,
		   afc_rsp->status,
		   afc_rsp->time_to_live,
		   afc_rsp->length,
		   afc_rsp->resp_format);

	afc_bin = (struct ath12k_afc_bin_resp_data *)&afc_rsp->afc_resp[0];

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         Binary fixed\n"
		   "---------------------------\n");
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\nLocal Error code: %u\nVersion: 0x%x\n"
		   "AFC Version: 0x%x\nRequest ID: %u\nDate: 0x%x\n"
		   "Time: 0x%x\nServer resp: %u\n"
		   "Freq objs: %u\nOpclass objs: %u\n"
		   "---------------------------\n",
		   afc_bin->local_err_code,
		   afc_bin->version,
		   afc_bin->afc_wfa_version,
		   afc_bin->request_id,
		   afc_bin->avail_exp_time_d,
		   afc_bin->avail_exp_time_t,
		   afc_bin->afc_serv_resp_code,
		   afc_bin->num_frequency_obj,
		   afc_bin->num_channel_obj);

	/* Display Frequency/PSD info from AFC Response */
	freq_obj = (struct ath12k_afc_resp_freq_psd_info *)
		((u8 *)afc_bin +
		 sizeof(struct ath12k_afc_bin_resp_data));
	tmp_ptr = (u8 *)freq_obj;

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         Freq Info\n"
		   "---------------------------\n");
	for (iter = 0; iter < afc_bin->num_frequency_obj; iter++) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Freq Info[%d]: 0x%x\nMax PSD[%d]: %u\n",
			   iter, freq_obj->freq_info, iter, freq_obj->max_psd);
		freq_obj = (struct ath12k_afc_resp_freq_psd_info *)
			((u8 *)freq_obj +
			 sizeof(struct ath12k_afc_resp_freq_psd_info));
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n");

	/* Display Opclass and channel EIRP info from AFC Response */
	opclass_obj = (struct ath12k_afc_resp_opclass_info *)
		((u8 *)tmp_ptr +
		 (afc_bin->num_frequency_obj *
		  sizeof(struct ath12k_afc_resp_freq_psd_info)));
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "      Opclass Info\n"
		   "---------------------------\n");
	for (iter = 0; iter < afc_bin->num_channel_obj; iter++) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "\nOpclass[%d]: %u\nNum channels[%d]: %u\n",
			   iter, opclass_obj->opclass, iter, opclass_obj->num_channels);

		eirp_obj = (struct ath12k_afc_resp_eirp_info *)
			((u8 *)opclass_obj +
			 sizeof(struct ath12k_afc_resp_opclass_info));
		for (iter_j = 0; iter_j < opclass_obj->num_channels; iter_j++) {
			ath12k_dbg(ab, ATH12K_DBG_AFC,
				   "\nChannel Info[%d]:\nCFI: %u\nEIRP: %u\n\n",
				   iter_j, eirp_obj->channel_cfi, eirp_obj->max_eirp_pwr);
			eirp_obj = (struct ath12k_afc_resp_eirp_info *)
				((u8 *)eirp_obj +
				 sizeof(struct ath12k_afc_resp_eirp_info));
		}

		opclass_obj = (struct ath12k_afc_resp_opclass_info *)
				((u8 *)opclass_obj +
				 sizeof(struct ath12k_afc_resp_opclass_info) +
				 (opclass_obj->num_channels *
				  sizeof(struct ath12k_afc_resp_eirp_info)));
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n");
}

static struct ath12k_afc_host_resp *ath12k_extract_afc_resp(struct ath12k_base *ab,
							    struct nlattr **attr,
							    int *afc_resp_len)
{
	struct ath12k_afc_resp_opclass_info *start_opcls = NULL, *opclass_list = NULL;
	struct nlattr *frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX + 1];
	struct nlattr *opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX + 1];
	struct nlattr *chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX + 1];
	struct ath12k_afc_bin_resp_data *afc_fixed_params = NULL;
	struct ath12k_afc_resp_freq_psd_info *frange_obj = NULL;
	struct ath12k_afc_resp_eirp_info *chan_obj = NULL;
	u16 start_freq = 0, end_freq = 0, nl_len = 0;
	struct ath12k_afc_host_resp *afc_rsp = NULL;
	u32 num_frange_obj = 0, num_channels = 0;
	struct nlattr *nl, *nl_attr;
	u32 total_channels = 0;
	u8 num_opclas_obj = 0;
	int rem, iter, i;
	u8 *temp;

	/* Calculate the total number of Frequency range objects received in the
	 * AFC response
	 */
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO],
				    rem) {
			nl_len = nla_len(nl);
			if (nl_len < QCA_WLAN_AFC_RESP_FREQ_PSD_INFO_INFO_MIN_LEN) {
				ath12k_dbg(ab, ATH12K_DBG_AFC,
					   "Insufficient length %d for Frequency PSD info",
					   nl_len);
				goto fail;
			}
			num_frange_obj++;
		}
	}

	/* Calculate the total number of opclass objects and corresponding number
	 * of channels in each opclass object received in the AFC response
	 */
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO],
				    rem) {
			num_channels = 0;
			nl_len = nla_len(nl);
			if (nl_len < QCA_WLAN_AFC_RESP_OPCLASS_CHAN_EIRP_INFO_MIN_LEN) {
				ath12k_dbg(ab, ATH12K_DBG_AFC,
					   "Insufficient length %d for Opclass/Channel EIRP info",
					   nl_len);
				goto fail;
			}

			if (nla_parse(opclass_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			nla_for_each_nested(nl_attr,
					    opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST],
					    iter) {
				num_channels++;
			}

			num_opclas_obj++;
			total_channels += num_channels;
		}
	}

	/* Calculate the total length required for AFC response
	 * buffer allocation.
	 */
	*afc_resp_len = (sizeof(struct ath12k_afc_host_resp) +
			 sizeof(struct ath12k_afc_bin_resp_data) +
			 (num_frange_obj * sizeof(struct ath12k_afc_resp_freq_psd_info)) +
			 (num_opclas_obj * sizeof(struct ath12k_afc_resp_opclass_info)) +
			 (total_channels * sizeof(struct ath12k_afc_resp_eirp_info)));

	afc_rsp = kzalloc(*afc_resp_len, GFP_KERNEL);

	if (!afc_rsp) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Error allocating buffer for AFC response");
		goto fail;
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]) {
		afc_rsp->time_to_live =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]);
	}

	/* Update the AFC fixed parameters from the AFC response */
	afc_fixed_params = (struct ath12k_afc_bin_resp_data *)afc_rsp->afc_resp;

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]) {
		afc_fixed_params->request_id =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]) {
		afc_fixed_params->avail_exp_time_d =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]) {
		afc_fixed_params->avail_exp_time_t =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]) {
		afc_fixed_params->afc_serv_resp_code =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]);
	}

	/* Update the number of frequency range objects and opclass objects
	 * to the AFC response structure.
	 */
	afc_fixed_params->num_frequency_obj = num_frange_obj;
	afc_fixed_params->num_channel_obj = num_opclas_obj;

	/* Start parsing and updating the frequency range list */
	temp = (u8 *)afc_fixed_params;
	frange_obj =
	(struct ath12k_afc_resp_freq_psd_info *)(temp +
						 sizeof(struct ath12k_afc_bin_resp_data));

	if (!frange_obj)
		goto fail;

	i = 0;
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO]) {
		nla_for_each_nested(nl, attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO],
				    rem) {
			if (nla_parse(frange_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START]) {
				start_freq =
				nla_get_u16(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START]);
			}

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END]) {
				end_freq =
				nla_get_u16(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END]);
			}
			frange_obj[i].freq_info = ((start_freq) | (end_freq << 16));

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD]) {
				frange_obj[i].max_psd =
				nla_get_u32(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD]);
			}

			i++;
		}
	}

	/* Start parsing and updating the opclass list and corresponding channel
	 * and EIRP power information.
	 */
	temp = (u8 *)frange_obj;
	start_opcls = (struct ath12k_afc_resp_opclass_info *)
			(temp + sizeof(struct ath12k_afc_resp_freq_psd_info) *
			 num_frange_obj);
	opclass_list = start_opcls;

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO],
				    rem) {
			if (nla_parse(opclass_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			if (opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS]) {
				opclass_list->opclass =
				nla_get_u8(opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS]);
			}

			temp = (u8 *)opclass_list;
			chan_obj = (struct ath12k_afc_resp_eirp_info *)
					(temp + sizeof(struct ath12k_afc_resp_opclass_info));
			i = 0;
			nla_for_each_nested(nl_attr,
					    opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST],
					    iter) {
				if (nla_parse(chan_info,
					      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX,
					      nla_data(nl_attr),
					      nla_len(nl_attr),
					      NULL, NULL)) {
					goto fail;
				}

				if (chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM]) {
					chan_obj[i].channel_cfi =
					nla_get_u8(chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM]);
				}

				if (chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP]) {
					chan_obj[i].max_eirp_pwr =
					nla_get_s32(chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP]);
				}

				i++;
			}

			opclass_list->num_channels = i;
			temp = (u8 *)chan_obj;
			opclass_list = (struct ath12k_afc_resp_opclass_info *)
					(temp + (sizeof(struct ath12k_afc_resp_eirp_info) *
						 opclass_list->num_channels));
		}
	}

	return afc_rsp;

fail:
	ath12k_dbg(ab, ATH12K_DBG_AFC, "Error parsing the AFC response from application");

	if (!afc_rsp)
		kfree(afc_rsp);

	return NULL;
}

static int ath12k_vendor_receive_afc_response(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1];
	struct ath12k_afc_host_resp *afc_rsp = NULL;
	int afc_resp_len = 0;
	enum ath12k_nl_afc_resp_type afc_resp_format;
	int ret = 0;
	u8 i;

	ar = ah->radio;

	for (i = 0; i < ah->num_radio; i++, ar++)
		if (ar->supports_6ghz)
			break;

	if (!ar)
		return -ENODATA;

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Received AFC response event\n");

	if (!(data && data_len)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX, data, data_len,
		      ath12k_cfg80211_afc_response_policy, NULL)) {
		ath12k_warn(ar->ab,
			    "invalid set afc config policy attribute\n");
		return -EINVAL;
	}

	afc_resp_format = QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP;
	switch (afc_resp_format) {
	case QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP:
		if (tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA]) {
			/* Extract total AFC response buffer length */
			afc_resp_len =
				nla_len(tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA]);

			if (afc_resp_len) {
				/* Memory allocation done to store AFC response
				 * sent by AFC application
				 */
				afc_rsp = kzalloc(afc_resp_len, GFP_KERNEL);
			} else {
				ath12k_warn(ar->ab,
					    "AFC JSON data is not present!");
				return -EINVAL;
			}

			/* Extract the AFC response buffer */
			if (afc_rsp) {
				nla_memcpy((void *)afc_rsp,
					   tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA],
					   afc_resp_len);
			} else {
				ath12k_warn(ar->ab,
					    "Response buffer allocation failed");
				return -EINVAL;
			}

		} else {
			ath12k_warn(ar->ab,
				    "AFC JSON data not found");
			return -EINVAL;
		}
		break;

	case QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP:
		/* The AFC response received from the user space application
		 * is expected to be packed in network byte order(Big endian).
		 * Since q6 is little endian, Host needs to convert the afc
		 * response to little endian format.
		 *
		 * Note: This conversion of data to little endian format is only
		 *       required for Binary type data. For raw JSON data,
		 *       no conversion is required since it is text string.
		 *
		 * Since all the members of the AFC response structure are defined
		 * to be 32-bit words, convert the length appropriately for
		 * conversion to little endian format.
		 */
		afc_rsp = ath12k_extract_afc_resp(ar->ab, tb, &afc_resp_len);

		if (!afc_rsp)
			return -EINVAL;

		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC response extraction successful!\n");

		ath12k_afc_response_buffer_display(ar->ab, afc_rsp);

		break;

	default:
		ath12k_warn(ar->ab, "Invalid response format type %d\n",
			    afc_resp_format);
		ret  = -EINVAL;
		goto exit;
	}

	/* Copy the data buffer to AFC memory location */
	ret = ath12k_copy_afc_response(ar, (char *)afc_rsp, afc_resp_len);
	if (ret)
		goto exit;

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC response copied to AFC memory\n");

	ret = ath12k_wmi_send_afc_resp_rx_ind(ar, afc_resp_format);
	if (ret) {
		ath12k_warn(ar->ab,
			    "AFC Rx indication to FW failed: %d\n", ret);
		goto exit;
	}
	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC Resp RX indication sent to target\n");

exit:
	kfree(afc_rsp);
	return ret;
}

#define nla_nest_end_checked(skb, start) do {           \
	if ((skb) && (start))                           \
		nla_nest_end(skb, start);               \
} while (0)

/**
 * ath12k_afc_power_event_update_or_get_len() - Function to fill vendor event
 * buffer  with AFC power update event or get required vendor buffer length
 * @vendor_event: Pointer to vendor event SK buffer
 * @pwr_evt: Pointer to AFC power event
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event, 0 if success, otherwise negative error code
 */
static int
ath12k_afc_power_event_update_or_get_len(struct ath12k *ar,
					 struct sk_buff *vendor_event,
					 struct ath12k_afc_sp_reg_info *pwr_evt)
{
	struct ath12k_afc_chan_obj *pow_evt_chan_info = NULL;
	struct ath12k_chan_eirp_obj *pow_evt_eirp_info = NULL;
	struct nlattr *nla_attr;
	struct nlattr *freq_info;
	struct nlattr *opclass_info;
	struct nlattr *chan_list;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC power update complete event type put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			    pwr_evt->resp_id)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
		nla_put_u8(vendor_event,
			   QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE,
			   pwr_evt->fw_status_code)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT STATUS CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
		nla_put_s32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE,
			    pwr_evt->serv_resp_code)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT SERVER RESP CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(s32));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE,
			    pwr_evt->avail_exp_time_d)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT EXPIRE DATE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME,
			    pwr_evt->avail_exp_time_t)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT EXPIRE TIME put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event) {
		/* Update the Frequency and corresponding PSD info */
		nla_attr =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < pwr_evt->num_freq_objs; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
			(nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				     pwr_evt->afc_freq_info[i].low_freq) ||
			 nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				     pwr_evt->afc_freq_info[i].high_freq) ||
			 nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD,
				     pwr_evt->afc_freq_info[i].max_psd))) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
				   "AFC FREQUENCY PSD INFO put failed, num %d",
				   pwr_evt->num_freq_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 3;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class, channel list and EIRP info */
		nla_attr =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	pow_evt_chan_info = pwr_evt->afc_chan_info;

	for (i = 0; i < pwr_evt->num_chan_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
			nla_put_u8(vendor_event,
				   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
				   pow_evt_chan_info[i].global_opclass)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
				   "AFC OPCLASS INFO put fail, num %d",
				   pwr_evt->num_chan_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list =
			nla_nest_start(vendor_event,
				       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		pow_evt_eirp_info = pow_evt_chan_info[i].chan_eirp_info;

		for (j = 0; j < pow_evt_chan_info[i].num_chans; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info)
					goto fail;
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    (nla_put_u8(vendor_event,
					QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
					pow_evt_eirp_info[j].cfi) ||
			     nla_put_u32(vendor_event,
				         QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP,
					 pow_evt_eirp_info[j].eirp_power))) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
					   "AFC CHAN EIRP_INFO put fail, num %d",
					   pow_evt_chan_info[i].num_chans);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
				len += nla_total_size(sizeof(u32));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}

	nla_nest_end_checked(vendor_event, nla_attr);

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

static int ath12k_vendor_wifi_config_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	int ppe_vp_type = 0;
	char *type = NULL;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);

	if (ret) {
		pr_err("Invalid attribute with vendor wifi config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE]) {
		ppe_vp_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE]);

	if (ppe_vp_type > PPE_VP_USER_TYPE_DS) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "ppe_vp_type value greater than 4 (%d)(%s)\n",
			   ppe_vp_type, wdev->netdev->name);
		return -EINVAL;
	}

	switch (ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
		type = "passive";
		break;
	case PPE_VP_USER_TYPE_ACTIVE:
		type = "active";
		break;
	case PPE_VP_USER_TYPE_DS:
		type = "ds";
		break;
	default:
		type = "passive";
		ppe_vp_type = 1;
		break;
	}

	if (!ath12k_ppe_ds_enabled) {
		type = "passive";
		ppe_vp_type = 1;
		pr_err("Overriding offload type to passive as DS isn't enabled\n");
	}

	if (wdev->ppe_vp_type != ppe_vp_type)
		wdev->ppe_vp_type = ppe_vp_type;
	else
		return ret;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "vif is NULL\n");
		return ret;
	}
	if (vif->type == NL80211_IFTYPE_AP_VLAN) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "vif is AP_VLAN\n");
		return ret;
	}
	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "ahvif is NULL\n");
		return ret;
	}
	if (ppe_vp_type != ATH12K_INVALID_PPE_VP_TYPE &&
	    ahvif->dp_vif.ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM) {
		ret = ath12k_vif_update_vp_config(ahvif, ppe_vp_type);

		if (ret)
			pr_err("ppe_vp mode config update failed\n");
		else
			wdev->ppe_vp_type = ppe_vp_type;
	}

	pr_info("[%s] vendor cmd type [%s] %d (%s) state %d\n",
		current->comm,  wdev->netdev->name, wdev->ppe_vp_type,
		type, netif_running(wdev->netdev));
	}

	return 0;
}

static int ath12k_vendor_get_wifi_config_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct sk_buff *skb;
	int ret;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	vif = wdev_to_ieee80211_vif_vlan(wdev, false);
	if (!vif) {
		ret = -EINVAL;
		goto err;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ret = -EINVAL;
		goto err;
	}

//	wiphy_lock(ahvif->ah->hw->wiphy);
	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE, ahvif->dp_vif.ppe_vp_type)) {
		wiphy_unlock(ahvif->ah->hw->wiphy);
		ret = -EINVAL;
		goto err;
	}
//	wiphy_unlock(ahvif->ah->hw->wiphy);

	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		pr_err("offload type send failed with err=%d\n", ret);
		return ret;
	}

	return 0;

err:
	pr_err("get offload type failed with err=%d\n", ret);
	kfree_skb(skb);
	return ret;
}

static
int ath12k_vendor_trigg_pri_link_migrate(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_mac_link_migrate_usr_params arg;
	u8 mac_addr[ETH_ALEN] = {0};
	struct ath12k_vif *ahvif;
	u8 link_id;
	int ret;

	if (WARN_ON(!vif))
		return -EINVAL;

	/* 1 byte of link ID or(and) 6 bytes of mac address */
	if (data_len != 1 && data_len != ETH_ALEN + 1)
		return -EINVAL;

	/* not supported in case of non-ML vif */
	if (!vif->valid_links)
		return -EOPNOTSUPP;

	/* get link ID */
	link_id = *(u8 *)data;

	/* get mac address if it is provided */
	if (data_len == ETH_ALEN + 1) {
		data++;
		memcpy(mac_addr, data, ETH_ALEN);
	}

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "primary link migration command received link_id %u, mac_addr %pM",
		   link_id, mac_addr);

	arg.link_id = link_id;
	memcpy(arg.addr, mac_addr, ETH_ALEN);

	mutex_lock(&ahvif->ah->hw_mutex);
	ret = ath12k_mac_process_link_migrate_req(ahvif, &arg);
	mutex_unlock(&ahvif->ah->hw_mutex);

	if (ret)
		ath12k_info(NULL,
			    "Failed to trigger primary link migration command\n");

	return ret;
}

int ath12k_vendor_send_power_update_complete(struct ath12k *ar)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = ar->afc.afc_reg_info;
	struct ath12k_link_vif *tmp_arvif = NULL, *arvif;
	struct sk_buff *vendor_event;
	struct wireless_dev *wdev;
	int vendor_buffer_len;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!tmp_arvif && arvif->is_started) {
			tmp_arvif = arvif;
			break;
		}
	}

	if (!tmp_arvif || !tmp_arvif->ahvif)
		return -EINVAL;

	wdev = ieee80211_vif_to_wdev(tmp_arvif->ahvif->vif);
	if (!wdev)
		return -EINVAL;

	vendor_buffer_len =
		ath12k_afc_power_event_update_or_get_len(ar, NULL,
							 afc_reg_info);

	vendor_event =
	cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, wdev, vendor_buffer_len,
				    QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
				    GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab,
			    "failed to allocate skb for afc expiry event\n");
		return -ENOMEM;
	}

	if (ath12k_afc_power_event_update_or_get_len(ar, vendor_event,
						     afc_reg_info)) {
		ath12k_warn(ar->ab, "Failed to update AFC power vendor event\n");
		goto fail;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending AFC update complete event to user application");
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	return 0;

fail:
	kfree_skb(vendor_event);
	return -EINVAL;
}

static struct wiphy_vendor_command ath12k_vendor_commands[] = {
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		.doit = ath12k_vendor_wifi_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
		.doit = ath12k_vendor_get_wifi_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE,
		.doit = ath12k_vendor_trigg_pri_link_migrate,
		.policy = VENDOR_CMD_RAW_DATA,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE,
		.doit = ath12k_vendor_receive_afc_response,
		.policy = ath12k_cfg80211_afc_response_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_MAX
	},
};

static const struct nl80211_vendor_cmd_info ath12k_vendor_events[] = {
	[QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT,
	},
};

int ath12k_vendor_register(struct ath12k_hw *ah)
{
	ah->hw->wiphy->vendor_commands = ath12k_vendor_commands;
	ah->hw->wiphy->n_vendor_commands = ARRAY_SIZE(ath12k_vendor_commands);
	ah->hw->wiphy->vendor_events = ath12k_vendor_events;
	ah->hw->wiphy->n_vendor_events = ARRAY_SIZE(ath12k_vendor_events);
	return 0;
}
