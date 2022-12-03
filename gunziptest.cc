// Based on https://cs.opensource.google/go/go/+/38801e55dbdd19d69935b92e38b1a4c9949316bf:src/lib/compress/flate/inflate.go;bpv=0

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstdio>

class BitReader {
public:
    BitReader(uint8_t* data) : data_(data) {}
    uint32_t Read(uint32_t requested_count) {
        if (available_count_ < requested_count) {
            bytes_ += (uint32_t)(data_[0]) << available_count_;
            ++data_;
            available_count_ += 8;
        }
        uint32_t result = bytes_ & ((1 << requested_count) - 1);
        bytes_ >>= requested_count;
        available_count_ -= requested_count;
        return result;
    }
    uint32_t bytes_ = 0;
    uint32_t available_count_ = 0;
    uint8_t* data_;
};

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
    BitReader reader(data);
    uint32_t final = reader.Read(1);
    uint32_t type = reader.Read(2);
    printf("final? = %s type = %d\n", (final ? "yes" : "no"), type);
    return 0;
}