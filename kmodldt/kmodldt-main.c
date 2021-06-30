#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <generated/autoconf.h>

#include "kmodldt.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("kldt char device example");
MODULE_VERSION("0.01");

/* The 64-bit descriptor */
struct desc_64_bit {
	u16	limit0;
	u16	base0;
	u16	base1 : 8, type : 5, dpl : 2, p : 1;
	u16	limit1 : 4, zero0 : 3, g : 1, base2 : 8;
} __attribute__((packed));

/* The 128-bit descriptor */
struct desc_128_bit {
	struct desc_64_bit low_part;
	u32	base3;
	u32	zero1;
} __attribute__((packed));

struct ldt_struct {
	struct desc_64_bit	*entries;
	unsigned int		nr_entries;
};

static bool dump_stack_trace = false;

struct kldt_dev {
    struct miscdevice misc_dev;
    struct device_attribute attr;
};

static ssize_t __maybe_unused kldt_attr_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    // cat /sys/devices/virtual/misc/kldt/kldt
    return sprintf(buf, "kldt\n");
}

static ssize_t __maybe_unused kldt_attr_store(struct device *dev, struct device_attribute *attr,
            const char *buf, size_t count)
{
	return strnlen(buf, count);
}

static int kldt_open(struct inode *, struct file *);
static int kldt_release(struct inode *, struct file *);
static long kldt_ioctl(struct file *, unsigned int, unsigned long);

static const struct file_operations kldt_file_ops = {
    .owner   = THIS_MODULE,
	.open    = kldt_open,
	.release = kldt_release,
    .unlocked_ioctl = kldt_ioctl,
};

static long kldt_ioctl(struct file *filp, unsigned int cmd, unsigned long param)
{
	struct ldt_struct *ldt;
    int ret;

    pr_info("IOCTL %#08x: task group %d\n", cmd, current->pid);

    if (dump_stack_trace) dump_stack();

	if (down_write_killable(&current->mm->context.ldt_usr_sem))
		return -EINTR;

	ldt = current->mm->context.ldt;

    // The um must set two short descriptors of type 0xC (call gate)
    // prior to calling here. The syscall code only allows that when
    // the non-present segment bit is set. Their base addresses
    // combined are the base 64-bit address.
    // The task at hand is to turn these into a one valid long descriptor
    // of type 0xC, adjusting RPL and other bits along the way.

    // Param contains the first entry number.

    switch (cmd) {
    default:
        ret = -ENOIOCTLCMD;
        break;
    }

	up_write(&current->mm->context.ldt_usr_sem);

    return ret;
}

static int kldt_open(struct inode *inode, struct file *filp)
{
    struct kldt_dev* dev = container_of(filp->private_data,  struct kldt_dev, misc_dev);

    pr_info("Opening %s in %s\n", dev->misc_dev.name, __func__);
    return 0;
}

static int kldt_release(struct inode *inode, struct file *filp)
{
    struct kldt_dev* dev = container_of(filp->private_data,  struct kldt_dev, misc_dev);

    pr_info("Closing %s in %s\n", dev->misc_dev.name, __func__);
    return 0;
}

static struct kldt_dev *dev;

static int __init init_kldt_example(void)
{
    int ret;

    if (!CONFIG_MODIFY_LDT_SYSCALL)
        return -ENOTSUPP;

    dev = kzalloc(sizeof(struct kldt_dev), GFP_KERNEL);

    if (!dev)
        return -ENOMEM;

    dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	dev->misc_dev.name = KLDT_NAME;
	dev->misc_dev.nodename = dev->misc_dev.name;
	dev->misc_dev.fops = &kldt_file_ops;
	dev->misc_dev.mode = 0755;
    dev->attr.attr.name = dev->misc_dev.name;
    dev->attr.attr.mode = dev->misc_dev.mode;
    dev->attr.show = kldt_attr_show;
    dev->attr.store = kldt_attr_store;

    pr_info("Registering %s in %s\n", dev->misc_dev.name, __func__);

    ret = misc_register(&dev->misc_dev);

    if (ret == 0)
        ret = device_create_file(dev->misc_dev.this_device, &dev->attr);

    return ret;
}

static void __exit exit_kldt_example(void)
{
    if (dev) {
        pr_info("Deregistering %s in %s\n", dev->misc_dev.name, __func__);
        device_remove_file(dev->misc_dev.this_device, &dev->attr);
        misc_deregister(&dev->misc_dev);
        kfree(dev);
        dev = NULL;
    }
}

module_init(init_kldt_example);
module_exit(exit_kldt_example);
