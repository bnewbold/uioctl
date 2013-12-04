/*
 * uioctl.c: Userspace I/O manipulation utility
 *
 * Copyright (C) 2013, Bryan Newbold <bnewbold@leaflabs.com>
 *
 * ISC License: Permission to use, copy, modify, and/or distribute this
 * software for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear in all
 * copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Changelog:
 *  2013-12-04: bnewbold: Initial version; missing width management, list mode
 *
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/timeb.h>

#define DEFAULT_WIDTH 4
#define PROGRAM_NAME "uioctl"

enum mode_type {
    MODE_READ,
    MODE_WRITE,
    MODE_LIST,
    MODE_MONITOR
};

static void usage(int exit_status) {
    fprintf(exit_status == EXIT_SUCCESS ? stdout : stderr,
        "Usage: %s [options] [-l] [/dev/uioX [-m] [<addr> [<value>]]]\n"
        "\n"
        "Functions:\n"
        "  monitor (-m) the device for interrupts\n"
        "  list (-l) all devices and their mappings\n"
        "  read words from <addr>\n"
        "  write <value> to <addr> (will zero-pad word width)\n"
        "\n"
        "Options:\n"
        "  -r\tselect the device's memory region to map (default: 0)\n"
        "  -w\tword size (in bytes; default: %d)\n"
        "  -n\tnumber of words to read (in words; default: 1)\n"
        "  -x\texit with success after the first interrupt (implies -m mode)\n"
        ,
        PROGRAM_NAME, DEFAULT_WIDTH);
    exit(exit_status);
}

static void list_devices() {
    // TODO:
    // - try to list /sys/devices/uio*
    // - for each in the above, print memory regions
    fprintf(stderr, "listing not yet implemented\n");
    exit(EXIT_FAILURE);
}

static void monitor(char *fpath, int forever) {
    char buf[4];
    int bytes;
    int fd;
    struct timeb tp;
    char startval[] = {0,0,0,1};
   
    printf("Waiting for interrupts on %s\n", fpath);
    fd = open(fpath, O_RDWR);
    if (fd < 1) {
        perror("uioctl");
        fprintf(stderr, "Couldn't open UIO device file: %s\n", fpath);
        exit(EXIT_FAILURE);
    }
    do {
        bytes = pwrite(fd, &startval, 4, 0);
        if (bytes != 4) {
            perror("uioctl");
            fprintf(stderr, "Problem clearing device file\n");
            exit(EXIT_FAILURE);
        }
        //lseek(fd, 0, SEEK_SET);
        bytes = pread(fd, buf, 4, 0);
        ftime(&tp);
        if (bytes != 4) {
            perror("uioctl");
            fprintf(stderr, "Problem reading from device file\n");
            exit(EXIT_FAILURE);
        }
        printf("[%ld.%03d] interrupt: %d\n", tp.time, tp.millitm,
            (buf[3] * 16777216 + buf[2] * 65536 + buf[1] * 256 + buf[0]));
    } while (forever);
    close(fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int c;
    int fd;
    enum mode_type mode = MODE_READ;
    char *fpath;
    void *ptr;
    int map_size, value, addr;
    int region = 0;
    int count = 1;
    int width = DEFAULT_WIDTH;
    int forever = 1;

    while((c = getopt(argc, argv, "hlmxr:n:w:")) != -1) {
        errno = 0;
        switch(c) {
        case 'h':
            usage(EXIT_SUCCESS);
        case 'l':
            list_devices();
        case 'm':
            mode = MODE_MONITOR;
            break;
        case 'x':
            mode = MODE_MONITOR;
            forever = 0;
            break;
        case 'r':
            region = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            if (region != 0) {
                fprintf(stderr, "region != 0 not yet implemented\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'n':
            count = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            break;
        case 'w':
            width = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            if (width != 4) {
                fprintf(stderr, "width != 4 not yet implemented\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Unexpected argument; try -h\n");
            exit(EXIT_FAILURE);
        }
    }

    if (mode == MODE_MONITOR) {
        if ((argc - optind) != 1) {
            fprintf(stderr, "Wrong number of arguments; try -h\n");
            exit(EXIT_FAILURE);
        }
        fpath = argv[optind];
        monitor(fpath, forever);
    }
    if (mode == MODE_READ) {
        if ((argc - optind) == 2) {
            fpath = argv[optind++];
            errno = 0;
            addr = strtoul(argv[optind++], NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            //printf("addr: 0x%08x\n", addr);
        } else
        if ((argc - optind) == 3) {
            mode = MODE_WRITE;
            fpath = argv[optind++];
            errno = 0;
            addr = strtoul(argv[optind++], NULL, 0);
            value = strtoul(argv[optind++], NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            //printf("addr: 0x%08x\n", addr);
            //printf("value: 0x%08x\n", value);
        } else {
            fprintf(stderr, "Wrong number of arguments; try -h\n");
            exit(EXIT_FAILURE);
        }
    }

    map_size = count * width;

    fd = open(fpath, O_RDWR|O_SYNC);
    if (fd < 1) {
        perror("uioctl");
        fprintf(stderr, "Couldn't open UIO device file: %s\n", fpath);
        return EXIT_FAILURE;
    }
    ptr = mmap(NULL, map_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    /* TODO: do the things... */
    /* value = *((unsigned *) (ptr + offset); */
    if (mode == MODE_READ) {
        for (; count > 0; count--) {
            value = *((unsigned *) (ptr + addr));
            printf("0x%08x\t%08x\n", addr, value);
            addr += width;
        }
    } else {
        *((unsigned *)(ptr + addr)) = value;
    }

    /* clean up */
    munmap(ptr, map_size);
    close(fd);

    return EXIT_SUCCESS;
}
