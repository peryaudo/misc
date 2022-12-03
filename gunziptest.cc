// Based on https://cs.opensource.google/go/go/+/38801e55dbdd19d69935b92e38b1a4c9949316bf:src/lib/compress/flate/inflate.go;bpv=0

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

const int kNumMetaCode = 19;
uint32_t kMetaCodeOrder[kNumMetaCode] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

class BitReader {
public:
    BitReader(uint8_t* data) : data_(data) {}
    uint32_t Read(uint32_t requested_count) {
        FillIfNeeded(requested_count);
        uint32_t result = Get(requested_count);
        bytes_ >>= requested_count;
        available_count_ -= requested_count;
        return result;
    }
    uint32_t Get(uint32_t requested_count) {
        FillIfNeeded(requested_count);
        return bytes_ & ((1 << requested_count) - 1);
    }
    uint32_t GetReverse(uint32_t requested_count) {
        uint32_t original = Get(requested_count);
        uint32_t reversed = 0;
        for (int i = 0; i < requested_count; ++i) {
            reversed <<= 1;
            reversed += original & 1;
            original >>= 1;
        }
        return reversed;
    }
private:
    void FillIfNeeded(uint32_t requested_count) {
        if (available_count_ < requested_count) {
            bytes_ += (uint32_t)(data_[0]) << available_count_;
            ++data_;
            available_count_ += 8;
        }
    }
    uint32_t bytes_ = 0;
    uint32_t available_count_ = 0;
    uint8_t* data_;
};

class Huffman {
public:
    Huffman(const std::vector<int>& lengths) {
        // <length, count>
        std::map<int, int> counts;
        for (int length : lengths) {
            ++counts[length];
        }
        uint32_t code = 0;
        // <length, code>
        std::map<int, uint32_t> next_codes;
        for (const auto& pa : counts) {
            const int length = pa.first;
            const int count = pa.second;
            next_codes[length] = code;
            code += count;
            code <<= 1;
        }
        for (int i = 0; i < lengths.size(); ++i) {
            int length = lengths[i];
            uint32_t code = next_codes[length];
            codes_[std::make_pair(length, code)] = i;
            ++next_codes[length];
        }
    }

    uint32_t Read(BitReader& reader) {
        for (int i = 1; i <= 16; ++i) {
            uint32_t code = reader.GetReverse(i);
            if (codes_.count(std::make_pair(i, code))) {
                reader.Read(i);
                return codes_[std::make_pair(i, code)];
            }
        }
        assert(false && "cannot find a symbol");
    }

    // <<length, code>, symbol>
    std::map<std::pair<int, uint32_t>, uint32_t> codes_;
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
    if (type == 2) {
        uint32_t nlit = reader.Read(5) + 257;
        uint32_t ndist = reader.Read(5) + 1;
        uint32_t nclen = reader.Read(4) + 4;
        printf("nlit = %d ndist = %d nclen = %d\n", nlit, ndist, nclen);
        std::vector<int> metaCodeLengths(kNumMetaCode, 0);
        for (int i = 0; i < nclen; ++i) {
            metaCodeLengths[kMetaCodeOrder[i]] = reader.Read(3);
        }
        printf("Huffman meta code: \n");
        for (int i = 0; i < metaCodeLengths.size(); ++i)
            printf("[%d] = %d\n", i, metaCodeLengths[i]);
        Huffman metaCode(metaCodeLengths);
        for (int i = 0; i < nlit + ndist; ++i) {
            uint32_t symbol = metaCode.Read(reader);
            printf("%d\n", symbol);
        }
    }
    return 0;
}