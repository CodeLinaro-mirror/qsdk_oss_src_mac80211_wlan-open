/*
 *Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __ATHDEBUG_NETLINK_H
#define __ATHDEBUG_NETLINK_H

#include <linux/ath_memdebug.h>

#define nlmsg_new(len, flags)		ath_nlmsg_new(len, flags, __LINE__, __func__)

#endif /* __ATHDEBUG_NETLINK_H */
