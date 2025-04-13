// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/inet.h>
#include "core.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "debugfs.h"
#include "debug.h"
#include "debugfs_htt_stats.h"
#include "qmi.h"
#include "wmi.h"

#define SEGMENT_ID	GENMASK(1,0)
#define CHRIP_ID	BIT(2)
#define OFFSET		GENMASK(10,3)
#define DETECTOR_ID	GENMASK(12,11)
#define FHSS		BIT(14)

int wmi_ctrl_path_awgn_stat(struct ath12k *ar, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_awgn_stats *awgn_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	list_for_each_entry(stats, &ar->debug.wmi_ctrl_path_stats.pdev_stats, list) {

		if (!stats)
			break;
		awgn_stats = stats->stats_ptr;

		if (!awgn_stats)
			break;

		len += scnprintf(buf + len, size - len,
				 "WMI_CTRL_PATH_AWGN_STATS_TLV:\n");
		len += scnprintf(buf + len, size - len,
				 "awgn_send_evt_cnt = %u\n",
				 awgn_stats->awgn_send_evt_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_pri_int_cnt = %u\n",
				 awgn_stats->awgn_pri_int_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_sec_int_cnt = %u\n",
				 awgn_stats->awgn_sec_int_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_pkt_drop_trigger_cnt = %u\n",
				 awgn_stats->awgn_pkt_drop_trigger_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_pkt_drop_trigger_reset_cnt = %u\n",
				 awgn_stats->awgn_pkt_drop_trigger_reset_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_bw_drop_cnt = %u\n",
				 awgn_stats->awgn_bw_drop_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_bw_drop_reset_cnt = %u\n",
				 awgn_stats->awgn_bw_drop_reset_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_cca_int_cnt = %u\n",
				 awgn_stats->awgn_cca_int_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_cca_int_reset_cnt = %u\n",
				 awgn_stats->awgn_cca_int_reset_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_cca_ack_blk_cnt = %u\n",
				 awgn_stats->awgn_cca_ack_blk_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_cca_ack_reset_cnt = %u\n",
				 awgn_stats->awgn_cca_ack_reset_cnt);
		len += scnprintf(buf + len, size - len,
				 "awgn_int_bw_cnt-AWGN_20[0]: %u\n",
				 awgn_stats->awgn_int_bw_cnt[0]);
		len += scnprintf(buf + len, size - len,
				 "AWGN_40[1]: %u\n",
				 awgn_stats->awgn_int_bw_cnt[1]);
		len += scnprintf(buf + len, size - len,
				 "AWGN_80[2]: %u\n",
				 awgn_stats->awgn_int_bw_cnt[2]);
		len += scnprintf(buf + len, size - len,
				 "AWGN_160[3]: %u\n",
				 awgn_stats->awgn_int_bw_cnt[3]);
		len += scnprintf(buf + len, size - len,
				 "AWGN_320[5]: %u\n",
				 awgn_stats->awgn_int_bw_cnt[5]);
	}
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);

	return ret_val;
}

int wmi_ctrl_path_cal_stat(struct ath12k *ar, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int size = 4096;
	char *buf;
	u8 cal_type_mask, cal_prof_mask, is_periodic_cal;
	int len = 0, ret_val;
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_cal_stats *cal_stats;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			"WMI_CTRL_PATH_CAL_STATS\n");
	len += scnprintf(buf + len, size - len,
			"%-25s %-25s %-17s %-16s %-16s %-16s\n",
			"cal_profile", "cal_type",
			"cal_triggered_cnt", "cal_fail_cnt",
			"cal_fcs_cnt", "cal_fcs_fail_cnt");

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	list_for_each_entry(stats, &ar->debug.wmi_ctrl_path_stats.pdev_stats, list) {
		if (!stats)
			break;

		cal_stats = stats->stats_ptr;

		if (!cal_stats)
			break;

		cal_prof_mask = FIELD_GET(WMI_CTRL_PATH_CAL_PROF_MASK,
				cal_stats->cal_info);
		if (cal_prof_mask == WMI_CTRL_PATH_STATS_CAL_PROFILE_INVALID)
			continue;

		cal_type_mask = FIELD_GET(WMI_CTRL_PATH_CAL_TYPE_MASK,
				cal_stats->cal_info);
		is_periodic_cal = FIELD_GET(WMI_CTRL_PATH_IS_PERIODIC_CAL,
				cal_stats->cal_info);


		if (!is_periodic_cal) {
			len += scnprintf(buf + len, size - len,
			   "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			   wmi_ctrl_path_cal_prof_id_to_name(cal_prof_mask),
			   wmi_ctrl_path_cal_type_id_to_name(cal_type_mask),
			   cal_stats->cal_triggered_cnt,
			   cal_stats->cal_fail_cnt,
			   cal_stats->cal_fcs_cnt,
			   cal_stats->cal_fcs_fail_cnt);
		} else {
			len += scnprintf(buf + len, size - len,
			   "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			   "PERIODIC_CAL",
			   wmi_ctrl_path_periodic_cal_type_id_to_name(cal_type_mask),
			   cal_stats->cal_triggered_cnt,
			   cal_stats->cal_fail_cnt,
			   cal_stats->cal_fcs_cnt,
			   cal_stats->cal_fcs_fail_cnt);
		}

	}

	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}

int wmi_ctrl_path_btcoex_stat(struct ath12k *ar, char __user *ubuf,
			size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	list_for_each_entry(stats, &ar->debug.wmi_ctrl_path_stats.pdev_stats, list) {
		if (!stats)
			break;

		btcoex_stats = stats->stats_ptr;

		if (!btcoex_stats)
			break;

		len += scnprintf(buf + len, size - len,
				"WMI_CTRL_PATH_BTCOEX_STATS:\n");
		len += scnprintf(buf + len, size - len,
				"pdev_id = %u\n",
				btcoex_stats->pdev_id);
		len += scnprintf(buf + len, size - len,
				"bt_tx_req_cntr = %u\n",
				btcoex_stats->bt_tx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_rx_req_cntr = %u\n",
				btcoex_stats->bt_rx_req_cntr);
		len += scnprintf(buf + len, size - len,
				"bt_req_nack_cntr = %u\n",
				btcoex_stats->bt_req_nack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_schd_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_schd_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_current_bt_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_current_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_nack_other_wlan_tx_reason_cntr = %u\n",
				btcoex_stats->wl_tx_req_nack_other_wlan_tx_reason_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_in_tx_abort_cntr = %u\n",
				btcoex_stats->wl_in_tx_abort_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_auto_resp_req_cntr = %u\n",
				btcoex_stats->wl_tx_auto_resp_req_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_ack_cntr = %u\n",
				btcoex_stats->wl_tx_req_ack_cntr);
		len += scnprintf(buf + len, size - len,
				"wl_tx_req_cntr = %u\n",
				btcoex_stats->wl_tx_req_cntr);
	}

	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}

static ssize_t ath12k_dump_mgmt_stats(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	int len = 0, ret, i;
	int size = (TARGET_NUM_VDEVS - 1) * 1500;
	char *buf;
	const char *mgmt_frm_type[ATH12K_STATS_MGMT_FRM_TYPE_MAX-1] = {
		"assoc_req", "assoc_resp",
		"reassoc_req", "reassoc_resp",
		"probe_req", "probe_resp",
		"timing_advertisement", "reserved",
		"beacon", "atim", "disassoc",
		"auth", "deauth", "action", "action_no_ack"};

	if (ar->ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	spin_lock_bh(&ar->data_lock);

	list_for_each_entry (arvif, &ar->arvifs, list) {
		if (!arvif)
			break;

		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
			continue;
		mgmt_stats = &arvif->ahvif->mgmt_stats;
		len += scnprintf(buf + len, size - len, "MGMT frame stats for vdev %u :\n", arvif->vdev_id);
		len += scnprintf(buf + len, size - len, "  TX stats :\n ");
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->tx_succ_cnt[i]);

		len += scnprintf(buf + len, size - len, "  Failed frames:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->tx_fail_cnt[i]);

		len += scnprintf(buf + len, size - len, "  RX stats :\n");
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->rx_cnt[i]);

		len += scnprintf(buf + len, size - len, " Tx completion stats :\n");
		len += scnprintf(buf + len, size - len, " success completions:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n",
					mgmt_frm_type[i], mgmt_stats->tx_compl_succ[i]);

		len += scnprintf(buf + len, size - len, " failure completions:\n");

		for (i = 0; i < ATH12K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "        %s: %d\n", mgmt_frm_type[i], mgmt_stats->tx_compl_fail[i]);
	}

	spin_unlock_bh(&ar->data_lock);

	if (len > size)
		len = size;

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	kfree(buf);
	return ret;
}

static const struct file_operations fops_dump_mgmt_stats = {
	.read = ath12k_dump_mgmt_stats,
	.open = simple_open
};

static ssize_t ath12k_debugfs_dump_device_dp_stats(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_device_dp_stats *device_stats = &ab->dp->device_stats;
	int len = 0, i, retval;
	const int size = 4096;
	static const char *rxdma_err[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX] = {
			"Overflow", "MPDU len", "FCS", "Decrypt", "TKIP MIC",
			"Unencrypt", "MSDU len", "MSDU limit", "WiFi parse",
			"AMSDU parse", "SA timeout", "DA timeout",
			"Flow timeout", "Flush req"};
	static const char *reo_err[HAL_REO_DEST_RING_ERROR_CODE_MAX] = {
			"Desc addr zero", "Desc inval", "AMPDU in non BA",
			"Non BA dup", "BA dup", "Frame 2k jump", "BAR 2k jump",
			"Frame OOR", "BAR OOR", "No BA session",
			"Frame SN equal SSN", "PN check fail", "2k err",
			"PN err", "Desc blocked"};

	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len, "SOC RX STATS:\n\n");
	len += scnprintf(buf + len, size - len, "err ring pkts: %u\n",
			 device_stats->err_ring_pkts);
	len += scnprintf(buf + len, size - len, "Invalid RBM: %u\n\n",
			 device_stats->invalid_rbm);
	len += scnprintf(buf + len, size - len, "RXDMA errors:\n");
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s: %u\n",
				 rxdma_err[i], device_stats->rxdma_error[i]);

	len += scnprintf(buf + len, size - len, "\nREO errors:\n");
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s: %u\n",
				 reo_err[i], device_stats->reo_error[i]);

	len += scnprintf(buf + len, size - len, "\nHAL REO errors:\n");
	len += scnprintf(buf + len, size - len,
			 "ring0: %u\nring1: %u\nring2: %u\nring3: %u\n",
			 device_stats->hal_reo_error[0],
			 device_stats->hal_reo_error[1],
			 device_stats->hal_reo_error[2],
			 device_stats->hal_reo_error[3]);

	len += scnprintf(buf + len, size - len, "\nSOC TX STATS:\n");
	len += scnprintf(buf + len, size - len, "\nTCL Ring Full Failures:\n");

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		len += scnprintf(buf + len, size - len, "ring%d: %u\n",
				 i, device_stats->tx_err.desc_na[i]);

	len += scnprintf(buf + len, size - len,
			 "\nMisc Transmit Failures: %d\n",
			 atomic_read(&device_stats->tx_err.misc_fail));


	if (len > size)
		len = size;
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_device_dp_stats = {
	.read = ath12k_debugfs_dump_device_dp_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_simulate_radar(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	u8 agile = 0, segment = 0, radar_type = 0, chirp = 0, fhss = 0;
	struct ath12k *ar = file->private_data;
	char buf[64], *token, *sptr;
	u32 radar_params;
	int offset = 0;
	int ret;
	int len;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len)) {
		return -EFAULT;
	}

	/* For backward compatibility */
	if (len <= 2)
		goto send_cmd;

	buf[len] = '\0';
	sptr = buf;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &segment))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &radar_type))
		return -EINVAL;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtoint(token, 10, &offset))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &agile))
		return -EINVAL;

	if ((segment > 1) || (radar_type > 2) || (agile > 2))
		return -EINVAL;

	/* TODO Add agile chandef validation
	 */
	if (agile)
		return -EINVAL;

send_cmd:
	/* radar_type 1 is for chirp, radar_type 2 is for FHSS */
	if (radar_type == 1)
		chirp = 1;
	if (radar_type == 2)
		fhss = 1;

	radar_params = u32_encode_bits(segment, SEGMENT_ID) |
		       u32_encode_bits(chirp, CHRIP_ID) |
		       u32_encode_bits(offset, OFFSET) |
		       u32_encode_bits(agile, DETECTOR_ID) |
		       u32_encode_bits(fhss, FHSS);

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	ret = ath12k_wmi_simulate_radar(ar, radar_params);
	if (ret)
		goto exit;

	ret = count;
exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_simulate_radar = {
	.write = ath12k_write_simulate_radar,
	.open = simple_open
};

static ssize_t ath12k_write_tpc_stats_type(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u8 type;
	int ret;

	ret = kstrtou8_from_user(user_buf, count, 0, &type);
	if (ret)
		return ret;

	if (type >= WMI_HALPHY_PDEV_TX_STATS_MAX)
		return -EINVAL;

	spin_lock_bh(&ar->data_lock);
	ar->debug.tpc_stats_type = type;
	spin_unlock_bh(&ar->data_lock);

	return count;
}

static int ath12k_debug_tpc_stats_request(struct ath12k *ar)
{
	enum wmi_halphy_ctrl_path_stats_id tpc_stats_sub_id;
	struct ath12k_base *ab = ar->ab;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->debug.tpc_complete);

	spin_lock_bh(&ar->data_lock);
	ar->debug.tpc_request = true;
	tpc_stats_sub_id = ar->debug.tpc_stats_type;
	spin_unlock_bh(&ar->data_lock);

	ret = ath12k_wmi_send_tpc_stats_request(ar, tpc_stats_sub_id);
	if (ret) {
		ath12k_warn(ab, "failed to request pdev tpc stats: %d\n", ret);
		spin_lock_bh(&ar->data_lock);
		ar->debug.tpc_request = false;
		spin_unlock_bh(&ar->data_lock);
		return ret;
	}

	return 0;
}

static int ath12k_get_tpc_ctl_mode_idx(struct wmi_tpc_stats_arg *tpc_stats,
				       enum wmi_tpc_pream_bw pream_bw, int *mode_idx)
{
	u32 chan_freq = le32_to_cpu(tpc_stats->tpc_config.chan_freq);
	u8 band;

	band = ((chan_freq > ATH12K_MIN_6GHZ_FREQ) ? NL80211_BAND_6GHZ :
		((chan_freq > ATH12K_MIN_5GHZ_FREQ) ? NL80211_BAND_5GHZ :
		NL80211_BAND_2GHZ));

	if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
		switch (pream_bw) {
		case WMI_TPC_PREAM_HT20:
		case WMI_TPC_PREAM_VHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT_VHT20_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HE20:
		case WMI_TPC_PREAM_EHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT20_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HT40:
		case WMI_TPC_PREAM_VHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT_VHT40_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_HE40:
		case WMI_TPC_PREAM_EHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT40_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_VHT80:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_VHT80_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT60:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT80_SU_PUNC20;
			break;
		case WMI_TPC_PREAM_HE80:
		case WMI_TPC_PREAM_EHT80:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT80_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_VHT160:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_VHT160_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT120:
		case WMI_TPC_PREAM_EHT140:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT160_SU_PUNC20;
			break;
		case WMI_TPC_PREAM_HE160:
		case WMI_TPC_PREAM_EHT160:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT160_5GHZ_6GHZ;
			break;
		case WMI_TPC_PREAM_EHT200:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC120;
			break;
		case WMI_TPC_PREAM_EHT240:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC80;
			break;
		case WMI_TPC_PREAM_EHT280:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC40;
			break;
		case WMI_TPC_PREAM_EHT320:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HE_EHT320_5GHZ_6GHZ;
			break;
		default:
			/* for 5GHZ and 6GHZ, default case will be for OFDM */
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_LEGACY_5GHZ_6GHZ;
			break;
		}
	} else {
		switch (pream_bw) {
		case WMI_TPC_PREAM_OFDM:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_LEGACY_2GHZ;
			break;
		case WMI_TPC_PREAM_HT20:
		case WMI_TPC_PREAM_VHT20:
		case WMI_TPC_PREAM_HE20:
		case WMI_TPC_PREAM_EHT20:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT20_2GHZ;
			break;
		case WMI_TPC_PREAM_HT40:
		case WMI_TPC_PREAM_VHT40:
		case WMI_TPC_PREAM_HE40:
		case WMI_TPC_PREAM_EHT40:
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_HT40_2GHZ;
			break;
		default:
			/* for 2GHZ, default case will be CCK */
			*mode_idx = ATH12K_TPC_STATS_CTL_MODE_CCK_2GHZ;
			break;
		}
	}

	return 0;
}

static s16 ath12k_tpc_get_rate(struct ath12k *ar,
			       struct wmi_tpc_stats_arg *tpc_stats,
			       u32 rate_idx, u32 num_chains, u32 rate_code,
			       enum wmi_tpc_pream_bw pream_bw,
			       enum wmi_halphy_ctrl_path_stats_id type,
			       u32 eht_rate_idx)
{
	u32 tot_nss, tot_modes, txbf_on_off, index_offset1, index_offset2, index_offset3;
	u8 chain_idx, stm_idx, num_streams;
	bool is_mu, txbf_enabled = 0;
	s8 rates_ctl_min, tpc_ctl;
	s16 rates, tpc, reg_pwr;
	u16 rate1, rate2;
	int mode, ret;

	num_streams = 1 + ATH12K_HW_NSS(rate_code);
	chain_idx = num_chains - 1;
	stm_idx = num_streams - 1;
	mode = -1;

	ret = ath12k_get_tpc_ctl_mode_idx(tpc_stats, pream_bw, &mode);
	if (ret) {
		ath12k_warn(ar->ab, "Invalid mode index received\n");
		tpc = TPC_INVAL;
		goto out;
	}

	if (num_chains < num_streams) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (le32_to_cpu(tpc_stats->tpc_config.num_tx_chain) <= 1) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (type == WMI_HALPHY_PDEV_TX_SUTXBF_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS)
		txbf_enabled = 1;

	if (type == WMI_HALPHY_PDEV_TX_MU_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS) {
		is_mu = true;
	} else {
		is_mu = false;
	}

	/* Below is the min calculation of ctl array, rates array and
	 * regulator power table. tpc is minimum of all 3
	 */
	if (pream_bw >= WMI_TPC_PREAM_EHT20 && pream_bw <= WMI_TPC_PREAM_EHT320) {
		rate2 = tpc_stats->rates_array2.rate_array[eht_rate_idx];
		if (is_mu)
			rates = u32_get_bits(rate2, ATH12K_TPC_RATE_ARRAY_MU);
		else
			rates = u32_get_bits(rate2, ATH12K_TPC_RATE_ARRAY_SU);
	} else {
		rate1 = tpc_stats->rates_array1.rate_array[rate_idx];
		if (is_mu)
			rates = u32_get_bits(rate1, ATH12K_TPC_RATE_ARRAY_MU);
		else
			rates = u32_get_bits(rate1, ATH12K_TPC_RATE_ARRAY_SU);
	}

	if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
		tot_nss = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d1);
		tot_modes = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d2);
		txbf_on_off = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.d3);
		index_offset1 = txbf_on_off * tot_modes * tot_nss;
		index_offset2 = tot_modes * tot_nss;
		index_offset3 = tot_nss;

		tpc_ctl = *(tpc_stats->ctl_array.ctl_pwr_table +
			    chain_idx * index_offset1 + txbf_enabled * index_offset2
			    + mode * index_offset3 + stm_idx);
	} else {
		tpc_ctl = TPC_MAX;
		ath12k_warn(ar->ab,
			    "ctl array for tpc stats not received from fw\n");
	}

	rates_ctl_min = min_t(s16, rates, tpc_ctl);

	reg_pwr = tpc_stats->max_reg_allowed_power.reg_pwr_array[chain_idx];

	if (reg_pwr < 0)
		reg_pwr = TPC_INVAL;

	tpc = min_t(s16, rates_ctl_min, reg_pwr);

	/* MODULATION_LIMIT is the maximum power limit,tpc should not exceed
	 * modulation limit even if min tpc of all three array is greater
	 * modulation limit
	 */
	tpc = min_t(s16, tpc, MODULATION_LIMIT);

out:
	return tpc;
}

static u16 ath12k_get_ratecode(u16 pream_idx, u16 nss, u16 mcs_rate)
{
	u16 mode_type = ~0;

	/* Below assignments are just for printing purpose only */
	switch (pream_idx) {
	case WMI_TPC_PREAM_CCK:
		mode_type = WMI_RATE_PREAMBLE_CCK;
		break;
	case WMI_TPC_PREAM_OFDM:
		mode_type = WMI_RATE_PREAMBLE_OFDM;
		break;
	case WMI_TPC_PREAM_HT20:
	case WMI_TPC_PREAM_HT40:
		mode_type = WMI_RATE_PREAMBLE_HT;
		break;
	case WMI_TPC_PREAM_VHT20:
	case WMI_TPC_PREAM_VHT40:
	case WMI_TPC_PREAM_VHT80:
	case WMI_TPC_PREAM_VHT160:
		mode_type = WMI_RATE_PREAMBLE_VHT;
		break;
	case WMI_TPC_PREAM_HE20:
	case WMI_TPC_PREAM_HE40:
	case WMI_TPC_PREAM_HE80:
	case WMI_TPC_PREAM_HE160:
		mode_type = WMI_RATE_PREAMBLE_HE;
		break;
	case WMI_TPC_PREAM_EHT20:
	case WMI_TPC_PREAM_EHT40:
	case WMI_TPC_PREAM_EHT60:
	case WMI_TPC_PREAM_EHT80:
	case WMI_TPC_PREAM_EHT120:
	case WMI_TPC_PREAM_EHT140:
	case WMI_TPC_PREAM_EHT160:
	case WMI_TPC_PREAM_EHT200:
	case WMI_TPC_PREAM_EHT240:
	case WMI_TPC_PREAM_EHT280:
	case WMI_TPC_PREAM_EHT320:
		mode_type = WMI_RATE_PREAMBLE_EHT;
		if (mcs_rate == 0 || mcs_rate == 1)
			mcs_rate += 14;
		else
			mcs_rate -= 2;
		break;
	default:
		return mode_type;
	}
	return ((mode_type << 8) | ((nss & 0x7) << 5) | (mcs_rate & 0x1F));
}

static bool ath12k_he_supports_extra_mcs(struct ath12k *ar, int freq)
{
	struct ath12k_pdev_cap *cap = &ar->pdev->cap;
	struct ath12k_band_cap *cap_band;
	bool extra_mcs_supported;

	if (freq <= ATH12K_2GHZ_MAX_FREQUENCY)
		cap_band = &cap->band[NL80211_BAND_2GHZ];
	else if (freq <= ATH12K_5GHZ_MAX_FREQUENCY)
		cap_band = &cap->band[NL80211_BAND_5GHZ];
	else
		cap_band = &cap->band[NL80211_BAND_6GHZ];

	extra_mcs_supported = u32_get_bits(cap_band->he_cap_info[1],
					   HE_EXTRA_MCS_SUPPORT);
	return extra_mcs_supported;
}

static int ath12k_tpc_fill_pream(struct ath12k *ar, char *buf, int buf_len, int len,
				 enum wmi_tpc_pream_bw pream_bw, u32 max_rix,
				 int max_nss, int max_rates, int pream_type,
				 enum wmi_halphy_ctrl_path_stats_id tpc_type,
				 int rate_idx, int eht_rate_idx)
{
	struct wmi_tpc_stats_arg *tpc_stats = ar->debug.tpc_stats;
	int nss, rates, chains;
	u8 active_tx_chains;
	u16 rate_code;
	s16 tpc;

	static const char *const pream_str[] = {
		[WMI_TPC_PREAM_CCK]     = "CCK",
		[WMI_TPC_PREAM_OFDM]    = "OFDM",
		[WMI_TPC_PREAM_HT20]    = "HT20",
		[WMI_TPC_PREAM_HT40]    = "HT40",
		[WMI_TPC_PREAM_VHT20]   = "VHT20",
		[WMI_TPC_PREAM_VHT40]   = "VHT40",
		[WMI_TPC_PREAM_VHT80]   = "VHT80",
		[WMI_TPC_PREAM_VHT160]  = "VHT160",
		[WMI_TPC_PREAM_HE20]    = "HE20",
		[WMI_TPC_PREAM_HE40]    = "HE40",
		[WMI_TPC_PREAM_HE80]    = "HE80",
		[WMI_TPC_PREAM_HE160]   = "HE160",
		[WMI_TPC_PREAM_EHT20]   = "EHT20",
		[WMI_TPC_PREAM_EHT40]   = "EHT40",
		[WMI_TPC_PREAM_EHT60]   = "EHT60",
		[WMI_TPC_PREAM_EHT80]   = "EHT80",
		[WMI_TPC_PREAM_EHT120]   = "EHT120",
		[WMI_TPC_PREAM_EHT140]   = "EHT140",
		[WMI_TPC_PREAM_EHT160]   = "EHT160",
		[WMI_TPC_PREAM_EHT200]   = "EHT200",
		[WMI_TPC_PREAM_EHT240]   = "EHT240",
		[WMI_TPC_PREAM_EHT280]   = "EHT280",
		[WMI_TPC_PREAM_EHT320]   = "EHT320"};

	active_tx_chains = ar->num_tx_chains;

	for (nss = 0; nss < max_nss; nss++) {
		for (rates = 0; rates < max_rates; rates++, rate_idx++, max_rix++) {
			/* FW send extra MCS(10&11) for VHT and HE rates,
			 *  this is not used. Hence skipping it here
			 */
			if (pream_type == WMI_RATE_PREAMBLE_VHT &&
			    rates > ATH12K_VHT_MCS_MAX)
				continue;

			if (pream_type == WMI_RATE_PREAMBLE_HE &&
			    rates > ATH12K_HE_MCS_MAX)
				continue;

			if (pream_type == WMI_RATE_PREAMBLE_EHT &&
			    rates > ATH12K_EHT_MCS_MAX)
				continue;

			rate_code = ath12k_get_ratecode(pream_bw, nss, rates);
			len += scnprintf(buf + len, buf_len - len,
					 "%d\t %s\t 0x%03x\t", max_rix,
					 pream_str[pream_bw], rate_code);

			for (chains = 0; chains < active_tx_chains; chains++) {
				if (nss > chains) {
					len += scnprintf(buf + len,
							 buf_len - len,
							 "\t%s", "NA");
				} else {
					tpc = ath12k_tpc_get_rate(ar, tpc_stats,
								  rate_idx, chains + 1,
								  rate_code, pream_bw,
								  tpc_type,
								  eht_rate_idx);

					if (tpc == TPC_INVAL) {
						len += scnprintf(buf + len,
								 buf_len - len, "\tNA");
					} else {
						len += scnprintf(buf + len,
								 buf_len - len, "\t%d",
								 tpc);
					}
				}
			}
			len += scnprintf(buf + len, buf_len - len, "\n");

			if (pream_type == WMI_RATE_PREAMBLE_EHT)
				/*For fetching the next eht rates pwr from rates array2*/
				++eht_rate_idx;
		}
	}

	return len;
}

static int ath12k_tpc_stats_print(struct ath12k *ar,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  char *buf, size_t len,
				  enum wmi_halphy_ctrl_path_stats_id type)
{
	u32 eht_idx = 0, pream_idx = 0, rate_pream_idx = 0, total_rates = 0, max_rix = 0;
	u32 chan_freq, num_tx_chain, caps, i, j = 1;
	size_t buf_len = ATH12K_TPC_STATS_BUF_SIZE;
	u8 nss, active_tx_chains;
	bool he_ext_mcs;
	static const char *const type_str[WMI_HALPHY_PDEV_TX_STATS_MAX] = {
		[WMI_HALPHY_PDEV_TX_SU_STATS]		= "SU",
		[WMI_HALPHY_PDEV_TX_SUTXBF_STATS]	= "SU WITH TXBF",
		[WMI_HALPHY_PDEV_TX_MU_STATS]		= "MU",
		[WMI_HALPHY_PDEV_TX_MUTXBF_STATS]	= "MU WITH TXBF"};

	u8 max_rates[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = ATH12K_CCK_RATES,
		[WMI_TPC_PREAM_OFDM]    = ATH12K_OFDM_RATES,
		[WMI_TPC_PREAM_HT20]    = ATH12K_HT_RATES,
		[WMI_TPC_PREAM_HT40]    = ATH12K_HT_RATES,
		[WMI_TPC_PREAM_VHT20]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT40]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT80]   = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_VHT160]  = ATH12K_VHT_RATES,
		[WMI_TPC_PREAM_HE20]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE40]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE80]    = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_HE160]   = ATH12K_HE_RATES,
		[WMI_TPC_PREAM_EHT20]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT40]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT60]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT80]   = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT120]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT140]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT160]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT200]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT240]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT280]  = ATH12K_EHT_RATES,
		[WMI_TPC_PREAM_EHT320]  = ATH12K_EHT_RATES};
	static const u8 max_nss[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = ATH12K_NSS_1,
		[WMI_TPC_PREAM_OFDM]    = ATH12K_NSS_1,
		[WMI_TPC_PREAM_HT20]    = ATH12K_NSS_4,
		[WMI_TPC_PREAM_HT40]    = ATH12K_NSS_4,
		[WMI_TPC_PREAM_VHT20]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT40]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT80]   = ATH12K_NSS_8,
		[WMI_TPC_PREAM_VHT160]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_HE20]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE40]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE80]    = ATH12K_NSS_8,
		[WMI_TPC_PREAM_HE160]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT20]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT40]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT60]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT80]   = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT120]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT140]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT160]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT200]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT240]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT280]  = ATH12K_NSS_4,
		[WMI_TPC_PREAM_EHT320]  = ATH12K_NSS_4};

	u16 rate_idx[WMI_TPC_PREAM_MAX] = {}, eht_rate_idx[WMI_TPC_PREAM_MAX] = {};
	static const u8 pream_type[WMI_TPC_PREAM_MAX] = {
		[WMI_TPC_PREAM_CCK]     = WMI_RATE_PREAMBLE_CCK,
		[WMI_TPC_PREAM_OFDM]    = WMI_RATE_PREAMBLE_OFDM,
		[WMI_TPC_PREAM_HT20]    = WMI_RATE_PREAMBLE_HT,
		[WMI_TPC_PREAM_HT40]    = WMI_RATE_PREAMBLE_HT,
		[WMI_TPC_PREAM_VHT20]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT40]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT80]   = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_VHT160]  = WMI_RATE_PREAMBLE_VHT,
		[WMI_TPC_PREAM_HE20]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE40]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE80]    = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_HE160]   = WMI_RATE_PREAMBLE_HE,
		[WMI_TPC_PREAM_EHT20]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT40]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT60]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT80]   = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT120]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT140]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT160]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT200]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT240]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT280]  = WMI_RATE_PREAMBLE_EHT,
		[WMI_TPC_PREAM_EHT320]  = WMI_RATE_PREAMBLE_EHT};

	chan_freq = le32_to_cpu(tpc_stats->tpc_config.chan_freq);
	num_tx_chain = le32_to_cpu(tpc_stats->tpc_config.num_tx_chain);
	caps = le32_to_cpu(tpc_stats->tpc_config.caps);

	active_tx_chains = ar->num_tx_chains;
	he_ext_mcs = ath12k_he_supports_extra_mcs(ar, chan_freq);

	/* mcs 12&13 is sent by FW for certain HWs in rate array, skipping it as
	 * it is not supported
	 */
	if (he_ext_mcs) {
		for (i = WMI_TPC_PREAM_HE20; i <= WMI_TPC_PREAM_HE160; ++i)
			max_rates[i] = ATH12K_HE_RATES;
	}

	if (type == WMI_HALPHY_PDEV_TX_MU_STATS ||
	    type == WMI_HALPHY_PDEV_TX_MUTXBF_STATS) {
		pream_idx = WMI_TPC_PREAM_VHT20;

		for (i = WMI_TPC_PREAM_CCK; i <= WMI_TPC_PREAM_HT40; ++i)
			max_rix += max_nss[i] * max_rates[i];
	}
	/* Enumerate all the rate indices */
	for (i = rate_pream_idx + 1; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i - 1] < num_tx_chain ?
		       max_nss[i - 1] : num_tx_chain);

		rate_idx[i] = rate_idx[i - 1] + max_rates[i - 1] * nss;

		if (pream_type[i] == WMI_RATE_PREAMBLE_EHT) {
			eht_rate_idx[j] = eht_rate_idx[j - 1] + max_rates[i] * nss;
			++j;
		}
	}

	for (i = 0; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i] < num_tx_chain ?
		       max_nss[i] : num_tx_chain);
		total_rates += max_rates[i] * nss;
	}

	len += scnprintf(buf + len, buf_len - len,
			 "No.of rates-%d\n", total_rates);

	len += scnprintf(buf + len, buf_len - len,
			 "**************** %s ****************\n",
			 type_str[type]);
	len += scnprintf(buf + len, buf_len - len,
			 "\t\t\t\tTPC values for Active chains\n");
	len += scnprintf(buf + len, buf_len - len,
			 "Rate idx Preamble Rate code");

	for (i = 1; i <= active_tx_chains; ++i) {
		len += scnprintf(buf + len, buf_len - len,
				 "\t%d-Chain", i);
	}

	len += scnprintf(buf + len, buf_len - len, "\n");
	for (i = pream_idx; i < WMI_TPC_PREAM_MAX; i++) {
		if (chan_freq <= 2483) {
			if (i == WMI_TPC_PREAM_VHT80 ||
			    i == WMI_TPC_PREAM_VHT160 ||
			    i == WMI_TPC_PREAM_HE80 ||
			    i == WMI_TPC_PREAM_HE160 ||
			    (i >= WMI_TPC_PREAM_EHT60 &&
			     i <= WMI_TPC_PREAM_EHT320)) {
				max_rix += max_nss[i] * max_rates[i];
				continue;
			}
		} else {
			if (i == WMI_TPC_PREAM_CCK) {
				max_rix += max_rates[i];
				continue;
			}
		}

		nss = (max_nss[i] < ar->num_tx_chains ? max_nss[i] : ar->num_tx_chains);

		if (!(caps &
		    (1 << ATH12K_TPC_STATS_SUPPORT_BE_PUNC))) {
			if (i == WMI_TPC_PREAM_EHT60 || i == WMI_TPC_PREAM_EHT120 ||
			    i == WMI_TPC_PREAM_EHT140 || i == WMI_TPC_PREAM_EHT200 ||
			    i == WMI_TPC_PREAM_EHT240 || i == WMI_TPC_PREAM_EHT280) {
				max_rix += max_nss[i] * max_rates[i];
				continue;
			}
		}

		len = ath12k_tpc_fill_pream(ar, buf, buf_len, len, i, max_rix, nss,
					    max_rates[i], pream_type[i],
					    type, rate_idx[i], eht_rate_idx[eht_idx]);

		if (pream_type[i] == WMI_RATE_PREAMBLE_EHT)
			/*For fetch the next index eht rates from rates array2*/
			++eht_idx;

		max_rix += max_nss[i] * max_rates[i];
	}
	return len;
}

static void ath12k_tpc_stats_fill(struct ath12k *ar,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  char *buf)
{
	size_t buf_len = ATH12K_TPC_STATS_BUF_SIZE;
	struct wmi_tpc_config_params *tpc;
	size_t len = 0;

	if (!tpc_stats) {
		ath12k_warn(ar->ab, "failed to find tpc stats\n");
		return;
	}

	spin_lock_bh(&ar->data_lock);

	tpc = &tpc_stats->tpc_config;
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "*************** TPC config **************\n");
	len += scnprintf(buf + len, buf_len - len,
			 "* powers are in 0.25 dBm steps\n");
	len += scnprintf(buf + len, buf_len - len,
			 "reg domain-%d\t\tchan freq-%d\n",
			 tpc->reg_domain, tpc->chan_freq);
	len += scnprintf(buf + len, buf_len - len,
			 "power limit-%d\t\tmax reg-domain Power-%d\n",
			 le32_to_cpu(tpc->twice_max_reg_power) / 2, tpc->power_limit);
	len += scnprintf(buf + len, buf_len - len,
			 "No.of tx chain-%d\t",
			 ar->num_tx_chains);

	ath12k_tpc_stats_print(ar, tpc_stats, buf, len,
			       ar->debug.tpc_stats_type);

	spin_unlock_bh(&ar->data_lock);
}

static int ath12k_open_tpc_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah->state != ATH12K_HW_STATE_ON) {
		ath12k_warn(ar->ab, "Interface not up\n");
		return -ENETDOWN;
	}

	void *buf __free(kfree) = kzalloc(ATH12K_TPC_STATS_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = ath12k_debug_tpc_stats_request(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request tpc stats: %d\n",
			    ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->debug.tpc_complete, TPC_STATS_WAIT_TIME)) {
		spin_lock_bh(&ar->data_lock);
		ath12k_wmi_free_tpc_stats_mem(ar);
		ar->debug.tpc_request = false;
		spin_unlock_bh(&ar->data_lock);
		return -ETIMEDOUT;
	}

	ath12k_tpc_stats_fill(ar, ar->debug.tpc_stats, buf);
	file->private_data = no_free_ptr(buf);

	spin_lock_bh(&ar->data_lock);
	ath12k_wmi_free_tpc_stats_mem(ar);
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

static ssize_t ath12k_read_tpc_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static int ath12k_release_tpc_stats(struct inode *inode,
				    struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations fops_tpc_stats = {
	.open = ath12k_open_tpc_stats,
	.release = ath12k_release_tpc_stats,
	.read = ath12k_read_tpc_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_tpc_stats_type = {
	.write = ath12k_write_tpc_stats_type,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_extd_rx_stats(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id, rx_filter = 0;
	bool enable;
	int ret, i;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (!ar->ab->hw_params->rxdma1_enable) {
		ret = count;
		goto exit;
	}

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (enable == ar->debug.extd_rx_stats) {
		ret = count;
		goto exit;
	}

	if (enable) {
		rx_filter =  HTT_RX_FILTER_TLV_FLAGS_MPDU_START;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO;

		tlv_filter.rx_filter = rx_filter;
		tlv_filter.pkt_filter_flags0 = HTT_RX_FP_MGMT_FILTER_FLAGS0;
		tlv_filter.pkt_filter_flags1 = HTT_RX_FP_MGMT_FILTER_FLAGS1;
		tlv_filter.pkt_filter_flags2 = HTT_RX_FP_CTRL_FILTER_FLASG2;
		tlv_filter.pkt_filter_flags3 = HTT_RX_FP_CTRL_FILTER_FLASG3 |
			HTT_RX_FP_DATA_FILTER_FLASG3;
	} else {
		tlv_filter = ath12k_mac_mon_status_filter_default;
	}

	ar->debug.rx_filter = tlv_filter.rx_filter;

	for (i = 0; i < ar->ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rxdma_mon_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id, ar->dp.mac_id + i,
						       HAL_RXDMA_MONITOR_DST,
						       DP_RX_MON_BUFFER_SIZE,
						       &tlv_filter);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set rx filter for monitor status ring\n");
			goto exit;
		}
	}

	ar->debug.extd_rx_stats = !!enable;
	ret = count;
exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_read_extd_rx_stats(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32];
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.extd_rx_stats);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_extd_rx_stats = {
	.read = ath12k_read_extd_rx_stats,
	.write = ath12k_write_extd_rx_stats,
	.open = simple_open,
};

static int ath12k_open_link_stats(struct inode *inode, struct file *file)
{
	struct ath12k_vif *ahvif = inode->i_private;
	size_t len = 0, buf_len = (PAGE_SIZE * 2);
	struct ath12k_link_stats linkstat;
	struct ath12k_link_vif *arvif;
	unsigned long links_map;
	struct wiphy *wiphy;
	int link_id, i;
	char *buf;

	if (!ahvif)
		return -EINVAL;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy = ahvif->ah->hw->wiphy;
	wiphy_lock(wiphy);

	links_map = ahvif->links_map;
	for_each_set_bit(link_id, &links_map,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = rcu_dereference_protected(ahvif->link[link_id],
						  lockdep_is_held(&wiphy->mtx));

		spin_lock_bh(&arvif->link_stats_lock);
		linkstat = arvif->link_stats;
		spin_unlock_bh(&arvif->link_stats_lock);

		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Tx Unicast Frames Enqueued  = %d\n",
				 link_id, linkstat.tx_enqueued);
		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Tx Broadcast Frames Enqueued = %d\n",
				 link_id, linkstat.tx_bcast_mcast);
		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Tx Frames Completed = %d\n",
				 link_id, linkstat.tx_completed);
		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Tx Frames Dropped = %d\n",
				 link_id, linkstat.tx_dropped);

		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Tx Frame descriptor Encap Type = ",
				 link_id);

		len += scnprintf(buf + len, buf_len - len,
					 " raw:%d",
					 linkstat.tx_encap_type[0]);

		len += scnprintf(buf + len, buf_len - len,
					 " native_wifi:%d",
					 linkstat.tx_encap_type[1]);

		len += scnprintf(buf + len, buf_len - len,
					 " ethernet:%d",
					 linkstat.tx_encap_type[2]);

		len += scnprintf(buf + len, buf_len - len,
				 "\nlink[%d] Tx Frame descriptor Encrypt Type = ",
				 link_id);

		for (i = 0; i < HAL_ENCRYPT_TYPE_MAX; i++) {
			len += scnprintf(buf + len, buf_len - len,
					 " %d:%d", i,
					 linkstat.tx_encrypt_type[i]);
		}
		len += scnprintf(buf + len, buf_len - len,
				 "\nlink[%d] Tx Frame descriptor Type = buffer:%d extension:%d\n",
				 link_id, linkstat.tx_desc_type[0],
				 linkstat.tx_desc_type[1]);

		len += scnprintf(buf + len, buf_len - len,
				 "link[%d] Rx Frames Dropped = %d\n",
				 link_id, linkstat.rx_dropped);

		len += scnprintf(buf + len, buf_len - len,
				"------------------------------------------------------\n");
	}

	wiphy_unlock(wiphy);

	file->private_data = buf;

	return 0;
}

static int ath12k_release_link_stats(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static ssize_t ath12k_read_link_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ath12k_fops_link_stats = {
	.open = ath12k_open_link_stats,
	.release = ath12k_release_link_stats,
	.read = ath12k_read_link_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);

	debugfs_create_file("link_stats", 0400, vif->debugfs_dir, ahvif,
			    &ath12k_fops_link_stats);
}
EXPORT_SYMBOL(ath12k_debugfs_op_vif_add);

static ssize_t ath12k_fse_ops_write(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_pdev *pdev;
	struct rx_flow_info flow_info = {0};
	u8 buf[160] = {0};
	u32 sip_addr[4] = {0};
	u32 dip_addr[4] = {0};

	char ops[8], ipver[8], srcip[48], destip[48];
	u32 srcport, destport, protocol;

	int ret, i;
	bool radioup = false;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = true;
			break;
		}
	}

	if (!radioup) {
		ath12k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}
	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		goto exit;

	buf[ret] = '\0';

	ret = sscanf(buf, "%7s %7s %47s %47s %d %d %d", ops, ipver,
		     srcip, destip, &srcport, &destport, &protocol);

	if (!(ret == 7 || ret == 1)) {
		ret = -EINVAL;
		goto exit;
	}
	if (ret == 1) {
		if (!strcmp(ops, "RST")) {
			ath12k_dp_rx_flow_delete_all_entries(ab);
			ret = count;
		} else {
			ret = -EINVAL;
		}
		goto exit;
	}

	if (!(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP)) {
		ret = -EINVAL;
		goto exit;
	}

	if (!(srcport >= ATH12K_UDP_TCP_START_PORT &&
	      srcport <= ATH12K_UDP_TCP_END_PORT) ||
	    !(destport >= ATH12K_UDP_TCP_START_PORT &&
	      destport <= ATH12K_UDP_TCP_END_PORT)) {
		ret = -EINVAL;
		goto exit;
	}

	if (!strcmp(ipver, "IPV4")) {
		if (!in4_pton(srcip, -1, (u8 *)&sip_addr[0], -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
		if (!in4_pton(destip, -1, (u8 *)&dip_addr[0], -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
	} else if (!strcmp(ipver, "IPV6")) {
		if (!in6_pton(srcip, -1, (u8 *)sip_addr, -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
		if (!in6_pton(destip, -1, (u8 *)dip_addr, -1, NULL)) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret = -EINVAL;
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "S_IP:%x:%x:%x:%x D_IP:%x:%x:%x:%x\n",
		   sip_addr[0], sip_addr[1], sip_addr[2], sip_addr[3],
		   dip_addr[0], dip_addr[1], dip_addr[2], dip_addr[3]);

	if (!strcmp(ops, "ADD") || !strcmp(ops, "DEL")) {
		flow_info.flow_tuple_info.src_port = srcport;
		flow_info.flow_tuple_info.dest_port = destport;
		flow_info.flow_tuple_info.l4_protocol = protocol;

		if (!strcmp(ipver, "IPV4")) {
			flow_info.is_addr_ipv4 = 1;
			flow_info.flow_tuple_info.src_ip_31_0 = sip_addr[0];
			flow_info.flow_tuple_info.dest_ip_31_0 = dip_addr[0];
		} else {
			flow_info.flow_tuple_info.src_ip_31_0 = sip_addr[3];
			flow_info.flow_tuple_info.src_ip_63_32 = sip_addr[2];
			flow_info.flow_tuple_info.src_ip_95_64 = sip_addr[1];
			flow_info.flow_tuple_info.src_ip_127_96 = sip_addr[0];

			flow_info.flow_tuple_info.dest_ip_31_0 = dip_addr[3];
			flow_info.flow_tuple_info.dest_ip_63_32 = dip_addr[2];
			flow_info.flow_tuple_info.dest_ip_95_64 = dip_addr[1];
			flow_info.flow_tuple_info.dest_ip_127_96 = dip_addr[0];
		}
		if (!strcmp(ops, "ADD")) {
			flow_info.fse_metadata = ATH12K_RX_FSE_FLOW_MATCH_DEBUGFS;
			ath12k_dp_rx_flow_add_entry(ab, &flow_info);
		} else {
			ath12k_dp_rx_flow_delete_entry(ab, &flow_info);
		}
		ret = count;
	}

exit:
	if (ret == -EINVAL) {
		ath12k_warn(ab, "Invalid input\nUsage:\n");
		ath12k_warn(ab, "<Case1> Cmd(RST)\n");
		ath12k_warn(ab, "<Example> RST\n");
		ath12k_warn(ab, "<Case2> Cmd(ADD/DEL) ipver(IPV4/IPV6) srcip destip srcport(0-65535) destport(0-65535) protocol(6-tcp 17-udp)\n");
		ath12k_warn(ab, "<Example1> ADD IPV4 192.168.1.1 192.168.1.2 15000 15500 17\n");
		ath12k_warn(ab, "<Example2> DEL IPV4 192.168.1.1 192.168.1.2 15000 15500 17\n");
		ath12k_warn(ab, "<Example3> ADD IPV6 ffaa:bbcc:1122:5566:ccbb:aadd:1122:5555 fefe:b1c1:1122:5566:ccbb:aadd:1122:5555 15000 15500 17\n");
		ath12k_warn(ab, "<Example4> DEL IPV6 ffaa:bbcc:1122:5566:ccbb:aadd:1122:5555 fefe:b1c1:1122:5566:ccbb:aadd:1122:5555 15000 15500 17\n");
	}
	return ret;
}

static ssize_t ath12k_read_fst_core_mask(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	char *buf;
	const int size = 256;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf + len, size - len,
			"\nFST core mask: %u\n",
			dp->fst_config.fst_core_mask);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t ath12k_write_fst_core_mask(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev *pdev;
	u32 fst_core_mask;
	int ret, i;
	bool radioup = false;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = true;
			break;
		}
	}

	if (!radioup) {
		ath12k_err(ab, "radio is not up\n");
		return -ENETDOWN;
	}

	ret = kstrtou32_from_user(ubuf, count, 0, &fst_core_mask);
	if (ret)
		return -EINVAL;

	if (!dp->dp_hw_grp->fst) {
		ath12k_err(ab, "FST table is NULL\n");
		return -EINVAL;
	}

	if (fst_core_mask < ATH12K_DP_MIN_FST_CORE_MASK ||
	    fst_core_mask > ATH12K_DP_MAX_FST_CORE_MASK) {
		ath12k_err(ab, "Invalid FST core mask:0x%x\n",
			   fst_core_mask);
		return -EINVAL;
	}

	dp->fst_config.fst_core_mask = fst_core_mask;
	if (fst_core_mask)
		ath12k_dp_fst_core_map_init(ab);

	ret = count;
	return ret;
}

static ssize_t ath12k_dump_fst_flow_stats(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	char *buf;
	const int size = 1024;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			"\nNo of IPv4 Flow entries inserted: %u\n",
			fst->ipv4_fse_rule_cnt);

	len += scnprintf(buf + len, size - len,
			"\nNo of IPv6 Flow entries inserted: %u\n",
			fst->ipv6_fse_rule_cnt);

	len += scnprintf(buf + len, size - len,
			"\nFlow addition failure: %u\n",
			fst->flow_add_fail);

	len += scnprintf(buf + len, size - len,
			"\nFlow deletion failure: %u\n",
			fst->flow_del_fail);

	len += scnprintf(buf + len, size - len,
			"\nNo of Flows per reo:\n0:%u\t1:%u\t2:%u\t3:%u\n",
			fst->flows_per_reo[0],
			fst->flows_per_reo[1],
			fst->flows_per_reo[2],
			fst->flows_per_reo[3]);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static ssize_t ath12k_dump_fst_dump_table(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char *buf;
	const int size = 256 * 2048;
	int len = 0, retval;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = ath12k_dp_dump_fst_table(ab, buf + len, size - len);

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_fse = {
	.open = simple_open,
	.write = ath12k_fse_ops_write,
};

static const struct file_operations fops_fst_core_mask = {
	.open = simple_open,
	.read = ath12k_read_fst_core_mask,
	.write = ath12k_write_fst_core_mask,
};

static const struct file_operations fops_fst_dp_stats = {
	.open = simple_open,
	.read = ath12k_dump_fst_flow_stats,
};

static const struct file_operations fops_fst_dump_table = {
	.open = simple_open,
	.read = ath12k_dump_fst_dump_table,
};

void ath12k_fst_debugfs_init(struct ath12k_base *ab)
{
	struct dentry *fsestats_dir = debugfs_create_dir("fst_config", ab->debugfs_soc);

	debugfs_create_file("fst_core_mask", 0600, fsestats_dir, ab,
			    &fops_fst_core_mask);

	debugfs_create_file("fst_dp_stats", 0400, fsestats_dir, ab,
			    &fops_fst_dp_stats);

	debugfs_create_file("fst_dump_table", 0400, fsestats_dir, ab,
			    &fops_fst_dump_table);

	debugfs_create_file("fse", 0200, fsestats_dir, ab,
			    &fops_fse);
}

void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab)
{
}

void ath12k_debugfs_soc_create(struct ath12k_base *ab)
{
	bool dput_needed;
	char soc_name[64] = { 0 };
	struct dentry *debugfs_ath12k;

	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (debugfs_ath12k) {
		/* a dentry from lookup() needs dput() after we don't use it */
		dput_needed = true;
	} else {
		debugfs_ath12k = debugfs_create_dir("ath12k", NULL);
		if (IS_ERR_OR_NULL(debugfs_ath12k))
			return;
		dput_needed = false;
	}

	scnprintf(soc_name, sizeof(soc_name), "%s-%s", ath12k_bus_str(ab->hif.bus),
		  dev_name(ab->dev));

	ab->debugfs_soc = debugfs_create_dir(soc_name, debugfs_ath12k);

	if (dput_needed)
		dput(debugfs_ath12k);

	ath12k_fst_debugfs_init(ab);
}

static ssize_t ath12k_write_wmi_ctrl_path_stats(struct file *file,
						const char __user *ubuf,
						size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wmi_ctrl_path_stats_arg arg = {};
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	u8 buf[128] = {0};
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%u %u", &arg.stats_id, &arg.action);
	if (ret != 2)
		return -EINVAL;

	if (!arg.action || arg.action > WMI_REQUEST_CTRL_PATH_STAT_RESET)
		return -EINVAL;

	guard(mutex)(&ah->hw_mutex);
	ret = ath12k_wmi_send_wmi_ctrl_stats_cmd(ar, &arg);
	return ret ? ret : count;
}

static int wmi_ctrl_path_pdev_stat(struct ath12k *ar, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char fw_tx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	char fw_rx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	struct wmi_ctrl_path_pdev_stats *stats, *tmp;
	u16 index_tx, index_rx;
	const int size = 2048;
	u8 i;
	int len = 0;

	char *buf __free(kfree) = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	LIST_HEAD(wmi_stats_list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	list_splice_tail_init(&ar->debug.wmi_ctrl_path_stats.pdev_stats, &wmi_stats_list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);

	list_for_each_entry_safe(stats, tmp, &wmi_stats_list, list) {
		if (!stats)
			break;

		index_tx = 0;
		index_rx = 0;

		for (i = 0; i < IEEE80211_MGMT_FRAME_SUBTYPE_MAX; i++) {
			index_tx += scnprintf(&fw_tx_mgmt_subtype[index_tx],
					      WMI_MAX_STRING_LEN - index_tx,
					      " %u:%u,", i,
					      stats->tx_mgmt_subtype[i]);
			index_rx += scnprintf(&fw_rx_mgmt_subtype[index_rx],
					      WMI_MAX_STRING_LEN - index_rx,
					      " %u:%u,", i,
					      stats->rx_mgmt_subtype[i]);
		}

		len += scnprintf(buf + len, size - len,
				 "WMI_CTRL_PATH_PDEV_TX_STATS:\n");
		len += scnprintf(buf + len, size - len,
				 "fw_tx_mgmt_subtype = %s\n",
				 fw_tx_mgmt_subtype);
		len += scnprintf(buf + len, size - len,
				 "fw_rx_mgmt_subtype = %s\n",
				 fw_rx_mgmt_subtype);
		len += scnprintf(buf + len, size - len,
				 "scan_fail_dfs_violation_time_ms = %u\n",
				 stats->scan_fail_dfs_viol_time_ms);
		len += scnprintf(buf + len, size - len,
				 "nol_chk_fail_last_chan_freq = %u\n",
				 stats->nol_chk_fail_last_chan_freq);
		len += scnprintf(buf + len, size - len,
				 "nol_chk_fail_time_stamp_ms = %u\n",
				 stats->nol_chk_fail_time_stamp_ms);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_create_cnt = %u\n",
				 stats->tot_peer_create_cnt);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_del_cnt = %u\n",
				 stats->tot_peer_del_cnt);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_del_resp_cnt = %u\n",
				 stats->tot_peer_del_resp_cnt);
		len += scnprintf(buf + len, size - len,
				 "vdev_pause_fail_rt_to_sched_algo_fifo_full_cnt = %u\n",
				 stats->sched_algo_fifo_full_cnt);
		list_del(&stats->list);
		kfree(stats);
	}

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

int wmi_ctrl_path_mem_stat(struct ath12k *ar, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;
	struct wmi_ctrl_path_mem_stats_params *mem_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;
	LIST_HEAD(wmi_stats_list);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			"WMI_CTRL_PATH_MEM_STATS:\n");

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	list_splice_tail_init(&ar->debug.wmi_ctrl_path_stats.pdev_stats, &wmi_stats_list);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, &wmi_stats_list, list) {
		if (!stats)
			break;

		mem_stats = stats->stats_ptr;

		if (!mem_stats)
			break;

		if (mem_stats->total_bytes){
			len += scnprintf(buf + len, size - len,
				"arena_id = %u\n",
				le32_to_cpu(mem_stats->arena_id));
			len += scnprintf(buf + len, size - len,
				"arena = %s\n",
				wmi_ctrl_path_fw_arena_id_to_name(le32_to_cpu(mem_stats->arena_id)));
			len += scnprintf(buf + len, size - len,
				"total_bytes = %u\n",
				le32_to_cpu(mem_stats->total_bytes));
			len += scnprintf(buf + len, size - len,
				"allocated_bytes = %u\n",
				le32_to_cpu(mem_stats->allocated_bytes));
		}

		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);

	}

	ar->ctrl_mem_stats = false;
	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret_val;
}

static ssize_t ath12k_read_wmi_ctrl_path_stats(struct file *file,
					       char __user *ubuf,
					       size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	enum wmi_tlv_tag tagid;

	tagid = ar->debug.wmi_ctrl_path_stats_tagid;

	switch (tagid) {
	case WMI_TAG_CTRL_PATH_PDEV_STATS:
		ret = wmi_ctrl_path_pdev_stat(ar, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_CAL_STATS:
		ret = wmi_ctrl_path_cal_stat(ar, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_BTCOEX_STATS:
		ret = wmi_ctrl_path_btcoex_stat(ar, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_AWGN_STATS:
		ret = wmi_ctrl_path_awgn_stat(ar, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_MEM_STATS:
		ret = wmi_ctrl_path_mem_stat(ar, ubuf, count, ppos);
		break;
		/* Add case for newly wmi ctrl path added stats here */
	default:
		/* Unsupported tag */
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations ath12k_fops_wmi_ctrl_stats = {
	.write = ath12k_write_wmi_ctrl_path_stats,
	.open = simple_open,
	.read = ath12k_read_wmi_ctrl_path_stats,
};

static void ath12k_debugfs_wmi_ctrl_stats_register(struct ath12k *ar)
{
	debugfs_create_file("wmi_ctrl_stats", 0600,
			    ar->debug.debugfs_pdev,
			    ar,
			    &ath12k_fops_wmi_ctrl_stats);
	INIT_LIST_HEAD(&ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_lock_init(&ar->debug.wmi_ctrl_path_stats_lock);
	init_completion(&ar->debug.wmi_ctrl_path_stats_rcvd);
	ar->debug.wmi_ctrl_path_stats_more_enabled = false;
}

void ath12k_debugfs_soc_destroy(struct ath12k_base *ab)
{
	debugfs_remove_recursive(ab->debugfs_soc);
	ab->debugfs_soc = NULL;
	/* We are not removing ath12k directory on purpose, even if it
	 * would be empty. This simplifies the directory handling and it's
	 * a minor cosmetic issue to leave an empty ath12k directory to
	 * debugfs.
	 */
}

void
ath12k_debugfs_fw_stats_process(struct ath12k *ar,
				struct ath12k_fw_stats *stats)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev *pdev;
	bool is_end;
	static unsigned int num_vdev, num_bcn;
	size_t total_vdevs_started = 0;
	int i;

	if (stats->stats_id == WMI_REQUEST_VDEV_STAT) {
		if (list_empty(&stats->vdevs)) {
			ath12k_warn(ab, "empty vdev stats");
			return;
		}
		/* FW sends all the active VDEV stats irrespective of PDEV,
		 * hence limit until the count of all VDEVs started
		 */
		rcu_read_lock();
		for (i = 0; i < ab->num_radios; i++) {
			pdev = rcu_dereference(ab->pdevs_active[i]);
			if (pdev && pdev->ar)
				total_vdevs_started += pdev->ar->num_started_vdevs;
		}
		rcu_read_unlock();

		is_end = ((++num_vdev) == total_vdevs_started);

		list_splice_tail_init(&stats->vdevs,
				      &ar->fw_stats.vdevs);

		if (is_end) {
			ar->fw_stats.fw_stats_done = true;
			num_vdev = 0;
		}
		return;
	}
	if (stats->stats_id == WMI_REQUEST_BCN_STAT) {
		if (list_empty(&stats->bcn)) {
			ath12k_warn(ab, "empty beacon stats");
			return;
		}
		/* Mark end until we reached the count of all started VDEVs
		 * within the PDEV
		 */
		is_end = ((++num_bcn) == ar->num_started_vdevs);

		list_splice_tail_init(&stats->bcn,
				      &ar->fw_stats.bcn);

		if (is_end) {
			ar->fw_stats.fw_stats_done = true;
			num_bcn = 0;
		}
	}
}

static int ath12k_open_vdev_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_fw_stats_req_params param;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (!ah)
		return -ENETDOWN;

	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	void *buf __free(kfree) = kzalloc(ATH12K_FW_STATS_BUF_SIZE, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	/* VDEV stats is always sent for all active VDEVs from FW */
	param.vdev_id = 0;
	param.stats_id = WMI_REQUEST_VDEV_STAT;

	ret = ath12k_mac_get_fw_stats(ar, &param);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request fw vdev stats: %d\n", ret);
		return ret;
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);

	file->private_data = no_free_ptr(buf);

	return 0;
}

static int ath12k_release_vdev_stats(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_vdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_stats = {
	.open = ath12k_open_vdev_stats,
	.release = ath12k_release_vdev_stats,
	.read = ath12k_read_vdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_bcn_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_link_vif *arvif;
	struct ath12k_fw_stats_req_params param;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah && ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	void *buf __free(kfree) = kzalloc(ATH12K_FW_STATS_BUF_SIZE, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	param.stats_id = WMI_REQUEST_BCN_STAT;

	/* loop all active VDEVs for bcn stats */
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_up)
			continue;

		param.vdev_id = arvif->vdev_id;
		ret = ath12k_mac_get_fw_stats(ar, &param);
		if (ret) {
			ath12k_warn(ar->ab, "failed to request fw bcn stats: %d\n", ret);
			return ret;
		}
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);
	/* since beacon stats request is looped for all active VDEVs, saved fw
	 * stats is not freed for each request until done for all active VDEVs
	 */
	spin_lock_bh(&ar->data_lock);
	ath12k_fw_stats_bcn_free(&ar->fw_stats.bcn);
	spin_unlock_bh(&ar->data_lock);

	file->private_data = no_free_ptr(buf);

	return 0;
}

static int ath12k_release_bcn_stats(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_bcn_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_bcn_stats = {
	.open = ath12k_open_bcn_stats,
	.release = ath12k_release_bcn_stats,
	.read = ath12k_read_bcn_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath12k_open_pdev_stats(struct inode *inode, struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_base *ab = ar->ab;
	struct ath12k_fw_stats_req_params param;
	int ret;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ah && ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	void *buf __free(kfree) = kzalloc(ATH12K_FW_STATS_BUF_SIZE, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	param.vdev_id = 0;
	param.stats_id = WMI_REQUEST_PDEV_STAT;

	ret = ath12k_mac_get_fw_stats(ar, &param);
	if (ret) {
		ath12k_warn(ab, "failed to request fw pdev stats: %d\n", ret);
		return ret;
	}

	ath12k_wmi_fw_stats_dump(ar, &ar->fw_stats, param.stats_id,
				 buf);

	file->private_data = no_free_ptr(buf);

	return 0;
}

static int ath12k_release_pdev_stats(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

static ssize_t ath12k_read_pdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_pdev_stats = {
	.open = ath12k_open_pdev_stats,
	.release = ath12k_release_pdev_stats,
	.read = ath12k_read_pdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_enable_vdev_stats_offload(struct file *file,
						      const char __user *ubuf,
						      size_t count, loff_t *ppos)
{
       struct ath12k *ar = file->private_data;
       bool enable;
       int ret;

       if (kstrtobool_from_user(ubuf, count, &enable))
               return -EINVAL;

       guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

       if (enable == ar->fw_stats.en_vdev_stats_ol) {
               ret = count;
               goto out;
       }

       ar->fw_stats.en_vdev_stats_ol = enable;
       ret = count;

out:
       return ret;
}

static ssize_t ath12k_read_enable_vdev_stats_offload(struct file *file,
	       					     char __user *ubuf,
						     size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	struct ath12k *ar = file->private_data;
	int len = 0;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%u\n",
			ar->fw_stats.en_vdev_stats_ol);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_stats_offload = {
	.read = ath12k_read_enable_vdev_stats_offload,
	.write = ath12k_write_enable_vdev_stats_offload,
	.open = simple_open
};

static
void ath12k_debugfs_fw_stats_register(struct ath12k *ar)
{
	struct dentry *fwstats_dir = debugfs_create_dir("fw_stats",
							ar->debug.debugfs_pdev);

	/* all stats debugfs files created are under "fw_stats" directory
	 * created per PDEV
	 */
	debugfs_create_file("vdev_stats", 0600, fwstats_dir, ar,
			    &fops_vdev_stats);
	debugfs_create_file("beacon_stats", 0600, fwstats_dir, ar,
			    &fops_bcn_stats);
	debugfs_create_file("pdev_stats", 0600, fwstats_dir, ar,
			    &fops_pdev_stats);
	debugfs_create_file("en_vdev_stats_ol", 0600, fwstats_dir, ar,
			    &fops_vdev_stats_offload);

	ath12k_fw_stats_init(ar);
}

static ssize_t ath12k_read_wmm_stats(struct file *file,
                                    char __user *ubuf,
                                    size_t count, loff_t *ppos)
{
       struct ath12k *ar = file->private_data;
       struct ath12k_base *ab = ar->ab;
       struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
       struct ath12k_pdev_dp *dp_pdev = NULL;
       int len = 0;
       int size = 2048;
       char *buf;
       ssize_t retval;
       u64 total_wmm_sent_pkts = 0;
       u64 total_wmm_received_pkts = 0;
       u64 total_wmm_fail_sent = 0;
       u64 total_wmm_fail_received = 0;

       if (!dp) {
	       ath12k_warn(ar->ab, "ath12k_dp not present%s\n", __func__);
	       return -ENOMEM;
       }

	rcu_read_lock();

       dp_pdev = ath12k_dp_to_dp_pdev(dp, ar->pdev_idx);
       if (!dp_pdev) {
		rcu_read_unlock();
	       ath12k_warn(dp, "dp_pdev not present for pdev_idx %d in %s\n",
			   ar->pdev_idx, __func__);
	       return -ENOMEM;
       }

       buf = kzalloc(size, GFP_KERNEL);
       if (!buf) {
		rcu_read_unlock();
               ath12k_warn(dp, "failed to allocate the buffer%s\n", __func__);
               return -ENOMEM;
       }

       wiphy_lock(dp_pdev->hw->wiphy);
       for (count = 0; count < WME_NUM_AC; count++) {
               total_wmm_sent_pkts += dp_pdev->wmm_stats.total_wmm_tx_pkts[count];
	       total_wmm_received_pkts += dp_pdev->wmm_stats.total_wmm_rx_pkts[count];
	       total_wmm_fail_sent += dp_pdev->wmm_stats.total_wmm_tx_drop[count];
	       total_wmm_fail_received += dp_pdev->wmm_stats.total_wmm_rx_drop[count];
       }

       len += scnprintf(buf + len, size - len, "Total number of wmm_sent: %llu\n",
                        total_wmm_sent_pkts);
       len += scnprintf(buf + len, size - len, "total number of wmm_received: %llu\n",
		        total_wmm_received_pkts);
       len += scnprintf(buf + len, size - len, "total number of wmm_fail_sent: %llu\n",
		        total_wmm_fail_sent);
       len += scnprintf(buf + len, size - len, "total number of wmm_fail_received: %llu\n",
		        total_wmm_fail_received);
       len += scnprintf(buf + len, size - len, "Num of BE wmm_sent: %llu\n",
                        dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_BE]);
       len += scnprintf(buf + len, size - len, "Num of BK wmm_sent: %llu\n",
                        dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_BK]);
       len += scnprintf(buf + len, size - len, "Num of VI wmm_sent: %llu\n",
                        dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_VI]);
       len += scnprintf(buf + len, size - len, "Num of VO wmm_sent: %llu\n",
                        dp_pdev->wmm_stats.total_wmm_tx_pkts[WME_AC_VO]);
       len += scnprintf(buf + len, size - len, "num of be wmm_received: %llu\n",
		        dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_BE]);
       len += scnprintf(buf + len, size - len, "num of bk wmm_received: %llu\n",
		        dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_BK]);
       len += scnprintf(buf + len, size - len, "num of vi wmm_received: %llu\n",
		        dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_VI]);
       len += scnprintf(buf + len, size - len, "num of vo wmm_received: %llu\n",
		        dp_pdev->wmm_stats.total_wmm_rx_pkts[WME_AC_VO]);
       len += scnprintf(buf + len, size - len, "num of be wmm_tx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_BE]);
       len += scnprintf(buf + len, size - len, "num of bk wmm_tx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_BK]);
       len += scnprintf(buf + len, size - len, "num of vi wmm_tx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_VI]);
       len += scnprintf(buf + len, size - len, "num of vo wmm_tx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_tx_drop[WME_AC_VO]);
       len += scnprintf(buf + len, size - len, "num of be wmm_rx_dropped: %llu\n",
	                dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_BE]);
       len += scnprintf(buf + len, size - len, "num of bk wmm_rx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_BK]);
       len += scnprintf(buf + len, size - len, "num of vi wmm_rx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_VI]);
       len += scnprintf(buf + len, size - len, "num of vo wmm_rx_dropped: %llu\n",
			dp_pdev->wmm_stats.total_wmm_rx_drop[WME_AC_VO]);

       wiphy_unlock(dp_pdev->hw->wiphy);

	rcu_read_unlock();

       if (len > size)
               len = size;
       retval = simple_read_from_buffer(ubuf, count, ppos, buf, len);
       kfree(buf);

       return retval;
}

static const struct file_operations fops_wmm_stats = {
       .read = ath12k_read_wmm_stats,
       .open = simple_open,
};

static ssize_t ath12k_athdiag_read(struct file *file,
				    char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	wiphy_lock(wiphy);

	buf = vmalloc(count);
	if (!buf) {
		return -ENOMEM;
	}

	ret = ath12k_qmi_mem_read(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath12k_warn(ar->ab, "failed to read address 0x%08x via diagnose window from debugfs: %d\n",
			    (u32)(*ppos), ret);
		goto exit;
	}

	ret = copy_to_user(user_buf, buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	count -= ret;
	*ppos += count;
	ret = count;

exit:
	vfree(buf);
	wiphy_unlock(wiphy);
	return ret;
}

static ssize_t ath12k_athdiag_write(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	wiphy_lock(wiphy);

	buf = vmalloc(count);
	if (!buf) {
		ret = -ENOMEM;
		goto error_unlock;
	}

	ret = copy_from_user(buf, user_buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	ret = ath12k_qmi_mem_write(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath12k_warn(ar->ab, "failed to write address 0x%08x via diagnose window from debugfs: %d\n",
			     (u32)(*ppos), ret);
		goto exit;
	}

	*ppos += count;
	ret = count;

exit:
	vfree(buf);

error_unlock:
	wiphy_unlock(wiphy);
	return ret;
}

static const struct file_operations fops_athdiag = {
	.read = ath12k_athdiag_read,
	.write = ath12k_athdiag_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_enable_m3_dump(struct file *file,
                                           const char __user *ubuf,
                                           size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        bool enable;
        int ret;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	wiphy_lock(wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (enable == ar->debug.enable_m3_dump) {
		ret = count;
		goto exit;
	}

	ret = ath12k_wmi_pdev_m3_dump_enable(ar, enable);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to enable m3 ssr dump %d\n",
			    ret);
		goto exit;
	}

	ar->debug.enable_m3_dump = enable;
	ret = count;

exit:
	wiphy_unlock(wiphy);
	return ret;
}

static ssize_t ath12k_read_enable_m3_dump(struct file *file,
					  char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[32];
	size_t len = 0;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.enable_m3_dump);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);

}

static const struct file_operations fops_enable_m3_dump = {
	.read = ath12k_read_enable_m3_dump,
	.write = ath12k_write_enable_m3_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
};

static ssize_t ath12k_write_btcoex(struct file *file,
                                   const char __user *ubuf,
                                   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	size_t buf_size;
	int ret = 0,coex = BTCOEX_CONFIGURE_DEFAULT, wlan_weight = 0,
	    wlan_prio_mask_value = 0;
	enum qca_wlan_priority_type wlan_prio_mask = QCA_WLAN_PRIORITY_BE;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%d %d %d" , &coex, &wlan_prio_mask_value, &wlan_weight);

	if (!ret) {
		ret = -EINVAL;
		goto exit;
	}

	if (coex != BTCOEX_ENABLE &&  coex != BTCOEX_CONFIGURE_DEFAULT && coex) {
		ret = -EINVAL;
		goto exit;
	}

	wiphy_lock(wiphy);

	switch (coex) {
		case BTCOEX_ENABLE:
			if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags))
				set_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags);
			break;
		case BTCOEX_CONFIGURE_DEFAULT:
			if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
				ret = -EINVAL;
				goto error_unlock;
			}
			break;
		case BTCOEX_DISABLE:
			clear_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags);
			break;
		default:
			ret = -EINVAL;
			goto error_unlock;
	}

	if ((wlan_weight < BTCOEX_CONFIGURE_DEFAULT) ||
			(wlan_prio_mask_value < BTCOEX_CONFIGURE_DEFAULT) ||
			(wlan_weight > BTCOEX_MAX_PKT_WEIGHT) ||
			(wlan_prio_mask_value > BTCOEX_MAX_WLAN_PRIORITY)) {
		ret = -EINVAL;
		goto error_unlock;
	}

	if (wlan_weight == BTCOEX_CONFIGURE_DEFAULT)
		wlan_weight = ar->coex.wlan_weight;

	wlan_prio_mask = ((wlan_prio_mask_value == BTCOEX_CONFIGURE_DEFAULT)?
			   ar->coex.wlan_prio_mask : wlan_prio_mask_value);

	if (ar->ah->state != ATH12K_HW_STATE_ON && ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
		ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		ret = -ENETDOWN;
		goto error_unlock;
	}

	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

	if (!arvif->is_started) {
		ret = -EINVAL;
		goto error_unlock;
	}

	ret = ath12k_mac_btcoex_config(ar, arvif, coex, wlan_prio_mask, wlan_weight);

	if (ret) {
		ath12k_warn(ar->ab,
				"failed to enable coex vdev_id %d ret %d\n",
				arvif->vdev_id, ret);
		goto error_unlock;
	}

	ar->coex.wlan_prio_mask = wlan_prio_mask;
	ar->coex.wlan_weight = wlan_weight;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	int len = 0;

	if (!ar)
		return -EINVAL;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%u %u %u\n",
			test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags),
			ar->coex.wlan_prio_mask,
			ar->coex.wlan_weight);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex = {
	.read = ath12k_read_btcoex,
	.write = ath12k_write_btcoex,
	.open = simple_open
};

static ssize_t ath12k_write_btcoex_duty_cycle(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	struct coex_config_arg coex_config;
	char buf[256] = {0};
	size_t buf_size;
	u32 duty_cycle = 0, wlan_duration = 0;
	int ret = 0;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%u %u", &duty_cycle, &wlan_duration);

	if (!ret) {
		ret = -EINVAL;
		goto exit;
	}

	/*Maximum duty_cycle period allowed is 100 Miliseconds*/
	if (duty_cycle < wlan_duration || !duty_cycle || !wlan_duration || duty_cycle > 100000) {
		ret = -EINVAL;
		goto exit;
	}

	wiphy_lock(wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON && ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
		ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		ret = -ENETDOWN;
		goto error_unlock;
	}

	if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
		ret = -EINVAL;
		goto error_unlock;
	}

	if (ar->coex.coex_algo_type != COEX_ALGO_OCS) {
		ath12k_err(ar->ab,"duty cycle algo is not enabled");
		ret = -EINVAL;
		goto error_unlock;
	}

	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

	if (!arvif->is_started) {
		ret = -EINVAL;
		goto error_unlock;
	}

	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_AP_TDM;
	coex_config.duty_cycle = duty_cycle;
	coex_config.wlan_duration = wlan_duration;
	wiphy_unlock(wiphy);

	ret = ath12k_send_coex_config_cmd(ar, &coex_config);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set duty cycle vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
		goto exit;
	}

	wiphy_lock(wiphy);
	ar->coex.duty_cycle = duty_cycle;
	ar->coex.wlan_duration = wlan_duration;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex_duty_cycle(struct file *file, char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
	int len = 0;

	if (!ar)
		return -EINVAL;

	wiphy_lock(wiphy);
	len = scnprintf(buf, sizeof(buf) - len, "%d %d\n",
			ar->coex.duty_cycle,ar->coex.wlan_duration);
	wiphy_unlock(wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_duty_cycle = {
        .read = ath12k_read_btcoex_duty_cycle,
        .write = ath12k_write_btcoex_duty_cycle,
        .open = simple_open
};

static ssize_t ath12k_write_btcoex_algo(struct file *file,
                                        const char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
        struct ath12k_link_vif *arvif = NULL;
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        struct coex_config_arg coex_config;
        char buf[256] = {0};
        size_t buf_size;
        u32 pta_num = 0, coex_mode = 0, bt_txrx_time  = 0,
        bt_priority_time = 0, pta_algorithm = 0,
        pta_priority = 0;
        int ret = 0;

        if (!ar) {
                ret = -EINVAL;
                goto exit;
        }

        buf_size = min(count, (sizeof(buf) - 1));

        if (copy_from_user(buf, ubuf, buf_size)) {
                ret = -EFAULT;
                goto exit;
        }

        buf[buf_size] = '\0';
        ret = sscanf(buf, "%u 0x%x 0x%x 0x%x 0x%x 0x%x" , &pta_num, &coex_mode,
                     &bt_txrx_time, &bt_priority_time,
                     &pta_algorithm, &pta_priority);

        if (!ret) {
                ret = -EINVAL;
                goto exit;
        }

        if (coex_mode > BTCOEX_PTA_MODE ||
            coex_mode < BTCOEX_THREE_WIRE_MODE ||
            pta_algorithm >= COEX_ALGO_MAX_SUPPORTED) {
                ret = -EINVAL;
                goto exit;
        }

	wiphy_lock(wiphy);

        if (ar->ah->state != ATH12K_HW_STATE_ON &&
            ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
                ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
                ret = -ENETDOWN;
                goto error_unlock;
        }

        if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
                ret = -EINVAL;
                goto error_unlock;
        }

        arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

        if (!arvif->is_started) {
                ret = -EINVAL;
                goto error_unlock;
        }

        coex_config.vdev_id            = arvif->vdev_id;
        coex_config.config_type        = WMI_COEX_CONFIG_PTA_INTERFACE;
        coex_config.pta_num            = pta_num;
        coex_config.coex_mode          = coex_mode;
        coex_config.bt_txrx_time       = bt_txrx_time;
        coex_config.bt_priority_time   = bt_priority_time;
        coex_config.pta_algorithm      = pta_algorithm;
        coex_config.pta_priority       = pta_priority;
	wiphy_unlock(wiphy);

        ret = ath12k_send_coex_config_cmd(ar, &coex_config);

        if (ret) {
                ath12k_warn(ar->ab,
                            "failed to set coex algorithm vdev_id %d ret %d\n",
                            coex_config.vdev_id, ret);
                goto exit;
        }

	wiphy_lock(wiphy);
	ar->coex.pta_num                =   pta_num;
	ar->coex.coex_mode              =   coex_mode;
	ar->coex.bt_active_time_slot    =   bt_txrx_time;
	ar->coex.bt_priority_time_slot  =   bt_priority_time;
	ar->coex.pta_algorithm          =   pta_algorithm;
	ar->coex.pta_priority           =   pta_priority;
	ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
	return ret;
}

static ssize_t ath12k_read_btcoex_algo(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	char buf[256] = {0};
        int len = 0;

	wiphy_lock(wiphy);
        len = scnprintf(buf, sizeof(buf) - len, "%u %u %u %u %u %u\n",
                        ar->coex.pta_num, ar->coex.coex_mode,
                        ar->coex.bt_active_time_slot,
                        ar->coex.bt_priority_time_slot,
                        ar->coex.pta_algorithm, ar->coex.pta_priority);
	wiphy_unlock(wiphy);

        return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_algo = {
        .read = ath12k_read_btcoex_algo,
        .write = ath12k_write_btcoex_algo,
        .open = simple_open
};

static ssize_t ath12k_btcoex_pkt_priority_write(struct file *file,
                                          const char __user *ubuf,
                                          size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        struct ath12k_link_vif *arvif = NULL;
        struct coex_config_arg coex_config;
        char buf[128] = {0};
        size_t buf_size;
        enum qca_wlan_priority_type wlan_pkt_type = 0;
        u32 wlan_pkt_type_continued = 0, wlan_pkt_weight = 0,
        bt_pkt_weight = 0;
        int ret;

        buf_size = min(count, (sizeof(buf) - 1));

        if (copy_from_user(buf, ubuf, buf_size)) {
                ret = -EFAULT;
                goto exit;
        }

        buf[buf_size] = '\0';
        ret = sscanf(buf, "%u %u %u %u" , &wlan_pkt_type,
                     &wlan_pkt_type_continued, &wlan_pkt_weight,
                     &bt_pkt_weight);

        if (!ret) {
                ret = -EINVAL;
                goto exit;
        }

        if (wlan_pkt_type > QCA_WLAN_PRIORITY_MGMT ||
            wlan_pkt_weight > BTCOEX_MAX_PKT_WEIGHT  ||
            bt_pkt_weight > BTCOEX_MAX_PKT_WEIGHT) {
                ret = -EINVAL;
                goto exit;
        }

	wiphy_lock(wiphy);

        if (ar->ah->state != ATH12K_HW_STATE_ON &&
            ar->ah->state != ATH12K_HW_STATE_RESTARTED) {
                ath12k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
                ret = -ENETDOWN;
                goto error_unlock;
        }

        if (!test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags)) {
                ret = -EINVAL;
                goto error_unlock;
        }

        arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);

        if (!arvif->is_started) {
                ret = -EINVAL;
                goto error_unlock;
        }

        coex_config.vdev_id                  = arvif->vdev_id;
        coex_config.config_type              = WMI_COEX_CONFIG_WLAN_PKT_PRIORITY;
        coex_config.wlan_pkt_type            = wlan_pkt_type;
        coex_config.wlan_pkt_type_continued  = wlan_pkt_type_continued;
        coex_config.wlan_pkt_weight          = wlan_pkt_weight;
        coex_config.bt_pkt_weight            = bt_pkt_weight;
	wiphy_unlock(wiphy);

        ret = ath12k_send_coex_config_cmd(ar, &coex_config);

        if (ret) {
                ath12k_warn(ar->ab,
                            "failed to set coex pkt priority vdev_id %d ret %d\n",
                            coex_config.vdev_id, ret);
                goto exit;
        }

	wiphy_lock(wiphy);
        ar->coex.wlan_pkt_type              = wlan_pkt_type;
        ar->coex.wlan_pkt_type_continued    = wlan_pkt_type_continued;
        ar->coex.wlan_weight                = wlan_pkt_weight;
        ar->coex.bt_weight                  = bt_pkt_weight;

        ret = count;

error_unlock:
	wiphy_unlock(wiphy);

exit:
        return ret;
}

static ssize_t ath12k_btcoex_pkt_priority_read(struct file *file,
                                         char __user *ubuf,
                                         size_t count, loff_t *ppos)
{
        struct ath12k *ar = file->private_data;
	struct wiphy *wiphy = ar->ah->hw->wiphy;
        u8 buf[128] = {0};
        size_t len = 0;

	wiphy_lock(wiphy);
        len = scnprintf(buf, sizeof(buf) - len,
                        "%u %u %u %u\n",ar->coex.wlan_pkt_type,
                        ar->coex.wlan_pkt_type_continued, ar->coex.wlan_weight,
                        ar->coex.bt_weight);
        wiphy_unlock(wiphy);

        return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_priority = {
        .read = ath12k_btcoex_pkt_priority_read,
        .write = ath12k_btcoex_pkt_priority_write,
        .open = simple_open,
        .owner = THIS_MODULE,
        .llseek = default_llseek,
};

static ssize_t ath12k_write_simulate_awgn(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	u32 chan_bw_interference_bitmap;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ath12k_warn(ar->ab, "Interface not up\n");
		return -ENETDOWN;
	}

	if (kstrtou32_from_user(user_buf, count, 0, &chan_bw_interference_bitmap))
		return -EINVAL;

	ret = ath12k_wmi_simulate_awgn(ar, chan_bw_interference_bitmap);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_simulate_awgn = {
        .write = ath12k_write_simulate_awgn,
        .open = simple_open
};

void ath12k_debugfs_register(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_hw *hw = ar->ah->hw;
	char pdev_name[5];
	char buf[100] = {0};

	scnprintf(pdev_name, sizeof(pdev_name), "%s%d", "mac", ar->pdev_idx);

	ar->debug.debugfs_pdev = debugfs_create_dir(pdev_name, ab->debugfs_soc);

	/* Create a symlink under ieee80211/phy* */
	scnprintf(buf, sizeof(buf), "../../ath12k/%pd2", ar->debug.debugfs_pdev);
	ar->debug.debugfs_pdev_symlink = debugfs_create_symlink("ath12k",
								hw->wiphy->debugfsdir,
								buf);

	if (ar->mac.sbands[NL80211_BAND_5GHZ].channels) {
		debugfs_create_file("dfs_simulate_radar", 0200,
				    ar->debug.debugfs_pdev, ar,
				    &fops_simulate_radar);
	}

	debugfs_create_file("tpc_stats", 0400, ar->debug.debugfs_pdev, ar,
			    &fops_tpc_stats);
	debugfs_create_file("tpc_stats_type", 0200, ar->debug.debugfs_pdev,
			    ar, &fops_tpc_stats_type);
	init_completion(&ar->debug.tpc_complete);

	debugfs_create_file("wmm_stats", 0644,
		            ar->debug.debugfs_pdev, ar,
			    &fops_wmm_stats);

	debugfs_create_file("athdiag", 0600, ar->debug.debugfs_pdev, ar,
			    &fops_athdiag);
	
	debugfs_create_file("enable_m3_dump", 0600, ar->debug.debugfs_pdev, ar,
                            &fops_enable_m3_dump);

        debugfs_create_file("btcoex", 0644, ar->debug.debugfs_pdev, ar,
                            &fops_btcoex);

        debugfs_create_file("btcoex_duty_cycle", 0644, ar->debug.debugfs_pdev, ar,
                            &fops_btcoex_duty_cycle);

        debugfs_create_file("btcoex_algorithm", 0644, ar->debug.debugfs_pdev, ar,
                            &fops_btcoex_algo);

        debugfs_create_file("btcoex_priority", 0600, ar->debug.debugfs_pdev, ar,
                            &fops_btcoex_priority);

	debugfs_create_file("dump_mgmt_stats", 0644,
				ar->debug.debugfs_pdev, ar,
				&fops_dump_mgmt_stats);

	ath12k_debugfs_htt_stats_register(ar);
	ath12k_debugfs_fw_stats_register(ar);

	if (test_bit(WMI_TLV_SERVICE_CTRL_PATH_STATS_REQUEST,
		     ar->ab->wmi_ab.svc_map))
		ath12k_debugfs_wmi_ctrl_stats_register(ar);

	debugfs_create_file("ext_rx_stats", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_extd_rx_stats);
	if (ar->mac.sbands[NL80211_BAND_6GHZ].channels) {
		debugfs_create_file("simulate_awgn", 0200,
				    ar->debug.debugfs_pdev, ar,
				    &fops_simulate_awgn);
	}
}

static ssize_t ath12k_read_simulate_fw_crash(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	const char buf[] =
		 "To simulate firmware crash write one of the keywords to this file:\n"
		 "`assert` - send WMI_FORCE_FW_HANG_CMDID to firmware to cause assert.\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf)); }

static ssize_t ath12k_write_simulate_fw_crash(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_base *tmp_ab, *ab = file->private_data;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	struct ath12k *ar = NULL;
	char buf[32] = {0};
	int i, ret;
	ssize_t rc;

	/* filter partial writes and invalid commands */
	if (*ppos != 0 || count >= sizeof(buf) || count == 0)
		return -EINVAL;

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (rc < 0)
		return rc;

	/* drop the possible '\n' from the end */
	if (buf[*ppos - 1] == '\n')
		buf[*ppos - 1] = '\0';

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar)
			break;
	}

	if (!ar)
		return -ENETDOWN;

	for (i = 0; i < ag->num_devices; i++) {
		tmp_ab = ag->ab[i];
		if(!tmp_ab)
			continue;
		if (test_bit(ATH12K_FLAG_RECOVERY, &tmp_ab->dev_flags)) {
			ath12k_err(tmp_ab, "Already in recovery\n");
			return -EPERM;
		}
	}

	if (!strcmp(buf, "assert")) {
		ath12k_info(ab, "simulating firmware assert crash\n");
		ret = ath12k_wmi_force_fw_hang_cmd(ar,
						   ATH12K_WMI_FW_HANG_ASSERT_TYPE,
						   ATH12K_WMI_FW_HANG_DELAY, false);
	} else {
		return -EINVAL;
	}

	if (ret) {
		ath12k_warn(ab, "failed to simulate firmware crash: %d\n", ret);
		return ret;
	}

	return count;
}

static const struct file_operations fops_simulate_fw_crash = {
	.read = ath12k_read_simulate_fw_crash,
	.write = ath12k_write_simulate_fw_crash,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_send_fw_hang_cmd(struct ath12k_base *ab,
                            unsigned int value)
{
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct ath12k_hw_group *ag = ab->ag;
	enum wmi_fw_hang_recovery_mode_type recovery_mode;
	int ret, radio_idx, radioup = 0;
	int i;

	if (!value)
		recovery_mode = ATH12K_WMI_DISABLE_FW_RECOVERY;
	else
		recovery_mode = (value == ATH12K_FW_RECOVERY_ENABLE_AUTO) ?
			ATH12K_WMI_FW_HANG_RECOVERY_MODE0 : ATH12K_WMI_DISABLE_FW_RECOVERY;

	if (ag->mlo_capable) {
		for (i = 0; i < ag->num_devices; i++) {
			ab = ag->ab[i];
			mutex_lock(&ab->core_lock);
			ab->fw_recovery_support = value;
			mutex_unlock(&ab->core_lock);

			/*
			 * TODO: Set MODE0 or MODE 1, if recovery mode addr is valid.
			 * TODO: Instead of checking recovery mode addr from
			 * TLV, need to check WMI caps once the support is
			 * added from FW.
			 */
			//if (ab->recovery_mode_address) {
				for (radio_idx = 0; radio_idx < ab->num_radios; radio_idx++) {

					pdev = &ab->pdevs[radio_idx];
					ar = pdev->ar;
					if (ar && ar->ah->state == ATH12K_HW_STATE_ON) {
						radioup = 1;
						break;
					}
				}

				if (radioup) {
					ret = ath12k_wmi_force_fw_hang_cmd(ar,
									   recovery_mode,
									   ATH12K_WMI_FW_HANG_DELAY, false);
					ath12k_info(ab, "setting FW assert mode [%d] ret [%d]\n", recovery_mode, ret);
				} else
					continue;
			//}
		}
	}
}

static ssize_t ath12k_debug_write_fw_recovery(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	unsigned int value;
	int ret;

	if (kstrtouint_from_user(user_buf, count, 0, &value))
		return -EINVAL;

	if (value < ATH12K_FW_RECOVERY_DISABLE ||
	    value > ATH12K_FW_RECOVERY_ENABLE_AUTO) {
		ath12k_warn(ab, "Please enter: 0 = Disable, 1 = Enable (auto recovery)");
		ret = -EINVAL;
		goto exit;
	}

	ath12k_send_fw_hang_cmd(ab, value);

	ret = count;

exit:
	return ret;
}

static ssize_t ath12k_debug_read_fw_recovery(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	char buf[32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ab->fw_recovery_support);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_fw_recovery = {
	.read = ath12k_debug_read_fw_recovery,
	.write = ath12k_debug_write_fw_recovery,
	.open = simple_open,
};

static ssize_t ath12k_read_fw_dbglog(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	size_t len;
	char buf[128];

	len = scnprintf(buf, sizeof(buf), "%u 0x%016llx\n",
			ab->fw_dbglog_param, ab->fw_dbglog_val);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_fw_dbglog(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k *ar = ab->pdevs[0].ar;
	char buf[128] = {0};
	unsigned int param;
	u64 value;
	int ret;

	if (!ar)
		return -EINVAL;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		goto out;

	ret = sscanf(buf, "%u %llx", &param, &value);

	if (ret != 2) {
		ret = -EINVAL;
		goto out;
	}

	ab->fw_dbglog_param = param;
	ab->fw_dbglog_val = value;
	ret = ath12k_wmi_dbglog_cfg(ar, param, value);
	if (ret) {
		ath12k_warn(ab, "dbglog cfg failed from debugfs: %d\n",
			    ret);
		goto out;
	}

	ret = count;

out:
	return ret;
}

static const struct file_operations fops_fw_dbglog = {
	.read = ath12k_read_fw_dbglog,
	.write = ath12k_write_fw_dbglog,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_debug_fw_reset_stats_read(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	int ret;
	size_t len = 0, buf_len = 500;

	void *buf __free(kfree) = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&ab->base_lock);
	len += scnprintf(buf + len, buf_len - len,
			 "fw_crash_counter\t\t%d\n", ab->stats.fw_crash_counter);
	len += scnprintf(buf + len, buf_len - len,
			 "last_recovery_time\t\t%d\n", ab->stats.last_recovery_time);
	spin_unlock_bh(&ab->base_lock);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	return ret;
}

static const struct file_operations fops_fw_reset_stats = {
	.open = simple_open,
	.read = ath12k_debug_fw_reset_stats_read,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_trace_qdss(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char buf[] =
	"1 - this will start qdss trace collection\n"
	"0 - this will stop and save the qdss trace collection\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

static ssize_t
ath12k_write_trace_qdss(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, ret;
	bool radioup = false;
	bool qdss_enable;

	if (kstrtobool_from_user(user_buf, count, &qdss_enable))
		return -EINVAL;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar && ar->ah->state == ATH12K_HW_STATE_ON) {
			radioup = true;
			break;
		}
	}

	if (!radioup) {
		ath12k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	if (qdss_enable) {
		if (ab->is_qdss_tracing) {
			ret = count;
			goto exit;
		}
		ath12k_config_qdss(ab);
	} else {
		if (!ab->is_qdss_tracing) {
			ret = count;
			goto exit;
		}
		ret = ath12k_send_qdss_trace_mode_req(ab,
						      QMI_WLANFW_QDSS_TRACE_OFF_V01);
		if (ret < 0)
			ath12k_warn(ab,
				    "Failed to stop QDSS: %d\n", ret);
	}

	ret = count;

exit:
	return ret;
}

static const struct file_operations fops_trace_qdss = {
	.read = ath12k_read_trace_qdss,
	.write = ath12k_write_trace_qdss,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_debugfs_pdev_create(struct ath12k_base *ab) {
	debugfs_create_file("simulate_fw_crash", 0600, ab->debugfs_soc, ab,
			    &fops_simulate_fw_crash);
	debugfs_create_file("set_fw_recovery", 0600, ab->debugfs_soc, ab,
			    &fops_fw_recovery);
	debugfs_create_file("fw_dbglog_config", 0600, ab->debugfs_soc, ab,
			    &fops_fw_dbglog);
	debugfs_create_file("fw_reset_stats", 0400, ab->debugfs_soc, ab,
			    &fops_fw_reset_stats);
	debugfs_create_file("trace_qdss", 0600, ab->debugfs_soc, ab,
			    &fops_trace_qdss);
	debugfs_create_file("device_dp_stats", 0600, ab->debugfs_soc, ab,
			    &fops_device_dp_stats);
}

void ath12k_debugfs_unregister(struct ath12k *ar)
{
	if (!ar->debug.debugfs_pdev)
		return;

	/* Remove symlink under ieee80211/phy* */
	debugfs_remove(ar->debug.debugfs_pdev_symlink);
	debugfs_remove_recursive(ar->debug.debugfs_pdev);
	ar->debug.debugfs_pdev_symlink = NULL;
	ar->debug.debugfs_pdev = NULL;

	/* Remove wmi ctrl stats file */
	debugfs_remove(ar->wmi_ctrl_stat);
	ar->wmi_ctrl_stat = NULL;
}

static ssize_t ath12k_write_twt_add_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_add_dialog_params params = { 0 };
	u8 buf[128] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf,
		     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u %u %u %hhu %hhu %hhu %hhu %hhu",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.wake_intvl_us,
		     &params.wake_intvl_mantis,
		     &params.wake_dura_us,
		     &params.sp_offset_us,
		     &params.twt_cmd,
		     &params.flag_bcast,
		     &params.flag_trigger,
		     &params.flag_flow_type,
		     &params.flag_protection);
	if (ret != 16)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_add_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_del_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_del_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id);
	if (ret != 7)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_del_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_pause_dialog(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_pause_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id);
	if (ret != 7)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_pause_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath12k_write_twt_resume_dialog(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct wmi_twt_resume_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath12k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.sp_offset_us,
		     &params.next_twt_size);
	if (ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath12k_wmi_send_twt_resume_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations ath12k_fops_twt_add_dialog = {
	.write = ath12k_write_twt_add_dialog,
	.open = simple_open
};

static const struct file_operations ath12k_fops_twt_del_dialog = {
	.write = ath12k_write_twt_del_dialog,
	.open = simple_open
};

static const struct file_operations ath12k_fops_twt_pause_dialog = {
	.write = ath12k_write_twt_pause_dialog,
	.open = simple_open
};

static const struct file_operations ath12k_fops_twt_resume_dialog = {
	.write = ath12k_write_twt_resume_dialog,
	.open = simple_open
};

void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_hw *hw = arvif->ar->ah->hw;
	struct ieee80211_vif *vif = ahvif->vif;
	u8 link_id = arvif->link_id;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return;

	if (vif->type != NL80211_IFTYPE_AP)
		return;

	if (arvif->debugfs_twt)
		return;

	if (!hw->wiphy->n_radio)
		arvif->debugfs_twt = debugfs_create_dir("twt",
							vif->debugfs_dir);
	else
		arvif->debugfs_twt = debugfs_create_dir("twt",
							vif->link_debugfs[link_id]);

	if (!arvif->debugfs_twt || IS_ERR(arvif->debugfs_twt)) {
		ath12k_warn(arvif->ar->ab,
			    "failed to create directory %p\n",
			    arvif->debugfs_twt);
		arvif->debugfs_twt = NULL;
		return;
	}

	debugfs_create_file("add_dialog", 0200, arvif->debugfs_twt,
			    arvif, &ath12k_fops_twt_add_dialog);

	debugfs_create_file("del_dialog", 0200, arvif->debugfs_twt,
			    arvif, &ath12k_fops_twt_del_dialog);

	debugfs_create_file("pause_dialog", 0200, arvif->debugfs_twt,
			    arvif, &ath12k_fops_twt_pause_dialog);

	debugfs_create_file("resume_dialog", 0200, arvif->debugfs_twt,
			    arvif, &ath12k_fops_twt_resume_dialog);
}

void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif)
{
	arvif->debugfs_twt = NULL;
}
