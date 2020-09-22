#ifndef __kmod_chardev__
#define __kmod_chardev__

#include <linux/ioctl.h>

#define KCDEV_MINOR_START    0
#define KCDEV_MAX_DEVICES    8
#define KCDEV_NAME           "kcdev"
#define KCDEVICE_CLASS       "kcdev_class"

#define KCDEV_BUF_SIZE        4096
#define KCDEV_MAX_NAME_LEN    32
#define KCDEV_DEFAULT_ENTRIES 8

#endif
