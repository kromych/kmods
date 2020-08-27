#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/cred.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("kcdev char device example");
MODULE_VERSION("0.01");

#define KCDEV_MINOR_START    0
#define KCDEV_MAX_DEVICES    8
#define KCDEV_NAME           "kcdev"
#define KCDEVICE_CLASS       "kcdev_class"

static bool     dump_stack_trace = false;
static ulong    kcdev_count = KCDEV_MAX_DEVICES;

module_param(dump_stack_trace, bool, 0644); // Permissions in /sysfs
MODULE_PARM_DESC(dump_stack_trace, "Dumping stack traces");

module_param(kcdev_count, ulong, 00); // Permissions in /sysfs
MODULE_PARM_DESC(kcdev_count, "Number of devices");

static int	         kcdev_open(struct inode *, struct file *);
static ssize_t	     kcdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t	     kcdev_write(struct file *, const char __user *, size_t, loff_t *);
static int	         kcdev_release(struct inode *, struct file *);
static int	         kcdev_mmap(struct file *, struct vm_area_struct *);
static long          kcdev_ioctl(struct file *, unsigned int, unsigned long);

static const struct file_operations kcdev_file_ops = {
    .owner             = THIS_MODULE,
	.open              = kcdev_open,
	.read              = kcdev_read,
	.write             = kcdev_write,
	.release           = kcdev_release,
	.mmap              = kcdev_mmap,
    .unlocked_ioctl    = kcdev_ioctl,
};

static dev_t            kcdev_num;
static struct cdev      kcdev;
static struct class*    kcdev_class;

static void kcdev_cleanup(void)
{
    u32 i;

    // Remove the device files and unregister the devices

    if (kcdev_class) {
        for (i = 0; i < kcdev_count; ++i) {
            device_destroy(kcdev_class, MKDEV(MAJOR(kcdev_num), MINOR(i)));
        }
        class_destroy(kcdev_class);
    }

    unregister_chrdev(MAJOR(kcdev_num), KCDEV_NAME);
}

static int __init init_kcdev_example(void)
{
    s64 ret = -EFAULT;
    u32 i;

    // Get the MAJOR and MINOR device numbers dynamically.
    // register_chrdev() does that statically eliminating the need to call
    // cdev_init() and cdev_add()

    ret = alloc_chrdev_region(
        &kcdev_num,
        KCDEV_MINOR_START,
        kcdev_count,
        KCDEV_NAME);

    if (ret < 0) {
        pr_err("kcdev: failed in alloc_chrdev_region, error %#08llx\n", ret);
        goto exit;
    }

    cdev_init(&kcdev, &kcdev_file_ops);
    kcdev.owner = THIS_MODULE;

    ret = cdev_add(&kcdev, kcdev_num, kcdev_count);
    if (ret) {
        pr_err("kcdev: failed in cdev_add, error %#08llx\n", ret);
        goto exit;
    }

    // The devices are now live, report.

    pr_info("kcdev: Registered %ld devices %#04x, major: %#02x, minor: %#02x\n",
        kcdev_count,
        MKDEV(MAJOR(kcdev_num), MINOR(kcdev_num)),
        MAJOR(kcdev_num), MINOR(kcdev_num));

    // To communicate with the devices, device files are required.
    // They are created with mknod under /dev by convention.

    // Now we are going to export the data through sysfs sparing
    // the user the trouble of calling mknod or running the udev
    // service for that purpose. If the kernel has devtmpfs, the
    // devices will be created automatically.

    kcdev_class = class_create(THIS_MODULE, KCDEVICE_CLASS);
    if (IS_ERR(kcdev_class)) {
        ret = PTR_ERR(kcdev_class);
        kcdev_class = NULL;

        pr_err("kcdev: failed in class_create, error %#08llx\n", ret);
        goto exit;
    }

    for (i = 0; i < kcdev_count; ++i) {
        struct device* dev = device_create(
            kcdev_class,
            NULL, // No parent
            MKDEV(MAJOR(kcdev_num), MINOR(i)),
            NULL, // No additional data
            KCDEV_NAME "%d", i);

        if (IS_ERR(dev)) {
            ret = PTR_ERR(dev);
            pr_err("kcdev: failed in device_create, error %#08llx\n", ret);
            goto exit;
        }

        pr_info("kcdev: Created device file /dev/" KCDEV_NAME "%d\n", i);
    }

    ret = 0;

exit:

    if (ret != 0) {
        kcdev_cleanup();

        unregister_chrdev(MAJOR(kcdev_num), KCDEV_NAME);
    }

    return ret;
}

static void __exit exit_kcdev_example(void)
{
    kcdev_cleanup();

    pr_info("kcdev: unregistered %ld device(s) %#04x, major: %#02x, minor: %#02x\n",
        kcdev_count,
        MKDEV(MAJOR(kcdev_num), MINOR(kcdev_num)),
        MAJOR(kcdev_num), MINOR(kcdev_num));
}

module_init(init_kcdev_example);
module_exit(exit_kcdev_example);

/*
    Call Trace (5.7):
        dump_stack+0x64/0x88
        kcdev_open+0xa/0xd [kmodchardev]
        ? chrdev_open+0xdd/0x210
        ? cdev_device_add+0xc0/0xc0
        ? do_dentry_open+0x13a/0x380
        ? path_openat+0xa9a/0xfe0
        ? do_filp_open+0x75/0x100
        ? __check_object_size+0x12e/0x13c
        ? __alloc_fd+0x44/0x150
        ? do_sys_openat2+0x8a/0x130
        ? __x64_sys_openat+0x46/0x70
        ? do_syscall_64+0x5b/0xf0
        ? entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int kcdev_open(struct inode *inodep, struct file *fp)
{
    long ret = 0;

    if (dump_stack_trace) dump_stack();

    return ret;
}

/*
    Call Trace (5.7):
        dump_stack+0x64/0x88
        kcdev_read+0xa/0x12 [kmodchardev]
        ? vfs_read+0x9d/0x150
        ? ksys_read+0x4f/0xc0
        ? do_syscall_64+0x5b/0xf0
        ? entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static ssize_t kcdev_read(struct file *fp, char __user *user_data, size_t size, loff_t *offesetp)
{
    long ret = -EFAULT;

    if (dump_stack_trace) dump_stack();

    return ret;
}

static ssize_t kcdev_write(struct file *fp, const char __user * user_data, size_t size, loff_t *offset)
{
    long ret = -EFAULT;

    if (dump_stack_trace) dump_stack();

    return ret;
}

/*
    Call stack (5.7):
        dump_stack+0x64/0x88
        kcdev_release+0xa/0xd [kmodchardev]
        ? __fput+0xe2/0x250
        ? task_work_run+0x68/0x90
        ? prepare_exit_to_usermode+0x198/0x1c0
        ? entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int kcdev_release(struct inode *inodep, struct file *fp)
{
    long ret = 0;

    if (dump_stack_trace) dump_stack();

    return ret;
}

static int kcdev_mmap(struct file *fp, struct vm_area_struct *vma)
{
    long ret = -EFAULT;

    if (dump_stack_trace) dump_stack();

    return ret;
}

static long kcdev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    long ret = -EFAULT;

    if (dump_stack_trace) dump_stack();

    return ret;
}
