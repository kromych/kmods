#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("misc char device example");
MODULE_VERSION("0.01");

static bool dump_stack_trace = false;

module_param(dump_stack_trace, bool, 0644); // Permissions in /sysfs
MODULE_PARM_DESC(dump_stack_trace, "Dumping stack traces");

static int kmisc_open(struct inode *, struct file *);
static int kmisc_release(struct inode *, struct file *);

static const struct file_operations kmisc_file_ops = {
    .owner   = THIS_MODULE,
	.open    = kmisc_open,
	.release = kmisc_release,
};

struct miscdevice kmisc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "kmisc",
	.fops = &kmisc_file_ops,
	.mode = 0766
};

static int kmisc_open(struct inode *inode, struct file *filp)
{
    pr_info("Opening %s in %s\n", kmisc_dev.name, __func__);
    return 0;
}

static int kmisc_release(struct inode *inode, struct file *filp)
{
    pr_info("Closing %s in %s\n", kmisc_dev.name, __func__);
    return 0;
}

static int __init init_kmisc_example(void)
{
    pr_info("Registering %s in %s\n", kmisc_dev.name, __func__);

    return misc_register(&kmisc_dev);
}

static void __exit exit_kmisc_example(void)
{
    pr_info("Deregistering %s in %s\n", kmisc_dev.name, __func__);
    misc_deregister(&kmisc_dev);
}

module_init(init_kmisc_example);
module_exit(exit_kmisc_example);
