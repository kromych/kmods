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

struct ring_buf {
    spinlock_t lock;
    size_t size;
    atomic_t read_idx;
    atomic_t write_idx;
    struct page *pages;
    u8 *buf;
};

static struct ring_buf *ring_buf_alloc(size_t page_count)
{
	int i;
	struct page **double_map; // Wraps around
	struct ring_buf* ring;

	double_map = NULL;

	ring = kmalloc(sizeof(struct ring_buf), GFP_KERNEL);
	if (!ring)
		goto fail;

	ring->size = page_count * PAGE_SIZE;
	spin_lock_init(&ring->lock);

	ring->pages = alloc_pages(GFP_KERNEL, get_order(ring->size));

	if (!ring->pages)
		goto fail;

	double_map = kcalloc(page_count*2, sizeof(struct page*), GFP_KERNEL);
	if (!double_map)
		goto fail;
	
	for (i = 0; i < page_count; ++i)
		double_map[i] = double_map[i + page_count] = &ring->pages[i];

	ring->buf = vmap(double_map, page_count*2, VM_MAP, PAGE_KERNEL);
    ring->buf[4096] = 0xAA;
    ring->buf[4097] = 0xD1;

    pr_info("ring buffer mapped at: %llx %#x %#x\n", (u64)&ring->buf[0], ring->buf[0], ring->buf[1]);

	if (!ring->buf)
		goto fail;

	kfree(double_map);

	return ring;

fail:

    if (double_map)
        kfree(double_map);

    if (ring) {
        if (ring->buf)
            vunmap(ring->buf);

        if (ring->pages)
            __free_pages(ring->pages, get_order(ring->size));

        kfree(ring);
    }

    return NULL;
}

static void ring_buf_free(struct ring_buf *ring)
{
    if (ring) {
        if (ring->buf)
            vunmap(ring->buf);

        if (ring->pages)
            __free_pages(ring->pages, get_order(ring->size));

        kfree(ring);
    }
}

struct kmisc_dev {
    struct ring_buf *ring;
    struct miscdevice misc_dev;
    struct device_attribute attr;
};

static ssize_t __maybe_unused kmisc_attr_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    return sprintf(buf, "%s\n", dev->init_name);
}

static ssize_t __maybe_unused kmisc_attr_store(struct device *dev, struct device_attribute *attr,
            const char *buf, size_t count)
{
	return strnlen(buf, count);
}
             
static int kmisc_open(struct inode *inode, struct file *filp)
{
    struct kmisc_dev* dev = container_of(filp->private_data,  struct kmisc_dev, misc_dev);

    pr_info("Opening %s in %s\n", dev->misc_dev.name, __func__);
    return 0;
}

static int kmisc_release(struct inode *inode, struct file *filp)
{
    struct kmisc_dev* dev = container_of(filp->private_data,  struct kmisc_dev, misc_dev);

    pr_info("Closing %s in %s\n", dev->misc_dev.name, __func__);
    return 0;
}

static struct kmisc_dev *dev;

static int __init init_kmisc_example(void)
{
    int ret;

    dev = kzalloc(sizeof(struct kmisc_dev), GFP_KERNEL);

    if (!dev)
        return -ENOMEM;

    dev->ring = ring_buf_alloc(1);

    if (!dev->ring)
        return -ENOMEM;

    dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	dev->misc_dev.name = "kmisc";
	dev->misc_dev.nodename = dev->misc_dev.name;
	dev->misc_dev.fops = &kmisc_file_ops;
	dev->misc_dev.mode = 0755;
    dev->attr.attr.name = dev->misc_dev.name;
    dev->attr.attr.mode = dev->misc_dev.mode;
    dev->attr.show = kmisc_attr_show;
    dev->attr.store = kmisc_attr_store;

    pr_info("Registering %s in %s\n", dev->misc_dev.name, __func__);

    ret = misc_register(&dev->misc_dev);

    if (ret == 0)
        ret = device_create_file(dev->misc_dev.this_device, &dev->attr);

    return ret;
}

static void __exit exit_kmisc_example(void)
{
    if (dev) {
        pr_info("Deregistering %s in %s\n", dev->misc_dev.name, __func__);
        device_remove_file(dev->misc_dev.this_device, &dev->attr);
        misc_deregister(&dev->misc_dev);
        ring_buf_free(dev->ring);
        kfree(dev);
        dev = NULL;
    }
}

module_init(init_kmisc_example);
module_exit(exit_kmisc_example);
