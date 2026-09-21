// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/relay.h>
#include "core.h"
#include "debug.h"
#include "debugfs.h"
#include "dp_peer.h"
#include "ath12k_notif.h"
#include "dp_mon.h"
#include "dp_mon_filter.h"

#define ATH12K_CFR_COMMON_NOISE_FLOOR  (-96)
#define ATH12K_CFR_INVALID_SNR         0x80
#define ATH12K_CFR_GAIN_TABLE_IDX GENMASK(9, 8)
#define ATH12K_CFR_GAIN_DB GENMASK(7, 0)
#define ATH12K_CFR_GAIN_INFO_L_U16 GENMASK(15, 0)
#define ATH12K_CFR_GAIN_INFO_M_U16 GENMASK(31, 16)

static void
ath12k_cfr_update_rcc_debug_stats(struct ath12k *ar,
				  const struct hal_rx_mon_ppdu_info *ppdu_info);

static inline
s32 ath12k_cfr_snr_to_signal_strength(u8 snr)
{
	/* SNR value 0x80 indicates -128 dB and should remain unchanged. */
	return (snr != ATH12K_CFR_INVALID_SNR) ?
		((s8)snr + ATH12K_CFR_COMMON_NOISE_FLOOR) : (s8)snr;
}

bool peer_is_in_cfr_unassoc_pool(struct ath12k *ar, u8 *peer_mac)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int i;

	if (!ar->cfr.cfr_enabled)
		return false;

	spin_lock_bh(&cfr->lock);
	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];
		if (!entry->is_valid)
			continue;

		if (ether_addr_equal(peer_mac, entry->peer_mac)) {
			/* Remove entry if it is single shot */
			if (entry->period == 0) {
				memset(entry->peer_mac, 0 , ETH_ALEN);
				entry->is_valid = false;
				cfr->cfr_enabled_peer_cnt--;
			}
			spin_unlock_bh(&cfr->lock);
			return true;
		}
	}

	spin_unlock_bh(&cfr->lock);

	return false;
}

void ath12k_cfr_lut_update_paddr(struct ath12k *ar, dma_addr_t paddr,
				 u32 buf_id)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut;

	if (cfr->lut) {
		lut = &cfr->lut[buf_id];
		lut->dbr_address = paddr;
	}
}

void ath12k_cfr_decrement_peer_count(struct ath12k *ar,
				     struct ath12k_link_sta *arsta)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	if (!cfr->cfr_enabled)
		return;

	spin_lock_bh(&cfr->lock);

	if (cfr->cfr_enabled_peer_cnt == 0) {
		spin_unlock_bh(&cfr->lock);
		return;
	}

	if (arsta->cfr_capture.cfr_enable)
		cfr->cfr_enabled_peer_cnt--;

	spin_unlock_bh(&cfr->lock);
}

int ath12k_cfr_peer_capture_validate(struct ath12k *ar,
				     struct ath12k_link_sta *arsta,
				     u32 link_sta_bw,
				     u32 *cfr_capture_enable,
				     u32 *cfr_capture_bw,
				     u32 *cfr_capture_period,
				     u32 *cfr_capture_method)
{
	if (*cfr_capture_enable == arsta->cfr_capture.cfr_enable &&
	    (*cfr_capture_period &&
	     *cfr_capture_period == arsta->cfr_capture.cfr_period) &&
	    *cfr_capture_bw == arsta->cfr_capture.cfr_bandwidth &&
	    *cfr_capture_method == arsta->cfr_capture.cfr_method)
		return -EALREADY;

	if (!*cfr_capture_enable &&
	    *cfr_capture_enable == arsta->cfr_capture.cfr_enable)
		return -EALREADY;

	if (*cfr_capture_enable > WMI_PEER_CFR_CAPTURE_ENABLE ||
	    *cfr_capture_bw > link_sta_bw ||
	    *cfr_capture_method > CFR_CAPURE_METHOD_NULL_FRAME_WITH_PHASE ||
	    *cfr_capture_period > WMI_PEER_CFR_PERIODICITY_MAX)
		return -EINVAL;

	if (ar->cfr.cfr_enabled_peer_cnt >= ATH12K_MAX_CFR_ENABLED_CLIENTS &&
	    !arsta->cfr_capture.cfr_enable) {
		ath12k_err(ar->ab, "CFR enable peer threshold reached %u\n",
			   ar->cfr.cfr_enabled_peer_cnt);
		return -EINVAL;
	}

	if (!*cfr_capture_enable) {
		*cfr_capture_bw = arsta->cfr_capture.cfr_bandwidth;
		*cfr_capture_period = arsta->cfr_capture.cfr_period;
		*cfr_capture_method = arsta->cfr_capture.cfr_method;
	}

	return 0;
}

void ath12k_cfr_peer_capture_fill_wmi_arg(struct wmi_peer_cfr_capture_conf_arg *arg,
					  u32 cfr_capture_enable,
					  u32 cfr_capture_bw,
					  u32 cfr_capture_period,
					  u32 cfr_capture_method)
{
	arg->request = cfr_capture_enable;
	arg->periodicity = cfr_capture_period;
	arg->bandwidth = cfr_capture_bw;
	arg->capture_method = cfr_capture_method;
}

void ath12k_cfr_peer_capture_update(struct ath12k *ar,
				    struct ath12k_link_sta *arsta,
				    u32 cfr_capture_enable,
				    u32 cfr_capture_bw,
				    u32 cfr_capture_period,
				    u32 cfr_capture_method)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	spin_lock_bh(&cfr->lock);
	if (cfr_capture_enable != arsta->cfr_capture.cfr_enable) {
		if (cfr_capture_enable)
			cfr->cfr_enabled_peer_cnt++;
		else
			cfr->cfr_enabled_peer_cnt--;
	}
	spin_unlock_bh(&cfr->lock);

	arsta->cfr_capture.cfr_enable = cfr_capture_enable;
	arsta->cfr_capture.cfr_period = cfr_capture_period;
	arsta->cfr_capture.cfr_bandwidth = cfr_capture_bw;
	arsta->cfr_capture.cfr_method = cfr_capture_method;
}

struct ath12k_dbring *ath12k_cfr_get_dbring(struct ath12k *ar)
{
	if (ar->cfr.cfr_enabled || ar->cfr.rcc_enabled)
		return &ar->cfr.rx_ring;

	return NULL;
}

static void ath12k_peer_cfr_default_ta_ra_config(struct cfr_rcc_param *rcc_info,
						 bool allvalid,
						 unsigned long reset_cfg)
{
	struct ta_ra_cfr_cfg *curr_cfg;
	int grp_id;
	unsigned long bitmap = reset_cfg;
	u8 def_mac[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u8 null_mac[ETH_ALEN] = { 0 };

	for (grp_id = 0; grp_id < MAX_TA_RA_ENTRIES; grp_id++) {
		if (!test_bit(grp_id, &bitmap))
			continue;

		curr_cfg = &rcc_info->curr[grp_id];

		curr_cfg->filter_group_id = grp_id;
		ether_addr_copy(curr_cfg->ta_addr, null_mac);
		ether_addr_copy(curr_cfg->ta_addr_mask, def_mac);
		ether_addr_copy(curr_cfg->ra_addr, null_mac);
		ether_addr_copy(curr_cfg->ra_addr_mask, def_mac);
		curr_cfg->bw = 0xf;
		curr_cfg->nss = 0xff;
		curr_cfg->mgmt_subtype_filter = 0;
		curr_cfg->ctrl_subtype_filter = 0;
		curr_cfg->data_subtype_filter = 0;

		if (!allvalid) {
			curr_cfg->valid_ta = 0;
			curr_cfg->valid_ta_mask = 0;
			curr_cfg->valid_ra = 0;
			curr_cfg->valid_ra_mask = 0;
			curr_cfg->valid_bw_mask = 0;
			curr_cfg->valid_nss_mask = 0;
			curr_cfg->valid_mgmt_subtype = 0;
			curr_cfg->valid_ctrl_subtype = 0;
			curr_cfg->valid_data_subtype = 0;
		} else {
			curr_cfg->valid_ta = 1;
			curr_cfg->valid_ta_mask = 1;
			curr_cfg->valid_ra = 1;
			curr_cfg->valid_ra_mask = 1;
			curr_cfg->valid_bw_mask = 1;
			curr_cfg->valid_nss_mask = 1;
			curr_cfg->valid_mgmt_subtype = 1;
			curr_cfg->valid_ctrl_subtype = 1;
			curr_cfg->valid_data_subtype = 1;
		}
	}
}

static void ath12k_peer_cfr_update_global_cfg(struct ath12k *ar)
{
	int grp_id;
	struct ta_ra_cfr_cfg *curr_cfg;
	struct ta_ra_cfr_cfg *glbl_cfg;

	for (grp_id = 0; grp_id < MAX_TA_RA_ENTRIES; grp_id++) {
		if (!test_bit(grp_id, &ar->cfr.rcc_param.modified_in_curr_session))
			continue;

		glbl_cfg = &ar->cfr.global[grp_id];
		curr_cfg = &ar->cfr.rcc_param.curr[grp_id];

		if (curr_cfg->valid_ta)
			ether_addr_copy(glbl_cfg->ta_addr, curr_cfg->ta_addr);

		if (curr_cfg->valid_ra)
			ether_addr_copy(glbl_cfg->ra_addr, curr_cfg->ra_addr);

		if (curr_cfg->valid_ta_mask)
			ether_addr_copy(glbl_cfg->ta_addr_mask,
					curr_cfg->ta_addr_mask);

		if (curr_cfg->valid_ra_mask)
			ether_addr_copy(glbl_cfg->ra_addr_mask,
					curr_cfg->ra_addr_mask);

		if (curr_cfg->valid_bw_mask)
			glbl_cfg->bw = curr_cfg->bw;

		if (curr_cfg->valid_nss_mask)
			glbl_cfg->nss = curr_cfg->nss;

		if (curr_cfg->valid_mgmt_subtype)
			glbl_cfg->mgmt_subtype_filter =
					curr_cfg->mgmt_subtype_filter;

		if (curr_cfg->valid_ctrl_subtype)
			glbl_cfg->ctrl_subtype_filter =
					curr_cfg->ctrl_subtype_filter;

		if (curr_cfg->valid_data_subtype)
			glbl_cfg->data_subtype_filter =
					curr_cfg->data_subtype_filter;
	}
}

static inline
void ath12k_cfr_release_lut_entry(struct ath12k_cfr_look_up_table *lut)
{
	memset(lut, 0, sizeof(*lut));
}

static void
ath12k_cfr_fill_peer_addr(struct ath12k_csi_cfr_header *header,
			  const struct hal_rx_mon_ppdu_info *ppdu_info,
			  u8 num_mu_users, const u8 *fallback_peer_addr)
{
	u8 i;

	if (!num_mu_users) {
		memcpy(header->meta_enh.peer_addr, fallback_peer_addr, ETH_ALEN);
		return;
	}

	for (i = 0; i < num_mu_users; i++) {
		const u8 *peer_addr = ppdu_info->userstats[i].peer_mac;

		memcpy(&header->meta_enh.peer_addr[i * ETH_ALEN], peer_addr, ETH_ALEN);
	}
}

static void ath12k_cfr_rfs_write(struct ath12k *ar, const void *head,
				 u32 head_len, const void *data, u32 data_len,
				 const void * tail, int tail_data)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	if (!ar->cfr.rfs_cfr_capture)
		return;

	relay_write(cfr->rfs_cfr_capture, head, head_len);
	relay_write(cfr->rfs_cfr_capture, data, data_len);
	relay_write(cfr->rfs_cfr_capture, tail, tail_data);
	relay_flush(cfr->rfs_cfr_capture);
}

static void ath12k_cfr_lut_ageout_timer(struct timer_list *t)
{
	struct ath12k_cfr *cfr = from_timer(cfr, t, lut_age_timer);
	struct ath12k *ar = container_of(cfr, struct ath12k, cfr);
	struct ath12k_cfr_look_up_table *lut = NULL;
	struct ath12k_dbring_element *buff;
	unsigned long cur_tstamp = jiffies;
	u64 diff;
	int i;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut)
		goto out;

	for (i = 0; i < cfr->lut_num; i++) {
		lut = &cfr->lut[i];

		if (!lut->dbr_recv || lut->tx_recv)
			continue;

		diff = jiffies_to_msecs((unsigned long)(cur_tstamp - lut->dbr_tstamp));
		if (diff <= ATH12K_CFR_LUT_AGE_TIMER)
			continue;

		spin_lock_bh(&cfr->rx_ring.idr_lock);
		buff = idr_find(&cfr->rx_ring.bufs_idr, i);
		if (!buff) {
			spin_unlock_bh(&cfr->rx_ring.idr_lock);
			ath12k_warn(ar->ab,
				    "Buffer not found in IDR for aged LUT[%d]\n", i);
			ath12k_cfr_release_lut_entry(lut);
			cfr->flush_timeout_dbr_cnt++;
			continue;
		}
		spin_unlock_bh(&cfr->rx_ring.idr_lock);

		ath12k_cfr_release_lut_entry(lut);
		ath12k_dbring_remove_buf_id(&cfr->rx_ring, i);
		ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
		cfr->flush_timeout_dbr_cnt++;
	}

out:
	spin_unlock_bh(&cfr->lut_lock);
	if (cfr->lut_age_timer_init)
		mod_timer(&cfr->lut_age_timer,
			  jiffies + msecs_to_jiffies(ATH12K_CFR_LUT_AGE_TIMER));
}

static void ath12k_cfr_free_pending_dbr_events(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut = NULL;
	struct ath12k_dbring_element *buff;
	int i;

	if (!cfr->lut)
		return;

	for (i = 0; i < cfr->lut_num; i++) {
		lut = &cfr->lut[i];
		if (lut->dbr_recv && !lut->tx_recv &&
		    (lut->dbr_tstamp < cfr->last_success_tstamp)) {
			spin_lock_bh(&cfr->rx_ring.idr_lock);
			buff = idr_find(&cfr->rx_ring.bufs_idr, i);
			if (!buff) {
				spin_unlock_bh(&cfr->rx_ring.idr_lock);
				ath12k_warn(ar->ab,
					    "Buffer not found in IDR for LUT[%d]\n",
					    i);
				ath12k_cfr_release_lut_entry(lut);
				cfr->flush_dbr_cnt++;
				continue;
			}
			spin_unlock_bh(&cfr->rx_ring.idr_lock);

			ath12k_cfr_release_lut_entry(lut);
			ath12k_dbring_remove_buf_id(&cfr->rx_ring, i);
			ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
						     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
			cfr->flush_dbr_cnt++;
		}
	}
}

/* Correlate and relay: This function correlate the data coming from
 * WMI_PDEV_DMA_RING_BUF_RELEASE_EVENT(DBR event) and
 * WMI_PEER_CFR_CAPTURE_EVENT(Tx capture event). if both the events
 * are received and PPDU id matches from the both events,
 * return CORRELATE_STATUS_RELEASE which means relay the correlated data
 * to user space. Otherwise return CORRELATE_STATUS_HOLD which means wait
 * for the second event to come. It will return CORRELATE_STATUS_ERR in
 * case of any error.
 *
 * It also check for the pending DBR events and clear those events
 * in case of corresponding TX capture event is not received for
 * the PPDU.
 */

static int ath12k_cfr_correlate_and_relay(struct ath12k *ar,
					  struct ath12k_cfr_look_up_table *lut,
					  u8 event_type)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	u64 diff;

	if (event_type == ATH12K_CORRELATE_TX_EVENT) {
		if (lut->tx_recv)
			cfr->cfr_dma_aborts++;
		lut->tx_recv = true;
	} else if (event_type == ATH12K_CORRELATE_DBR_EVENT) {
		cfr->dbr_evt_cnt++;
		lut->dbr_recv = true;
	}

	if (lut->dbr_recv && lut->tx_recv) {
		if (lut->dbr_ppdu_id == lut->tx_ppdu_id) {
			cfr->last_success_tstamp = lut->dbr_tstamp;
			if (lut->dbr_tstamp > lut->txrx_tstamp) {
				diff = lut->dbr_tstamp - lut->txrx_tstamp;
				ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
					   "txrx event -> dbr event delay = %u ms",
					   jiffies_to_msecs(diff));
			} else if (lut->txrx_tstamp > lut->dbr_tstamp) {
				diff = lut->txrx_tstamp - lut->dbr_tstamp;
				ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
					   "dbr event -> txrx event delay = %u ms",
					   jiffies_to_msecs(diff));
			}
			ath12k_cfr_free_pending_dbr_events(ar);

			cfr->release_cnt++;
			return ATH12K_CORRELATE_STATUS_RELEASE;
		} else {
			/*
			 * When there is a ppdu id mismatch, discard the TXRX
			 * event since multiple PPDUs are likely to have same
			 * dma addr, due to ucode aborts.
			 */

			ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
				   "Received dbr event twice for the same lut entry");
			lut->tx_recv = false;
			lut->tx_ppdu_id = 0;
			cfr->clear_txrx_event++;
			cfr->cfr_dma_aborts++;
			return ATH12K_CORRELATE_STATUS_HOLD;
		}
	} else {
		return ATH12K_CORRELATE_STATUS_HOLD;
	}
}

u8 freeze_reason_to_capture_type(struct ath12k_base *ab, void *freeze_tlv)
{
	struct macrx_freeze_capture_channel *freeze = freeze_tlv;
	u8 capture_reason = FIELD_GET(MACRX_FREEZE_CC_INFO0_CAPTURE_REASON,
				      freeze->info0);

	switch (capture_reason) {
	case FREEZE_REASON_TM:
		return CFR_CAPTURE_METHOD_TM;
	case FREEZE_REASON_FTM:
		return CFR_CAPTURE_METHOD_FTM;
	case FREEZE_REASON_TA_RA_TYPE_FILTER:
		return CFR_CAPTURE_METHOD_TA_RA_TYPE_FILTER;
	case FREEZE_REASON_NDPA_NDP:
		return CFR_CAPTURE_METHOD_NDPA_NDP;
	case FREEZE_REASON_ALL_PACKET:
		return CFR_CAPTURE_METHOD_ALL_PACKET;
	case FREEZE_REASON_ACK_RESP_TO_TM_FTM:
		return CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM;
	default:
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR Capture Method Type not found");
	}

	return CFR_CAPTURE_METHOD_AUTO;
}
EXPORT_SYMBOL(freeze_reason_to_capture_type);

void extract_peer_mac_from_freeze_tlv(void *freeze_tlv, uint8_t *peermac)
{
	struct macrx_freeze_capture_channel_v3 *freeze =
		(struct macrx_freeze_capture_channel_v3 *)freeze_tlv;

	peermac[0] = freeze->packet_ta_lower_16 & 0x00FF;
	peermac[1] = (freeze->packet_ta_lower_16 & 0xFF00) >> 8;
	peermac[2] = freeze->packet_ta_mid_16 & 0x00FF;
	peermac[3] = (freeze->packet_ta_mid_16 & 0xFF00) >> 8;
	peermac[4] = freeze->packet_ta_upper_16 & 0x00FF;
	peermac[5] = (freeze->packet_ta_upper_16 & 0xFF00) >> 8;
}
EXPORT_SYMBOL(extract_peer_mac_from_freeze_tlv);

/* ath12k_cfr_parse_enh_dma_hdr for chips using the
 * ath12k_cfir_enh_dma_hdr DMA header layout -- shared by wifi6
 * (qcn9074_ops, qcn9160_ops) and wifi7 (qcn9274_ops, wcn7850_ops), all
 * of which use this exact layout; only buffer sizes differ per chip.
 * wifi8/QCN9625 uses a different layout (locsens_common_header_t) and
 * has its own ath12k_hw_qcn9625_parse_cfr_enh_dma_hdr() in wifi8/hw.c.
 *
 * ath12k_wifi6.o, ath12k_wifi7.o, and ath12k_wifi8.o are separate
 * kernel modules from each other and from this (ath12k.o) -- this
 * function lives here, exported, so wifi6 and wifi7 can share one
 * implementation instead of each carrying their own copy.
 *
 * Fills @lut's header-derived fields from the ucode DMA header at
 * @data and returns the total header+payload length in @length.
 */
int ath12k_cfr_parse_enh_dma_hdr(struct ath12k *ar, u8 *data,
				 struct ath12k_cfr_look_up_table *lut,
				 u32 *length)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_cfir_enh_dma_hdr dma_hdr;
	struct cfr_enh_metadata *meta;
	void *freeze_tlv = NULL;
	u8 *peer_macaddr;
	u8 capture_type;

	memcpy(&dma_hdr, data, sizeof(struct ath12k_cfir_enh_dma_hdr));

	if (dma_hdr.freeze_data_incl) {
		freeze_tlv = data + sizeof(struct ath12k_cfir_enh_dma_hdr);
		capture_type = freeze_reason_to_capture_type(ab, freeze_tlv);
	} else {
		capture_type = CFR_CAPTURE_METHOD_AUTO;
	}

	*length = dma_hdr.length * 4;
	*length += dma_hdr.total_bytes;

	lut->dbr_ppdu_id = dma_hdr.phy_ppdu_id;
	lut->header_length = dma_hdr.length;
	lut->payload_length = dma_hdr.total_bytes;
	memcpy(&lut->dma_hdr.enh_hdr, &dma_hdr,
	       sizeof(struct ath12k_cfir_enh_dma_hdr));

	meta = &lut->header.meta_enh;

	if (capture_type != CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM) {
		if (!dma_hdr.mu_rx_data_incl) {
			peer_macaddr = meta->peer_addr;
			if (dma_hdr.freeze_data_incl)
				extract_peer_mac_from_freeze_tlv(freeze_tlv,
								 peer_macaddr);
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_cfr_parse_enh_dma_hdr);

/* Generation-agnostic CFR DBR (direct-buffer-rx) event handler.
 * Delegates the ucode-specific DMA header parsing to
 * ab->hw_params->hw_ops->parse_cfr_enh_dma_hdr(), which fills in the LUT
 * entry's header/metadata fields and returns the total data length.
 * Locking, LUT lookup, correlate-and-relay, and RFS write stay here since
 * they do not vary by chip generation.
 */
static int ath12k_cfr_process_dbr_data(struct ath12k *ar,
				       struct ath12k_dbring_data *param)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut;
	u32 buf_id;
	u32 length = 0;
	u32 end_magic = ATH12K_CFR_END_MAGIC;
	int ret = 0;
	int status;

	buf_id = param->buf_id;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut = &cfr->lut[buf_id];
	if (!lut) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "lut failure to process cfr data id:%d\n", buf_id);
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	ret = ab->hw_params->hw_ops->parse_cfr_enh_dma_hdr(ar, param->data,
							   lut, &length);
	if (ret) {
		spin_unlock_bh(&cfr->lut_lock);
		return ret;
	}

	ath12k_dbg_dump(ab, ATH12K_DBG_CFR_DUMP, "data_from_buf_rel:", "",
			param->data, length);

	lut->buff = param->buff;
	lut->data = param->data;
	lut->data_len = length;
	lut->dbr_tstamp = jiffies;

	status = ath12k_cfr_correlate_and_relay(ar, lut,
						ATH12K_CORRELATE_DBR_EVENT);

	if (status == ATH12K_CORRELATE_STATUS_RELEASE) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "releasing CFR data to user space");
		if (!lut->header_len)
			lut->header_len =
				ATH12K_CFR_HEADER_LEN(lut->header.meta_enh.num_mu_users);

		ath12k_cfr_rfs_write(ar, &lut->header, lut->header_len,
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		ath12k_cfr_release_lut_entry(lut);
		ret = ATH12K_CORRELATE_STATUS_RELEASE;
	} else if (status == ATH12K_CORRELATE_STATUS_HOLD) {
		ret = ATH12K_CORRELATE_STATUS_HOLD;
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "tx event is not yet received holding the buf");
	} else {
		ret = ATH12K_CORRELATE_STATUS_ERR;
		ath12k_err(ab, "error in processing buf rel event");
	}

	spin_unlock_bh(&cfr->lut_lock);

	return ret;
}

struct ath12k_cfr_vif_iter {
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
};

static void ath12k_cfr_get_first_arvif_iter(void *data, u8 *mac,
					    struct ieee80211_vif *vif)
{
	struct ath12k_cfr_vif_iter *arvif_iter = data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	unsigned long links_map = ahvif->links_map;
	struct ath12k_link_vif *arvif;
	u8 link_id;

	if (arvif_iter->arvif)
		return;

	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		arvif = rcu_dereference(ahvif->link[link_id]);
		if (!arvif)
			continue;

		if (arvif->ar == arvif_iter->ar) {
			arvif_iter->arvif = arvif;
			return;
		}
	}
}

static struct ath12k_link_vif *ath12k_cfr_get_first_arvif(struct ath12k *ar)
{
	struct ath12k_cfr_vif_iter arvif_iter = {};
	u32 flags;

	/* To use the arvif returned, caller must have held rcu read lock. */
	WARN_ON(!rcu_read_lock_held());

	arvif_iter.ar = ar;

	flags = IEEE80211_IFACE_ITER_RESUME_ALL;
	ieee80211_iterate_active_interfaces_atomic(ath12k_ar_to_hw(ar),
						   flags,
						   ath12k_cfr_get_first_arvif_iter,
						   &arvif_iter);

	return arvif_iter.arvif;
}

static enum wmi_phy_mode ath12k_cfr_chan_to_phymode(struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	const struct cfg80211_chan_def *def = &arvif->chanctx.def;
	struct ieee80211_bss_conf *link_conf;
	enum wmi_phy_mode phymode = MODE_UNKNOWN;

	if (!def->chan)
		return MODE_UNKNOWN;

	link_conf = rcu_dereference(vif->link_conf[arvif->link_id]);
	if (!link_conf)
		return MODE_UNKNOWN;

	switch (def->chan->band) {
	case NL80211_BAND_2GHZ:
		switch (def->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
			if (def->chan->flags & IEEE80211_CHAN_NO_OFDM)
				phymode = MODE_11B;
			else
				phymode = MODE_11G;
			break;
		case NL80211_CHAN_WIDTH_20:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR20_2G;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT20_2G;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE20_2G;
			else if (arvif->vht_cap)
				phymode = MODE_11AC_VHT20_2G;
			else
				phymode = MODE_11NG_HT20;
			break;
		case NL80211_CHAN_WIDTH_40:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR40_2G;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT40_2G;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE40_2G;
			else if (arvif->vht_cap)
				phymode = MODE_11AC_VHT40_2G;
			else
				phymode = MODE_11NG_HT40;
			break;
		default:
			break;
		}
		break;
	case NL80211_BAND_5GHZ:
		switch (def->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
			phymode = MODE_11A;
			break;
		case NL80211_CHAN_WIDTH_20:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR20;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT20;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE20;
			else if (arvif->vht_cap)
				phymode = MODE_11AC_VHT20;
			else
				phymode = MODE_11NA_HT20;
			break;
		case NL80211_CHAN_WIDTH_40:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR40;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT40;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE40;
			else if (arvif->vht_cap)
				phymode = MODE_11AC_VHT40;
			else
				phymode = MODE_11NA_HT40;
			break;
		case NL80211_CHAN_WIDTH_80:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR80;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT80;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE80;
			else
				phymode = MODE_11AC_VHT80;
			break;
		case NL80211_CHAN_WIDTH_160:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR160;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT160;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE160;
			else
				phymode = MODE_11AC_VHT160;
			break;
		case NL80211_CHAN_WIDTH_80P80:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR80_80;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT80_80;
			else if (link_conf->he_support)
				phymode = MODE_11AX_HE80_80;
			else
				phymode = MODE_11AC_VHT80_80;
			break;
		case NL80211_CHAN_WIDTH_320:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR320;
			else
				phymode = MODE_11BE_EHT320;
			break;
		default:
			break;
		}
		break;
	case NL80211_BAND_6GHZ:
		switch (def->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
		case NL80211_CHAN_WIDTH_20:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR20;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT20;
			else
				phymode = MODE_11AX_HE20;
			break;
		case NL80211_CHAN_WIDTH_40:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR40;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT40;
			else
				phymode = MODE_11AX_HE40;
			break;
		case NL80211_CHAN_WIDTH_80:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR80;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT80;
			else
				phymode = MODE_11AX_HE80;
			break;
		case NL80211_CHAN_WIDTH_160:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR160;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT160;
			else
				phymode = MODE_11AX_HE160;
			break;
		case NL80211_CHAN_WIDTH_80P80:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR80_80;
			else if (link_conf->eht_support)
				phymode = MODE_11BE_EHT80_80;
			else
				phymode = MODE_11AX_HE80_80;
			break;
		case NL80211_CHAN_WIDTH_320:
			if (link_conf->uhr_support)
				phymode = MODE_11BN_UHR320;
			else
				phymode = MODE_11BE_EHT320;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	WARN_ON(phymode == MODE_UNKNOWN);
	return phymode;
}

static bool ath12k_cfr_is_valid_gain_table_idx(const struct ath12k_cfr *cfr,
					       u16 tbl_idx)
{
	/* default gain table is always valid */
	if (!tbl_idx)
		return true;

	/* If target did not share aoa caldata and max gain table details,
	 * do not derive phase delta locally. just return false
	 */
	if (!cfr->is_enh_aoa_data)
		return false;

	return tbl_idx < cfr->enh_aoa_data.max_agc_gain_tbls;
}

static u16 ath12k_cfr_get_max_agc_gain(const struct ath12k_cfr *cfr,
				       u16 center_freq1, u16 tbl_idx)
{
	const struct cfr_enhanced_aoa_data *aoa = &cfr->enh_aoa_data;
	const u16 *max_agc_gain_per_tbl;

	if (!cfr->is_enh_aoa_data)
		return ATH12K_CFR_INVALID_AGC_GAIN;

	if (center_freq1 < 3000)
		max_agc_gain_per_tbl = aoa->max_agc_gain_per_tbl_2g;
	else if (center_freq1 >= 5925)
		max_agc_gain_per_tbl = aoa->max_agc_gain_per_tbl_6g;
	else
		max_agc_gain_per_tbl = aoa->max_agc_gain_per_tbl_5g;

	if (ath12k_cfr_is_valid_gain_table_idx(cfr, tbl_idx))
		return max_agc_gain_per_tbl[tbl_idx];

	return ATH12K_CFR_INVALID_AGC_GAIN;
}

static void
ath12k_cfr_fill_rx_tlv_agc_info(const struct hal_rx_ppdu_cfr_info *cfr_info,
				struct ath12k_cfr_peer_tx_param *params)
{
	u16 gain_info[HOST_MAX_CHAINS];
	int i;

	gain_info[0] = (u16)u32_get_bits(cfr_info->agc_gain_info0,
					 ATH12K_CFR_GAIN_INFO_L_U16);
	gain_info[1] = (u16)u32_get_bits(cfr_info->agc_gain_info0,
					 ATH12K_CFR_GAIN_INFO_M_U16);
	gain_info[2] = (u16)u32_get_bits(cfr_info->agc_gain_info1,
					 ATH12K_CFR_GAIN_INFO_L_U16);
	gain_info[3] = (u16)u32_get_bits(cfr_info->agc_gain_info1,
					 ATH12K_CFR_GAIN_INFO_M_U16);
	gain_info[4] = (u16)u32_get_bits(cfr_info->agc_gain_info2,
					 ATH12K_CFR_GAIN_INFO_L_U16);
	gain_info[5] = (u16)u32_get_bits(cfr_info->agc_gain_info2,
					 ATH12K_CFR_GAIN_INFO_M_U16);
	gain_info[6] = (u16)u32_get_bits(cfr_info->agc_gain_info3,
					 ATH12K_CFR_GAIN_INFO_L_U16);
	gain_info[7] = (u16)u32_get_bits(cfr_info->agc_gain_info3,
					 ATH12K_CFR_GAIN_INFO_M_U16);

	for (i = 0; i < min_t(u8, HOST_MAX_CHAINS, WMI_MAX_CHAINS); i++) {
		params->agc_gain[i] = u16_get_bits(gain_info[i], ATH12K_CFR_GAIN_DB);
		params->agc_gain_tbl_index[i] = u16_get_bits(gain_info[i],
							     ATH12K_CFR_GAIN_TABLE_IDX);
	}
}

static void
ath12k_cfr_populate_enh_chain_phase(struct ath12k *ar,
				    struct ath12k_cfr_peer_tx_param *params,
				    u16 center_freq1)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_enhanced_aoa_data *aoa = &cfr->enh_aoa_data;
	u16 *phase_array, *gain_array;
	u32 max_chains = min_t(u32, HOST_MAX_CHAINS, WMI_MAX_CHAINS);
	u32 chain_limit;
	u32 start_ent, stop_ent, chain, tbl_idx, grp_stp_idx;
	u32 data_idx, rf_chain;
	u16 max_agc_gain, phase_delta;
	bool found;
	bool invalid_gain_table_idx = false;

	for (chain = 0; chain < max_chains; chain++) {
		tbl_idx = params->agc_gain_tbl_index[chain];
		if (!ath12k_cfr_is_valid_gain_table_idx(cfr, tbl_idx))
			invalid_gain_table_idx = true;

		/* cap the agc gain if it crosses max allowed gain value for the
		 * given gain table index.
		 */
		max_agc_gain = ath12k_cfr_get_max_agc_gain(cfr, center_freq1, tbl_idx);
		if (max_agc_gain != ATH12K_CFR_INVALID_AGC_GAIN &&
		    params->agc_gain[chain] > max_agc_gain)
			params->agc_gain[chain] = max_agc_gain;
	}

	/* When invalid gain table index reported do not populate the phase delta.
	 * And when aoa caldata is not shared via WMI event as well do not populate the
	 * chain phase as it means either target does not support enhanced AoA or it
	 * reports the data via dma header
	 */
	if (invalid_gain_table_idx || !cfr->is_enh_aoa_data ||
	    !aoa->gain_stop_index_array || !aoa->enh_phase_delta_array ||
	    !aoa->max_entries_all_table) {
		for (chain = 0; chain < max_chains; chain++)
			params->chain_phase[chain] = ATH12K_CFR_INVALID_PHASE_DELTA;
		return;
	}

	chain_limit = min_t(u32, max_chains, aoa->max_aoa_chains);
	for (chain = 0; chain < chain_limit; chain++) {
		tbl_idx = params->agc_gain_tbl_index[chain];

		if (tbl_idx >= aoa->max_agc_gain_tbls ||
		    tbl_idx >= ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS)
			continue;

		rf_chain = aoa->xbar_config ?
			   ((aoa->xbar_config >> (3 * chain)) & 0x07) : chain;
		if (rf_chain >= WMI_MAX_CHAINS)
			continue;

		data_idx = rf_chain * aoa->max_entries_all_table;
		phase_array = &aoa->enh_phase_delta_array[data_idx];
		gain_array = &aoa->gain_stop_index_array[data_idx];
		start_ent = aoa->start_ent[tbl_idx];
		if (start_ent >= aoa->max_entries_all_table)
			continue;

		stop_ent = start_ent + aoa->max_bdf_entries_per_tbl[tbl_idx];
		stop_ent = min_t(u32, stop_ent, aoa->max_entries_all_table);

		/* If default gain table exceeds max_agc_gain, chain_phase needs to be
		 * considered as 0. Remaining gain tables would have a phase delta
		 * assigned with max agc gain as well
		 */
		max_agc_gain = ath12k_cfr_get_max_agc_gain(cfr, center_freq1, tbl_idx);
		if (!tbl_idx && max_agc_gain != ATH12K_CFR_INVALID_AGC_GAIN &&
		    params->agc_gain[chain] == max_agc_gain) {
			phase_delta = 0;
			params->chain_phase[chain] =
				(aoa->ibf_cal_val[rf_chain] + phase_delta) & 0x3FF;
			continue;
		}

		found = false;
		for (grp_stp_idx = start_ent; grp_stp_idx < stop_ent; grp_stp_idx++) {
			if (params->agc_gain[chain] <= gain_array[grp_stp_idx]) {
				phase_delta = phase_array[grp_stp_idx];
				found = true;
				break;
			}
		}

		if (!found)
			phase_delta = 0;

		if (phase_delta == ATH12K_CFR_INVALID_PHASE_DELTA)
			params->chain_phase[chain] =
				ATH12K_CFR_INVALID_PHASE_DELTA;
		else
			params->chain_phase[chain] =
				(aoa->ibf_cal_val[rf_chain] + phase_delta) & 0x3FF;
	}
}

static void
ath12k_cfr_fill_rx_tlv_chain_phase_info(struct ath12k *ar,
					struct ath12k_cfr_peer_tx_param *params,
					u16 center_freq1)
{
	/* for enhanced aoa data supported target */
	ath12k_cfr_populate_enh_chain_phase(ar, params, center_freq1);
}

static int ath12k_cfr_process_rx_tlv_ppdu(struct ath12k *ar,
					  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut = NULL, *temp = NULL;
	struct ath12k_dbring_element *buff;
	struct ath12k_cfr_peer_tx_param params = {0};
	struct ath12k_csi_cfr_header *header;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_dp_link_peer *link_peer;
	const struct hal_rx_ppdu_cfr_info *cfr_info = &ppdu_info->cfr_info;
	dma_addr_t buf_addr;
	u32 end_magic = ATH12K_CFR_END_MAGIC;
	u16 center_freq1 = ppdu_info->freq;
	u16 center_freq2 = 0;
	u16 puncture_bitmap = ppdu_info->punctured_pattern;
	enum wmi_phy_mode cfr_phymode = MODE_UNKNOWN;
	u8 num_mu_users = 0;
	u32 i;
	int ret = 0;
	int status;
	int lut_idx = -1;

	if (!cfr_info->bb_captured_channel)
		return 0;

	if (!test_bit(WMI_SERVICE_CFR_CAPTURE_FILTER_SUPPORT, ab->wmi_ab.svc_map))
		return 0;

	buf_addr = cfr_info->rtt_che_buffer_pointer_low32 |
		   (((u64)(cfr_info->rtt_che_buffer_pointer_high8 & 0xf))
		    << 32);

	if (!buf_addr)
		return -EINVAL;

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(ab->dp, NULL,
							     ppdu_info->peer_id);
	if (link_peer && (link_peer->peer_id != ppdu_info->peer_id ||
			  link_peer->pdev_idx != ar->pdev_idx))
		link_peer = NULL;

	if (cfr->rcc_param.vdev_id != 0xff)
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, cfr->rcc_param.vdev_id);
	else
		arvif = ath12k_cfr_get_first_arvif(ar);

	if (arvif && arvif->ar != ar)
		arvif = NULL;

	if (!arvif && link_peer)
		arvif = ath12k_mac_get_arvif(ar, link_peer->vdev_id);

	if (!arvif) {
		rcu_read_unlock();
		return -ENOENT;
	}

	if (arvif && arvif->chanctx.def.chan) {
		center_freq1 = arvif->chanctx.def.center_freq1;
		center_freq2 = arvif->chanctx.def.center_freq2;
		puncture_bitmap = arvif->chanctx.def.punctured;
	}

	if (link_peer)
		ether_addr_copy(params.peer_mac_addr, link_peer->addr);
	else
		ether_addr_copy(params.peer_mac_addr, ppdu_info->addr2);

	cfr_phymode = ath12k_cfr_chan_to_phymode(arvif);

	rcu_read_unlock();

	params.status = WMI_CFR_PEER_CAPTURE_STATUS;
	params.bandwidth = ppdu_info->bw;
	params.phy_mode = cfr_phymode;
	params.band_center_freq1 = center_freq1;
	params.band_center_freq2 = center_freq2;
	params.cfo_measurement = cfr_info->rtt_cfo_measurement;
	params.rx_start_ts = cfr_info->rx_start_ts;
	params.mcs_rate = cfr_info->mcs_rate;
	params.gi_type = cfr_info->gi_type;

	ath12k_cfr_fill_rx_tlv_agc_info(cfr_info, &params);
	ath12k_cfr_fill_rx_tlv_chain_phase_info(ar, &params, center_freq1);

	for (i = 0; i < ARRAY_SIZE(ppdu_info->rssi_db_per_chain_pri20); i++) {
		params.chain_rssi[i] =
		ath12k_cfr_snr_to_signal_strength(ppdu_info->rssi_db_per_chain_pri20[i]);
	}

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		ret = -EINVAL;
		goto unlock;
	}

	for (i = 0; i < cfr->lut_num; i++) {
		temp = &cfr->lut[i];
		if (temp->dbr_address == buf_addr) {
			lut = &cfr->lut[i];
			lut_idx = i;
			break;
		}
	}

	if (!lut) {
		cfr->tx_dbr_lookup_fail++;
		ret = -EINVAL;
		goto unlock;
	}

	lut->tx_ppdu_id = ppdu_info->ppdu_id;
	lut->tx_address1 = cfr_info->rtt_che_buffer_pointer_low32;
	lut->tx_address2 = cfr_info->rtt_che_buffer_pointer_high8;
	lut->txrx_tstamp = jiffies;

	if (ppdu_info->reception_type != HAL_RECEPTION_TYPE_SU)
		num_mu_users = min_t(u8, ppdu_info->num_users,
				     (u8)cfr->max_mu_users);

	header = &lut->header;
	ab->hw_params->hw_ops->fill_cfr_hdr_info(ar, header, &params);
	num_mu_users = min_t(u8, num_mu_users, ATH12K_CFR_MAX_MU_USERS);
	header->cfr_metadata_len = ATH12K_CFR_META_LEN(num_mu_users);
	header->meta_enh.num_mu_users = num_mu_users;
	ath12k_cfr_fill_peer_addr(header, ppdu_info, num_mu_users,
				  params.peer_mac_addr);
	lut->header_len = ATH12K_CFR_HEADER_LEN(num_mu_users);
	header->meta_enh.puncture_bitmap = puncture_bitmap;
	header->meta_enh.beamformed = ppdu_info->beamformed;

	cfr->rx_tlv_evt_cnt++;

	status = ath12k_cfr_correlate_and_relay(ar, lut,
						ATH12K_CORRELATE_TX_EVENT);
	if (status == ATH12K_CORRELATE_STATUS_RELEASE) {
		ath12k_cfr_rfs_write(ar, &lut->header, lut->header_len,
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		spin_lock_bh(&cfr->rx_ring.idr_lock);
		buff = idr_find(&cfr->rx_ring.bufs_idr, lut_idx);
		spin_unlock_bh(&cfr->rx_ring.idr_lock);
		if (!buff) {
			ret = -ENOENT;
			goto unlock;
		}

		ath12k_cfr_release_lut_entry(lut);
		ath12k_dbring_remove_buf_id(&cfr->rx_ring, lut_idx);
		ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
	} else {
		ret = -EINVAL;
	}

unlock:
	spin_unlock_bh(&cfr->lut_lock);
	return ret;
}

static int ath12k_cfr_ppdu_rx_notifier(struct notifier_block *nb,
				       unsigned long val, void *v)
{
	struct ath12k_cfr *cfr = container_of(nb, struct ath12k_cfr,
					      ppdu_rx_notifier);
	struct ath12k *ar = container_of(cfr, struct ath12k, cfr);
	struct ath12k_ppdu_event *event = v;
	struct ath12k_ppdu_rx_info *rx_evt;

	if (val != ATH12K_EVENT_PPDU_RX_COMPLETE || !event || !event->skb)
		return NOTIFY_DONE;

	if (event->skb->len < sizeof(*rx_evt))
		return NOTIFY_DONE;

	rx_evt = (struct ath12k_ppdu_rx_info *)event->skb->data;

	if (rx_evt->ppdu_info.device_id != ath12k_get_ab_device_id(ar->ab))
		return NOTIFY_DONE;

	ath12k_cfr_update_rcc_debug_stats(ar, &rx_evt->ppdu_info);
	ath12k_cfr_process_rx_tlv_ppdu(ar, &rx_evt->ppdu_info);

	return NOTIFY_DONE;
}

/**
 * ath12k_register_ppdu_notifier - Register PPDU event listener
 * @nb: Notifier block with callback
 * @event_mask: Bitmask of events (1 << ATH12K_EVENT_PPDU_RX_COMPLETE)
 *              or (1 << ATH12K_EVENT_PPDU_TX_COMPLETE) or both
 *
 * Registers a listener for PPDU completion events. The callback
 * will be invoked synchronously from atomic context.
 *
 * Return: 0 on success, negative on failure
 */
static int
ath12k_register_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "PPDU notifier register, mask=0x%lx\n",
		   event_mask);

	ret = atomic_notifier_chain_register(&ath12k_ppdu_notifier_chain, nb);

	return ret;
}

/**
 * ath12k_unregister_ppdu_notifier - Unregister PPDU event listener
 * @nb: Notifier block to remove
 * @event_mask: Same bitmask used during registration
 *
 * Return: 0 on success, negative on failure
 */
static int
ath12k_unregister_ppdu_notifier(struct notifier_block *nb, unsigned long event_mask)
{
	int ret;

	if (!nb)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "PPDU notifier unregister, mask=0x%lx\n",
		   event_mask);
	ret = atomic_notifier_chain_unregister(&ath12k_ppdu_notifier_chain, nb);

	return ret;
}

int ath12k_cfr_register_ppdu_rx_notifier(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	int ret;

	if (cfr->ppdu_rx_notifier_registered)
		return 0;

	cfr->ppdu_rx_notifier.notifier_call = ath12k_cfr_ppdu_rx_notifier;
	ret = ath12k_register_ppdu_notifier(&cfr->ppdu_rx_notifier,
					    BIT(ATH12K_EVENT_PPDU_RX_COMPLETE));
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to register CFR RX PPDU notifier for pdev %d: %d\n",
			    ar->pdev->pdev_id, ret);
		return ret;
	}

	cfr->ppdu_rx_notifier_registered = true;
	return 0;
}

int ath12k_cfr_unregister_ppdu_rx_notifier(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	int ret;

	if (!cfr->ppdu_rx_notifier_registered)
		return 0;

	ret = ath12k_unregister_ppdu_notifier(&cfr->ppdu_rx_notifier,
					      BIT(ATH12K_EVENT_PPDU_RX_COMPLETE));
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to unregister CFR RX PPDU notifier for pdev %d: %d\n",
			    ar->pdev->pdev_id, ret);
		return ret;
	}

	cfr->ppdu_rx_notifier_registered = false;
	return 0;
}

int ath12k_process_cfr_capture_event(struct ath12k_base *ab,
				     struct ath12k_cfr_peer_tx_param *params)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	struct ath12k_link_vif *arvif;
	struct ath12k_cfr_look_up_table *lut = NULL, *temp = NULL;
	struct ath12k_dbring_element *buff;
	struct ath12k_csi_cfr_header *header;
	dma_addr_t buf_addr;
	u32 end_magic = ATH12K_CFR_END_MAGIC;
	u8 tx_status;
	int ret = 0;
	int status;
	int i;
	int lut_idx = -1;

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, params->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "Failed to get arvif for vdev id %d\n",
			    params->vdev_id);
		rcu_read_unlock();
		return -ENOENT;
	}

	ar = arvif->ar;
	cfr = &ar->cfr;
	rcu_read_unlock();

	if (WMI_CFR_CAPTURE_STATUS_PEER_PS & params->status) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR capture failed as peer %pM is in powersave",
			   params->peer_mac_addr);
		return -EINVAL;
	}

	if (!(WMI_CFR_PEER_CAPTURE_STATUS & params->status)) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR capture failed for the peer : %pM",
			   params->peer_mac_addr);
		cfr->tx_peer_status_cfr_fail++;
		return -EINVAL;
	}

	tx_status = FIELD_GET(WMI_CFR_FRAME_TX_STATUS, params->status);

	if (tx_status != WMI_FRAME_TX_STATUS_OK) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "WMI tx status %d for the peer %pM",
			   tx_status, params->peer_mac_addr);
		cfr->tx_evt_status_cfr_fail++;
		return -EINVAL;
	}

	buf_addr = (((u64)FIELD_GET(WMI_CFR_CORRELATION_INFO2_BUF_ADDR_HIGH,
				    params->correlation_info_2)) << 32) |
		   params->correlation_info_1;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	for (i = 0; i < cfr->lut_num; i++) {
		temp = &cfr->lut[i];
		if (temp->dbr_address == buf_addr) {
			lut = &cfr->lut[i];
			lut_idx = i;
			break;
		}
	}

	if (!lut) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "lut failure to process tx event\n");
		cfr->tx_dbr_lookup_fail++;
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut->tx_ppdu_id = FIELD_GET(WMI_CFR_CORRELATION_INFO2_PPDU_ID,
				    params->correlation_info_2);
	lut->tx_address1 = params->correlation_info_1;
	lut->tx_address2 = params->correlation_info_2;
	lut->txrx_tstamp = jiffies;

	header = &lut->header;
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;

	ab->hw_params->hw_ops->fill_cfr_hdr_info(ar, header, params);
	lut->header_len = ATH12K_CFR_HEADER_LEN(0);
	header->meta_enh.puncture_bitmap = arvif->chanctx.def.punctured;

	cfr->tx_evt_cnt++;

	status = ath12k_cfr_correlate_and_relay(ar, lut,
						ATH12K_CORRELATE_TX_EVENT);
	if (status == ATH12K_CORRELATE_STATUS_RELEASE) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "Releasing CFR data to user space");
		ath12k_cfr_rfs_write(ar, &lut->header, lut->header_len,
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		spin_lock_bh(&cfr->rx_ring.idr_lock);
		buff = idr_find(&cfr->rx_ring.bufs_idr, lut_idx);
		if (!buff) {
			spin_unlock_bh(&cfr->rx_ring.idr_lock);
			ret = -ENOENT;
			goto out_unlock;
		}
		spin_unlock_bh(&cfr->rx_ring.idr_lock);

		ath12k_cfr_release_lut_entry(lut);
		ath12k_dbring_remove_buf_id(&cfr->rx_ring, lut_idx);

		ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
	} else {
		ret = -EINVAL;
	}

out_unlock:
	spin_unlock_bh(&cfr->lut_lock);
	return ret;
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

static struct rchan_callbacks rfs_cfr_capture_cb = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

static ssize_t ath12k_read_file_enable_cfr(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32] = {0};
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%d\n", ar->cfr.cfr_enabled);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_file_enable_cfr(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool enable_cfr = false;
	int ret;

	if (kstrtobool_from_user(ubuf, count, &enable_cfr))
		return -EINVAL;

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (ar->cfr.cfr_enabled == enable_cfr) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PER_PEER_CFR_ENABLE,
					enable_cfr, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to enable/disable per peer cfr (%d)\n",
			    ret);
		goto out;
	}

	ar->cfr.cfr_enabled = enable_cfr;
	ret = count;
out:
	return ret;
}

static const struct file_operations fops_enable_cfr = {
	.read = ath12k_read_file_enable_cfr,
	.write = ath12k_write_file_enable_cfr,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_file_cfr_unassoc(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	char buf[64] = {0};
	u8 peer_mac[6];
	u32 cfr_capture_enable;
	u32 cfr_capture_period;
	int available_idx = -1;
	int ret, i;

	simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);

	spin_lock_bh(&cfr->lock);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (!ar->cfr.cfr_enabled) {
		ret = -EINVAL;
		ath12k_err(ar->ab, "CFR is not enabled on this pdev %d\n",
			   ar->pdev_idx);
		goto out;
	}

	ret = sscanf(buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx %u %u",
		     &peer_mac[0], &peer_mac[1], &peer_mac[2], &peer_mac[3],
		     &peer_mac[4], &peer_mac[5], &cfr_capture_enable,
		     &cfr_capture_period);

	if (ret < 1) {
		ret = -EINVAL;
		goto out;
	}

	if (cfr_capture_enable && ret != 8) {
		ret = -EINVAL;
		goto out;
	}

	if (!cfr_capture_enable) {
		for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
			entry = &cfr->unassoc_pool[i];
			if (ether_addr_equal(peer_mac, entry->peer_mac)) {
				memset(entry->peer_mac, 0, ETH_ALEN);
				entry->is_valid = false;
				cfr->cfr_enabled_peer_cnt--;
			}
		}

		ret = count;
		goto out;
	}


	if (cfr->cfr_enabled_peer_cnt >= ATH12K_MAX_CFR_ENABLED_CLIENTS) {
		ath12k_info(ar->ab, "Max cfr peer threshold reached\n");
		ret = count;
		goto out;
	}

	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];

		if ((available_idx < 0) && !entry->is_valid)
			available_idx = i;

		if (ether_addr_equal(peer_mac, entry->peer_mac)) {
			ath12k_info(ar->ab,
				    "peer entry already present updating params\n");
			entry->period = cfr_capture_period;
			ret = count;
			goto out;
		}
	}

	if (available_idx >= 0) {
		entry = &cfr->unassoc_pool[available_idx];
		ether_addr_copy(entry->peer_mac, peer_mac);
		entry->period = cfr_capture_period;
		entry->is_valid = true;
		cfr->cfr_enabled_peer_cnt++;
	}

	ret = count;
out:
	spin_unlock_bh(&cfr->lock);
	return ret;
}

static ssize_t ath12k_read_file_cfr_unassoc(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char buf[512] = {0};
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int len = 0, i;

	spin_lock_bh(&cfr->lock);

	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];
		if (entry->is_valid)
			len += scnprintf(buf + len, sizeof(buf) - len,
					 "peer: %pM period: %u\n",
					 entry->peer_mac, entry->period);
	}

	spin_unlock_bh(&cfr->lock);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_configure_cfr_unassoc = {
	.write = ath12k_write_file_cfr_unassoc,
	.read = ath12k_read_file_cfr_unassoc,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void
ath12k_cfr_update_rcc_debug_stats(struct ath12k *ar,
				  const struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	const struct hal_rx_ppdu_cfr_info *cfr_info = &ppdu_info->cfr_info;

	spin_lock_bh(&cfr->lock);

	if (cfr_info->chan_capture_status < ATH12K_CFR_CAPTURE_MAX)
		cfr->rcc_stats.chan_capture_status[cfr_info->chan_capture_status]++;

	if (cfr_info->rx_location_info_valid) {
		cfr->rcc_stats.rx_loc_info_valid_cnt++;

		if (cfr_info->bb_captured_channel) {
			cfr->rcc_stats.bb_captured_channel_cnt++;
			if (cfr_info->bb_captured_reason < FREEZE_REASON_MAX)
				cfr->rcc_stats.reason_cnt[cfr_info->bb_captured_reason]++;
		} else if (cfr_info->bb_captured_timeout) {
			cfr->rcc_stats.bb_captured_timeout_cnt++;
			if (cfr_info->bb_captured_reason < FREEZE_REASON_MAX)
				cfr->rcc_stats.reason_cnt[cfr_info->bb_captured_reason]++;
		}
	}

	spin_unlock_bh(&cfr->lock);
}

static const char *
ath12k_cfr_rcc_capture_status_to_str(u32 capture_status)
{
	switch (capture_status) {
	case ATH12K_CFR_CAPTURE_IDLE:
		return "CAPTURE_IDLE";
	case ATH12K_CFR_CAPTURE_BUSY:
		return "CAPTURE_BUSY";
	case ATH12K_CFR_CAPTURE_ACTIVE:
		return "CAPTURE_ACTIVE";
	case ATH12K_CFR_CAPTURE_NO_BUFFER:
		return "CAPTURE_NO_BUFFER";
	default:
		return "UNKNOWN";
	}
}

static const char *
ath12k_cfr_rcc_freeze_reason_to_str(u32 freeze_reason)
{
	switch (freeze_reason) {
	case FREEZE_REASON_TM:
		return "FREEZE_REASON_TM";
	case FREEZE_REASON_FTM:
		return "FREEZE_REASON_FTM";
	case FREEZE_REASON_ACK_RESP_TO_TM_FTM:
		return "FREEZE_REASON_ACK_RESP_TO_TM_FTM";
	case FREEZE_REASON_TA_RA_TYPE_FILTER:
		return "FREEZE_REASON_TA_RA_TYPE_FILTER";
	case FREEZE_REASON_NDPA_NDP:
		return "FREEZE_REASON_NDPA_NDP";
	case FREEZE_REASON_ALL_PACKET:
		return "FREEZE_REASON_ALL_PACKET";
	default:
		return "UNKNOWN";
	}
}

static int
ath12k_cfr_append_rcc_debug_stats(struct ath12k_cfr *cfr,
				  char *buf, size_t size)
{
	int len = 0;
	u32 i;

	spin_lock_bh(&cfr->lock);

	len += scnprintf(buf + len, size - len,
			 "bb_captured_channel_cnt: %llu\n",
			 cfr->rcc_stats.bb_captured_channel_cnt);
	len += scnprintf(buf + len, size - len,
			 "bb_captured_timeout_cnt: %llu\n",
			 cfr->rcc_stats.bb_captured_timeout_cnt);
	len += scnprintf(buf + len, size - len,
			 "rx_loc_info_valid_cnt: %llu\n",
			 cfr->rcc_stats.rx_loc_info_valid_cnt);

	len += scnprintf(buf + len, size - len, "Channel capture status:\n");
	for (i = 0; i < ATH12K_CFR_CAPTURE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s = %llu\n",
				 ath12k_cfr_rcc_capture_status_to_str(i),
				 cfr->rcc_stats.chan_capture_status[i]);

	len += scnprintf(buf + len, size - len, "Freeze reason:\n");
	for (i = 0; i < FREEZE_REASON_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s = %llu\n",
				 ath12k_cfr_rcc_freeze_reason_to_str(i),
				 cfr->rcc_stats.reason_cnt[i]);

	spin_unlock_bh(&cfr->lock);

	return len;
}

static ssize_t ath12k_read_file_rcc_debug_counters(struct file *file,
						   char __user *user_buf,
						   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;
	char *buf;
	const size_t buf_size = 4096;
	ssize_t ret;
	int len = 0;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock_bh(&cfr->lut_lock);

	len += scnprintf(buf + len, buf_size - len,
			 "dbr_evt_cnt = %llu\n", cfr->dbr_evt_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "tx_evt_cnt = %llu\n", cfr->tx_evt_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "rx_tlv_evt_cnt = %llu\n", cfr->rx_tlv_evt_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "release_cnt = %llu\n", cfr->release_cnt);
	len += scnprintf(buf + len, buf_size - len, "Error cnt:\n");
	len += scnprintf(buf + len, buf_size - len,
			 "flush_dbr_cnt = %llu\n", cfr->flush_dbr_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "invalid_dma_length_cnt = %llu\n",
			 cfr->invalid_dma_length_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "flush_timeout_dbr_cnt = %llu\n",
			 cfr->flush_timeout_dbr_cnt);
	len += scnprintf(buf + len, buf_size - len,
			 "tx_dbr_cookie_lookup_fail = %llu\n",
			 cfr->tx_dbr_lookup_fail);
	len += scnprintf(buf + len, buf_size - len,
			 "clear_txrx_event = %llu\n", cfr->clear_txrx_event);
	len += scnprintf(buf + len, buf_size - len,
			 "cfr_dma_aborts = %llu\n", cfr->cfr_dma_aborts);

	spin_unlock_bh(&cfr->lut_lock);

	len += scnprintf(buf + len, buf_size - len,
			 "tx_peer_status_cfr_fail = %llu\n",
			 cfr->tx_peer_status_cfr_fail);
	len += scnprintf(buf + len, buf_size - len,
			 "tx_evt_status_cfr_fail = %llu\n",
			 cfr->tx_evt_status_cfr_fail);

	len += ath12k_cfr_append_rcc_debug_stats(cfr, buf + len,
						 buf_size - len);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t ath12k_write_file_clr_dbg_counters(struct file *file,
						  const char __user *ubuf,
						  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;

	if (!count)
		return -EINVAL;

	spin_lock_bh(&cfr->lut_lock);

	cfr->tx_evt_cnt = 0;
	cfr->dbr_evt_cnt = 0;
	cfr->rx_tlv_evt_cnt = 0;
	cfr->release_cnt = 0;
	cfr->tx_dbr_lookup_fail = 0;
	cfr->flush_dbr_cnt = 0;
	cfr->invalid_dma_length_cnt = 0;
	cfr->clear_txrx_event = 0;
	cfr->cfr_dma_aborts = 0;
	cfr->flush_timeout_dbr_cnt = 0;

	spin_unlock_bh(&cfr->lut_lock);

	cfr->tx_peer_status_cfr_fail = 0;
	cfr->tx_evt_status_cfr_fail = 0;

	spin_lock_bh(&cfr->lock);
	memset(&cfr->rcc_stats, 0, sizeof(cfr->rcc_stats));
	spin_unlock_bh(&cfr->lock);

	return count;
}

static const struct file_operations fops_rcc_debug_counters = {
	.read = ath12k_read_file_rcc_debug_counters,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_clr_dbg_counters = {
	.write = ath12k_write_file_clr_dbg_counters,
	.open = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static inline void ath12k_cfr_debug_unregister(struct ath12k *ar)
{
	debugfs_remove(ar->cfr.enable_cfr);
	ar->cfr.enable_cfr = NULL;
	debugfs_remove(ar->cfr.cfr_unassoc);
	ar->cfr.cfr_unassoc = NULL;
	debugfs_remove(ar->cfr.rcc_debug_counters);
	ar->cfr.rcc_debug_counters = NULL;
	debugfs_remove(ar->cfr.clr_dbg_counters);
	ar->cfr.clr_dbg_counters = NULL;

	if (ar->cfr.rfs_cfr_capture) {
		relay_close(ar->cfr.rfs_cfr_capture);
		ar->cfr.rfs_cfr_capture = NULL;
	}
}

static inline int ath12k_cfr_debug_register(struct ath12k *ar)
{
	int ret;

	ar->cfr.rfs_cfr_capture = relay_open("cfr_capture",
					     ar->debug.debugfs_pdev,
					     ar->ab->hw_params->cfr_stream_buf_size,
					     ar->ab->hw_params->cfr_num_stream_bufs,
					     &rfs_cfr_capture_cb, NULL);
	if (!ar->cfr.rfs_cfr_capture) {
		ath12k_warn(ar->ab, "failed to open relay for cfr in pdev %d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	ar->cfr.enable_cfr = ath12k_debugfs_create_file("enable_cfr", 0600,
							ar->debug.debugfs_pdev,
							ar->ab, ar,
							&fops_enable_cfr);
	if (!ar->cfr.enable_cfr) {
		ath12k_warn(ar->ab, "failed to open debugfs in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	ar->cfr.cfr_unassoc = ath12k_debugfs_create_file("cfr_unassoc", 0600,
							 ar->debug.debugfs_pdev,
							 ar->ab, ar,
							 &fops_configure_cfr_unassoc);

	if (!ar->cfr.cfr_unassoc) {
		ath12k_warn(ar->ab,
			    "failed to open debugfs for unassoc pool in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	ar->cfr.rcc_debug_counters =
		ath12k_debugfs_create_file("rcc_debug_counters",
					   0400, ar->debug.debugfs_pdev,
					   ar->ab, ar, &fops_rcc_debug_counters);
	if (!ar->cfr.rcc_debug_counters) {
		ath12k_warn(ar->ab,
			    "failed to create RCC debug counters file in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	ar->cfr.clr_dbg_counters =
		ath12k_debugfs_create_file("clr_dbg_counters",
					   0200, ar->debug.debugfs_pdev,
					   ar->ab, ar, &fops_clr_dbg_counters);
	if (!ar->cfr.clr_dbg_counters) {
		ath12k_warn(ar->ab,
			    "failed to create RCC counter clear file in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	return 0;

debug_unregister :
	ath12k_cfr_debug_unregister(ar);
	return ret;
}

static int ath12k_cfr_ring_alloc(struct ath12k *ar,
				 struct ath12k_dbring_cap *db_cap)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	int ret;

	ret = ath12k_dbring_srng_setup(ar, &cfr->rx_ring,
				       1, db_cap->min_elem);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring\n");
		return ret;
	}

	ath12k_dbring_set_cfg(ar, &cfr->rx_ring,
			      ATH12K_CFR_NUM_RESP_PER_EVENT,
			      ATH12K_CFR_EVENT_TIMEOUT_MS,
			      ath12k_cfr_process_dbr_data);

	ret = ath12k_dbring_buf_setup(ar, &cfr->rx_ring, db_cap);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring buffer\n");
		goto srng_cleanup;
	}

	ret = ath12k_dbring_wmi_cfg_setup(ar, &cfr->rx_ring, WMI_DIRECT_BUF_CFR);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring cfg\n");
		goto buffer_cleanup;
	}

	return 0;

buffer_cleanup:
	ath12k_dbring_buf_cleanup(ar, &cfr->rx_ring);
srng_cleanup:
	ath12k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	return ret;
}

void ath12k_cfr_ring_free(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	ath12k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	ath12k_dbring_buf_cleanup(ar, &cfr->rx_ring);
}

void ath12k_cfr_deinit(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT, ab->wmi_ab.svc_map) ||
	    !ab->hw_params->cfr_support)
		return;

	for (i = 0; i <  ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;
		if (cfr->lut_age_timer_init) {
			cfr->lut_age_timer_init = false;
			timer_delete_sync(&cfr->lut_age_timer);
		}

		spin_lock_bh(&cfr->lut_lock);
		if (cfr->lut) {
			kfree(cfr->lut);
			cfr->lut = NULL;
		}
		spin_unlock_bh(&cfr->lut_lock);

		kfree(cfr->enh_aoa_data.gain_stop_index_array);
		cfr->enh_aoa_data.gain_stop_index_array = NULL;
		kfree(cfr->enh_aoa_data.enh_phase_delta_array);
		cfr->enh_aoa_data.enh_phase_delta_array = NULL;
		ar->cfr.cfr_enabled = 0;
		ath12k_cfr_debug_unregister(ar);
		ath12k_cfr_ring_free(ar);
	}
}

static void
ath12k_wmi_enhanced_aoa_unpack_hdr(struct ath12k_cfr *cfr,
				   struct cfr_enhanced_aoa_data *aoa,
				   struct ath12k_wmi_enhanced_aoa_phasedelta_parse *parse,
				   u32 chain_u16_offset)
{
	u32 max_dst_ent = aoa->max_entries_all_table * aoa->max_aoa_chains;
	u32 max_src_words = parse->data_buf_len / sizeof(__le32);
	u32 phase_pos = chain_u16_offset;
	u32 gain_pos = chain_u16_offset;
	u32 src_word_idx = 0;
	u32 i;

	for (i = 0; i < parse->num_data_hdr; i++) {
		u32 data_info = __le32_to_cpu(parse->data_hdr[i].data_info);
		u32 data_type = u32_get_bits(data_info, WMI_AOA_DATA_TYPE);
		u32 num_entries = u32_get_bits(data_info, WMI_AOA_NUM_ENTRIES);
		u16 *dst = NULL;
		u32 dst_pos = 0;
		u32 word_off;

		if (data_type == WMI_PHASE_DELTA_ARRAY) {
			dst = aoa->enh_phase_delta_array;
			dst_pos = phase_pos;
		} else if (data_type == WMI_GAIN_GROUP_STOP_ARRAY) {
			dst = aoa->gain_stop_index_array;
			dst_pos = gain_pos;
		}

		if (dst) {
			for (word_off = 0;
			     word_off < num_entries &&
			     src_word_idx + word_off < max_src_words &&
			     dst_pos + word_off * 2 + 1 < max_dst_ent;
			     word_off++) {
				u32 word = le32_to_cpu(parse->data_buf[src_word_idx +
									word_off]);

				dst[dst_pos + word_off * 2] = (u16)word;
				dst[dst_pos + word_off * 2 + 1] = (u16)(word >> 16);
			}
		}

		if (data_type == WMI_PHASE_DELTA_ARRAY)
			phase_pos += num_entries * 2;
		else if (data_type == WMI_GAIN_GROUP_STOP_ARRAY)
			gain_pos += num_entries * 2;

		src_word_idx += num_entries;
	}
}

void
ath12k_wmi_cfr_handle_aoa_data(struct ath12k *ar,
			       struct ath12k_wmi_enhanced_aoa_phasedelta_parse *parse)
{
	struct ath12k_cfr *cfr;
	struct cfr_enhanced_aoa_data *aoa;
	u32 chain_info, max_chains, data_for_chainmask, chain_u16_offset;

	cfr = &ar->cfr;
	aoa = &cfr->enh_aoa_data;

	if (!cfr->is_enh_aoa_data) {
		ath12k_warn(ar->ab, "AoA phase delta event received without service caps");
		return;
	}

	chain_info = __le32_to_cpu(parse->fixed_param.chain_info);
	max_chains = u32_get_bits(chain_info, WMI_AOA_MAX_SUPPORTED_CHAINS);

	if (max_chains > WMI_MAX_CHAINS) {
		ath12k_warn(ar->ab, "Invalid AOA max chains");
		return;
	}
	aoa->max_aoa_chains = max_chains;
	aoa->freq = __le32_to_cpu(parse->fixed_param.freq);
	aoa->xbar_config = __le32_to_cpu(parse->fixed_param.xbar_config);
	for (int i = 0; i < WMI_MAX_CHAINS; i++)
		aoa->ibf_cal_val[i] =
			__le32_to_cpu(parse->fixed_param.per_chain_ibf_cal_val[i]);

	data_for_chainmask = u32_get_bits(chain_info, WMI_AOA_SUPPORTED_CHAINMASK);
	chain_u16_offset = (data_for_chainmask ?
			    aoa->max_entries_all_table * __ffs(data_for_chainmask) : 0);

	ath12k_wmi_enhanced_aoa_unpack_hdr(cfr, aoa, parse, chain_u16_offset);
}

int ath12k_cfr_get_enhanced_aoa_caps(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_wmi_enh_aoa_caps_arg *caps = &ar->ab->enh_aoa_caps;
	struct cfr_enhanced_aoa_data *data = &cfr->enh_aoa_data;
	u32 i, gain_tbl_sz;

	cfr->is_enh_aoa_data = false;

	if (caps->valid) {
		if (caps->max_agc_gain_tbls > ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS)
			return -EINVAL;

		data->max_agc_gain_tbls = caps->max_agc_gain_tbls;
		gain_tbl_sz = sizeof(u16) * ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS;
		memcpy(data->max_agc_gain_per_tbl_2g, caps->max_agc_gain_per_tbl_2g,
		       gain_tbl_sz);
		memcpy(data->max_agc_gain_per_tbl_5g, caps->max_agc_gain_per_tbl_5g,
		       gain_tbl_sz);
		memcpy(data->max_agc_gain_per_tbl_6g, caps->max_agc_gain_per_tbl_6g,
		       gain_tbl_sz);
		memcpy(data->max_bdf_entries_per_tbl, caps->max_bdf_entries_per_tbl,
		       sizeof(u8) * ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS);

		data->max_entries_all_table = 0;
		data->start_ent[0] = 0;
		for (i = 0; i < data->max_agc_gain_tbls; i++) {
			data->max_entries_all_table += data->max_bdf_entries_per_tbl[i];
			if ((i + 1) < data->max_agc_gain_tbls)
				data->start_ent[i + 1] =
					(data->max_bdf_entries_per_tbl[i] +
					 data->start_ent[i]);
		}

		data->gain_stop_index_array = kzalloc(sizeof(u16) *
						      data->max_entries_all_table *
						      WMI_MAX_CHAINS, GFP_KERNEL);
		if (!data->gain_stop_index_array)
			return -ENOMEM;

		data->enh_phase_delta_array = kzalloc(sizeof(u16) *
						      data->max_entries_all_table *
						      WMI_MAX_CHAINS, GFP_KERNEL);
		if (!data->enh_phase_delta_array) {
			kfree(data->gain_stop_index_array);
			data->gain_stop_index_array = NULL;
			return -ENOMEM;
		}

		cfr->is_enh_aoa_data = true;
	}

	return 0;
}

int ath12k_cfr_init(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	struct ath12k_dbring_cap db_cap;
	struct ath12k_cfr_look_up_table *lut;
	u32 num_lut_entries;
	int ret = 0;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT, ab->wmi_ab.svc_map) ||
	    !ab->hw_params->cfr_support)
		return ret;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;

		ret = ath12k_dbring_get_cap(ar->ab, ar->pdev_idx,
					    WMI_DIRECT_BUF_CFR, &db_cap);
		if (ret)
			continue;

		idr_init(&cfr->rx_ring.bufs_idr);
		spin_lock_init(&cfr->rx_ring.idr_lock);
		spin_lock_init(&cfr->lock);
		spin_lock_init(&cfr->lut_lock);
		num_lut_entries = min((u32)CFR_MAX_LUT_ENTRIES, db_cap.min_elem);
		cfr->max_mu_users = HAL_MAX_UL_MU_USERS;

		cfr->lut = kzalloc(num_lut_entries * sizeof(*lut), GFP_KERNEL);
		if (!cfr->lut) {
			ath12k_warn(ab, "failed to allocate lut for pdev %d\n", i);
			return -ENOMEM;
		}

		ret = ath12k_cfr_ring_alloc(ar, &db_cap);
		if (ret) {
			ath12k_warn(ab, "failed to init cfr ring for pdev %d\n", i);
			goto deinit;
		}

		spin_lock_bh(&cfr->lock);
		cfr->lut_num = num_lut_entries;
		spin_unlock_bh(&cfr->lock);

		ret = ath12k_cfr_debug_register(ar);
		if (ret) {
			ath12k_warn(ab, "failed to register cfr for pdev %d\n", i);
			goto deinit;
		}

		ret = ath12k_cfr_get_enhanced_aoa_caps(ar);
		if (ret) {
			ath12k_warn(ab, "Failed to get enhanced aoa caps");
			goto deinit;
		}

		if (!test_bit(WMI_SERVICE_CFR_CAPTURE_FILTER_SUPPORT,
			      ab->wmi_ab.svc_map))
			continue;

		cfr->rcc_param.modified_in_curr_session = MAX_RESET_CFG_ENTRY;
		cfr->rcc_param.num_grp_tlvs = MAX_TA_RA_ENTRIES;
		cfr->rcc_param.pdev_id = ar->pdev->pdev_id;
		cfr->rcc_param.srng_id = 0;
		cfr->rcc_param.vdev_id = 0xff;

		ath12k_peer_cfr_default_ta_ra_config(&cfr->rcc_param, true,
						     MAX_RESET_CFG_ENTRY);

		ret = ath12k_wmi_send_cfr_rcc_cmd(ar, &cfr->rcc_param);
		if (ret) {
			ath12k_warn(ab,
				    "failed to send default cfr rcc config for pdev %d: %d\n",
				    i, ret);
			goto deinit;
		}

		ath12k_peer_cfr_update_global_cfg(ar);
		cfr->rcc_param.modified_in_curr_session = 0;
		cfr->rcc_param.num_grp_tlvs = 0;

		timer_setup(&cfr->lut_age_timer, ath12k_cfr_lut_ageout_timer, 0);
		cfr->lut_age_timer_init = true;
	}
	return 0;

deinit:
	ath12k_cfr_deinit(ab);
	return ret;
}

int
ath12k_cfr_set_rcc_capture_mode(struct ath12k *ar,
				enum qca_wlan_vendor_cfr_capture_type type,
				bool enable)
{
	switch (type) {
	case QCA_WLAN_VENDOR_CFR_TA_RA:
		ar->cfr.rcc_param.m_ta_ra_filter = enable;
		break;
	case QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP:
		ar->cfr.rcc_param.m_ndpa_ndp_directed = enable;
		break;
	case QCA_WLAN_VENDOR_CFR_NDPA_NDP_ALL:
		ar->cfr.rcc_param.m_ndpa_ndp_all = enable;
		break;
	default:
		ath12k_err(ar->ab, "Unknown RCC capture type %u",
			   type);
		return -EINVAL;
	}

	return 0;
}

int
ath12k_cfr_set_capture_duration(struct ath12k *ar, u32 duration)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	if (duration > 0xffffff) {
		ath12k_err(ar->ab,
			   "Duration should not be greater than (16.8 sec) 0xFFFFFF");
		return -EINVAL;
	}

	if (rcc->capture_interval &&
	    duration > rcc->capture_interval) {
		ath12k_err(ar->ab,
			   "Capture duration is exceeding capture interval");
		return -EINVAL;
	}

	rcc->capture_duration = duration;

	return 0;
}

int
ath12k_cfr_set_capture_interval(struct ath12k *ar, u32 interval)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	if (interval > 0xffffff) {
		ath12k_err(ar->ab,
			   "Interval should not be greater than 16.8 sec) 0xFFFFFF");
		return -EINVAL;
	}

	if (rcc->capture_duration > interval) {
		ath12k_err(ar->ab,
			   "Capture interval should be more than capture duration");
		return -EINVAL;
	}

	rcc->capture_interval = interval;
	return 0;
}

int
ath12k_cfr_set_freeze_tlv_delay_cnt(struct ath12k *ar, u32 val)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	if (val > 0xff) {
		ath12k_err(ar->ab,
			   "Invalid freeze tlv delay (allowed 0x0 - 0xff");
		return -EINVAL;
	}
	rcc->freeze_tlv_delay_cnt_thr = val;
	if (rcc->freeze_tlv_delay_cnt_thr)
		rcc->freeze_tlv_delay_cnt_en = 1;
	else
		rcc->freeze_tlv_delay_cnt_en = 0;

	return 0;
}

int
ath12k_cfr_set_capture_count(struct ath12k *ar, u16 val)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	if (!test_bit(WMI_SERVICE_CFR_CAPTURE_COUNT_SUPPORT,
		      ar->ab->wmi_ab.svc_map)) {
		ath12k_err(ar->ab,
			   "Capture count support not enabled");
		return -EOPNOTSUPP;
	}

	if (val > 0xFFFF) {
		ath12k_err(ar->ab,
			   "Invalid value: count must be <= 0xFFFF");
		return -EINVAL;
	}

	rcc->capture_count = val;

	return 0;
}

int
ath12k_cfr_set_enable_bitmap(struct ath12k *ar, u32 val)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	if (val > 0xffff) {
		ath12k_err(ar->ab,
			   "group bitmap should be in between 0 to 0xFFFF");
		return -EINVAL;
	}

	rcc->filter_group_bitmap = val;

	return 0;
}

void
ath12k_cfr_set_ul_mu_mask(struct ath12k *ar, u64 val)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	rcc->ul_mu_user_mask_lower = lower_32_bits(val);
	rcc->ul_mu_user_mask_upper = upper_32_bits(val);
}

void
ath12k_cfr_set_tara_group_params(struct ath12k *ar, struct cfr_rcc_param *param)
{
	struct cfr_rcc_param *rcc = &ar->cfr.rcc_param;

	for (int i = 0; i < MAX_TA_RA_ENTRIES; i++) {
		if (BIT(i) & param->modified_in_curr_session)
			memcpy(&rcc->curr[i], &param->curr[i],
			       sizeof(struct ta_ra_cfr_cfg));
	}
	rcc->modified_in_curr_session = param->modified_in_curr_session;
	rcc->num_grp_tlvs = param->num_grp_tlvs;
}

void
ath12k_cfr_reset_group_configs_for_bitmap(struct ath12k *ar, u32 bitmap)
{
	ar->cfr.rcc_param.modified_in_curr_session |= bitmap;

	ath12k_peer_cfr_default_ta_ra_config(&ar->cfr.rcc_param, true, bitmap);
}

/*
 * commit: serialize all staged rcc_param state and send one WMI command.
 * Clears modified_groups on success.
 */
int ath12k_peer_cfr_rcc_commit(struct ath12k *ar)
{
	bool rcc_enabled = false;
	struct ath12k_base *ab = ar->ab;
	struct cfr_rcc_param rcc_param;
	int ret;
	u8 rcc_type = 0;

	/* check if target supports rcc via service bit, if not reject the
	 * command
	 */
	if (!test_bit(WMI_SERVICE_CFR_CAPTURE_FILTER_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "RCC filter support not enabled on this target");
		return -EINVAL;
	}

	spin_lock_bh(&ar->cfr.lock);

	if (ar->cfr.cfr_enabled) {
		ath12k_err(ar->ab,
			   "Can't control RCC as Periodic CFR enabled on the radio");
		spin_unlock_bh(&ar->cfr.lock);
		return -EINVAL;
	}

	if (ar->cfr.rcc_param.m_directed_ftm)
		rcc_type |= DP_MON_RCC_TYPE_M_DIRECTED_FTM;
	if (ar->cfr.rcc_param.m_all_ftm_ack)
		rcc_type |= DP_MON_RCC_TYPE_M_ALL_FTM_ACK;
	if (ar->cfr.rcc_param.m_ndpa_ndp_directed)
		rcc_type |= DP_MON_RCC_TYPE_M_NDPA_NDP_DIRECTED;
	if (ar->cfr.rcc_param.m_ndpa_ndp_all)
		rcc_type |= DP_MON_RCC_TYPE_M_NDPA_NDP_ALL;
	if (ar->cfr.rcc_param.m_ta_ra_filter)
		rcc_type |= DP_MON_RCC_TYPE_M_TA_RA_FILTER;
	if (ar->cfr.rcc_param.m_all_packet)
		rcc_type |= DP_MON_RCC_TYPE_M_ALL_PACKET;

	if (rcc_type)
		rcc_enabled = true;

	/* check if any of the filter type is enabled in rcc_param
	 * if enabled: register the rx ppdu completion event notifier.
	 * populate the filter values and configure the monitor status ring
	 * as per the cfr capture type. Before configuring the ring, start
	 * lut age timer if already dp not started with cfr
	 * if none-enabled: the rx ppdu completion event notifier.
	 * stop the age timer if dp had cfr enabled before. then reset
	 * the filter setting of the mon status ring
	 */
	if (rcc_enabled) {
		/* check if dp cfr is disabled, then start the lut age timer
		 */
		if (!ath12k_dp_mon_get_cfr_rcc_filter_valid(ar)) {
			ar->cfr.lut_age_timer_init = true;
			mod_timer(&ar->cfr.lut_age_timer,
				  jiffies + msecs_to_jiffies(ATH12K_CFR_LUT_AGE_TIMER));
		}

		/* subscribe the PPDU rx notifier */
		spin_unlock_bh(&ar->cfr.lock);
		ret = ath12k_cfr_register_ppdu_rx_notifier(ar);
		if (ret) {
			ath12k_err(ab, "Failed to register ppdu rx notifier");
			return ret;
		}

		/* update the DP monitor filter as per the current CFR session */
		ath12k_dp_mon_cfr_rcc_config(ar, true, rcc_type);
		ret = ath12k_dp_mon_rx_update_filter(ar);
		if (ret) {
			ath12k_err(ab, "RCC Enable: Failed to update dp mon rx filter");
			return ret;
		}
		spin_lock_bh(&ar->cfr.lock);
	} else {
		/* If dp cfr enabled, stop the lut age timer */
		if (ath12k_dp_mon_get_cfr_rcc_filter_valid(ar)) {
			ar->cfr.lut_age_timer_init = false;
			/* release lock to avoid deadlock from already running timer
			 * handler and timer_delete_sync
			 */
			spin_unlock_bh(&ar->cfr.lock);
			timer_delete_sync(&ar->cfr.lut_age_timer);
			spin_lock_bh(&ar->cfr.lock);
		}

		/* disable the cfr filter */
		spin_unlock_bh(&ar->cfr.lock);
		ath12k_dp_mon_cfr_rcc_config(ar, false, rcc_type);
		ret = ath12k_dp_mon_rx_update_filter(ar);
		if (ret) {
			ath12k_err(ab, "RCC Disable: Failed to update the mon rx filter");
			return ret;
		}

		/* unsubscribe the ppdu rx notifier */
		ret = ath12k_cfr_unregister_ppdu_rx_notifier(ar);
		if (ret) {
			ath12k_err(ab, "Failed to unregister ppdu rx notifier");
			return ret;
		}
		spin_lock_bh(&ar->cfr.lock);
	}

	/* save the rcc params locally and release the cfr lock temporarily
	 * to send wmi command
	 */
	rcc_param = ar->cfr.rcc_param;
	spin_unlock_bh(&ar->cfr.lock);
	/* send the wmi command to fw */
	ret = ath12k_wmi_send_cfr_rcc_cmd(ar, &rcc_param);
	/* re-qcquire the cfr lock post sending wmi command */
	spin_lock_bh(&ar->cfr.lock);
	/* If wmi send passed, then save the configurations to global and reset
	 * curr to default state for next commit session.
	 */
	if (!ret) {
		ath12k_peer_cfr_update_global_cfg(ar);

		/* Bring curr cfg to default state for next commit session.
		 * Leave the common parameters as it is as they are non-group
		 * parameters and can be retained across commit sessions.
		 */
		ath12k_peer_cfr_default_ta_ra_config(&ar->cfr.rcc_param,
						     false, MAX_RESET_CFG_ENTRY);
		if (rcc_enabled)
			ar->cfr.rcc_enabled = true;
		else
			ar->cfr.rcc_enabled = false;
	} else {
		/* If fail to send WMI command, which usually should not happen,
		 * leave the state of timer, mon ring, ppdu notifier as it is.
		 */
		ath12k_err(ab, "Failed to send RCC WMI command to target");
	}

	/* reset modified_in_current_session, num_grp_tlvs to 0 (despite of
	 * wmi status)
	 */
	ar->cfr.rcc_param.num_grp_tlvs = 0;
	ar->cfr.rcc_param.modified_in_curr_session = 0;
	spin_unlock_bh(&ar->cfr.lock);

	return ret;
}
