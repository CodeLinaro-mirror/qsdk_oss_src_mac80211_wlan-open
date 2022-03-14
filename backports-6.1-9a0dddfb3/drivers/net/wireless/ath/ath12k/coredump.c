// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/devcoredump.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/dma-direction.h>
#include <linux/mm.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/elf.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/soc/qcom/mdt_loader.h>

#include "core.h"
#include "pci.h"
#include "hif.h"
#include "coredump.h"
#include "debug.h"
#include "mhi.h"
#include "ahb.h"

struct ath12k_coredump_segment_info ath12k_coredump_seg_info;
EXPORT_SYMBOL(ath12k_coredump_seg_info);
struct ath12k_coredump_info ath12k_coredump_ram_info;
EXPORT_SYMBOL(ath12k_coredump_ram_info);

static void *ath12k_coredump_find_segment(loff_t user_offset,
                                         struct ath12k_dump_segment *segment,
                                         int num_seg, size_t *data_left)
{
       int i;

       for (i = 0; i < num_seg; i++, segment++) {
               if (user_offset < segment->len) {
                       *data_left = user_offset;
                       return segment;
               }
               user_offset -= segment->len;
       }

       *data_left = 0;
       return NULL;
}

static ssize_t ath12k_coredump_read_q6dump(char *buffer, loff_t offset, size_t count,
                                          void *data, size_t header_size)
{
       struct ath12k_coredump_state *dump_state = data;
       struct ath12k_dump_segment *segments = dump_state->segments;
       struct ath12k_dump_segment *seg;
       void *elfcore = dump_state->header;
       size_t data_left, copy_size, bytes_left = count;
       void __iomem *addr;

       /* Copy the header first */
       if (offset < header_size) {
               copy_size = header_size - offset;
               copy_size = min(copy_size, bytes_left);

               memcpy(buffer, elfcore + offset, copy_size);
               offset += copy_size;
               bytes_left -= copy_size;
               buffer += copy_size;

               return copy_size;
       }

       while (bytes_left) {
               seg = ath12k_coredump_find_segment(offset - header_size, segments,
                                                  dump_state->num_seg, &data_left);
               /* End of segments check */
               if (!seg) {
                       pr_info("Ramdump complete %lld bytes read\n", offset);
                       return 0;
               }

               if (data_left)
                       copy_size = min_t(size_t, bytes_left, data_left);
               else
                       copy_size = bytes_left;

               addr = (void __iomem *)seg->vaddr;
               addr += data_left;
               memcpy_fromio(buffer, addr, copy_size);

               offset += copy_size;
               buffer += copy_size;
               bytes_left -= copy_size;
       }

       return count - bytes_left;
}

static void ath12k_coredump_free_q6dump(void *data)
{
       struct ath12k_coredump_state *dump_state = data;

       complete(&dump_state->dump_done);
}

void ath12k_coredump_build_inline(struct ath12k_base *ab,
                                 struct ath12k_dump_segment *segments, int num_seg)
{
       struct ath12k_coredump_state dump_state;
       struct timespec64 timestamp;
       struct ath12k_dump_file_data *file_data;
       size_t header_size;
       struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
       struct device *dev;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
       u8 *buf;

       header_size = sizeof(*file_data);
       header_size += num_seg * sizeof(*segments);
       header_size = PAGE_ALIGN(header_size);
       buf = kzalloc(header_size, GFP_KERNEL);
       if (!buf) {
		ath12k_warn(ab, "Failed to allocate memory for coredump\n");
               return;
	}

       file_data = (struct ath12k_dump_file_data *)buf;
       strscpy(file_data->df_magic, "ATH12K-FW-DUMP",
               sizeof(file_data->df_magic));
       file_data->len = cpu_to_le32(header_size);
       file_data->version = cpu_to_le32(ATH12K_FW_CRASH_DUMP_V2);
	if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) {
		file_data->chip_id = ab->qmi.target.chip_id;
		file_data->qrtr_id = ab->qmi.service_ins_id;
		file_data->bus_id = ab_ahb->userpd_id;
	} else {
	       file_data->chip_id = cpu_to_le32(ar_pci->dev_id);
	       file_data->qrtr_id = cpu_to_le32(ar_pci->ab->qmi.service_ins_id);
	       file_data->bus_id = pci_domain_nr(ar_pci->pdev->bus);
	}
       dev = ab->dev;;

       guid_gen(&file_data->guid);
       ktime_get_real_ts64(&timestamp);
       file_data->tv_sec = cpu_to_le64(timestamp.tv_sec);
       file_data->tv_nsec = cpu_to_le64(timestamp.tv_nsec);
       file_data->num_seg = cpu_to_le32(num_seg);
       file_data->seg_size = cpu_to_le32(sizeof(*segments));

       /* copy segment details to file */
       buf += offsetof(struct ath12k_dump_file_data, seg);
       file_data->seg = (struct ath12k_dump_segment *)buf;
       memcpy(file_data->seg, segments, num_seg * sizeof(*segments));

       dump_state.header = file_data;
       dump_state.num_seg = num_seg;
       dump_state.segments = segments;
       init_completion(&dump_state.dump_done);

       dev_coredumpm(dev, THIS_MODULE, &dump_state, header_size, GFP_KERNEL,
                     ath12k_coredump_read_q6dump, ath12k_coredump_free_q6dump);

       /* Wait until the dump is read and free is called */
       wait_for_completion(&dump_state.dump_done);
       kfree(file_data);
}

enum
ath12k_fw_crash_dump_type ath12k_coredump_get_dump_type(enum ath12k_qmi_target_mem type)
{
	enum ath12k_fw_crash_dump_type dump_type;

	switch (type) {
	case HOST_DDR_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_REMOTE_MEM_DATA;
		break;
	case M3_DUMP_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_M3_DUMP;
		break;
	case PAGEABLE_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_PAGEABLE_DATA;
		break;
	case CALDB_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_CALDB_DATA;
		break;
	case MLO_GLOBAL_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_MLO_GLOBAL_DATA;
		break;
	case AFC_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_AFC_DATA;
		break;
	default:
		dump_type = FW_CRASH_DUMP_TYPE_MAX;
		break;
	}

	return dump_type;
}

#ifdef CPTCFG_ATH12K_COREDUMP
void ath12k_coredump_upload(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, dump_work);

	ath12k_info(ab, "Uploading coredump\n");
	// dev_coredumpv() takes ownership of the buffer
	dev_coredumpv(ab->dev, ab->dump_data, ab->ath12k_coredump_len, GFP_KERNEL);
	ab->dump_data = NULL;
}
#endif

static void ath12k_coredump_q6crash_reason(struct ath12k_base *ab)
{
        int i = 0;
        uint64_t coredump_offset = 0;
        struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
        struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
        struct mhi_buf *mhi_buf;
        struct image_info *rddm_image;
        struct ath12k_coredump_q6ramdump_header *ramdump_header;
        struct ath12k_coredump_q6ramdump_entry *ramdump_table;
        char *msg = NULL;
        struct pci_dev *pci_dev = ar_pci->pdev;

        rddm_image = mhi_ctrl->rddm_image;
        mhi_buf = rddm_image->mhi_buf;

        ath12k_info(ab, "CRASHED - [DID:DOMAIN:BUS:SLOT] - %x:%04u:%02u:%02u\n",
                    pci_dev->device, pci_dev->bus->domain_nr,
                    pci_dev->bus->number, PCI_SLOT(pci_dev->devfn));

        /* Get RDDM header size */
        ramdump_header = (struct ath12k_coredump_q6ramdump_header *)mhi_buf[0].buf;
        ramdump_table = ramdump_header->ramdump_table;
        coredump_offset = le32_to_cpu(ramdump_header->header_size);

        /* Traverse ramdump table to get coredump offset */
        while (i < MAX_RAMDUMP_TABLE_SIZE) {
                if (!strncmp(ramdump_table->description, COREDUMP_DESC,
                             sizeof(COREDUMP_DESC)) ||
                    !strncmp(ramdump_table->description, Q6_SFR_DESC,
                             sizeof(Q6_SFR_DESC))) {
                        break;
                }
                coredump_offset += le64_to_cpu(ramdump_table->size);
                ramdump_table++;
                i++;
        }

        if (i == MAX_RAMDUMP_TABLE_SIZE) {
                ath12k_warn(ab, "Cannot find '%s' entry in ramdump\n",
                            COREDUMP_DESC);
                return;
        }

        /* Locate coredump data from the ramdump segments */
        for (i = 0; i < rddm_image->entries; i++) {
                if (coredump_offset < mhi_buf[i].len) {
                        msg = mhi_buf[i].buf + coredump_offset;
                        break;
                }

                coredump_offset -= mhi_buf[i].len;
        }

        if (msg && msg[0])
                ath12k_err(ab, "Fatal error received from wcss!\n%s\n",
                            msg);
}

void ath12k_coredump_download_rddm(struct ath12k_base *ab)
{
       struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
       struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
       struct image_info *rddm_img, *fw_img;
       struct ath12k_dump_segment *segment, *seg_info;
       int i, rem_seg_cnt = 0, len, num_seg, seg_sz, qdss_seg_cnt = 1;

	int skip_count = 0;
	enum ath12k_fw_crash_dump_type mem_type;
	struct ath12k_coredump_segment_info *chip_seg;
	int dump_count;
	struct ath12k_hw_group *ag = ab->ag;
	bool state = false;

	if (ab->in_panic)
		state = true;


       ath12k_mhi_coredump(mhi_ctrl, state);
        ath12k_coredump_q6crash_reason(ab);

       rddm_img = mhi_ctrl->rddm_image;
       fw_img = mhi_ctrl->fbc_image;

       for (i = 0; i < ab->qmi.mem_seg_count; i++) {
               if (ab->qmi.target_mem[i].type == HOST_DDR_REGION_TYPE ||
                   (ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE &&
		   ath12k_cold_boot_cal && ab->hw_params->cold_boot_calib) ||
                   ab->qmi.target_mem[i].type == M3_DUMP_REGION_TYPE ||
			ab->qmi.target_mem[i].type == PAGEABLE_MEM_REGION_TYPE ||
			ab->qmi.target_mem[i].type == MLO_GLOBAL_MEM_REGION_TYPE ||
			ab->qmi.target_mem[i].type == AFC_REGION_TYPE)

                       rem_seg_cnt++;
       }

       num_seg = fw_img->entries + rddm_img->entries + rem_seg_cnt;

	if (ab->is_qdss_tracing)
		num_seg += qdss_seg_cnt;

       len = num_seg * sizeof(*segment);

       segment = kzalloc(len, GFP_NOWAIT);
       if (!segment) {
		ath12k_err(ab, " Failed to allocate memory for segment for rddm download\n");
               return;
	}

       seg_info = segment;
       for (i = 0; i < fw_img->entries ; i++) {

		if (!fw_img->mhi_buf[i].buf) {
			skip_count++;
			continue;
		}
               seg_sz = fw_img->mhi_buf[i].len;
               seg_info->len = PAGE_ALIGN(seg_sz);
               seg_info->addr = fw_img->mhi_buf[i].dma_addr;
               seg_info->vaddr = fw_img->mhi_buf[i].buf;
               seg_info->type = FW_CRASH_DUMP_PAGING_DATA;
               seg_info++;
       }

       for (i = 0; i < rddm_img->entries; i++) {

		if (!rddm_img->mhi_buf[i].buf) {
			skip_count++;
			continue;
		}

               seg_sz = rddm_img->mhi_buf[i].len;
               seg_info->len = PAGE_ALIGN(seg_sz);
               seg_info->addr = rddm_img->mhi_buf[i].dma_addr;
               seg_info->vaddr = rddm_img->mhi_buf[i].buf;
               seg_info->type = FW_CRASH_DUMP_RDDM_DATA;
               seg_info++;
       }

       for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		mem_type = ath12k_coredump_get_dump_type(ab->qmi.target_mem[i].type);
		if(mem_type == FW_CRASH_DUMP_TYPE_MAX) {
			ath12k_info(ab, "target mem region type %d not supported", ab->qmi.target_mem[i].type);
			continue;
		}

		if (mem_type == FW_CRASH_DUMP_CALDB_DATA &&
			!(ath12k_cold_boot_cal &&
			ab->hw_params->cold_boot_calib))
			continue;

		if (!ab->qmi.target_mem[i].paddr) {
			skip_count++;
			ath12k_info(ab, "Skipping mem region type %d", ab->qmi.target_mem[i].type);
			continue;
		}
		seg_info->len = ab->qmi.target_mem[i].size;
		seg_info->addr = ab->qmi.target_mem[i].paddr;
		seg_info->vaddr = ab->qmi.target_mem[i].v.ioaddr;
		seg_info->type = mem_type;
		ath12k_info(ab,
		    "seg vaddr is %px len is 0x%x type %d\n",
			    seg_info->vaddr,
			    seg_info->len,
			    seg_info->type);
		seg_info++;

       }


	if (ab->is_qdss_tracing) {
		seg_info->len = ab->qmi.target_mem[0].size;
		seg_info->addr = ab->qmi.target_mem[0].paddr;
		seg_info->vaddr = ab->qmi.target_mem[0].v.ioaddr;
		seg_info->type = FW_CRASH_DUMP_AFC_DATA;
		seg_info++;
	}
	num_seg = num_seg - skip_count;

	if (!ab->fw_recovery_support || ab->in_panic) {
		if (ag->mlo_capable) {
			dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
			if (dump_count >= ATH12K_MAX_SOCS) {
				ath12k_err(ab, "invalid chip number %d\n",
					   dump_count);
				return;
			} else {
				chip_seg = &ath12k_coredump_ram_info.chip_seg_info[dump_count];
				chip_seg->chip_id = ar_pci->dev_id;
				chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
				chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
				chip_seg->num_seg = num_seg;
				chip_seg->seg = segment;
				atomic_inc(&ath12k_coredump_ram_info.num_chip);
			}
		} else {
			/* This part of code for 12.2 without mlo_capable=1 */
			dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
			chip_seg = &ath12k_coredump_ram_info.chip_seg_info[dump_count];
			chip_seg->chip_id = ar_pci->dev_id;
			chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
			chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
			chip_seg->num_seg = num_seg;
			chip_seg->seg = segment;
			atomic_inc(&ath12k_coredump_ram_info.num_chip);
		}

		chip_seg = &ath12k_coredump_seg_info;
		chip_seg->chip_id = ar_pci->dev_id;
		chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
		chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
		chip_seg->num_seg = num_seg;
		chip_seg->seg = segment;

		ath12k_core_issue_bug_on(ab);

	} else if (!ab->in_panic) {
		ath12k_info(ab, "WLAN target is restarting");
		ath12k_coredump_build_inline(ab, segment, num_seg);
		kfree(segment);
	}

}
#ifdef CPTCFG_ATH12K_COREDUMP
void ath12k_coredump_collect(struct ath12k_base *ab)
{
	ath12k_coredump_q6crash_reason(ab);
	ath12k_hif_coredump_download(ab);
}
#endif
