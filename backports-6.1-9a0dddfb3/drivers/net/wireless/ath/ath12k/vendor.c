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
#include "telemetry_agent_if.h"
#include "erp.h"

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
	[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
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

	ret = ath12k_wmi_send_afc_cmd_tlv(ar, afc_resp_format,
					  WMI_AFC_CMD_SERV_RESP_READY);
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

static const struct nla_policy
ath12k_vendor_rm_generic_policy[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_wlan_telemetry_req_policy[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC] = {.type = NLA_BINARY,
							.len = ETH_ALEN},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID] = {.type = NLA_U64},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_wlan_telemetry_feat_policy[QCA_VENDOR_ATTR_WLAN_FEAT_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_FEAT_TX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_RX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY] = {.type = NLA_FLAG},
};

int ath12k_extract_feat_inputs(struct nlattr *tb_attr,
			       struct ath12k_telemetry_command *cmd)
{
	struct nlattr *feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_MAX + 1] = {0};
	int ret;

	memset(&cmd->feat, 0, sizeof(struct ath12k_stats_feat));

	ret = nla_parse_nested(feat_attr, QCA_VENDOR_ATTR_WLAN_FEAT_MAX, tb_attr,
			       ath12k_wlan_telemetry_feat_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "nla parse failure: Feature input\n");
		return ret;
	}

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_TX])
		cmd->feat.feat_tx = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_RX])
		cmd->feat.feat_rx = true;

	if (cmd->svc_id != INVALID_SVC_ID &&
	    feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX])
		cmd->feat.feat_sdwftx = true;

	if (cmd->svc_id != INVALID_SVC_ID &&
	    feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY])
		cmd->feat.feat_sdwfdelay = true;

	return ret;
}

static int ath12k_extract_user_inputs(struct nlattr **tb,
				      struct ath12k_telemetry_command *cmd)
{
	int ret;

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE])
		cmd->obj = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID])
		cmd->svc_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE])
		ret = ath12k_extract_feat_inputs(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE],
						 cmd);

	/**
	 * To have a unique request ID for an application, the request ID of
	 * the command is compounded with the PID of the requesting application
	 * such that the upper 32 bits represent the PID and the lower
	 * 32 bits represent the request ID provided for the command by the
	 * application.
	 */
	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID])
		cmd->request_id = nla_get_u64(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID])
		cmd->link_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID]);
	else
		cmd->link_id = INVALID_LINK_ID;

	if (cmd->obj == STATS_OBJ_PEER) {
		if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC] &&
		    (nla_len(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC]) == ETH_ALEN))
			memcpy(cmd->mac,
			       nla_data(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC]),
			       ETH_ALEN);
	}

	return ret;
}

static int ath12k_prepare_telemetry_common_vendor_attr(struct sk_buff *vendor_event,
						       struct ath12k_telemetry_command *cmd)
{
	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_OBJECT_EVENT,
		       cmd->obj)) {
		ath12k_err(NULL, "nla put failure: Common attr obj field");
		return -EINVAL;
	}

	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID_EVENT,
		       cmd->link_id)) {
		ath12k_err(NULL, "nla put failure: Common attr link_id field");
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID_EVENT,
			      cmd->request_id, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Common attr req_id field");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_rxdma_err_attrs(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       uint32_t *rxdma_error)
{
	struct nlattr *attr;
	int rxdma;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device rxdma error");
		return -EINVAL;
	}

	for (rxdma = 0; rxdma < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; rxdma++)
		if (nla_put_u32(vendor_event, rxdma + 1, rxdma_error[rxdma])) {
			ath12k_err(ab,
				   "nla put failure: Device rxdma err attr %d",
				   rxdma + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_reo_err_attrs(struct ath12k_base *ab,
				     struct sk_buff *vendor_event,
				     uint32_t *reo_error)
{
	struct nlattr *attr;
	int reo_attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device reo error");
		return -EINVAL;
	}

	for (reo_attr = 0; reo_attr < HAL_REO_DEST_RING_ERROR_CODE_MAX;
	     reo_attr++)
		if (nla_put_u32(vendor_event, reo_attr + 1,
				reo_error[reo_attr])) {
			ath12k_err(ab,
				   "nla put failure: Device reo err attr %d",
				   reo_attr + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_device_rx_sw_wbm_drop_attrs(struct ath12k_base *ab,
						   struct sk_buff *vendor_event,
						   struct ath12k_telemetry_dp_device *device_dp_stats)
{
	struct nlattr *attr;
	int drop;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_WBM_SW_DROP_REASON_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device rx sw wbm drop");
		return -EINVAL;
	}

	for (drop = 0; drop < WBM_ERR_DROP_MAX ; drop++) {
		if (nla_put_u32(vendor_event, drop + 1,
				device_dp_stats->rx_wbm_sw_drop_reason[drop])) {
			ath12k_err(ab,
				   "nla put failure: Device rx sw wbm drop attr %d",
				   drop + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_device_rx_sw_reo_drop_attrs(struct ath12k_base *ab,
						   struct sk_buff *vendor_event,
						   struct ath12k_telemetry_dp_device *device_dp_stats)
{
	int reo_drop_attr, ring_attr;
	struct nlattr *attr1, *attr2;

	attr1 = nla_nest_start(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_SW_DROP_REASON_EVENT);
	if (!attr1) {
		ath12k_err(ab, "nla nest failure: Device reo sw drop");
		return -EINVAL;
	}

	for (ring_attr = 0; ring_attr < DP_REO_RING_MAX; ring_attr++) {
		attr2 = nla_nest_start(vendor_event, ring_attr + 1);
		if (!attr2) {
			ath12k_err(ab,
				   "nla nest failure: Device reo sw drop ring %d",
				   ring_attr + 1);
			return -EINVAL;
		}

		for (reo_drop_attr = 0; reo_drop_attr < DP_RX_ERR_MAX;
		     reo_drop_attr++) {
			if (nla_put_u32(vendor_event, reo_drop_attr + 1,
					device_dp_stats->reo_sw_drop_reason[reo_drop_attr][ring_attr])) {
				ath12k_err(ab,
					   "nla put failure: Device rx sw REO drop attr %d ring %d",
					   reo_drop_attr + 1, ring_attr + 1);
				nla_nest_end(vendor_event, attr2);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, attr2);
	}
	nla_nest_end(vendor_event, attr1);

	return 0;
}

static int ath12k_fill_device_rx_stats(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	if (ath12k_fill_rxdma_err_attrs(ab, vendor_event,
					device_dp_stats->rxdma_error)) {
		ath12k_err(ab, "Error filling device rxdma err Stats");
		return -EINVAL;
	}

	if (ath12k_fill_reo_err_attrs(ab, vendor_event,
				      device_dp_stats->reo_error)) {
		ath12k_err(ab, "Error filling device reo err stats");
		return -EINVAL;
	}

	if (ath12k_fill_device_rx_sw_wbm_drop_attrs(ab, vendor_event,
						    device_dp_stats)) {
		ath12k_err(ab, "Error filling device rx sw wbm drop stats");
		return -EINVAL;
	}

	if (ath12k_fill_device_rx_sw_reo_drop_attrs(ab, vendor_event,
						    device_dp_stats)) {
		ath12k_err(ab, "Error filling device rx sw reo drop stats");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_device_tx_comp_err_attr(struct ath12k_base *ab,
					       struct sk_buff *vendor_event,
					       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	int tx_comp_attr, ring_attr;
	struct nlattr *attr1;
	struct nlattr *attr2;

	attr1 = nla_nest_start(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_EVENT);
	if (!attr1) {
		ath12k_err(ab, "nla nest failure: Device tx comp error");
		return -EINVAL;
	}

	for (ring_attr = 0; ring_attr < DP_TCL_NUM_RING_MAX; ring_attr++) {
		attr2 = nla_nest_start(vendor_event, ring_attr + 1);
		if (!attr2) {
			ath12k_err(ab,
				   "nla nest failure: Device tx comp error - ring %d",
				   ring_attr + 1);
			return -EINVAL;
		}

		for (tx_comp_attr = 0; tx_comp_attr < DP_TX_COMP_ERR_MAX;
		     tx_comp_attr++) {
			if (nla_put_u32(vendor_event, tx_comp_attr + 1,
					device_dp_stats->tx_comp_err[tx_comp_attr][ring_attr])) {
				ath12k_err(ab,
					   "nla put failure: Device tx comp err attr %d ring %d",
					   ring_attr + 1, tx_comp_attr + 1);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, attr2);
	}
	nla_nest_end(vendor_event, attr1);

	return 0;
}

static int ath12k_fill_device_tx_stats(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	if (ath12k_fill_device_tx_comp_err_attr(ab, vendor_event,
						device_dp_stats)) {
		ath12k_err(ab, "Error filling device tx comp err stats");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_prepare_device_vendor_event(struct sk_buff *vendor_event,
					      struct ath12k_dp *dp,
					      struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_device *telemetry_device;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_device = vmalloc(sizeof(*telemetry_device));
	if (!telemetry_device) {
		ath12k_err(dp->ab, "Failed to allocate telemetry_device for device stats");
		return -ENOMEM;
	}

	memset(telemetry_device, 0, sizeof(*telemetry_device));
	ath12k_dp_get_device_stats(dp, telemetry_device);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_device_rx_stats(dp->ab, vendor_event,
							telemetry_device)) {
				ath12k_err(dp->ab,
					   "Error filling device rx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(dp->ab,
				   "nla nest failure: device rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_device_tx_stats(dp->ab, vendor_event,
							telemetry_device)) {
				ath12k_err(dp->ab,
					   "Error filling device tx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(dp->ab, "nla nest failure: device tx feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(telemetry_device);
	return ret;
}

static int ath12k_get_common_nl_event_attr_size(void)
{
	int common_size;

	common_size = nla_total_size(sizeof(u32)) +
		      nla_total_size(sizeof(u8)) + /* Link Id */
		      nla_total_size(sizeof(u64)); /* Request Id */

	return common_size;
}

static int ath12k_get_device_feat_rx_attr_size(void)
{
	int payload_size;
	int total_size;
	int attr_size;
	int ring;

	/* RXDMA ERR */
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	attr_size = nla_total_size_nested(payload_size);

	/* REO ERR */
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_DEST_RING_ERROR_CODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* WBM DROP Reason */
	payload_size = nla_total_size(sizeof(u32)) * WBM_ERR_DROP_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* REO DROP */
	for (ring = 0; ring < DP_REO_RING_MAX; ring++) {
		payload_size = nla_total_size(sizeof(u32)) * DP_RX_ERR_MAX;
		/* Size of each rings */
		attr_size += nla_total_size_nested(payload_size);
	}
	attr_size += nla_total_size_nested(attr_size);

	/* Parent RX STATS */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_device_feat_tx_attr_size(void)
{
	int attr_size = 0;
	int payload_size;
	int total_size;
	int ring;

	/* TX COMP ERR */
	for (ring = 0; ring < DP_TCL_NUM_RING_MAX; ring++) {
		payload_size = nla_total_size(sizeof(u32)) *
				DP_TX_COMP_ERR_MAX;
		/* Size of each rings */
		attr_size += nla_total_size_nested(payload_size);
	}

	attr_size += nla_total_size_nested(attr_size);

	/* Parent TX STATS */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_device_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_rx)
		total_size += ath12k_get_device_feat_rx_attr_size();

	if (cmd->feat.feat_tx)
		total_size += ath12k_get_device_feat_tx_attr_size();

	return total_size;
}

static int ath12k_get_peer_rx_stats_size(void)
{
	struct ath12k_dp_peer_rx_stats rx_stats;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_REO_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(rx_stats.recv_from_reo.packets)) +
				nla_total_size(sizeof(rx_stats.recv_from_reo.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(rx_stats.sent_to_stack.packets)) +
				nla_total_size(sizeof(rx_stats.sent_to_stack.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(rx_stats.sent_to_stack_fast.packets)) +
				nla_total_size(sizeof(rx_stats.sent_to_stack_fast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		/* Advance */
		attr_size += nla_total_size(sizeof(rx_stats.mcast)) +
				nla_total_size(sizeof(rx_stats.ucast)) +
				nla_total_size(sizeof(rx_stats.non_amsdu)) +
				nla_total_size(sizeof(rx_stats.msdu_part_of_amsdu)) +
				nla_total_size(sizeof(rx_stats.mpdu_retry));

		/* Ring Attr Size */
		total_size += nla_total_size_nested(attr_size);
	}

	return total_size;
}

static int ath12k_get_feat_rx_peer_attr_size(void)
{
	int payload_size;
	int total_size;
	int attr_size;

	/* Rx Stats */
	payload_size = ath12k_get_peer_rx_stats_size();
	/* Rx Per Pkt Stats Attr */
	attr_size = nla_total_size_nested(payload_size);

	/* Rx WBM Err*/
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_DEST_RING_ERROR_CODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* Parent RX Stats Attr Size */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_feat_tx_peer_attr_size(void)
{
	struct ath12k_dp_peer_tx_stats tx_stats;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(tx_stats.comp_pkt.packets)) +
				nla_total_size(sizeof(tx_stats.comp_pkt.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(tx_stats.tx_success.packets)) +
				nla_total_size(sizeof(tx_stats.tx_success.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(tx_stats.tx_failed));

		/* Advance */
		payload_size = nla_total_size(sizeof(u32)) *
				(HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX);
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(u32)) *
				(HAL_WBM_TQM_REL_REASON_MAX);
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(tx_stats.release_src_not_tqm)) +
				nla_total_size(sizeof(tx_stats.retry_count)) +
				nla_total_size(sizeof(tx_stats.total_msdu_retries)) +
				nla_total_size(sizeof(tx_stats.multiple_retry_count)) +
				nla_total_size(sizeof(tx_stats.ofdma)) +
				nla_total_size(sizeof(tx_stats.amsdu_cnt)) +
				nla_total_size(sizeof(tx_stats.non_amsdu_cnt)) +
				nla_total_size(sizeof(tx_stats.inval_link_id_pkt_cnt)) +
				nla_total_size(sizeof(tx_stats.mcast)) +
				nla_total_size(sizeof(tx_stats.ucast)) +
				nla_total_size(sizeof(tx_stats.bcast));

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}

	/* Parent Tx per_pkt Attr Size */
	total_size = nla_total_size_nested(total_size);

	/* Parent Tx Attr Size */
	total_size = nla_total_size_nested(total_size);
	return total_size;
}

static int get_feat_sdwftx_attr_size_per_msduq(void)
{
	int payload_size = 0, attr_size = 0;
	int nested2_size = 0, nested3_size = 0;
	struct ath12k_tele_qos_tx_ctx tx_ctx;
	struct ath12k_tele_qos_tx tx;

	attr_size = nla_total_size(sizeof(tx.tx_success.num));
	attr_size += nla_total_size(sizeof(tx.tx_success.bytes));
	nested2_size = nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.tx_failed.num));
	attr_size += nla_total_size(sizeof(tx.tx_failed.bytes));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.tx_ingress.num));
	attr_size += nla_total_size(sizeof(tx.tx_ingress.bytes));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.dropped.fw_rem.num));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem.bytes));
	nested3_size = nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.dropped.fw_rem_notx));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_tx));
	attr_size += nla_total_size(sizeof(tx.dropped.age_out));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason1));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason2));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason3));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_queue_disable));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_no_match));
	attr_size += nla_total_size(sizeof(tx.dropped.drop_threshold));
	attr_size += nla_total_size(sizeof(tx.dropped.drop_link_desc_na));
	attr_size += nla_total_size(sizeof(tx.dropped.invalid_drop));
	attr_size += nla_total_size(sizeof(tx.dropped.mcast_vdev_drop));
	attr_size += nla_total_size(sizeof(tx.dropped.invalid_rr));

	nested2_size += nla_total_size_nested(nested3_size + attr_size);

	attr_size = nla_total_size(sizeof(tx.svc_intval_stats.success_cnt));
	attr_size = nla_total_size(sizeof(tx.svc_intval_stats.failure_cnt));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.burst_size_stats.success_cnt));
	attr_size = nla_total_size(sizeof(tx.burst_size_stats.failure_cnt));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.queue_depth));
	attr_size += nla_total_size(sizeof(tx.throughput));
	attr_size += nla_total_size(sizeof(tx.ingress_rate));
	attr_size += nla_total_size(sizeof(tx.min_throughput));
	attr_size += nla_total_size(sizeof(tx.max_throughput));
	attr_size += nla_total_size(sizeof(tx.avg_throughput));
	attr_size += nla_total_size(sizeof(tx.per));
	attr_size += nla_total_size(sizeof(tx.retries_pct));
	attr_size += nla_total_size(sizeof(tx.total_retries_count));
	attr_size += nla_total_size(sizeof(tx.retry_count));
	attr_size += nla_total_size(sizeof(tx.multiple_retry_count));
	attr_size += nla_total_size(sizeof(tx.failed_retry_count));
	attr_size += nla_total_size(sizeof(tx.reinject_pkt));
	attr_size += nla_total_size(sizeof(tx_ctx.tid));
	attr_size += nla_total_size(sizeof(tx_ctx.msduq));

	payload_size = nested2_size + attr_size;

	attr_size = nla_total_size(sizeof(u32)) * MAX_MCS;
	nested3_size = nla_total_size_nested(attr_size) *DOT11_MAX;
	/* pkt type */
	nested2_size = nla_total_size_nested(nested3_size + attr_size);
	/* sdwftx stats per msduq */
	payload_size += nla_total_size_nested(nested2_size + payload_size);

	return payload_size;
}

static int ath12k_get_feat_sdwftx_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0, per_msduq_size = 0, msduqs_size = 0;
	int sdwftx_event_size = 0, svc_event_size;
	u8 msduq = 0, user_def_msduq_per_tid = 0;

	if (cmd->svc_id == 0) {
		user_def_msduq_per_tid = QOS_TID_MDSUQ_MAX;
		msduq = user_def_msduq_per_tid * QOS_TID_MAX;
		per_msduq_size = get_feat_sdwftx_attr_size_per_msduq();
		msduqs_size = msduq * per_msduq_size;
		sdwftx_event_size = nla_total_size_nested(msduqs_size);
	} else {
		per_msduq_size = get_feat_sdwftx_attr_size_per_msduq();
		sdwftx_event_size = nla_total_size_nested(per_msduq_size);
	}

	svc_event_size = nla_total_size(sizeof(u8));
	total_size = svc_event_size + sdwftx_event_size;

	return total_size;
}

static int get_feat_sdwfdelay_attr_size_per_msduq(void)
{
	int payload_size = 0, attr_size = 0, attr1_size = 0;
	int nested1_size = 0, nested2_size = 0, nested3_size = 0;
	struct ath12k_tele_qos_delay_ctx delay_ctx;
	struct ath12k_tele_qos_delay delay;

	attr_size = nla_total_size(sizeof(delay.nwdelay_avg));
	attr_size += nla_total_size(sizeof(delay.swdelay_avg));
	attr_size += nla_total_size(sizeof(delay.hwdelay_avg));
	attr_size += nla_total_size(sizeof(delay_ctx.tid));
	attr_size += nla_total_size(sizeof(delay_ctx.msduq));

	payload_size = attr_size;

	attr_size = nla_total_size(sizeof(delay.invalid_delay_pkts));
	attr_size += nla_total_size(sizeof(delay.delay_success));
	attr_size += nla_total_size(sizeof(delay.delay_failure));
	attr_size += nla_total_size(sizeof(delay.delay_hist.min));
	attr_size += nla_total_size(sizeof(delay.delay_hist.max));
	attr_size += nla_total_size(sizeof(delay.delay_hist.avg));

	attr1_size = nla_total_size(sizeof(u64)) * HIST_BUCKET_MAX;
	/* HW_TX_COMP_DELAY nest */
	nested3_size = nla_total_size_nested(attr1_size);

	/* HWDELAY_HISTOGRAM nest */
	nested2_size = nla_total_size_nested(nested3_size);

	/* SDWFDELAY_HWDELAY */
	nested1_size = nla_total_size_nested(nested2_size + attr_size);

	payload_size += nested1_size;

	return payload_size;
}

static int ath12k_get_feat_sdwfdelay_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0, per_msduq_size = 0, msduqs_size = 0;
	int sdwfdelay_event_size = 0, svc_event_size;
	u8 msduq = 0, user_def_msduq_per_tid = 0;

	if (cmd->svc_id == 0) {
		user_def_msduq_per_tid = QOS_TID_MDSUQ_MAX;
		msduq = user_def_msduq_per_tid * QOS_TID_MAX;

		per_msduq_size = get_feat_sdwfdelay_attr_size_per_msduq();
		msduqs_size = msduq * per_msduq_size;

		sdwfdelay_event_size = nla_total_size_nested(msduqs_size);
	} else {
		per_msduq_size = get_feat_sdwfdelay_attr_size_per_msduq();
		sdwfdelay_event_size = nla_total_size_nested(per_msduq_size);
	}

	svc_event_size = nla_total_size(sizeof(u8));
	total_size = svc_event_size + sdwfdelay_event_size;

	return total_size;
}

static int ath12k_get_dp_peer_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_rx)
		total_size += ath12k_get_feat_rx_peer_attr_size();

	if (cmd->feat.feat_tx)
		total_size += ath12k_get_feat_tx_peer_attr_size();

	if (cmd->feat.feat_sdwftx)
		total_size += ath12k_get_feat_sdwftx_attr_size(cmd);

	if (cmd->feat.feat_sdwfdelay)
		total_size += ath12k_get_feat_sdwfdelay_attr_size(cmd);

	return total_size;
}

static int ath12k_get_dp_ingress_attr_len(void)
{
	struct ath12k_dp_tx_ingress_stats ingress_stats;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		payload_size = nla_total_size(sizeof(ingress_stats.recv_from_stack.packets)) +
				nla_total_size(sizeof(ingress_stats.recv_from_stack.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(ingress_stats.enque_to_hw.packets)) +
				nla_total_size(sizeof(ingress_stats.enque_to_hw.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(ingress_stats.enque_to_hw_fast.packets)) +
				nla_total_size(sizeof(ingress_stats.enque_to_hw_fast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_TCL_ENCAP_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_ENCRYPT_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TCL_DESC_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TX_ENQ_ERR_MAX;
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(ingress_stats.mcast));

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}
	/* Parent Ingress Stats Attr size */
	total_size += nla_total_size_nested(total_size);

	return total_size;
}

static int ath12k_get_dp_vif_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_tx)
		total_size += ath12k_get_dp_ingress_attr_len();

	/*Aggregated Sta Stats Size */
	total_size += ath12k_get_dp_peer_attr_len(cmd);

	return total_size;
}

static int ath12k_get_dp_radio_attr_len(struct ath12k_telemetry_command *cmd)
{
	/*Aggregated Sta Stats Size */
	return ath12k_get_dp_peer_attr_len(cmd);
}

int ath12k_get_dp_vendor_event_len(struct ath12k_telemetry_command *cmd)
{
	int total_size;

	total_size = ath12k_get_common_nl_event_attr_size();

	switch (cmd->obj) {
	case STATS_OBJ_PEER:
		total_size += ath12k_get_dp_peer_attr_len(cmd);
		break;
	case STATS_OBJ_VIF:
		total_size += ath12k_get_dp_vif_attr_len(cmd);
		break;
	case STATS_OBJ_RADIO:
		total_size += ath12k_get_dp_radio_attr_len(cmd);
		break;
	case STATS_OBJ_DEVICE:
		total_size += ath12k_get_device_attr_size(cmd);
		break;
	default:
		ath12k_err(NULL, "Invalid obj Type");
	}

	return NLMSG_HDRLEN + total_size;
}

static int ath12k_fill_peer_tx_per_pkt_stats_attrs(struct sk_buff *vendor_event,
						   struct ath12k_dp_peer_stats *peer_stats,
						   int ring_num,
						   bool is_extended)
{
	struct nlattr *attr;
	int reason;

	/* Basic Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - tx comp");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			peer_stats->tx[ring_num].comp_pkt.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].comp_pkt.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - tx success");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			peer_stats->tx[ring_num].tx_success.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].tx_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED,
			peer_stats->tx[ring_num].tx_failed)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED);
		return -EINVAL;
	}

	/* Advance Stats */
	if (!is_extended)
		return 0;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - wbm rel rsn");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX;
	     reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				peer_stats->tx[ring_num].wbm_rel_reason[reason])) {
			ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
				   reason + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - tqm rel rsn");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX;
	     reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				peer_stats->tx[ring_num].tqm_rel_reason[reason])) {
			ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON,
				   reason + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
			peer_stats->tx[ring_num].release_src_not_tqm)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
			peer_stats->tx[ring_num].retry_count)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT, ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
			peer_stats->tx[ring_num].total_msdu_retries)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
			peer_stats->tx[ring_num].multiple_retry_count)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA,
			peer_stats->tx[ring_num].ofdma)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA, ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
			peer_stats->tx[ring_num].amsdu_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
			peer_stats->tx[ring_num].non_amsdu_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
			peer_stats->tx[ring_num].inval_link_id_pkt_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
			peer_stats->tx[ring_num].mcast)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
			peer_stats->tx[ring_num].ucast)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,
			peer_stats->tx[ring_num].bcast)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,
			   ring_num + 1);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_peer_tx_stats(struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     bool is_extended)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	/*Tx Per Pkt Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer per pkt stats");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats - ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_fill_peer_tx_per_pkt_stats_attrs(vendor_event, peer_stats,
							    ring_num, is_extended)) {
			ath12k_err(NULL, "Error filling peer tx per pkt stats for ring %d",
				   ring_num + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_rx_wbm_err_attrs(struct sk_buff *vendor_event,
					     struct ath12k_dp_peer_stats *peer_stats)
{
	if (ath12k_fill_rxdma_err_attrs(NULL, vendor_event,
					peer_stats->wbm_err.rxdma_error)) {
		ath12k_err(NULL, "Error filling peer rxdma error");
		return -EINVAL;
	}

	if (ath12k_fill_reo_err_attrs(NULL, vendor_event,
				      peer_stats->wbm_err.reo_error)) {
		ath12k_err(NULL, "Error filling peer reo error");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_peer_rx_per_pkt_stats_attrs(struct sk_buff *vendor_event,
						   struct ath12k_dp_peer_stats *peer_stats,
						   int ring_num,
						   bool is_extended)
{
	struct nlattr *attr;

	/* Basic Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx from reo");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			peer_stats->rx[ring_num].recv_from_reo.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].recv_from_reo.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx to stack");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].sent_to_stack.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx to stack fast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].sent_to_stack_fast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/* Advance Stats */
	if (!is_extended)
		return 0;

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MCAST,
			peer_stats->rx[ring_num].mcast)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MCAST,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_UCAST,
			peer_stats->rx[ring_num].ucast)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_UCAST,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
			peer_stats->rx[ring_num].non_amsdu)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
			peer_stats->rx[ring_num].msdu_part_of_amsdu)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
			peer_stats->rx[ring_num].mpdu_retry)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
			   ring_num + 1);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_peer_rx_stats(struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     bool is_extended)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	/*Rx Per Pkt Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats Rx");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats Rx ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_fill_peer_rx_per_pkt_stats_attrs(vendor_event,
							    peer_stats,
							    ring_num,
							    is_extended)) {
			ath12k_err(NULL,
				   "nla put failure: Peer rx per_pkt stats for ring %d",
				   ring_num + 1);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	/*Rx WBM Err Stats*/
	if (ath12k_fill_peer_rx_wbm_err_attrs(vendor_event, peer_stats)) {
		ath12k_err(NULL, "Error filling peer rx wbm err stats");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_tele_sdwftx_stats_update(struct sk_buff *skb, struct ath12k_tele_qos_tx *tx,
					   u8 tid, u8 q_id)
{
	struct nlattr *attr1 = NULL, *attr2 = NULL;
	int ret = -EINVAL, pkt_type, mcs;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_DEPTH, tx->queue_depth) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_THROUGHPUT, tx->throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS_RATE, tx->ingress_rate) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MIN_THROUGHPUT, tx->min_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX_THROUGHPUT, tx->max_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AVG_THROUGHPUT, tx->avg_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_ERROR_RATE, tx->per) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PERCENTAGE, tx->retries_pct) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PKTS_CNT, tx->retry_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TOTAL_RETRIES_CNT, tx->total_retries_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MULTIPLE_RETRIES_CNT, tx->multiple_retry_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED_RETRIES_CNT, tx->failed_retry_count) ||
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_REINJECT_PKTS, tx->reinject_pkt) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TID, tid) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_ID, q_id)) {
		ath12k_err(NULL, "nla_put_failure: SDWF TX attributes \n");
		goto end;
	}

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SUCCESS);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_SUCCESS \n");
		goto end;
	}

	if (nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			      tx->tx_success.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      tx->tx_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_SUCCESS attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_FAILED \n");
		goto end;
	}

	if (nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			      tx->tx_failed.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      tx->tx_failed.bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_FAILED attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_INGRESS \n");
		goto end;
	}

	if (nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			      tx->tx_ingress.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      tx->tx_ingress.bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_INGRESS attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_DROP \n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_TX,
			tx->dropped.fw_rem_tx) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_NOTX,
			tx->dropped.fw_rem_notx) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_AGED_FRAMES,
			tx->dropped.age_out) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON1,
			tx->dropped.fw_reason1) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON2,
			tx->dropped.fw_reason2) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON3,
			tx->dropped.fw_reason3) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_DISABLE_QUEUE,
			tx->dropped.fw_rem_queue_disable) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_TILL_NONMATCHING,
			tx->dropped.fw_rem_no_match) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_THRESHOLD_DROP,
			tx->dropped.drop_threshold) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_LINK_DESC_UNAVAIL_DROP,
			tx->dropped.drop_link_desc_na) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_MSDU_OR_DROP,
			tx->dropped.invalid_drop) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MULTICAST_DROP,
			tx->dropped.mcast_vdev_drop) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_RR,
			tx->dropped.invalid_rr)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_DROP attributes\n");
		goto end;
	}

	attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_MPDU);
	if (!attr2) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_DROP REMOVE MPDU\n");
		goto end;
	}

	if (nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			      tx->dropped.fw_rem.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      tx->dropped.fw_rem.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_DROP REMOVE MPDU attributes\n");
		goto end;
	}

	nla_nest_end(skb, attr2);
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE\n");
		goto end;
	}

	for (pkt_type = 0; pkt_type < DOT11_MAX; pkt_type++) {
		attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A + pkt_type);
		if (!attr2) {
			ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE_80211\n");
			goto end;
		}
		for (mcs = 0; mcs < MAX_MCS; mcs++) {
			if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 + mcs,
				tx->pkt_type[pkt_type].mcs_count[mcs])) {
				ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE_80211 %d mcs attribute %d\n",
					   QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A + pkt_type,
					   QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 + mcs);
				goto end;
			}
		}
		nla_nest_end(skb, attr2);
	}
	nla_nest_end(skb, attr1);

	attr1 =  nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SERVICE_INTERVAL);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX SERVICE_INTERVAL\n");
		goto end;
	}
	if (nla_put_u64_64bit(skb,
			      QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT,
			      tx->svc_intval_stats.success_cnt,
			      NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,
			      tx->svc_intval_stats.failure_cnt,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX SERVICE_INTERVAL attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 =  nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_BURST_SIZE);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX BURST SIZE\n");
		goto end;
	}
	if (nla_put_u64_64bit(skb,
			      QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT,
			      tx->burst_size_stats.success_cnt,
			      NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,
			      tx->burst_size_stats.failure_cnt,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX BURST SIZE attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);
	ret = 0;
end:
	return ret;
}

static int ath12k_fill_sdwftx_stats(struct sk_buff *skb,
			     struct ath12k_tele_qos_tx_ctx *tx_ctx,
			     u8 svc_id)
{
	struct nlattr *attr = NULL;
	struct ath12k_tele_qos_tx *tx = NULL;
	int ret = -EINVAL;
	u8 msduq = 0, tid, q_idx;

	if (svc_id == 0) {
		for (tid = 0; tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0; q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				tx = &tx_ctx->tx[tid][q_idx];
				attr = nla_nest_start(skb, msduq);
				if (!attr) {
					ath12k_err(NULL, "nla_nest_failure: SDWF TX for msduq %u\n", msduq);
					goto end;
				}
				ret = ath12k_tele_sdwftx_stats_update(skb, tx,
							  tid, q_idx);
				if (ret) {
					ath12k_err(NULL, "sdwf tx stats update failure for msduq %u\n", msduq);
					goto end;
				}
				nla_nest_end(skb, attr);
				msduq++;
			}
		}
	} else {
		tx = &tx_ctx->tx[0][0];
		if (!tx) {
			ath12k_err(NULL, "nla_nest_failure: SDWF TX stats NA \n");
			goto end;
		}
		attr = nla_nest_start(skb, msduq);
		if (!attr) {
			ath12k_err(NULL, "nla_nest_failure: SDWF TX for msduq %u\n", msduq);
			goto end;
		}

		ret = ath12k_tele_sdwftx_stats_update(skb, tx, tx_ctx->tid,
					  tx_ctx->msduq);
		if (ret) {
			ath12k_err(NULL, "sdwf tx stats update failure for msduq : 0\n");
			goto end;
		}
		nla_nest_end(skb, attr);
	}
end:
	return ret;
}

static int ath12k_tele_sdwfdelay_stats_update(struct sk_buff *skb,
					      struct ath12k_tele_qos_delay *delay,
					      u8 tid, u8 q_id)
{
	struct nlattr *attr1 = NULL, *attr2 = NULL, *attr3 = NULL;
	size_t size;
	int ret = -EINVAL, buc_id;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_SOFTWARE_DELAY_AVG, delay->swdelay_avg) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_NETWORK_DELAY_AVG, delay->nwdelay_avg) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HARDWARE_DELAY_AVG, delay->hwdelay_avg) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_TID, tid) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_QUEUE_ID, q_id)) {
		ath12k_err(NULL, "nla_put_failure: SDWF DELAY stats attributes \n");
		goto end;
	}

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HWDELAY\n");
		goto end;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_INVALID_PKTS,
			delay->invalid_delay_pkts) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_SUCCESS,
			      delay->delay_success, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_FAILURE,
			      delay->delay_failure, NL80211_ATTR_PAD) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAXIMUM,
		    sizeof(int), &delay->delay_hist.max) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MINIMUM,
		    sizeof(int), &delay->delay_hist.min) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AVERAGE,
		    sizeof(int), &delay->delay_hist.avg)) {
		ath12k_err(NULL, "nla_put_failure: SDWF DELAY HWDELAY attributes\n");
		goto end;
	}

	attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_HISTOGRAM);
	if (!attr2) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HWDELAY HISTOGRAM\n");
		goto end;
	}

	attr3 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_TX_COMP_DELAY);
	if (!attr3) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HW_TX_COMP_DELAY TYPE\n");
		goto end;
	}

	size = ARRAY_SIZE(delay->delay_hist.hist.freq);
	for (buc_id = 0; buc_id < HIST_BUCKET_MAX && buc_id < size; buc_id++) {
		if (nla_put_u64_64bit(skb,
				QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_0 + buc_id,
				delay->delay_hist.hist.freq[buc_id],
				NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla_put_failure: SDWF DELAY HW_TX_COMP_DELAY TYPE attributes\n");
			goto end;
		}
	}
	nla_nest_end(skb, attr3);

	nla_nest_end(skb, attr2);

	nla_nest_end(skb, attr1);
	ret = 0;
end:
	return ret;
}

static int ath12k_fill_sdwfdelay_stats(struct sk_buff *skb,
				       struct ath12k_tele_qos_delay_ctx *delay_ctx,
				       u8 svc_id)
{
	struct nlattr *attr = NULL;
	struct ath12k_tele_qos_delay *delay = NULL;
	int ret = -EINVAL;
	u8 msduq = 0, tid, q_idx;

	if (svc_id == 0) {
		for (tid = 0; tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0; q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				delay = &delay_ctx->delay[tid][q_idx];
				attr = nla_nest_start(skb, msduq);
				if (!attr) {
					ath12k_err(NULL, "nla_nest_failure: SDWF DELAY for msduq %u\n", msduq);
					goto end;
				}
				ret = ath12k_tele_sdwfdelay_stats_update(skb, delay, tid, q_idx);
				if (ret) {
					ath12k_err(NULL, "sdwf delay stats update failure for msduq %u\n", msduq);
					goto end;
				}
				nla_nest_end(skb, attr);
				msduq++;
			}
		}
	} else {
		delay = &delay_ctx->delay[0][0];
		if (!delay) {
			ath12k_err(NULL, "nla_nest_failure: SDWF DELAY stats NA \n");
			goto end;
		}
		attr = nla_nest_start(skb, msduq);
		if (!attr) {
			ath12k_err(NULL, "nla_nest_failure: SDWF DELAY for msduq %u\n", msduq);
			goto end;
		}

		ret = ath12k_tele_sdwfdelay_stats_update(skb, delay, delay_ctx->tid, delay_ctx->msduq);
		if (ret) {
			ath12k_err(NULL, "sdwf stats update failure for msduq : 0\n");
			goto end;
		}
		nla_nest_end(skb, attr);
	}
end:
	return ret;
}

static int ath12k_prepare_peer_vendor_event(struct sk_buff *vendor_event,
					    struct ath12k_vif *ahvif,
					    struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_peer *telemetry_peer;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_peer = vmalloc(sizeof(*telemetry_peer));
	if (!telemetry_peer) {
		ath12k_err(NULL, "Allocation failure for peer_stats");
		return -ENOMEM;
	}

	memset(telemetry_peer, 0, sizeof(*telemetry_peer));

	if (ath12k_dp_get_peer_stats(ahvif, telemetry_peer, cmd->mac,
				     cmd->link_id)) {
		ath12k_err(NULL, "Error getting peer stats from dp");
		goto out;
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_tx_stats(vendor_event,
						      &telemetry_peer->peer_stats,
						      telemetry_peer->is_extended)) {
				ath12k_err(NULL, "nla put failure: Sta tx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta tx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_rx_stats(vendor_event,
						      &telemetry_peer->peer_stats,
						      telemetry_peer->is_extended)) {
				ath12k_err(NULL, "nla put failure: Sta rx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta rx feat stats");
			goto out;
		}
	}

	if (cmd->svc_id != INVALID_SVC_ID &&
	    (cmd->feat.feat_sdwftx || cmd->feat.feat_sdwfdelay)) {
		ret = ath12k_telemetry_get_qos_stats(ahvif,
						     telemetry_peer, cmd);
		if (ret) {
			ath12k_err(NULL, "SDWF stats get failure\n");
			goto out;
		}

		if (nla_put_u8(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID_EVENT,
			       cmd->svc_id)) {
			ath12k_err(NULL, "nla put failure: SDWF svc id event");
			ret = -EINVAL;
			goto out;
		}

		if (cmd->feat.feat_sdwftx) {
			attr = nla_nest_start(vendor_event,
					      QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFTX_STATS_EVENT);
			if (!attr) {
				ath12k_err(NULL, "nla nest failure: SDWF tx feat event");
				ret = -EINVAL;
				goto out;
			}

			if (ath12k_fill_sdwftx_stats(vendor_event,
						     &telemetry_peer->peer_stats.tx_ctx,
						     cmd->svc_id)) {
				ath12k_err(NULL, "nla put failure: SDWF tx stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		}

		if (cmd->feat.feat_sdwfdelay) {
			attr = nla_nest_start(vendor_event,
					      QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFDELAY_STATS_EVENT);
			if (!attr) {
				ath12k_err(NULL, "nla nest failure: SDWF delay feat event");
				ret = -EINVAL;
				goto out;
			}

			if (ath12k_fill_sdwfdelay_stats(vendor_event,
							&telemetry_peer->peer_stats.delay_ctx,
							cmd->svc_id)) {
				ath12k_err(NULL, "nla put failure: SDWF delay stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		}
	}

	ret = 0;
out:
	vfree(telemetry_peer);
	return ret;
}

static int ath12k_stats_device_setup(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct sk_buff *vendor_event;
	struct ath12k *ar;
	struct ath12k_dp *dp;
	int len, ret;

	if (cmd->link_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid HW Link ID %d", cmd->link_id);
		return -EINVAL;
	}

	ar = &ah->radio[cmd->link_id];
	if (!ar) {
		ath12k_err(NULL, "ar not present\n");
		return -EINVAL;
	}

	dp = ar->ab->dp;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ath12k_err(ar->ab, "Device stats return. Recovery in progress\n");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd);
	ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d\n",
		   len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT,
						   GFP_KERNEL);
	if (!vendor_event) {
		ath12k_err(ar->ab, "Error allocating vendor event\n");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_device_vendor_event(vendor_event, dp, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(ar->ab, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

static struct ath12k_vif *ath12k_get_ahvif_from_wdev(struct wireless_dev *wdev)
{
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	return ahvif;
}

static int ath12k_stats_peer_setup(struct ath12k_telemetry_command *cmd)
{
	struct ath12k_vif *ahvif = NULL;
	struct sk_buff *vendor_event;
	int len, ret;

	ahvif = ath12k_get_ahvif_from_wdev(cmd->wdev);

	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd);
	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d", len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT,
						   GFP_KERNEL);

	if (!vendor_event) {
		ath12k_err(NULL, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_peer_vendor_event(vendor_event, ahvif, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(NULL, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

static int ath12k_fill_vap_rx_stats(struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/* Aggregated Peer Rx Stats */
	ret = ath12k_fill_peer_rx_stats(vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					telemetry_vif->is_extended);

	return ret;
}

static int ath12k_fill_tx_ingress_stats_attrs(struct sk_buff *vendor_event,
					      struct ath12k_dp_tx_ingress_stats *ingress_tx_stats,
					      bool is_extended)
{
	struct nlattr *attr;
	int attr_index;

	/* Basic stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats from stack");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			ingress_tx_stats->recv_from_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      ingress_tx_stats->recv_from_stack.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      ingress_tx_stats->enque_to_hw.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw fast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      ingress_tx_stats->enque_to_hw_fast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats drop type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < DP_TX_ENQ_ERR_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->drop[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (!is_extended)
		return 0;

	/* Extended stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encap type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < HAL_TCL_ENCAP_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->encap_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encrypt type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < HAL_ENCRYPT_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->encrypt_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats desc type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < DP_TCL_DESC_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->desc_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST,
			ingress_tx_stats->mcast)) {
		ath12k_err(NULL, "nla put failure: Tx ingress mcast");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_tx_ingress_stats(struct sk_buff *vendor_event,
					struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vif ingress stats");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: vif ingress stats - ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_fill_tx_ingress_stats_attrs(vendor_event,
						       &telemetry_vif->aggr_vif_stats.stats[ring_num].tx_i,
						       telemetry_vif->is_extended)) {
			ath12k_err(NULL,
				   "Error filling peer tx ingress stats for ring %d",
				   ring_num + 1);
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_vap_tx_stats(struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/* Aggregated sta tx Stats */
	ret = ath12k_fill_peer_tx_stats(vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					telemetry_vif->is_extended);
	if (ret) {
		ath12k_err(NULL, "Error filling vap tx stats");
		return ret;
	}

	/* Ingress tx stats */
	ret = ath12k_fill_tx_ingress_stats(vendor_event, telemetry_vif);

	return ret;
}

static int ath12k_prepare_vif_vendor_event(struct sk_buff *vendor_event,
					   struct ath12k_vif *ahvif,
					   struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_vif *telemetry_vif;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_vif = vmalloc(sizeof(*telemetry_vif));
	if (!telemetry_vif) {
		ath12k_err(NULL, "Allocation failed for vap stats");
		return -ENOMEM;
	}

	memset(telemetry_vif, 0, sizeof(*telemetry_vif));
	ath12k_dp_get_vif_stats(ahvif, telemetry_vif, cmd->link_id);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_vap_rx_stats(vendor_event,
						     telemetry_vif)) {
				ath12k_err(NULL, "Error filling vap rx stats");
				goto out;
			}

			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Vap rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_vap_tx_stats(vendor_event,
						     telemetry_vif)) {
				ath12k_err(NULL, "Error filling vap tx stats");
				goto out;
			}

			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Vap tx feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(telemetry_vif);
	return ret;
}

static int ath12k_stats_vif_setup(struct ath12k_telemetry_command *cmd)
{
	struct ath12k_vif *ahvif = NULL;
	struct sk_buff *vendor_event;
	int len, ret;

	ahvif = ath12k_get_ahvif_from_wdev(cmd->wdev);

	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present");
		return -EINVAL;
	}

	if (cmd->link_id != INVALID_LINK_ID &&
	    !(ahvif->links_map & BIT(cmd->link_id))) {
		ath12k_err(NULL, "Invalid link_id");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd);
	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d", len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT,
						   GFP_KERNEL);

	if (!vendor_event) {
		ath12k_err(NULL, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_vif_vendor_event(vendor_event, ahvif, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(NULL, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

static int ath12k_fill_radio_rx_stats(struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	int ret;

	/* Aggregated peer rx stats */
	ret = ath12k_fill_peer_rx_stats(vendor_event,
					&telemetry_radio->aggr_pdev_stats.peer_stats,
					telemetry_radio->is_extended);

	return ret;
}

static int ath12k_fill_radio_tx_stats(struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	int ret;

	/* Aggregated peer tx stats */
	ret = ath12k_fill_peer_tx_stats(vendor_event,
					&telemetry_radio->aggr_pdev_stats.peer_stats,
					telemetry_radio->is_extended);

	return ret;
}

static int ath12k_prepare_radio_vendor_event(struct sk_buff *vendor_event,
					     struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_radio *telemetry_radio;
	struct ath12k_base *ab = dp_pdev->ar->ab;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_radio = vmalloc(sizeof(*telemetry_radio));
	if (!telemetry_radio) {
		ath12k_err(ab, "Allocation failure for radio_stats");
		return -EINVAL;
	}

	memset(telemetry_radio, 0, sizeof(*telemetry_radio));
	ath12k_dp_get_pdev_stats(dp_pdev, telemetry_radio);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_rx_stats(vendor_event,
						       telemetry_radio)) {
				ath12k_err(ab,
					   "Error filling radio rx feat stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "nla nest failure: Radio rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_tx_stats(vendor_event,
						       telemetry_radio)) {
				ath12k_err(ab,
					   "Error filling radio tx feat stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "NLA nest failure: Radio tx feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(telemetry_radio);
	return ret;
}

static int ath12k_stats_radio_setup(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *vendor_event;
	struct ath12k *ar;
	int len, ret;

	if (cmd->link_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid HW Link ID %d", cmd->link_id);
		return -EINVAL;
	}

	ar = &ah->radio[cmd->link_id];
	if (!ar) {
		ath12k_err(NULL, "ar not present");
		return -EINVAL;
	}

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ath12k_err(ar->ab, "Radio stats return. Recovery in progress\n");
		return -EINVAL;
	}

	dp_pdev = &ar->dp;
	if (!dp_pdev) {
		ath12k_err(ar->ab, "dp_pdev not present");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd);
	ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d",
		   len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT,
						   GFP_KERNEL);
	if (!vendor_event) {
		ath12k_err(ar->ab, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_radio_vendor_event(vendor_event, dp_pdev, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;

out:
	ath12k_err(ar->ab, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

int ath12k_wifi_stats_reply_setup(struct ath12k_telemetry_command *cmd)
{
	int ret;

	switch (cmd->obj) {
	case STATS_OBJ_PEER:
		ret = ath12k_stats_peer_setup(cmd);
		break;
	case STATS_OBJ_VIF:
		ret = ath12k_stats_vif_setup(cmd);
		break;
	case STATS_OBJ_RADIO:
		ret = ath12k_stats_radio_setup(cmd);
		break;
	case STATS_OBJ_DEVICE:
		ret = ath12k_stats_device_setup(cmd);
		break;
	default:
		ath12k_err(NULL, "Invalid obj type\n");
		ret = -EINVAL;
	}

	return ret;
}

static int ath12k_wifi_stats_reply_setup_schedule(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_stats_list_entry *stats_entry;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag;
	struct ath12k *ar;

	stats_entry = kzalloc(sizeof(*stats_entry), GFP_KERNEL);
	if (!stats_entry) {
		ath12k_err(NULL, "Allocation failure for stats_entry\n");
		return -EINVAL;
	}

	ar = ah->radio;
	if (!ar) {
		ath12k_err(NULL, "No radio present\n");
		kfree(stats_entry);
		return -EINVAL;
	}

	if (ar->ab && ar->ab->ag) {
		ag = ar->ab->ag;
	} else {
		ath12k_err(ar->ab, "ag not found\n");
		kfree(stats_entry);
		return -EINVAL;
	}

	memcpy(&stats_entry->usr_command, cmd,
	       sizeof(struct ath12k_telemetry_command));

	list_add_tail(&stats_entry->node, &ag->stats_work.work_list);

	wiphy_work_queue(ah->hw->wiphy, &ag->stats_work.stats_nb_work);

	return 0;
}

static int ath12k_vendor_wlan_telemetry_wiphy_getstats(struct wiphy *wiphy,
						       struct wireless_dev *wdev,
						       const void *data,
						       int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1];
	struct ath12k_telemetry_command cmd = {0};
	int ret;

	ret = nla_parse(tb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX, data, data_len,
			ath12k_wlan_telemetry_req_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "nla parse failure: Getstats wiphy telemetry\n");
		return ret;
	}

	cmd.wiphy = wiphy;
	cmd.svc_id = INVALID_SVC_ID;

	if (ath12k_extract_user_inputs(tb, &cmd)) {
		ath12k_err(NULL, "Error parsing user input\n");
		return -EINVAL;
	}

	ret = ath12k_wifi_stats_reply_setup_schedule(&cmd);

	return ret;
}

static int ath12k_vendor_wlan_telemetry_wdev_getstats(struct wiphy *wiphy,
						      struct wireless_dev *wdev,
						      const void *data,
						      int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1];
	struct ath12k_telemetry_command cmd = {0};
	int ret;

	ret = nla_parse(tb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX, data, data_len,
			ath12k_wlan_telemetry_req_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "NLA Parse failure - Getstats wdev telemetry\n");
		return ret;
	}

	cmd.wiphy = wiphy;
	cmd.wdev = wdev;
	cmd.svc_id = INVALID_SVC_ID;

	if (ath12k_extract_user_inputs(tb, &cmd)) {
		ath12k_err(NULL, "Error parsing user input\n");
		return ret;
	}

	ret = ath12k_wifi_stats_reply_setup_schedule(&cmd);

	return ret;
}

/* Send a 'reload' NL80211 event to userspace
 * link_id is 'invalid' for non-mlo wdev.
 * For mlo wdev, if 'invalid' link_id is passed, userspace will reload all
 * BSSes of the mlo
 * If valid link_id is passed for mlo wdev, only that BSS is reset
 * in userspace.
 */
static int ath12k_vendor_event_iface_reload_link(struct wiphy *wiphy,
					    struct wireless_dev *wdev, u8 link_id)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(wiphy, wdev,
					  NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX,
					  GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_LINKID, link_id)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "send event to userspace link_id %d\n", link_id);
	cfg80211_vendor_event(skb, GFP_KERNEL);

	return 0;
}

/* Find the wdevs corresponding to the radio index of the wiphy
 * and send the reload event to userspace
 */
static int ath12k_vendor_event_iface_reload(struct wiphy *wiphy, u8 radio_idx)
{
	struct ieee80211_hw *hw = NULL;
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct wireless_dev *wdev = NULL;

	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw->priv;
	ar = &ah->radio[radio_idx];
	if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
		return -ENODATA;
	}
	list_for_each_entry(arvif, &ar->arvifs, list) {
		wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
		if (!wdev) {
			ath12k_err(NULL, "Failed to find wdev\n");
			return -ENODATA;
		}
		ath12k_vendor_event_iface_reload_link(wiphy, wdev, arvif->link_id);
	}
	return 0;
}

/* Set link-vif level parameters
 * set 'reload' to true to send reload event to userspace */
static int ath12k_vendor_set_arvif_params(struct ath12k_link_vif *arvif,
					  u32 param, u32 value, bool *reload)
{
	int ret = -1;

	switch (param) {
	case QCA_WLAN_VENDOR_VDEV_PARAM_TEST_RELOAD:
		*reload = true;
		ret = 0;
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

/* Set radio level parameters
 * set 'reload' to true to send reload event to userspace */
static int ath12k_vendor_set_radio_params(struct ath12k *ar,
					  u32 param, u32 value, bool *reload)
{
	int ret = -1;

	switch (param) {
	case QCA_WLAN_VENDOR_RADIO_PARAM_TEST_RELOAD:
		*reload = true;
		ret = 0;
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

/* Find the link-vif of the wdev with the link id to set params
 * If link id is invalid use the default link
 */
static int ath12k_vendor_set_wifi_params(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ath12k_hw *ah = NULL;
	u32 param = params->value;
	u32 *data = (u32 *)params->data;
	u32 value = *data;
	int ret = -1;
	bool reload = false;

	lockdep_assert_wiphy(wiphy);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif || !data)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	ah = ahvif->ah;

	rcu_read_lock();
	if (ah && params->link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (params->link_id < ATH12K_NUM_MAX_LINKS)
			arvif = rcu_dereference(ahvif->link[params->link_id]);
	}
	if (!arvif) {
		rcu_read_unlock();
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "vif: %p param: %d value: %d if: %d link: %d\n",
		   vif, param, value,
		   params->ifindex, params->link_id);

	ret = ath12k_vendor_set_arvif_params(arvif, param, value, &reload);
	rcu_read_unlock();

	if (!ret && reload)
		ath12k_vendor_event_iface_reload_link(wiphy, wdev, params->link_id);

	return ret;
}

/* Find the 'ar' radio instance using the radio index of the wiphy
 * to set params
 */
static int ath12k_vendor_set_wiphy_params(struct wiphy *wiphy,
					  struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_hw *hw = NULL;
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar = NULL;
	u32 param = params->value;
	u32 *data = (u32 *)params->data;
	u32 value = *data;
	int ret = -1;
	bool reload = false;

	lockdep_assert_wiphy(wiphy);

	if (params->radio_idx == INVALID_RADIO_INDEX)
		return -ENODATA;

	hw = wiphy_to_ieee80211_hw(wiphy);
	ah = hw->priv;
	ar = &ah->radio[params->radio_idx];
	if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
		return -ENODATA;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p param: %d value: %d if: %d radio: %d\n",
		   ar, param, value,
		   params->ifindex, params->radio_idx);

	ret = ath12k_vendor_set_radio_params(ar, param, value, &reload);

	if (!ret && reload)
		ath12k_vendor_event_iface_reload(wiphy, params->radio_idx);

	return ret;
}

/* Get link-vif level parameters */
static int ath12k_vendor_get_arvif_params(struct ath12k_link_vif *arvif,
					  u32 param, u32 *value)
{
	int ret = -1;

	switch (param) {
	case QCA_WLAN_VENDOR_VDEV_PARAM_TEST:
		*value = 0;
		ret = 0;
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

/* Get radio level parameters */
static int ath12k_vendor_get_radio_params(struct ath12k *ar,
					  u32 param, u32 *value)
{
	int ret = -1;

	switch (param) {
	case QCA_WLAN_VENDOR_RADIO_PARAM_TEST:
		*value = 0;
		ret = 0;
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Un-supported param: %d\n", param);
		break;
	}

	return ret;
}

/* Find the link-vif of the wdev with the link id to get params
 * If link id is invalid use the default link
 */
static int ath12k_vendor_get_wifi_params(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 struct ath12k_wifi_generic_params *params,
					 u32 *value)
{
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	int param = params->value;
	int ret = -1;


	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	ah = ahvif->ah;

	rcu_read_lock();
	if (ah && params->link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (params->link_id < ATH12K_NUM_MAX_LINKS)
			arvif = rcu_dereference(ahvif->link[params->link_id]);
	}
	if (!arvif) {
		rcu_read_unlock();
		return -EINVAL;
	}
	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "vif: %p param: %d if: %d link: %d\n",
		   vif, param, params->ifindex, params->link_id);

	ret = ath12k_vendor_get_arvif_params(arvif, param, value);
	rcu_read_unlock();

	return ret;
}

/* Find the 'ar' radio instance using the radio index of the wiphy
 * to get params
 */
static int ath12k_vendor_get_wiphy_params(struct wiphy *wiphy,
					 struct ath12k_wifi_generic_params *params,
					 u32 *value)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;
	int param = params->value;
	int ret = -1;

	if (params->radio_idx == INVALID_RADIO_INDEX)
		return -ENODATA;

	ar = &ah->radio[params->radio_idx];
	if (!ar) {
		ath12k_err(NULL, "Failed to find ar\n");
		return -ENODATA;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ar: %p param: %d if: %d radio: %d\n",
		   ar, param, params->ifindex, params->radio_idx);
	ret = ath12k_vendor_get_radio_params(ar, param, value);

	return ret;
}

/* Extract attributes from the NL message */
static void ath12k_vendor_wifi_extract_generic_command_params(struct nlattr **tb,
							      struct ath12k_wifi_generic_params *params)
{
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND])
		params->command = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE])
		params->value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]) {
		params->data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
		params->data_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH])
		params->length = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS])
		params->flags = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX])
		params->ifindex = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX]);
	else
		params->ifindex = 0xFFFFFFFF;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		params->link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
	else
		params->link_id = INVALID_LINK_ID;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
		params->radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
	else
		params->radio_idx = INVALID_RADIO_INDEX;

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "wifi param: %d data: %p data len: %d ifindex: %d link id: %d radio idx: %d\n",
		   params->value, params->data, params->data_len,
		   params->ifindex, params->link_id, params->radio_idx);
}


static int ath12k_vendor_wifi_config_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	int ppe_vp_type = 0;
	char *type = NULL;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);

	if (ret) {
		ath12k_err(NULL,
			   "Invalid attribute with vendor wifi config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_err(NULL,
			   "wiphy:%p wdev: %p Extract wifi params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid param command received\n");
				return -EINVAL;
			}
			ret = ath12k_vendor_set_wifi_params(wiphy, wdev,
							    &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wifi params \n");
				return -EINVAL;
			}
			break;
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
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

static int ath12k_vendor_wiphy_config_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);

	if (ret) {
		ath12k_err(NULL,
			   "Invalid attribute with vendor wiphy config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wiphy params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid param command received\n");
				return -EINVAL;
			}
			ret = ath12k_vendor_set_wiphy_params(wiphy,
							    &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy params \n");
				return -EINVAL;
			}
			break;
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

static int ath12k_vendor_get_wifi_config_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct sk_buff *skb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	int ret;
	u32 value = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute with vendor wifi config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wifi params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			ret = ath12k_vendor_get_wifi_params(wiphy, wdev,
							    &wifi_params, &value);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wifi params \n");
				return -EINVAL;
			}
			break;
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		if ((nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, value)) ||
		    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, sizeof(u32)))
		    || (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))){
			ret = -EINVAL;
			goto err;
		}
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE]) {
		vif = wdev_to_ieee80211_vif_vlan(wdev, false);
		if (!vif) {
			ret = -EINVAL;
			goto err;
		}

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

		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE, ahvif->dp_vif.ppe_vp_type)) {
			ret = -EINVAL;
			goto err;
		}
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		ath12k_err(NULL,
			   "send failed with err=%d\n", ret);
		return ret;
	}

	return 0;

err:
	ath12k_err(NULL,
		   "get failed with err=%d\n", ret);
	kfree_skb(skb);
	return ret;
}

static int ath12k_vendor_get_wiphy_config_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct sk_buff *skb;
	int ret;
	u32 value = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute with vendor wiphy config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wiphy params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			ret = ath12k_vendor_get_wiphy_params(wiphy,
							    &wifi_params, &value);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wifi params \n");
				return -EINVAL;
			}
			break;
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		if ((nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, value)) ||
		    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, sizeof(u32)))
		    || (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))){
			ret = -EINVAL;
			goto err;
		}
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		ath12k_err(NULL,
			   "send failed with err=%d\n", ret);
		return ret;
	}

	return 0;

err:
	ath12k_err(NULL,
		   "put failed with err=%d\n", ret);
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

static const struct nla_policy
ath12k_afc_clear_payload_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
};

static int ath12k_vendor_clear_afc_payload(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	u8 link_id = 0;
	int err;

	if (!wdev)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_clear_payload_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	err = ath12k_wmi_send_afc_cmd_tlv(ar, QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
					  WMI_AFC_CMD_CLEAR_AFC_PAYLOAD);

	return err;
}

static const struct nla_policy
ath12k_afc_reset_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
};

static int ath12k_vendor_reset_afc(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	u8 link_id = 0;
	int err;

	if (!wdev)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_reset_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	err = ath12k_wmi_send_afc_cmd_tlv(ar, QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
					  WMI_AFC_CMD_RESET_AFC);

	return err;
}

static const struct nla_policy
ath12k_cfg80211_power_mode_set_policy[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID] = {.type = NLA_U8 },
};

static int ath12k_vendor_6ghz_power_mode_change(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct ath12k *ar;
	u8 link_id = 0;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX + 1];
	u8 ap_6ghz_pwr_mode;
	int err;

	if (!wdev)
		return -EINVAL;

	if (wdev->iftype != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "Invalid iftype %d for 6 GHz power mode change",
			   wdev->iftype);
		return -EINVAL;
	}

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX, data,
		      data_len, ath12k_cfg80211_power_mode_set_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE parsing failed");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID]) {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID]);
		if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
			ath12k_err(NULL, "Invalid link id %d", link_id);
			return -EINVAL;
		}
	}

	if (!wdev->links[link_id].ap.beacon_interval) {
		ath12k_err(NULL, "Beacon interval not set for link id %d",
			   link_id);
		return -EOPNOTSUPP;
	}

	if (!wdev->links[link_id].ap.chandef.chan ||
		wdev->links[link_id].ap.chandef.chan->band != NL80211_BAND_6GHZ) {
		ath12k_err(NULL, "Invalid channel / band for link id %d",
			   link_id);
		return -EINVAL;
	}

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

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "6 GHz power mode change request for link id %d, pwr mode %d\n",
		   link_id, ap_6ghz_pwr_mode);
	err = ieee80211_6ghz_power_mode_change(wiphy, wdev,
					       ap_6ghz_pwr_mode, link_id);

	return err;
}

int ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						       struct wireless_dev *wdev,
						       u8 link_id)
{
	struct sk_buff *vendor_event;
	int ret = 0;
	int vendor_buffer_len = nla_total_size(sizeof(u8));
	u8 ap_power_mode = wdev->links[link_id].reg_6g_power_mode;

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

	if (wdev->valid_links) {
		ret = nla_put_u8(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID, link_id);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to put 6 GHz link id\n");
			goto out;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Send power mode update complete for Link id %d\n", link_id);
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

static const struct nla_policy
ath12k_vendor_sdwf_dev_policy[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_sdwf_streaming[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_telemetric_sla_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC] = {.type = NLA_BINARY,
								 .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC] = {.type = NLA_BINARY,
								     .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC] = {.type = NLA_U8},
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
		ath12k_telemetry_set_svclass_cfg(true, svc_id,
						 param_dl.min_data_rate,
						 param_dl.mean_data_rate,
						 param_dl.burst_size,
						 param_dl.min_service_interval,
						 param_dl.delay_bound,
						 param_dl.msdu_life_time,
						 param_dl.msdu_delivery_info);
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
	if (dl_qos_id != QOS_ID_INVALID) {
		ret = ath12k_qos_disable(ab, NULL,
					 QOS_PROFILE_DL,
					 dl_qos_id,
					 NULL);
		ath12k_telemetry_set_svclass_cfg(false, svc_id, 0, 0,
						 0, 0, 0, 0, 0);
	}

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

static int ath12k_vendor_parse_rm(struct wiphy *wiphy, struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX + 1];
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX,
			data, data_len, ath12k_vendor_rm_generic_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]) {
		ath12k_err(NULL, "invalid attributes provided for QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return ret;
	}

	return ath12k_vendor_parse_rm_erp(wiphy, wdev,
					  tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]);
}

static const struct nla_policy
ath12k_reg_get_eirp_policy[QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE] = { .type = NLA_U8 },
};

/**
 * wlan_cfg80211_send_reg_eirp_update - Send EIRP update to userspace
 * @wiphy: Pointer to the wiphy structure
 * @eirp_list: Array of channel_power structures
 * @n_channels: Number of channels in the list
 *
 * This function packages the EIRP data (center frequency, channel number,
 * and transmit power) into nested netlink attributes and sends it to
 * userspace via a vendor command reply.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int wlan_cfg80211_send_reg_eirp_update(struct wiphy *wiphy,
					      struct channel_power *eirp_list,
					      u8 n_channels)
{
	struct sk_buff *skb;
	struct nlattr *nla_attr;
	int i;

	if (!eirp_list || n_channels == 0)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	/* Center Frequency */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].center_freq))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	/* Channel Number */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].chan_num))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	/* Transmit Power */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].tx_power))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

/**
 * ath12k_vendor_get_reg_eirp_handler - Handle vendor command to get 6 GHz EIRP data
 * @wiphy: Pointer to the wiphy structure
 * @wdev: Pointer to the wireless device
 * @data: Pointer to vendor command attributes
 * @data_len: Length of the vendor command data
 *
 * This function parses vendor attributes to determine the 6 GHz AP/client power mode,
 * retrieves the corresponding regulatory EIRP data, and sends it back to userspace.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int ath12k_vendor_get_reg_eirp_handler(struct wiphy *wiphy, struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	enum wmi_reg_6g_client_type client_type = WMI_REG_MAX_CLIENT_TYPE;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX + 1];
	enum wmi_reg_6g_ap_type ap_6ghz_pwr_mode;
	struct ieee80211_supported_band *band;
	struct channel_power *chan_eirp_list;
	bool is_client_needed;
	struct ath12k *ar;
	int ret_val = 0;
	u8 link_id;

	if (!wdev || !data || !data_len) {
		ath12k_err(NULL, "Invalid input to EIRP handler");
		return -EINVAL;
	}

	for_each_valid_link(wdev, link_id) {
		if (wdev->links[link_id].ap.chandef.chan &&
		    wdev->links[link_id].ap.chandef.chan->band == NL80211_BAND_6GHZ)
			break;
	}
	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	band = &ar->mac.sbands[NL80211_BAND_6GHZ];
	if (!band || band->n_channels == 0)
		return -EINVAL;

	chan_eirp_list = kzalloc(band->n_channels * sizeof(*chan_eirp_list), GFP_KERNEL);
	if (!chan_eirp_list)
		return -ENOMEM;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE, data,
		      data_len, ath12k_reg_get_eirp_policy, NULL)) {
		ath12k_err(NULL, "Failed to parse EIRP vendor attributes");
		ret_val = -EINVAL;
		goto free_eirp;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE]) {
		ath12k_err(NULL, "Missing mandatory EIRP power type attribute");
		ret_val = -EINVAL;
		goto free_eirp;
	}

	ap_6ghz_pwr_mode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE]);
	if (ap_6ghz_pwr_mode < WMI_REG_INDOOR_AP || ap_6ghz_pwr_mode > WMI_REG_VLP_AP) {
		ath12k_err(NULL, "Invalid 6 GHz AP power mode: %u", ap_6ghz_pwr_mode);
		ret_val = -EINVAL;
		goto free_eirp;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE])
		client_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE]);
	if (client_type >= WMI_REG_MAX_CLIENT_TYPE) {
		ath12k_err(NULL, "Invalid client type: %u", client_type);
		ret_val = -EINVAL;
		goto free_eirp;
	}

	is_client_needed = (client_type < WMI_REG_MAX_CLIENT_TYPE);

	ret_val = ath12k_mac_reg_get_max_reg_eirp_from_chan_list(ar, ap_6ghz_pwr_mode,
								 client_type, is_client_needed,
								 chan_eirp_list);
	if (ret_val) {
		ath12k_err(NULL, "Failed to retrieve regulatory EIRP data");
		goto free_eirp;
	}

	ret_val = wlan_cfg80211_send_reg_eirp_update(wiphy, chan_eirp_list, band->n_channels);

free_eirp:
	kfree(chan_eirp_list);
	return ret_val;
}

static int ath12k_vendor_sdwf_streaming_stats_configure(struct wireless_dev *wdev,
							struct nlattr *streaming_stats)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX + 1];
	struct ath12k *ar = NULL;
	int ret = 0;
	u8 basic_stats_configure, extnd_stats_configure, link_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX,
			       streaming_stats,
			       ath12k_vendor_sdwf_streaming, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid sawf streaming stats configuration\n");
		return ret;
	}

	if (wdev->valid_links) { /* MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID])
			return -EINVAL;
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID]);
		if (!(wdev->valid_links & BIT(link_id)))
			return -ENOLINK;
	} else { /* NON-MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID])
			link_id = 0;
		else
			return -EINVAL;
	}

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS]) {
		basic_stats_configure = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS]);
		ret = ath12k_htt_sawf_streaming_stats_configure(ar, HTT_STRM_GEN_MPDUS_STATS,
								basic_stats_configure, 0, 0, 0, 0);
		if (ret)
			return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS]) {
		extnd_stats_configure = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS]);
		ret = ath12k_htt_sawf_streaming_stats_configure(ar, HTT_STRM_GEN_MPDUS_DETAILS_STATS,
								extnd_stats_configure, 0, 0, 0, 0);
	}

	return ret;
}

static int ath12k_vendor_telemetry_sla_reset_stats(struct nlattr *clr_stats)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX + 1];
	int ret = 0;
	u8 svc_id, mac_addr[ETH_ALEN] = { 0 }, mld_mac_addr[ETH_ALEN] = { 0 }, set_clear;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX,
			       clr_stats,
			       ath12k_telemetric_sla_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "Invalid attribute with telemetry sla reset stats command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID])
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC] &&
	    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC]) == ETH_ALEN))
		memcpy(mac_addr, nla_data(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC]),
		       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC] &&
	    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC]) == ETH_ALEN))
		memcpy(mld_mac_addr, nla_data(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC]),
		       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR])
		set_clear = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR]);

	return ath12k_telemetry_reset_peer_stats(mac_addr);
}

static int ath12k_vendor_sdwf_dev_operations(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX + 1];
	u8 sdwf_oper;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX, data, data_len,
			ath12k_vendor_sdwf_dev_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attributes with SAWF device level commands\n");
		goto end;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION]) {
		sdwf_oper = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION]);
	} else {
		ath12k_err(NULL, "SAWF device level operation missing\n");
		ret = -EINVAL;
		goto end;
	}

	switch (sdwf_oper) {
	case QCA_WLAN_VENDOR_SDWF_DEV_OPER_STREAMING_STATS:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS]) {
			ret = ath12k_vendor_sdwf_streaming_stats_configure(wdev, tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS]);
		} else {
			ath12k_err(NULL, "SAWF default streaming statsparameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_DEV_OPER_RESET_STATS:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS]) {
			ret = ath12k_vendor_telemetry_sla_reset_stats(tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS]);
		} else {
			ath12k_err(NULL, "SAWF clear telemetry stats parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	default:
		ath12k_err(NULL, "Invalid operation = %d with SAWF device level commands\n", sdwf_oper);
		ret = -EINVAL;
	}
end:
	return ret;
}

void ath12k_vendor_telemetry_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 svc_id, u8 param, bool set_clear,
					   u8 tid, u8 *mld_addr)
{
	struct nlattr *notify_params;
	struct wireless_dev *wdev;
	struct sk_buff *skb;
	u8 access_category;

	wdev = ieee80211_vif_to_wdev(vif);

	if (!wdev)
		return;

	if (!wdev->wiphy)
		return;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL, "No memory available to send notify breach event\n");
		return;
	}

	switch (tid) {
	case 0:
	case 3:
		access_category = 0; //AC_BE
		break;
	case 1:
	case 2:
		access_category = 1; //AC_BK
		break;
	case 4:
	case 5:
		access_category = 2; //AC_VI
		break;
	case 6:
	case 7:
		access_category = 3; //AC_VO
		break;
	default:
		ath12k_err(NULL, "Invalid TID = %u for notifying breach event\n", tid);
		goto err;
	}

	notify_params = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SDWF_DEV_SLA_BREACHED_PARAMS);
	if (!notify_params)
		goto err;
	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC, ETH_ALEN, mac_addr) ||
	    (mld_addr && nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC,
	    ETH_ALEN, mld_addr)) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID, svc_id) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE, param) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR, set_clear) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC, access_category)) {
		ath12k_err(NULL, "No memory available at NL to send notify breach event\n");
		goto err;
	}

	nla_nest_end(skb, notify_params);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	return;
err:
	kfree(skb);
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
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC,
		.doit = ath12k_vendor_parse_rm,
		.policy = ath12k_vendor_rm_generic_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX,
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
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY,
		.doit = ath12k_vendor_wlan_telemetry_wiphy_getstats,
		.policy = ath12k_wlan_telemetry_req_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV,
		.doit = ath12k_vendor_wlan_telemetry_wdev_getstats,
		.policy = ath12k_wlan_telemetry_req_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_CLEAR_PAYLOAD,
		.doit = ath12k_vendor_clear_afc_payload,
		.policy = ath12k_afc_clear_payload_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_RESET,
		.doit = ath12k_vendor_reset_afc,
		.policy = ath12k_afc_reset_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION,
		.doit = ath12k_vendor_wiphy_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION,
		.doit = ath12k_vendor_get_wiphy_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_GET_REG_EIRP,
		.doit = ath12k_vendor_get_reg_eirp_handler,
		.policy = ath12k_reg_get_eirp_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS,
		.doit = ath12k_vendor_sdwf_dev_operations,
		.policy = ath12k_vendor_sdwf_dev_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX,
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
	[QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT] = {
	      .vendor_id = QCA_NL80211_VENDOR_ID,
	      .subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV,
	},
        [QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX] = {
                .vendor_id = QCA_NL80211_VENDOR_ID,
                .subcmd = QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD
        },
	[QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS,
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
