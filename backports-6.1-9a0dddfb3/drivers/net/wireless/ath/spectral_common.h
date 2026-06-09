/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SPECTRAL_COMMON_H
#define SPECTRAL_COMMON_H

#define SPECTRAL_HT20_NUM_BINS		56
#define SPECTRAL_HT20_40_NUM_BINS		128

/* TODO: could possibly be 512, but no samples this large
 * could be acquired so far.
 */
#define SPECTRAL_ATH10K_MAX_NUM_BINS		256
#define SPECTRAL_ATH12K_MAX_NUM_BINS        512

/* FFT sample format given to userspace via debugfs.
 *
 * Please keep the type/length at the front position and change
 * other fields after adding another sample type
 *
 * TODO: this might need rework when switching to nl80211-based
 * interface.
 */
enum ath_fft_sample_type {
	ATH_FFT_SAMPLE_HT20 = 1,
	ATH_FFT_SAMPLE_HT20_40,
	ATH_FFT_SAMPLE_ATH10K,
	ATH_FFT_SAMPLE_ATH11K,
	ATH_FFT_SAMPLE_ATH12K
};

struct fft_sample_tlv {
	u8 type;	/* see ath_fft_sample */
	__be16 length;
	/* type dependent data follows */
} __packed;

struct fft_sample_ht20 {
	struct fft_sample_tlv tlv;

	u8 max_exp;

	__be16 freq;
	s8 rssi;
	s8 noise;

	__be16 max_magnitude;
	u8 max_index;
	u8 bitmap_weight;

	__be64 tsf;

	u8 data[SPECTRAL_HT20_NUM_BINS];
} __packed;

struct fft_sample_ht20_40 {
	struct fft_sample_tlv tlv;

	u8 channel_type;
	__be16 freq;

	s8 lower_rssi;
	s8 upper_rssi;

	__be64 tsf;

	s8 lower_noise;
	s8 upper_noise;

	__be16 lower_max_magnitude;
	__be16 upper_max_magnitude;

	u8 lower_max_index;
	u8 upper_max_index;

	u8 lower_bitmap_weight;
	u8 upper_bitmap_weight;

	u8 max_exp;

	u8 data[SPECTRAL_HT20_40_NUM_BINS];
} __packed;

struct fft_sample_ath10k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	__be16 freq1;
	__be16 freq2;
	__be16 noise;
	__be16 max_magnitude;
	__be16 total_gain_db;
	__be16 base_pwr_db;
	__be64 tsf;
	s8 max_index;
	u8 rssi;
	u8 relpwr_db;
	u8 avgpwr_db;
	u8 max_exp;

	u8 data[];
} __packed;

struct fft_sample_ath11k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	s8 max_index;
	u8 max_exp;
	bool is_primary;
	__be16 freq1;
	__be16 freq2;
	__be16 max_magnitude;
	__be16 rssi;
	__be32 tsf;
	__be32 noise;

	u8 data[];
} __packed;

struct fft_sample_ath12k_detector_info {
	__be32 start_frequency;
	__be32 end_frequency;
	__be32 timestamp;
	__be32 last_tstamp;
	__be32 last_raw_timestamp;
	__be32 timestamp_war_offset;
	__be32 raw_timestamp;
	__be32 reset_delay;
	__be16 start_bin_idx;
	__be16 end_bin_idx;
	__be16 max_index;
	__be16 max_magnitude;
	__be16 noise_floor;
	u8 rssi;
	u8 agc_total_gain;
	u8 gainchange;
	u8 pri80ind;
	u8 is_sec80;
	u8 blanking_status;
} __packed;

struct fft_sample_ath12k {
	struct fft_sample_tlv tlv;
	__be32 signature;
	__be32 pri_freq;
	__be32 target_reset_count;
	__be32 cfreq1;
	__be32 cfreq2;
	__be32 sscan_cfreq1;
	__be32 sscan_cfreq2;
	__be32 bin_pwr_count;
	struct fft_sample_ath12k_detector_info detector_info;
	s8 spectral_lower_rssi;
	s8 spectral_upper_rssi;
	u8 macaddr[SPECTRAL_MAC_ADDR_SIZE];
	u8 spectral_mode;
	u8 operating_bw;
	u8 sscan_bw;
	u8 fft_width;
	u8 dcs_enabled;
	u8 int_type;
	u8 data[0];
} __packed;

#endif /* SPECTRAL_COMMON_H */
