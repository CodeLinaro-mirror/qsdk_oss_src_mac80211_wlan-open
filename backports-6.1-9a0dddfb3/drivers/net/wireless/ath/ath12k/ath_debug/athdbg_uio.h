/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef _ATHDBG_UIO_H_
#define _ATHDBG_UIO_H_

#include <linux/stdarg.h>
#include "../core.h"

#define ATH12K_DEV "host"

void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab,
					 u32 crit_enum);

struct ath12k_crit_record {
	u64 ts_nsec;
	u32 crit_enum;
	u16 radio_id;
} __packed;

struct athdbg_uio_trace {
	u32 dropped;
};

#endif /* _ATHDBG_UIO_H_ */
