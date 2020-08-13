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
    pr_alert("Hello, world!");
    return 0;
}

static void __exit exit_hello(void)
{
    pr_alert("Bye, world!");
}

module_init(init_hello);
module_exit(exit_hello);
