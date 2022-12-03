#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <gzip file name>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st)) {
        perror("fstat");
        return 1;
    }

    uint8_t* data = (uint8_t*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
    }
    if (data[0] != 0x1f || data[1] != 0x8b || data[2] != 8) {
        fprintf(stderr, "not a gzip file\n");
        return 0;
    }
    uint8_t flags = data[3];
    printf("flags = %02x\n", flags);
    data += 10;
    if (flags == 0) {
        // do nothing
    } else if (flags == 8) {
        printf("file name = %s\n", (char *) data);
        while (*data) ++data;
        ++data;
    }
    uint8_t byte = data[0];
    printf("final? = %s type = %d\n", (byte & 1 ? "yes" : "no"), (byte & 6) >> 1);
    return 0;
}