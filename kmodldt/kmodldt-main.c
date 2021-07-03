#define pr_fmt(fmt) "kldt: " fmt

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
#include <linux/uaccess.h>

#include <asm/segment.h>
#include <asm/ldt.h>

#include <generated/autoconf.h>

#include "kmodldt.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("kldt char device example");
MODULE_VERSION("0.01");

#define dump_hex(prefix, buf, len) \
	{ \
		print_hex_dump(KERN_INFO, prefix, \
				DUMP_PREFIX_ADDRESS, 16, 1, (buf), (len), true); \
		pr_info("\n"); \
	}

/* The 64-bit call gate descriptor, type == 0xC */
struct call_gate_64 {
	u16	offset0;
	u16	target_sel;
	u16	zero0 : 8, type : 5, dpl : 2, p : 1;
	u16	offset1;
	u32	offset2;
	u32	zero1;
} __attribute__((packed));

struct ldt_struct {
	unsigned long   *entries;
	unsigned int    nr_entries;
	int			    slot;    
};

/* This is a multiple of PAGE_SIZE. */
#define LDT_SLOT_STRIDE (LDT_ENTRIES * LDT_ENTRY_SIZE)

static inline void *ldt_slot_va(int slot)
{
	return (void *)(LDT_BASE_ADDR + LDT_SLOT_STRIDE * slot);
}

static void flush_ldt(void *__mm)
{
	struct mm_struct *mm = __mm;
	struct ldt_struct *ldt;

    //asm volatile("lldt %w0"::"q" (GDT_ENTRY_LDT*8));

	ldt = READ_ONCE(mm->context.ldt);

    // If page table isolation is enabled, ldt->entries
    // will not be mapped in the userspace pagetables.
    // Tell the CPU to access the LDT through the alias
    // at ldt_slot_va(ldt->slot).
    if (static_cpu_has(X86_FEATURE_PTI)) {
        if (WARN_ON_ONCE((unsigned long)ldt->slot > 1)) {
            // Whoops -- either the new LDT isn't mapped
            // (if slot == -1) or is mapped into a bogus
            // slot (if slot > 1).
            return;
        }

        set_ldt(ldt_slot_va(ldt->slot), ldt->nr_entries);
    } else {
        set_ldt(ldt->entries, ldt->nr_entries);
    }

    dump_hex("LDT ", ldt->entries, 32);
}

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
    struct mm_struct *mm = current->mm;
    struct ldt_struct *ldt;
    struct setup_gate gate;
    int err;

    pr_info("IOCTL %#08x: task group %d\n", cmd, current->pid);

    if (dump_stack_trace)
        dump_stack();

    if (cmd != KLDT_IOCTL_SETUP_GATE)
        return -ENOIOCTLCMD;

    if (copy_from_user(&gate, (void*)param, sizeof(struct setup_gate))) {
        pr_err("could not copy data from the user land\n");
        return -EINVAL;
    }

    if (down_write_killable(&mm->context.ldt_usr_sem))
		return -EINTR;

	ldt = READ_ONCE(mm->context.ldt);

    if (ldt) {
        int entry_idx = gate.idx;

        // The um must set two short descriptors of type 0xC (call gate)
        // prior to calling here. The syscall code only allows that when
        // the non-present segment bit is set.

        pr_info("base address %#lx, index %d\n", gate.base, gate.idx);
        pr_info("%d entries in LDT\n", ldt->nr_entries);

        if (entry_idx + 2 <= ldt->nr_entries) {
            struct call_gate_64 *gate_desc = (struct call_gate_64*)&ldt->entries[entry_idx];

        	gate_desc->offset0 = gate.base & 0xffff;
	        gate_desc->target_sel = gate.rpl == 3 ? __USER_CS : __KERNEL_CS;
            gate_desc->zero0 = 0;
            gate_desc->type = 0xC; // 64-bit call gate
            gate_desc->dpl = gate.rpl;
            gate_desc->p = 1;
            gate_desc->offset1 = (gate.base & 0xffff0000) >> 16;
            gate_desc->offset2 = gate.base >> 32;
            gate_desc->zero1 = 0;

	        mutex_lock(&mm->context.lock);
	        on_each_cpu_mask(mm_cpumask(mm), flush_ldt, mm, true);
	        mutex_unlock(&mm->context.lock);

            err = 0;
        } else {
            pr_err("not enough entries in LDT\n");
            err = -ENOTSUPP;
        }
    } else {
        pr_err("LDT is not allocated\n");
        err = -ENOTSUPP;
    }

	up_write(&mm->context.ldt_usr_sem);

    schedule();

    return err;
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
