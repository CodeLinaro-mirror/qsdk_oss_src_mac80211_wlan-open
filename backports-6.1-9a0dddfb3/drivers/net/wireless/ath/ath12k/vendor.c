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
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include "../net/wireless/core.h"
#include "qcn_extns/ath12k_cmn_extn.h"
#include "qcn_extns/vendor_extn.h"
#include "mac.h"
#include "ppe.h"
#include "vendor.h"
#include "telemetry.h"
#include "telemetry_agent_if.h"
#include "erp.h"
#include "vendor_services.h"
#include "dp_peer.h"
#include "dp_mon.h"

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
	[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS] = { .type = NLA_NESTED },
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
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG] = {.type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG] = {.type = NLA_BINARY,
							    .len = ATF_OFFLOAD_MAX_PAYLOAD},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_atf_grouping_param_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]  = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_vendor_atf_offload_peer_config_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_atf_peer_param_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC] = {.type = NLA_BINARY,
						       .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED] = {.type = NLA_FLAG},
};

static const struct nla_policy
ath12k_vendor_atf_offload_sched_duration_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_AC] =  {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION] = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_vendor_atf_offload_ssid_sched_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_pri_link_migrate_policy[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR] = {.type = NLA_BINARY,
							     .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID] = {.type =  NLA_U8},
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
	struct nlattr *nla_attr = NULL;
	struct nlattr *freq_info;
	struct nlattr *opclass_info = NULL;
	struct nlattr *chan_list = NULL;
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

	hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
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
	struct nlattr *nla_attr = NULL;
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

	hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
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

	hw_index = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
					       ar->freq_range.start_freq);
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
	int ret = 0;

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
		ret = -ENOMEM;
		goto out;
	}

	ret = ath12k_afc_expiry_event_update_or_get_len(ar, vendor_event, afc_req);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to update AFC request vendor event\n");
		ret = -EINVAL;
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
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_TTLM_MAPPING] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_PMLO] = {.type = NLA_STRING},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_LINK_BW_NSS_CHANGE] = {
								 .type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_DETSCHED] = {
								 .type = NLA_STRING},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_TTLM_INFO] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF] = {.type = NLA_U8},
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
	[QCA_VENDOR_ATTR_WLAN_FEAT_PROTO] = {.type = NLA_FLAG},
};

int ath12k_extract_feat_inputs(struct nlattr *tb_attr,
			       struct ath12k_telemetry_command *cmd)
{
	struct nlattr *feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_MAX + 1] = {0};
	int ret = 0;

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

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_PROTO])
		cmd->feat.feat_proto = true;

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
	int ret = 0;

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
		payload_size = nla_total_size(sizeof(rx_stats.mcast.packets)) +
				nla_total_size(sizeof(rx_stats.mcast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(rx_stats.ucast.packets)) +
				nla_total_size(sizeof(rx_stats.ucast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(rx_stats.non_amsdu)) +
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

static int ath12k_get_htt_tx_stats_basic_attr_size(void)
{
	int total_size = 0;
	int payload_size;
	int attr_size;

	/* TX Unicast Success (struct dp_pkt_info: num + bytes) */
	payload_size = nla_total_size(sizeof(u64)) +  /* num */
		       nla_total_size(sizeof(u64));    /* bytes */
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* TX mcast Success (struct dp_pkt_info: num + bytes) */
	payload_size = nla_total_size(sizeof(u64)) +  /* num */
		       nla_total_size(sizeof(u64));    /* bytes */
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;
	/* TX PPDUs */
	total_size += nla_total_size(sizeof(u32));

	/* MPDU BASIC - TX MPDUs Success */
	total_size += nla_total_size(sizeof(u32));

	/* TX MPDUs Tried */
	total_size += nla_total_size(sizeof(u32));

	/* Retries MPDU */
	total_size += nla_total_size(sizeof(u32));

	/* RSSI BASIC - Last ACK RSSI */
	total_size += nla_total_size(sizeof(u32));

	/* Average ACK RSSI */
	total_size += nla_total_size(sizeof(u32));

	/* RSSI Chain Array [RSSI_CHAIN_LEN = 8] */
	payload_size = nla_total_size(sizeof(u32) * RSSI_CHAIN_LEN);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* TX Rate */
	total_size += nla_total_size(sizeof(u32));

	return total_size;
}

static int ath12k_get_htt_tx_stats_adv_attr_size(void)
{
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ru_payload;
	int tx_type_payload;
	int mu_payload;

	/* DEBUG/ADV - STBC */
	total_size += nla_total_size(sizeof(u32));

	/* LDPC */
	total_size += nla_total_size(sizeof(u32));

	/* WME AC Type Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u32) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* WME AC Type Bytes Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u64) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Excess Retries Per AC Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u32) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* AMPDU Count */
	total_size += nla_total_size(sizeof(u32));

	/* Non-AMPDU Count */
	total_size += nla_total_size(sizeof(u32));

	/* Num PPDU Cookie Valid */
	total_size += nla_total_size(sizeof(u32));

	/* Last TX Rate MCS */
	total_size += nla_total_size(sizeof(u32));

	/* Multicast Last TX Rate */
	total_size += nla_total_size(sizeof(u32));

	/* Multicast Last TX Rate MCS */
	total_size += nla_total_size(sizeof(u32));

	/* Average TX Rate */
	total_size += nla_total_size(sizeof(u64));

	/* TX Ratecode */
	total_size += nla_total_size(sizeof(u16));

	/* Preamble Puncture Count */
	total_size += nla_total_size(sizeof(u32));

	/* RU Location Array [MAX_RU_LOCATIONS = 16] */
	/* Each dp_tx_pkt_info has: num_mpdu (u32) + mpdu_tried (u32) */
	payload_size = 0;
	ru_payload = nla_total_size(sizeof(u32)) +  /* num_mpdu */
		     nla_total_size(sizeof(u32));   /* mpdu_tried */
	payload_size += nla_total_size_nested(ru_payload * MAX_RU_LOCATIONS);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Transmit Type Array [MAX_TRANSMIT_TYPES = 9] */
	/* Each dp_tx_pkt_info has: num_mpdu (u32) + mpdu_tried (u32) */
	payload_size = 0;
	tx_type_payload = nla_total_size(sizeof(u32)) +  /* num_mpdu */
			  nla_total_size(sizeof(u32));   /* mpdu_tried */
	payload_size += nla_total_size_nested(tx_type_payload * MAX_TRANSMIT_TYPES);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* SU BE PPDU Count (struct pkt_type) */
	/* pkt_type has: mcs_count[MAX_MCS = 17] */
	payload_size = nla_total_size(sizeof(u32) * MAX_MCS);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* MU BE PPDU Count Array [TXRX_TYPE_MU_MAX = 2] */
	/* Each pkt_type has: mcs_count[MAX_MCS = 17] */
	payload_size = 0;
	mu_payload = nla_total_size(sizeof(u32)) * MAX_MCS;
	payload_size += nla_total_size_nested(mu_payload * TXRX_TYPE_MU_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Punctured BW Array [MAX_PUNCTURED_MODE = 5] */
	payload_size = nla_total_size(sizeof(u32) * MAX_PUNCTURED_MODE);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* RTS Success */
	total_size += nla_total_size(sizeof(u32));

	/* RTS Failure */
	total_size += nla_total_size(sizeof(u32));

	/* BAR Count */
	total_size += nla_total_size(sizeof(u32));

	/* NDPA Count */
	total_size += nla_total_size(sizeof(u32));

	/* TX PPDU Duration */
	total_size += nla_total_size(sizeof(u64));

	/* TX Power */
	total_size += nla_total_size(sizeof(u8));

	/* TX msdu flush rsn */
	total_size += nla_total_size(sizeof(u32));

	return total_size;
}

static int ath12k_get_tx_ext_htt_stats_attr_size(void)
{
	struct ath12k_htt_tx_stats htt_tx_stats;
	int htt_payload_size = 0;
	int htt_attr_size;
	int total_size = 0;

	/*HTT stats*/
	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_LEGACY_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_HT_MCS_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_VHT_MCS_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_HE_MCS_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_EHT_MCS_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_BW_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_NSS_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * ATH12K_GI_NUM);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * HTT_PPDU_STATS_PPDU_TYPE_MAX);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u64) * HAL_RX_RU_ALLOC_TYPE_MAX);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(htt_payload_size * ATH12K_COUNTER_TYPE_MAX);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(htt_payload_size * ATH12K_STATS_TYPE_MAX);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(u32) * MAX_MU_GROUP_ID);
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_attr_size = nla_total_size(sizeof(htt_tx_stats.tx_duration)) +
				nla_total_size(sizeof(htt_tx_stats.ba_fails)) +
				nla_total_size(sizeof(htt_tx_stats.ack_fails)) +
				nla_total_size(sizeof(htt_tx_stats.ru_start)) +
				nla_total_size(sizeof(htt_tx_stats.ru_tones));
	htt_payload_size += nla_total_size_nested(htt_attr_size);

	htt_payload_size += ath12k_get_htt_tx_stats_basic_attr_size();

	htt_payload_size += ath12k_get_htt_tx_stats_adv_attr_size();

	/*Parent htt attr size*/
	total_size += nla_total_size_nested(htt_payload_size);

	return total_size;
}

static int ath12k_get_feat_proto_peer_attr_size(void)
{
	int total_size = 0;
	int l3_attr_size, l4_attr_size, l5_attr_size, tx_attr_size, rx_attr_size;

	l3_attr_size = sizeof(u64) * DP_PKT_TYPE_L3_MAX;
	total_size += nla_total_size_nested(l3_attr_size);

	l4_attr_size = sizeof(u64) * DP_PKT_TYPE_L4_MAX;
	total_size += nla_total_size_nested(l4_attr_size);

	l5_attr_size = sizeof(u64) * DP_PKT_TYPE_L5_MAX;
	total_size += nla_total_size_nested(l5_attr_size);

	/* Nested Tx and Rx Levels Proto Event */
	tx_attr_size = nla_total_size_nested(total_size * TX_COMP_MAX);
	rx_attr_size = nla_total_size_nested(total_size * RX_RECV_MAX);

	/* Nested Tx and Rx Proto Event */
	total_size += nla_total_size_nested(tx_attr_size);
	total_size += nla_total_size_nested(rx_attr_size);

	/* Nested Proto Event */
	total_size += nla_total_size_nested(total_size);
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

		payload_size = nla_total_size(sizeof(tx_stats.mcast.packets)) +
				nla_total_size(sizeof(tx_stats.mcast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(tx_stats.ucast.packets)) +
				nla_total_size(sizeof(tx_stats.ucast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(tx_stats.bcast.packets)) +
				nla_total_size(sizeof(tx_stats.bcast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(tx_stats.release_src_not_tqm)) +
				nla_total_size(sizeof(tx_stats.retry_count)) +
				nla_total_size(sizeof(tx_stats.total_msdu_retries)) +
				nla_total_size(sizeof(tx_stats.multiple_retry_count)) +
				nla_total_size(sizeof(tx_stats.ofdma)) +
				nla_total_size(sizeof(tx_stats.amsdu_cnt)) +
				nla_total_size(sizeof(tx_stats.non_amsdu_cnt)) +
				nla_total_size(sizeof(tx_stats.inval_link_id_pkt_cnt));

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

/**
 * ath12k_vendor_get_rx_mon_stats_size() - Calculate size needed for RX monitor stats
 *
 * Calculates the total netlink buffer size required to serialize all RX monitor
 * statistics including basic counters, arrays, and nested rate statistics.
 *
 * Return: Total size in bytes needed for netlink attributes
 */
static int ath12k_vendor_get_rx_mon_stats_size(void)
{
	struct ath12k_rx_peer_stats stats;
	int total_size = 0;
	int payload_size;
	int attr_size, payload_size_pkt, attr_size_pkt, payload_size_user;
	int attr_size_user, attr_size_type, attr_size_dot11;
	int payload_size_ppdu_nss, payload_size_ppdu_mcs;
	int attr_size_ppdu_nss, attr_size_ppdu_mcs, attr_signal_size;
	int mcs_size, nss_size, gi_size, bw_size;

	/* Basic counters */
	total_size += nla_total_size_64bit(sizeof(stats.num_msdu));
	total_size += nla_total_size_64bit(sizeof(stats.num_msdu_bytes));
	total_size += nla_total_size_64bit(sizeof(stats.num_mpdu_fcs_ok));
	total_size += nla_total_size_64bit(sizeof(stats.num_mpdu_fcs_err));
	total_size += nla_total_size_64bit(sizeof(stats.tcp_msdu_count));
	total_size += nla_total_size_64bit(sizeof(stats.udp_msdu_count));
	total_size += nla_total_size_64bit(sizeof(stats.other_msdu_count));
	total_size += nla_total_size_64bit(sizeof(stats.ampdu_msdu_count));
	total_size += nla_total_size_64bit(sizeof(stats.non_ampdu_msdu_count));
	total_size += nla_total_size_64bit(sizeof(stats.stbc_count));
	total_size += nla_total_size_64bit(sizeof(stats.beamformed_count));
	total_size += nla_total_size_64bit(sizeof(stats.dcm_count));
	total_size += nla_total_size_64bit(sizeof(stats.rx_duration));

	/* Coding count array */
	payload_size = nla_total_size_64bit(sizeof(stats.coding_count[0])) *
					    HAL_RX_SU_MU_CODING_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* TID count array */
	payload_size = nla_total_size_64bit(sizeof(stats.tid_count[0])) *
					    (IEEE80211_NUM_TIDS + 1);
	total_size += nla_total_size_nested(payload_size);

	/* Preamble count array */
	payload_size = nla_total_size_64bit(sizeof(stats.pream_cnt[0])) *
					    HAL_RX_PREAMBLE_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* Reception type array */
	payload_size = nla_total_size_64bit(sizeof(stats.reception_type[0])) *
					    HAL_RX_RECEPTION_TYPE_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* RU allocation count array */
	payload_size = nla_total_size_64bit(sizeof(stats.ru_alloc_cnt[0])) *
					    HAL_RX_RU_ALLOC_TYPE_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* Packet stats - nested rate stats structure */
	attr_size = 0;

	/* HT MCS array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.ht_mcs_count[0])) *
					    ATH12K_HT_MCS_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* VHT MCS array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.vht_mcs_count[0])) *
					    ATH12K_VHT_MCS_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* HE MCS array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.he_mcs_count[0])) *
					    ATH12K_HE_MCS_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* BE MCS array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.be_mcs_count[0])) *
					    ATH12K_EHT_MCS_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* NSS array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.nss_count[0])) *
					    ATH12K_NSS_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* BW array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.bw_count[0])) *
					    ATH12K_BW_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* GI array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.gi_count[0])) *
					    ATH12K_GI_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* Legacy rates array */
	payload_size = nla_total_size_64bit(sizeof(stats.pkt_stats.legacy_count[0])) *
					    ATH12K_LEGACY_NUM;
	attr_size += nla_total_size_nested(payload_size);

	/* rx_rate array [BW][GI][NSS][MCS] */
	mcs_size = nla_total_size_64bit(sizeof(stats.pkt_stats.rx_rate[0][0][0][0])) *
					ATH12K_HT_MCS_NUM;
	nss_size = nla_total_size_nested(mcs_size);
	gi_size = nla_total_size_nested(nss_size * ATH12K_NSS_NUM);
	bw_size = nla_total_size_nested(gi_size * ATH12K_GI_NUM);
	attr_size += nla_total_size_nested(bw_size * ATH12K_BW_NUM);

	/* Parent pkt_stats nest and Byte stats nest */
	total_size += nla_total_size_nested(attr_size) * 2;

	total_size += nla_total_size(sizeof(stats.num_msdu_retry_count));

	total_size += nla_total_size_64bit(sizeof(stats.num_mpdus));
	total_size += nla_total_size(sizeof(stats.num_mpdu_retry_count));

	total_size += nla_total_size_64bit(sizeof(stats.num_ppdus));

	total_size += nla_total_size(sizeof(stats.num_bar));
	total_size += nla_total_size(sizeof(stats.num_ndpa));

	total_size += nla_total_size(sizeof(stats.last_rx_rate));
	total_size += nla_total_size(sizeof(stats.rnd_avg_rx_rate));
	total_size += nla_total_size(sizeof(stats.avg_rx_rate));
	total_size += nla_total_size(sizeof(stats.rx_ratecode));

	/* PPDU count array */
	payload_size = nla_total_size(sizeof(stats.ppdu_reception[0]) *
			HAL_RX_RECEPTION_TYPE_MAX);
	total_size += nla_total_size_nested(payload_size);

	/* PPDU nss array */
	payload_size = nla_total_size(sizeof(stats.ppdu_nss[0]) *
			HAL_RX_MAX_NSS);
	total_size += nla_total_size_nested(payload_size);

	/* punc bw array */
	payload_size = nla_total_size(sizeof(stats.punc_bw[0]) *
			MAX_PUNCTURED_MODE);
	total_size += nla_total_size_nested(payload_size);

	/* wireless multimedia AC array */
	payload_size = nla_total_size_64bit(sizeof(u64)) * 2; /* pkts + bytes */
	attr_size += nla_total_size_nested(payload_size) * WME_NUM_AC;
	total_size += nla_total_size_nested(attr_size);

	/* su_ppdu_count stats */
	payload_size_pkt = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX;
	attr_size_pkt   = nla_total_size_nested(payload_size_pkt);
	attr_size_dot11 = attr_size_pkt *
				QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX;
	total_size     += nla_total_size_nested(attr_size_dot11);

	/* proto_type stats */
	payload_size_pkt = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX;
	attr_size_pkt   = nla_total_size_nested(payload_size_pkt);
	attr_size_dot11 = attr_size_pkt *
				QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX;
	total_size     += nla_total_size_nested(attr_size_dot11);

	/* rx_mu stats*/
	payload_size_ppdu_nss = nla_total_size(sizeof(u64)) *
				QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX;
	attr_size_ppdu_nss = nla_total_size_nested(payload_size_ppdu_nss);

	payload_size_ppdu_mcs = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX;
	attr_size_ppdu_mcs = nla_total_size_nested(payload_size_ppdu_mcs);

	payload_size_user =
		nla_total_size(sizeof(u32)) + /* mpdu_cnt_fcs_ok */
		nla_total_size(sizeof(u32)) + /* mpdu_cnt_fcs_err */
		attr_size_ppdu_nss +          /* ppdu nss */
		attr_size_ppdu_mcs;           /* ppdu mcs*/

	attr_size_user = nla_total_size_nested(payload_size_user);
	attr_size_type = attr_size_user * QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MAX;
	attr_size_dot11 = nla_total_size_nested(attr_size_type) *
			   QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX;
	total_size += nla_total_size_nested(attr_size_dot11);

	attr_signal_size  = nla_total_size_64bit(sizeof(stats.signal_stats.snr));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.snr_avg));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.snr_dp));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.snr_dp_avg));

	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.rssi));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.rssi_avg));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.rssi_dp));
	attr_signal_size += nla_total_size_64bit(sizeof(stats.signal_stats.rssi_dp_avg));

	total_size += nla_total_size_nested(attr_signal_size);

	/* Parent RX attr size */
	total_size = nla_total_size_nested(total_size);

	return total_size;
}

static int ath12k_get_dp_peer_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_rx) {
		total_size += ath12k_get_feat_rx_peer_attr_size();
		/* Add size for RX monitor stats */
		total_size += ath12k_vendor_get_rx_mon_stats_size();
	}

	if (cmd->feat.feat_tx) {
		total_size += ath12k_get_feat_tx_peer_attr_size();
		total_size += ath12k_get_tx_ext_htt_stats_attr_size();
	}

	if (cmd->feat.feat_proto)
		total_size += ath12k_get_feat_proto_peer_attr_size();

	if (cmd->feat.feat_sdwftx)
		total_size += ath12k_get_feat_sdwftx_attr_size(cmd);

	if (cmd->feat.feat_sdwfdelay)
		total_size += ath12k_get_feat_sdwfdelay_attr_size(cmd);

	return total_size;
}

static int ath12k_get_feat_proto_vap_attr_size(void)
{
	int total_size = 0;
	int l3_attr_size, l4_attr_size, l5_attr_size;

	l3_attr_size = sizeof(u64) * DP_PKT_TYPE_L3_MAX;
	total_size += nla_total_size_nested(l3_attr_size);

	l4_attr_size = sizeof(u64) * DP_PKT_TYPE_L4_MAX;
	total_size += nla_total_size_nested(l4_attr_size);

	l5_attr_size = sizeof(u64) * DP_PKT_TYPE_L5_MAX;
	total_size += nla_total_size_nested(l5_attr_size);

	/* Nested Tx and Rx Levels Proto Event */
	total_size += nla_total_size_nested(total_size * TX_ENQUEUE_MAX);

	/* Nested Tx and Rx Proto Event */
	total_size += nla_total_size_nested(total_size);

	/* Nested Proto Event */
	total_size += nla_total_size_nested(total_size);
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

		payload_size = nla_total_size(sizeof(ingress_stats.mcast.packets)) +
				nla_total_size(sizeof(ingress_stats.mcast.bytes));
		attr_size += nla_total_size_nested(payload_size);

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

	if (cmd->feat.feat_proto)
		total_size += ath12k_get_feat_proto_vap_attr_size();

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

static int
ath12k_fill_peer_tx_htt_stats_data(struct sk_buff *vendor_event,
				   struct ath12k_dp_link_peer_stats *link_peer_stats,
				   int htt_type,
				   int htt_pkt_info)
{
	struct nlattr *attr;
	struct ath12k_htt_data_stats *htt_tx_data = NULL;
	int i;

	htt_tx_data = &link_peer_stats->tx_stats->stats[htt_type];

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_LEGACY_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats legacy parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_LEGACY_MCS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->legacy[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_LEGACY_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_HT_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats HT parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_HT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->ht[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_HT_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_VHT_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats VHT parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_VHT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->vht[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_VHT_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_HE_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats HE parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_HE_MCS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->he[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_HE_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_EHT_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats EHT parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->eht[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_EHT_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_BW_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats BW parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_BW_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->bw[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_BW_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_NSS_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats NSS parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_NSS_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->nss[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_NSS_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_GI_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats GI parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_GI_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->gi[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_GI_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_PPDU_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats transmit_type parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_PPDU_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->transmit_type[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_PPDU_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_RU_LOC_INFO);
	if (!attr) {
		ath12k_err(NULL, "nla_nest failure: tx htt stats ru_loc parse");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      htt_tx_data->ru_loc[htt_pkt_info][i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put error: type %d subtype = %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_RU_LOC_INFO,
				   i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_ru_loc_mpdu_succ_tried(struct sk_buff *vendor_event,
				   struct ath12k_dp_link_peer_stats
				   *link_peer_stats)
{
	struct nlattr *attr, *ru_attr;
	struct ath12k_htt_tx_stats *tx_stats = NULL;
	int i;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RU_MPDU_SUC_TRD);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: ru loc");
		return -EINVAL;
	}

	tx_stats = link_peer_stats->tx_stats;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_MAX; i++) {
		ru_attr = nla_nest_start(vendor_event, (i + 1));
		if (!ru_attr) {
			ath12k_err(NULL, "nla nest failure: ru loc %d", i);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_TX_PKT_INFO_NUM_MPDU,
				tx_stats->ru_loc_mpdu_succ_tried[i].num_mpdu)) {
			ath12k_err(NULL,
				   "nla put failed: ru loc num mpdu %d", i);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_TX_PKT_INFO_MPDU_TRD,
				tx_stats->ru_loc_mpdu_succ_tried[i].mpdu_tried)) {
			ath12k_err(NULL,
				   "nla put failed: ru loc mpdu tried %d", i);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, ru_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_transmit_type_mpdu_succ_tried(struct sk_buff *vendor_event,
					  struct ath12k_dp_link_peer_stats
					  *link_peer_stats)
{
	struct nlattr *attr, *tx_type_attr;
	struct ath12k_tx_pkt_info *tx_type_info;
	int i;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_MPDU_SUC_TRD);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: transmit type");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_PPDU_MAX; i++) {
		tx_type_attr = nla_nest_start(vendor_event, (i + 1));
		if (!tx_type_attr) {
			ath12k_err(NULL,
				   "nla nest failure: transmit type %d", i);
			return -EINVAL;
		}

		tx_type_info =
			&link_peer_stats->tx_stats->transmit_type_mpdu_succ_tried[i];

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_TX_PKT_INFO_NUM_MPDU,
				tx_type_info->num_mpdu)) {
			ath12k_err(NULL,
				   "nla put failed: transmit type num mpdu %d",
				   i);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_TX_PKT_INFO_MPDU_TRD,
				tx_type_info->mpdu_tried)) {
			ath12k_err(NULL,
				   "nla put failed: transmit type mpdu tried %d",
				   i);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, tx_type_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_su_be_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct nlattr *attr;
	int i;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_SU_BE_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: su be ppdu cnt");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->su_be_ppdu_cnt.mcs_count[i])) {
			ath12k_err(NULL,
				   "nla put failure: su be ppdu mcs %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_mu_be_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct nlattr *attr, *mu_attr;
	struct ath12k_htt_tx_stats *tx_stats = NULL;
	int i, j;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MU_BE_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: mu be ppdu cnt");
		return -EINVAL;
	}

	tx_stats = link_peer_stats->tx_stats;
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_TXRX_MU_MAX;
	     i++) {
		mu_attr = nla_nest_start(vendor_event, (i + 1));
		if (!mu_attr) {
			ath12k_err(NULL,
				   "nla nest failure: mu be ppdu %d", i);
			return -EINVAL;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX; j++) {
			if (nla_put_u32(vendor_event, (j + 1),
					tx_stats->mu_be_ppdu_cnt[i].mcs_count[j])) {
				ath12k_err(NULL,
					   "nla put failure: mu be ppdu mcs %d:%d",
					   i, j);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, mu_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_tx_ext_htt_stats_attr(struct ath12k *ar, struct sk_buff *vendor_event,
				       struct ath12k_dp_link_peer_stats *link_peer_stats,
				       int peer_type)
{
	struct nlattr *attr, *attr1, *attr2;
	struct ath12k_htt_tx_stats *tx_stats = NULL;
	int i, j;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RATE_DATA);
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_DATA_TYPE_MAX;
	     i++) {
		attr1 = nla_nest_start(vendor_event, (i + 1));
		if (!attr1) {
			ath12k_err(NULL, "nla nest failure: tx ext htt data type");
			return -EINVAL;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_PKT_INFO_MAX;
		     j++) {
			attr2 = nla_nest_start(vendor_event, (j + 1));
			if (!attr2) {
				ath12k_err(NULL, "nla nest failure: htt rate data stats");
				return -EINVAL;
			}

			if (ath12k_fill_peer_tx_htt_stats_data(vendor_event,
							       link_peer_stats, i, j)) {
				ath12k_err(NULL, "nla nest failure: htt byte/pkt info");
				return -EINVAL;
			}
			nla_nest_end(vendor_event, attr2);
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_BA_FAILS,
			      link_peer_stats->tx_stats->ba_fails,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: htt stats %d",
			   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_BA_FAILS);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_ACK_FAILS,
			      link_peer_stats->tx_stats->ack_fails,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: htt stats %d",
			   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_ACK_FAILS);
		return -EINVAL;
	}

	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_DUR,
				      link_peer_stats->tx_stats->tx_duration,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: htt stats %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_DUR);
			return -EINVAL;
		}

		if (nla_put_u16(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RU_START,
				link_peer_stats->tx_stats->ru_start)) {
			ath12k_err(NULL, "nla put failed: htt stats %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RU_START);
			return -EINVAL;
		}

		if (nla_put_u16(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RU_TONES,
				link_peer_stats->tx_stats->ru_tones)) {
			ath12k_err(NULL, "nla put failed: htt stats %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RU_TONES);
			return -EINVAL;
		}

		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MU_INFO);
		if (!attr) {
			ath12k_err(NULL, "nla nest failure: htt data stats type %d",
				   QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MU_INFO);
			return -EINVAL;
		}

		for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_MU_GRP_MAX;
		     i++) {
			if (nla_put_u32(vendor_event, (i + 1),
					link_peer_stats->tx_stats->mu_group[i])) {
				ath12k_err(NULL, "nla put failure: htt mu grp info");
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, attr);

		/* RSSI BASIC - Last ACK RSSI */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_LAST_ACK_RSSI,
				link_peer_stats->tx_stats->last_ack_rssi)) {
			ath12k_err(NULL, "nla put failed: last ack rssi");
			return -EINVAL;
		}

		tx_stats = link_peer_stats->tx_stats;
		/* Average ACK RSSI */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_AVG_ACK_RSSI,
				WEIGHTED_AVG_OUT(tx_stats->avg_ack_rssi))) {
			ath12k_err(NULL, "nla put failed: avg ack rssi");
			return -EINVAL;
		}

		/* TX Rate */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_RATE,
				link_peer_stats->tx_stats->tx_rate)) {
			ath12k_err(NULL, "nla put failed: tx rate");
			return -EINVAL;
		}
	}

	/* TX Unicast Success */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_UCAST_SUCC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx ucast success");
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      link_peer_stats->tx_stats->tx_ucast_success.num,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: tx ucast success pkts");
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      link_peer_stats->tx_stats->tx_ucast_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: tx ucast success bytes");
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/* TX Multicast Success */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_MCAST_SUCC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx mcast success");
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      link_peer_stats->tx_stats->tx_mcast_success.num,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: tx mcast success pkts");
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      link_peer_stats->tx_stats->tx_mcast_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failed: tx mcast success bytes");
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/* TX PPDUs */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_PPDUS,
			link_peer_stats->tx_stats->tx_ppdus)) {
		ath12k_err(NULL, "nla put failed: tx ppdus");
		return -EINVAL;
	}

	/* MPDU BASIC - TX MPDUs Success */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_MPDUS_SUCCESS,
			link_peer_stats->tx_stats->tx_mpdus_success)) {
		ath12k_err(NULL, "nla put failed: tx mpdus success");
		return -EINVAL;
	}

	/* TX MPDUs Tried */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_MPDUS_TRIED,
			link_peer_stats->tx_stats->tx_mpdus_tried)) {
		ath12k_err(NULL, "nla put failed: tx mpdus tried");
		return -EINVAL;
	}

	/* Retries MPDU */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RETRIES_MPDU,
			link_peer_stats->tx_stats->retries_mpdu)) {
		ath12k_err(NULL, "nla put failed: retries mpdu");
		return -EINVAL;
	}

	/* RSSI Chain */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RSSI_CHAIN);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: rssi chain");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->rssi_chain[i])) {
			ath12k_err(NULL, "nla put failure: rssi chain %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (!ath12k_dp_advance_stats_enabled(&ar->dp))
		return 0;

	/* DEBUG/ADV - STBC */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_STBC,
			link_peer_stats->tx_stats->stbc)) {
		ath12k_err(NULL, "nla put failed: stbc");
		return -EINVAL;
	}

	/* LDPC */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_LDPC,
			link_peer_stats->tx_stats->ldpc)) {
		ath12k_err(NULL, "nla put failed: ldpc");
		return -EINVAL;
	}

	/* WME AC Type */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_WME_AC_TYPE);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: wme ac type");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_WME_AC_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->wme_ac_type[i])) {
			ath12k_err(NULL, "nla put failure: wme ac type %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* WME AC Type Bytes */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_WME_AC_BYTES);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: wme ac type bytes");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_WME_AC_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      link_peer_stats->tx_stats->wme_ac_type_bytes[i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: wme ac type bytes %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* Excess Retries Per AC */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_EXCESS_RETRY_AC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: excess retries per ac");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_WME_AC_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->excess_retries_per_ac[i])) {
			ath12k_err(NULL, "nla put failure: excess retries per ac %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* AMPDU Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_AMPDU_CNT,
			link_peer_stats->tx_stats->ampdu_cnt)) {
		ath12k_err(NULL, "nla put failed: ampdu cnt");
		return -EINVAL;
	}

	/* Non-AMPDU Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_NON_AMPDU_CNT,
			link_peer_stats->tx_stats->non_ampdu_cnt)) {
		ath12k_err(NULL, "nla put failed: non ampdu cnt");
		return -EINVAL;
	}

	/* Num PPDU Cookie Valid */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_NUM_PPDU_COOKIE_VALID,
			link_peer_stats->tx_stats->num_ppdu_cookie_valid)) {
		ath12k_err(NULL, "nla put failed: num ppdu cookie valid");
		return -EINVAL;
	}
	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		/* Average TX Rate */
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_AVG_RATE,
				      WEIGHTED_AVG_OUT(tx_stats->avg_tx_rate),
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: avg tx rate");
			return -EINVAL;
		}
		/* TX Ratecode */
		if (nla_put_u16(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_RATECODE,
				link_peer_stats->tx_stats->tx_ratecode)) {
			ath12k_err(NULL, "nla put failed: tx ratecode");
			return -EINVAL;
		}
		/* Last TX Rate MCS */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_LAST_RATE_MCS,
				link_peer_stats->tx_stats->last_tx_rate_mcs)) {
			ath12k_err(NULL, "nla put failed: last tx rate mcs");
			return -EINVAL;
		}
		/* TX PPDU Duration */
		if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_PPDU_DURATION,
			      link_peer_stats->tx_stats->tx_ppdu_duration,
			      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: tx ppdu duration");
			return -EINVAL;
		}
		/* TX Power */
		if (nla_put_u8(vendor_event,
			       QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_TX_PWR,
			       link_peer_stats->tx_stats->tx_pwr)) {
			ath12k_err(NULL, "nla put failed: tx pwr");
			return -EINVAL;
		}
	}

	/* Multicast Last TX Rate */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MCAST_LAST_TX_RATE,
			link_peer_stats->tx_stats->mcast_last_tx_rate)) {
		ath12k_err(NULL, "nla put failed: mcast last tx rate");
		return -EINVAL;
	}

	/* Multicast Last TX Rate MCS */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MCAST_LAST_TX_RATE_MCS,
			link_peer_stats->tx_stats->mcast_last_tx_rate_mcs)) {
		ath12k_err(NULL, "nla put failed: mcast last tx rate mcs");
		return -EINVAL;
	}

	/* Preamble Puncture Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_PREAM_PUNCT_CNT,
			link_peer_stats->tx_stats->pream_punct_cnt)) {
		ath12k_err(NULL, "nla put failed: pream punct cnt");
		return -EINVAL;
	}

	/* RU Location Array */
	if (ath12k_fill_ru_loc_mpdu_succ_tried(vendor_event,
					       link_peer_stats))
		return -EINVAL;

	/* Transmit Type Array */
	if (ath12k_fill_transmit_type_mpdu_succ_tried(vendor_event,
						      link_peer_stats))
		return -EINVAL;

	/* SU BE PPDU Count */
	if (ath12k_fill_su_be_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* MU BE PPDU Count */
	if (ath12k_fill_mu_be_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* Punctured BW */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_PUNC_BW);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: punc bw");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_PUNC_BW_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->punc_bw[i])) {
			ath12k_err(NULL, "nla put failure: punc bw %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* RTS Success */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RTS_SUCCESS,
			link_peer_stats->tx_stats->rts_success)) {
		ath12k_err(NULL, "nla put failed: rts success");
		return -EINVAL;
	}

	/* RTS Failure */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_RTS_FAILURE,
			link_peer_stats->tx_stats->rts_failure)) {
		ath12k_err(NULL, "nla put failed: rts failure");
		return -EINVAL;
	}

	/* BAR Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_BAR_CNT,
			link_peer_stats->tx_stats->bar_cnt)) {
		ath12k_err(NULL, "nla put failed: bar cnt");
		return -EINVAL;
	}

	/* NDPA Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_NDPA_CNT,
			link_peer_stats->tx_stats->ndpa_cnt)) {
		ath12k_err(NULL, "nla put failed: ndpa cnt");
		return -EINVAL;
	}
	/* TX MSDU Flush Reason */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_MSDU_FLUSH_RSN);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx msdu flush rsn");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_EXT_HTT_FLUSH_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				link_peer_stats->tx_stats->tx_msdu_flush_rsn[i])) {
			ath12k_err(NULL, "nla put failure: tx msdu flush rsn %d", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_tx_ext_stats_wrapper(struct ath12k *ar, struct sk_buff *vendor_event,
				      struct ath12k_dp_link_peer_stats *link_peer_stats,
				      int peer_type)
{
	struct	nlattr *attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_EXT_HTT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx Ext Htt stats");
		return -EINVAL;
	}

	if (ath12k_fill_peer_tx_ext_htt_stats_attr(ar, vendor_event,
						   link_peer_stats,
						   peer_type)) {
		ath12k_err(NULL, "Error filling Tx Ext Htt Stats");
		return -EINVAL;
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_proto_rx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_peer_stats *peer_stats,
						 u8 level)
{
struct nlattr *attr;
	int i, j;
	u64 stats = 0;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l3[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Rx L3 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l4[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Rx L4 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l5[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Rx L5 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_proto_tx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_peer_stats *peer_stats,
						 u8 level)
{
	struct nlattr *attr;
	int i, j;
	u64 stats = 0;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l3[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Tx L3 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l4[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Tx L4 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l5[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: Tx L5 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].comp_pkt.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].tx_success.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - mcast");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - ucast");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].ucast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].ucast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - bcast");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].bcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].bcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_proto_rx_stats(struct ath12k *ar,
					   struct sk_buff *vendor_event,
					   struct ath12k_dp_peer_stats *peer_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8 level;

	/*Peer Proto Rx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_RX);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_RX_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (ath12k_fill_peer_proto_rx_stats_attrs(vendor_event, peer_stats,
							  level)) {
			ath12k_err(NULL, "Error filling Rx proto stats");
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_peer_proto_tx_stats(struct ath12k *ar,
					   struct sk_buff *vendor_event,
					   struct ath12k_dp_peer_stats *peer_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8  level;

	/*Peer Proto Tx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_TX);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (ath12k_fill_peer_proto_tx_stats_attrs(vendor_event, peer_stats,
							  level)) {
			ath12k_err(NULL, "Error filling Tx proto stats");
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_peer_proto_stats(struct ath12k *ar,
					struct sk_buff *vendor_event,
					struct ath12k_dp_peer_stats *peer_stats)
{
	if (ath12k_fill_peer_proto_tx_stats(ar,
					    vendor_event,
					    peer_stats)) {
		ath12k_err(NULL, "NL failure: Proto tx stats");
		return -EINVAL;
	}

	if (ath12k_fill_peer_proto_rx_stats(ar,
					    vendor_event,
					    peer_stats)) {
		ath12k_err(NULL, "NL failure: Proto rx stats");
		return -EINVAL;
	}
	return 0;
}

static int ath12k_fill_peer_tx_stats(struct ath12k *ar,
				     struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     bool is_extended,
				     struct ath12k_dp_link_peer_stats *link_peer_stats,
				     int peer_type)
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

	/*Tx Ext Htt Stats*/
	if (link_peer_stats &&
	    ath12k_extd_tx_stats_enabled(ar)) {
		if (ath12k_fill_peer_tx_ext_stats_wrapper(ar, vendor_event,
							  link_peer_stats,
							  peer_type)) {
			return -EINVAL;
		}
	}

	return 0;
}

static int
ath12k_fill_vap_proto_tx_stats_attrs(struct sk_buff *vendor_event,
				     struct ath12k_dp_aggr_vif_stats *vif_stats,
				     u8 level)
{
	struct nlattr *attr;
	int i, j;
	u64 stats = 0;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + vif_stats->stats[j].proto->tx[level].l3[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: vap Tx L3 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + vif_stats->stats[j].proto->tx[level].l4[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: vap Tx L4 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + vif_stats->stats[j].proto->tx[level].l5[i];

		if (nla_put_u64_64bit(vendor_event,
				      i + 1, stats, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: vap Tx L5 stats");
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_vap_proto_tx_stats(struct ath12k *ar,
					  struct sk_buff *vendor_event,
					  struct ath12k_dp_aggr_vif_stats *vif_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8  level;

	/*Peer Proto Tx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_VAP);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (ath12k_fill_vap_proto_tx_stats_attrs(vendor_event, vif_stats,
		    level)) {
			ath12k_err(NULL, "Error filling Tx proto stats");
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_vap_proto_stats(struct ath12k *ar,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_vif *vif_stats)
{
	int ret;

	/* Aggregated Peer Proto  Stats */
	ret = ath12k_fill_peer_proto_stats(ar, vendor_event,
					   &(vif_stats->aggr_vif_stats.peer_stats));
	if (ret) {
		ath12k_err(NULL, "Error filling vap tx stats");
		return ret;
	}

	/* Vap proto stats */
	ret = ath12k_fill_vap_proto_tx_stats(ar, vendor_event,
					     &vif_stats->aggr_vif_stats);
	return ret;
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].recv_from_reo.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx mcast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx ucast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].ucast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].ucast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

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

static int ath12k_vendor_fill_rx_wme_ac_stats(struct sk_buff *skb,
					       struct ath12k_rx_peer_total_stats *ac_type)
{
	struct nlattr *nla_ac;
	int i;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX; i++) {
		nla_ac = nla_nest_start(skb, i + 1);
		if (!nla_ac)
			return -EMSGSIZE;

		if (nla_put_u64_64bit(skb,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
				      ac_type[i].total_pkts,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: rx wme pkt attr error");
			nla_nest_cancel(skb, nla_ac);
			return -EMSGSIZE;
		}
		if (nla_put_u64_64bit(skb,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
				      ac_type[i].total_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: rx wme byte attr error");
			nla_nest_cancel(skb, nla_ac);
			return -EMSGSIZE;
		}
		nla_nest_end(skb, nla_ac);
	}
	return 0;
}

/**
 * ath12k_vendor_fill_rx_mon_stats() - Serialize RX signal statistics
 * @skb: Socket buffer for netlink message
 * @signal_stats: Pointer to RX peer signal structure
 *
 * Serializes RX signal statistics to netlink attributes.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int
ath12k_vendor_fill_rx_signal_stats(struct sk_buff *skb,
				   struct ath12k_dp_link_peer_rx_signal_stats *stats)
{
	if (nla_put_u8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR,
		       stats->snr) ||
	    nla_put_u16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_AVG,
			stats->snr_avg) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP,
		       stats->snr_dp) ||
	    nla_put_u16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP_AVG,
			stats->snr_dp_avg) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI,
		       stats->rssi) ||
	    nla_put_s16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_AVG,
			stats->rssi_avg) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP,
		       stats->rssi_dp) ||
	    nla_put_s16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP_AVG,
			stats->rssi_dp_avg))
		return -EMSGSIZE;

	return 0;
}

/**
 * ath12k_vendor_fill_rx_rate_stats() - Serialize RX rate statistics
 * @skb: Socket buffer for netlink message
 * @rate_stats: Pointer to rate statistics structure
 *
 * Serializes rate-specific RX statistics including MCS counts, NSS,
 * bandwidth, guard interval, legacy rates, and the 4D rx_rate array.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int ath12k_vendor_fill_rx_rate_stats(struct sk_buff *skb,
					    struct ath12k_rx_peer_rate_stats *rate_stats)
{
	struct nlattr *rx_rate_attr, *bw_nest, *gi_nest, *nss_nest;
	struct nlattr *legacy_attr, *ht_attr, *vht_attr, *he_attr;
	struct nlattr *eht_attr, *bw_attr, *nss_attr, *gi_attr;
	int i, bw, gi, nss;
	u64 count;
	const int max_mcs = QCA_VENDOR_WLAN_TELEMETRY_HT_MCS_MAX;

	/* HT MCS counts */
	ht_attr = nla_nest_start(skb,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HT_CNT);
	if (!ht_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_HT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->ht_mcs_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, ht_attr);

	/* VHT MCS counts */
	vht_attr = nla_nest_start(skb,
				  QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_VHT_CNT);
	if (!vht_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_VHT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->vht_mcs_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, vht_attr);

	/* HE MCS counts */
	he_attr = nla_nest_start(skb,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_HE_CNT);
	if (!he_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_HE_MCS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->he_mcs_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, he_attr);

	/* EHT MCS counts */
	eht_attr = nla_nest_start(skb,
				  QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_EHT_CNT);
	if (!eht_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->be_mcs_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, eht_attr);

	/* NSS counts */
	nss_attr = nla_nest_start(skb,
				  QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_NSS_CNT);
	if (!nss_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->nss_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, nss_attr);

	/* Bandwidth counts */
	bw_attr = nla_nest_start(skb,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_BW_CNT);
	if (!bw_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_BW_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->bw_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, bw_attr);

	/* Guard interval counts */
	gi_attr = nla_nest_start(skb,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_GI_CNT);
	if (!gi_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_GI_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->gi_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, gi_attr);

	/* Legacy rate counts */
	legacy_attr = nla_nest_start(skb,
				     QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_LEGACY_CNT);
	if (!legacy_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_LEGACY_MCS_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rate_stats->legacy_count[i],
				      NL80211_ATTR_PAD))
			return -EMSGSIZE;
	}
	nla_nest_end(skb, legacy_attr);

	/* 4D rx_rate array: [BW][GI][NSS][MCS] */
	rx_rate_attr = nla_nest_start(skb,
				      QCA_VENDOR_ATTR_RX_RATE_STATS_RX_MON_RATE);
	if (!rx_rate_attr)
		return -EMSGSIZE;

	for (bw = 0; bw < QCA_VENDOR_WLAN_TELEMETRY_BW_MAX; bw++) {
		bw_nest = nla_nest_start(skb, (bw + 1));
		if (!bw_nest)
			goto cancel_rx_rate;

		for (gi = 0; gi < QCA_VENDOR_WLAN_TELEMETRY_GI_MAX; gi++) {
			gi_nest = nla_nest_start(skb, (gi + 1));
			if (!gi_nest)
				goto cancel_bw;

			for (nss = 0; nss < QCA_VENDOR_WLAN_TELEMETRY_NSS_MAX; nss++) {
				nss_nest = nla_nest_start(skb, (nss + 1));
				if (!nss_nest)
					goto cancel_gi;

				for (i = 0; i < max_mcs; i++) {
					count = rate_stats->rx_rate[bw][gi][nss][i];

					/* Skip zero values to reduce size */
					if (count == 0)
						continue;

					if (nla_put_u64_64bit(skb, (i + 1),
							      count,
							      NL80211_ATTR_PAD))
						goto cancel_nss;
				}
				nla_nest_end(skb, nss_nest);
			}
			nla_nest_end(skb, gi_nest);
		}
		nla_nest_end(skb, bw_nest);
	}
	nla_nest_end(skb, rx_rate_attr);

	return 0;

cancel_nss:
	nla_nest_cancel(skb, nss_nest);
cancel_gi:
	nla_nest_cancel(skb, gi_nest);
cancel_bw:
	nla_nest_cancel(skb, bw_nest);
cancel_rx_rate:
	nla_nest_cancel(skb, rx_rate_attr);
	return -EMSGSIZE;
}

static int ath12k_put_rx_mu_stats(struct sk_buff *skb,
				   struct ath12k_rx_peer_user_stats *rx_mu)
{
	struct nlattr *user, *ppdu_nss, *mcs;
	int i, j;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MAX; i++) {
		user = nla_nest_start(skb, i + 1); /* use index as nested tag */
		if (!user)
			return -EMSGSIZE;

		nla_put_u32(skb, QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_OK,
			    rx_mu[i].mpdu_cnt_fcs_ok);
		nla_put_u32(skb, QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_ERR,
			    rx_mu[i].mpdu_cnt_fcs_err);

		ppdu_nss = nla_nest_start(skb,
					  QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_PPDU_NSS);
		if (!ppdu_nss)
			return -EMSGSIZE;

		for (j = 0; j < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX; j++)
			nla_put_u64_64bit(skb, j + 1, rx_mu[i].ppdu_nss[j],
					  NL80211_ATTR_PAD);
		nla_nest_end(skb, ppdu_nss);

		mcs = nla_nest_start(skb,
				     QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MCS_COUNTS);
		if (!mcs)
			return -EMSGSIZE;

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX; j++)
			nla_put_u32(skb, j + 1,
				    rx_mu[i].ppdu.mcs_count[j]);

		nla_nest_end(skb, mcs);

		nla_nest_end(skb, user);
	}
	return 0;
}

/**
 * ath12k_vendor_fill_rx_mon_stats() - Serialize RX monitor statistics
 * @skb: Socket buffer for netlink message
 * @rx_stats: Pointer to RX peer stats structure
 * @is_extended: Boolean flag to control serialization of extended statistics
 *
 * Serializes RX monitor statistics to netlink attributes. Basic counters are
 * always serialized. Extended statistics (large arrays and detailed breakdowns)
 * are only serialized when is_extended is true.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int ath12k_vendor_fill_rx_mon_stats(struct sk_buff *skb,
					   struct ath12k_rx_peer_stats *rx_stats,
					   bool is_extended,
					   int peer_type, struct ath12k *ar)
{
	struct nlattr *coding_attr, *tid_attr, *pream_attr;
	struct nlattr *reception_attr, *ru_attr;
	struct nlattr *pkt_stats_attr, *byte_stats_attr, *signal_stat_attr;
	struct nlattr *ppdu_nss_attr, *punc_bw_attr, *su_ppdu_cnt_attr;
	struct nlattr *nla_wme, *ppdu_cnt_attr;
	struct nlattr *pkt_type_attr, *mu_stats;
	struct nlattr *pkt_type_nest, *pkt_type_mu_stats;
	int i, j;
	struct ath12k_dp_link_peer_rx_signal_stats *signal_stats =
							&rx_stats->signal_stats;
	u32 val;

	/* Basic counters */
	if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU,
			      rx_stats->num_msdu, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_OK,
			      rx_stats->num_mpdu_fcs_ok, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_ERR,
			     rx_stats->num_mpdu_fcs_err, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCP_MSDU_COUNT,
			     rx_stats->tcp_msdu_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_UDP_MSDU_COUNT,
			     rx_stats->udp_msdu_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_OTHER_MSDU_COUNT,
			     rx_stats->other_msdu_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_AMPDU_MSDU_COUNT,
			     rx_stats->ampdu_msdu_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NON_AMPDU_MSDU_COUNT,
			     rx_stats->non_ampdu_msdu_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_STBC_COUNT,
			     rx_stats->stbc_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_BEAMFORMED_COUNT,
			     rx_stats->beamformed_count, NL80211_ATTR_PAD) ||
	   nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_DCM_COUNT,
			     rx_stats->dcm_count, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla failure: RX mon stats");
		return -EMSGSIZE;
	}

	if (peer_type == ATH12K_LEGACY_PEER ||
	    peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_DURATION,
				      rx_stats->rx_duration, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla failure: RX mon stats - rx duration");
			return -EMSGSIZE;
		}
	}

	/* Coding count array */
	coding_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_CODING_COUNT);
	if (!coding_attr)
		return -EMSGSIZE;

	if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_BCC,
			      rx_stats->coding_count[HAL_RX_SU_MU_CODING_BCC],
			      NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_CODING_LDPC,
			      rx_stats->coding_count[HAL_RX_SU_MU_CODING_LDPC],
			      NL80211_ATTR_PAD)) {
		nla_nest_cancel(skb, coding_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, coding_attr);

	/* TID count array */
	tid_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_COUNT);
	if (!tid_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon TID count");
		return -EMSGSIZE;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_NON_QOS - 1; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rx_stats->tid_count[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, tid_attr);
			return -EMSGSIZE;
		}
	}
	/* Non-QoS TID */
	if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_TID_NON_QOS,
			      rx_stats->tid_count[IEEE80211_NUM_TIDS],
			      NL80211_ATTR_PAD)) {
		nla_nest_cancel(skb, tid_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, tid_attr);

	/* Preamble count array */
	pream_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PREAMBLE_COUNT);
	if (!pream_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon preamble count");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PREAMBLE_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rx_stats->pream_cnt[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, pream_attr);
			return -EMSGSIZE;
		}
	}

	nla_nest_end(skb, pream_attr);

	/* Reception type array */
	reception_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RECEP_TYPE);
	if (!reception_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon reception type");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rx_stats->reception_type[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, reception_attr);
			return -EMSGSIZE;
		}
	}

	nla_nest_end(skb, reception_attr);

	/* RU allocation count array */
	ru_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RU_ALLOC_COUNT);
	if (!ru_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon RU alloc count");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_MAX; i++) {
		if (nla_put_u64_64bit(skb, (i + 1),
				      rx_stats->ru_alloc_cnt[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, ru_attr);
			return -EMSGSIZE;
		}
	}

	nla_nest_end(skb, ru_attr);

	/* Packet statistics (nested rate stats) */
	pkt_stats_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKT_STATS);
	if (!pkt_stats_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon packet stats");
		return -EMSGSIZE;
	}

	if (ath12k_vendor_fill_rx_rate_stats(skb, &rx_stats->pkt_stats)) {
		nla_nest_cancel(skb, pkt_stats_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, pkt_stats_attr);

	/* Byte statistics (nested rate stats) */
	byte_stats_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_BYTE_STATS);
	if (!byte_stats_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon byte stats");
		return -EMSGSIZE;
	}

	if (ath12k_vendor_fill_rx_rate_stats(skb, &rx_stats->byte_stats)) {
		nla_nest_cancel(skb, byte_stats_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, byte_stats_attr);

	if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU_BYTES,
			      rx_stats->num_msdu_bytes, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU,
			      rx_stats->num_mpdus, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_PPDU,
			      rx_stats->num_ppdus, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla nest failure: RX mon basic bytes stats");
		return -EMSGSIZE;
	}
	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_RETRY_COUNT,
			rx_stats->num_mpdu_retry_count) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU_RETRY_COUNT,
			rx_stats->num_msdu_retry_count)) {
		ath12k_err(NULL, "nla nest failure: RX mon retry count stats");
		return -EMSGSIZE;
	}


	signal_stat_attr = nla_nest_start(skb,
					  QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_SIGNAL_STATS);
	if (!signal_stat_attr) {
		ath12k_err(NULL, "nla nest failure: RX signal stats");
		return -EMSGSIZE;
	}

	if (ath12k_vendor_fill_rx_signal_stats(skb, signal_stats)) {
		ath12k_err(NULL, "nla put failure: RX signal stats");
		nla_nest_cancel(skb, signal_stat_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, signal_stat_attr);

	/* Extended stats placeholder */
	if (!ath12k_dp_advance_stats_enabled(&ar->dp))
		return 0;

	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_BAR,
			rx_stats->num_bar) ||
		nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_NDPA,
			    rx_stats->num_ndpa)) {
		ath12k_err(NULL, "nla failure: Failed to put extended RX mon stats");
		return -EMSGSIZE;
	}

	ppdu_cnt_attr = nla_nest_start(skb,
				       QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_RECEPTION);
	if (!ppdu_cnt_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MAX; i++) {
		if (nla_put_u64_64bit(skb, i + 1, rx_stats->ppdu_reception[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, ppdu_cnt_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, ppdu_cnt_attr);

	ppdu_nss_attr = nla_nest_start(skb,
				       QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_NSS);
	if (!ppdu_nss_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX; i++) {
		if (nla_put_u64_64bit(skb, i + 1, rx_stats->ppdu_nss[i],
				      NL80211_ATTR_PAD)) {
			nla_nest_cancel(skb, ppdu_nss_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, ppdu_nss_attr);

	/* per mcs pkt type stats */

	pkt_type_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_TYPE);
	if (!pkt_type_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX; i++) {
		pkt_type_nest = nla_nest_start(skb, i + 1);
		if (!pkt_type_nest) {
			nla_nest_cancel(skb, pkt_type_attr);
			return -EMSGSIZE;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX; j++) {
			val = rx_stats->proto_type[i].mcs_count[j];

			if (nla_put_u32(skb, j + 1, val)) {
				nla_nest_cancel(skb, pkt_type_nest);
				nla_nest_cancel(skb, pkt_type_attr);
				return -EMSGSIZE;
			}
		}

		nla_nest_end(skb, pkt_type_nest);
	}
	nla_nest_end(skb, pkt_type_attr);

	/* WME AC stats (nested per AC) */
	nla_wme = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE);
	if (!nla_wme)
		return -ENOBUFS;

	if (ath12k_vendor_fill_rx_wme_ac_stats(skb, rx_stats->wme_ac_type)) {
		nla_nest_cancel(skb, nla_wme);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, nla_wme);

	/* su_ppdu_cnt stats */

	su_ppdu_cnt_attr = nla_nest_start(skb,
					  QCA_VENDOR_ATTR_WLAN_TELEMETRY_SU_PPDU_COUNT);
	if (!su_ppdu_cnt_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX; i++) {
		pkt_type_nest = nla_nest_start(skb, i + 1);
		if (!pkt_type_nest) {
			nla_nest_cancel(skb, su_ppdu_cnt_attr);
			return -EMSGSIZE;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX; j++) {
			val = rx_stats->su_ppdu_count[i].mcs_count[j];
			if (nla_put_u32(skb, j + 1, val)) {
				nla_nest_cancel(skb, pkt_type_nest);
				nla_nest_cancel(skb, su_ppdu_cnt_attr);
				return -EMSGSIZE;
			}
		}

		nla_nest_end(skb, pkt_type_nest);
	}
	nla_nest_end(skb, su_ppdu_cnt_attr);

	punc_bw_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PUNC_BW);
	if (!punc_bw_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNC_BW_MAX; i++) {
		if (nla_put_u32(skb, i + 1, rx_stats->punc_bw[i])) {
			nla_nest_cancel(skb, punc_bw_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, punc_bw_attr);


	/* RX MU stats */
	mu_stats = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MU);
	if (!mu_stats)
		return -EMSGSIZE;

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX; i++) {
		pkt_type_mu_stats = nla_nest_start(skb, i + 1);
		if (!pkt_type_mu_stats)
			return -EMSGSIZE;

		if (ath12k_put_rx_mu_stats(skb, rx_stats->rx_mu[i])) {
			nla_nest_cancel(skb, pkt_type_mu_stats);
			return -EMSGSIZE;
		}
		nla_nest_end(skb, pkt_type_mu_stats);
	}
	nla_nest_end(skb, mu_stats);

	/* Rate stats */
	if (peer_type == ATH12K_LEGACY_PEER ||
	    peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_LAST_RX_RATE,
				rx_stats->last_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RND_AVG_RX_RATE,
				    rx_stats->rnd_avg_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_AVG_RX_RATE,
				    rx_stats->avg_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RATECODE,
				    rx_stats->rx_ratecode)) {
			ath12k_err(NULL, "nla failure: extended RX mon rate stats");
			return -EMSGSIZE;
		}
	}

	return 0;
}

static int ath12k_fill_peer_rx_stats(struct ath12k *ar,
				     struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     struct ath12k_dp_link_peer_stats *link_peer_stats,
				     bool is_extended,
				     int peer_type)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;
	struct ath12k_rx_peer_stats *rx_mon_stats;

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

	/* RX Monitor Stats - gated by extended stats flag */
	if (link_peer_stats && ath12k_extd_rx_stats_enabled(ar)) {
		rx_mon_stats = link_peer_stats->rx_stats;

		if (!rx_mon_stats)
			return 0;

		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_STATS);
		if (!attr) {
			ath12k_err(NULL, "nla nest failure: RX monitor stats");
			return -EINVAL;
		}

		if (ath12k_vendor_fill_rx_mon_stats(vendor_event, rx_mon_stats,
						    is_extended, peer_type, ar)) {
			ath12k_err(NULL, "nla put failure: RX monitor stats");
			nla_nest_cancel(vendor_event, attr);
			return -EMSGSIZE;
		}

		nla_nest_end(vendor_event, attr);
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
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      tx->tx_success.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      tx->tx_failed.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      tx->tx_ingress.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			      tx->dropped.fw_rem.num, NL80211_ATTR_PAD) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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
	struct ath12k_htt_tx_stats *htt_tx_stats;
	struct ath12k *ar = &ahvif->ah->radio[0];
	struct ath12k_dp_proto_stats_peer *proto;
	struct ath12k_rx_peer_stats *rx_mon_stats;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_peer = vmalloc(sizeof(*telemetry_peer));
	if (!telemetry_peer) {
		ath12k_err(NULL, "Allocation failure for peer_stats");
		return -ENOMEM;
	}

	memset(telemetry_peer, 0, sizeof(*telemetry_peer));
	/* Allocate RX stats if requested */
	rx_mon_stats = vmalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats) {
		vfree(telemetry_peer);
		return -ENOMEM;
	}
	memset(rx_mon_stats, 0,
	       sizeof(struct ath12k_rx_peer_stats));
	telemetry_peer->link_peer_stats.rx_stats = rx_mon_stats;

	htt_tx_stats = vzalloc(sizeof(*htt_tx_stats));
	if (!htt_tx_stats) {
		vfree(telemetry_peer);
		return -ENOMEM;
	}
	telemetry_peer->peer_type = ATH12K_PEER_INVAL;
	telemetry_peer->link_peer_stats.tx_stats = htt_tx_stats;

	proto = vzalloc(sizeof(*proto));
	if (!proto) {
		vfree(htt_tx_stats);
		vfree(rx_mon_stats);
		vfree(telemetry_peer);
		return -ENOMEM;
	}
	telemetry_peer->peer_stats.proto = proto;

	if (ath12k_dp_get_peer_stats(ahvif, telemetry_peer, cmd->mac,
				     cmd->link_id)) {
		ath12k_err(NULL, "Error getting peer stats from dp");
		goto out;
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_tx_stats(ar,
						      vendor_event,
						      &telemetry_peer->peer_stats,
						      telemetry_peer->is_extended,
						      &telemetry_peer->link_peer_stats,
						      telemetry_peer->peer_type)) {
				ath12k_err(NULL, "nla put failure: Sta tx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta tx feat stats");
			goto out;
		}
	}
	if (cmd->feat.feat_proto) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_EVENT);
		if (attr) {
			if (ath12k_fill_peer_proto_stats(ar,
							 vendor_event,
							 &telemetry_peer->peer_stats)) {
				ath12k_err(NULL, "nla put failure: Proto stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta Proto feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_rx_stats(ar,
						      vendor_event,
						      &telemetry_peer->peer_stats,
						      &telemetry_peer->link_peer_stats,
						      telemetry_peer->is_extended,
						      telemetry_peer->peer_type)) {
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
	vfree(proto);
	vfree(htt_tx_stats);
	vfree(rx_mon_stats);
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

	if (cmd->link_id != INVALID_LINK_ID &&
	    !(ahvif->links_map & BIT(cmd->link_id))) {
		ath12k_err(NULL, "Invalid link_id %d in peer stats setup",
			   cmd->link_id);
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

static int ath12k_fill_vap_rx_stats(struct ath12k *ar,
				    struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/* Aggregated Peer Rx Stats */
	ret = ath12k_fill_peer_rx_stats(ar,
					vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					&telemetry_vif->aggr_vif_stats.link_peer_stats,
					telemetry_vif->is_extended,
					ATH12K_PEER_INVAL);

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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->recv_from_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
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

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats mcast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

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

static int ath12k_fill_vap_tx_stats(struct ath12k *ar,
				    struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/* Aggregated sta tx Stats */
	ret = ath12k_fill_peer_tx_stats(ar,
					vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					telemetry_vif->is_extended,
					&telemetry_vif->aggr_vif_stats.link_peer_stats,
					ATH12K_PEER_INVAL);
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
	struct ath12k *ar = &ahvif->ah->radio[0];
	struct nlattr *attr;
	struct ath12k_htt_tx_stats *htt_tx_stats;
	struct ath12k_rx_peer_stats *rx_mon_stats;
	struct ath12k_dp_proto_stats_peer *peer_proto;
	struct ath12k_dp_proto_stats_vif *vif_proto;
	u8 index;
	int ret = -EINVAL;

	telemetry_vif = vmalloc(sizeof(*telemetry_vif));
	if (!telemetry_vif) {
		ath12k_err(NULL, "Allocation failed for vap stats");
		return -ENOMEM;
	}

	memset(telemetry_vif, 0, sizeof(*telemetry_vif));

	htt_tx_stats = vzalloc(sizeof(*htt_tx_stats));
	if (!htt_tx_stats) {
		vfree(telemetry_vif);
		return -ENOMEM;
	}
	telemetry_vif->aggr_vif_stats.link_peer_stats.tx_stats = htt_tx_stats;

	/* Allocate RX monitor stats for vif aggregation */
	rx_mon_stats = vzalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats) {
		vfree(telemetry_vif);
		return -ENOMEM;
	}
	telemetry_vif->aggr_vif_stats.link_peer_stats.rx_stats = rx_mon_stats;

	peer_proto = vzalloc(sizeof(*peer_proto));
	if (!peer_proto) {
		vfree(rx_mon_stats);
		vfree(htt_tx_stats);
		vfree(telemetry_vif);
			return -ENOMEM;
	}
	telemetry_vif->aggr_vif_stats.peer_stats.proto = peer_proto;

	vif_proto = vzalloc(sizeof(*vif_proto) * DP_TCL_NUM_RING_MAX);
	if (!vif_proto) {
		vfree(peer_proto);
		vfree(rx_mon_stats);
		vfree(htt_tx_stats);
		vfree(telemetry_vif);
		return -ENOMEM;
	}

	for (index = 0; index < DP_TCL_NUM_RING_MAX; index++)
		telemetry_vif->aggr_vif_stats.stats[index].proto = (vif_proto + index);

	ath12k_dp_get_vif_stats(ahvif, telemetry_vif, cmd->link_id);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_vap_rx_stats(ar,
						     vendor_event,
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
			if (ath12k_fill_vap_tx_stats(ar,
						     vendor_event,
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

	if (cmd->feat.feat_proto) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_EVENT);
		if (attr) {
			if (ath12k_fill_vap_proto_stats(ar,
							vendor_event,
							telemetry_vif)) {
				ath12k_err(NULL, "nla put failure: vap Proto stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: vap Proto feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(vif_proto);
	vfree(peer_proto);
	vfree(htt_tx_stats);
	vfree(rx_mon_stats);
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

static int ath12k_fill_radio_rx_stats(struct ath12k *ar,
				      struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	int ret;

	/* Aggregated peer rx stats */
	ret = ath12k_fill_peer_rx_stats(ar,
					vendor_event,
					&telemetry_radio->aggr_pdev_stats.peer_stats,
					&telemetry_radio->aggr_pdev_stats.link_peer_stats,
					telemetry_radio->is_extended,
					ATH12K_PEER_INVAL);

	return ret;
}

static int ath12k_fill_radio_tx_stats(struct ath12k *ar,
				      struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	int ret;
	struct ath12k_dp_link_peer_stats *link_peer_stats;

	link_peer_stats = &telemetry_radio->aggr_pdev_stats.link_peer_stats;
	/* Aggregated peer tx stats */
	ret = ath12k_fill_peer_tx_stats(ar,
					vendor_event,
					&telemetry_radio->aggr_pdev_stats.peer_stats,
					telemetry_radio->is_extended,
					link_peer_stats,
					ATH12K_PEER_INVAL);

	return ret;
}

static int ath12k_prepare_radio_vendor_event(struct sk_buff *vendor_event,
					     struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_radio *telemetry_radio;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = dp_pdev->ar->ab;
	struct ath12k_htt_tx_stats *htt_tx_stats;
	struct ath12k_rx_peer_stats *rx_mon_stats;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_radio = vmalloc(sizeof(*telemetry_radio));
	if (!telemetry_radio) {
		ath12k_err(ab, "Allocation failure for radio_stats");
		return -EINVAL;
	}

	memset(telemetry_radio, 0, sizeof(*telemetry_radio));

	htt_tx_stats = vzalloc(sizeof(*htt_tx_stats));
	if (!htt_tx_stats) {
		vfree(telemetry_radio);
		return -ENOMEM;
	}
	telemetry_radio->aggr_pdev_stats.link_peer_stats.tx_stats = htt_tx_stats;

	rx_mon_stats = vzalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats) {
		vfree(telemetry_radio);
		return -ENOMEM;
	}
	telemetry_radio->aggr_pdev_stats.link_peer_stats.rx_stats = rx_mon_stats;

	ath12k_dp_get_pdev_stats(dp_pdev, telemetry_radio);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_rx_stats(ar,
						       vendor_event,
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
			if (ath12k_fill_radio_tx_stats(ar, vendor_event,
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
	vfree(htt_tx_stats);
	vfree(rx_mon_stats);
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
	u8 vap_submode;
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
			ret = ath12k_vendor_set_wifi_params_extn(wiphy, wdev,
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

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
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
#endif

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

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]) {
		vap_submode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]);
		if (vap_submode > QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MAX) {
			ath12k_err(NULL,
				   "%s-Invalid vap_submode: %d for(%s)\n", __func__,
				   wdev->vap_submode, wdev->netdev->name);
			return -EINVAL;
		}

		if (wdev->vap_submode == vap_submode) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "%s-vap_submode: %d for(%s) already configured\n",
				   __func__, wdev->vap_submode, wdev->netdev->name);
			return 0;
		}
		wdev->vap_submode = vap_submode;
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "%s-configured vap_submode: %d for(%s)\n", __func__,
			   wdev->vap_submode, wdev->netdev->name);
	}

	if (ath12k_vendor_set_wifi_config_extn(wiphy, tb, wdev))
		return -EINVAL;

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
			ret = ath12k_vendor_set_wiphy_params_extn(wiphy,
							    &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy params \n");
				return -EINVAL;
			}
			break;

#ifdef CPTCFG_QCN_EXTN
		case QCA_NL80211_VENDOR_RADIO_CONFIG_HWADDR:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid hwaddr received\n");
				return -EINVAL;
			}
			ret = ath12k_vendor_set_wiphy_hwaddr_extn(wiphy,
								  &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy hwaddr\n");
				return -EINVAL;
			}
			break;
#endif /* CPTCFG_QCN_EXTN */

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
	u64 value = 0;

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
			ret = ath12k_vendor_get_wifi_params_extn(wiphy, wdev,
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
		switch (wifi_params.value) {
		case QCA_WLAN_VENDOR_VDEV_PARAM_VDEV_TSF:
			if ((nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
			    value, NL80211_ATTR_PAD)) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
			    sizeof(u64))) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
		default:
			if ((nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, value)) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
			    sizeof(u32))) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
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

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]) {
		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE,
			       wdev->vap_submode)) {
			ath12k_err(NULL,
				   "nla_put failed for ATTR_CONFIG_VAP_SUBMODE\n");
			ret = -EINVAL;
			goto err;
		}
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "%s-vap_submode: %d for(%s)\n", __func__,
			   wdev->vap_submode, wdev->netdev->name);
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
			ret = ath12k_vendor_get_wiphy_params_extn(wiphy,
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
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX + 1] = {0};
	u8 link_id;
	int ret;

	if (WARN_ON(!vif))
		return -EINVAL;

	/* not supported in case of non-ML vif */
	if (!vif->valid_links)
		return -EOPNOTSUPP;

	if (data_len > ETH_ALEN) {
		ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX, data, data_len,
				ath12k_pri_link_migrate_policy, NULL);
		if (ret) {
			ath12k_err(NULL, "Invalid attribute in %s %d\n", __func__, ret);
			return ret;
		}

		if (tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR] &&
		    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR]) == ETH_ALEN)) {
			memcpy(mac_addr,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR]),
			       ETH_ALEN);
		} else {
			ath12k_err(NULL, "invalid MAC address %s\n", mac_addr);
			return -EINVAL;
		}
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID]);
	} else {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID]);
	}

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return -EINVAL;

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
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

int ath12k_vendor_put_umac_migration_notif(struct ieee80211_vif *vif, u8 *mac_addr, u8 link_id)
{
	struct wireless_dev *wdev;
	struct sk_buff *skb;

	wdev = ieee80211_vif_to_wdev(vif);
	if (!wdev)
		return -EINVAL;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR, ETH_ALEN, mac_addr) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID, link_id)) {
		kfree(skb);
		return -1;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;
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

#ifndef CPTCFG_QCN_EXTN
static
#endif
struct ath12k *ath12k_get_ar_from_wdev(struct wireless_dev *wdev, u8 link_id)
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

static const struct nla_policy
ath12k_afc_fetch_power_info_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
};

/**
 * ath12k_validate_afc_fetch_input - Validate input parameters for AFC power
 * info fetch
 * @data: Pointer to input data
 * @data_len: Length of input data
 *
 * This function validates the input parameters for fetching AFC power
 * information. It checks if the wireless device and data pointers are
 * valid and if the data length is non-zero.
 * Returns 0 if the input is valid, otherwise returns a negative error code.
 */
static int ath12k_validate_afc_fetch_input(const void *data, int data_len)
{
	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length or NULL data pointer");
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_parse_afc_fetch_attrs - Parse attributes for AFC power info fetch
 * @data: Pointer to input data
 * @data_len: Length of input data
 * @tb: Array to store parsed attributes
 *
 * This function parses the attributes from the input data for fetching AFC
 * power information. It uses nla_parse to extract the attributes
 * based on the defined policy.
 * Returns 0 on success or a negative error code on failure.
 */
static int ath12k_parse_afc_fetch_attrs(const void *data, int data_len,
					struct nlattr *tb[])
{
	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_fetch_power_info_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	return 0;
}

/**
 * ath12k_get_radio_by_index - Get ath12k instance by radio index
 * @wiphy: Pointer to wiphy
 * @tb: Array of parsed attributes
 *
 * This function retrieves the ath12k instance corresponding to the
 * specified radio index from the parsed attributes. It checks if the
 * radio index attribute is present and valid.
 * Returns a pointer to the ath12k instance or NULL if not found or invalid.
 */
static struct ath12k *ath12k_get_radio_by_index(struct wiphy *wiphy,
						struct nlattr *tb[])
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	u8 radio_id;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
		return NULL;

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio id %d", radio_id);
		return NULL;
	}

	return &ah->radio[radio_id];
}

/**
 * ath12k_prepare_and_send_afc_response - Prepare and send AFC response
 * @ar: Pointer to ath12k instance
 * @wiphy: Pointer to wiphy
 * @afc_reg_info: Pointer to AFC regulatory info
 *
 * This function prepares and sends the AFC response to the user space
 * application. It allocates a socket buffer, populates it with the AFC
 * regulatory information, and sends it using cfg80211_vendor_cmd_reply.
 * Returns 0 on success or a negative error code on failure.
 */
static int
ath12k_prepare_and_send_afc_response(struct ath12k *ar,
				     struct wiphy *wiphy,
				     struct ath12k_afc_sp_reg_info *afc_reg_info)
{
	struct sk_buff *skb;
	int skb_buf_len;

	skb_buf_len = ath12k_afc_power_event_update_or_get_len(ar, NULL, afc_reg_info);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, skb_buf_len);
	if (!skb) {
		ath12k_err(ar->ab, "skb alloc failed");
		return -ENOMEM;
	}

	if (ath12k_afc_power_event_update_or_get_len(ar, skb, afc_reg_info)) {
		ath12k_warn(ar->ab, "Failed to update AFC power fetch event");
		kfree_skb(skb);
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
	     "Sending AFC power fetch complete event to user application");

	return cfg80211_vendor_cmd_reply(skb);
}

/**
 * ath12k_vendor_fetch_afc_power_info - Fetch AFC power info from driver
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wireless device
 * @data: Pointer to input data
 * @data_len: Length of input data
 *
 * This function handles the vendor command to fetch AFC power information
 * from the driver. It validates the input, retrieves the appropriate
 * ath12k instance, and prepares the response to be sent back to the user.
 * Returns 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_fetch_afc_power_info(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	struct ath12k_afc_info *afc_info;
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	int err;

	err = ath12k_validate_afc_fetch_input(data, data_len);
	if (err)
		return err;

	err = ath12k_parse_afc_fetch_attrs(data, data_len, tb);
	if (err)
		return err;

	ar = ath12k_get_radio_by_index(wiphy, tb);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL in %s", __func__);
		return -ENODATA;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC fetch power info command received radio_id: %u",
		   nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]));

	if (!ar->supports_6ghz || !ar->afc.is_6ghz_afc_power_event_received) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "6 GHz radio: %u, AFC power event received: %u",
			   ar->supports_6ghz, ar->afc.is_6ghz_afc_power_event_received);
		return -EOPNOTSUPP;
	}

	afc_info = &ar->afc;
	afc_reg_info = afc_info->afc_reg_info;
	if (!afc_reg_info) {
		ath12k_err(NULL, "AFC reg info not found");
		return -EINVAL;
	}

	return ath12k_prepare_and_send_afc_response(ar, wiphy, afc_reg_info);
}

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
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD] = {.type = NLA_U8},
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
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD] = {.type = NLA_U8},
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

	if (ath12k_check_erp_power_down(ab->ag)) {
		ret = ath12k_core_power_up(ab->ag);
		if (ret) {
			ath12k_err(ab, "power up is failed\n");
			return ret;
		}
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
	for (i = (svc_id) ? (svc_id) : (*storage);
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
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID,
				profile->params.tid) ||
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
	t_param.per =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE]);
	t_param.mcs_min_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD]);
	t_param.mcs_max_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD]);
	t_param.retries_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD]);

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
	t_param.per =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE]);
	t_param.mcs_min_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD]);
	t_param.mcs_max_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD]);
	t_param.retries_thres =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD]);

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

static int ath12k_vendor_atf_offload_ssid_group_config(struct ath12k *ar,
						       struct nlattr **tb)
{
	struct nlattr *group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX + 1];
	struct ath12k_atf_group_info *group_info;
	struct nlattr *group_attr;
	int ret, rem, i = 0;

	nla_for_each_nested(group_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG],
			    rem) {
		ret = nla_parse_nested(group, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX,
				       group_attr, ath12k_vendor_atf_grouping_param_policy, NULL);
		if (ret) {
			ath12k_warn(ar->ab, "ATF: Group payload has invalid attributes");
			return -EINVAL;
		}

		if (!group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]) {
			ath12k_warn(ar->ab, "ATF: All group parameters not present");
			return -EINVAL;
		}

		group_info = &ar->atf_table.group_info[i++];
		if (i >= ATH12K_ATF_MAX_GROUPS) {
			ath12k_warn(ar->ab, "ATF: Too many groups, maximum allowed is %d",
				    ATH12K_ATF_MAX_GROUPS);
			return -EINVAL;
		}

		group_info->group_id =
			(u32)nla_get_u8(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX]);
		group_info->group_airtime =
			(u32)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED]);
		group_info->group_policy =
			(u32)nla_get_u8(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY]);
		group_info->unconfigured_peers =
			(u16)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS]);
		group_info->configured_peers =
			(u16)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS]);
		group_info->unconfigured_peers_airtime =
			(u32)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]);
	}
	ar->atf_table.total_groups = i;

	ret = ath12k_wmi_atf_send_group_config(ar);
	if (ret)
		ath12k_warn(ar->ab, "ATF: Failed to send group config");

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
	memcpy(ptr, buf, buf_len);

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

static void ath12k_update_atf_peer_info(struct ath12k *ar,
					struct ath12k_atf_peer_params *peer_param)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_atf_peer_info *param_peer_info;
	struct ath12k_base *ab = ar->ab;
	int i;

	param_peer_info = peer_param->peer_info;

	dp = ath12k_ab_to_dp(ab);
	spin_lock_bh(&dp->dp_lock);

	for (i = 0; i < peer_param->num_peers; i++) {
		link_peer = ath12k_dp_link_peer_find_by_addr(dp, param_peer_info->peer_macaddr);
		if (!link_peer)
			continue;

		link_peer->atf_peer_conf_airtime = param_peer_info->percentage_peer;
		link_peer->atf_group_index = param_peer_info->group_index;
		param_peer_info++;
	}

	spin_unlock_bh(&dp->dp_lock);
}

static int ath12k_vendor_atf_offload_peer_config(struct ath12k *ar,
						 struct nlattr *peer_config)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1];
	struct nlattr *peer_attr;
	struct nlattr *peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX + 1];
	struct ath12k_atf_peer_params atf_peer_param = {0};
	struct ath12k_atf_peer_info *peer_info;
	int ret, rem;
	u32 num_peers = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX,
			       peer_config,
			       ath12k_vendor_atf_offload_peer_config_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "ATF: No data present in ATF peer condig command\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD]) {
		ath12k_err(ar->ab, "ATF: Peer payload is missing");
		return -EINVAL;
	}

	nla_for_each_nested(peer_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD],
			    rem) {
		num_peers++;
	}

	atf_peer_param.num_peers = num_peers;

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE])
		atf_peer_param.atf_flags = FIELD_PREP(WMI_ATF_FULL_UPDATE, 1);

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE])
		atf_peer_param.atf_flags = atf_peer_param.atf_flags |
					   FIELD_PREP(WMI_ATF_PEER_PENDING, 1);

	atf_peer_param.atf_flags = atf_peer_param.atf_flags |
				   FIELD_PREP(WMI_ATF_PEER_VALID_PDEV, 1);

	atf_peer_param.pdev_id = ar->pdev->pdev_id;

	peer_info = kzalloc(atf_peer_param.num_peers * sizeof(struct ath12k_atf_peer_info),
			    GFP_KERNEL);
	if (!peer_info) {
		ath12k_err(ar->ab, "ATF: Failed to allocate memory for peer_info");
		return -ENOMEM;
	}

	num_peers = 0;
	nla_for_each_nested(peer_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD],
			    rem) {
		ret = nla_parse_nested(peer, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX,
				       peer_attr, ath12k_vendor_atf_peer_param_policy, NULL);
		if (ret) {
			ath12k_warn(ar->ab, "ATF: Peer payload is invalid");
			kfree(peer_info);
			return -EINVAL;
		}

		if (!peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC] ||
		    !(nla_len(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC]) == ETH_ALEN) ||
		    !peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME] ||
		    !peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX]) {
			ath12k_warn(ar->ab, "ATF: All peer parameters not present");
			kfree(peer_info);
			return -EINVAL;
		}


		memcpy(peer_info[num_peers].peer_macaddr,
				nla_data(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC]),
					 ETH_ALEN);
		peer_info[num_peers].percentage_peer =
			nla_get_u16(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME]);
		peer_info[num_peers].group_index =
			(u16)nla_get_u8(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX]);
		if (peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED])
			peer_info[num_peers].explicit_peer_flag = 1;
		else
			peer_info[num_peers].explicit_peer_flag = 0;
		num_peers++;
	}

	atf_peer_param.peer_info = peer_info;

	ath12k_update_atf_peer_info(ar, &atf_peer_param);
	ret = ath12k_wmi_atf_send_peer_config(ar, &atf_peer_param);
	if (ret)
		ath12k_warn(ar->ab, "Failed to send peer config");

	kfree(peer_info);
	return ret;
}

static void ath12k_atf_offload_reset_stats(struct ath12k *ar)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_atf_peer_airtime *atf_peer_airtime;
	struct ath12k_pdev_dp *dp = &ar->dp;
	struct ath12k_pdev_dp_stats *pdev_stats = &dp->stats;
	struct ath12k_atf_pdev_airtime *atf_pdev_airtime =
		&pdev_stats->atf_airtime;
	struct ath12k_atf *atf_table = &ar->atf_table;
	struct ath12k_dp *ab_dp = ath12k_ab_to_dp(ab);
	int i;

	memset(atf_pdev_airtime, 0, sizeof(*atf_pdev_airtime));

	for (i = 0; i < atf_table->total_groups; i++) {
		atf_table->group_info[i].atf_actual_airtime = 0;
		atf_table->group_info[i].atf_actual_duration = 0;
		atf_table->group_info[i].atf_actual_ul_duration = 0;
	}

	spin_lock_bh(&ab_dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->pdev_idx != ar->pdev_idx && !peer->sta)
			continue;

		peer->atf_actual_airtime = 0;
		atf_peer_airtime = &peer->atf_peer_airtime;

		memset(atf_peer_airtime, 0, sizeof(*atf_peer_airtime));
	}
	spin_unlock_bh(&ab_dp->dp_lock);
	ar->atf_stats_accum_start_time = ath12k_get_timestamp_in_us();
}

static void ath12k_atf_offload_update_peer_airtime(struct ath12k *ar)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_atf_peer_airtime *atf_peer_airtime;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *ar_dp = &ar->dp;
	struct ath12k_pdev_dp_stats *pdev_stats = &ar_dp->stats;
	struct ath12k_atf_pdev_airtime *atf_pdev_airtime =
		&pdev_stats->atf_airtime;
	u8 group_index = 0xFF;
	u32 peer_airtime, peer_ul_airtime, pdev_actual_airtime = 0, pdev_ul_airtime = 0;
	int ac, i;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		pdev_actual_airtime += atf_pdev_airtime->tx_airtime_consumption[ac];
		pdev_ul_airtime += atf_pdev_airtime->rx_airtime_consumption[ac];
	}

	dp = ath12k_ab_to_dp(ar->ab);
	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->pdev_idx != ar->pdev_idx && !peer->sta)
			continue;
		peer_airtime = 0;
		peer_ul_airtime = 0;

		for (i = 0; i < ar->atf_table.total_groups; i++) {
			if (peer->atf_group_index ==
					ar->atf_table.group_info[i].group_id) {
				group_index = i;
				break;
			}
		}

		atf_peer_airtime = &peer->atf_peer_airtime;

		for (ac = 0; ac < WME_NUM_AC; ac++) {
			peer_airtime += atf_peer_airtime->tx_airtime_consumption[ac].consumption;
			peer_ul_airtime += atf_peer_airtime->rx_airtime_consumption[ac].consumption;
		}

		if (peer_airtime > 0 && pdev_actual_airtime > 0) {
			peer->atf_actual_duration = peer_airtime;
			peer->atf_actual_airtime =
				(u32)div_u64((u64)peer_airtime * 100ULL,  pdev_actual_airtime);
		} else {
			peer->atf_actual_airtime = 0;
			peer->atf_actual_duration = 0;
		}

		if (peer_ul_airtime > 0 && pdev_ul_airtime > 0) {
			peer->atf_actual_ul_duration = peer_ul_airtime;
			peer->atf_ul_airtime =
				(u32)div_u64((u64)peer_ul_airtime * 100ULL, pdev_ul_airtime);
		} else {
			peer->atf_ul_airtime = 0;
			peer->atf_actual_ul_duration = 0;
		}

		if (group_index < ar->atf_table.total_groups) {
			ar->atf_table.group_info[group_index].atf_actual_duration += peer_airtime;
			ar->atf_table.group_info[group_index].atf_actual_ul_duration += peer_ul_airtime;
			ar->atf_table.group_info[group_index].atf_actual_airtime +=
				peer->atf_actual_airtime;
		} else {
			ath12k_warn(ar->ab, "ATF: Invalid group index %u for peer %pM (max: %u)",
				    group_index, peer->addr, ar->atf_table.total_groups - 1);
		}
	}
	spin_unlock_bh(&dp->dp_lock);
	ath12k_info(ar->ab, "Total Airtime(us)     %u", pdev_actual_airtime);
	ath12k_info(ar->ab, "Total UL Airtime(us)  %u", pdev_ul_airtime);
}

static void ath12k_atf_offload_print_stats(struct timer_list *t)
{
	struct ath12k *ar = from_timer(ar, t, atf_stats_timer);
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp;
	u8 borrowed, unused;
	int i, peer_count = 0;
	struct ath12k_atf *atf_table = &ar->atf_table;
	struct atf_peer_stat *peer_stats;
	u64 current_time = ath12k_get_timestamp_in_us();
	u32 time_diff = (u32)(current_time - ar->atf_stats_accum_start_time);

	peer_stats = kcalloc(ATH12K_ATF_MAX_PEERS, sizeof(*peer_stats), GFP_ATOMIC);
	if (!peer_stats) {
		ath12k_warn(ar->ab, "ATF: Failed to allocate memory for peer stats");
		return;
	}

	dp = ath12k_ab_to_dp(ar->ab);

	ath12k_info(ar->ab, "Total radio duration(us): %u", time_diff);
	ath12k_atf_offload_update_peer_airtime(ar);
	ath12k_info(ar->ab, "************************************* ATF STATS For SSID Groups **************************************");
	ath12k_info(ar->ab, "GroupID   Configured   Actual(Relative)   Borrowed   Unused   Duration(us)   ActualUL   UL(us)   Actual");

	for (i = 0; i < atf_table->total_groups; i++) {
		borrowed = 0;
		unused = 0;

		if ((atf_table->group_info[i].group_airtime / 10) >
		    atf_table->group_info[i].atf_actual_airtime)
			unused = (atf_table->group_info[i].group_airtime / 10) -
				 atf_table->group_info[i].atf_actual_airtime;
		else
			borrowed = atf_table->group_info[i].atf_actual_airtime -
				   (atf_table->group_info[i].group_airtime / 10);

		ath12k_info(ar->ab, "  %-9d %-15d %-17d %-8d %-10d %-14d %-6d %-10d %-7d",
			    atf_table->group_info[i].group_id,
			    atf_table->group_info[i].group_airtime / 10,
			    atf_table->group_info[i].atf_actual_airtime,
			    borrowed,
			    unused,
			    ar->atf_table.group_info[i].atf_actual_duration,
			    ar->atf_table.group_info[i].atf_ul_airtime,
			    ar->atf_table.group_info[i].atf_actual_ul_duration,
			    time_diff ?
			    (u32)div_u64((u64)ar->atf_table.group_info[i].atf_actual_duration * 100ULL, time_diff) : 0);
	}

	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->pdev_idx != ar->pdev_idx)
			continue;

		if (!peer->sta)
			continue;

		memcpy(peer_stats[peer_count].addr, peer->addr, ETH_ALEN);
		peer_stats[peer_count].atf_actual_airtime = peer->atf_actual_airtime;
		peer_stats[peer_count].atf_peer_conf_airtime = peer->atf_peer_conf_airtime;
		peer_stats[peer_count].atf_group_index = peer->atf_group_index;
		peer_stats[peer_count].atf_actual_duration = peer->atf_actual_duration;
		peer_stats[peer_count].atf_ul_airtime = peer->atf_ul_airtime;
		peer_stats[peer_count].atf_actual_ul_duration = peer->atf_actual_ul_duration;

		peer_count++;
	}
	spin_unlock_bh(&dp->dp_lock);

	ath12k_info(ar->ab, "*****************************************************************************************************");
	ath12k_info(ar->ab, "**************************************** ATF STATS For PEERs ****************************************");
	ath12k_info(ar->ab, "PeerMAC             GroupId  Configured  Actual(Relative)    Borrowed    Unused    Duration(us)   ActualUL  UL(us)  Actual");

	for (i = 0; i < peer_count; i++) {
		borrowed = 0;
		unused = 0;

		if (peer_stats[i].atf_peer_conf_airtime / 10 > peer_stats[i].atf_actual_airtime)
			unused = (peer_stats[i].atf_peer_conf_airtime / 10) -
				 peer_stats[i].atf_actual_airtime;
		else
			borrowed = peer_stats[i].atf_actual_airtime -
				   (peer_stats[i].atf_peer_conf_airtime / 10);

		ath12k_info(ar->ab, "%-3pM     %-8d %-13d %-19d %-9d %-11d %-13d %-6d %-9d %-7d",
			    peer_stats[i].addr,
			    peer_stats[i].atf_group_index,
			    peer_stats[i].atf_peer_conf_airtime / 10,
			    peer_stats[i].atf_actual_airtime,
			    borrowed,
			    unused,
			    peer_stats[i].atf_actual_duration,
			    peer_stats[i].atf_ul_airtime,
			    peer_stats[i].atf_actual_ul_duration,
			    time_diff ?
			    (u32)div_u64((u64)peer_stats[i].atf_actual_duration * 100ULL, time_diff) : 0);
	}

	ath12k_atf_offload_reset_stats(ar);
	mod_timer(&ar->atf_stats_timer,
		  jiffies + (ar->atf_stats_timeout * HZ));

	kfree(peer_stats);
}

static int ath12k_vendor_offload_sched_duration_config(struct ath12k *ar,
						       struct nlattr *sched_duration_param)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1];
	int ret;
	u32 ac, duration, pdev_id = ar->pdev->pdev_id, value;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX,
			       sched_duration_param,
			       ath12k_vendor_atf_offload_sched_duration_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF schedule duration policy\n");
		return ret;
	}

	ac = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_AC]);
	duration = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION]);

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
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX + 1];
	int ret;
	u8 ssid_cheduling, link_id = 0;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX,
			       ssid_sched_param,
			       ath12k_vendor_atf_offload_ssid_sched_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF SSID schedule config policy\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID]);

	ssid_cheduling = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING]);

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

static int ath12k_vendor_atf_stats_dumpit(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct sk_buff *msg,
					  const void *data,
					  int data_len,
					  unsigned long *storage)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1];
	struct nlattr *peer_attr, *peer_data, *peers_data;
	int ret, j = 0;
	int tailroom = 0, nest_start_length = 0;
	int nest_end_length = 0, nested_range = 0;
	u8 radio_id;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX, data, data_len,
			ath12k_atf_offload_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "AFT: Invalid attributes in ATF stats view\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]) {
		ath12k_err(NULL, "ATF: Missing radio index in ATF stats view\n");
		return -EINVAL;
	}

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]);
	ar = ath12k_ah_to_ar(ah, radio_id);
	if (!ar)
		return -ENODEV;

	ab = ar->ab;
	if (!ab)
		return -ENODEV;

	if (!storage)
		return -ENODATA;

	dp = ath12k_ab_to_dp(ar->ab);

	peer_attr = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS);
	if (!peer_attr)
		return -ENOBUFS;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BE_AIRTIME,
			ar->dp.stats.atf_airtime.tx_airtime_consumption[0]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BK_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[1]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VI_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[2]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VO_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[3]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BE_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[0]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BK_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[1]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VO_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[2]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VI_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[3])) {
		return -ENOBUFS;
	}

	peers_data = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS);
	if (!peers_data)
		return -ENOBUFS;

	tailroom = skb_tailroom(msg);
	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->pdev_idx != ar->pdev_idx && !peer->sta)
			continue;

		if (tailroom <= nested_range)
			break;

		peer_data = nla_nest_start(msg, j++);
		if (!peer_data)
			return -ENOBUFS;

		if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC,
			    ETH_ALEN, peer->addr) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BE_AIRTIME,
				peer->atf_peer_airtime.tx_airtime_consumption[0].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BK_AIRTIME,
				peer->atf_peer_airtime.tx_airtime_consumption[1].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VI_AIRTIME,
				peer->atf_peer_airtime.tx_airtime_consumption[2].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VO_AIRTIME,
				peer->atf_peer_airtime.tx_airtime_consumption[3].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BE_AIRTIME,
				peer->atf_peer_airtime.rx_airtime_consumption[0].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BK_AIRTIME,
				peer->atf_peer_airtime.rx_airtime_consumption[1].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VI_AIRTIME,
				peer->atf_peer_airtime.rx_airtime_consumption[2].consumption) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VO_AIRTIME,
				peer->atf_peer_airtime.rx_airtime_consumption[3].consumption)) {
			nla_nest_cancel(msg, peer_data);
			spin_unlock_bh(&dp->dp_lock);
			return -ENOBUFS;
		}
		nla_nest_end(msg, peer_data);

		*storage += 1;
		nest_end_length = nla_nest_end(msg, peer_data);
		nested_range = nest_end_length - nest_start_length;
		tailroom -= nested_range;
	}
	spin_unlock_bh(&dp->dp_lock);
	nla_nest_end(msg, peers_data);
	nla_nest_end(msg, peer_attr);
	if (*storage == ar->num_peers)
		return msg->len;

	return 0;
}

static void
ath12k_atf_offload_set_atf_stats_timeout(struct ath12k *ar)
{
	if (timer_pending(&ar->atf_stats_timer))
		del_timer_sync(&ar->atf_stats_timer);

	mod_timer(&ar->atf_stats_timer, jiffies + (ar->atf_stats_timeout * HZ));
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
	u8 radio_id, atf_enable, atf_strict_scheduling;
	u16 vo_dedicated_time, vi_dedicated_time;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX, data, data_len,
			ath12k_atf_offload_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attributes with ATF config commands\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]) {
		ath12k_err(NULL, "ATF: Missing radio index in ATF config\n");
		return -EINVAL;
	}
	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]);

	ar = ath12k_ah_to_ar(ah, radio_id);

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED]) {
		atf_enable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED]);
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_DYNAMIC_ENABLE,
						atf_enable, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF: %d\n",
				    atf_enable ? "enable" : "disable",
				    ret);
			return ret;
		}
		ar->commitatf = atf_enable;
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG]) {
		ret = ath12k_vendor_atf_offload_ssid_group_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set SSID config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]) {
		ret = ath12k_vendor_atf_offload_wmm_ac_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set WMM AC config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]) {
		ret = ath12k_vendor_atf_offload_peer_config(ar,
							    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set peer config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED]) {
		ar->atf_stats_enable =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED]);

		if (timer_pending(&ar->atf_stats_timer)) {
			del_timer_sync(&ar->atf_stats_timer);
			return 0;
		}

		if (ar->atf_stats_enable) {
			ath12k_atf_offload_reset_stats(ar);
			ar->atf_stats_timeout = ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT;
			timer_setup(&ar->atf_stats_timer,
				    ath12k_atf_offload_print_stats, 0);
			mod_timer(&ar->atf_stats_timer, jiffies +
				  (ar->atf_stats_timeout * HZ));
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED]) {
		atf_strict_scheduling =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_STRICT_SCH,
						atf_strict_scheduling, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF sctrict scheduling: %d\n",
				    atf_strict_scheduling ? "enable" : "disable",
				    ret);
			return ret;
		}
		ar->atf_strict_scheduling = atf_strict_scheduling;
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]) {
		vo_dedicated_time = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VO_DEDICATED_TIME,
						vo_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vo_dedicated_time  %d\n", ret);
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]) {
		vi_dedicated_time =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VI_DEDICATED_TIME,
						vi_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vi_dedicated_time  %d\n", ret);
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]) {
		ret = ath12k_vendor_offload_sched_duration_config(ar,
								  tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF schedule duration\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]) {
		ret = ath12k_vendor_offload_ssid_scheduling_config(hw, ar, wdev,
								   tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF ssid scheduling\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT]) {
		ar->atf_stats_timeout =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT]);
		ath12k_atf_offload_set_atf_stats_timeout(ar);
		return 0;
	}

	ath12k_err(NULL, "Invalid or missing ATF offload attributes\n");
	return -EINVAL;

}

static int ath12k_vendor_parse_rm(struct wiphy *wiphy, struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX + 1];
	struct ath12k_vendor_service_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX,
			data, data_len, ath12k_vendor_rm_generic_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION])
		info.app_info.app_version =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION]);

	info.app_info.driver_version = 0x1;

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]) {
		ret = ath12k_vendor_parse_rm_erp(wiphy, wdev,
						 tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]);
		return ret;
	} else if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID]) {
		info.id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID]);
		info.service_data = -EINVAL;
		if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA])
			info.service_data =
				nla_get_u64(
					tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA]);

		ath12k_err(NULL, "service id:%d data:%lld\n",
			  info.id, info.service_data);
	} else {
		ath12k_err(NULL, "invalid service id attributes provided for QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF])
		info.init_config_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF]);
	else
		ath12k_err(NULL, "invalid attributes provided for QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");

	ret = ath12k_vendor_initialize_service(wiphy, wdev, &info);
	if (ret) {
		ath12k_err(NULL,
			   "RM Init failed for service id:%d data:%lld\n",
			   info.id, info.service_data);
	}

	return ret;
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

static const struct nla_policy
ath12k_vendor_channel_switch_time_policy[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_PUNCT_BMAP] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL] = { .type = NLA_U32 },
};

int ath12k_get_num_beaconing_vifs(struct ath12k *ar)
{
	u32 count = 0;
	struct ath12k_link_vif *arvif;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_up &&
		    arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
		    arvif->beacon_interval > 0)
			count++;
	}
	return count;
}

static enum nl80211_chan_width
ath12k_vendor_bandwidth_to_chan_width(u32 bandwidth)
{
	switch (bandwidth) {
	case 20:
		return NL80211_CHAN_WIDTH_20;
	case 40:
		return NL80211_CHAN_WIDTH_40;
	case 80:
		return NL80211_CHAN_WIDTH_80;
	case 160:
		return NL80211_CHAN_WIDTH_160;
	case 5:
		return NL80211_CHAN_WIDTH_5;
	case 10:
		return NL80211_CHAN_WIDTH_10;
	case 1:
		return NL80211_CHAN_WIDTH_1;
	case 2:
		return NL80211_CHAN_WIDTH_2;
	case 4:
		return NL80211_CHAN_WIDTH_4;
	case 8:
		return NL80211_CHAN_WIDTH_8;
	case 16:
		return NL80211_CHAN_WIDTH_16;
	default:
		return bandwidth;
	}
}

static int
ath12k_vendor_validate_cs_time_chandef(struct wiphy *wiphy,
				       u32 freq, u32 bandwidth_attr,
				       u32 center_freq1, u32 center_freq2,
				       struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *channel;

	if (!freq)
		return -EINVAL;

	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel)
		return -EINVAL;

	memset(chandef, 0, sizeof(*chandef));
	chandef->chan = channel;
	chandef->width = ath12k_vendor_bandwidth_to_chan_width(bandwidth_attr);

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_5:
	case NL80211_CHAN_WIDTH_10:
	case NL80211_CHAN_WIDTH_1:
	case NL80211_CHAN_WIDTH_2:
	case NL80211_CHAN_WIDTH_4:
	case NL80211_CHAN_WIDTH_8:
	case NL80211_CHAN_WIDTH_16:
		if (center_freq2)
			return -EINVAL;
		if (center_freq1 && center_freq1 != freq)
			return -EINVAL;
		center_freq1 = freq;
		center_freq2 = 0;
		break;
	case NL80211_CHAN_WIDTH_40:
	case NL80211_CHAN_WIDTH_80:
	case NL80211_CHAN_WIDTH_160:
		if (center_freq2)
			return -EINVAL;
		if (!center_freq1)
			center_freq1 = freq;
		center_freq2 = 0;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		if (!center_freq1 || !center_freq2 ||
		    center_freq1 == center_freq2)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	chandef->center_freq1 = center_freq1;
	chandef->center_freq2 = center_freq2;
	return 0;
}

static int ath12k_vendor_get_channel_switch_time(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	u32 beacon_time, restart_time, dfs_time;
	struct ath12k_link_vif *arvif;
	u64 tot_chan_switch_time;
	struct ath12k *ar;
	int dfs_required;
	u8 link_id;
	struct ath12k_base *ab;
	struct sk_buff *reply;
	u32 tgt_restart_time;
	u32 drv_restart_time;
	u32 active_vifs;
	u32 freq = 0, center_freq1 = 0, center_freq2 = 0;
	u32 bandwidth_attr = NL80211_CHAN_WIDTH_20_NOHT;
	u32 channel_switch_time = 0;
	struct cfg80211_chan_def chandef;
	int ret;

	if (!wdev || !wdev->netdev)
		return -EINVAL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "No data provided for channel switch time\n");
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX,
			data, data_len, ath12k_vendor_channel_switch_time_policy,
			NULL);
	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Failed to parse channel switch time attributes: %d\n", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ]) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Channel switch time: missing freq\n");
		return -EINVAL;
	}

	freq = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH])
		bandwidth_attr =
		      nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1])
		center_freq1 =
		   nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2])
		center_freq2 =
		   nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2]);

	ret = ath12k_vendor_validate_cs_time_chandef(hw->wiphy, freq,
						     bandwidth_attr,
						     center_freq1,
						     center_freq2,
						     &chandef);
	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Channel switch time: invalid chandef params (freq=%u width=%u cf1=%u cf2=%u)\n",
			   freq, bandwidth_attr, center_freq1, center_freq2);
		return ret;
	}

	/*
	 * Get beacon interval of link vif that corresponds to the channel.
	 * Use ath12k_mac_select_scan_device to get the ar corresponding
	 * to this channel freq and get corresponding link.
	 */
	ar = ath12k_mac_select_scan_device(hw, vif, center_freq1);
	if (!ar)
		return -EINVAL;

	link_id = ath12k_mac_find_link_id_by_ar(ahvif, ar);
	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	if (!arvif)
		return -EINVAL;
	/*
	 * Accounts for two beacon intervals:
	 * 1. Time to receive CSA completion event from firmware.
	 * 2. Time for firmware to transmit a beacon on the new channel
	 *    after processing the vdev up command from the host.
	 */
	beacon_time = arvif->beacon_interval * 2;

	ab = ar->ab;
	if (!ab)
		return -EINVAL;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "vendor get channel switch time: freq=%u bw=%u cf1=%u cf2=%u\n",
		   freq, bandwidth_attr, center_freq1, center_freq2);

	active_vifs = ath12k_get_num_beaconing_vifs(ar);
	tgt_restart_time = ATH12K_CSA_FW_RESTART_TIME_DELAY;
	drv_restart_time = (active_vifs * ATH12K_CHAN_SWITCH_RESTART_TIME_DELAY);

	restart_time = tgt_restart_time + drv_restart_time;

	if (!ab->qmi.cal_done)
		restart_time += ATH12K_CSA_CALDB_UNDONE_TIME;

	dfs_required = cfg80211_chandef_dfs_required(hw->wiphy, &chandef,
						     vif->type);
	if (dfs_required > 0)
		dfs_time = chandef.chan->dfs_cac_ms;
	else
		dfs_time = 0;

	tot_chan_switch_time = beacon_time + restart_time + dfs_time;
	if (tot_chan_switch_time > U32_MAX)
		channel_switch_time = U32_MAX;
	else
		channel_switch_time = tot_chan_switch_time;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "get channel switch time: beacon=%u restart=%u dfs=%u total=%u\n",
		   beacon_time, restart_time, dfs_time, channel_switch_time);

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!reply)
		return -ENOMEM;

	if (nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL,
			channel_switch_time)) {
		kfree_skb(reply);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(reply);
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

static const char *get_breach_type_str(u8 breach_type)
{
	switch (breach_type) {
	case 0:
		return "RSSI_MIN";
	case 1:
		return "RSSI_MAX";
	case 2:
		return "ACK_RSSI_MIN";
	case 3:
		return "ACK_RSSI_MAX";
	case 4:
		return "TX_RATE_MIN";
	case 5:
		return "TX_RATE_MAX";
	case 6:
		return "RX_RATE_MIN";
	case 7:
		return "RX_RATE_MAX";
	default:
		return "UNKNOWN";
	}
}

/**
 * ath12k_vendor_rssi_rate_notify_breach - Send RSSI/Rate breach NL event
 * This function creates and sends a netlink vendor event to userspace
 * notifying about RSSI/Rate threshold breach.
 */
void ath12k_vendor_rssi_rate_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 breach_type, u32 threshold_value,
					   u32 detected_value, bool set_clear,
					   u8 *mld_addr)
{
	struct nlattr *notify_params;
	struct wireless_dev *wdev;
	struct sk_buff *skb;

	wdev = ieee80211_vif_to_wdev(vif);

	if (!wdev)
		return;

	if (!wdev->wiphy)
		return;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX,
					  GFP_ATOMIC);
	if (!skb) {
		ath12k_err(NULL, "No memory for RSSI/Rate breach event\n");
		return;
	}

	notify_params =
		nla_nest_start(skb,
			       QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RSSI_RATE_BREACH_PARAMS);
	if (!notify_params)
		goto err;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MAC,
		    ETH_ALEN, mac_addr) ||
	    (mld_addr && nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MLD_MAC,
				 ETH_ALEN, mld_addr)) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_TYPE, breach_type))
		goto nla_put_failure;

	if (breach_type <= THRESHOLD_ACKRSSI_MAX) {
		if (nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD,
				(s32)threshold_value) ||
		    nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE,
				(s32)detected_value))
			goto nla_put_failure;
	} else {
		if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD,
				threshold_value) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE,
				detected_value))
			goto nla_put_failure;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_SET_CLEAR,
		       set_clear))
		goto nla_put_failure;

	nla_nest_end(skb, notify_params);
	cfg80211_vendor_event(skb, GFP_ATOMIC);

	if (set_clear) {
		if (breach_type <= THRESHOLD_ACKRSSI_MAX) {
			pr_err("RSSI/Rate Telemetry: %s Breach Detected, peer: %pM, Threshold: %d, Detected Value: %d\n",
			       get_breach_type_str(breach_type), mac_addr,
			       (s32)threshold_value, (s32)detected_value);
		} else {
			pr_err("RSSI/Rate Telemetry: %s Breach Detected, peer: %pM, Threshold: %u, Detected Value: %u\n",
			       get_breach_type_str(breach_type), mac_addr,
			       threshold_value, detected_value);
		}
	}
	return;

nla_put_failure:
	ath12k_err(NULL, "No memory for RSSI/Rate breach NL attributes\n");
err:
	kfree_skb(skb);
}

static struct ath12k_link_vif *
ath12k_vendor_get_non_scan_arvif(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (list_empty(&ar->arvifs))
		return NULL;

	list_for_each_entry(arvif, &ar->arvifs, list)
		if (!arvif->is_scan_vif && arvif->is_started)
			return arvif;

	return list_first_entry(&ar->arvifs, typeof(*arvif), list);
}

int ath12k_vendor_put_ar_hw_link_id(struct sk_buff *vendor_event,
				    struct ath12k *ar)
{
	if (nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID,
			ar->pdev->hw_link_id)) {
		ath12k_err(ar->ab, "failed to put hw link id %u for soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ar_link_mac_addr(struct sk_buff *vendor_event,
				       struct ath12k *ar)
{
	struct ath12k_link_vif *arvif = ath12k_vendor_get_non_scan_arvif(ar);

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LINK_MAC,
		    6, (void *)arvif->bssid)) {
		ath12k_err(ar->ab, "failed to put mac addr for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

enum qca_wlan_vendor_channel_width
ath12k_nl_chan_bw_to_qca_vendor_chan_bw(enum nl80211_chan_width chan_bw)
{
	switch (chan_bw) {
	case NL80211_CHAN_WIDTH_20:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ;
	case NL80211_CHAN_WIDTH_40:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ;
	case NL80211_CHAN_WIDTH_80:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_160:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ;
	case NL80211_CHAN_WIDTH_80P80:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ;
	case NL80211_CHAN_WIDTH_320:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ;
	default:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID;
	}

	return QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID;
}

int ath12k_vendor_put_ar_chan_info(struct sk_buff *vendor_event,
				   struct ath12k *ar)
{
	struct ieee80211_chanctx_conf *ctx = NULL;

	ctx = ath12k_mac_get_first_active_arvif_chanctx(ar);

	if (!ctx)
		return -1;

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW,
		       ath12k_nl_chan_bw_to_qca_vendor_chan_bw(ctx->def.width))) {
		ath12k_err(ar->ab, "failed to put chan bw for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ,
			ctx->def.chan->center_freq)) {
		ath12k_err(ar->ab, "failed to put chan center freq for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ar_nss_chains(struct sk_buff *vendor_event,
				    struct ath12k *ar)
{
	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK,
		       ar->pdev->cap.tx_chain_mask)) {
		ath12k_err(ar->ab, "failed to put tx chain mask for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK,
		     ar->pdev->cap.rx_chain_mask)) {
		ath12k_err(ar->ab, "failed to put rx chain mask for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ab_soc_id(struct sk_buff *vendor_event,
				struct ath12k_base *ab)
{
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID,
		       ab->device_id)) {
		ath12k_err(ab, "failed to put soc device id for soc %d",
			   ab->device_id);
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ab_num_links(struct sk_buff *vendor_event,
				   struct ath12k_base *ab,
				   const int num_active_links)
{
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS,
		       num_active_links)) {
		ath12k_err(ab, "failed to put number of hw links for soc %d",
			   ab->device_id);
		return -1;
	}

	return 0;
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
		.policy = ath12k_pri_link_migrate_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX,
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
		.dumpit = ath12k_vendor_atf_stats_dumpit,
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
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_FETCH_POWER_EVENT,
		.doit = ath12k_vendor_fetch_afc_power_info,
		.policy = ath12k_afc_fetch_power_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
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

#ifdef CPTCFG_QCN_EXTN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO,
		.doit = ath12k_vendor_get_rropinfo,
		.policy = ath12k_rrop_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO,
		.doit = ath12k_vendor_get_sta_240mhz_info,
		.policy = ath12k_240mhz_sta_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_240MHZ_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
		.doit = ath12k_vendor_rule_config_notify,
		.policy = ath12k_rule_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DERIVE_LINK_BSS_ADDR,
		.doit = ath12k_vendor_derive_link_bss_addr_extn,
		.policy = ath12k_wifi_mac_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_CTL_TABLE,
		.doit = ath12k_vendor_ctl_table,
		.policy = ath12k_ctl_table_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX,
		.doit = ath12k_vendor_home_offchan_tx_rx_handler,
		.policy = ath12k_vendor_home_offchan_tx_rx_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG,
		.doit = ath12k_vendor_dcs_config_handler,
		.policy = ath12k_vendor_dcs_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_DCS_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CHANNEL_SWITCH_TIME,
		.doit = ath12k_vendor_get_channel_switch_time,
		.policy = ath12k_vendor_channel_switch_time_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
#endif

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
	[QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
	},
	[QCA_NL80211_VENDOR_SUBCMD_ESP_ESTIMATE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX,
	},
	[QCA_NL80211_VENDOR_SUBCMD_DCS_INTERFERENCE_COMPUTE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG,
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
