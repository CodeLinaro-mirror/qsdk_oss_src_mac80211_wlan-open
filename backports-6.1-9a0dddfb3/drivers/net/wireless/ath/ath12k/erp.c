/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <linux/mutex.h>
#include <net/netlink.h>
#include "core.h"
#include "vendor.h"
#include "dp_rx.h"
#include "erp.h"
#include "debugfs.h"

#if LINUX_VERSION_IS_GEQ(6,7,0)
static const struct netlink_range_validation
#else
static struct netlink_range_validation
#endif
ath12k_vendor_erp_config_trigger_range = {
	.min = 1,
	.max = BIT(QCA_WLAN_VENDOR_TRIGGER_TYPE_MAX) - 1,
};

static const struct nla_policy
ath12k_vendor_erp_config_policy[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER] =
		NLA_POLICY_FULL_RANGE(NLA_U32, &ath12k_vendor_erp_config_trigger_range),
};

static const struct nla_policy
ath12k_vendor_erp_policy[QCA_WLAN_VENDOR_ATTR_ERP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START] = { .type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE] = { .type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG] =
		NLA_POLICY_NESTED(ath12k_vendor_erp_config_policy),
	[QCA_WLAN_VENDOR_ATTR_ERP_EXIT] = { .type = NLA_FLAG },
};

enum ath12k_erp_states {
	ATH12K_ERP_OFF,
	ATH12K_ERP_ENTER_STARTED,
	ATH12K_ERP_ENTER_COMPLETE,
};

struct ath12k_erp_active_ar {
	struct ath12k *ar;
	enum ath12k_routing_pkt_type trigger;
};

struct ath12k_erp_state_machine {
	bool initialized;
	/* Protects the structure members */
	struct mutex lock;
	enum ath12k_erp_states state;
	struct ath12k_erp_active_ar active_ar;
};

static struct ath12k_erp_state_machine erp_sm = {};

static void ath12k_erp_reset_state(void)
{
	erp_sm.state = ATH12K_ERP_OFF;
	memset(&erp_sm.active_ar, 0, sizeof(erp_sm.active_ar));
}

static u32 ath12k_erp_convert_trigger_bitmap(u32 bitmap)
{
	u32 new_bitmap = 0;
	u8 bit_set;

	WARN_ON(sizeof(bitmap) > sizeof(new_bitmap));

	while (bitmap) {
		bit_set = ffs(bitmap);
		if (bit_set > ATH12K_PKT_TYPE_MAX)
			break;

		bitmap &= ~BIT(bit_set - 1);

		switch (bit_set - 1) {
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ARP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ARP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_NS_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_NS_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IGMP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IGMP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_MLD_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_MLD_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DHCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DHCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_TCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_TCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_UDP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_UDP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ICMP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ICMP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_TCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_TCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_UDP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_UDP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IPV6);
			break;
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_EAP:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_EAP);
			break;
		default:
			break;
		}
	}

       return new_bitmap;
}

static int ath12k_erp_set_pkt_filter(struct ath12k *ar, u32 bitmap,
				     enum ath12k_wmi_pkt_route_opcode op_code)
{
	struct ath12k_wmi_pkt_route_param param = {};

	lockdep_assert_held(&erp_sm.lock);

	if (!bitmap)
		return 0;

	if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
		bitmap = ath12k_erp_convert_trigger_bitmap(bitmap);
		if (!bitmap) {
			ath12k_err(NULL, "invalid wake up trigger bitmap\n");
			return -EINVAL;
		}
	}

	param.opcode = op_code;
	param.meta_data = ATH12K_RX_PROTOCOL_TAG_START_OFFSET + bitmap;
	param.dst_ring = ATH12K_REO_RELEASE_RING;
	param.dst_ring_handler = ATH12K_WMI_PKTROUTE_USE_CCE;
	param.route_type_bmap = bitmap;
	if (ath12k_wmi_send_pdev_pkt_route(ar, &param)) {
		ath12k_err(NULL, "failed to set packet bitmap");
		return -EINVAL;
	}

	spin_lock_bh(&ar->ab->base_lock);
	if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
		ar->erp_trigger_set = true;
		erp_sm.active_ar.trigger = bitmap;
	} else if (op_code == ATH12K_WMI_PKTROUTE_DEL) {
		ar->erp_trigger_set = false;
		erp_sm.active_ar.trigger = 0;
	}
	spin_unlock_bh(&ar->ab->base_lock);

	return 0;
}

static int ath12k_erp_config_trigger(struct wiphy *wiphy, struct nlattr **attrs)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	u32 trigger;
	struct ath12k *ar;

	ar = ah->radio;

	trigger = nla_get_u32(attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER]);
	if (ath12k_erp_set_pkt_filter(ar, trigger, ATH12K_WMI_PKTROUTE_ADD))
		return -EINVAL;

	erp_sm.active_ar.ar = ar;
	return 0;
}

static int ath12k_erp_config(struct wiphy *wiphy, struct nlattr *attrs)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX + 1];
	int ret;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX,
			attrs, ath12k_vendor_erp_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ErP parameters\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER]) {
		ath12k_err(NULL, "empty ErP parameters\n");
		return ret;
	}

	return ath12k_erp_config_trigger(wiphy, tb);
}

static int ath12k_erp_enter(struct wiphy *wiphy,
			    struct wireless_dev *wdev,
			    struct nlattr **attrs)
{
	int ret = -EINVAL;

	mutex_lock(&erp_sm.lock);

	if (nla_get_flag(attrs[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START])) {
		if (erp_sm.state != ATH12K_ERP_OFF) {
			ath12k_err(NULL, "driver is already in ErP mode\n");
			goto out;
		}
		erp_sm.state = ATH12K_ERP_ENTER_STARTED;
	} else if (erp_sm.state != ATH12K_ERP_ENTER_STARTED) {
		ath12k_err(NULL,
			   "driver has not started ErP mode configurations\n");
		goto out;
	}

	if (attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG]) {
		ret = ath12k_erp_config(wiphy,
					attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG]);
		if (ret)
			goto out;
	}

	if (nla_get_flag(attrs[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE]))
		erp_sm.state = ATH12K_ERP_ENTER_COMPLETE;

	mutex_unlock(&erp_sm.lock);
	return 0;

out:
	mutex_unlock(&erp_sm.lock);
	return ret;
}

static void ath12k_vendor_send_erp_trigger(struct wiphy *wiphy)
{
	struct sk_buff *skb;
	struct nlattr *erp_ath;

	if (!wiphy)
		return;

	wiphy_lock(wiphy);
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL, "No memory available to send ErP vendor event\n");
		wiphy_unlock(wiphy);
		return;
	}

	erp_ath = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP);
	if (!erp_ath) {
		ath12k_err(NULL, "failed to put ErP parameters\n");
		kfree(skb);
		wiphy_unlock(wiphy);
		return;
	}

	if (nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_ERP_EXIT)) {
		ath12k_err(NULL, "Failure to put ErP vendor event flag\n");
		/* Allow this error case for now */
	}

	nla_nest_end(skb, erp_ath);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	wiphy_unlock(wiphy);
}

static int ath12k_erp_exit(struct wiphy *wiphy, bool send_event)
{
	struct ath12k_erp_active_ar *active_ar;
	int ret;

	if (!send_event) {
		/* When exit is triggered by userspace, no need to send event
		 * back. Wiphy lock must already be acquired in this case. */
		lockdep_assert_wiphy(wiphy);
	}

	mutex_lock(&erp_sm.lock);
	if (erp_sm.state != ATH12K_ERP_ENTER_COMPLETE) {
		ath12k_err(NULL, "driver is not in ErP mode\n");
		mutex_unlock(&erp_sm.lock);
		return -EINVAL;
	}

	active_ar = &erp_sm.active_ar;
	if (active_ar->ar) {
		ret = ath12k_erp_set_pkt_filter(active_ar->ar,
				active_ar->trigger,
				ATH12K_WMI_PKTROUTE_DEL);
		if (ret) {
			ath12k_err(active_ar->ar->ab, "failed to reset pkt bitmap %d", ret);
			mutex_unlock(&erp_sm.lock);
			return ret;
		}
	}

	ath12k_erp_reset_state();
	mutex_unlock(&erp_sm.lock);

	if (send_event)
		ath12k_vendor_send_erp_trigger(wiphy);

	return 0;
}

int ath12k_vendor_parse_rm_erp(struct wiphy *wiphy, struct wireless_dev *wdev,
			       struct nlattr *attrs)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ERP_MAX + 1];
	int ret;

	if (!erp_sm.initialized)
		return -EOPNOTSUPP;

	lockdep_assert_wiphy(wiphy);

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ERP_MAX, attrs,
			       ath12k_vendor_erp_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ErP attributes\n");
		return ret;
	}

	if (nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START]) ||
			nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE]) ||
			tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG])
		ret = ath12k_erp_enter(wiphy, wdev, tb);
	else if (nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_EXIT])) {
		ret = ath12k_erp_exit(wiphy, false);
	}

	return ret;
}

void ath12k_erp_handle_trigger(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k, erp_handle_trigger_work);

	if (!ar->erp_trigger_set)
		return;

	(void)ath12k_erp_exit(ath12k_ar_to_hw(ar)->wiphy, true);
}

void ath12k_erp_handle_ssr(struct ath12k *ar)
{
	if (!erp_sm.initialized)
		return;

	mutex_lock(&erp_sm.lock);
	if (erp_sm.state == ATH12K_ERP_OFF || !erp_sm.active_ar.trigger ||
	    !erp_sm.active_ar.ar || ar != erp_sm.active_ar.ar) {
		mutex_unlock(&erp_sm.lock);
		return;
	}

	ath12k_erp_set_pkt_filter(ar, erp_sm.active_ar.trigger, ATH12K_WMI_PKTROUTE_ADD);
	mutex_unlock(&erp_sm.lock);

}

void ath12k_erp_init(void)
{
	if (erp_sm.initialized)
		return;

	mutex_init(&erp_sm.lock);
	ath12k_erp_reset_state();

	erp_sm.initialized = true;
}
EXPORT_SYMBOL(ath12k_erp_init);

void ath12k_erp_deinit(void)
{
	if (!erp_sm.initialized)
		return;

	mutex_lock(&erp_sm.lock);

	ath12k_erp_reset_state();
	erp_sm.initialized = false;

	mutex_unlock(&erp_sm.lock);
	mutex_destroy(&erp_sm.lock);
}
EXPORT_SYMBOL(ath12k_erp_deinit);
