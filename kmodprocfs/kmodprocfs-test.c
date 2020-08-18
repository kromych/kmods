#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "kmodprocfs.h"

void test_read(void) {
    FILE* f = fopen(PROC_FILE_PATH, "rb");

    if (!f) {
        fprintf(stderr, "Opening " PROC_FILE_PATH " failed: %#04x\n", errno);
    } else {
        char* buffer = calloc(1, PROC_BUF_SIZE);

        if (!buffer) {
            fputs("Not enough memory", stderr);
        } else {
            fread(buffer, PROC_BUF_SIZE, 1, f);
            free(buffer);
        }

        fclose(f);
    }
}

void test_write(void) {
    FILE* f = fopen(PROC_FILE_PATH, "wb");

    if (!f) {
        fprintf(stderr, "Opening " PROC_FILE_PATH " failed: %#04x\n", errno);
    } else {
        char* buffer = calloc(1, PROC_BUF_SIZE);

        if (!buffer) {
            fputs("Not enough memory", stderr);
        } else {
            fwrite(buffer, PROC_BUF_SIZE, 1, f);
            free(buffer);
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
    
        if (!addr) {
            fprintf(stderr, "Mapping failed: %#04x", errno);
        } else { 
            fprintf(stdout, "Mapped at: %p", addr);
            munmap(addr, PROC_BUF_SIZE);
        }

        close(fd);
    }
}

int main() {
    test_read();
    test_write();
    //test_ioctl();
    //test_seek();
    test_mmap();

    return 0;
}