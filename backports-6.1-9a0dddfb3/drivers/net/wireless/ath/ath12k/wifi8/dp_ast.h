/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_AST_H
#define ATH12K_DP_AST_H

#include <linux/types.h>
#include <linux/if_ether.h>

#define TOEPLITZ_KEYLEN 9
#define INPUT_HASH_KEY_LEN 7
#define ATH12K_AST_HASH_KEY_1 0x4ab61d84
#define ATH12K_AST_HASH_KEY_2 0xdab4adb4
#define ATH12K_AST_HASH_KEY_3 0
#define MAX_NUM_AST_ENTRIES 2048
#define MAX_SKID_LEN	16

struct ath12k_dp_hw_group;

struct ath12k_dp_ast_hash_keys {
	u32 ase_hash_key1;
	u32 ase_hash_key2;
	u32 ase_hash_key3;
};

struct ath12k_dp_global_ast_table {
	/* Memory allocation for AST entries.
	 * Will be programming this address in HW.
	 */
	void *ast_vaddr_aligned;
	void *ast_vaddr_unaligned;
	dma_addr_t ast_paddr;
	size_t hw_ast_table_size;
	/* global ast lock */
	spinlock_t ast_lock;
	struct ath12k_dp_ast_hash_keys hash_keys;
	u32 ast_hash_mask;
	u16 num_ast_entries;
	u8 skid_len;
};

int ath12k_dp_ast_table_init(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_ast_table_deinit(struct ath12k_dp_hw_group *dp_hw_grp);
#endif
