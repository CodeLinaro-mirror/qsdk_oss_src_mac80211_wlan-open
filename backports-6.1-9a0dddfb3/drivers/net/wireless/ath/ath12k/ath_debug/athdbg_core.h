/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef DEBUG_CORE_H
#define DEBUG_CORE_H
#include "../core.h"
#include <linux/workqueue.h>
#include "../athdbg_if.h"

#define DEV_NAME_LEN 20
#define BUS_NAME_LEN 4
#define QMI_HANDLER 5

/*Need to be enabled for upstream Build for QDSS*/
//#define CONFIG_UPSTREAM_BUILD

struct pci_bus_info{
	int domain;
	int bus;
	int slot;
	int func;
};

#ifdef CONFIG_UPSTREAM_BUILD
struct ath12k_dump_segment {
	unsigned long addr;
	void *vaddr;
	unsigned int len;
	unsigned int type;
	struct completion dump_done;
};
#endif

enum athdbg_request_type{
	ATH_DBG_REQ_UNKNOWN = 0x00,
	ATH_DBG_REQ_SETMASK = 0x01,
	ATH_DBG_REQ_COLLECT_MINI_DUMP = 0x02,
	ATH_DBG_REQ_ENABLE_QDSS = 0x04,
	ATH_DBG_REQ_DUMP_QDSS = 0x08,
	ATH_DBG_REQ_WMI_ENABLE = 0x10,
	ATH_DBG_REQ_WMI_DUMP   = 0x20,
	ATH_DBG_REQ_WMI_VERBOSITY = 0x40,
};

enum athdbg_qdss_dump_type {
	ATHDBG_QDSS_DUMP  = 1,
	ATHDBG_PHYA0_DUMP = 64,
	ATHDBG_PHYA1_DUMP = 128,
};

struct athdbg_request{
	struct ath12k_base *ab;
	struct list_head req_list;
	enum athdbg_request_type req_type;
	unsigned int data;
	char *input_buf;
};

struct ath_debug_base {
	struct work_struct dbg_wk;
	struct workqueue_struct *dbg_wq;
	struct pci_bus_info pci_dev;
	struct ath12k_base *drv_ab;
	struct list_head req_list;
	struct mutex req_lock;
	const struct athdbg_to_ath12k_ops *dbg_to_ath_ops;
	struct qmi_msg_handler *wdbg_handlers[QMI_HANDLER];
	u8 wdbg_handlers_cnt;
	char dev[DEV_NAME_LEN];
	char bus[BUS_NAME_LEN];
};

unsigned int athdbg_conv_str_to_dbgmask(char *dbgmask);
#endif
