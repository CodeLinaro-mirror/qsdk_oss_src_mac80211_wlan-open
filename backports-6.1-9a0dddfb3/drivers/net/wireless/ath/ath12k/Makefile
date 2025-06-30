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
	    dp_peer.o \
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
	    ahb.o \
	    vendor.o \
	    umac_reset.o \
	    accel_cfg.o \
	    ini.o \
	    ini_parse.o \
	    dp_mon_filter.o

ath12k-$(CPTCFG_ATH12K_AHB) += ahb.o
ath12k-$(CPTCFG_ATH12K_PPE_DS_SUPPORT) += ppe.o

obj-$(CPTCFG_ATH12K) += wifi7/

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
