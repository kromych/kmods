#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("Hello module");
MODULE_VERSION("0.01");

// module_param();

static int __init init_hello(void)
{
    // Without \n, the output will be delayed until the printk buffer is full
    pr_alert("Hello, world!\n");
    dump_stack();
    return 0;
}

static void __exit exit_hello(void)
{
    // Without \n, the output will be delayed until the printk buffer is full
    pr_alert("Bye, world!\n");
    dump_stack();
}

module_init(init_hello);
module_exit(exit_hello);
