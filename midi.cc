#include <cstdint>
#include <cstdio>

#include <arpa/inet.h>

struct __attribute__((__packed__)) MIDIHeader {
  char magic[4] = {};
  uint32_t length = 0;
  uint16_t format = 0;
  uint16_t ntrks = 0;
  uint16_t division = 0;

  void Read(FILE* fp) {
    fread(this, sizeof(*this), 1, fp);
    length = ntohl(length);
    format = ntohs(format);
    ntrks = ntohs(ntrks);
    division = ntohs(division);
  }

  void Dump() {
    printf("%.4s length = %u, format = %u, ntrks = %u, division = %u\n",
           magic, length, format, ntrks, division);
  }
};

struct __attribute__((__packed__)) MIDITrack {
  char magic[4] = {};
  uint32_t length = 0;

  void Read(FILE* fp) {
    fread(this, sizeof(*this), 1, fp);
    length = ntohl(length);
  }

  void Dump() {
    printf("%.4s length = %u\n", magic, length);
  }
};

struct MIDIEvent {
  uint32_t delta_time = 0;

  void Read(FILE* fp) {
    delta_time = 0;
    while (true) {
      uint32_t c = fgetc(fp);
      delta_time <<= 7;
      delta_time |= c & 0x7F;
      if ((c & 0x80) == 0)
        break;
    }
    // WIP
  }
};

int main(int argc, char *argv[]) {
  FILE *fp = fopen("BGM8.MID", "rb");
  MIDIHeader header;
  header.Read(fp);
  header.Dump();
  for (int i = 0; i < header.ntrks; ++i) {
    MIDITrack track;
    track.Read(fp);
    track.Dump();
    fseek(fp, track.length, SEEK_CUR);
  }
  fclose(fp);
  return 0;
}
