#ifndef __kmod_ldt__
#define __kmod_ldt__

#define KLDT_NAME              "kldt"

#define KLDT_IOCTL_BASE         'L'
#define KLDT_IOCTL_SETUP_GATE   _IO(KLDT_IOCTL_BASE, 0)

struct setup_gate {
    unsigned short idx;
    unsigned long base;
    unsigned char rpl;
};

#endif
