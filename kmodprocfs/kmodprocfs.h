#ifndef __kmod_procfs__
#define __kmod_procfs__

#define PROC_DIR_NAME        "kmodprocfs"
#define PROC_DIR_PATH        "/proc/" PROC_DIR_NAME "/"
#define PROC_BUF_SIZE        4096
#define PROC_MAX_NAME_LEN    32
#define PROC_DEFAULT_ENTRIES 8

#define PROC_IOCTL_BASE         ('P' << 24 | 'I' << 16 | 'R' << 8)
#define PROC_IOCTL_RESET_POS    PROC_IOCTL_BASE | 0

#endif
