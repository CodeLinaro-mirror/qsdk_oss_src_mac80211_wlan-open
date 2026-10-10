// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include "ptp.h"

/* ─ sysfs: /sys/class/timesync_mode/current_mode/mode (read)
 * Kept as a sysfs class device so hotplug scripts can read the role
 * and receive uevents via kobject_uevent_env().
 */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ath12k_hw_group *ag = dev_get_drvdata(dev);

	if (!ag)
		return -ENODEV;

	return sysfs_emit(buf, "%s\n",
			  (ag->timesync_ctx.current_ts_mode == ROOT_MODE)
			  ? "ROOT" : "STA_OR_REPEATER");
}
static DEVICE_ATTR_RO(mode);

/* ─ debugfs: timesync_debug_enable (rw)
 * /sys/kernel/debug/ath12k/timesync/debug_enable
 * Used to enable/disable timesync debug messages.
 */
static ssize_t ath12k_ts_debug_enable_read(struct file *file, char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath12k_debugfs_ctx *ctx = file->private_data;
	struct ath12k_hw_group *ag = ctx->data;
	char buf[4];
	int len;

	len = scnprintf(buf, sizeof(buf), "%u\n",
			ag->timesync_ctx.ath12k_ts_debug_enable ? 1 : 0);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t ath12k_ts_debug_enable_write(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath12k_debugfs_ctx *ctx = file->private_data;
	struct ath12k_hw_group *ag = ctx->data;
	u32 val;
	int ret;

	ret = kstrtouint_from_user(ubuf, count, 0, &val);
	if (ret)
		return ret;

	ag->timesync_ctx.ath12k_ts_debug_enable = !!val;
	return count;
}

static const struct file_operations fops_ts_debug_enable = {
	.read  = ath12k_ts_debug_enable_read,
	.write = ath12k_ts_debug_enable_write,
	.open  = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

/* ─ debugfs: timesync_sampling_rate (rw)
 * /sys/kernel/debug/ath12k/timesync/sample_hz
 * Used to control the frequency rate for timesync operation
 */
static ssize_t ath12k_ts_sampling_rate_read(struct file *file, char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct ath12k_debugfs_ctx *ctx = file->private_data;
	struct ath12k_hw_group *ag = ctx->data;
	char buf[16];
	int len;

	len = scnprintf(buf, sizeof(buf), "%u\n",
			ag->timesync_ctx.ath12k_ts_sampling_rate);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t ath12k_ts_sampling_rate_write(struct file *file,
					 const char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath12k_debugfs_ctx *ctx = file->private_data;
	struct ath12k_hw_group *ag = ctx->data;
	u32 val;
	int ret;

	ret = kstrtouint_from_user(ubuf, count, 0, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	ag->timesync_ctx.ath12k_ts_sampling_rate = val;
	return count;
}

static const struct file_operations fops_ts_sampling_rate = {
	.read  = ath12k_ts_sampling_rate_read,
	.write = ath12k_ts_sampling_rate_write,
	.open  = ath12k_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

/* ─ timesync_mode sysfs class (hotplug uevent path only)
 * Creates /sys/class/timesync_mode/current_ts_mode/mode.
 * All other timesync tunables live in debugfs (see above).
 */
int ath12k_ts_mode_sysfs_init(struct ath12k_hw_group *ag)
{
	int ret;

	ag->timesync_ctx.ts_mode_class = class_create("timesync_mode");
	if (IS_ERR(ag->timesync_ctx.ts_mode_class))
		return PTR_ERR(ag->timesync_ctx.ts_mode_class);

	ag->timesync_ctx.ts_mode_dev =
		device_create(ag->timesync_ctx.ts_mode_class, NULL, 0, ag,
			      "current_ts_mode");
	if (IS_ERR(ag->timesync_ctx.ts_mode_dev)) {
		ret = PTR_ERR(ag->timesync_ctx.ts_mode_dev);
		ag->timesync_ctx.ts_mode_dev = NULL;
		goto err_class;
	}

	ret = device_create_file(ag->timesync_ctx.ts_mode_dev, &dev_attr_mode);
	if (ret)
		goto err_dev;

	return 0;

err_dev:
	device_destroy(ag->timesync_ctx.ts_mode_class, 0);
	ag->timesync_ctx.ts_mode_dev = NULL;
err_class:
	class_destroy(ag->timesync_ctx.ts_mode_class);
	ag->timesync_ctx.ts_mode_class = NULL;
	return ret;
}

void ath12k_ts_mode_sysfs_deinit(struct ath12k_hw_group *ag)
{
	if (ag->timesync_ctx.ts_mode_dev) {
		device_remove_file(ag->timesync_ctx.ts_mode_dev, &dev_attr_mode);
		device_destroy(ag->timesync_ctx.ts_mode_class, 0);
		ag->timesync_ctx.ts_mode_dev = NULL;
	}
	if (ag->timesync_ctx.ts_mode_class) {
		class_destroy(ag->timesync_ctx.ts_mode_class);
		ag->timesync_ctx.ts_mode_class = NULL;
	}
}

/* ─ debugfs timesync directory init─
 * Creates /sys/kernel/debug/ath12k/timesync/{debug_enable,sampling_rate}
 * The debugfs_lookup guard ensures
 * a single directory even when multiple radios call this.
 */
static void ath12k_timesync_debugfs_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct dentry *debugfs_ath12k;
	struct dentry *dir;

	debugfs_ath12k = debugfs_lookup("ath12k", NULL);
	if (!debugfs_ath12k)
		return;

	/* Only create once — skip if already exists */
	dir = debugfs_lookup("timesync", debugfs_ath12k);
	if (dir) {
		dput(dir);
		dput(debugfs_ath12k);
		return;
	}

	dir = debugfs_create_dir("timesync", debugfs_ath12k);
	dput(debugfs_ath12k);
	if (IS_ERR_OR_NULL(dir))
		return;

	ath12k_debugfs_create_file("debug_enable", 0600, dir, ab, ag,
				   &fops_ts_debug_enable);
	ath12k_debugfs_create_file("sampling_rate", 0600, dir, ab, ag,
				   &fops_ts_sampling_rate);

	ath12k_ts_mode_sysfs_init(ag);
}

int ath12k_timesync_init(struct ath12k_base *ab)
{
	ath12k_timesync_debugfs_init(ab);
	return 0;
}

void ath12k_timesync_deinit(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	ath12k_ts_mode_sysfs_deinit(ag);
}
