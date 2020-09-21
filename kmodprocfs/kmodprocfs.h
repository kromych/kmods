#ifndef __kmod_procfs__
#define __kmod_procfs__

#include <linux/ioctl.h>

#define PROC_DIR_NAME        "kmodprocfs"
#define PROC_DIR_PATH        "/proc/" PROC_DIR_NAME "/"
#define PROC_BUF_SIZE        4096
#define PROC_MAX_NAME_LEN    32
#define PROC_DEFAULT_ENTRIES 8

#define PROC_IOCTL_BASE         'P'
#define PROC_IOCTL_RESET_POS    _IO(PROC_IOCTL_BASE, 0)

#endif
