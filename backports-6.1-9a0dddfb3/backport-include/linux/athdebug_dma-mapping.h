/*
 *Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __ATHDEBUG_DMA_MAPPING_H
#define __ATHDEBUG_DMA_MAPPING_H

#include <linux/ath_memdebug.h>

#define dma_alloc_coherent(dev, len, handle, flags)		ath_dma_alloc_coherent(dev, len, handle, flags, __LINE__, __func__);
#define dma_free_coherent(dev, size, cpu_addr, dma_handle)	ath_dma_free_coherent(dev, size, cpu_addr, dma_handle);

#endif /* __ATHDEBUG_DMA_MAPPING_H */
