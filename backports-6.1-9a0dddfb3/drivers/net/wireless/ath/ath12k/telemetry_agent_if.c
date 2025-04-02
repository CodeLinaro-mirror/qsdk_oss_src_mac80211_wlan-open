// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/export.h>

struct telemetry_agent_ops *g_agent_ops;

EXPORT_SYMBOL(g_agent_ops);

int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops) {
	return 0;
}
EXPORT_SYMBOL(register_telemetry_agent_ops);

int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops){
	return 0;
}
EXPORT_SYMBOL(unregister_telemetry_agent_ops);

