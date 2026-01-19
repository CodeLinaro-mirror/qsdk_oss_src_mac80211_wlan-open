// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/rcupdate.h>
#include "core.h"
#include "me.h"

/**
 * ath12k_me_db_reset(): Multicast Enhancement database reset
 * @db - ME Database
 */
void ath12k_me_db_reset(struct ath12k_me_db *db)
{
	/* Reset flags and limits */
	db->me_flags = 0;
	db->grp_limit = 0;
}

/**
 * ath12k_me_db_deinit(): Multicast Enhancement database deinit
 * @ahvif - Pointer to virtual interface structure
 *
 * Return: status of success or failure.
 */
int ath12k_me_db_deinit(struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_db *db;

	if (!dp_vif)
		return -EINVAL;

	db = rcu_dereference_protected(dp_vif->me_db, true);
	if (!db)
		return 0;

	ath12k_me_db_reset(db);

	rcu_assign_pointer(dp_vif->me_db, NULL);
	synchronize_rcu();
	kfree(db);

	return 0;
}

/**
 * ath12k_me_db_init(): Multicast Enhancement database init
 * @ahvif - Pointer to virtual interface structure
 *
 * Return: status of success or failure.
 */
int ath12k_me_db_init(struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_me_db *db;

	if (!dp_vif)
		return -EINVAL;

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (!db)
		return -ENOMEM;

	ath12k_me_db_reset(db);
	rcu_assign_pointer(dp_vif->me_db, db);

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_me_db_init);
