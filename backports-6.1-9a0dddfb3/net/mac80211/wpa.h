/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2002-2004, Instant802 Networks, Inc.
 * Copyright (C) 2022 Intel Corporation
 */

#ifndef WPA_H
#define WPA_H

#include <linux/skbuff.h>
#include <linux/types.h>
#include "ieee80211_i.h"

ieee80211_tx_result
ieee80211_tx_h_michael_mic_add(struct ieee80211_tx_data *tx);
ieee80211_rx_result
ieee80211_rx_h_michael_mic_verify(struct ieee80211_rx_data *rx);

ieee80211_tx_result
ieee80211_crypto_tkip_encrypt(struct ieee80211_tx_data *tx);
ieee80211_rx_result
ieee80211_crypto_tkip_decrypt(struct ieee80211_rx_data *rx);

ieee80211_tx_result
ieee80211_crypto_ccmp_encrypt(struct ieee80211_tx_data *tx,
			      unsigned int mic_len);
ieee80211_rx_result
ieee80211_crypto_ccmp_decrypt(struct ieee80211_rx_data *rx,
			      unsigned int mic_len);

ieee80211_tx_result
ieee80211_crypto_aes_cmac_encrypt(struct ieee80211_tx_data *tx);
ieee80211_tx_result
ieee80211_crypto_aes_cmac_256_encrypt(struct ieee80211_tx_data *tx);
ieee80211_rx_result
ieee80211_crypto_aes_cmac_decrypt(struct ieee80211_rx_data *rx);
ieee80211_rx_result
ieee80211_crypto_aes_cmac_256_decrypt(struct ieee80211_rx_data *rx);
ieee80211_tx_result
ieee80211_crypto_aes_gmac_encrypt(struct ieee80211_tx_data *tx);
ieee80211_rx_result
ieee80211_crypto_aes_gmac_decrypt(struct ieee80211_rx_data *rx);

ieee80211_tx_result
ieee80211_crypto_gcmp_encrypt(struct ieee80211_tx_data *tx);
ieee80211_rx_result
ieee80211_crypto_gcmp_decrypt(struct ieee80211_rx_data *rx);

/* CCMP/GCMP AAD + nonce construction and PN<->header conversion, shared
 * with out-of-tree software crypto that cannot use ieee80211_crypto_*
 * (those are tied to struct ieee80211_key / ieee80211_tx_data /
 * ieee80211_rx_data and the normal key-management lifecycle).
 */
void ieee80211_ccmp_special_blocks(struct sk_buff *skb, u8 *pn, u8 *b_0,
				   u8 *aad, bool spp_amsdu);
void ieee80211_ccmp_pn2hdr(u8 *hdr, u8 *pn, int key_id);
void ieee80211_ccmp_hdr2pn(u8 *pn, u8 *hdr);

void ieee80211_gcmp_special_blocks(struct sk_buff *skb, u8 *pn, u8 *j_0,
				   u8 *aad, bool spp_amsdu);
void ieee80211_gcmp_pn2hdr(u8 *hdr, const u8 *pn, int key_id);
void ieee80211_gcmp_hdr2pn(u8 *pn, const u8 *hdr);

#endif /* WPA_H */
