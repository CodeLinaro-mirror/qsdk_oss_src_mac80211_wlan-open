# SPDX-License-Identifier: BSD-3-Clause-Clear
obj-$(CPTCFG_ATH12K) += ath12k.o
ath12k-y += core.o \
	    hal.o \
	    wmi.o \
	    mac.o \
	    reg.o \
	    htc.o \
	    qmi.o \
	    dp.o  \
	    dp_tx.o \
	    dp_rx.o \
	    dp_htt.o \
	    dp_htt_logger.o \
	    dp_peer.o \
	    dp_stats.o \
	    debug.o \
	    ce.o \
	    peer.o \
	    dbring.o \
	    mhi.o \
	    pcic.o \
	    pci.o \
	    dp_mon.o \
	    fw.o \
	    p2p.o \
	    fse.o \
	    vendor.o \
	    umac_reset.o \
	    accel_cfg.o \
	    qos.o \
	    sdwf.o \
	    telemetry.o \
	    telemetry_agent_if.o \
	    ini.o \
	    ini_parse.o \
	    dp_mon_filter.o \
	    erp.o \
	    vendor_services.o \
	    pktlog.o \
	    ce_stats.o \
	    mgmt_rx.o \
	    event.o

ath12k-$(CPTCFG_ATHDEBUG) += athdbg_if.o
ath12k-$(CPTCFG_ATH12K_AHB) += ahb.o
ath12k-$(CPTCFG_ATH12K_PPE_DS_SUPPORT) += ppe.o

ifneq ($(CPTCFG_ATH12K_PPE_DS_SUPPORT),y)
ath12k-y += ppe_stub.o
endif

obj-$(CPTCFG_QCN_EXTN) += qcn_extns/wifi6/
obj-$(CPTCFG_ATH12K) += wifi7/
obj-$(CPTCFG_ATH12K) += wifi8/
obj-$(CPTCFG_ATHDEBUG) += ath_debug/

ath12k-$(CPTCFG_QCN_EXTN) += \
	qcn_extns/core_extn.o \
	qcn_extns/vendor_extn.o \
	qcn_extns/esp_extn.o \
	qcn_extns/txrx_extn.o

ath12k-$(CPTCFG_ATH12K_DEBUGFS) += debugfs.o debugfs_htt_stats.o debugfs_sta.o
ath12k-$(CONFIG_ACPI) += acpi.o
ath12k-$(CPTCFG_ATH12K_TRACING) += trace.o
ath12k-$(CPTCFG_ATH12K_SAWF) += telemetry_agent_if.o
ath12k-$(CONFIG_PM) += wow.o
ath12k-$(CPTCFG_NL80211_TESTMODE) += testmode.o
ath12k-$(CPTCFG_ATH12K_SPECTRAL) += spectral.o
ath12k-$(CPTCFG_WANT_DEV_COREDUMP) += coredump.o
ath12k-$(CONFIG_THERMAL) += thermal.o
ath12k-$(CPTCFG_ATH12K_CFR) += cfr.o

# for tracing framework to find trace.h
CFLAGS_trace.o := -I$(src)

ifeq ($(CONFIG_KERNEL_ATH_GCOV_PROFILE),)
GCOV_PROFILE:=y
endif

# Prevent arch-agnostic top-level ath12k sources from
# including headers from the wifi7/ or wifi8/ subdirectories.
ath12k_src_dir := $(if $(src),$(src),.)
ath12k_c_files := $(wildcard $(ath12k_src_dir)/*.c)
ath12k_h_files := $(wildcard $(ath12k_src_dir)/*.h)
arch_specific_includes := $(shell grep -n -e 'include "wifi7/' -e 'include <wifi7/' -e 'include "wifi8/' -e 'include <wifi8/' $(ath12k_c_files) 2>/dev/null | sed -E 's|.*/([^/]+):([0-9]+):.*|\1:\2|' || true)
arch_specific_includes_h := $(shell grep -n -e 'include "wifi7/' -e 'include <wifi7/' -e 'include "wifi8/' -e 'include <wifi8/' $(ath12k_h_files) 2>/dev/null | sed -E 's|.*/([^/]+):([0-9]+):.*|\1:\2|' || true)

# Exceptions: allow ahb.c and pcic.c to include wifi7/ or wifi8/ headers. This is a temperory exception until the headers are fixed.
arch_specific_exceptions := ahb.c pcic.c
arch_specific_includes_filtered := $(filter-out $(addsuffix :%,$(arch_specific_exceptions)),$(arch_specific_includes))
arch_specific_all := $(strip $(arch_specific_includes_filtered) $(arch_specific_includes_h))
ifneq ($(arch_specific_all),)
$(error ath12k: arch_specific include of wifi7 or wifi8 headers in top-level sources: $(arch_specific_includes_filtered) $(arch_specific_includes_h))
endif
