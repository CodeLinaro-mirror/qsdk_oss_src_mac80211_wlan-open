// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/debugfs.h>
#include <linux/init.h>

#include "athdbg_core.h"
#include "athdbg_minidump.h"
#include "athdbg_wmi_recording.h"

extern struct ath_debug_base *athdbg_base;
const struct file_operations debugfs_req_fops;
const struct file_operations debugfs_mask_fops;
const struct file_operations debugfs_qdss_enable_fops;
const struct file_operations debugfs_qdss_collect_fops;

#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
static ssize_t athdbg_minidump_read(struct file *file, char __user *user_buf,
									size_t count, loff_t *ppos)
{
	static const char debugfs_data[] =
	"USAGE:\n"
	"echo 1 > collect\n"
	"echo 1 > show_all_alloc_struct\n"
	"echo <struct_name> > add_to_dump_list\n"
	"echo 1 > show_dump_list\n"
	"echo <0/1> > disable_minidump\n";

	return simple_read_from_buffer(user_buf, count, ppos, debugfs_data, sizeof(debugfs_data));
}


static ssize_t athdbg_minidump_write(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	const char *filename = file->f_path.dentry->d_name.name;
	char buf[128] = {0};
	int ret = 0, len = 0;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len <= 0) {
		pr_err("athdbg_core: Invalid Input %d %zu", len, count);
		ret = count;
		goto exit;
	}

	buf[len] = '\0';

	strim(buf);

	len = strlen(buf);
	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->input_buf = kzalloc(len + 1, GFP_ATOMIC);
	if (!dbg_req->input_buf) {
		kfree(dbg_req);
		return -ENOMEM;
	}
	ret = strscpy(dbg_req->input_buf, buf, len + 1);
	if (ret < 0) {
		ret = count;
		kfree(dbg_req->input_buf);
		kfree(dbg_req);
		return ret;
	}
	dbg_req->data = get_minidump_req(filename);
	dbg_req->req_type = ATH_DBG_REQ_COLLECT_MINI_DUMP;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_minidump_fops = {
	.read = athdbg_minidump_read,
	.write = athdbg_minidump_write,
	.open = simple_open,
	.owner = THIS_MODULE,
};

void athdbg_create_minidump_debugfs(struct dentry *dbg_dir,
				    struct ath12k_base *drv_ab)
{
	struct dentry	*minidump_dir;
	int i = 0;

	minidump_dir = debugfs_create_dir("minidump", dbg_dir);

	if (IS_ERR_OR_NULL(minidump_dir))
		return;

	while (athdbg_debugfs_handlers[i].filename != NULL) {
		debugfs_create_file(athdbg_debugfs_handlers[i].filename,
				    athdbg_debugfs_handlers[i].permissions,
				    minidump_dir, drv_ab,
				    &debugfs_minidump_fops);
		i++;
	}
}
EXPORT_SYMBOL(athdbg_create_minidump_debugfs);
#endif

static ssize_t athdbg_mask_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	u64 mask = 0;
	char buf[512];
	int len = 0, ret;

	len += scnprintf(buf + len, sizeof(buf) - len,
		       "echo <mask>:<mask>:<mask> > dbgmask \t\n"
		       "echo <mask> > dbgmask\n");

	if (athdbg_base && athdbg_base->dbg_to_ath_ops &&
		athdbg_base->dbg_to_ath_ops->get_dbg_mask)
		mask = athdbg_base->dbg_to_ath_ops->get_dbg_mask();

	len += scnprintf(buf + len, sizeof(buf) - len, "Current: ");
	ret = athdbg_dbgmask_to_str(mask, buf + len, sizeof(buf) - len);
	if (ret < 0)
		return ret;
	len += ret;

	len += scnprintf(buf + len, sizeof(buf) - len, "\n");
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t athdbg_mask_write(struct file *file,
		const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	u64 dbg_mask = 0;
	char buf[256] = {0};
	char ip_mask[256] = {0};
	char *token, *tmp_mask;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("athdbg_core: Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';
	strim(buf);

	if (!buf[0])
		goto exit;

	ret = sscanf(buf, "%255s", ip_mask);
	if (ret <= 0)
		goto exit;

	tmp_mask = ip_mask;
	while ((token = strsep(&tmp_mask, ":")) != NULL) {
		if (!*token)
			continue;
		dbg_mask |= athdbg_conv_str_to_dbgmask(token);
	}

	if (!dbg_mask) {
		pr_err("athdbg_core: unknown dbg request");
		ret = count;
		return ret;
	}

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_SETMASK;
	dbg_req->data = dbg_mask;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_mask_fops = {
	.read = athdbg_mask_read,
	.write = athdbg_mask_write,
	.open = simple_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_mask_fops);

static ssize_t athdbg_qdss_enable_read(struct file *file,
				       char __user *user_buf,
				       size_t count,
					loff_t *ppos)
{
	static const char debugfs_data[] =
		" 1 - enable debugfs\n";

	return simple_read_from_buffer(user_buf, count, ppos,
					debugfs_data,
					sizeof(debugfs_data));
}


static ssize_t athdbg_qdss_enable_write(struct file *file,
					const char __user *user_buf,
					size_t count,
					loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	u8 enable_qdss;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';

	if (kstrtou8(buf, 10, &enable_qdss))
		return -EINVAL;

	if (enable_qdss == 1 && ab->is_qdss_tracing)
		return count;

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_ENABLE_QDSS;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}


const struct file_operations debugfs_qdss_enable_fops = {
	.read = athdbg_qdss_enable_read,
	.write = athdbg_qdss_enable_write,
	.open = simple_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_qdss_enable_fops);

static ssize_t athdbg_qdss_collect_read(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *ppos)
{
	static const char debugfs_data[] =
		"echo 0x1  - QDSS Dump\n"
		"echo 0x40 - PHYA0 Dump\n"
		"echo 0x80 - PHYA1 Dump\n";

	return simple_read_from_buffer(user_buf, count, ppos,
					debugfs_data,
					sizeof(debugfs_data));
}

static ssize_t athdbg_qdss_collect_write(struct file *file,
					 const char __user *user_buf,
					 size_t count,
					 loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	u32 val;
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret <= 0) {
		pr_err("Invalid Input %d %zu", ret, count);
		goto exit;
	}

	buf[ret] = '\0';

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	switch (val) {
	case ATHDBG_QDSS_DUMP:
	case ATHDBG_PHYA0_DUMP:
		break;

	case ATHDBG_PHYA1_DUMP:
		if (!ab->is_dualmac) {
			pr_err("PHYA1 dump not supported %x\n", val);
			return -EINVAL;
		}
		break;

	default:
		pr_err("Invalid value %x\n", val);
		return -EINVAL;
	}

	if (!ab->is_qdss_tracing) {
		pr_err("QDSS tracing is not enabled\n");
		return count;
	}

	/* Verify firmware is ready to accept QDSS commands */
	if (!test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags)) {
		pr_err("Firmware not ready for QDSS collection\n");
		return -EAGAIN;
	}

	if (!val)
		return ret;

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->req_type = ATH_DBG_REQ_DUMP_QDSS;
	dbg_req->data = val;
	dbg_req->ab = ab;

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	ret = count;

exit:
	return ret;
}

const struct file_operations debugfs_qdss_collect_fops = {
	.read = athdbg_qdss_collect_read,
	.write = athdbg_qdss_collect_write,
	.open = simple_open,
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(debugfs_qdss_collect_fops);

static ssize_t athdbg_wmi_common_read(struct file *file, char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	static const char usage[] =
		"Usage:\n"
		"1. Enable/Disable Recording\n"
		"   echo 0 > enable        : Disable recording\n"
		"   echo 1 > enable        : Enable (Default size 1024)\n"
		"   echo NNNN > enable     : Enable with buffer size NNNN\n"
		"\n"
		"2. Control Verbosity Level of Logs\n"
		"   echo 0 > verbosity     : Minimal (Header only)\n"
		"   echo 1 > verbosity     : Standard (Header + 8 bytes data)\n"
		"   echo 2 > verbosity     : Full (Header + 16 bytes data)\n"
		"\n"
		"3. Print Logs to Console\n"
		"   echo 1 > dump          : Dump WMI Commands\n"
		"   echo 2 > dump          : Dump WMI Events\n"
		"   echo 3 > dump          : Dump WMI TX Completions\n";

	return simple_read_from_buffer(user_buf, count, ppos, usage, sizeof(usage));
}

static ssize_t athdbg_wmi_common_write(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct ath12k_base *ab = file->private_data;
	enum athdbg_wmi_request wmi_req;
	struct athdbg_request *dbg_req;
	char buf[128] = {0};
	int len;

	if (!ab)
		return -EINVAL;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (len <= 0)
		return count;

	buf[len] = '\0';
	strim(buf);

	dbg_req = kzalloc(sizeof(*dbg_req), GFP_ATOMIC);
	if (!dbg_req)
		return -ENOMEM;

	dbg_req->input_buf = kstrdup(buf, GFP_ATOMIC);
	if (!dbg_req->input_buf) {
		kfree(dbg_req);
		return -ENOMEM;
	}

	dbg_req->ab = ab;
	wmi_req = athdbg_wmi_get_req_from_name(file->f_path.dentry->d_name.name);

	switch (wmi_req) {
	case ATHDBG_WMI_REQ_ENABLE:
		dbg_req->req_type = ATH_DBG_REQ_WMI_ENABLE;
		break;
	case ATHDBG_WMI_REQ_VERBOSITY:
		dbg_req->req_type = ATH_DBG_REQ_WMI_VERBOSITY;
		break;
	case ATHDBG_WMI_REQ_DUMP:
		dbg_req->req_type = ATH_DBG_REQ_WMI_DUMP;
		break;
	default:
		dbg_req->req_type = ATH_DBG_REQ_UNKNOWN;
		break;
	}

	mutex_lock(&athdbg_base->req_lock);
	list_add_tail(&dbg_req->req_list, &athdbg_base->req_list);
	mutex_unlock(&athdbg_base->req_lock);

	queue_work(athdbg_base->dbg_wq, &athdbg_base->dbg_wk);

	return count;
}

const struct file_operations debugfs_wmi_common_fops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.read  = athdbg_wmi_common_read,
	.write = athdbg_wmi_common_write,
};
EXPORT_SYMBOL(debugfs_wmi_common_fops);

void athdbg_create_wmi_debugfs(struct dentry *dbg_dir, struct ath12k_base *drv_ab)
{
	struct dentry *wmi_dir;

	wmi_dir = debugfs_create_dir("wmi_recording", dbg_dir);
	if (IS_ERR_OR_NULL(wmi_dir))
		return;

	debugfs_create_file("enable", 0644, wmi_dir, drv_ab,
			    &debugfs_wmi_common_fops);
	debugfs_create_file("dump", 0200, wmi_dir, drv_ab,
			    &debugfs_wmi_common_fops);
	debugfs_create_file("verbosity", 0644, wmi_dir, drv_ab,
			    &debugfs_wmi_common_fops);
}
EXPORT_SYMBOL(athdbg_create_wmi_debugfs);

