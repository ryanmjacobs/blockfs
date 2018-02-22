#include "buse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

static int fd = -1;
static int size = -1;
static void *data = NULL;

static int xmpl_debug = 1;
static const char *block = "/dev/nbd0";

static int xmp_read(void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    if (*(int *)userdata)
        fprintf(stderr, "R - %lu, %u\n", offset, len);

    memcpy(buf, (char *)data + offset, len);
    return 0;
}

static int xmp_write(const void *buf, u_int32_t len, u_int64_t offset, void *userdata) {
    if (*(int *)userdata)
        fprintf(stderr, "W - %lu, %u\n", offset, len);

    memcpy((char *)data + offset, buf, len);
    return 0;
}

static void xmp_disc(void *userdata) {
    fprintf(stderr, "Received a disconnect request.\n");
}

static int xmp_flush(void *userdata) {
    fprintf(stderr, "Received a flush request.\n");
    return 0;
}

static int xmp_trim(u_int64_t from, u_int32_t len, void *userdata) {
    fprintf(stderr, "T - %lu, %u\n", from, len);
    return 0;
}

static void *get_mmap(const char *fname, size_t size) {
    fd = open(fname, O_RDWR);

    if (fd == -1) {
        perror("open");
        exit(1);
    }

    data = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    return data;
}

static off_t get_fsize(const char *fname) {
    struct stat st;
    if (stat(fname, &st) == -1) {
        perror("stat");
        exit(1);
    }
    return st.st_size;
}

static void cleanup(void) {
    fprintf(stderr, "cleaning up...\n");
    if (data != NULL) munmap(data, size);
    if (fd != -1) close(fd);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <block>\n", argv[0]);
        return 1;
    }

    atexit(cleanup);
    size = get_fsize(argv[1]);
    data = get_mmap(argv[1], size);

    struct buse_operations aop = {
        .read = xmp_read,
        .write = xmp_write,
        .disc = xmp_disc,
        .flush = xmp_flush,
        .trim = xmp_trim,
        .size = size
    };

    return buse_main(block, &aop, (void *)&xmpl_debug);
}
