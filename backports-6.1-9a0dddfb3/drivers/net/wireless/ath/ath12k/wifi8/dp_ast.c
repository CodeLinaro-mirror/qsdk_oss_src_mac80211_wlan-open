// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ast.h"
#include "dp.h"
#include "hal.h"
#include "../debug.h"
#include "dp_htt.h"

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

int ath12k_dp_rx_ast_info_setup(struct ath12k_dp *dp)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_hal_ast_param ast_info = {0};
	struct ath12k_dp_global_ast_table *ast_base;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "ast info setup dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	ast_base = ath12k_dp_get_global_ast_table(dp_hw_grp);
	if (!ast_base) {
		ath12k_err(NULL, "ast info setup unable to fetch ast_base\n");
		return -EINVAL;
	}

	ath12k_dp_ast_param_init(ast_base, &ast_info);
	return ath12k_dp_rx_htt_ast_info_setup(dp->ab, &ast_info);
}

int ath12k_ast_entry_rhash_add(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry *sw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	int ret;

	if (!ast_base->rhead_ast_entry)
		return -EPERM;

	if (sw_ast_entry->rhash_done)
		return 0;

	ret = rhashtable_lookup_insert_fast(ast_base->rhead_ast_entry,
					    &sw_ast_entry->rhash_addr,
					    ast_base->rhash_ast_entry_param);
	if (ret)
		sw_ast_entry->rhash_done = false;
	else
		sw_ast_entry->rhash_done = true;

	return ret;
}

int ath12k_ast_entry_rhash_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_ast_entry *sw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	int ret;

	if (!ast_base->rhead_ast_entry)
		return -EPERM;

	if (!sw_ast_entry->rhash_done)
		return 0;

	ret = rhashtable_remove_fast(ast_base->rhead_ast_entry,
				     &sw_ast_entry->rhash_addr,
				     ast_base->rhash_ast_entry_param);

	if (ret && ret != -ENOENT)
		return ret;

	sw_ast_entry->rhash_done = false;

	return ret;
}

struct ath12k_ast_entry *
ath12k_ast_entry_find_by_addr(struct ath12k_dp_hw_group *dp_hw_grp, const u8 *addr)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base->rhead_ast_entry)
		return NULL;

	return rhashtable_lookup_fast(ast_base->rhead_ast_entry, addr,
			ast_base->rhash_ast_entry_param);
}

int ath12k_dp_ast_entry_rhash_tbl_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct rhashtable_params *param;
	struct rhashtable *rhash_ast_tbl;
	int ret;
	size_t size;

	if (ast_base->rhead_ast_entry)
		return 0;

	size = sizeof(*ast_base->rhead_ast_entry);
	rhash_ast_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_ast_tbl)
		return -ENOMEM;

	param = &ast_base->rhash_ast_entry_param;

	param->key_offset = offsetof(struct ath12k_ast_entry, mac_addr);
	param->head_offset = offsetof(struct ath12k_ast_entry, rhash_addr);
	param->key_len = sizeof_field(struct ath12k_ast_entry, mac_addr);
	param->automatic_shrinking = true;
	param->nelem_hint = ast_base->num_ast_entries;

	ret = rhashtable_init(rhash_ast_tbl, param);
	if (ret)
		goto err_free;

	ast_base->rhead_ast_entry = rhash_ast_tbl;

	return 0;

err_free:
	kfree(rhash_ast_tbl);

	return ret;
}

void ath12k_dp_ast_entry_tbl_destroy(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (!ast_base->rhead_ast_entry)
		return;

	rhashtable_destroy(ast_base->rhead_ast_entry);
	kfree(ast_base->rhead_ast_entry);
	ast_base->rhead_ast_entry = NULL;
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
				GFP_KERNEL);
	if (!ast_base->ast_vaddr_unaligned) {
		ath12k_err(ab, "failed to allocate memory for global AST table");
		return -ENOMEM;
	}

	ast_base->ast_vaddr_aligned = PTR_ALIGN(ast_base->ast_vaddr_unaligned,
						HAL_HW_AST_ENTRY_ALIGN);
	ast_base->ast_paddr = ath12k_core_dma_map_single(dev,
							 ast_base->ast_vaddr_aligned,
							 ast_base->hw_ast_table_size,
							 DMA_BIDIRECTIONAL);
	if (!ast_base->ast_paddr) {
		ath12k_err(ab, "failed to map AST table\n");
		ret = -ENOMEM;
		goto free_hw_ast_table;
	}

	/* index based mapping for SW AST entries */
	ast_base->ast_entries = kzalloc((ast_base->num_ast_entries *
					 sizeof(struct ath12k_ast_entry *)),
					GFP_KERNEL);
	if (!ast_base->ast_entries) {
		ath12k_err(ab, "failed to allocate memory for AST index table\n");
		ret = -ENOMEM;
		goto unmap_hw_ast_table;
	}
	/* Hash table for SW AST entries */
	ret = ath12k_dp_ast_entry_rhash_tbl_init(dp_hw_grp);
	if (ret) {
		ath12k_err(ab, "failed to init the hash table for SW ast entries\n");
		goto free_sw_ast_table;
	}

	ast_base->hash_keys.ase_hash_key1 = ATH12K_AST_HASH_KEY_1;
	ast_base->hash_keys.ase_hash_key2 = ATH12K_AST_HASH_KEY_2;
	ast_base->hash_keys.ase_hash_key3 = ATH12K_AST_HASH_KEY_3;
	ath12k_dp_init_ast_hash_keys(ast_base->hash_keys.ase_hash_key1,
				     ast_base->hash_keys.ase_hash_key2,
				     ast_base->hash_keys.ase_hash_key3);

	spin_lock_init(&ast_base->ast_lock);

	ath12k_dp_ast_param_init(ast_base, &ast_info);
	if (!ath12k_ftm_mode)
		ath12k_wifi8_hal_hw_ase_init(ab, &ast_info);

	return 0;

free_sw_ast_table:
	kfree(ast_base->ast_entries);

unmap_hw_ast_table:
	ath12k_core_dma_unmap_single(dev, ast_base->ast_paddr,
				     ast_base->hw_ast_table_size,
				     DMA_BIDIRECTIONAL);
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
		ath12k_core_dma_unmap_single(dev, ast_base->ast_paddr,
					     ast_base->hw_ast_table_size,
					     DMA_BIDIRECTIONAL);
	}

	ath12k_dp_ast_entry_tbl_destroy(dp_hw_grp);
	kfree(ast_base->ast_entries);

	kfree(ast_base->ast_vaddr_unaligned);
	ast_base->ast_vaddr_unaligned = NULL;
	ast_base->ast_vaddr_aligned = NULL;
}

static inline bool ath12k_dp_hw_ast_entry_is_valid(struct hal_ast_entry *hw_ast_entry)
{
	return !!le32_get_bits(hw_ast_entry->info1, HAL_AST_ENTRY_INFO1_ENTRY_VALID);
}

static struct hal_ast_entry *
ath12k_dp_get_hw_ast_entry(struct ath12k_dp_hw_group *dp_hw_grp, u16 index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct hal_ast_entry *ast_table = NULL;
	struct hal_ast_entry *ast_entry = NULL;
	dma_addr_t paddr;

	if (!dev)
		return NULL;

	/* Validate index bounds */
	if (index >= ast_base->num_ast_entries)
		return NULL;

	ast_table = (struct hal_ast_entry *)ast_base->ast_vaddr_aligned;
	ast_entry = ast_table + index;

	/* invalidate the entry */
	paddr = (dma_addr_t)(((u8 *)ast_base->ast_paddr) + index * HAL_HW_AST_ENTRY_SIZE);
	ath12k_core_dma_sync_single_for_cpu(dev,
					    paddr,
					    HAL_HW_AST_ENTRY_SIZE,
					    DMA_BIDIRECTIONAL);
	return ast_entry;
}

struct ath12k_ast_entry *
ath12k_dp_get_sw_ast_entry_by_index(struct ath12k_dp_hw_group *dp_hw_grp, u16 ast_index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);

	if (ast_index >= ast_base->num_ast_entries)
		return NULL;

	return ast_base->ast_entries[ast_index];
}

void ath12k_dp_hw_ast_entry_sync(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_ast_entry *sw_ast_entry,
				 struct hal_ast_entry *hw_ast_entry)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	u32 mac_addr_31_0 = 0;
	u16 mac_addr_47_32 = 0;
	bool is_valid = !!(sw_ast_entry->ast_entry_flags & ATH12K_AST_ENTRY_IS_VALID);
	dma_addr_t paddr;

	if (is_valid) {
		/* HW AST entry setup */
		memset(hw_ast_entry, 0, sizeof(struct hal_ast_entry));
		ath12k_dp_copy_mac_addr(&mac_addr_31_0,
					&mac_addr_47_32,
					sw_ast_entry->mac_addr);

		hw_ast_entry->info0 = le32_encode_bits(mac_addr_31_0,
						       HAL_AST_ENTRY_INFO0_MAC_ADDR_31_0);
		hw_ast_entry->info1 =
			le32_encode_bits(mac_addr_47_32,
					 HAL_AST_ENTRY_INFO1_MAC_ADDR_47_32) |
			le32_encode_bits(!!(sw_ast_entry->ast_entry_flags &
					    ATH12K_AST_ENTRY_IS_MEC),
					 HAL_AST_ENTRY_INFO1_MEC) |
			le32_encode_bits(ATH12K_AST_ENTRY_WILDCARD_LINK_ID,
					 HAL_AST_ENTRY_INFO1_LINK_ID) |
			le32_encode_bits(!!(sw_ast_entry->ast_entry_flags &
					    ATH12K_AST_ENTRY_IS_USE_ADDRX),
					 HAL_AST_ENTRY_INFO1_USE_ADDRX_SEARCH) |
			le32_encode_bits(2,
					 HAL_AST_ENTRY_INFO1_NUM_WHO_CLASSIFY_INFO);
		hw_ast_entry->info2 =
			le32_encode_bits(sw_ast_entry->tx_classify_info_paddr,
					 HAL_AST_ENTRY_INFO2_WHO_CLASSIFY_INFO_31_0);
		hw_ast_entry->info3 =
		le32_encode_bits(((u64)sw_ast_entry->tx_classify_info_paddr) >> 32,
				 HAL_AST_ENTRY_INFO3_WHO_CLASSIFY_INFO_39_32);
		hw_ast_entry->info5 = le32_encode_bits(sw_ast_entry->peer_id,
						       HAL_AST_ENTRY_INFO5_SW_PEER_ID);
		hw_ast_entry->info6 = le32_encode_bits(sw_ast_entry->mld_id,
						       HAL_AST_ENTRY_INFO6_VDEV_ID);
		/* TODO: check for other variable init */

		/* Set Valid only after updating everything else */
		hw_ast_entry->info1 |= le32_encode_bits(is_valid,
							HAL_AST_ENTRY_INFO1_ENTRY_VALID);
	} else {
		hw_ast_entry->info1 &= ~HAL_AST_ENTRY_INFO1_ENTRY_VALID;
	}

	if (!dev) {
		ath12k_err(NULL, "dev is NULL during AST entry sync for index %u\n",
			   sw_ast_entry->ast_index);
		return;
	}

	/* flush the entry */
	paddr =	(dma_addr_t)(((u8 *)ast_base->ast_paddr) +
			     (sw_ast_entry->ast_index * HAL_HW_AST_ENTRY_SIZE));
	ath12k_core_dma_sync_single_for_device(dev,
					       paddr,
					       HAL_HW_AST_ENTRY_SIZE,
					       DMA_BIDIRECTIONAL);

	/* TODO send the TCL command to invalidate the cache */
}

int ath12k_dp_ast_entry_create(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_ast_entry_config_params *param)
{
	bool is_mcast = !!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MCAST);
	bool is_mec = !!(param->ast_entry_flags & ATH12K_AST_ENTRY_IS_MEC);
	struct hal_ast_entry *hw_ast_entry;
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	int ret = -ENOSPC;
	u16 i;
	u16 ast_index;
	u16 ast_hash;
	bool free_slot_found = false;

	param->ast_index = ATH12K_INVALID_AST_INDEX;
	param->ast_hash = ATH12K_INVALID_AST_INDEX;
	ast_hash = ath12k_dp_compute_ast_hash(param->mac_addr,
					      ast_base->ast_hash_mask,
					      is_mcast, is_mec);
	ast_index = ast_hash;
	spin_lock_bh(&ast_base->ast_lock);
	for (i = 0; i < ast_base->skid_len; ++i) {
		/* wrap around case */
		if (ast_index == ast_base->num_ast_entries)
			ast_index = 0;

		hw_ast_entry = ath12k_dp_get_hw_ast_entry(dp_hw_grp, ast_index);
		if (!hw_ast_entry) {
			ath12k_err(NULL,
				   "invalid hw ast entry while finding free slot %u\n",
				   ast_index);
			ret = -EINVAL;
			goto error_handle;
		}
		if (ath12k_dp_hw_ast_entry_is_valid(hw_ast_entry)) {
			ast_index++;
			continue;
		}

		free_slot_found = true;
		break;
	}

	if (!free_slot_found) {
		ath12k_err(NULL,
			   "Failed to allocate AST entry: no free slot available %u\n",
			   ast_index);
		ret = -EINVAL;
		goto error_handle;
	}

	/* SW AST entry setup */
	sw_ast_entry = kzalloc(sizeof(*sw_ast_entry), GFP_ATOMIC);
	if (!sw_ast_entry) {
		ret = -ENOMEM;
		goto error_handle;
	}

	memcpy(sw_ast_entry->mac_addr, param->mac_addr, ETH_ALEN);
	sw_ast_entry->tx_classify_info_paddr = param->tx_classify_info_paddr;
	sw_ast_entry->peer_id = param->peer_id;
	sw_ast_entry->mld_id = param->mld_id;
	sw_ast_entry->ast_entry_flags = param->ast_entry_flags;
	sw_ast_entry->ast_index = ast_index;
	sw_ast_entry->ast_hash = ast_hash;
	sw_ast_entry->ast_entry_flags |= ATH12K_AST_ENTRY_IS_VALID;

	/* Add the SW AST entry in index based and hash tables */
	ast_base->ast_entries[ast_index] = sw_ast_entry;
	ret = ath12k_ast_entry_rhash_add(dp_hw_grp, sw_ast_entry);
	if (ret) {
		ath12k_err(NULL, "Failed to add SW AST entry to hash table index = %u\n",
			   ast_index);
		ast_base->ast_entries[ast_index] = NULL;
		/* Clear the entry flags before freeing */
		sw_ast_entry->ast_entry_flags = ATH12K_AST_ENTRY_EMPTY_FLAGS;
		goto free_sw_entry;
	}
	ath12k_dp_hw_ast_entry_sync(dp_hw_grp, sw_ast_entry, hw_ast_entry);

	param->ast_index = ast_index;
	param->ast_hash = ast_hash;
	spin_unlock_bh(&ast_base->ast_lock);
	return 0;

free_sw_entry:
	kfree(sw_ast_entry);
error_handle:
	spin_unlock_bh(&ast_base->ast_lock);

	return ret;
}

void ath12k_dp_ast_entry_delete(struct ath12k_dp_hw_group *dp_hw_grp,
				u16 ast_index)
{
	struct ath12k_dp_global_ast_table *ast_base =
				ath12k_dp_get_global_ast_table(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_ast_entry *sw_ast_entry = NULL;
	struct hal_ast_entry *hw_ast_entry;

	spin_lock_bh(&ast_base->ast_lock);

	hw_ast_entry = ath12k_dp_get_hw_ast_entry(dp_hw_grp, ast_index);
	if (!hw_ast_entry) {
		ath12k_err(ab, "unable to find the hw ast entry %d\n", ast_index);
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	sw_ast_entry = ath12k_dp_get_sw_ast_entry_by_index(dp_hw_grp, ast_index);
	if (!sw_ast_entry) {
		ath12k_err(ab, "unable to find the sw ast entry %d\n", ast_index);
		spin_unlock_bh(&ast_base->ast_lock);
		return;
	}

	/* reset the valid flag */
	sw_ast_entry->ast_entry_flags &= ~ATH12K_AST_ENTRY_IS_VALID;

	ath12k_dp_hw_ast_entry_sync(dp_hw_grp, sw_ast_entry, hw_ast_entry);

	/* remove SW AST entry from index based and hash tables */
	ast_base->ast_entries[ast_index] = NULL;
	(void)ath12k_ast_entry_rhash_delete(dp_hw_grp, sw_ast_entry);

	kfree(sw_ast_entry);
	spin_unlock_bh(&ast_base->ast_lock);
}
