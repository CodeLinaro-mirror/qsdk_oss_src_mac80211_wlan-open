/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_SPECTRAL_H
#define ATH12K_SPECTRAL_H

#include "../spectral_common.h"
#include "dbring.h"
#include "vendor.h"

/* enum ath12k_spectral_mode:
 *
 * @SPECTRAL_DISABLED: spectral mode is disabled
 * @SPECTRAL_BACKGROUND: hardware sends samples when it is not busy with
 *	something else.
 * @SPECTRAL_MANUAL: spectral scan is enabled, triggering for samples
 *	is performed manually.
 */
enum ath12k_spectral_mode {
	ATH12K_SPECTRAL_DISABLED = 0,
	ATH12K_SPECTRAL_BACKGROUND,
	ATH12K_SPECTRAL_MANUAL,
};

/**
 * struct ath12k_spectral_params - parameters passed to WMI spectral scan config
 *
 * @scan_count:          number of FFT samples to capture; 0 = unlimited
 *                       (always 0 in background mode regardless of this value)
 * @scan_period:         time between consecutive scan triggers (in TU)
 * @scan_priority:       scan priority relative to other HW operations
 * @scan_fft_size:       FFT size as log2 (e.g. 8 => 256 bins); validated
 *                       against per-chip capability at scan start
 * @scan_gc_ena:         enable gain change between scans
 * @scan_restart_ena:    auto-restart scan after each sweep
 * @scan_noise_floor_ref: reference noise floor in dBm
 * @scan_init_delay:     delay in OFDM symbols before the first scan
 * @scan_nb_tone_thr:    narrowband tone-detection threshold
 * @scan_str_bin_thr:    strong-bin count threshold used for reporting
 * @scan_wb_rpt_mode:    wideband report mode (0 = no report, 1 = summary)
 * @scan_rssi_rpt_mode:  RSSI-based report gating (0 = always, 1 = only above thr)
 * @scan_rssi_thr:       RSSI threshold for conditional reporting
 * @scan_pwr_format:     power format: 0 = linear magnitude, 1 = dBm
 * @scan_rpt_mode:       controls which FFT bins are included in the report
 * @scan_bin_scale:      scaling factor applied to FFT bins before reporting
 * @scan_dbm_adj:        dBm adjustment offset applied to reported power values
 * @scan_chn_mask:       bitmask of ADC chains to include in the scan
 * @fft_period:          interval between consecutive FFTs within a sweep
 * @short_report:        if non-zero, only the peak-magnitude bin is reported
 * @frequency:           primary channel center frequency (MHz)
 * @frequency2:          secondary channel center frequency (MHz); used in 80+80 mode
 * @bandwidth:           channel bandwidth
 * @fft_recapture:       allow HW to recapture FFT if the initial capture is invalid
 * @completion_timeout_us: maximum time to wait for scan completion (microseconds)
 */
struct ath12k_spectral_params {
	u32 scan_count;
	u32 scan_period;
	u32 scan_priority;
	u32 scan_fft_size;
	u32 scan_gc_ena;
	u32 scan_restart_ena;
	u32 scan_noise_floor_ref;
	u32 scan_init_delay;
	u32 scan_nb_tone_thr;
	u32 scan_str_bin_thr;
	u32 scan_wb_rpt_mode;
	u32 scan_rssi_rpt_mode;
	u32 scan_rssi_thr;
	u32 scan_pwr_format;
	u32 scan_rpt_mode;
	u32 scan_bin_scale;
	u32 scan_dbm_adj;
	u32 scan_chn_mask;
	u32 fft_period;
	u32 short_report;
	u32 frequency;
	u32 frequency2;
	u8  bandwidth;
	u8  fft_recapture;
	u32 completion_timeout_us;
};

struct ath12k_spectral_diag_stats {
	u64 sig_mismatch;
	u64 sec80_sfft_insufflen;
	u64 no_sec80_sfft;
	u64 vhtseg1id_mismatch;
	u64 vhtseg2id_mismatch;
};

struct ath12k_spectral {
	struct ath12k_dbring rx_ring;
	/* Protects enabled, mode, and scan_active */
	spinlock_t lock;
	struct rchan *rfs_scan;	/* relay(fs) channel for spectral scan */
	struct dentry *scan_ctl;
	struct dentry *scan_count;
	struct dentry *scan_bins;
	enum ath12k_spectral_mode mode;
	struct ath12k_spectral_params     params;
	struct ath12k_spectral_diag_stats diag;
	bool enabled;
	bool is_primary;
	bool scan_active;
	u32 ch_width;
	struct wmi_spectral_capabilities_event spectral_cap;
	u32 samples_done;
};

#ifdef CPTCFG_ATH12K_SPECTRAL

int ath12k_spectral_init(struct ath12k_base *ab);
void ath12k_spectral_deinit(struct ath12k_base *ab);
int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif);
void ath12k_spectral_reset_buffer(struct ath12k *ar);
enum ath12k_spectral_mode ath12k_spectral_get_mode(struct ath12k *ar);
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar);
int ath12k_spectral_configure_scan_params(struct ath12k *ar,
					  enum ath12k_spectral_mode mode);
int ath12k_spectral_stop_scan(struct ath12k *ar);
int ath12k_spectral_start_scan(struct ath12k *ar);
int ath12k_spectral_send_complete_event(struct ath12k *ar, int status);

#else

static inline int ath12k_spectral_init(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_spectral_deinit(struct ath12k_base *ab)
{
}

static inline int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif)
{
	return 0;
}

static inline void ath12k_spectral_reset_buffer(struct ath12k *ar)
{
}

static inline
enum ath12k_spectral_mode ath12k_spectral_get_mode(struct ath12k *ar)
{
	return ATH12K_SPECTRAL_DISABLED;
}

static inline
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar)
{
	return NULL;
}

static inline int ath12k_spectral_configure_scan_params(struct ath12k *ar,
							enum ath12k_spectral_mode mode)
{
	return 0;
}

static inline int ath12k_spectral_stop_scan(struct ath12k *ar)
{
	return 0;
}

static inline int ath12k_spectral_start_scan(struct ath12k *ar)
{
	return 0;
}

#endif /* CPTCFG_ATH12K_SPECTRAL */
#endif /* ATH12K_SPECTRAL_H */
