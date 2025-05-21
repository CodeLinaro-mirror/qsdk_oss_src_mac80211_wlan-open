/*
*Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include "athdbg_if.h"
#include "ath_debug/athdbg_core.h"
#include "ath_debug/athdbg_minidump.h"

extern struct ath_debug_base *athdbg_base;

#define MAX_SOC_DIR_NAME_SIZE 64

const struct athdbg_to_ath12k_ops dbg_to_ath_ops = {
	.get_reserved_mem_by_name = ath12k_core_get_reserved_mem_by_name,
	.coredump_build_inline = ath12k_coredump_build_inline,
	.dev_running_status = athdbg_if_check_dev_running,
	.set_dbg_mask = athdbg_if_setmask,
};

static int athdbg_if_create_debugfs(struct ath12k_base *ab)
{
	struct dentry *debugfs_soc_dir,
				  *debugfs_ath12k_dir,
				  *athdbg_dir;
#if !defined(CONFIG_DEBUG_MEM_USAGE)
	struct dentry	*minidump_dir;
#endif
	struct ath12k_base *partner_ab;
	char soc_name[MAX_SOC_DIR_NAME_SIZE] = { 0 };
	int i;

	debugfs_ath12k_dir = debugfs_lookup("ath12k", NULL);

	for (i = 0; i < ab->ag->num_devices; i++) {
		partner_ab = ab->ag->ab[i];
		if (!partner_ab)
			continue;

		scnprintf(soc_name, sizeof(soc_name), "%s-%s", ath12k_bus_str(partner_ab->hif.bus),
				dev_name(partner_ab->dev));

		debugfs_soc_dir = debugfs_lookup(soc_name, debugfs_ath12k_dir);

		if (IS_ERR_OR_NULL(debugfs_soc_dir))
			goto out;

		athdbg_dir = debugfs_create_dir("ath_debug", debugfs_soc_dir);

		if (IS_ERR_OR_NULL(athdbg_dir))
			goto out;

		debugfs_create_file("dbgmask", 0644, athdbg_dir, partner_ab, &debugfs_mask_fops);

#if !defined(CONFIG_DEBUG_MEM_USAGE)
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
		minidump_dir = debugfs_create_dir("minidump", athdbg_dir);

		if (IS_ERR_OR_NULL(minidump_dir))
			goto out;

		athdbg_create_minidump_debugfs(minidump_dir, partner_ab);
#endif
#endif
	}
	return 0;
out:
	return -ENOMEM;
}

void athdbg_if_register(struct ath12k_base *ab)
{
	int ret;

	ret = athdbg_if_create_debugfs(ab);
	if (ret) {
		pr_err("athdbg_if: debugfs entry create failure %d", ret);
		return;
	}

	athdbg_base->dbg_to_ath_ops = &dbg_to_ath_ops;
}

void athdbg_if_unregister(struct ath12k_base *ab)
{
	athmem_clear_memdebug_info();
}

/* Interface provided to perform any action that need to be done in the
 * ath12k driver flow. This call can be triggered from the ath12k.
 */
int athdbg_if_get_service(struct ath12k_base *ab, enum athdbg_service srv)
{
	int ret = 0;

	switch (srv) {
	case ATHDBG_SRV_CONFIG_QDSS:
		break;
	case ATHDBG_SRV_DO_MINIDUMP:
		athdbg_do_dump_minidump(ab);
		break;
	case ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES:
		athdbg_collect_reference_segments(ab);
		break;
	}

	return ret;
}

bool athdbg_if_check_dev_running(struct ath12k_base *ab)
{
	bool ret = FALSE;

	if (test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags))
		ret = TRUE;

	return ret;
}

void athdbg_if_setmask(unsigned int debug_mask)
{
	ath12k_debug_mask = debug_mask;
}
