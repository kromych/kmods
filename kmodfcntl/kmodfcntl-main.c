#define pr_fmt(fmt) "kmodfcntl: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("fcntl module");
MODULE_VERSION("0.01");

static struct task_struct *producer_task;

static u8 message;
DECLARE_COMPLETION(message_ready);

#define PRODUCE_PERIOD_MSEC 2000

int kmodfcntl_producer_kthread(void* dummy)
{
	pr_info("Starting producing messages\n");
	while (!kthread_should_stop()) {
		msleep_interruptible(PRODUCE_PERIOD_MSEC);

		if (!cmpxchg(&message, 0, 'M')) {
			pr_info("Produced message\n");
			complete(&message_ready);
		} else {
			pr_info("Previous message not read, not producing\n");
		}
	}

	return 0;
}

static u8 kmodfcntl_get_message(void)
{
	u8 msg = xchg(&message, 0);
	if (msg) {
		pr_info("Consumed message, pid %d\n", current->pid);
		reinit_completion(&message_ready);
	}

	return msg;
}

static ssize_t kmod_fcntl_read(struct file *, char __user *, size_t, loff_t *);

static const struct file_operations kmod_fcntl_file_ops = {
    .owner = THIS_MODULE,
    .read = kmod_fcntl_read,
};

static ssize_t kmod_fcntl_read(struct file *filp, char __user *user_buf, size_t user_buf_size, loff_t *user_offset)
{
	int ret;
	u8 msg;

	if (user_buf_size == 0)
		return -EINVAL;

	for (;;) {
		bool block = (filp->f_flags & O_NONBLOCK) == 0;
		if (block) {
			ret = wait_for_completion_interruptible(&message_ready);
			if (ret)
				return ret;
		}

		msg = kmodfcntl_get_message();
		if (msg) {
			if (copy_to_user(user_buf, &msg, 1))
				return -EFAULT;
			break;
		} else {
			if (!block) 
				return -EAGAIN;
		}
	}

    return 1;
}

static struct miscdevice kmod_fcntl_dev = {
    .minor = MISC_DYNAMIC_MINOR,
	.name = "kmod_fcntl",
	.nodename = "kmod_fcntl",
	.fops = &kmod_fcntl_file_ops,
	.mode = 0755
};

static int __init init_kmodfcntl(void)
{
	int ret;

	pr_info("Initializing\n");

    pr_info("Registering %s in %s\n", kmod_fcntl_dev.name, __func__);
    ret = misc_register(&kmod_fcntl_dev);
	if (ret)
		return ret;

	producer_task = kthread_run(kmodfcntl_producer_kthread, NULL, "kmodfcntl_producer_kthread");
	if (!producer_task)
		return -ENOMEM;

	if (IS_ERR(producer_task))
		return PTR_ERR(producer_task);

	wake_up_process(producer_task);

	pr_info("Initialized\n");

	return 0;
}

static void __exit exit_kmodfcntl(void)
{
	pr_info("Exiting\n");

	misc_deregister(&kmod_fcntl_dev);

	kthread_stop(producer_task);
	wake_up_process(producer_task);

	pr_info("Exited\n");
}

module_init(init_kmodfcntl);
module_exit(exit_kmodfcntl);
