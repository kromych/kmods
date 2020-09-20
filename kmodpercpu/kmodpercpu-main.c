#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/percpu.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("per cpu example");
MODULE_VERSION("0.01");

static DEFINE_PER_CPU(long, cpu_local_static) = 0x42;
static long __percpu *cpu_local_dynamic;

static int __init init_percpu_example(void)
{
    int cpu;

	pr_info("Entering the module at 0x%p\n", init_percpu_example);

	for_each_possible_cpu(cpu)
		per_cpu(cpu_local_static, cpu) = 0x50 + cpu;

	pr_info("cpu_local_static on CPU%d = %#08lx\n",
			smp_processor_id(), get_cpu_var(cpu_local_static));
	put_cpu_var(cpu_local_static);

	cpu_local_dynamic = alloc_percpu(long);

	for_each_possible_cpu(cpu)
		*per_cpu_ptr(cpu_local_dynamic, cpu) = 0x80 + cpu;

	pr_info("cpu_local_dynamic on CPU%d = %#08lx\n",
			smp_processor_id(), *get_cpu_ptr(cpu_local_dynamic));
	put_cpu_ptr(cpu_local_dynamic);

	return 0;
}

static void __exit exit_percpu_example(void)
{
	int cpu;
	pr_info("Exiting percpu module...\n");

	for_each_possible_cpu(cpu) {
		pr_info("cpu_local_static CPU%d = %#08lx\n", 
            cpu, per_cpu(cpu_local_static, cpu));
    }

    /* Below calling smp_processor_id() and not pinning the code to the CPU
        might give wrong results. get_cpu* disables preemption to fix that. */
	pr_info("cpu_local_dynamic = %#08lx on CPU%d\n", 
        *per_cpu_ptr(cpu_local_dynamic, smp_processor_id()), smp_processor_id());
	free_percpu(cpu_local_dynamic);

	pr_info("Module unloaded from 0x%p\n", exit_percpu_example);
}

module_init(init_percpu_example);
module_exit(exit_percpu_example);
