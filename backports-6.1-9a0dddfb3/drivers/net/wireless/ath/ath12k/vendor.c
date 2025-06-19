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
#include "telemetry.h"

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
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA] = { .type = NLA_BINARY,
						 .len = QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE },
};

static const struct nla_policy
ath12k_atf_offload_config_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_CONFIG] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG] = {.type = NLA_BINARY,
								.len = ATF_OFFLOAD_MAX_PAYLOAD},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG] = {.type = NLA_BINARY,
							    .len = ATF_OFFLOAD_MAX_PAYLOAD},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG] = {.type = NLA_BINARY,
							  .len = ATF_OFFLOAD_MAX_PAYLOAD},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STATS_CONFIG] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STRICT_SCH_CONFIG] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_atf_offload_peer_config_policy[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_NUMBER_OF_PEERS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_FLAGS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_PAYLOAD] = {.type = NLA_BINARY},
};

static const struct nla_policy
ath12k_vendor_atf_offload_sched_duration_policy[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_AC] =  {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION] = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_vendor_atf_offload_ssid_sched_policy[QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED] = {.type = NLA_U8},
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
	int ret = 0, hw_idx = -1;
	u8 i;

	ath12k_dbg(NULL, ATH12K_DBG_AFC, "Received AFC response event\n");

	if (!(data && data_len)) {
		ath12k_dbg(NULL, ATH12K_DBG_AFC,
			   "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX, data, data_len,
		      ath12k_cfg80211_afc_response_policy, NULL)) {
		ath12k_dbg(NULL, ATH12K_DBG_AFC,
			    "invalid set afc config policy attribute\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX]) {
		hw_idx = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX]);
		if (hw_idx >= ah->num_radio) {
			ath12k_dbg(NULL, ATH12K_DBG_AFC, "Invalid hw_idx attribute\n");
			ret = -EINVAL;
			goto out;
		}

		ar = &ah->radio[hw_idx];
	} else {
		ar = ah->radio;
		for (i = 0; i < ah->num_radio; i++, ar++)
			if (ar->supports_6ghz)
				break;
	}

	if (!ar) {
		ath12k_err(NULL, "ar is NULL \n");
		ret = -ENODATA;
		goto out;
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
				ret = -EINVAL;
				goto out;
			}

			/* Extract the AFC response buffer */
			if (afc_rsp) {
				nla_memcpy((void *)afc_rsp,
					   tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA],
					   afc_resp_len);
			} else {
				ath12k_warn(ar->ab,
					    "Response buffer allocation failed");
				ret = -EINVAL;
				goto out;
			}

		} else {
			ath12k_warn(ar->ab,
				    "AFC JSON data not found");
			ret = -EINVAL;
			goto out;
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

		if (!afc_rsp) {
			ret = -EINVAL;
			goto out;
		}

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
out:
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
	u8 hw_idx;

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

	hw_idx = ieee80211_get_radio_idx_by_freq(ar->ah->hw->wiphy,
					      ar->freq_range.start_freq);
	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX, hw_idx)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "hw_idx put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

/**
 * ath12k_afc_expiry_event_update_or_get_len() - Function to fill vendor evt buffer
 * with info extracted from AFC request, or get required vendor buffer length.
 * @ar - Pointer to ath12k structure
 * @vendor_event: Pointer to vendor event SK buffer structure
 * @afc_req: Pointer to AFC request from regulatory component
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event  0 if success, otherwise negative error code
 */
static int
ath12k_afc_expiry_event_update_or_get_len(struct ath12k *ar,
					  struct sk_buff *vendor_event,
					  struct ath12k_afc_host_request *afc_req)
{
	struct nlattr *nla_attr;
	struct nlattr *freq_info;
	struct nlattr *opclass_info = NULL;
	struct nlattr *chan_list = NULL;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;
	struct ath12k_afc_opclass_obj *afc_opclass_obj;
	u8 hw_idx;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			afc_req->req_id)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_ATTR_AFC_REQ_ID put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION,
			(afc_req->version_major << 16) |
			afc_req->version_minor)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EVENT WFA version put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER,
			afc_req->min_des_power)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_ATTR_AFC_REQ_MIN_DES_PWR put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u16));
	}

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT,
		       afc_req->afc_location->deployment_type)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EVENT AP deployment put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event) {
		/* Update the frequency range list from the Expiry event */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC FREQ RANGE LIST start put fail\n");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->freq_lst->num_ranges; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put freq list nest %d\n",
					   i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    (nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				 afc_req->freq_lst->range_objs[i].lowfreq) ||
		     nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				 afc_req->freq_lst->range_objs[i].highfreq))) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC REQ FREQ RANGE LIST put fail, num %d\n",
				   afc_req->freq_lst->num_ranges);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 2;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class and channel list */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS CHAN LIST start put fail\n");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->opclass_obj_lst->num_opclass_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put opclass nest %d\n",
					   i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		afc_opclass_obj = &afc_req->opclass_obj_lst->opclass_objs[i];

		if (vendor_event &&
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
			       afc_opclass_obj->opclass)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS INFO OPCLASS put fail, num %d\n",
				   afc_req->opclass_obj_lst->num_opclass_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list = nla_nest_start(vendor_event,
						   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS INFO CHAN LIST start put fail\n");
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		for (j = 0; j < afc_opclass_obj->opclass_num_cfis; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info) {
					ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put opclass cfis nest %d\n",
						   j);
					goto fail;
				}
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    nla_put_u8(vendor_event,
				       QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
				       afc_opclass_obj->cfis[j])) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EIRP INFO CHAN NUM put fail, num %d\n",
					   afc_opclass_obj->opclass_num_cfis);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	hw_idx = ieee80211_get_radio_idx_by_freq(ar->ah->hw->wiphy,
					      ar->freq_range.start_freq);
	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX, hw_idx)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "hw_idx put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

/**
 * afc_payload_reset_evt_get_data_len: Get the AFC payload resent event data
 * length.
 *
 * Return: Data length.
 */
static int afc_payload_reset_evt_get_data_len(void)
{
	u32 len = NLMSG_HDRLEN;

	/* Size reserved for event type and HW index */
	len += nla_total_size(sizeof(u8)) + nla_total_size(sizeof(u32));
	len = nla_total_size(len);

	return len;
}

int ath12k_send_afc_payload_reset(struct ath12k *ar)
{
	struct sk_buff *vendor_event;
	int ret = -EINVAL;
	int vendor_buffer_len, hw_index;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *tmp_arvif = NULL, *arvif;
	struct wireless_dev *wdev;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started) {
			tmp_arvif = arvif;
			break;
		}
	}

	if (!tmp_arvif || !tmp_arvif->ahvif) {
		ath12k_warn(ar->ab, "Unable to send AFC payload reset event, no vif started\n");
		goto out;
	}

	wdev = ieee80211_vif_to_wdev(tmp_arvif->ahvif->vif);
	/* Hostapd application is a consumer of this afc payload reset event, without
	 * the presence of the vif, it cannot take any action on the received payload
	 * reset event. Hence, send this event only when a vif is present.
	 */
	if (!wdev) {
		ath12k_warn(ar->ab, "Unable to send AFC payload reset event, no wdev\n");
		goto out;
	}

	hw_index = ieee80211_get_radio_idx_by_freq(ar->ah->hw->wiphy, ar->freq_range.start_freq);
	if (hw_index == -1) {
		ath12k_err(ab, "Failed to get hw index for freq %d\n",
			   ar->freq_range.start_freq);
		goto out;
	}

	vendor_buffer_len = afc_payload_reset_evt_get_data_len();
	vendor_event = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
						   wdev,
						   vendor_buffer_len,
						   QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
						   GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "failed to allocate skb for afc expiry event\n");
		goto out;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET)) {
		ath12k_warn(ar->ab, "AFC payload reset complete event type put fail");
		goto out;
	}
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX,
			hw_index)) {
		ath12k_warn(ar->ab, "AFC payload reset complete event hw index put fail");
		goto out;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending afc payload reset to higher layer of type %d, hw_index: %d\n",
		   QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT, hw_index);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	ret = 0;

out:
	return ret;
}

int ath12k_send_afc_request(struct ath12k *ar, struct ath12k_afc_host_request *afc_req)
{
	struct sk_buff *vendor_event;
	int vendor_buffer_len;
	int ret;

	if (!afc_req) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC Host request is NULL\n");
		return -EINVAL;
	}

	vendor_buffer_len = ath12k_afc_expiry_event_update_or_get_len(ar, NULL, afc_req);
	vendor_event = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
						   NULL,
						   vendor_buffer_len,
						   QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
						   GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "failed to allocate skb for afc expiry event\n");
		goto out;
	}

	ret = ath12k_afc_expiry_event_update_or_get_len(ar, vendor_event, afc_req);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to update AFC request vendor event\n");
		goto out;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending expiry event to higher layer of type %d\n",
		   QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
out:
	return ret;
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

int
ath12k_vendor_send_power_update_complete(struct ath12k *ar,
					 struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
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

static const struct nla_policy
ath12k_cfg80211_power_mode_set_policy[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE] = { .type = NLA_U8 },
};

static struct ath12k *ath12k_get_ar_from_wdev(struct wireless_dev *wdev, u8 link_id)
{
	struct ieee80211_vif *vif =  NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ieee80211_hw *hw = NULL;
	struct ath12k *ar = NULL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	hw = ahvif->ah->hw;
	if (!hw) {
		return NULL;
	}

	ar = ath12k_get_ar_by_vif(hw, vif, link_id);

	return ar;
}

static int ath12k_vendor_6ghz_power_mode_change(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct ath12k *ar;
	u8 link_id = 0;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE + 1];
	u8 ap_6ghz_pwr_mode;
	int err;

	if (!wdev)
		return -EINVAL;

	if (wdev->iftype != NL80211_IFTYPE_AP)
		return -EOPNOTSUPP;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE, data,
		      data_len, ath12k_cfg80211_power_mode_set_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE parsing failed");
		return -EINVAL;
	}

	for_each_valid_link(wdev, link_id) {
		if (!wdev->links[link_id].ap.beacon_interval)
			continue;

		if (wdev->links[link_id].ap.chandef.chan &&
		    wdev->links[link_id].ap.chandef.chan->band ==
		    NL80211_BAND_6GHZ)
			break;
	}

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	if (!tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE])
		return -EINVAL;

	ap_6ghz_pwr_mode =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE]);

	if (ap_6ghz_pwr_mode < QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI ||
	    ap_6ghz_pwr_mode > QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP) {
		ath12k_err(NULL, "Invalid 6 GHZ pwr mode configuration");
		return -EINVAL;
	}

	err = ieee80211_6ghz_power_mode_change(wiphy, wdev,
					       ap_6ghz_pwr_mode, link_id);

	return err;
}

int ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						       struct wireless_dev *wdev)
{
	struct sk_buff *vendor_event;
	int ret = 0;
	int vendor_buffer_len = nla_total_size(sizeof(u8));
	u8 ap_power_mode = wdev->reg_6g_power_mode;

	/* NOTE: lockdep_assert_held is called in ath12k_mac_bss_info_changed */
	vendor_event =
	cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, wdev, vendor_buffer_len,
				    QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX,
				    GFP_KERNEL);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "SKB alloc failed for 6 GHz power mode evt\n");
		goto out;
	}

	ret = nla_put_u8(vendor_event,
			 QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE,
			 ap_power_mode);

	if (ret) {
		ath12k_warn(ar->ab, "6 GHZ power mode vendor evt failed\n");
		goto out;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Send power mode update complete event\n");
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
out:
	return ret;
}

static const struct nla_policy
ath12k_vendor_sdwf_phy_policy[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_sdwf_svc_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_samples_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_thershold_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_detect_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS] = {.type = NLA_U32},
};

static int ath12k_vendor_set_sdwf_config(struct ath12k_base *ab,
					 struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 struct nlattr *svc_params)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	struct ath12k_qos_params param_dl = {0};
	struct ath12k_qos_params param_ul = {0};
	bool ul_params = false;
	bool dl_params = false;
	int ret = 0;
	u16 qos_id_dl = QOS_ID_INVALID;
	u16 qos_id_ul = QOS_ID_INVALID;
	u16 svc_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
				svc_params,
				ath12k_vendor_sdwf_svc_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attribute with SDWF configure command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
	} else {
		ath12k_err(ab, "Mandatory attributes not available\n");
		return -EINVAL;
	}

	/* Check if DL prams exists */
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS]){
		dl_params = true;
	}

	/* Check if UL prams exists */
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE] \
	   || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY] \
	   || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE]){
		ul_params = true;
	}

	/* Get the required params */

	if (dl_params) {
		ath12k_qos_set_default(&param_dl);

		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP])
			param_dl.min_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP])
			param_dl.mean_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE])
			param_dl.burst_size = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL])
			param_dl.min_service_interval = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND])
			param_dl.delay_bound = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL])
			param_dl.msdu_life_time = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO])
			param_dl.priority = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID])
			 param_dl.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS])
			param_dl.msdu_delivery_info = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS]);

		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_dl = ath12k_qos_configure(ab, NULL,
							 &param_dl,
							 QOS_PROFILE_DL,
							 NULL);
			if (qos_id_dl == QOS_ID_INVALID) {
				ath12k_err(ab, "Unable to configure DL QoS profile(SVC__ID:%d)", svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_dl = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
			if (qos_id_dl != QOS_ID_INVALID) {
				ath12k_qos_update(ab, NULL, &param_dl,
						  QOS_PROFILE_DL,
						  qos_id_dl, NULL);
			} else {
				qos_id_dl = ath12k_qos_configure(ab, NULL,
								 &param_dl,
								 QOS_PROFILE_DL,
								 NULL);
			}
		}
	}

	if (ul_params) {
		ath12k_qos_set_default(&param_ul);

		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL])
			param_ul.min_service_interval = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE])
			param_ul.burst_size = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT])
			param_ul.min_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY])
			param_ul.delay_bound = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE])
			param_ul.ul_ofdma_disable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE])
			param_ul.ul_mu_mimo_disable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID])
			 param_ul.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);

		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_ul = ath12k_qos_configure(ab, NULL,
							 &param_ul,
							 QOS_PROFILE_UL,
							 NULL);
			if (qos_id_ul == QOS_ID_INVALID) {
				ath12k_err(ab, "Config Failed Svc ID:%d",
					   svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_ul = ath12k_sdwf_get_ul_qos_id(ab,
							      svc_id);
			if (qos_id_ul != QOS_ID_INVALID)
				ath12k_qos_update(ab, NULL, &param_ul,
						  QOS_PROFILE_UL,
						  qos_id_ul, NULL);
			else
				qos_id_ul = ath12k_qos_configure(ab, NULL,
								 &param_ul,
								 QOS_PROFILE_UL,
								 NULL);
		}
	}

	/* Create a DL only service class if only TID is specified */
	if (!ul_params && !dl_params && tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]) {
		ath12k_qos_set_default(&param_dl);
		param_dl.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);
		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_dl = ath12k_qos_configure(ab, NULL,
							 &param_dl,
							 QOS_PROFILE_DL,
							 NULL);
			if (qos_id_dl == QOS_ID_INVALID) {
				ath12k_err(ab, "Unable to configure DL QoS profile(SVC__ID:%d)",
					   svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_dl = ath12k_sdwf_get_dl_qos_id(ab,
							      svc_id);
			if (qos_id_dl != QOS_ID_INVALID)
				ath12k_qos_update(ab, NULL, &param_dl,
						  QOS_PROFILE_DL, qos_id_dl,
						  NULL);
			else
				qos_id_dl = ath12k_qos_configure(ab, NULL,
								 &param_dl,
								 QOS_PROFILE_DL,
								 NULL);
		}
	}

	ath12k_sdwf_map_service_class(ab, svc_id, qos_id_dl, qos_id_ul);

	return 0;
}

static int ath12k_vendor_disable_sdwf_config(struct ath12k_base *ab,
					     struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     struct nlattr *svc_params)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	u8 svc_id = 0;
	int ret = 0;
	u16 dl_qos_id;
	u16 ul_qos_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
			       svc_params,
			       ath12k_vendor_sdwf_svc_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attributes with SDWF disable command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
	} else {
		ath12k_err(ab, "Mandatory attribute not available\n");
		return -EINVAL;
	}

	if (!ath12k_sdwf_service_configured(ab, svc_id)) {
		ath12k_err(ab, "Service Class %d is not configured\n",
			   svc_id);
		return -EINVAL;
	}

	dl_qos_id = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
	if (dl_qos_id != QOS_ID_INVALID)
		ret = ath12k_qos_disable(ab, NULL,
					 QOS_PROFILE_DL,
					 dl_qos_id,
					 NULL);

	ul_qos_id = ath12k_sdwf_get_ul_qos_id(ab, svc_id);
	if (ul_qos_id != QOS_ID_INVALID)
		ret = ath12k_qos_disable(ab, NULL,
					 QOS_PROFILE_UL,
					 ul_qos_id,
					 NULL);

	ret = ath12k_sdwf_unmap_service_class(ab, svc_id);

	return ret;
}

static int ath12k_vendor_view_sdwf_config(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct sk_buff *msg,
					  const void *data,
					  int data_len,
					  unsigned long *storage)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah;
	struct ath12k_base *ab;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];
	struct nlattr *svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_qos *profile;
	struct nlattr *svc_classes, *svc_class;
	int ret = 0, i, j = 0;
	int tailroom = 0, nest_start_length = 0;
	int nest_end_length = 0, nested_range = 0;
	u8 svc_id = 0;

	ah = ath12k_hw_to_ah(hw);
	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return -EINVAL;
	}

	ab = ar->ab;

	if (!ab) {
		ath12k_err(NULL, "ab is NULL");
		return -EINVAL;
	}

	if (!storage)
		return -ENODATA;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS context not valid");
		return -ENODATA;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX, data, data_len,
			ath12k_vendor_sdwf_phy_policy, NULL);

	if (ret) {
		ath12k_err(ab, "Invalid attr with SDWF cmd");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION] &&
	    nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]) == QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_GET) {
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = nla_parse_nested(svc, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
					       tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS],
					       ath12k_vendor_sdwf_svc_policy, NULL);
			if (ret) {
				ath12k_err(ab, "Invalid attr SDWF view cmd");
				return -EINVAL;
			}
			if (svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {

				svc_id = nla_get_u8(svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
				if (!ath12k_sdwf_service_configured(ab, svc_id)) {
					ath12k_err(ab, "Invalid Svc ID: %d",
						   svc_id);
					return -EINVAL;
				}
			}
		}
	} else {
		ath12k_err(ab, "Invalid attribute with SDWF view command\n");
		return -EINVAL;
	}
	/* return 0 to end the dump */
	if (*storage == QOS_PROFILES_MAX)
		return 0;

	svc_classes = nla_nest_start(msg,
				     QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS);

	if (!svc_classes)
		return -ENOBUFS;

	tailroom = skb_tailroom(msg);
	for (i = (svc_id);
	     i < QOS_PROFILES_MAX && tailroom > nested_range;
	     i += (svc_id) ? (QOS_PROFILES_MAX) : (1)) {
		u16 id_dl = qos_ctx->svc_class[i].dl_qos_id;
		u16 id_ul = qos_ctx->svc_class[i].ul_qos_id;

		if (!ath12k_sdwf_service_configured(ab, i))
			continue;

		profile = &qos_ctx->profiles[id_dl];
		nest_start_length = msg->len;
		svc_class = nla_nest_start(msg, j);
		if (!svc_class)
			goto nla_put_failure;

		if ((id_dl != QOS_ID_INVALID) &&
		    (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID,
			       i) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP,
				profile->params.min_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP,
				profile->params.mean_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE,
				profile->params.burst_size) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL,
				profile->params.min_service_interval) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND,
				profile->params.delay_bound) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL,
				profile->params.msdu_life_time) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO,
				 profile->params.priority) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID,
				profile->params.tid) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS,
				profile->params.msdu_delivery_info)
		    ))
			goto nla_put_failure;

		profile = &qos_ctx->profiles[id_ul];
		if ((id_ul !=  QOS_ID_INVALID) &&
		    (nla_put_u8(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID, i) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL,
				profile->params.min_service_interval) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT,
				profile->params.min_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY,
				profile->params.delay_bound) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE,
				profile->params.burst_size) ||
		    nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE,
			       profile->params.ul_ofdma_disable) ||
		    nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE,
			       profile->params.ul_mu_mimo_disable)
		))
			goto nla_put_failure;

		nest_end_length = nla_nest_end(msg, svc_class);
		nested_range = nest_end_length - nest_start_length;
		tailroom -= nested_range;
		j++;
	}
	nla_nest_end(msg, svc_classes);

	*storage = (svc_id) ? (QOS_PROFILES_MAX) : (i);

	if (!j)
		return 0;

	return msg->len;

nla_put_failure:
	return -ENOBUFS;
}

static int ath12k_vendor_telemetry_sdwf_sla_samples_config(struct nlattr *sla_samples)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX + 1];
	struct ath12k_sla_samples_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX,
			       sla_samples,
			       ath12k_vendor_telemetry_sdwf_sla_samples_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid set telemetry sla samples config policy attribute\n");
		return ret;
	}

	t_param.moving_avg_pkt =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT]);
	t_param.moving_avg_win =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN]);
	t_param.sla_num_pkt =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT]);
	t_param.sla_time_sec =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC]);

	ret = ath12k_telemetry_sdwf_sla_samples_config(t_param);
	return ret;
}

static int ath12k_vendor_telemetry_sdwf_sla_thershold_config(struct nlattr *sla_threshold)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX + 1];
	struct ath12k_sla_thershold_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX,
			       sla_threshold,
			       ath12k_vendor_telemetry_sdwf_sla_thershold_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid telemetry sla thershold config policy attribute\n");
		return ret;
	}

	t_param.svc_id =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID]);
	t_param.min_throughput_rate =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP]);
	t_param.max_throughput_rate =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP]);
	t_param.burst_size =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE]);
	t_param.service_interval =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL]);
	t_param.delay_bound =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND]);
	t_param.msdu_ttl =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL]);
	t_param.msdu_rate_loss =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS]);

	ret = ath12k_telemetry_sdwf_sla_thershold_config(t_param);

	return ret;
}

static int ath12k_vendor_telemetry_sdwf_sla_detection_config(struct nlattr *sla_detect)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX + 1];
	struct ath12k_sla_detect_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX,
			       sla_detect,
			       ath12k_vendor_telemetry_sdwf_sla_detect_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid telemetry sdwf sla detection config policy attribute\n");
		return ret;
	}

	t_param.sla_detect =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM]);
	t_param.min_throughput_rate =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP]);
	t_param.max_throughput_rate =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP]);
	t_param.burst_size =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE]);
	t_param.service_interval =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL]);
	t_param.delay_bound =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND]);
	t_param.msdu_ttl =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL]);
	t_param.msdu_rate_loss =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS]);

	ret = ath12k_telemetry_sdwf_sla_detection_config(t_param);

	return ret;
}

static int ath12k_vendor_sdwf_phy_operations(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah;
	struct ath12k_base *ab;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];
	u8 sdwf_oper;
	int ret = 0;

	ah = ath12k_hw_to_ah(hw);
	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return -EINVAL;
	}

	ab = ar->ab;
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX, data, data_len,
			ath12k_vendor_sdwf_phy_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attr with SDWF radio commands");
		goto end;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]) {
		sdwf_oper = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]);
	} else {
		ath12k_err(ab, "SDWF radio level operation missing");
		ret = -EINVAL;
		goto end;
	}

	switch (sdwf_oper) {
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = ath12k_vendor_set_sdwf_config(ab, wiphy, wdev,
							    tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]);
		} else {
			ath12k_err(ab, "SDWF svc parameters missing");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_DEL:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = ath12k_vendor_disable_sdwf_config(ab, wiphy, wdev, tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]);
		} else {
			ath12k_err(ab, "SDWF service id missing with delete");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_SAMPLES_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_samples_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla samples parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_BREACH_DETECTION_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_detection_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla breach detect parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_THRESHOLD_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_thershold_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla threshnew parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	default:
		ath12k_err(ab, "Invalid operation with SDWF radio commands");
		ret = -EINVAL;
	}
end:
	return ret;
}

static int ath12k_vendor_atf_offload_ssid_grouping_config(struct ath12k *ar,
							  struct nlattr **tb)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	void *buf, *ptr;
	u32 buf_len, len, pdev_id;
	int ret;
	struct wmi_atf_ssid_grp_request_fixed_param *cmd;

	len = sizeof(*cmd);

	pdev_id = ar->pdev->pdev_id;

	buf = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG]);
	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG]);
	if (!buf_len) {
		ath12k_warn(ar->ab, "No data present in ATF SSID group config command\n");
		return -EINVAL;
	}
	len += buf_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_atf_ssid_grp_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_SSID_GRP_REQUEST_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);
	memcpy(skb->data, buf, buf_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF SSID group config for pdev id %u\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ATF_SSID_GROUPING_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_ATF_SSID_GROUPING_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int ath12k_vendor_atf_offload_wmm_ac_config(struct ath12k *ar,
						   struct nlattr **tb)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	void *buf, *ptr;
	u32 buf_len, len, pdev_id;
	int ret;
	struct wmi_atf_ssid_grp_request_fixed_param *cmd;

	len = sizeof(*cmd);

	pdev_id = ar->pdev->pdev_id;

	buf = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]);
	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]);
	if (!buf_len) {
		ath12k_warn(ar->ab, "No data present in ATF WMM AC config command\n");
		return -EINVAL;
	}
	len += buf_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_atf_ssid_grp_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_GRP_WMM_AC_CFG_REQUEST_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);
	memcpy(skb->data, buf, buf_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF WMM ac config for pdev id %u\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int ath12k_vendor_atf_offload_peer_config(struct ath12k *ar,
						 struct nlattr *peer_config)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1];
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	void *buf, *ptr;
	u32 buf_len, len;
	int ret;
	struct wmi_peer_atf_request_fixed_param *cmd;
	u32 num_peers, atf_flags, pdev_id = ar->pdev->pdev_id;

	len = sizeof(*cmd);

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_CONFIG_MAX,
			       peer_config,
			       ath12k_vendor_atf_offload_peer_config_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "No data present in ATF peer condig command\n");
		return ret;
	}

	num_peers = nla_get_u8(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_NUMBER_OF_PEERS]);
	atf_flags = nla_get_u8(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_FLAGS]);
	buf = nla_data(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_PAYLOAD]);
	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER_PAYLOAD]);
	if (!buf_len) {
		ath12k_warn(ar->ab, "No data present in ATF peer config command\n");
			return -EINVAL;
	}
	len += buf_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_peer_atf_request_fixed_param *)ptr;
	memcpy(skb->data, buf, buf_len);
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ATF_REQUEST,
						 sizeof(*cmd));
	cmd->num_peers = cpu_to_le32(num_peers);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->atf_flags = cpu_to_le32(atf_flags);
	ptr += sizeof(*cmd);
	memcpy(ptr, buf, buf_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF peer config for num_peers %u pdev id %u atf_flags %u\n",
		   num_peers, pdev_id, atf_flags);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_ATF_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_PEER_ATF_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static int ath12k_vendor_offload_sched_duration_config(struct ath12k *ar,
						       struct nlattr *sched_duration_param)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1];
	int ret;
	u32 ac, duration, pdev_id = ar->pdev->pdev_id, value;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_MAX,
			       sched_duration_param,
			       ath12k_vendor_atf_offload_sched_duration_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF schedule duration policy\n");
		return ret;
	}

	ac = nla_get_u16(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_AC]);
	duration = nla_get_u16(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION]);

	value = ((ac << 30) & GENMASK(31, 30)) | (duration & GENMASK(29, 0));
	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_SCHED_DURATION,
					value, pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set ATF schedule param for pdev: %u\n", pdev_id);
		return ret;
	}

	return ret;
}

static int ath12k_vendor_offload_ssid_scheduling_config(struct ieee80211_hw *hw,
							struct ath12k *ar,
							struct wireless_dev *wdev,
							struct nlattr *ssid_sched_param)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_MAX + 1];
	int ret;
	u8 ssid_cheduling, link_id = 0;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_MAX,
			       ssid_sched_param,
			       ath12k_vendor_atf_offload_ssid_sched_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF SSID schedule config policy\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_LINK_ID]);

	ssid_cheduling = nla_get_u8(tb[QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED]);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;
	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return -EINVAL;

	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif)
		return -EINVAL;

	ret = ath12k_wmi_vdev_set_param_cmd(ar,
					    arvif->vdev_id, WMI_VDEV_PARAM_ATF_SSID_SCHED_POLICY,
					    ssid_cheduling);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set ATF SSID schedule param for vdev: %u\n", arvif->vdev_id);
		return ret;
	}

	return ret;
}

static int
ath12k_vendor_atf_offload_config_handler(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1];
	u8 config_type, radio_id, atf_enable, atf_stats_enable, atf_strict_scheduling;
	u16 vo_dedicated_time, vi_dedicated_time;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX, data, data_len,
			ath12k_atf_offload_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attributes with ATF config commands\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_OPERATION]) {
		ath12k_err(NULL, "ATF config command missing\n");
		return -EINVAL;
	}
	config_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_OPERATION]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_ID]) {
		ath12k_err(NULL, "ATF config command missing\n");
		return -EINVAL;
	}
	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_ID]);

	ar = ath12k_ah_to_ar(ah, radio_id);

	switch (config_type) {
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_ENABLE_DISABLE:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_CONFIG]) {
			ath12k_err(NULL, "ATF enable config missing\n");
			return -EINVAL;
		}
		atf_enable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_DYNAMIC_ENABLE,
						atf_enable, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF: %d\n",
				    atf_enable ? "enable" : "disable",
				    ret);
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_GROUP:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG]) {
			ath12k_err(NULL, "ATF SSID grouping config missing\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_atf_offload_ssid_grouping_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set SSID config\n");
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_WMM_AC:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]) {
			ath12k_err(NULL, "ATF WMM AC config missing\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_atf_offload_wmm_ac_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set WMM AC config\n");
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]) {
			ath12k_err(NULL, "ATF peer request config missing\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_atf_offload_peer_config(ar,
							    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set peer config\n");
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_STATS_ENABLE_DISABLE:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STATS_CONFIG]) {
			ath12k_err(NULL, "ATF stats config missing\n");
			return -EINVAL;
		}
		atf_stats_enable =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STATS_CONFIG]);
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_STRICT_SCH:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STRICT_SCH_CONFIG]) {
			ath12k_err(NULL, "ATF strict scheduling config missing\n");
			return -EINVAL;
		}
		atf_strict_scheduling =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STRICT_SCH_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_STRICT_SCH,
						atf_strict_scheduling, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF sctrict scheduling: %d\n",
				    atf_strict_scheduling ? "enable" : "disable",
				    ret);
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_VO_TIME:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]) {
			ath12k_err(NULL, "ATF vo dedicated time config missing\n");
			return -EINVAL;
		}
		vo_dedicated_time = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VO_DEDICATED_TIME,
						vo_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vo_dedicated_time  %d\n", ret);
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_VI_TIME:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]) {
			ath12k_err(NULL, "ATF vi dedicated time config missing\n");
			return -EINVAL;
		}
		vi_dedicated_time =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VI_DEDICATED_TIME,
						vi_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vi_dedicated_time  %d\n", ret);
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]) {
			ath12k_err(NULL, "ATF schedule duration config missing\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_offload_sched_duration_config(ar,
								  tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF schedule duration\n");
			return ret;
		}
		break;
	case QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHEDULING:
		if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]) {
			ath12k_err(NULL, "ATF ssid scheduling config missing\n");
			return -EINVAL;
		}
		ret = ath12k_vendor_offload_ssid_scheduling_config(hw, ar, wdev,
								   tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF ssid scheduling\n");
			return ret;
		}
		break;
	default:
		ath12k_err(NULL, "Invalid operation with ATF offload commands\n");
		ret = -EINVAL;
	}

	return ret;
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
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE,
		.doit = ath12k_vendor_6ghz_power_mode_change,
		.policy = ath12k_cfg80211_power_mode_set_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS,
		.doit = ath12k_vendor_sdwf_phy_operations,
		.dumpit = ath12k_vendor_view_sdwf_config,
		.policy = ath12k_vendor_sdwf_phy_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS,
		.doit = ath12k_vendor_atf_offload_config_handler,
		.policy = ath12k_atf_offload_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
};

static const struct nl80211_vendor_cmd_info ath12k_vendor_events[] = {
	[QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_POWER_MODE_CHANGE_COMPLETED
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
