/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_RANGING_H
#define ATH12K_RANGING_H

/* RTT responder role bits */
#define ATH12K_RTT_11MC_RESPONDER_ROLE		BIT(0)
#define ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE	BIT(1)
#define ATH12K_RTT_11AZ_TB_RESPONDER_ROLE	BIT(2)
#define ATH12K_RTT_RESPONDER_ROLE_MASK		(ATH12K_RTT_11MC_RESPONDER_ROLE | \
						 ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE | \
						 ATH12K_RTT_11AZ_TB_RESPONDER_ROLE)

/* AP iftype extended capability byte index and bits for 11az ranging */
#define ATH12K_EXT_CAPA_11AZ_RANGING_IDX		11
#define ATH12K_EXT_CAPA12_NTB_RANGING_RESPONDER	BIT(2)
#define ATH12K_EXT_CAPA12_TB_RANGING_RESPONDER		BIT(3)

struct ath12k;
struct ath12k_base;
struct ath12k_hw;

u32 ath12k_supported_rtt_responder_roles(struct ath12k_base *ab);
int ath12k_mac_setup_iftype_11az_ranging(struct ath12k_hw *ah);
void ath12k_mac_cleanup_iftype_11az_ranging(struct ath12k_hw *ah);
void ath12k_mac_set_ranging_ext_features(struct wiphy *wiphy, struct ath12k *ar);
void ath12k_rtt_reconfig_responder_role(struct ath12k *ar);

#endif /* ATH12K_RANGING_H */
