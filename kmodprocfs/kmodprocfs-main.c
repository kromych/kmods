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

#include "kmodprocfs.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("procfs example");
MODULE_VERSION("0.01");

static bool     dump_stack_trace = false;
static ulong    entry_count = PROC_DEFAULT_ENTRIES;

module_param(dump_stack_trace, bool, 0644); // Permissions in /sysfs
MODULE_PARM_DESC(dump_stack_trace, "Dumping stack traces");

module_param(entry_count, ulong, 00); // Permissions in /sysfs
MODULE_PARM_DESC(entry_count, "Number of entries");

static int	         proc_open(struct inode *, struct file *);
static ssize_t	     proc_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t	     proc_write(struct file *, const char __user *, size_t, loff_t *);
static int	         proc_release(struct inode *, struct file *);
static int	         proc_mmap(struct file *, struct vm_area_struct *);
static long          proc_ioctl(struct file *, unsigned int, unsigned long);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)

static const struct file_operations procfs_example_ops = {
    .owner             = THIS_MODULE,
	.open              = proc_open,
	.read              = proc_read,
	.write             = proc_write,
	.release           = proc_release,
	.mmap              = proc_mmap,
    .unlocked_ioctl    = proc_ioctl,
};

#else

static const struct proc_ops procfs_example_ops = {
    .proc_owner             = THIS_MODULE,
	.proc_open              = proc_open,
	.proc_read              = proc_read,
	.proc_write             = proc_write,
	.proc_release           = proc_release,
	.proc_mmap              = proc_mmap,
};

#endif

struct proc_entry_datum {
    struct mutex lock;
    struct proc_dir_entry *dentry;

    u8       data_buffer[PROC_BUF_SIZE];
    char     name[PROC_MAX_NAME_LEN];

    bool     opened;
    size_t   data_size;
    off_t    data_pos;
    pid_t    owner_pid;
    uid_t    owner_uid;
};

static struct proc_dir_entry   *root_dentry;
static struct proc_entry_datum *data;

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        ? 0xffffffffc00b0000
        init_procfs_example+0x4e/0x1000 [kmodprocfs]
        do_one_initcall+0x46/0x1c4
        ? kobject_uevent_env+0x10f/0x650
        ? kmem_cache_alloc_trace+0x130/0x180
        do_init_module+0x5c/0x210
        load_module+0x226e/0x24e0
        ? __do_sys_finit_module+0xbf/0xe0
        __do_sys_finit_module+0xbf/0xe0
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int __init init_procfs_example(void)
{
    u32 i;

    root_dentry = proc_mkdir(
        PROC_DIR_NAME,
        NULL); // No parent

    if (root_dentry == NULL) {
        pr_err("Creating " PROC_DIR_PATH " failed\n");
        return -ENOMEM;
    } else {
        pr_info("Created " PROC_DIR_PATH "\n");
        if (dump_stack_trace) dump_stack();

        if (!data) {
            data = kzalloc(entry_count*sizeof(struct proc_entry_datum), GFP_KERNEL);            
        }

        if (!data) {
            return -ENOMEM;
        }

        for (i = 0; i < entry_count; ++i) {
            mutex_init(&data[i].lock);
            snprintf(&data[i].name[0], PROC_MAX_NAME_LEN - 1, "%d", i);

            data[i].dentry = proc_create_data(
                &data[i].name[0],
                0666,
                root_dentry,
                &procfs_example_ops,
                &data[i]);

            if (!data[i].dentry) {
                pr_err("Can't create " PROC_DIR_PATH "%s\n", &data[i].name[0]);
                return -ENOMEM;
            } else {
                pr_info("Created " PROC_DIR_PATH "%s\n", &data[i].name[0]);
            }
        }
    }

    return 0;
}

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        __x64_sys_delete_module+0x164/0x200
        ? exit_to_usermode_loop+0x94/0xd0
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static void __exit exit_procfs_example(void)
{
    if (data) {
        u32 i;

        for (i = 0; i < entry_count; ++i) {
            proc_remove(data[i].dentry);
        }

        kfree(data);
    }

    if (root_dentry) {
        proc_remove(root_dentry);
    }
}

module_init(init_procfs_example);
module_exit(exit_procfs_example);

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        proc_open+0x8a/0x90 [kmodprocfs]
        proc_reg_open+0x6b/0x110
        ? proc_i_callback+0x20/0x20
        do_dentry_open+0x13d/0x370
        path_openat+0x2e9/0x1270
        do_filp_open+0x91/0x100
        ? close_pdeo+0x8f/0xf0
        ? list_lru_add+0x1a/0x100
        do_sys_open+0x184/0x210
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int proc_open(struct inode *inodep, struct file *fp)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));

    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&datum->lock);

    pr_info("Opening %s task group %d, uid %d\n", datum->name, pid, uid);

    if (!datum->opened && uid == 0) {
        datum->opened = true;
        datum->owner_pid = pid;
        datum->owner_uid = uid;
        datum->data_pos = 0;

        ret = 0;

        pr_info("Opened %s task group %d, uid %d\n", datum->name, pid, uid);
        if (dump_stack_trace) dump_stack();
    } else {
        pr_err("Opening %s failed: task group %d, uid %d\n", datum->name, pid, uid);
        ret = -ENXIO;
    }

    mutex_unlock(&datum->lock);

    return ret;
}

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        proc_read.cold+0x53/0x55 [kmodprocfs]
        proc_reg_read+0x3c/0x60
        __vfs_read+0x37/0x190
        vfs_read+0x9d/0x150
        ksys_read+0x57/0xd0
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static ssize_t proc_read(struct file *fp, char __user *user_data, size_t size, loff_t *offesetp)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));
    size_t available;
    int ret;

    mutex_lock(&datum->lock);

    available = datum->data_size - datum->data_pos;

    pr_info("Reading %lu bytes from %s, available %lu: task group %d, uid %d\n", 
        size, datum->name, available, datum->owner_pid, datum->owner_uid);

    if (dump_stack_trace) dump_stack();

    if (size > available) {
        size = available;
    }

    if (size > 0) {
        ret = copy_to_user(user_data, datum->data_buffer + datum->data_pos, size) ? -EFAULT : size;

        if (ret > 0) {
            datum->data_pos += size;
        }
    } else {
        ret = 0;
    }

    mutex_unlock(&datum->lock);

    return ret;
}

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        proc_write.cold+0x51/0x53 [kmodprocfs]
        proc_reg_write+0x3c/0x60
        __vfs_write+0x37/0x1a0
        ? __do_sys_newfstat+0x5a/0x70
        vfs_write+0xb6/0x190
        ksys_write+0x57/0xd0
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static ssize_t proc_write(struct file *fp, const char __user * user_data, size_t size, loff_t *offset)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));
    size_t available;
    int ret;

    mutex_lock(&datum->lock);

    available = PROC_BUF_SIZE - datum->data_pos;

    pr_info("Writing %lu bytes to %s, available %lu: task group %d, uid %d\n", 
        size, datum->name, available, datum->owner_pid, datum->owner_uid);

    if (dump_stack_trace) dump_stack();

    if (size > available) {
        size = available;
    }

    if (size > 0) {
        ret = copy_from_user(datum->data_buffer + datum->data_pos, user_data, size) ? -EFAULT : size;

        if (ret > 0) {
            datum->data_pos += size;
            datum->data_size = datum->data_pos > datum->data_size ? datum->data_pos : datum->data_size;
        }
    } else {
        ret = -ENOSPC;
    }

    mutex_unlock(&datum->lock);

    return ret;
}

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        proc_release.cold+0x55/0x5e [kmodprocfs]
        close_pdeo+0x43/0xf0
        proc_reg_release+0x40/0x50
        __fput+0xa5/0x1d0
        task_work_run+0x8f/0xb0
        exit_to_usermode_loop+0xc2/0xd0
        do_syscall_64+0xde/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int proc_release(struct inode *inodep, struct file *fp)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));

    mutex_lock(&datum->lock);

    pr_info("Closed %s: task group %d, uid %d\n", datum->name, datum->owner_pid, datum->owner_uid);
    if (dump_stack_trace) dump_stack();

    mutex_unlock(&datum->lock);

    return 0;
}

/* Example call trace (4.19):
        dump_stack+0x66/0x90
        proc_mmap.cold+0x55/0x57 [kmodprocfs]
        proc_reg_mmap+0x39/0x60
        mmap_region+0x3e9/0x660
        do_mmap+0x38b/0x560
        vm_mmap_pgoff+0xd1/0x120
        ksys_mmap_pgoff+0x18c/0x230
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static int proc_mmap(struct file *fp, struct vm_area_struct *vma)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));

    int ret;

    mutex_lock(&datum->lock);

    pr_info("Mapping %s: task group %d, uid %d\n", datum->name, datum->owner_pid, datum->owner_uid);
    if (dump_stack_trace) dump_stack();

    ret = remap_pfn_range(
        vma,
        vma->vm_start,
        virt_to_phys((u8*)datum->data_buffer + (vma->vm_pgoff << PAGE_SHIFT)) >> PAGE_SHIFT,
        PROC_BUF_SIZE,
        vma->vm_page_prot) ? -EINVAL : 0;

    mutex_unlock(&datum->lock);

    return ret;
}

/* Example stack trace (4.19):
        dump_stack+0x66/0x90
        proc_ioctl.cold+0x63/0x7d [kmodprocfs]
        proc_reg_unlocked_ioctl+0x3a/0x60
        do_vfs_ioctl+0x3e4/0x640
        ? handle_mm_fault+0xdc/0x210
        ksys_ioctl+0x5e/0x90
        __x64_sys_ioctl+0x16/0x20
        do_syscall_64+0x55/0x110
        entry_SYSCALL_64_after_hwframe+0x44/0xa9
*/
static long proc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    struct proc_entry_datum* datum = PDE_DATA(file_inode(fp));

    int ret;

    mutex_lock(&datum->lock);

    pr_info("IOCTL %#08x for %s: task group %d, uid %d\n", cmd, datum->name, datum->owner_pid, datum->owner_uid);
    if (dump_stack_trace) dump_stack();

    switch (cmd) {
    case PROC_IOCTL_RESET_POS:
        datum->data_pos = 0;
        ret = 0;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    mutex_unlock(&datum->lock);

    return ret;
}
