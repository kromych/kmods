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

static struct lcall_addr addr;
static char *code_start;

int main() {
    int fd = -1;
    char *target = NULL;
    int ldt_index = 0;
    unsigned char rpl = 3;
    unsigned short sel;

    affinitize();

    // Open our special device
    fd = open("/dev/" KLDT_NAME, O_RDWR);
    ASSERT(fd >= 0);

    // Allocate memory
    target = mmap(
        NULL, 0x2000,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT(target != MAP_FAILED);
    printf("base address: %#lx\n", (unsigned long)target);

    code_start = target + 0x800;

    // See if possible to execute anything on the page
    code_start[0] = 0x90; // NOP
    code_start[1] = 0xC3; // RET
    asm volatile ("call *code_start\n");

    // Time to setup the LDT.
    // Reserve space for a long descriptor by setting two short ones
    // of any type, and the third one for the code segment.
    struct user_desc desc = {
        .limit           = 0xfff,
        .base_addr       = 0xffffffff,
        .seg_32bit       = 1,
        .contents        = 0,
        .read_exec_only  = 1,
        .limit_in_pages  = 1,
        .seg_not_present = 0,
        .useable         = 0,
    };
    desc.entry_number = ldt_index;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);
    desc.entry_number = ldt_index + 1;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);
    desc.entry_number = ldt_index + 2;
    ASSERT(syscall(SYS_modify_ldt, 0x11, &desc, sizeof(desc)) == 0);

    struct setup_gate gate = {
        .base = (unsigned long)code_start, // Can a compiled function, too, of course
        .idx = ldt_index,
        .rpl = rpl
    };
    
    ASSERT(ioctl(fd, KLDT_IOCTL_SETUP_GATE, &gate) == 0);

    // Segment selector (what goes into a segment register):
    //  0:1     RPL (request priviledge)
    //  2       Table (0 - GDT, 1 - LDT)
    //  3:15    Index in the descriptor table
    sel = (gate.idx << 3) | 4 /*LDT*/ | (rpl & 0x3);

    code_start[0] = 0x90; // NOP
    code_start[1] = rpl == 3 ? 0xC3 : 0xCB /*LRET*/; // LRET is needed for a priveledge-changing call
    addr.offset = 0; // Ignored for the long call
    addr.sel = sel;

    // Adding rex.w would've meant the offset part has 64 bits.
    // That works on Intel parts but not on the AMD ones.
    asm volatile ("lcall *(addr)\n");

    return 0;
}
