/*
 * Greybus Netlink driver for Greybus
 *
 * Released under the GPLv2 only.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/netdevice.h>
#include <net/genetlink.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <net/sock.h>

#include "greybus.h"
#include "gb_netlink.h"

struct gb_netlink {
	struct socket *socket;
	unsigned int cport_id;
};

static dev_t first;
static struct class *class;
static struct gb_host_device *nl_hd;

static int _gb_netlink_init(struct device *dev);
static void _gb_netlink_exit(void);

static inline struct gb_netlink *hd_to_netlink(struct gb_host_device *hd)
{
	return (struct gb_netlink *)&hd->hd_priv;
}

static const struct nla_policy gb_nl_policy[GB_NL_A_MAX + 1] = {
	[GB_NL_A_DATA] = { .type = NLA_BINARY, .len = GB_NETLINK_MTU },
	[GB_NL_A_CPORT] = { .type = NLA_U32},
};

static int gb_netlink_msg(struct sk_buff *skb, struct genl_info *info);
static int gb_netlink_hd_reset(struct sk_buff *skb, struct genl_info *info);

static const struct genl_ops gb_nl_ops[] = {
	{
		.cmd = GB_NL_C_MSG,
		.flags = 0,
		.doit = gb_netlink_msg,
		.dumpit = NULL,
	},
	{
		.cmd = GB_NL_C_HD_RESET,
		.flags = 0,
		.doit = gb_netlink_hd_reset,
		.dumpit = NULL,
	},
};

#define VERSION_NR 1
static struct genl_family gb_nl_family = {
	.hdrsize = 0,
	.name = GB_NL_NAME,
	.version = VERSION_NR,
	.maxattr = GB_NL_A_MAX,
	.ops = gb_nl_ops,
	.policy = gb_nl_policy,
	.n_ops = ARRAY_SIZE( gb_nl_ops ),
};

static int message_send(struct gb_host_device *hd, u16 cport_id,
			struct gb_message *message, gfp_t gfp_mask)
{
	struct nl_msg *nl_msg;
	struct sk_buff *skb;
	int retval;

	skb = genlmsg_new(sizeof(*message->header) + sizeof(u32) +
			  message->payload_size, GFP_KERNEL);
	if (!skb)
		goto out;

	nl_msg = genlmsg_put(skb, GB_NL_PID, 0,
			     &gb_nl_family, 0, GB_NL_C_MSG);
	if (!nl_msg) {
		retval = -ENOMEM;
		goto out;
	}

	retval = nla_put_u32(skb, GB_NL_A_CPORT, cport_id);
	if (retval)
		goto out;

	retval = nla_put(skb, GB_NL_A_DATA,
			 sizeof(*message->header) + message->payload_size,
			 message->header);
	if (retval)
		goto out;

	genlmsg_end(skb, nl_msg);

	retval = genlmsg_unicast(&init_net, skb, GB_NL_PID);
	if (retval)
		goto out;

	/*
	 * Tell the submitter that the message send (attempt) is
	 * complete, and report the status.
	 */
	greybus_message_sent(hd, message, retval < 0 ? retval : 0);

	return 0;

out:
	return -1;
}

static void message_cancel(struct gb_message *message)
{

}

static int gb_netlink_msg(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na;
	u16 cport_id;
	void *data;

	if (!info)
		return -EPROTO;

	na = info->attrs[GB_NL_A_CPORT];
	if (!na) {
		dev_err(&nl_hd->dev,
			"Received message without cport id attribute\n");
		return -EPROTO;
	}

	cport_id = nla_get_u32(na);
	if (!cport_id_valid(nl_hd, cport_id)) {
		dev_err(&nl_hd->dev, "invalid cport id %u received", cport_id);
		return -EINVAL;
	}

	na = info->attrs[GB_NL_A_DATA];
	if (!na) {
		dev_err(&nl_hd->dev,
			"Received message without data attribute\n");
		return -EPROTO;
	}

	data = nla_data(na);
	if (!data) {
		dev_err(&nl_hd->dev,
			"Received message without data\n");
		return -EINVAL;
	}

	greybus_data_rcvd(nl_hd, cport_id, data, nla_len(na));

	return 0;
}

static int gb_netlink_hd_reset(struct sk_buff *skb, struct genl_info *info)
{
	struct device *dev;
	struct gb_host_device *hd = nl_hd;

	dev = hd->dev.parent;
	_gb_netlink_exit();
	_gb_netlink_init(dev);

	return 0;
}

static struct gb_hd_driver tcpip_driver = {
	.hd_priv_size		= sizeof(struct gb_netlink),
	.message_send		= message_send,
	.message_cancel		= message_cancel,
};

static void _gb_netlink_exit(void)
{
	struct gb_host_device *hd = nl_hd;

	if (!hd)
		return;

	gb_hd_del(hd);
	gb_hd_put(hd);
	nl_hd = NULL;
}

static void __exit gb_netlink_exit(void)
{
	_gb_netlink_exit();

	unregister_chrdev_region(first, 1);
	device_destroy(class, first);
	class_destroy(class);

	genl_unregister_family(&gb_nl_family);
}

static int _gb_netlink_init(struct device *dev)
{
	int retval;
	struct gb_host_device *hd;
	struct gb_netlink *gb;

	hd = gb_hd_create(&tcpip_driver, dev, GB_NETLINK_MTU,
			  GB_NETLINK_NUM_CPORT);
	if (IS_ERR(hd))
		return PTR_ERR(hd);

	nl_hd = hd;
	gb = hd_to_netlink(hd);

	retval = gb_hd_add(hd);
	if (retval)
		goto err_gb_hd_del;

	return 0;

err_gb_hd_del:
	gb_hd_del(hd);
	gb_hd_put(hd);

	return retval;
}

static int __init gb_netlink_init(void)
{
	int retval;
	struct device *dev;

	retval = genl_register_family(&gb_nl_family);
	if (retval)
		return retval;

	retval = alloc_chrdev_region(&first, 0, 1, "gb_nl");
	if (retval)
		goto err_genl_unregister;

	class = class_create(THIS_MODULE, "gb_nl");
	if (IS_ERR(class)) {
		retval = PTR_ERR(class);
		goto err_chrdev_unregister;
	}

	dev = device_create(class, NULL, first, NULL, "gn_nl");
	if (IS_ERR(dev)) {
		retval = PTR_ERR(dev);
		goto err_class_destroy;
	}

	retval = _gb_netlink_init(dev);
	if (retval)
		goto err_device_destroy;

	return 0;

err_device_destroy:
	device_destroy(class, first);
err_chrdev_unregister:
	unregister_chrdev_region(first, 1);
err_class_destroy:
	class_destroy(class);
err_genl_unregister:
	genl_unregister_family(&gb_nl_family);

	return retval;
}

module_init(gb_netlink_init);
module_exit(gb_netlink_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alexandre Bailon <abailon@baylibre.com>");
