// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/relay.h>
#include <linux/cpu.h>
#include <asm/unaligned.h>
#include "core.h"
#include "debug.h"

#define ATH12K_SPECTRAL_NUM_RESP_PER_EVENT	2
#define ATH12K_SPECTRAL_EVENT_TIMEOUT_MS	1

#define ATH12K_SPECTRAL_DWORD_SIZE		4
#define ATH12K_SPECTRAL_BIN_SIZE		1
#define ATH12K_SPECTRAL_ATH12K_MIN_IB_BINS	(ATH12K_SPECTRAL_ATH12K_MIN_BINS>>1)
#define ATH12K_SPECTRAL_ATH12K_MAX_IB_BINS(x)	\
					((x)->hw_params->spectral.max_fft_bins)

#define ATH12K_SPECTRAL_PER_SAMPLE_SIZE(x)	(sizeof(struct fft_sample_ath12k) + \
					 ATH12K_SPECTRAL_ATH12K_MAX_IB_BINS(x))
#define ATH12K_SPECTRAL_SUB_BUFF_SIZE(x)	ATH12K_SPECTRAL_PER_SAMPLE_SIZE(x)

#define ATH12K_SPECTRAL_20MHZ			20
#define ATH12K_SPECTRAL_40MHZ			40
#define ATH12K_SPECTRAL_80MHZ			80
#define ATH12K_SPECTRAL_160MHZ                  160
#define ATH12K_SPECTRAL_320MHZ                  320

#define ATH12K_SPECTRAL_SIGNATURE               0xFA

#define ATH12K_SPECTRAL_TAG_RADAR_SUMMARY	0x0
#define ATH12K_SPECTRAL_TAG_RADAR_FFT		0x1
#define ATH12K_SPECTRAL_TAG_SCAN_SUMMARY	0x2
#define ATH12K_SPECTRAL_TAG_SCAN_SEARCH		0x3

#define ATH12K_SPECTRAL_PWR_FORMAT_LINEAR	0
#define ATH12K_SPECTRAL_PWR_FORMAT_DBM		1

#define SPECTRAL_TLV_HDR_LEN				GENMASK(15, 0)
#define SPECTRAL_TLV_HDR_TAG				GENMASK(23, 16)
#define SPECTRAL_TLV_HDR_SIGN				GENMASK(31, 24)

#define SPECTRAL_SUMMARY_INFO0_AGC_TOTAL_GAIN		GENMASK(7, 0)
#define SPECTRAL_SUMMARY_INFO0_OB_FLAG			BIT(8)
#define SPECTRAL_SUMMARY_INFO0_GRP_IDX			GENMASK(16, 9)
#define SPECTRAL_SUMMARY_INFO0_RECENT_RFSAT		BIT(17)
#define SPECTRAL_SUMMARY_INFO0_INBAND_PWR_DB		GENMASK(27, 18)
#define SPECTRAL_SUMMARY_INFO0_FALSE_SCAN		BIT(28)
#define SPECTRAL_SUMMARY_INFO0_DETECTOR_ID		GENMASK(30, 29)
#define SPECTRAL_SUMMARY_INFO0_PRI80			BIT(31)

#define SPECTRAL_SUMMARY_INFO2_PEAK_SIGNED_IDX		GENMASK(11, 0)
#define SPECTRAL_SUMMARY_INFO2_PEAK_MAGNITUDE		GENMASK(21, 12)
#define SPECTRAL_SUMMARY_INFO2_NARROWBAND_MASK		GENMASK(29, 22)
#define SPECTRAL_SUMMARY_INFO3_GAIN_CHANGE		BIT(16)

#define ATH12K_SPECTRAL_SUMMARY_PAD_BLANKING_TAG	0xc0debeaf

struct spectral_tlv {
	__le32 timestamp;
	__le32 header;
} __packed;

struct spectral_summary_fft_report {
	__le32 timestamp;
	__le32 tlv_header;
	__le32 info0;
	__le32 reserve0;
	__le32 info2;
	__le32 reserve1;
} __packed;

struct spectral_summary_report_padding {
	__le32 hdr_a;
	__le32 hdr_b;
	__le32 hdr_c;
	__le32 hdr_d;
} __packed;

struct ath12k_spectral_summary_report {
	struct ath12k_wmi_dma_buf_release_meta_data_params meta;
	u32 timestamp;
	u8 agc_total_gain;
	u8 grp_idx;
	u16 inb_pwr_db;
	s16 peak_idx;
	u16 peak_mag;
	u8 detector_id;
	bool out_of_band_flag;
	bool rf_saturation;
	bool primary80;
	bool gain_change;
	bool false_scan;
	u8 blanking_status;
};

#define SPECTRAL_FFT_REPORT_INFO0_DETECTOR_ID		GENMASK(1, 0)
#define SPECTRAL_FFT_REPORT_INFO0_FFT_NUM		GENMASK(4, 2)
#define SPECTRAL_FFT_REPORT_INFO0_RADAR_CHECK		GENMASK(18, 5)
#define SPECTRAL_FFT_REPORT_INFO0_PEAK_SIGNED_IDX	GENMASK(29, 19)

#define SPECTRAL_FFT_REPORT_INFO1_CHAIN_IDX		GENMASK(2, 0)
#define SPECTRAL_FFT_REPORT_INFO1_BASE_PWR_DB		GENMASK(11, 3)
#define SPECTRAL_FFT_REPORT_INFO1_TOTAL_GAIN_DB		GENMASK(19, 12)

#define SPECTRAL_FFT_REPORT_INFO2_NUM_STRONG_BINS	GENMASK(7, 0)
#define SPECTRAL_FFT_REPORT_INFO2_PEAK_MAGNITUDE	GENMASK(17, 8)
#define SPECTRAL_FFT_REPORT_INFO2_AVG_PWR_DB		GENMASK(24, 18)
#define SPECTRAL_FFT_REPORT_INFO2_REL_PWR_DB		GENMASK(31, 25)

#define ATH12K_SPECTRAL_FFT_PEAK_IDX_WIDTH		11
#define ATH12K_SPECTRAL_FFT_PEAK_MAG_WIDTH		10
#define ath12k_spectral_signed_val(value, width) ({ \
	u32 __val = (value); \
	u32 __sign = BIT((width) - 1); \
	(s16)((__val ^ __sign) - __sign); \
})

struct spectral_search_fft_report {
	__le32 timestamp;
	__le32 tlv_header;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 reserve0;
	u8 bins[0];
} __packed;

struct ath12k_spectral_search_report {
	u32 timestamp;
	u32 last_raw_timestamp;
	u32 adjusted_timestamp;
	u32 last_timestamp;
	u32 timestamp_war_offset;
	u8 detector_id;
	u8 fft_count;
	u16 radar_check;
	s16 peak_idx;
	u8 chain_idx;
	u16 base_pwr_db;
	u8 total_gain_db;
	u8 strong_bin_count;
	s16 peak_mag;
	u8 avg_pwr_db;
	u8 rel_pwr_db;
};

static enum spectral_scan_mode
ath12k_spectral_get_scan_mode_from_detector(struct ath12k *ar, u8 detector_id)
{
	if (ath12k_mac_is_phya1_pdev(ar)) {
		if (detector_id == ATH12K_SPECTRAL_DETECTOR_PHYA1_NORMAL)
			return SPECTRAL_SCAN_MODE_NORMAL;
		return SPECTRAL_SCAN_MODE_INVALID;
	}

	switch (detector_id) {
	case ATH12K_SPECTRAL_DETECTOR_NORMAL:
		return SPECTRAL_SCAN_MODE_NORMAL;
	case ATH12K_SPECTRAL_DETECTOR_AGILE:
		return SPECTRAL_SCAN_MODE_AGILE;
	default:
		return SPECTRAL_SCAN_MODE_INVALID;
	}
}

static u8 ath12k_spectral_get_detector_from_scan_mode(struct ath12k *ar,
						      enum spectral_scan_mode smode)
{
	switch (smode) {
	case SPECTRAL_SCAN_MODE_NORMAL:
		return ath12k_mac_is_phya1_pdev(ar) ?
			ATH12K_SPECTRAL_DETECTOR_PHYA1_NORMAL :
			ATH12K_SPECTRAL_DETECTOR_NORMAL;
	case SPECTRAL_SCAN_MODE_AGILE:
		return ATH12K_SPECTRAL_DETECTOR_AGILE;
	default:
		return ATH12K_SPECTRAL_NUM_DETECTORS;
	}
}

bool ath12k_spectral_is_agile_capable(struct ath12k *ar)
{
	struct ath12k_pdev_cap *cap;

	if (!ar || !ar->pdev)
		return false;

	cap = &ar->pdev->cap;

	return cap->agile_spectral_cap ||
	       cap->agile_spectral_cap_160 ||
	       cap->agile_spectral_cap_80p80 ||
	       cap->agile_spectral_cap_320;
}

bool ath12k_spectral_is_agile_bw_capable(struct ath12k *ar,
					 enum nl80211_chan_width width)
{
	struct ath12k_pdev_cap *cap;

	if (!ar || !ar->pdev)
		return false;

	cap = &ar->pdev->cap;

	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_40:
	case NL80211_CHAN_WIDTH_80:
		return cap->agile_spectral_cap;
	case NL80211_CHAN_WIDTH_160:
		return cap->agile_spectral_cap_160;
	case NL80211_CHAN_WIDTH_80P80:
		return cap->agile_spectral_cap_80p80;
	case NL80211_CHAN_WIDTH_320:
		return cap->agile_spectral_cap_320;
	default:
		return false;
	}
}

static void ath12k_spectral_verify_ts(struct ath12k *ar, u8 *buf,
				      u32 current_ts, u8 detector_id)
{
	struct ath12k_spectral *sp = &ar->spectral;

	if (!sp->dbr_buff_debug)
		return;

	if (detector_id >= ATH12K_SPECTRAL_NUM_DETECTORS) {
		ath12k_warn(ar->ab, "spectral detector_id %u exceeds range\n",
			    detector_id);
		return;
	}

	if (sp->prev_tstamp[detector_id] &&
	    current_ts == sp->prev_tstamp[detector_id]) {
		ath12k_warn(ar->ab,
			    "spectral timestamp(%u) in buffer(%p) matches previous timestamp\n",
			    current_ts, buf);
	}

	sp->prev_tstamp[detector_id] = current_ts;
}

static void ath12k_spectral_timestamp_war_init(struct ath12k *ar)
{
	struct ath12k_spectral *sp = &ar->spectral;
	int i;

	for (i = 0; i < SPECTRAL_SCAN_MODE_MAX; i++) {
		sp->last_fft_timestamp[i] = 0;
		sp->timestamp_war_offset[i] = 0;
	}

	sp->target_reset_count = 0;
	memset(sp->prev_tstamp, 0, sizeof(sp->prev_tstamp));
}

static int ath12k_spectral_get_adjusted_timestamp(struct ath12k *ar,
						  u32 raw_timestamp,
						  u32 reset_delay,
						  enum spectral_scan_mode smode,
						  u32 *adjusted_timestamp,
						  u32 *last_raw_timestamp,
						  u32 *last_timestamp,
						  u32 *timestamp_war_offset)
{
	struct ath12k_spectral *sp = &ar->spectral;
	u32 prev_raw;
	u32 prev_offset;
	int i;

	if (smode >= SPECTRAL_SCAN_MODE_MAX)
		return -EINVAL;

	prev_raw = sp->last_fft_timestamp[smode];
	prev_offset = sp->timestamp_war_offset[smode];

	if (reset_delay) {
		for (i = 0; i < SPECTRAL_SCAN_MODE_MAX; i++)
			sp->timestamp_war_offset[i] +=
				reset_delay + sp->last_fft_timestamp[i];
		sp->target_reset_count++;
	}

	sp->last_fft_timestamp[smode] = raw_timestamp;

	*adjusted_timestamp = raw_timestamp + sp->timestamp_war_offset[smode];
	*last_raw_timestamp = prev_raw;
	*last_timestamp = prev_raw + prev_offset;
	*timestamp_war_offset = sp->timestamp_war_offset[smode];

	return 0;
}

static size_t ath12k_spectral_get_bin_count_after_len_adj(struct ath12k *ar,
							  size_t fft_bin_len,
							  u32 rpt_mode,
							  size_t *fft_bin_size)
{
	struct ath12k_base *ab = ar->ab;
	size_t bin_sz = ab->hw_params->spectral.fft_bin_sz;
	size_t bin_count;

	if (!bin_sz) {
		*fft_bin_size = 0;
		return 0;
	}

	if (rpt_mode == ATH12K_SPECTRAL_RPT_MODE_1) {
		/* Header + summary report only, no FFT bins expected */
		*fft_bin_size = 0;
		return 0;
	}

	bin_count = fft_bin_len / bin_sz;
	*fft_bin_size = bin_sz;

	/* Mode 2 forwards only the in-band half of the FFT; mode 3
	 * forwards all bins and needs no halving.
	 */
	if (rpt_mode == ATH12K_SPECTRAL_RPT_MODE_2)
		bin_count >>= 1;

	return bin_count;
}

static struct dentry *create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      umode_t mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;

	buf_file = debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);
	*is_global = 1;
	return buf_file;
}

static int remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);

	return 0;
}

static int ath12k_spectral_subbuf_start(struct rchan_buf *buf, void *subbuf,
					void *prev_subbuf, size_t prev_padding)
{
	struct ath12k_spectral *sp = buf->chan->private_data;

	(void)subbuf;
	(void)prev_subbuf;
	(void)prev_padding;

	if (relay_buf_full(buf))
		atomic64_inc(&sp->relay_overwrite_events);

	return 1;
}

static bool
ath12k_spectral_relay_write_record(struct rchan *chan, const void *data,
				   size_t length)
{
	unsigned long flags;
	struct rchan_buf *buf;

	local_irq_save(flags);
	buf = *this_cpu_ptr(chan->buf);
	if (unlikely(buf->offset + length > chan->subbuf_size)) {
		length = relay_switch_subbuf(buf, length);
		if (!length) {
			local_irq_restore(flags);
			return false;
		}
	}

	memcpy(buf->data + buf->offset, data, length);
	buf->offset += length;
	if (buf->offset == chan->subbuf_size)
		relay_switch_subbuf(buf, 0);
	local_irq_restore(flags);

	return true;
}

static ssize_t
ath12k_spectral_relay_overwrite_events_read(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_overwrite_events));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath12k_spectral_relay_full_events_read(struct file *file,
				       char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_full_events));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath12k_spectral_relay_write_attempts_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_write_attempts));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath12k_spectral_relay_write_accepted_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_write_accepted));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath12k_spectral_relay_write_rejected_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_write_rejected));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static u64 ath12k_spectral_relay_occupancy(struct ath12k_spectral *sp)
{
	struct rchan_buf *buf;
	size_t produced, consumed, occupancy;

	if (!sp->rfs_scan || !sp->rfs_scan->buf)
		return 0;

	buf = *per_cpu_ptr(sp->rfs_scan->buf, 0);
	produced = READ_ONCE(buf->subbufs_produced);
	consumed = READ_ONCE(buf->subbufs_consumed);
	if (produced < consumed)
		return 0;

	occupancy = produced - consumed;
	if (READ_ONCE(buf->offset))
		occupancy++;
	if (occupancy > sp->num_sub_bufs)
		occupancy = sp->num_sub_bufs;

	return occupancy;
}

static void ath12k_spectral_relay_update_peak(struct ath12k_spectral *sp)
{
	u64 occupancy = ath12k_spectral_relay_occupancy(sp);

	if (occupancy > atomic64_read(&sp->relay_peak_occupancy))
		atomic64_set(&sp->relay_peak_occupancy, occupancy);
}

static ssize_t
ath12k_spectral_relay_peak_occupancy_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)atomic64_read(&sp->relay_peak_occupancy));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath12k_spectral_relay_current_occupancy_read(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	char buf[32];
	int len;

	len = scnprintf(buf, sizeof(buf), "%llu\n",
			(unsigned long long)ath12k_spectral_relay_occupancy(sp));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ath12k_spectral_relay_write_attempts_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_write_attempts_read,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_write_accepted_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_write_accepted_read,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_write_rejected_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_write_rejected_read,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_overwrite_events_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_overwrite_events_read,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_current_occupancy_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_current_occupancy_read,
	.llseek = default_llseek,
};

static ssize_t
ath12k_spectral_relay_reset_write(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath12k_spectral *sp = file->private_data;
	bool scan_active;

	(void)user_buf;
	(void)ppos;

	if (!count)
		return 0;
	if (!sp->rfs_scan)
		return -ENODEV;

	/* Write-only trigger; the payload itself is intentionally ignored. */
	spin_lock_bh(&sp->lock);
	scan_active = sp->scan_active;
	spin_unlock_bh(&sp->lock);
	if (scan_active)
		return -EBUSY;

	/*
	 * Block new relay writers, then wait for the writer already in flight.
	 * The gate remains set while relay_reset() runs outside the spinlock.
	 */
	atomic_set(&sp->relay_resetting, 1);
	spin_lock_bh(&sp->lock);
	spin_unlock_bh(&sp->lock);
	spin_lock_bh(&sp->relay_lock);
	spin_unlock_bh(&sp->relay_lock);
	relay_reset(sp->rfs_scan);
	spin_lock_bh(&sp->lock);
	spin_unlock_bh(&sp->lock);
	spin_lock_bh(&sp->relay_lock);

	/* relay_reset() resets subbufs_produced/consumed to 0 but does not
	 * zero the underlying memory. Without this memset, stale FFT records
	 * from the previous scan remain in the relay pages and are read back
	 * as fresh data when subbufs_produced wraps around to those slots
	 * again (~923ms after scan start at 224us period). The stale records
	 * pass iw_is_selected_sample, causing samples >> relay_write_attempts
	 * and filling the relay with CPU-bound filter work, which in turn
	 * starves the producer and causes relay_full_events.
	 */
	{
		struct rchan_buf *buf;
		int cpu;

		for_each_possible_cpu(cpu) {
			buf = *per_cpu_ptr(sp->rfs_scan->buf, cpu);
			if (!buf)
				continue;

			memset(buf->start, 0,
			       sp->rfs_scan->subbuf_size * sp->rfs_scan->n_subbufs);
		}
	}

	atomic64_set(&sp->relay_write_attempts, 0);
	atomic64_set(&sp->relay_write_accepted, 0);
	atomic64_set(&sp->relay_write_rejected, 0);
	atomic64_set(&sp->relay_full_events, 0);
	atomic64_set(&sp->relay_overwrite_events, 0);
	atomic64_set(&sp->relay_peak_occupancy, 0);
	spin_unlock_bh(&sp->relay_lock);
	atomic_set(&sp->relay_resetting, 0);

	return count;
}

static const struct file_operations ath12k_spectral_relay_reset_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = ath12k_spectral_relay_reset_write,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_full_events_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_full_events_read,
	.llseek = default_llseek,
};

static const struct file_operations ath12k_spectral_relay_peak_occupancy_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ath12k_spectral_relay_peak_occupancy_read,
	.llseek = default_llseek,
};

static struct rchan_callbacks rfs_scan_cb = {
	.subbuf_start = ath12k_spectral_subbuf_start,
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

struct ath12k_link_vif *ath12k_spectral_get_vdev(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* if there already is a created vif doing spectral, return that. */
	list_for_each_entry(arvif, &ar->arvifs, list)
		if (arvif->is_created && arvif->spectral_enabled)
			return arvif;

	/* otherwise, return the first created vif. arvifs left over from
	 * SSR recovery have is_created cleared but may still be linked.
	 */
	list_for_each_entry(arvif, &ar->arvifs, list)
		if (arvif->is_created)
			return arvif;

	return NULL;
}

int ath12k_spectral_nl80211_bw_to_idx(enum nl80211_chan_width bw)
{
	switch (bw) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:	return ATH12K_SPECTRAL_BW_20MHZ;
	case NL80211_CHAN_WIDTH_40:	return ATH12K_SPECTRAL_BW_40MHZ;
	case NL80211_CHAN_WIDTH_80:	return ATH12K_SPECTRAL_BW_80MHZ;
	case NL80211_CHAN_WIDTH_160:return ATH12K_SPECTRAL_BW_160MHZ;
	case NL80211_CHAN_WIDTH_320:return ATH12K_SPECTRAL_BW_320MHZ;
	default:
		return -1;
	}
}

static int ath12k_spectral_nl80211_bw_to_wmi_chwidth(enum nl80211_chan_width bw)
{
	switch (bw) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		return WMI_PEER_CHWIDTH_20MHZ;
	case NL80211_CHAN_WIDTH_40:
		return WMI_PEER_CHWIDTH_40MHZ;
	case NL80211_CHAN_WIDTH_80:
		return WMI_PEER_CHWIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_160:
		return WMI_PEER_CHWIDTH_160MHZ;
	case NL80211_CHAN_WIDTH_320:
		return WMI_PEER_CHWIDTH_320MHZ;
	default:
		return -1;
	}
}

static void ath12k_spectral_update_chwidth(struct ath12k *ar,
					   struct ath12k_link_vif *arvif)
{
	struct cfg80211_chan_def *chandef = &arvif->chanctx.def;
	int wmi_bw;

	if (!chandef->chan) {
		ath12k_warn(ar->ab,
			    "spectral start_scan: vdev %u has no channel context\n",
			    arvif->vdev_id);
		return;
	}

	wmi_bw = ath12k_spectral_nl80211_bw_to_wmi_chwidth(chandef->width);
	if (wmi_bw < 0) {
		ath12k_warn(ar->ab,
			    "spectral start_scan: unsupported chandef width %u\n",
			    chandef->width);
		return;
	}

	spin_lock_bh(&ar->spectral.lock);
	ar->spectral.ch_width     = wmi_bw;
	ar->spectral.sscan_bw     = wmi_bw;
	ar->spectral.pri20_freq   = chandef->chan->center_freq;
	ar->spectral.oper_cfreq1  = chandef->center_freq1;
	ar->spectral.sscan_cfreq1 = chandef->center_freq1;
	ar->spectral.sscan_cfreq2 = chandef->center_freq2;
	spin_unlock_bh(&ar->spectral.lock);
}

static void ath12k_spectral_init_param_min_max(struct ath12k *ar)
{
	struct ath12k_spectral_param_min_max *pmm = &ar->spectral.param_min_max;
	const u16 *hw_max = ar->ab->hw_params->spectral.fft_size_max;
	int i;

	pmm->fft_size_min   = ar->ab->hw_params->spectral.fft_size_min;
	pmm->scan_count_max = ar->spectral.scan_count_max;


	for (i = 0; i < ATH12K_SPECTRAL_NUM_BW_SLOTS; i++)
		pmm->fft_size_max[i] = hw_max[i];
}

int ath12k_spectral_start_scan(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	enum spectral_scan_mode smode;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* No WMI config in firmware yet; trigger would be undefined. */
	if (ar->spectral.mode >= SPECTRAL_SCAN_MODE_MAX)
		return 0;

	/* Scan already running; avoid restarting and losing in-flight samples. */
	if (ar->spectral.scan_active) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral start_scan: scan already active, skipping\n");
		return 0;
	}

	arvif = ath12k_spectral_get_vdev(ar);
	if (!arvif) {
		ath12k_warn(ar->ab,
			    "spectral start_scan: no active vdev on pdev %u\n",
			    ar->pdev_idx);
		return -ENODEV;
	}

	/* Always seed home channel info from the vdev channel before scan
	 * starts so FFT samples have the correct operating channel BW and
	 * center frequency.  When the firmware supports SESSION_INFO, it
	 * sends updated agile scan params (sscan_cfreq1/2, sscan_bw) via
	 * WMI_PDEV_SSCAN_FW_PARAM_EVENTID after the FFT samples, so
	 * ch_width and oper_cfreq1 must be set here and not overwritten
	 * by that event.
	 */
	ath12k_spectral_update_chwidth(ar, arvif);

	/* For agile mode, perform a full DMA ring reset before sending
	 * wmi_cfg_setup.  After a normal scan, the SRNG HP pointer is at an
	 * arbitrary offset X.  When FW processes a new wmi_cfg_setup it resets
	 * its internal TP=0 but reads HP=X from the shadow register.  If X==0
	 * (HP wrapped) FW computes 0 available entries and produces no DMA
	 * output.  Resetting HP/TP shadow registers to 0 and refilling the
	 * ring ensures FW always sees a full ring after wmi_cfg_setup on N->A.
	 *
	 * For A->A rapid-fire: after CLEAR+DISABLE the FW agile engine takes
	 * some time to drain and return DMA buffers.  If we send wmi_cfg_setup
	 * while the engine is still active, FW silently ignores it and never
	 * sends sscan_fw_param, producing 0 samples.  Poll num_free (which reads
	 * FW's TP from shadow memory) until FW has returned a meaningful fraction
	 * of ring buffers, confirming the agile engine has stopped.
	 */
	if (ar->spectral.mode == SPECTRAL_SCAN_MODE_AGILE) {
		struct ath12k_dbring *dbr = &ar->spectral.rx_ring;
		struct hal_srng *srng =
			&ar->ab->hal.srng_list[dbr->refill_srng.ring_id];

		/* Drain poll for A->A rapid-fire: agile_drain_needed is set by
		 * configure_scan_params() when transitioning to agile from any
		 * prior scan mode. After CLEAR+DISABLE the FW agile engine takes
		 * time to return DMA buffers. Sending wmi_cfg_setup while it is
		 * still draining causes FW to silently ignore it (no sscan_fw_param
		 * event, zero samples). Poll until FW has returned enough buffers.
		 *
		 * Early-exit threshold: bufs_max -
		 * ATH12K_SPECTRAL_DBRING_FREE_SLOTS(bufs_max).
		 * FW permanently holds back
		 * ATH12K_SPECTRAL_DBRING_FREE_SLOTS(bufs_max) buffers
		 * and will never return more than
		 * bufs_max - ATH12K_SPECTRAL_DBRING_FREE_SLOTS(bufs_max).
		 */
		if (ar->spectral.agile_drain_needed) {
			unsigned long deadline = jiffies + msecs_to_jiffies(500);
			int num_free_now;

			spin_lock_bh(&srng->lock);
			num_free_now = ath12k_hal_srng_src_num_free(ar->ab, srng,
								    true);
			spin_unlock_bh(&srng->lock);

			/* Poll until FW has returned enough DMA buffers to confirm
			 * it has stopped consuming the ring.
			 */
			while (num_free_now <
			       dbr->bufs_max -
			       ATH12K_SPECTRAL_DBRING_FREE_SLOTS(dbr->bufs_max) &&
			       time_before(jiffies, deadline)) {
				usleep_range(5000, 10000);
				spin_lock_bh(&srng->lock);
				num_free_now = ath12k_hal_srng_src_num_free(ar->ab,
									    srng,
									    true);
				spin_unlock_bh(&srng->lock);
			}

			spin_lock_bh(&ar->spectral.lock);
			ar->spectral.agile_drain_needed = false;
			spin_unlock_bh(&ar->spectral.lock);
		}

		{
			int filled = 0;
			int saved_bufs_max = ar->spectral.rx_ring.bufs_max;
			bool hp_wrapped;

			spin_lock_bh(&srng->lock);
			hp_wrapped = (srng->u.src_ring.hp == 0);
			spin_unlock_bh(&srng->lock);

			if (hp_wrapped) {
				/* HP has wrapped to 0 — firmware would compute
				 * 0 available entries from the shadow register.
				 * Must reset HP/TP and refill the ring.
				 */
				ret = ath12k_dbring_reset(ar, &ar->spectral.rx_ring,
							  &filled);
				if (ret)
					return ret;
				ar->spectral.agile_ring_filled = filled;
				ar->spectral.rx_ring.bufs_max = filled;
				ret = ath12k_dbring_wmi_cfg_setup(ar,
								&ar->spectral.rx_ring,
								WMI_DIRECT_BUF_SPECTRAL);
				ar->spectral.rx_ring.bufs_max = saved_bufs_max;
			} else {
				/* HP is at a non-zero position — firmware already
				 * knows the ring and can compute available entries
				 * correctly. Skip dbring_reset to preserve the full
				 * ring (all IDR entries enqueued) so firmware gets
				 * maximum DMA slots and delivers all 4095 samples.
				 * Store current HP/entry_size as filled count for
				 * A->N topup.
				 */
				spin_lock_bh(&srng->lock);
				filled = srng->u.src_ring.hp / srng->entry_size;
				spin_unlock_bh(&srng->lock);
				ar->spectral.agile_ring_filled = filled;
				ret = ath12k_dbring_wmi_cfg_setup(ar,
								&ar->spectral.rx_ring,
								WMI_DIRECT_BUF_SPECTRAL);
			}
		}
		if (ret)
			return ret;
	}

	/* Enable the spectral engine in firmware before triggering capture. */
	ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
					      ATH12K_WMI_SPECTRAL_TRIGGER_CMD_IGNORE,
					      ATH12K_WMI_SPECTRAL_ENABLE_CMD_ENABLE);
	if (ret)
		return ret;

	/* Arm and start FFT capture. */
	ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
					      ATH12K_WMI_SPECTRAL_TRIGGER_CMD_TRIGGER,
					      ATH12K_WMI_SPECTRAL_ENABLE_CMD_IGNORE);
	if (ret)
		return ret;

	spin_lock_bh(&ar->spectral.lock);
	smode = ar->spectral.mode;

	if (smode >= SPECTRAL_SCAN_MODE_MAX) {
		spin_unlock_bh(&ar->spectral.lock);
		return 0;
	}

	ar->spectral.last_fft_timestamp[smode] = 0;
	ar->spectral.timestamp_war_offset[smode] = 0;
	ar->spectral.samples_done = 0;   /* reset per-scan counter */
	ar->spectral.timeout_received_count = 0;
	ar->spectral.timeout_prev_count = 0;
	ar->spectral.scan_active = true;
	/* Cache vdev_id so timeout_work can send CLEAR+DISABLE without
	 * needing wiphy_lock or walking ar->arvifs from workqueue context.
	 */
	ar->spectral.active_vdev_id = arvif->vdev_id;
	spin_unlock_bh(&ar->spectral.lock);

	/* Arm the host-side completion timer. If the FW fails to deliver
	 * scan_count FFT reports within this window we send a TIMEOUT event
	 * with the partial count.
	 */
	if (ar->spectral.params[ar->spectral.mode].completion_timeout_us > 0) {
		u64 timeout_ns =
		(u64)ar->spectral.params[ar->spectral.mode].completion_timeout_us *
				NSEC_PER_USEC;

		timeout_ns =
			(u64)ar->spectral.params[ar->spectral.mode].completion_timeout_us;
		timeout_ns *= NSEC_PER_USEC;
		hrtimer_start(&ar->spectral.scan_completion_timer,
			      ns_to_ktime(timeout_ns),
			      HRTIMER_MODE_REL_SOFT);
	}

	return 0;
}

int ath12k_spectral_stop_scan(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	enum spectral_scan_mode prev_mode;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* Already disabled; skip redundant CLEAR+DISABLE to firmware. */
	if (ar->spectral.mode >= SPECTRAL_SCAN_MODE_MAX) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral stop_scan: already disabled, skipping\n");
		return 0;
	}

	arvif = ath12k_spectral_get_vdev(ar);
	if (!arvif) {
		ath12k_warn(ar->ab,
			    "spectral stop_scan: no active vdev on pdev %u\n",
			    ar->pdev_idx);

		/* Clean up agile DMA-cached state. No WMI possible (no vdev).
		 * rollback_agile() acquires sp->lock internally; do not hold it
		 * here or the nested spin_lock_bh in rollback_agile() deadlocks.
		 */
		if (ar->spectral.mode == SPECTRAL_SCAN_MODE_AGILE)
			ath12k_spectral_rollback_agile(ar);

		return -ENODEV;
	}
	spin_lock_bh(&ar->spectral.lock);
	prev_mode = ar->spectral.mode;
	ar->spectral.scan_active = false;
	ar->spectral.last_fft_timestamp[SPECTRAL_SCAN_MODE_NORMAL] = 0;
	ar->spectral.timestamp_war_offset[SPECTRAL_SCAN_MODE_NORMAL] = 0;
	ar->spectral.samples_done = 0;
	/* Also clear firmware-cached agile detector state */
	if (prev_mode == SPECTRAL_SCAN_MODE_AGILE) {
		ar->spectral.sscan_cfreq1 = 0;
		ar->spectral.sscan_cfreq2 = 0;
		ar->spectral.sscan_bw = 0;
		ar->spectral.start_freq = 0;
		ar->spectral.end_freq = 0;
	}
	/* Do NOT set mode = INVALID yet — the CLEAR+DISABLE WMI command below
	 * derives scan_mode from ar->spectral.mode.  If we clear mode before
	 * sending the command, firmware receives scan_mode=NORMAL even when
	 * stopping an AGILE scan and silently ignores the stop, leaving the
	 * agile detector alive.  Set INVALID only after the WMI is sent.
	 */
	spin_unlock_bh(&ar->spectral.lock);

	/* Cancel after dropping the spectral lock — the hrtimer cb takes the
	 * same lock, so cancelling while holding it would deadlock.
	 */
	hrtimer_cancel(&ar->spectral.scan_completion_timer);

	ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
						ATH12K_WMI_SPECTRAL_TRIGGER_CMD_CLEAR,
					ATH12K_WMI_SPECTRAL_ENABLE_CMD_IGNORE);
	if (!ret)
		ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
						ATH12K_WMI_SPECTRAL_TRIGGER_CMD_IGNORE,
						ATH12K_WMI_SPECTRAL_ENABLE_CMD_DISABLE);
	/* Keep mode=prev_mode — do NOT reset to INVALID. */
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to disable spectral scan on vdev %d: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	/* After stopping an agile scan, send configure(cf1=0) to clear the
	 * agile detector frequency in firmware. Without this, firmware keeps
	 * the agile detector configured at the last scanned frequency, which
	 * interferes with subsequent normal and agile scans (A→N and A→A).
	 * This mirrors what the prop driver does in target_if_sops_stop_spectral_scan().
	 * scan_mode is still AGILE here (mode not yet reset to INVALID).
	 */
	if (prev_mode == SPECTRAL_SCAN_MODE_AGILE) {
		struct ath12k_wmi_vdev_spectral_conf_arg zero = { 0 };

		zero.vdev_id           = arvif->vdev_id;
		zero.scan_mode         = ATH12K_WMI_SPECTRAL_SCAN_MODE_AGILE;
		zero.scan_center_freq1 = 0;
		zero.scan_center_freq2 = 0;
		zero.scan_chan_width    = 0;
		ret = ath12k_wmi_vdev_spectral_conf(ar, &zero);
	}
	return ret;
}

void ath12k_spectral_rollback_agile(struct ath12k *ar)
{
	/* State-only cleanup. Called from both process and softirq contexts.
	 * The zero-cf configure WMI to clear firmware's agile detector state
	 * is sent from stop_scan() in process context instead.
	 */
	spin_lock_bh(&ar->spectral.lock);
	ar->spectral.scan_active = false;
	ar->spectral.sscan_cfreq1 = 0;
	ar->spectral.sscan_cfreq2 = 0;
	ar->spectral.sscan_bw = 0;
	ar->spectral.start_freq = 0;
	ar->spectral.end_freq = 0;
	spin_unlock_bh(&ar->spectral.lock);

}

int ath12k_spectral_configure_scan_params(struct ath12k *ar,
					  enum spectral_scan_mode mode)
{
	struct ath12k_wmi_vdev_spectral_conf_arg param = { 0 };
	struct ath12k_spectral_params *p = &ar->spectral.params[mode];
	struct ath12k_link_vif *arvif;
	enum spectral_scan_mode prev_mode;
	int ret = 0;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (mode >= SPECTRAL_SCAN_MODE_MAX)
		return -EINVAL;

	arvif = ath12k_spectral_get_vdev(ar);
	if (!arvif) {
		ath12k_warn(ar->ab,
			    "spectral configure_scan_params: no active vdev on pdev %u\n",
			    ar->pdev_idx);
		if (mode == SPECTRAL_SCAN_MODE_AGILE)
			ath12k_spectral_rollback_agile(ar);
		return -ENODEV;
	}
	spin_lock_bh(&ar->spectral.lock);
	prev_mode = ar->spectral.mode;
	ar->spectral.mode = mode;
	ar->spectral.scan_active = false;
	spin_unlock_bh(&ar->spectral.lock);

	hrtimer_cancel(&ar->spectral.scan_completion_timer);
	cancel_work_sync(&ar->spectral.scan_timeout_work);
	cancel_work_sync(&ar->spectral.agile_dma_rebind_work);

	ar->spectral.samples_done = 0;

	if (prev_mode != SPECTRAL_SCAN_MODE_INVALID) {
		spin_lock_bh(&ar->spectral.lock);
		ar->spectral.mode = prev_mode;
		spin_unlock_bh(&ar->spectral.lock);
		ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
						ATH12K_WMI_SPECTRAL_TRIGGER_CMD_CLEAR,
						ATH12K_WMI_SPECTRAL_ENABLE_CMD_IGNORE);
		if (!ret)
			ret = ath12k_wmi_vdev_spectral_enable(ar, arvif->vdev_id,
						ATH12K_WMI_SPECTRAL_TRIGGER_CMD_IGNORE,
						ATH12K_WMI_SPECTRAL_ENABLE_CMD_DISABLE);
		/* When transitioning from NORMAL to AGILE, FW's normal-mode
		 * detector does not cleanly release after CLEAR+DISABLE alone.
		 * Send a zero-count wmi_spectral_conf(normal) as an explicit
		 * teardown command so FW transitions its normal engine to idle
		 * before we send the agile config.  Without this, FW ignores
		 * the subsequent wmi_spectral_conf(agile) and never fires
		 * sscan_fw_param (0 samples).
		 */
		if (!ret && mode == SPECTRAL_SCAN_MODE_AGILE &&
		    prev_mode == SPECTRAL_SCAN_MODE_NORMAL) {
			struct ath12k_wmi_vdev_spectral_conf_arg zero = { 0 };

			zero.vdev_id    = arvif->vdev_id;
			zero.scan_count = 0;
			zero.scan_mode  = ATH12K_WMI_SPECTRAL_SCAN_MODE_NORMAL;
			ret = ath12k_wmi_vdev_spectral_conf(ar, &zero);
			ret = 0; /* non-fatal: proceed to agile conf regardless */
		}

		/* Give FW time to drain the previous spectral engine before
		 * sending the new wmi_spectral_conf(agile).
		 */
		if (!ret && mode == SPECTRAL_SCAN_MODE_AGILE)
			msleep(50);
	}

	/* Set the new mode */
	spin_lock_bh(&ar->spectral.lock);
	ar->spectral.mode = mode;
	/* If we just sent CLEAR+DISABLE for any running scan and the next mode
	 * is AGILE, mark that start_scan needs to wait for FW to drain before
	 * sending wmi_cfg_setup.  This covers both A->A and N->A: after
	 * CLEAR+DISABLE the FW engine (normal or agile) takes time to drain its
	 * DMA pipeline; sending wmi_cfg_setup too soon causes FW to silently
	 * reject it (no sscan_fw_param, zero samples).
	 */
	if (prev_mode != SPECTRAL_SCAN_MODE_INVALID && mode == SPECTRAL_SCAN_MODE_AGILE)
		ar->spectral.agile_drain_needed = true;
	spin_unlock_bh(&ar->spectral.lock);

	/* Agile->Normal transition: ensure normal scan gets a full ring.
	 * When dbring_reset() ran (HP wrapped), FREE_SLOTS were left empty.
	 * When dbring_reset() was skipped (HP non-zero), agile_ring_filled
	 * records the current HP position so only genuinely empty slots
	 * are filled. Either way, normal scan gets a full ring.
	 */
	if (prev_mode == SPECTRAL_SCAN_MODE_AGILE &&
	    mode == SPECTRAL_SCAN_MODE_NORMAL)
		ath12k_dbring_topup(ar, &ar->spectral.rx_ring,
				    ar->spectral.agile_ring_filled);
	if (ret) {
		ath12k_warn(ar->ab, "failed to clear spectral scan config: %d\n",
			    ret);
		if (mode == SPECTRAL_SCAN_MODE_AGILE) {
			ath12k_spectral_rollback_agile(ar);
			spin_lock_bh(&ar->spectral.lock);
			ar->spectral.mode = prev_mode;
			spin_unlock_bh(&ar->spectral.lock);
		} else {
			spin_lock_bh(&ar->spectral.lock);
			ar->spectral.mode = prev_mode;
			spin_unlock_bh(&ar->spectral.lock);
		}
		return ret;
	}

	param.vdev_id              = arvif->vdev_id;
	param.scan_count           = max_t(u32, 0, p->scan_count);
	param.scan_period          = p->scan_period;
	param.scan_priority        = p->scan_priority;
	param.scan_fft_size        = p->scan_fft_size;
	param.scan_gc_ena          = p->scan_gc_ena;
	param.scan_restart_ena     = p->scan_restart_ena;
	param.scan_noise_floor_ref = p->scan_noise_floor_ref;
	param.scan_init_delay      = p->scan_init_delay;
	param.scan_nb_tone_thr     = p->scan_nb_tone_thr;
	param.scan_str_bin_thr     = p->scan_str_bin_thr;
	param.scan_wb_rpt_mode     = p->scan_wb_rpt_mode;
	param.scan_rssi_rpt_mode   = p->scan_rssi_rpt_mode;
	param.scan_rssi_thr        = p->scan_rssi_thr;
	param.scan_pwr_format      = p->scan_pwr_format;
	param.scan_rpt_mode        = p->scan_rpt_mode;
	param.scan_bin_scale       = p->scan_bin_scale;
	param.scan_dbm_adj         = p->scan_dbm_adj;
	param.scan_chn_mask        = p->scan_chn_mask;

	/* Mode and agile-specific parameters.
	 * For AGILE mode: tell firmware to scan at the configured center
	 * frequency and bandwidth, independent of the operating channel.
	 * For NORMAL mode: zero these fields so firmware uses the operating
	 * channel (FW ignores them when scan_mode == NORMAL).
	 */
	if (mode == SPECTRAL_SCAN_MODE_AGILE) {
		int wmi_bw;
		param.scan_mode         = ATH12K_WMI_SPECTRAL_SCAN_MODE_AGILE;
		param.scan_center_freq1 = p->frequency;
		param.scan_center_freq2 = p->frequency2;
		wmi_bw = ath12k_spectral_nl80211_bw_to_wmi_chwidth(p->bandwidth);
		if (wmi_bw < 0) {
			ath12k_warn(ar->ab,
				    "spectral: unsupported agile BW %u\n",
				    p->bandwidth);
			return -EINVAL;
		}
		param.scan_chan_width = wmi_bw;
		/* scan_chan_freq: always 0 for agile mode — firmware does not
		 * use this field for agile and silently drops DMA if it is set.
		 */
	} else {
		param.scan_mode         = ATH12K_WMI_SPECTRAL_SCAN_MODE_NORMAL;
		param.scan_center_freq1 = 0;
		param.scan_center_freq2 = 0;
		param.scan_chan_width    = 0;
	}

	ret = ath12k_wmi_vdev_spectral_conf(ar, &param);
	if (ret) {
		ath12k_warn(ar->ab, "failed to configure spectral scan: %d\n",
			    ret);
		if (mode == SPECTRAL_SCAN_MODE_AGILE)
			ath12k_spectral_rollback_agile(ar);
		return ret;
	}

	return 0;
}

static int ath12k_spectral_pull_summary(struct ath12k *ar,
					struct ath12k_wmi_dma_buf_release_meta_data_params *meta,
					struct spectral_summary_fft_report *summary,
					struct ath12k_spectral_summary_report *report)
{
	struct spectral_summary_report_padding *padding;
	u32 tlv_header;
	u32 summary_pad_sz;
	u8 tag;
	u8 sign;

	if (!meta || !summary || !report)
		return -EINVAL;

	tlv_header = __le32_to_cpu(summary->tlv_header);
	sign = FIELD_GET(SPECTRAL_TLV_HDR_SIGN, tlv_header);
	tag = FIELD_GET(SPECTRAL_TLV_HDR_TAG, tlv_header);
	if (sign != ATH12K_SPECTRAL_SIGNATURE ||
	    tag != ATH12K_SPECTRAL_TAG_SCAN_SUMMARY)
		return -EINVAL;

	report->timestamp = __le32_to_cpu(summary->timestamp);
	report->agc_total_gain = FIELD_GET(SPECTRAL_SUMMARY_INFO0_AGC_TOTAL_GAIN,
					   __le32_to_cpu(summary->info0));
	report->out_of_band_flag = FIELD_GET(SPECTRAL_SUMMARY_INFO0_OB_FLAG,
					     __le32_to_cpu(summary->info0));
	report->grp_idx = FIELD_GET(SPECTRAL_SUMMARY_INFO0_GRP_IDX,
				    __le32_to_cpu(summary->info0));
	report->rf_saturation = FIELD_GET(SPECTRAL_SUMMARY_INFO0_RECENT_RFSAT,
					  __le32_to_cpu(summary->info0));
	report->inb_pwr_db = FIELD_GET(SPECTRAL_SUMMARY_INFO0_INBAND_PWR_DB,
				       __le32_to_cpu(summary->info0));
	report->false_scan = FIELD_GET(SPECTRAL_SUMMARY_INFO0_FALSE_SCAN,
				       __le32_to_cpu(summary->info0));
	report->detector_id = FIELD_GET(SPECTRAL_SUMMARY_INFO0_DETECTOR_ID,
					__le32_to_cpu(summary->info0));
	if (report->detector_id >= ATH12K_SPECTRAL_NUM_DETECTORS) {
		ath12k_warn(ar->ab, "invalid detector id %u\n",
			    report->detector_id);
		return -EINVAL;
	}
	report->primary80 = FIELD_GET(SPECTRAL_SUMMARY_INFO0_PRI80,
				      __le32_to_cpu(summary->info0));
	report->peak_idx = FIELD_GET(SPECTRAL_SUMMARY_INFO2_PEAK_SIGNED_IDX,
				     __le32_to_cpu(summary->info2));
	report->peak_mag = FIELD_GET(SPECTRAL_SUMMARY_INFO2_PEAK_MAGNITUDE,
				     __le32_to_cpu(summary->info2));
	report->gain_change = FIELD_GET(SPECTRAL_SUMMARY_INFO3_GAIN_CHANGE,
					__le32_to_cpu(summary->reserve1));

	report->blanking_status = 0;
	summary_pad_sz = ar->ab->hw_params->spectral.summary_pad_sz;
	if (summary_pad_sz >= sizeof(*padding) &&
	    ath12k_scan_radio_blanking_supported(ar->pdev)) {
		padding = (struct spectral_summary_report_padding *)
			((u8 *)summary + sizeof(*summary));
		if (__le32_to_cpu(padding->hdr_a) ==
		    ATH12K_SPECTRAL_SUMMARY_PAD_BLANKING_TAG)
			report->blanking_status = 1;
	}

	memcpy(&report->meta, meta, sizeof(*meta));

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral summary: ts=%u agc_gain=%u ob_flag=%u grp_idx=%u rf_sat=%u inb_pwr_db=%u false_scan=%u det_id=%u pri80=%u peak_idx=%d peak_mag=%u gain_chg=%u blanking=%u\n",
		   report->timestamp, report->agc_total_gain,
		   report->out_of_band_flag, report->grp_idx,
		   report->rf_saturation, report->inb_pwr_db,
		   report->false_scan, report->detector_id,
		   report->primary80, report->peak_idx,
		   report->peak_mag, report->gain_change,
		   report->blanking_status);

	return 0;
}

static int ath12k_spectral_pull_search(struct ath12k *ar,
				       struct spectral_search_fft_report *search,
				       struct ath12k_spectral_search_report *report)
{
	u32 peak_idx = 0, peak_mag = 0;

	report->timestamp = __le32_to_cpu(search->timestamp);
	report->detector_id = FIELD_GET(SPECTRAL_FFT_REPORT_INFO0_DETECTOR_ID,
					__le32_to_cpu(search->info0));
	report->fft_count = FIELD_GET(SPECTRAL_FFT_REPORT_INFO0_FFT_NUM,
				      __le32_to_cpu(search->info0));
	report->radar_check = FIELD_GET(SPECTRAL_FFT_REPORT_INFO0_RADAR_CHECK,
					__le32_to_cpu(search->info0));
	peak_idx = FIELD_GET(SPECTRAL_FFT_REPORT_INFO0_PEAK_SIGNED_IDX,
			     __le32_to_cpu(search->info0));
	report->peak_idx = ath12k_spectral_signed_val(peak_idx,
						      ATH12K_SPECTRAL_FFT_PEAK_IDX_WIDTH);
	report->chain_idx = FIELD_GET(SPECTRAL_FFT_REPORT_INFO1_CHAIN_IDX,
				      __le32_to_cpu(search->info1));
	report->base_pwr_db = FIELD_GET(SPECTRAL_FFT_REPORT_INFO1_BASE_PWR_DB,
					__le32_to_cpu(search->info1));
	report->total_gain_db = FIELD_GET(SPECTRAL_FFT_REPORT_INFO1_TOTAL_GAIN_DB,
					  __le32_to_cpu(search->info1));
	report->strong_bin_count = FIELD_GET(SPECTRAL_FFT_REPORT_INFO2_NUM_STRONG_BINS,
					     __le32_to_cpu(search->info2));
	peak_mag = FIELD_GET(SPECTRAL_FFT_REPORT_INFO2_PEAK_MAGNITUDE,
			     __le32_to_cpu(search->info2));
	report->peak_mag = ath12k_spectral_signed_val(peak_mag,
						      ATH12K_SPECTRAL_FFT_PEAK_MAG_WIDTH);
	report->avg_pwr_db = FIELD_GET(SPECTRAL_FFT_REPORT_INFO2_AVG_PWR_DB,
				       __le32_to_cpu(search->info2));
	report->rel_pwr_db = FIELD_GET(SPECTRAL_FFT_REPORT_INFO2_REL_PWR_DB,
				       __le32_to_cpu(search->info2));

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral fft report: ts=%u det_id=%u fft_num=%u radar_check=%u peak_sidx=%d chn_idx=%u base_pwr_db=%u total_gain_db=%u num_str_bins=%u peak_mag=%d avgpwr_db=%u relpwr_db=%u\n",
		   report->timestamp, report->detector_id, report->fft_count,
		   report->radar_check, report->peak_idx, report->chain_idx,
		   report->base_pwr_db, report->total_gain_db,
		   report->strong_bin_count, report->peak_mag,
		   report->avg_pwr_db, report->rel_pwr_db);

	return 0;
}

static u8 ath12k_spectral_clamp_fft_bin_value(u16 fft_bin_value, u32 pwr_format)
{
	switch (pwr_format) {
	case ATH12K_SPECTRAL_PWR_FORMAT_LINEAR:
		if (fft_bin_value > U8_MAX)
			return U8_MAX;

		return fft_bin_value;
	case ATH12K_SPECTRAL_PWR_FORMAT_DBM:
		if ((s8)fft_bin_value > S8_MAX)
			return S8_MAX;

		if ((s8)fft_bin_value < S8_MIN)
			return S8_MIN;

		return fft_bin_value;
	default:
		return 0;
	}
}

static void ath12k_spectral_parse_fft(u8 *outbins, u8 *inbins, int num_bins,
				      u8 fft_sz, u32 pwr_format)
{
	int i, j;
	u32 fft_bin_val;

	i = 0;
	j = 0;
	while (i < num_bins) {
		switch (fft_sz) {
		case sizeof(u8):
			fft_bin_val = inbins[j];
			break;
		case sizeof(__le16):
			fft_bin_val = get_unaligned_le16(&inbins[j]);
			break;
		case sizeof(__le32):
			fft_bin_val = get_unaligned_le32(&inbins[j]);
			break;
		default:
			fft_bin_val = inbins[j];
			break;
		}

		outbins[i] = ath12k_spectral_clamp_fft_bin_value(fft_bin_val,
								 pwr_format);
		i++;
		j += fft_sz;
	}
}

static u8 ath12k_spectral_chwidth_to_nl(u32 ch_width)
{
	switch (ch_width) {
	case WMI_PEER_CHWIDTH_20MHZ:
		return NL80211_CHAN_WIDTH_20;
	case WMI_PEER_CHWIDTH_40MHZ:
		return NL80211_CHAN_WIDTH_40;
	case WMI_PEER_CHWIDTH_80MHZ:
		return NL80211_CHAN_WIDTH_80;
	case WMI_PEER_CHWIDTH_160MHZ:
		return NL80211_CHAN_WIDTH_160;
	case WMI_PEER_CHWIDTH_MAX:
	case WMI_PEER_CHWIDTH_320MHZ:
		return NL80211_CHAN_WIDTH_320;
	default:
		return NL80211_CHAN_WIDTH_20;
	}
}

static u32 ath12k_spectral_chwidth_to_mhz(u32 ch_width)
{
	switch (ch_width) {
	case WMI_PEER_CHWIDTH_20MHZ:
	default:
		return ATH12K_SPECTRAL_20MHZ;
	case WMI_PEER_CHWIDTH_40MHZ:
		return ATH12K_SPECTRAL_40MHZ;
	case WMI_PEER_CHWIDTH_80MHZ:
		return ATH12K_SPECTRAL_80MHZ;
	case WMI_PEER_CHWIDTH_160MHZ:
		return ATH12K_SPECTRAL_160MHZ;
	case WMI_PEER_CHWIDTH_320MHZ:
	case WMI_PEER_CHWIDTH_MAX:
		return ATH12K_SPECTRAL_320MHZ;
	}
}

static int ath12k_spectral_get_lowest_chn_idx(struct ath12k *ar,
					      u8 *chn_idx_lowest_enabled)
{
	u8 rx_chainmask;

	if (!chn_idx_lowest_enabled)
		return -EINVAL;

	rx_chainmask = ar->cfg_rx_chainmask;
	if (!rx_chainmask) {
		ath12k_warn(ar->ab, "invalid rx chainmask 0x%x\n", rx_chainmask);
		return -EINVAL;
	}

	*chn_idx_lowest_enabled = __ffs(rx_chainmask);
	if (*chn_idx_lowest_enabled >= WMI_MAX_CHAINS) {
		ath12k_warn(ar->ab, "invalid chain index %u for rx chainmask 0x%x\n",
			    *chn_idx_lowest_enabled, rx_chainmask);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_spectral_fill_fft_sample(struct ath12k *ar,
					   struct ath12k_spectral_summary_report *summary,
					   struct spectral_search_fft_report *fft_report,
					   const struct ath12k_spectral_search_report
					   *search,
					   struct fft_sample_ath12k *fft_sample,
					   int num_bins,
					   bool fragment_sample)
{
	struct ath12k_base *ab = ar->ab;
	u16 length, end_bin;
	u32 center_freq, half_bw, start_freq, end_freq;
	u32 cfreq1, cfreq2;
	u32 meta_freq1, meta_freq2;
	u8 chn_idx_lowest_enabled;
	int ret;

	length = sizeof(*fft_sample) - sizeof(struct fft_sample_tlv) + num_bins;
	fft_sample->tlv.type = ATH_FFT_SAMPLE_ATH12K;
	fft_sample->tlv.length = __cpu_to_be16(length);

	meta_freq1 = __le32_to_cpu(summary->meta.freq1);
	meta_freq2 = __le32_to_cpu(summary->meta.freq2);

	/*
	 * cfreq1/cfreq2: home operating channel center frequencies.
	 * Always use the meta frequencies from the summary report —
	 * these reflect the primary radio's operating channel, not the
	 * agile scan span.
	 *
	 * sscan_cfreq1/2: agile scan center frequencies.
	 * Use the cached agile params if set (agile mode), otherwise
	 * fall back to meta_freq (normal mode).
	 */
	cfreq1 = ar->spectral.sscan_cfreq1 ? ar->spectral.sscan_cfreq1 : meta_freq1;
	cfreq2 = ar->spectral.sscan_cfreq2 ? ar->spectral.sscan_cfreq2 : meta_freq2;

	ret = ath12k_spectral_get_lowest_chn_idx(ar, &chn_idx_lowest_enabled);
	if (ret)
		return ret;

	fft_sample->signature = __cpu_to_be32(ATH12K_SPECTRAL_SIGNATURE);
	fft_sample->pri_freq = __cpu_to_be32(ar->spectral.pri20_freq);
	fft_sample->target_reset_count = __cpu_to_be32(ar->spectral.target_reset_count);
	/* Home channel center frequency - use the value saved at scan start.
	 * cfreq2 is not changed as it is only relevant for 80+80 mode
	 * which is not supported on currently.
	 */
	fft_sample->cfreq1 = __cpu_to_be32(ar->spectral.oper_cfreq1);
	fft_sample->cfreq2 = __cpu_to_be32(meta_freq2);
	/* agile scan center frequencies */
	fft_sample->sscan_cfreq1 = __cpu_to_be32(cfreq1);
	fft_sample->sscan_cfreq2 = __cpu_to_be32(cfreq2);
	fft_sample->bin_pwr_count = __cpu_to_be32(num_bins);

	center_freq = cfreq1;
	/* Use configured scan BW for agile, home channel BW for normal.
	 * firmware sends wrong BW in meta.ch_width for agile scans.
	 */
	if (ar->spectral.params[ar->spectral.mode].bandwidth) {
		int nl_bw = ar->spectral.params[ar->spectral.mode].bandwidth;
		int wmi_bw = ath12k_spectral_nl80211_bw_to_wmi_chwidth(nl_bw);

		half_bw = wmi_bw >= 0 ?
			  ath12k_spectral_chwidth_to_mhz(wmi_bw) / 2 :
			  ath12k_spectral_chwidth_to_mhz(ar->spectral.ch_width) / 2;
	} else {
		half_bw = ath12k_spectral_chwidth_to_mhz(ar->spectral.ch_width) / 2;
	}
	start_freq = center_freq - half_bw;
	end_freq = center_freq + half_bw;

	end_bin = num_bins ? num_bins - 1 : 0;
	fft_sample->detector_info.start_frequency = __cpu_to_be32(start_freq);
	fft_sample->detector_info.end_frequency = __cpu_to_be32(end_freq);
	fft_sample->detector_info.timestamp =
		__cpu_to_be32(search->adjusted_timestamp);
	fft_sample->detector_info.last_tstamp =
		__cpu_to_be32(search->last_timestamp);
	fft_sample->detector_info.last_raw_timestamp =
		__cpu_to_be32(search->last_raw_timestamp);
	fft_sample->detector_info.timestamp_war_offset =
		__cpu_to_be32(search->timestamp_war_offset);
	fft_sample->detector_info.raw_timestamp =
		__cpu_to_be32(search->timestamp);
	fft_sample->detector_info.reset_delay = __cpu_to_be32(summary->meta.reset_delay);
	fft_sample->detector_info.start_bin_idx = __cpu_to_be16(0);
	fft_sample->detector_info.end_bin_idx = __cpu_to_be16(end_bin);
	fft_sample->detector_info.max_index =
		__cpu_to_be16((s16)search->peak_idx);
	fft_sample->detector_info.max_magnitude =
		__cpu_to_be16(search->peak_mag);
	fft_sample->detector_info.noise_floor =
		__cpu_to_be16((s16)summary->meta.noise_floor[chn_idx_lowest_enabled]);

	summary->inb_pwr_db >>= 1;
	fft_sample->detector_info.rssi = (u8)summary->inb_pwr_db;
	fft_sample->detector_info.agc_total_gain = summary->agc_total_gain;
	fft_sample->detector_info.gainchange = summary->gain_change;
	fft_sample->detector_info.pri80ind = summary->primary80;
	fft_sample->detector_info.blanking_status = summary->blanking_status;
	fft_sample->spectral_mode =
		ath12k_spectral_get_scan_mode_from_detector(ar, search->detector_id);
	/* Home channel BW saved at scan start from the vdev channel */
	fft_sample->operating_bw =
		ath12k_spectral_chwidth_to_nl(ar->spectral.ch_width);
	/* Scan BW - use the configured value for agile, home channel BW for normal */
	fft_sample->sscan_bw =
		ar->spectral.params[ar->spectral.mode].bandwidth ?
		ar->spectral.params[ar->spectral.mode].bandwidth :
		ath12k_spectral_chwidth_to_nl(ar->spectral.ch_width);
	fft_sample->fft_width = ATH12K_SPECTRAL_BIN_SIZE;
	memcpy(fft_sample->macaddr, ar->mac_addr, sizeof(fft_sample->macaddr));
	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL, "spectral fft: ar->mac_addr=%pM\n",
		   ar->mac_addr);

	ath12k_spectral_parse_fft(fft_sample->data, fft_report->bins, num_bins,
				  ab->hw_params->spectral.fft_bin_sz,
				  ar->spectral.params[ar->spectral.mode].scan_pwr_format);

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
		   "spectral fft bins: num_bins=%d fft_size=%u\n", num_bins,
		   ar->spectral.params[ar->spectral.mode].scan_fft_size);
	if (ath12k_debug_mask & ATH12K_DBG_SPECTRAL)
		print_hex_dump(KERN_DEBUG, "fft bins: ", DUMP_PREFIX_OFFSET, 16, 1,
			       fft_sample->data, num_bins, false);

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
		   "fft_sample: pri_freq=%u cfreq1=%u cfreq2=%u sscan_cfreq1=%u sscan_cfreq2=%u bins=%u mode=%u op_bw=%u sscan_bw=%u start_freq=%u end_freq=%u rssi=%u nf=%d macaddr=%pM\n",
		   be32_to_cpu(fft_sample->pri_freq),
		   be32_to_cpu(fft_sample->cfreq1),
		   be32_to_cpu(fft_sample->cfreq2),
		   be32_to_cpu(fft_sample->sscan_cfreq1),
		   be32_to_cpu(fft_sample->sscan_cfreq2),
		   be32_to_cpu(fft_sample->bin_pwr_count),
		   fft_sample->spectral_mode,
		   fft_sample->operating_bw,
		   fft_sample->sscan_bw,
		   be32_to_cpu(fft_sample->detector_info.start_frequency),
		   be32_to_cpu(fft_sample->detector_info.end_frequency),
		   fft_sample->detector_info.rssi,
		   (s16)be16_to_cpu(fft_sample->detector_info.noise_floor),
		   fft_sample->macaddr);

	/* This path is reached for active FFT reports; count every relay attempt. */
	if (ar->spectral.rfs_scan) {
		unsigned long flags;
		size_t sample_len;

		sample_len = sizeof(*fft_sample) +
			     ATH12K_SPECTRAL_ATH12K_MAX_IB_BINS(ar->ab);
		spin_lock_irqsave(&ar->spectral.relay_lock, flags);
		if (unlikely(atomic_read(&ar->spectral.relay_resetting))) {
			spin_unlock_irqrestore(&ar->spectral.relay_lock, flags);
			return -EBUSY;
		}
		atomic64_inc(&ar->spectral.relay_write_attempts);
		if (ath12k_spectral_relay_write_record(ar->spectral.rfs_scan,
						       fft_sample, sample_len))
			atomic64_inc(&ar->spectral.relay_write_accepted);
		else
			atomic64_inc(&ar->spectral.relay_write_rejected);
		ath12k_spectral_relay_update_peak(&ar->spectral);
		spin_unlock_irqrestore(&ar->spectral.relay_lock, flags);
	}

	return 0;
}

static
int ath12k_spectral_process_fft(struct ath12k *ar,
				struct ath12k_spectral_summary_report *summary,
				void *data,
				struct fft_sample_ath12k *fft_sample,
				u32 data_len)
{
	struct ath12k_base *ab = ar->ab;
	struct spectral_search_fft_report *fft_report = data;
	struct ath12k_spectral_search_report search;
	struct spectral_tlv *tlv;
	int tlv_len, bin_len, num_bins;
	size_t fft_bin_size;
	size_t total_bins;
	u8 chan_width;
	int ret, i;
	u32 check_length;
	bool fragment_sample = false;
	struct wmi_spectral_capabilities_event spectral_cap;
	u32 supported_flags;
	u8 sign, tag;
	enum spectral_scan_mode smode;

	spectral_cap = ar->spectral.spectral_cap;

	lockdep_assert_held(&ar->spectral.lock);

	if (!ab->hw_params->spectral.fft_sz) {
		ath12k_warn(ab, "invalid bin size type for hw rev %d\n",
			    ab->hw_rev);
		return -EINVAL;
	}

	tlv = (struct spectral_tlv *)data;
	tlv_len = FIELD_GET(SPECTRAL_TLV_HDR_LEN, __le32_to_cpu(tlv->header));
	/* convert Dword into bytes */
	tlv_len *= ATH12K_SPECTRAL_DWORD_SIZE;
	sign = FIELD_GET(SPECTRAL_TLV_HDR_SIGN, __le32_to_cpu(tlv->header));
	tag = FIELD_GET(SPECTRAL_TLV_HDR_TAG, __le32_to_cpu(tlv->header));
	if (sign != ATH12K_SPECTRAL_SIGNATURE ||
	    tag != ATH12K_SPECTRAL_TAG_SCAN_SEARCH) {
		ath12k_warn(ab, "invalid fft tlv: sign=0x%x tag=0x%x\n",
			    sign, tag);
		return -EINVAL;
	}

	if (tlv_len < ab->hw_params->spectral.fft_hdr_len) {
		ath12k_warn(ab, "invalid fft header length %d\n", tlv_len);
		return -EINVAL;
	}

	bin_len = tlv_len - ab->hw_params->spectral.fft_hdr_len;

	if (data_len < (bin_len + sizeof(*fft_report))) {
		ath12k_warn(ab, "mismatch in expected bin len %d and data len %d\n",
			    bin_len, data_len);
		return -EINVAL;
	}

	total_bins = bin_len / ab->hw_params->spectral.fft_bin_sz;
	num_bins = ath12k_spectral_get_bin_count_after_len_adj(
			ar, bin_len, ar->spectral.params[ar->spectral.mode].scan_rpt_mode,
			&fft_bin_size);

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
		   "spectral fft hdr: fft_timestamp=0x%x hdr_len=%d(dwords) tag=0x%x sig=0x%x payload_len=%d total_len=%zu bin_len=%d bin_sz=%zu num_bins=%d\n",
		   __le32_to_cpu(fft_report->timestamp),
		   tlv_len / ATH12K_SPECTRAL_DWORD_SIZE,
		   (u32)FIELD_GET(SPECTRAL_TLV_HDR_TAG, __le32_to_cpu(tlv->header)),
		   (u32)FIELD_GET(SPECTRAL_TLV_HDR_SIGN, __le32_to_cpu(tlv->header)),
		   tlv_len, tlv_len + sizeof(*tlv),
		   bin_len, fft_bin_size, num_bins);

	if (ar->spectral.params[ar->spectral.mode].scan_rpt_mode !=
	    ATH12K_SPECTRAL_RPT_MODE_1 &&
	    (num_bins < ATH12K_SPECTRAL_ATH12K_MIN_IB_BINS ||
	     num_bins > ATH12K_SPECTRAL_ATH12K_MAX_IB_BINS(ab) ||
	     !is_power_of_2(num_bins))) {
		ath12k_warn(ab, "Invalid num of bins %d\n", num_bins);
		return -EINVAL;
	}

	check_length = sizeof(*fft_report);
	ret = ath12k_dbring_validate_buffer(ar, data, check_length);
	if (ret) {
		ath12k_warn(ar->ab, "found magic value in fft data, dropping\n");

		if (ath12k_debug_mask & ATH12K_DBG_SPECTRAL)
			print_hex_dump(KERN_DEBUG, "spectral fft dump: ",
				       DUMP_PREFIX_OFFSET, 16, 1, data,
				       min_t(u32, data_len, sizeof(*tlv) + tlv_len),
				       false);
		return ret;
	}

	ret = ath12k_spectral_pull_search(ar, data, &search);
	if (ret) {
		ath12k_warn(ab, "failed to pull search report %d\n", ret);
		return ret;
	}

	if (search.detector_id != summary->detector_id) {
		ath12k_warn(ab, "detector mismatch: summary=%u fft=%u\n",
			    summary->detector_id, search.detector_id);
		return -EINVAL;
	}

	if (search.detector_id >= ATH12K_SPECTRAL_NUM_DETECTORS) {
		ath12k_warn(ab, "invalid detector id %u\n",
			    search.detector_id);
		return -EINVAL;
	}

	search.last_raw_timestamp = 0;
	search.adjusted_timestamp = 0;
	search.last_timestamp = 0;
	search.timestamp_war_offset = 0;

	if (ar->spectral.mode < SPECTRAL_SCAN_MODE_MAX &&
	    ath12k_spectral_get_detector_from_scan_mode(ar, ar->spectral.mode) !=
							search.detector_id) {
		ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
			   "spectral detector mismatch: mode=%u detector=%u\n",
			   ar->spectral.mode, search.detector_id);
	}

	smode = ath12k_spectral_get_scan_mode_from_detector(ar, search.detector_id);
	if (smode >= SPECTRAL_SCAN_MODE_MAX) {
		ath12k_warn(ab, "invalid spectral mode %u\n", smode);
		return -EINVAL;
	}	ret = ath12k_spectral_get_adjusted_timestamp(ar, search.timestamp,
						     summary->meta.reset_delay, smode,
						     &search.adjusted_timestamp,
						     &search.last_raw_timestamp,
						     &search.last_timestamp,
						     &search.timestamp_war_offset);
	if (ret) {
		ath12k_warn(ab, "failed to adjust timestamp %d\n", ret);
		return ret;
	}

	ath12k_spectral_verify_ts(ar, data, search.adjusted_timestamp,
				  search.detector_id);

	chan_width = ar->spectral.ch_width;

	/* Use per-mode params so normal and agile fft_size are independent */
	struct ath12k_spectral_params *p = &ar->spectral.params[smode];

	switch (chan_width) {
	case WMI_PEER_CHWIDTH_20MHZ:
		for (i = 0; i < spectral_cap.num_fft_size_caps_entry; i++) {
			if (spectral_cap.fft_size_caps->sscan_bw == chan_width) {
				supported_flags = spectral_cap.fft_size_caps->supported_flags;
				supported_flags >>= 6;
				if (!(supported_flags & 1)) {
					ath12k_warn(ab,
						    "Spectral fft size %d not supported",
						    p->scan_fft_size);
					return -EINVAL;
				}
				break;
			}
			spectral_cap.fft_size_caps++;
		}
		summary->meta.ch_width = chan_width;
		break;
	case WMI_PEER_CHWIDTH_40MHZ:
		for (i = 0; i < spectral_cap.num_fft_size_caps_entry; i++) {
			if (spectral_cap.fft_size_caps->sscan_bw == chan_width) {
				supported_flags = spectral_cap.fft_size_caps->supported_flags;
				supported_flags >>= 7;
				if (!(supported_flags & 1)) {
					ath12k_warn(ab,
						    "Spectral fft size %d not supported",
						    p->scan_fft_size);
					return -EINVAL;
				} else {
					num_bins <<= 1;
					p->scan_fft_size = 8;
				}
				break;
			}
			spectral_cap.fft_size_caps++;
		}
		summary->meta.ch_width = chan_width;
		break;
	case WMI_PEER_CHWIDTH_80MHZ:
		for (i = 0; i < spectral_cap.num_fft_size_caps_entry; i++) {
			if (spectral_cap.fft_size_caps->sscan_bw == chan_width) {
				supported_flags = spectral_cap.fft_size_caps->supported_flags;
				supported_flags >>= 8;
				if (!(supported_flags & 1)) {
					ath12k_warn(ab,
						    "Spectral fft size %d not supported",
						    p->scan_fft_size);
					return -EINVAL;
				} else {
					num_bins <<= 2;
					p->scan_fft_size = 9;
				}
				break;
			}
			spectral_cap.fft_size_caps++;
		}
		summary->meta.ch_width = chan_width;
		break;
	case WMI_PEER_CHWIDTH_160MHZ:
		for (i = 0; i < spectral_cap.num_fft_size_caps_entry; i++) {
			if (spectral_cap.fft_size_caps->sscan_bw == chan_width) {
				supported_flags = spectral_cap.fft_size_caps->supported_flags;
				supported_flags >>= 8;
				if (!(supported_flags & 1)) {
					ath12k_warn(ab,
						    "Spectral fft size %d not supported",
						    p->scan_fft_size);
					return -EINVAL;
				} else {
					p->scan_fft_size = 9;
					num_bins <<= 2;
				}
				break;
			}
			spectral_cap.fft_size_caps++;
		}
		summary->meta.ch_width = chan_width;
		if (ab->hw_params->spectral.fragment_160mhz)
			fragment_sample = true;
		break;
	case WMI_PEER_CHWIDTH_MAX:
		for (i = 0; i < spectral_cap.num_fft_size_caps_entry; i++) {
			if (spectral_cap.fft_size_caps->sscan_bw == chan_width) {
				supported_flags = spectral_cap.fft_size_caps->supported_flags;
				supported_flags >>= 8;
				if (!(supported_flags & 1)) {
					ath12k_warn(ab,
						    "Spectral fft size %d not supported",
						    p->scan_fft_size);
					return -EINVAL;
				} else {
					p->scan_fft_size = 9;
					num_bins <<= 2;
				}
				break;
			}
			spectral_cap.fft_size_caps++;
		}
		summary->meta.ch_width = chan_width;
		break;
	default:
		ath12k_warn(ab, "invalid channel width %d\n", chan_width);
		return -EINVAL;
	}

	ret = ath12k_spectral_fill_fft_sample(ar, summary, fft_report, &search,
					      fft_sample, num_bins,
					      fragment_sample);
	if (ret) {
		ath12k_warn(ab, "failed to fill fft sample %d\n", ret);
		return ret;
	}
	return 0;
}

static int ath12k_spectral_process_data(struct ath12k *ar,
					struct ath12k_dbring_data *param)
{
	struct ath12k_base *ab = ar->ab;
	struct spectral_tlv *tlv;
	struct spectral_summary_fft_report *summary = NULL;
	struct ath12k_spectral_summary_report summ_rpt = {0};
	struct fft_sample_ath12k *fft_sample = NULL;
	u8 *data;
	u32 data_len, i;
	u32 check_length;
	u8 sign, tag;
	int tlv_len, sample_sz;
	int ret, event_ret;
	bool quit = false;
	bool send_complete = false;

	spin_lock_bh(&ar->spectral.lock);

	if (!ar->spectral.enabled) {
		ret = -EINVAL;
		goto unlock;
	}

	sample_sz = sizeof(*fft_sample) + ATH12K_SPECTRAL_ATH12K_MAX_IB_BINS(ab);
	fft_sample = kzalloc(sample_sz, GFP_ATOMIC);
	if (!fft_sample) {
		ret = -ENOBUFS;
		goto unlock;
	}

	data = param->data;
	data_len = param->data_sz;
	i = 0;

	/* Stale post-scan DMA buffers arrive after CLEAR+DISABLE while the
	 * next scan's dbring_reset has already refilled the ring with magic.
	 * Valid spectral TLV always starts with sign=0xFA in bits[31:24];
	 * 0xdeadbeaf (0xDE...) can never be a valid TLV header, so discard
	 * silently to avoid spurious TLV parse errors.
	 */
	if (data_len >= 4 &&
	    le32_to_cpu(*(__le32 *)data) == ATH12K_DB_MAGIC_VALUE) {
		ret = 0;
		goto err;
	}

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
		   "spectral process data: data_len=%u ch_width=%u pri20_freq=%u cfreq1=%u\n",
		   data_len, ar->spectral.ch_width,
		   ar->spectral.pri20_freq, ar->spectral.sscan_cfreq1);

	while (!quit && (i < data_len)) {
		if ((i + sizeof(*tlv)) > data_len) {
			ath12k_warn(ab, "failed to parse spectral tlv hdr at bytes %d\n",
				    i);
			ret = -EINVAL;
			goto err;
		}

		tlv = (struct spectral_tlv *)&data[i];
		sign = FIELD_GET(SPECTRAL_TLV_HDR_SIGN,
				 __le32_to_cpu(tlv->header));
		if (sign != ATH12K_SPECTRAL_SIGNATURE) {
			/*TODO: Need to add this warn print back after resolving cpu cache issue
			 *
			ath12k_warn(ab, "Invalid sign 0x%x at bytes %d\n",
				    sign, i);*/
			ar->spectral.diag.sig_mismatch++;
			ret = -EINVAL;
			goto err;
		}

		tlv_len = FIELD_GET(SPECTRAL_TLV_HDR_LEN,
				    __le32_to_cpu(tlv->header));
		/* convert Dword into bytes */
		tlv_len *= ATH12K_SPECTRAL_DWORD_SIZE;
		if ((i + sizeof(*tlv) + tlv_len) > data_len) {
			ath12k_warn(ab, "failed to parse spectral tlv payload at bytes %d tlv_len:%d data_len:%d\n",
				    i, tlv_len, data_len);
			ret = -EINVAL;
			goto err;
		}

		tag = FIELD_GET(SPECTRAL_TLV_HDR_TAG,
				__le32_to_cpu(tlv->header));
		switch (tag) {
		case ATH12K_SPECTRAL_TAG_SCAN_SUMMARY:
			/* HW bug in tlv length of summary report,
			 * HW report 3 DWORD size but the data payload
			 * is 4 DWORD size (16 bytes).
			 * Need to remove this workaround once HW bug fixed
			 */
			tlv_len = sizeof(*summary) - sizeof(*tlv) +
				ab->hw_params->spectral.summary_pad_sz;

			if (tlv_len < (sizeof(*summary) - sizeof(*tlv))) {
				ath12k_warn(ab, "failed to parse spectral summary at bytes %d tlv_len:%d\n",
					    i, tlv_len);
				ret = -EINVAL;
				goto err;
			}

			/*
			 * Validate current summary TLV only. Do not include optional
			 * summary padding bytes, as firmware may leave them untouched.
			 */
			check_length = sizeof(*summary);
			ret = ath12k_dbring_validate_buffer(ar, tlv, check_length);
			if (ret) {
				ath12k_warn(ar->ab, "found magic value in spectral summary, dropping\n");
				if (ath12k_debug_mask & ATH12K_DBG_SPECTRAL)
					print_hex_dump(KERN_DEBUG,
						       "spectral summary dump: ",
						       DUMP_PREFIX_OFFSET, 16, 1, tlv,
						       min_t(u32, data_len - i,
							     sizeof(*tlv) + tlv_len),
						       false);
				goto err;
			}

			summary = (struct spectral_summary_fft_report *)tlv;
			ret = ath12k_spectral_pull_summary(ar, &param->meta,
							   summary, &summ_rpt);
			if (ret) {
				ath12k_warn(ab, "failed to pull spectral summary %d\n",
					    ret);
				goto err;
			}
			break;
		case ATH12K_SPECTRAL_TAG_SCAN_SEARCH:
			if (tlv_len < (sizeof(struct spectral_search_fft_report) -
				       sizeof(*tlv))) {
				ath12k_warn(ab, "failed to parse spectral search fft at bytes %d\n",
					    i);
				ret = -EINVAL;
				goto err;
			}

			/* Drop FFT samples that arrive after userspace stopped
			 * the scan or the timeout fired — mode/scan_active have
			 * already been flipped under the lock we hold.
			 */
			if (ar->spectral.mode == SPECTRAL_SCAN_MODE_INVALID) {
				ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
					   "spectral: dropping stale fft sample after scan stop\n");
				quit = true;
				break;
			}

			ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
				   "spectral fft tlv: len=%d\n", tlv_len);
			if (ath12k_debug_mask & ATH12K_DBG_SPECTRAL)
				print_hex_dump(KERN_DEBUG, "fft tlv: ",
					       DUMP_PREFIX_OFFSET, 16, 1, tlv,
					       sizeof(*tlv) + tlv_len, false);
			memset(fft_sample, 0, sample_sz);
			ret = ath12k_spectral_process_fft(ar, &summ_rpt, tlv,
							  fft_sample,
							  data_len - i);
			if (ret) {
				ath12k_warn(ab, "failed to process spectral fft at bytes %d\n",
					    i);
				goto err;
			}
			if (ar->spectral.params[ar->spectral.mode].scan_count > 0 &&
			    ++ar->spectral.samples_done >=
			    ar->spectral.params[ar->spectral.mode].scan_count)
				send_complete = true;
			quit = true;
			break;
		}
		i += sizeof(*tlv) + tlv_len;
	}

	ret = 0;

err:
	kfree(fft_sample);
unlock:
	/* Snapshot mode under the lock before releasing it - the two reads
	 * below must see the mode that was active for this batch of FFTs, not
	 * a mode that a concurrent configure_scan_params() may have already
	 * switched to (rapid A -> A retune racing with in-flight FFT delivery).
	 */
	{
		enum spectral_scan_mode mode_snap = ar->spectral.mode;

		spin_unlock_bh(&ar->spectral.lock);

		/* Clean up agile state on FFT processing error so stale frequency
		 * and bandwidth params do not persist for the next scan attempt.
		 */
		if (ret && mode_snap == SPECTRAL_SCAN_MODE_AGILE)
			ath12k_spectral_rollback_agile(ar);

		if (!ret && send_complete) {
			enum qca_wlan_vendor_spectral_scan_complete_status s =
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_SUCCESSFUL;
			u32 samples_done;

			/* Cancel the host-side timeout — finite scan succeeded.
			 * Lock already dropped, so this can't deadlock against the
			 * hrtimer callback which takes the same lock.
			 */
			hrtimer_cancel(&ar->spectral.scan_completion_timer);

			spin_lock_bh(&ar->spectral.lock);
			samples_done = ar->spectral.samples_done;
			spin_unlock_bh(&ar->spectral.lock);

			if (mode_snap == SPECTRAL_SCAN_MODE_AGILE) {
				ath12k_spectral_rollback_agile(ar);
			} else {
				/* Keep mode=NORMAL and do NOT reset to INVALID.
				 * The next configure_scan_params() uses prev_mode to send
				 * CLEAR+DISABLE in the correct mode to firmware.
				 */
				spin_lock_bh(&ar->spectral.lock);
				ar->spectral.scan_active = false;
				spin_unlock_bh(&ar->spectral.lock);
			}

			event_ret = ath12k_spectral_send_complete_event(ar, s,
									samples_done);
			if (event_ret)
				ath12k_warn(ar->ab,
					    "failed to send spectral scan complete event: %d\n",
					    event_ret);
		}
	} /* end mode_snap block */
	return ret;
}

static int ath12k_spectral_ring_alloc(struct ath12k *ar,
				      struct ath12k_dbring_cap *db_cap)
{
	struct ath12k_spectral *sp = &ar->spectral;
	int ret;

	ret = ath12k_dbring_srng_setup(ar, &sp->rx_ring,
				       0, db_cap->min_elem);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring\n");
		return ret;
	}

	ath12k_dbring_set_cfg(ar, &sp->rx_ring,
			      ATH12K_SPECTRAL_NUM_RESP_PER_EVENT,
			      ATH12K_SPECTRAL_EVENT_TIMEOUT_MS,
			      ath12k_spectral_process_data);

	ret = ath12k_dbring_buf_setup(ar, &sp->rx_ring, db_cap);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring buffer\n");
		goto srng_cleanup;
	}

	ret = ath12k_dbring_wmi_cfg_setup(ar, &sp->rx_ring,
					  WMI_DIRECT_BUF_SPECTRAL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring cfg\n");
		goto buffer_cleanup;
	}

	return 0;

buffer_cleanup:
	ath12k_dbring_buf_cleanup(ar, &sp->rx_ring);
srng_cleanup:
	ath12k_dbring_srng_cleanup(ar, &sp->rx_ring);
	return ret;
}

static inline void ath12k_spectral_ring_free(struct ath12k *ar)
{
	struct ath12k_spectral *sp = &ar->spectral;

	ath12k_dbring_srng_cleanup(ar, &sp->rx_ring);
	ath12k_dbring_buf_cleanup(ar, &sp->rx_ring);
}

static inline void ath12k_spectral_debug_unregister(struct ath12k *ar)
{
	if (ar->spectral.rfs_scan) {
		relay_close(ar->spectral.rfs_scan);
		ar->spectral.rfs_scan = NULL;
	}
}

int ath12k_spectral_send_complete_event(struct ath12k *ar,
					int status,
					u32 received_samples)
{
	struct sk_buff *skb;
	struct ath12k_hw *ah = ar->ah;

	spin_lock_bh(&ar->spectral.lock);
	ar->spectral.scan_active = false;
	spin_unlock_bh(&ar->spectral.lock);

	if (ar->spectral.rfs_scan)
		relay_flush(ar->spectral.rfs_scan);

	int evid = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE_INDEX;

	skb = cfg80211_vendor_event_alloc(ah->hw->wiphy, NULL,
					  NLMSG_DEFAULT_SIZE,
					  evid, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_STATUS,
			status) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETE_RECEIVED_SAMPLES,
			received_samples)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;
}

/* Process-context worker: re-binds the DMA ring to firmware after it has
 * acknowledged the agile scan config via WMI_PDEV_SSCAN_FW_PARAM_EVENTID.
 * Must run from process context because ath12k_dbring_wmi_cfg_setup calls
 * ath12k_wmi_cmd_send which may sleep.
 */
static void ath12k_spectral_agile_dma_rebind_work(struct work_struct *work)
{
	struct ath12k_spectral *sp =
		container_of(work, struct ath12k_spectral, agile_dma_rebind_work);
	struct ath12k *ar = container_of(sp, struct ath12k, spectral);
	int ret;

	ret = ath12k_dbring_wmi_cfg_setup(ar, &sp->rx_ring,
					  WMI_DIRECT_BUF_SPECTRAL);
	if (ret)
		ath12k_warn(ar->ab,
			    "spectral agile DMA rebind: dbring wmi cfg setup failed: %d\n",
			    ret);
}

/* Process-context worker: runs when the hrtimer expires. The hrtimer
 * itself runs in softirq and cannot issue WMI commands or sleep.
 *
 * LOCKING NOTE: this function intentionally does NOT call wiphy_lock().
 * Calling wiphy_lock() here would deadlock when wiphy->mtx is already
 * held by the nl80211 vendor-command dispatch path (e.g. while hostapd
 * or spectraltool is processing a vendor command).  The vdev_id needed
 * to send CLEAR+DISABLE is cached in sp->active_vdev_id at scan-start
 * (under sp->lock) so we never need to walk ar->arvifs here.
 */
static void ath12k_spectral_timeout_work(struct work_struct *work)
{
	struct ath12k_spectral *sp =
		container_of(work, struct ath12k_spectral, scan_timeout_work);
	struct ath12k *ar = container_of(sp, struct ath12k, spectral);
	enum spectral_scan_mode prev_mode;
	u32 received;
	u32 vdev_id;
	int ret;

	spin_lock_bh(&sp->lock);
	/* Drop the timeout if the scan already completed by another path
	 * (success in process_data, explicit stop_scan, or deinit) — the
	 * hrtimer cb may have raced with that path and queued us anyway.
	 * scan_active flips to false in send_complete_event() and stop_scan().
	 */
	if (!sp->scan_active || sp->mode == SPECTRAL_SCAN_MODE_INVALID) {
		spin_unlock_bh(&sp->lock);
		return;
	}

	/* Read current mode before using it in the rearm check below. */
	prev_mode = sp->mode;

	/* If firmware made progress since the last worker invocation
	 * (timeout_received_count advanced beyond timeout_prev_count),
	 * the scan is still running rearm the timer with a 200ms floor
	 * to handle firmware delivery rate variations. Advance
	 * timeout_prev_count so the next firing detects a genuine stall.
	 */
	if (sp->params[prev_mode].scan_count > 0 &&
	    sp->timeout_received_count > sp->timeout_prev_count &&
	    sp->timeout_received_count < sp->params[prev_mode].scan_count) {
		u32 remaining = sp->params[prev_mode].scan_count -
				sp->timeout_received_count;
		u64 rearm_ns  = (u64)remaining * sp->params[prev_mode].scan_period * 2 *
				NSEC_PER_USEC;

		if (rearm_ns < 200 * NSEC_PER_MSEC)
			rearm_ns = 200 * NSEC_PER_MSEC;

		sp->timeout_prev_count = sp->timeout_received_count;
		spin_unlock_bh(&sp->lock);

		hrtimer_start(&sp->scan_completion_timer,
			      ns_to_ktime(rearm_ns),
			      HRTIMER_MODE_REL_SOFT);
		return;
	}

	received = sp->timeout_received_count;
	vdev_id  = sp->active_vdev_id;
	prev_mode = sp->mode;

	ath12k_warn(ar->ab, "spectral: scan timeout mode=%d received=%u/%u vdev_id=%u\n",
		    prev_mode, received, sp->params[prev_mode].scan_count, vdev_id);
	/* Do NOT clear sp->mode yet - ath12k_wmi_vdev_spectral_enable() reads
	 * ar->spectral.mode (== sp->mode) to derive the scan_mode field in the
	 * WMI command.  If we set mode=INVALID before calling it, firmware gets
	 * scan_mode=NORMAL even when timing out an AGILE scan, so the agile
	 * detector is never properly torn down. Set INVALID after the WMI call.
	 */
	if (prev_mode == SPECTRAL_SCAN_MODE_AGILE) {
		/* Clear firmware-cached agile detector state */
		sp->sscan_cfreq1 = 0;
		sp->sscan_cfreq2 = 0;
		sp->sscan_bw = 0;
		sp->start_freq = 0;
		sp->end_freq = 0;
	}
	sp->scan_active = false;
	spin_unlock_bh(&sp->lock);

	/* Send CLEAR+DISABLE to firmware using the cached vdev_id.
	 * No wiphy_lock needed — see function comment above.
	 */
	ret = ath12k_wmi_vdev_spectral_enable(ar, vdev_id,
				ATH12K_WMI_SPECTRAL_TRIGGER_CMD_CLEAR,
				ATH12K_WMI_SPECTRAL_ENABLE_CMD_IGNORE);
	if (!ret)
		ret = ath12k_wmi_vdev_spectral_enable(ar, vdev_id,
						ATH12K_WMI_SPECTRAL_TRIGGER_CMD_IGNORE,
						ATH12K_WMI_SPECTRAL_ENABLE_CMD_DISABLE);
	/* Keep mode=prev_mode - do NOT reset to INVALID. */
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to disable spectral scan on vdev %d after timeout: %d\n",
			    vdev_id, ret);

	ath12k_spectral_send_complete_event(ar,
		QCA_WLAN_VENDOR_SPECTRAL_SCAN_COMPLETE_STATUS_TIMEOUT,
		received);
}

/* hrtimer callback: runs in softirq context. Snapshot received-sample
 * count and defer the rest to the work item — WMI commands and the
 * vendor cmd reply skb cannot run in softirq.
 */
static enum hrtimer_restart ath12k_spectral_scan_timeout(struct hrtimer *timer)
{
	struct ath12k_spectral *sp =
		container_of(timer, struct ath12k_spectral, scan_completion_timer);

	/* Softirq context (HRTIMER_MODE_REL_SOFT) — plain spin_lock is sufficient. */
	spin_lock(&sp->lock);

	if (sp->mode == SPECTRAL_SCAN_MODE_INVALID || !sp->scan_active) {
		spin_unlock(&sp->lock);
		return HRTIMER_NORESTART;
	}

	sp->timeout_received_count = sp->samples_done;

	spin_unlock(&sp->lock);

	schedule_work(&sp->scan_timeout_work);

	return HRTIMER_NORESTART;
}

int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif)
{
	if (!arvif->spectral_enabled)
		return 0;

	return ath12k_spectral_stop_scan(arvif->ar);
}

void ath12k_spectral_reset_buffer(struct ath12k *ar)
{
	if (!ar->spectral.enabled)
		return;

	if (ar->spectral.rfs_scan)
		relay_reset(ar->spectral.rfs_scan);
}

void ath12k_spectral_deinit(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_spectral *sp;
	int i;

	for (i = 0; i <  ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		sp = &ar->spectral;

		if (sp->spectral_cap.fft_size_caps) {
			kfree(sp->spectral_cap.fft_size_caps);
			sp->spectral_cap.fft_size_caps = NULL;
		}

		if (!sp->enabled)
			continue;

		spin_lock_bh(&sp->lock);
		sp->enabled = false;
		spin_unlock_bh(&sp->lock);

		/* Cancel the host-side timeout and flush its worker before
		 * tearing down the WMI scan / debugfs / dbring. After
		 * cancel_work_sync the worker can't re-arm anything.
		 */
		hrtimer_cancel(&sp->scan_completion_timer);
		cancel_work_sync(&sp->scan_timeout_work);
		cancel_work_sync(&sp->agile_dma_rebind_work);

		if (ar->spectral.mode < SPECTRAL_SCAN_MODE_MAX) {
			wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
			ath12k_spectral_stop_scan(ar);
			wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		}

		ath12k_spectral_debug_unregister(ar);
		ath12k_spectral_ring_free(ar);
	}
}

/**
 * ath12k_spectral_validate_scan_count_max - Validate and clamp scan count max
 * @ab: ath12k base structure
 * @ini_value: Value read from INI
 * @pdev_idx: Physical device index for logging
 *
 * Clamps the INI value to the valid range [MIN, HW_LIMIT].
 * Values above the HW limit (4095) are capped with a warning.
 * Values below the minimum (512) are raised with a warning.
 *
 * Returns: Validated scan count value
 */
#ifdef CPTCFG_QCN_EXTN
static u32 ath12k_spectral_validate_scan_count_max(struct ath12k_base *ab,
						   u32 ini_value,
						   int pdev_idx)
{
	u32 clamped_scan_count = ini_value;

	/* Cap to hardware maximum (4095 buffers, ~16MB per radio) */
	if (clamped_scan_count > ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT) {
		ath12k_warn(ab,
			    "spectral: scan_count_max=%u exceeds HW limit %u, capping to %u (pdev %d)\n",
			    clamped_scan_count,
			    ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT,
			    ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT,
			    pdev_idx);
		clamped_scan_count = ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT;
	}

	/* Enforce minimum (512 buffers minimum, ~2MB per radio) */
	if (clamped_scan_count < ATH12K_SPECTRAL_SCAN_COUNT_MIN) {
		ath12k_warn(ab,
			    "spectral: scan_count_max=%u below minimum %u, capping to %u (pdev %d)\n",
			    clamped_scan_count,
			    ATH12K_SPECTRAL_SCAN_COUNT_MIN,
			    ATH12K_SPECTRAL_SCAN_COUNT_MIN,
			    pdev_idx);
		clamped_scan_count = ATH12K_SPECTRAL_SCAN_COUNT_MIN;
	}

	return clamped_scan_count;
}
#endif /* CPTCFG_QCN_EXTN */

static inline int ath12k_spectral_debug_register(struct ath12k *ar)
{
	ath12k_info(ar->ab,
		    "spectral: opening relay with num_sub_bufs=%u (pdev %d)\n",
		    ar->spectral.scan_count_max, ar->pdev_idx);

	ar->spectral.rfs_scan = relay_open("spectral_scan",
					   ar->debug.debugfs_pdev,
					   ATH12K_SPECTRAL_SUB_BUFF_SIZE(ar->ab),
					   ar->spectral.scan_count_max,
					   &rfs_scan_cb, &ar->spectral);
	if (!ar->spectral.rfs_scan) {
		ath12k_warn(ar->ab, "failed to open relay in pdev %d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	ath12k_info(ar->ab,
		    "spectral: relay opened successfully with %u buffers (pdev %d)\n",
		    ar->spectral.scan_count_max, ar->pdev_idx);

	ar->spectral.sub_buf_size = ATH12K_SPECTRAL_SUB_BUFF_SIZE(ar->ab);
	ar->spectral.num_sub_bufs = ar->spectral.scan_count_max;

	debugfs_create_u32("spectral_data_sub_buffer_size", 0444,
			   ar->debug.debugfs_pdev,
			   &ar->spectral.sub_buf_size);

	debugfs_create_u32("spectral_data_num_sub_buffers", 0444,
			   ar->debug.debugfs_pdev,
			   &ar->spectral.num_sub_bufs);

	debugfs_create_file("spectral_relay_full_events", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_full_events_fops);
	debugfs_create_file("spectral_relay_write_attempts", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_write_attempts_fops);
	debugfs_create_file("spectral_relay_write_accepted", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_write_accepted_fops);
	debugfs_create_file("spectral_relay_write_rejected", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_write_rejected_fops);
	debugfs_create_file("spectral_relay_overwrite_events", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_overwrite_events_fops);
	debugfs_create_file("spectral_relay_reset", 0200,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_reset_fops);
	debugfs_create_file("spectral_relay_peak_occupancy", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_peak_occupancy_fops);
	debugfs_create_file("spectral_relay_current_occupancy", 0444,
			    ar->debug.debugfs_pdev, &ar->spectral,
			    &ath12k_spectral_relay_current_occupancy_fops);

	debugfs_create_bool("spectral_dbr_buff_debug", 0600,
			    ar->debug.debugfs_pdev,
			    &ar->spectral.dbr_buff_debug);

	return 0;
}

int ath12k_spectral_init(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_spectral *sp;
	struct ath12k_dbring_cap db_cap;
	int ret;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_FREQINFO_IN_METADATA,
		      ab->wmi_ab.svc_map))
		return 0;

	if (!ab->hw_params->spectral.fft_sz)
		return 0;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		sp = &ar->spectral;

		ret = ath12k_dbring_get_cap(ar->ab, ar->pdev_idx,
					    WMI_DIRECT_BUF_SPECTRAL,
					    &db_cap);
		if (ret)
			continue;

		idr_init(&sp->rx_ring.bufs_idr);
		spin_lock_init(&sp->rx_ring.idr_lock);
		spin_lock_init(&sp->lock);
		spin_lock_init(&sp->relay_lock);
		atomic_set(&sp->relay_resetting, 0);
#if LINUX_VERSION_IS_GEQ(6, 18, 0)
		hrtimer_setup(&sp->scan_completion_timer, ath12k_spectral_scan_timeout,
			      CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
#else
		hrtimer_init(&sp->scan_completion_timer, CLOCK_MONOTONIC,
			     HRTIMER_MODE_REL_SOFT);
		sp->scan_completion_timer.function = ath12k_spectral_scan_timeout;
#endif
		INIT_WORK(&sp->scan_timeout_work, ath12k_spectral_timeout_work);
		INIT_WORK(&sp->agile_dma_rebind_work,
			  ath12k_spectral_agile_dma_rebind_work);

		ret = ath12k_spectral_ring_alloc(ar, &db_cap);
		if (ret) {
			ath12k_warn(ab, "failed to init spectral ring for pdev %d\n",
				    i);
			goto deinit;
		}

		spin_lock_bh(&sp->lock);

		atomic64_set(&sp->relay_write_attempts, 0);
		atomic64_set(&sp->relay_write_accepted, 0);
		atomic64_set(&sp->relay_write_rejected, 0);
		atomic64_set(&sp->relay_full_events, 0);
		atomic64_set(&sp->relay_overwrite_events, 0);
		atomic64_set(&sp->relay_peak_occupancy, 0);
		sp->mode = SPECTRAL_SCAN_MODE_INVALID;

		for (int m = 0; m < SPECTRAL_SCAN_MODE_MAX; m++) {
			sp->params[m].scan_count = ATH12K_WMI_SPECTRAL_COUNT_DEFAULT;
			sp->params[m].scan_period = ATH12K_WMI_SPECTRAL_PERIOD_DEFAULT;
			sp->params[m].scan_priority =
				ATH12K_WMI_SPECTRAL_PRIORITY_DEFAULT;
			sp->params[m].scan_fft_size =
				ATH12K_WMI_SPECTRAL_FFT_SIZE_DEFAULT;
			sp->params[m].scan_gc_ena = ATH12K_WMI_SPECTRAL_GC_ENA_DEFAULT;
			sp->params[m].scan_restart_ena =
				ATH12K_WMI_SPECTRAL_RESTART_ENA_DEFAULT;
			sp->params[m].scan_noise_floor_ref =
				ATH12K_WMI_SPECTRAL_NOISE_FLOOR_REF_DEFAULT;
			sp->params[m].scan_init_delay =
				ATH12K_WMI_SPECTRAL_INIT_DELAY_DEFAULT;
			sp->params[m].scan_nb_tone_thr =
				ATH12K_WMI_SPECTRAL_NB_TONE_THR_DEFAULT;
			sp->params[m].scan_str_bin_thr =
				ATH12K_WMI_SPECTRAL_STR_BIN_THR_DEFAULT;
			sp->params[m].scan_wb_rpt_mode =
				ATH12K_WMI_SPECTRAL_WB_RPT_MODE_DEFAULT;
			sp->params[m].scan_rssi_rpt_mode =
				ATH12K_WMI_SPECTRAL_RSSI_RPT_MODE_DEFAULT;
			sp->params[m].scan_rssi_thr =
				ATH12K_WMI_SPECTRAL_RSSI_THR_DEFAULT;
			sp->params[m].scan_pwr_format =
				ATH12K_WMI_SPECTRAL_PWR_FORMAT_DEFAULT;
			sp->params[m].scan_rpt_mode =
				ATH12K_WMI_SPECTRAL_RPT_MODE_DEFAULT;
			sp->params[m].scan_bin_scale =
				ATH12K_WMI_SPECTRAL_BIN_SCALE_DEFAULT;
			sp->params[m].scan_dbm_adj =
				ATH12K_WMI_SPECTRAL_DBM_ADJ_DEFAULT;
			sp->params[m].scan_chn_mask = ar->cfg_rx_chainmask ?
				ar->cfg_rx_chainmask :
				ATH12K_WMI_SPECTRAL_CHN_MASK_DEFAULT;
		}
		sp->dbr_buff_debug = false;
		ath12k_spectral_timestamp_war_init(ar);
		sp->enabled = true;

		spin_unlock_bh(&sp->lock);

		/* Read spectral scan count max from chip-specific INI.*/
#ifdef CPTCFG_QCN_EXTN
		sp->scan_count_max =
			ath12k_spectral_validate_scan_count_max(ab,
				ath12k_cfg_get(ab,
					       ATH12K_CFG_SPECTRAL_SCAN_COUNT_MAX),
				i);

		ath12k_info(ab,
			    "spectral: scan_count_max=%u (pdev %d, valid range: %u-%u)\n",
			    sp->scan_count_max, i,
			    ATH12K_SPECTRAL_SCAN_COUNT_MIN,
			    ATH12K_SPECTRAL_SCAN_COUNT_MAX_HW_LIMIT);
#else
		sp->scan_count_max = ATH12K_SPECTRAL_SCAN_COUNT_MAX_DEFAULT;
#endif /* CPTCFG_QCN_EXTN */

		ath12k_spectral_init_param_min_max(ar);

		ret = ath12k_spectral_debug_register(ar);
		if (ret) {
			ath12k_warn(ab, "failed to register spectral for pdev %d\n",
				    i);
			goto deinit;
		}
	}

	return 0;

deinit:
	ath12k_spectral_deinit(ab);
	return ret;
}

struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar)
{
	if (ar->spectral.enabled)
		return &ar->spectral.rx_ring;
	else
		return NULL;
}
