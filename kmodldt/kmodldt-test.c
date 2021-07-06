#define _GNU_SOURCE 
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

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
    unsigned int offset;
    unsigned short sel;
} __attribute__((packed));

void __attribute__((__unused__)) affinitize(void)
{
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(0, &set);
    ASSERT(sched_setaffinity(getpid(), sizeof(set), &set) == 0);
}

void test_same_priviledge(void)
{
    const size_t size = 0x2000;
    const size_t code_offset = 0x800;    

    const unsigned short ldt_index = 0;
    const unsigned char  rpl = 3;

    // Segment selector (what goes into a segment register):
    //  0:1     RPL (request priviledge)
    //  2       Table (0 - GDT, 1 - LDT)
    //  3:15    Index in the descriptor table
    const unsigned short sel = (ldt_index << 3) | 4 /*LDT*/ | (rpl & 0x3);

    int  fd = -1;
    char *target = NULL;
    char *code_start;

    struct user_desc    desc = {
        .limit           = 0xfff,
        .base_addr       = 0xffffffff,
        .seg_32bit       = 1,
        .contents        = 0,
        .read_exec_only  = 1,
        .limit_in_pages  = 1,
        .seg_not_present = 0,
        .useable         = 0,
    };
    struct setup_gate gate = {
        .base = 0, // will point to the allocated memory
        .idx = ldt_index,
        .rpl = rpl
    };   
    struct lcall_addr lcall_addr = {
        .offset = 0, // Ignored for far calls
        .sel = sel
    };

    // Open our special device

    fd = open("/dev/" KLDT_NAME, O_RDWR);
    ASSERT(fd >= 0);

    // Allocate memory

    target = mmap(
        NULL, size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT(target != MAP_FAILED);
    
    code_start = target + code_offset;
    code_start[0] = 0x90; // NOP
    code_start[1] = 0xC3; // RET is fine for the same priviledge call

    printf("base address: %#lx\n", (unsigned long)target);
    ASSERT(mprotect(target, size, PROT_READ | PROT_EXEC) == 0);

    // See if possible to execute anything on the page
    asm volatile (
        "leaq   %0, %%rax\n"
        "call   *(%%rax)\n"
        : : "m"(code_start) : "rax"
    );

    // Time to setup the LDT.
    // Reserve space for a long descriptor by setting two short ones
    // of any type, and the third one for the code segment.

    desc.entry_number = ldt_index;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);
    desc.entry_number = ldt_index + 1;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);
    desc.entry_number = ldt_index + 2;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);

    // Can point to a compiled function, too, of course
    gate.base = (unsigned long)code_start;
    ASSERT(ioctl(fd, KLDT_IOCTL_SETUP_GATE, &gate) == 0);

    // Adding rex.w would've meant the offset part has 64 bits.
    // That works on Intel parts but not on the AMD ones.
    asm volatile (
        "leaq   %0, %%rax\n"
        "lcall  *(%%rax)\n"
        : : "m"(lcall_addr) : "rax"
    );

    munmap(target, size);
    close(fd);
}

void test_ring0(void)
{
}

int main()
{
    test_same_priviledge();

    return 0;
}
