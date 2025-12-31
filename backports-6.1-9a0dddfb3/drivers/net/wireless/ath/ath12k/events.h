/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_EVENTS_H
#define ATH12K_EVENTS_H

#include <linux/types.h>
#include <linux/llist.h>
#include <linux/bitops.h>
#include <kunit/visibility.h>
#include <linux/export.h>
#include <kunit/static_stub.h>

/* Generic VIF Event Types */
enum ath12k_vif_event_type {
	ATH12K_VIF_EVENT_TYPE_PEER,
};

/* Generic VIF Event Structure */
struct ath12k_vif_event {
	struct llist_node node;
	enum ath12k_vif_event_type type;
	u16 peer_id;  /* For safe peer lookup instead of container_of */
	u8 link_id;
	u8 hw_link_id;
};

/* Generic Peer Event Flags */
enum ath12k_peer_event_flags {
	ATH12K_PEER_EVENT_RSSI_LOW = BIT(0),
};

#endif /* ATH12K_EVENTS_H */

