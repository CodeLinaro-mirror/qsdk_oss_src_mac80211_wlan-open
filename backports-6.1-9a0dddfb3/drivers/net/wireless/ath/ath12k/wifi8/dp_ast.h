/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_AST_H
#define ATH12K_DP_AST_H

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/rhashtable.h>

#define TOEPLITZ_KEYLEN 9
#define INPUT_HASH_KEY_LEN 7
#define ATH12K_AST_HASH_KEY_1 0x4ab61d84
#define ATH12K_AST_HASH_KEY_2 0xdab4adb4
#define ATH12K_AST_HASH_KEY_3 0
#define MAX_NUM_AST_ENTRIES 2048
#define MAX_SKID_LEN	16

struct ath12k_dp_hw_group;

#define ATH12K_INVALID_AST_INDEX		0xFFFF
#define ATH12K_AST_ENTRY_WILDCARD_LINK_ID	0x7
#define ATH12K_NUM_TX_CLASSIFY_BANKS		4
#define ATH12K_MAX_TX_ASE_CMD_SEQ_NUM		0x7FFF

struct ath12k_dp_hw_group;
struct ath12k_dp;

enum ATH12K_AST_ENTRY_FLAGS {
	ATH12K_AST_ENTRY_EMPTY_FLAGS	= 0x0,
	ATH12K_AST_ENTRY_IS_VALID	= 0x1,
	ATH12K_AST_ENTRY_IS_MCAST	= 0x2,
	ATH12K_AST_ENTRY_IS_MEC		= 0x4,
	ATH12K_AST_ENTRY_IS_USE_ADDRX	= 0x8,
};

enum ATH12K_AST_ENTRY_INVALIDATE_STATUS_FLAGS {
	ATH12K_AST_ENTRY_TX_INVAL_STATUS,
};

struct ath12k_ast_entry_config_params {
	u8 mac_addr[ETH_ALEN];
	u16 peer_id;
	dma_addr_t tx_classify_info_paddr;
	u16 ast_index;
	u16 ast_hash;
	u8 mld_id;
	u8 ast_entry_flags;
};

struct ath12k_ast_entry {
	u8 mac_addr[ETH_ALEN];
	u16 peer_id;
	dma_addr_t tx_classify_info_paddr;
	u16 ast_index;
	u16 ast_hash;
	u8 mld_id;
	u8 ast_entry_flags;
	/* peer addr based rhashtable list pointer */
	struct rhash_head rhash_addr;
	bool rhash_done;
	unsigned long ast_create_invalidate_status;
	u16 tx_cmd_seq_num;
};

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
	u8 ase_tx_cache_en:1,
	   reserved:7;

	struct ath12k_ast_entry **ast_entries;
	struct rhashtable *rhead_ast_entry;
	struct rhashtable_params rhash_ast_entry_param;
};

enum ath12k_ase_cache_op {
	ATH12K_INVALIDATE_ALL,
	ATH12K_INVALIDATE_ENTRY,
};

enum ast_entry_op_state {
	AST_ENTRY_DELETE =      0,
	AST_ENTRY_CREATE =      1,
};

#define ATH12K_AST_META_DATA_0_AST_INDEX	GENMASK(15, 0)
#define ATH12K_AST_META_DATA_0_TX_CMD_SEQ_NUM	GENMASK(30, 16)
#define ATH12K_AST_META_DATA_0_STATE		BIT(31)
struct ath12k_ase_cache_op_param {
	int cmd;
	u32 meta_data_0;
	u16 ast_index;
};

int ath12k_wifi8_invalidate_peer_ase_cache_table(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_wifi8_dp_tx_cmd_status_handler(struct ath12k_dp *dp,
					  int budget);
bool ath12k_wifi8_dp_ase_tx_cache_enabled(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_dp_ast_table_init(struct ath12k_dp_hw_group *dp_hw_grp);
void ath12k_dp_ast_table_deinit(struct ath12k_dp_hw_group *dp_hw_grp);
int ath12k_dp_rx_ast_info_setup(struct ath12k_dp *dp);
int ath12k_dp_ast_entry_create(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry_config_params *param);
void ath12k_dp_ast_entry_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				u16 ast_index);
#endif
