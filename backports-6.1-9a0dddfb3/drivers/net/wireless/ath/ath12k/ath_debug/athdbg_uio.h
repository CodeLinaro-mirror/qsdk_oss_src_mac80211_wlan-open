/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef _ATHDBG_UIO_H_
#define _ATHDBG_UIO_H_

#include <linux/stdarg.h>
#include "../core.h"

/* Log levels */
#define ATH12K_LOG_LEVEL_DEBUG 0
#define ATH12K_LOG_LEVEL_INFO  1
#define ATH12K_LOG_LEVEL_WARN  2
#define ATH12K_LOG_LEVEL_ERR   3

struct ath12k_debug_log_entry {
	u64 timestamp;     /* ns */
	u64 debug_mask;    /* which mask triggered */
	u8  log_level;     /* 0=debug,1=info,2=warn,3=err */
	char message[256];
};

/* Ring sizes (match your earlier values) */
#define ATH12K_UIO_DBG_SIZE_BYTES  (256 * 1024)
#define ATH12K_UIO_CRIT_SIZE_BYTES (128 * 1024)

int athdbg_uio_register(struct ath12k_base *ab);
void athdbg_uio_unregister(struct ath12k_base *ab);
void athdbg_uio_log_info(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_warn(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_err(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_debug(struct ath12k_base *ab, u64 mask, const char *fmt,
			  va_list args);

#endif /* _ATHDBG_UIO_H_ */
