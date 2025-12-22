// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ast.h"
#include "dp.h"
#include "hal.h"
#include "../debug.h"

/* Whenever we configure HW keys, we need to update the below cache.
 * This is prevent mismatches between HW and SW indexing.
 */
u16 ath12k_ast_key_cache[TOEPLITZ_KEYLEN][1 << 8];

static inline struct ath12k_dp_global_ast_table *
		ath12k_dp_get_global_ast_table(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	return &dp_hw_grp_wifi8->dp_ast_base;
}

static inline void ath12k_dp_copy_mac_addr(u32 *mac_addr_31_0,
					   u16 *mac_addr_47_32,
					   u8 *mac_addr)
{
	memcpy(mac_addr_31_0, mac_addr, sizeof(u32));
	memcpy(mac_addr_47_32, mac_addr + 4, ETH_ALEN - 4);
}

u16 ath12k_dp_compute_ast_hash(u8 *mac_addr,
			       u32 ast_hash_mask,
			       bool is_mcast,
			       bool is_mec)
{
	u8 input_data[INPUT_HASH_KEY_LEN];
	u8 input_i;
	u8 cache_i;
	u8 i;
	u64 final_input;
	u16 hash_out = 0;
	u32 mac_addr_31_0 = 0;
	u16 mac_addr_47_32 = 0;

	ath12k_dp_copy_mac_addr(&mac_addr_31_0, &mac_addr_47_32, mac_addr);

	final_input = ((u64)mac_addr_31_0) |
		      ((u64)mac_addr_47_32 << 32) |
		      ((u64)is_mcast << 48) |
		      ((u64)is_mec << 49);
	final_input <<= 6;

	for (i = 0; i < INPUT_HASH_KEY_LEN; i++)
		input_data[6 - i] = (u8)((final_input >> (8 * i)) & 0xff);

	for (input_i = 0, cache_i = 0;
	     input_i < INPUT_HASH_KEY_LEN;
	     input_i++, cache_i++)
		hash_out ^= ath12k_ast_key_cache[cache_i][input_data[input_i]];

	return hash_out & ast_hash_mask;
}

void ath12k_dp_init_ast_toeplitz_key_cache(const u8 key[TOEPLITZ_KEYLEN])
{
	u16 cur_key = key[0] << 8 | key[1];
	u8  i;

	for (i = 0; i < TOEPLITZ_KEYLEN; i++) {
		u8  new_key_byte;
		u16 shifted_key[8];
		u8  bit;
		u32 val;

		if ((i + 2) < TOEPLITZ_KEYLEN) {
			new_key_byte = key[i + 2];
		} else {
			/* no further key material, shift in zeros */
			new_key_byte = 0;
		}

		shifted_key[0] = cur_key;

		for (bit = 1; bit < 8; bit++) {
			/* for each iteration, shift out one more bit of the
			 * current key and shift in one more bit of the new key
			 * material
			 */
			shifted_key[bit] = cur_key << bit | new_key_byte >> (8 - bit);
		}

		for (val = 0; val < (1 << 8); val++) {
			u16 hash = 0;
			u32 mask;

			for (bit = 0, mask = 1 << 7; bit < 8; bit++, mask >>= 1) {
				if ((val & mask)) {
					/* for each bit set in the input, XOR in
					 * the appropriately shifted key.
					 */
					hash ^= shifted_key[bit];
				}
			}

			ath12k_ast_key_cache[i][val] = hash;
		}

		cur_key = (cur_key << 8) | new_key_byte;
	}
}

void ath12k_dp_init_ast_hash_keys(u32 hash_key1,
				  u32 hash_key2,
				  u32 hash_key3)
{
	u64 temp_hash_key;
	u8 ase_keys[TOEPLITZ_KEYLEN];
	int i;

	temp_hash_key = ((u64)hash_key2 << 32) | ((u64)hash_key1);
	temp_hash_key >>= 1;
	temp_hash_key |= ((u64)hash_key3 << 63);

	for (i = 0; i < (TOEPLITZ_KEYLEN - 1); i++)
		ase_keys[i] = (u8)((temp_hash_key >> ((7 - i) * 8)) & 0xff);

	ase_keys[8] = (u8)((hash_key1 & 0xFF) << 7);

	/* Configure local hash key cache */
	ath12k_dp_init_ast_toeplitz_key_cache(ase_keys);
}

static inline void
ath12k_dp_ast_param_init(struct ath12k_dp_global_ast_table *ast_base,
			 struct ath12k_hal_ast_param *ast_info)
{
	ast_info->num_ast_entries = ast_base->num_ast_entries;
	ast_info->skid_len = ast_base->skid_len;

	/* Disable the ASE cache */
	ast_info->ast_cache_en = 0;
	/* Disable the failure cache */
	ast_info->ast_cache_failure_en = 0;

	/* init the hash keys */
	ast_info->ase_hash_key1 = ast_base->hash_keys.ase_hash_key1;
	ast_info->ase_hash_key2 = ast_base->hash_keys.ase_hash_key2;
	ast_info->ase_hash_key3 = ast_base->hash_keys.ase_hash_key3;
	ast_info->paddr = ast_base->ast_paddr;
}

int ath12k_dp_ast_table_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct device *dev = NULL;
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	struct ath12k_hal_ast_param ast_info = {0};
	struct ath12k_base *ab = NULL;
	int ret;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "ASE init dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);

	if (!ast_base || !dev || !ab) {
		ath12k_err(NULL, "ASE init unable to fetch ast_base or dev or ab\n");
		return -EINVAL;
	}

	if (ast_base->ast_vaddr_aligned)
		return 0;

	ast_base->num_ast_entries = MAX_NUM_AST_ENTRIES;
	ast_base->ast_hash_mask = ast_base->num_ast_entries - 1;
	ast_base->skid_len = MAX_SKID_LEN;

	ast_base->hw_ast_table_size = ast_base->num_ast_entries * HAL_HW_AST_ENTRY_SIZE;
	ast_base->ast_vaddr_unaligned =
			kzalloc(ast_base->hw_ast_table_size + HAL_HW_AST_ENTRY_ALIGN - 1,
				GFP_ATOMIC);
	if (!ast_base->ast_vaddr_unaligned) {
		ath12k_err(ab, "failed to allocate memory for global AST table");
		return -ENOMEM;
	}

	ast_base->ast_vaddr_aligned = PTR_ALIGN(ast_base->ast_vaddr_unaligned,
						HAL_HW_AST_ENTRY_ALIGN);
	ast_base->ast_paddr = dma_map_single(dev, ast_base->ast_vaddr_aligned,
					     ast_base->hw_ast_table_size,
					     DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, ast_base->ast_paddr);
	if (ret) {
		ath12k_err(ab, "failed to map AST table ret: %d\n", ret);
		goto free_hw_ast_table;
	}

	ast_base->hash_keys.ase_hash_key1 = ATH12K_AST_HASH_KEY_1;
	ast_base->hash_keys.ase_hash_key2 = ATH12K_AST_HASH_KEY_2;
	ast_base->hash_keys.ase_hash_key3 = ATH12K_AST_HASH_KEY_3;
	ath12k_dp_init_ast_hash_keys(ast_base->hash_keys.ase_hash_key1,
				     ast_base->hash_keys.ase_hash_key2,
				     ast_base->hash_keys.ase_hash_key3);

	spin_lock_init(&ast_base->ast_lock);

	ath12k_dp_ast_param_init(ast_base, &ast_info);
	ath12k_wifi8_hal_hw_ase_init(ab, &ast_info);

	return 0;

free_hw_ast_table:
	kfree(ast_base->ast_vaddr_unaligned);
	ast_base->ast_vaddr_unaligned = NULL;
	ast_base->ast_vaddr_aligned = NULL;
	return ret;
}

void ath12k_dp_ast_table_deinit(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base = NULL;
	struct device *dev = NULL;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "ASE deinit dp_hw_grp is NULL\n");
		return;
	}

	dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	if (!ast_base) {
		ath12k_err(NULL, "ASE deinit unable to fetch ast_base\n");
		return;
	}

	if (!ast_base->ast_vaddr_unaligned) {
		ath12k_err(NULL, "ASE deinit vaddr unaligned is NULL\n");
		return;
	}

	if (dev) {
		dma_unmap_single(dev, ast_base->ast_paddr,
				 ast_base->hw_ast_table_size,
				 DMA_BIDIRECTIONAL);
	}

	kfree(ast_base->ast_vaddr_unaligned);
	ast_base->ast_vaddr_unaligned = NULL;
	ast_base->ast_vaddr_aligned = NULL;
}
