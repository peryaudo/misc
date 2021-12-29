#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <arpa/inet.h>

// http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

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
  uint32_t length = 0;
  uint8_t status = 0;
  uint8_t data1 = 0;
  uint8_t data2 = 0;

  void Read(FILE* fp, uint8_t prev_status) {
    length = 0;
    delta_time = ReadVariableLength(fp);

    // Running status
    status = fgetc(fp);
    ++length;
    if ((status & 0x80) == 0) {
      ungetc(status, fp);
      --length;
      status = prev_status;
    }

    if (status == 0xFF || status == 0xF0 || status == 0xF7) {
      if (status == 0xFF) {
        data1 = fgetc(fp);
        ++length;
      }
      uint32_t data_length = ReadVariableLength(fp);
      fseek(fp, data_length, SEEK_CUR);
      length += data_length;
      return;
    }
    switch (status & 0xF0) {
      case 0xC0:
      case 0xD0:
        data1 = fgetc(fp);
        ++length;
        break;
      case 0x80:
      case 0x90:
      case 0xA0:
      case 0xB0:
      case 0xE0:
        data1 = fgetc(fp);
        data2 = fgetc(fp);
        length += 2;
        break;
      default:
        printf("unsupported event type %02x\n", status);
        exit(1);
    }
  }

  uint32_t ReadVariableLength(FILE* fp) {
    uint32_t x = 0;
    for (int i = 0; i < 4; ++i) {
      uint32_t c = fgetc(fp);
      ++length;
      x <<= 7;
      x |= c & 0x7F;
      if ((c & 0x80) == 0)
        return x;
    }
    return 0;
  }

  void Dump() {
    printf("delta_time = %d status = %02x length = %d\n", delta_time, status, length);
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
    uint32_t rem_bytes = track.length;
    uint8_t prev_status = 0;
    while (rem_bytes > 0) {
      MIDIEvent event;
      event.Read(fp, prev_status);
      event.Dump();
      prev_status = event.status;
      rem_bytes -= event.length;
    }
  }
  fclose(fp);
  return 0;
}