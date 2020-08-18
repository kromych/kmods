#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "kmodprocfs.h"

void test_read(void) {
    int fd = open(PROC_FILE_PATH, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Opening " PROC_FILE_PATH " failed: %#04x\n", errno);
    } else {
        char *buffer = calloc(1, PROC_BUF_SIZE);

        if (!buffer) {
            fputs("Not enough memory", stderr);
        } else {
            if (ioctl(fd, PROC_IOCTL_RESET_POS) == 0) {
                size_t bytes_read;

                bytes_read = read(fd, buffer, PROC_BUF_SIZE);
                fprintf(stdout, "Read %lu bytes: %s\n", bytes_read, buffer);
            } else {
                fputs("IOCTL failed", stderr);
            }

            free(buffer);
        }

        close(fd);
    }
}

void test_write(void) {
    FILE* f = fopen(PROC_FILE_PATH, "wb");

    if (!f) {
        fprintf(stderr, "Opening " PROC_FILE_PATH " failed: %#04x\n", errno);
    } else {
        char buffer[] = "testTest_TeST";

        if (!buffer) {
            fputs("Not enough memory", stderr);
        } else {
            size_t nitem_written;

            nitem_written = fwrite(buffer, sizeof(buffer), 1, f);
            fprintf(stdout, "Wrote %lu bytes: %s\n", nitem_written*sizeof(buffer), buffer);
        }

        fclose(f);
    }
}

void test_mmap(void) {
    int fd = open(PROC_FILE_PATH, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Opening " PROC_FILE_PATH " failed: %#04x\n", errno);
    } else {
        char* addr = mmap(NULL, PROC_BUF_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    
        if (addr == MAP_FAILED) {
            fprintf(stderr, "Mapping failed: %#04x\n", errno);
        } else { 
            fprintf(stdout, "Mapped at: %p\n", addr);
            
            close(fd);            

            fprintf(stdout, "Contents: %s\n", addr);

            munmap(addr, PROC_BUF_SIZE);
        }
    }
}

int main() {
    test_write();
    test_read();
    test_mmap();
    //test_ioctl();
    //test_seek();

    return 0;
}