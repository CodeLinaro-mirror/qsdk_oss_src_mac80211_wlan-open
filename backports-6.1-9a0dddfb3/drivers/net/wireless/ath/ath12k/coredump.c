// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/devcoredump.h>
#include <linux/pci.h>

#include "pci.h"
#include "hif.h"
#include "coredump.h"
#include "debug.h"

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
	case BDF_MEM_REGION_TYPE:
	case CALDB_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_NONE;
		break;
	case MLO_GLOBAL_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_MLO_GLOBAL_DATA;
		break;
	default:
		dump_type = FW_CRASH_DUMP_TYPE_MAX;
		break;
	}

	return dump_type;
}

void ath12k_coredump_upload(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, dump_work);

	ath12k_info(ab, "Uploading coredump\n");
	/* dev_coredumpv() takes ownership of the buffer */
	dev_coredumpv(ab->dev, ab->dump_data, ab->ath12k_coredump_len, GFP_KERNEL);
	ab->dump_data = NULL;
}

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

void ath12k_coredump_collect(struct ath12k_base *ab)
{
	ath12k_coredump_q6crash_reason(ab);
	ath12k_hif_coredump_download(ab);
}
