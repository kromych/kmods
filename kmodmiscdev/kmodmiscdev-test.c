#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "kmodmiscdev.h"

#define ASSERT2(cond, str_cond) \
    if (!(cond)) \
        { printf("(%d, %d) Assertion failed: '" str_cond "' in %s:%d\n", i, j, __FILE__, __LINE__); abort(); }
#define ASSERT(cond) ASSERT2(cond, #cond)

static const char* data[] = {
    "1",
    "22",
    "333",
    "4444",
    "55555",
    "666666",
    "7777777",
    "88888888"
};

static char buffer[KMISC_BUF_SIZE*2];

static void test_read_write(void) {
    int fd;
    size_t bytes_read;
    size_t bytes_written;

    int i, j;

    fd = open("/dev/" KMISC_NAME, O_RDWR);

    if (fd == -1) {
        fprintf(stderr, "Couldn't open the file: %#04x\n", errno);
        return;
    }

    bytes_read = read(fd, buffer, sizeof buffer);
    ASSERT(bytes_read >= 0);

    fprintf(stdout, "Had %ld bytes in the buffer before the test\n", bytes_read);

    ASSERT(read(fd, buffer, KMISC_BUF_SIZE) == 0);
    ASSERT(write(fd, buffer, 2*KMISC_BUF_SIZE) == KMISC_BUF_SIZE);
    ASSERT(write(fd, buffer, 2*KMISC_BUF_SIZE) == 0);
    ASSERT(read(fd, buffer, 2*KMISC_BUF_SIZE) == KMISC_BUF_SIZE);
    ASSERT(read(fd, buffer, KMISC_BUF_SIZE) == 0);

    for (i = 0; i < 2*KMISC_BUF_SIZE; ++i) {
        for (j = 0; j < sizeof data/sizeof data[0]; ++j) {
            ASSERT(read(fd, buffer, j + 1) == 0);

            ASSERT(write(fd, data[j], j + 1) == j + 1);
            ASSERT(write(fd, buffer, KMISC_BUF_SIZE) == KMISC_BUF_SIZE - (j + 1));
            ASSERT(write(fd, data[j], 1) == 0);
            ASSERT(read(fd, buffer, KMISC_BUF_SIZE - (j + 1)) == KMISC_BUF_SIZE - (j + 1));

            ASSERT(write(fd, data[j], j + 1) == j + 1);
            ASSERT(write(fd, data[j], j + 1) == j + 1);

            ASSERT(read(fd, buffer, j + 1) == j + 1);
            ASSERT(read(fd, buffer, j + 1) == j + 1);
            ASSERT(read(fd, buffer, j + 1) == j + 1);

            ASSERT(read(fd, buffer, j + 1) == 0);
        }
    }

    fprintf(stdout, "(%d, %d) Test has passed\n", i, j);

    close(fd);
}

int main() { 
    test_read_write();
    return 0;
}
