#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <asm/ldt.h>

#include "kmodldt.h"

#define ASSERT2(cond, str_cond) \
    if (!(cond)) \
        { printf("Assertion failed: '" str_cond "' %s:%d\n", __FILE__, __LINE__); abort(); }
#define ASSERT(cond) ASSERT2(cond, #cond)

struct lcall_addr {
    unsigned long offset;
    unsigned short sel;
} __attribute__((packed));

static struct lcall_addr addr;

int main() {
    
	char *target = mmap(
		NULL, 0x1000,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	ASSERT(target != MAP_FAILED);

    target[0] = 0x90; // NOP
    target[1] = 0xC3; // RET

	printf("base address: %#lx\n", (unsigned long)target);

	struct user_desc desc = {
		.limit           = 0xfff,
		.seg_32bit       = 0,
		.contents        = 3,
		.read_exec_only  = 1,
		.limit_in_pages  = 1,
		.seg_not_present = 1,
		.useable         = 1,
	};

    // Reserve space for a long descriptor by setting two short ones
    desc.entry_number = 0;
	desc.base_addr = (unsigned long)target & 0xffffffff;
	ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);
    desc.entry_number = 1;
	desc.base_addr = (unsigned long)target >> 32;
	ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);

    addr.offset = (unsigned long)target;
    addr.sel = 0; // Only matters for the long call

    asm volatile (
        "call *addr\n"
    );

    asm volatile (
        "lcall *addr\n"
    );

    return 0;
}
