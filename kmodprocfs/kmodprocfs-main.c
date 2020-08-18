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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>

#include <asm/uaccess.h>

#include "kmodprocfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("procfs example");
MODULE_VERSION("0.01");

// module_param();

static int	         proc_open(struct inode *, struct file *);
static ssize_t	     proc_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t	     proc_write(struct file *, const char __user *, size_t, loff_t *);
static int	         proc_release(struct inode *, struct file *);
static int	         proc_mmap(struct file *, struct vm_area_struct *);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)

static const struct file_operations procfs_example_ops = {
    .owner             = THIS_MODULE,
	.open              = proc_open,
	.read              = proc_read,
	.write             = proc_write,
	.release           = proc_release,
	.mmap              = proc_mmap,
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

static struct proc_dir_entry *dentry;

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
    dentry = proc_create(
        PROC_FILE_NAME,
        0666,
        NULL, // No parent
        &procfs_example_ops);

    if (dentry == NULL) {
        pr_err("Creating " PROC_FILE_PATH " failed\n");
        return -ENOMEM;
    } else {
        pr_info("Created " PROC_FILE_PATH "\n");
        dump_stack();
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
    if (dentry) {
        proc_remove(dentry);
        pr_info("Removed " PROC_FILE_PATH "\n");
        dump_stack();
    }
}

module_init(init_procfs_example);
module_exit(exit_procfs_example);

static DEFINE_MUTEX(state_mutex);

static bool opened;
static pid_t owner_pid;
static uid_t owner_uid;

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
    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&state_mutex);

    pr_info("Opening " PROC_FILE_PATH " task group %d, uid %d\n", pid, uid);

    if (!opened && uid == 0) {
        opened = true;
        owner_pid = pid;
        owner_uid = uid;
        ret = 0;

        pr_info("Opened " PROC_FILE_PATH " task group %d, uid %d\n", pid, uid);
        dump_stack();
    } else {
        pr_err("Opening " PROC_FILE_PATH " failed: task group %d, uid %d\n", pid, uid);
        ret = -ENXIO;
    }

    mutex_unlock(&state_mutex);

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
    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&state_mutex);

    if (opened && pid == owner_pid && uid == owner_uid) {
        pr_info("Reading from " PROC_FILE_PATH ": task group %d, uid %d\n", pid, uid);
        dump_stack();
        ret = 0;
    } else {
        pr_info("Reading from " PROC_FILE_PATH " failed: task group %d, uid %d\n", pid, uid);
        ret = -EBADF; // TODO Might be -EPERM
    }

    mutex_unlock(&state_mutex);

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
    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&state_mutex);

    if (opened && pid == owner_pid && uid == owner_uid) {
        pr_info("Writing to " PROC_FILE_PATH ": task group %d, uid %d\n", pid, uid);
        dump_stack();
        ret = size;
    } else {
        pr_info("Writing to " PROC_FILE_PATH " failed: task group %d, uid %d\n", pid, uid);
        ret = -EBADF; // TODO Might be -EPERM
    }

    mutex_unlock(&state_mutex);

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
    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&state_mutex);

    if (opened && pid == owner_pid && uid == owner_uid) {
        pr_info("Closed " PROC_FILE_PATH ": task group %d, uid %d\n", pid, uid);
        dump_stack();
        opened = false;
        ret = 0;
    } else {
        pr_info("Closing " PROC_FILE_PATH " failed: task group %d, uid %d\n", pid, uid);
        ret = -EBADF; // TODO Might be -EPERM
    }

    mutex_unlock(&state_mutex);

    return ret;
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
    pid_t pid = task_pid_vnr(current);
    uid_t uid = current_uid().val;

    int ret;

    mutex_lock(&state_mutex);

    if (opened && pid == owner_pid && uid == owner_uid) {
        pr_info("Mapping " PROC_FILE_PATH ": task group %d, uid %d\n", pid, uid);
        dump_stack();
        ret = 0;
    } else {
        pr_info("Mapping " PROC_FILE_PATH " failed: task group %d, uid %d\n", pid, uid);
        ret = -EBADF; // TODO Might be -EPERM
    }

    mutex_unlock(&state_mutex);

    return ret;
}
