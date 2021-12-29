#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

#include <arpa/inet.h>

const int32_t kSampleRate = 44100;

struct WaveHeader {
  WaveHeader(int32_t raw_length) {
    subchunk2_size = raw_length;
    chunk_size = raw_length + 36;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    block_align = num_channels * bits_per_sample / 8;
  }
  char chunk_id[4] = {'R', 'I', 'F', 'F'};
  int32_t chunk_size;
  char format[4] = {'W', 'A', 'V', 'E'};

  char subchunk1_id[4] = {'f', 'm', 't', ' '};
  int32_t subchunk1_size = 16;
  int16_t audio_format = 1;
  int16_t num_channels = 1;
  int32_t sample_rate = kSampleRate;
  int32_t byte_rate;
  int16_t block_align;
  int16_t bits_per_sample = 16;

  char subchunk2_id[4] = {'d', 'a', 't', 'a'};
  int32_t subchunk2_size;
};

double MidiFreq(int n) {
  return pow(2, (n - 69.0) / 12.0) * 440.0;
}

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

  void Dump() const {
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

  void Dump() const {
    printf("%.4s length = %u\n", magic, length);
  }
};

enum MIDIEventType {
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  POLYPHONIC_KEY_PRESSURE = 0xA0,
  CONTROL_CHANGE = 0xB0,
  PROGRAM_CHANGE = 0xC0,
  CHANNEL_PRESSURE = 0xD0,
  PITCH_BEND = 0xE0,
  SYSEX = 0xF0,
  METADATA = 0xFF,
};

enum MIDIMetadataType {
  END_OF_TRACK = 0x2F,
  SET_TEMPO = 0x51,
};

const char* GetEventTypeName(MIDIEventType event_type) {
  switch (event_type) {
    case NOTE_OFF: return "NOTE_OFF";
    case NOTE_ON: return "NOTE_ON";
    case POLYPHONIC_KEY_PRESSURE: return "POLYPHONIC_KEY_PRESSURE";
    case CONTROL_CHANGE: return "CONTROL_CHANGE";
    case PROGRAM_CHANGE: return "PROGRAM_CHANGE";
    case CHANNEL_PRESSURE: return "CHANNEL_PRESSURE";
    case PITCH_BEND: return "PITCH_BEND";
    case SYSEX: return "SYSEX";
    case METADATA: return "METADATA";
    default: return "(undefined)";
  }
}

struct MIDIEvent {
  uint32_t delta_time = 0;
  uint32_t absolute_time = 0;
  uint32_t length = 0;
  uint8_t status = 0;
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint8_t* metadata = nullptr;

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
      metadata = new uint8_t[data_length];
      fread(metadata, sizeof(uint8_t), data_length, fp);
      length += data_length;
      return;
    }
    switch (event_type()) {
      case PROGRAM_CHANGE:
      case CHANNEL_PRESSURE:
        data1 = fgetc(fp);
        ++length;
        break;
      case NOTE_OFF:
      case NOTE_ON:
      case POLYPHONIC_KEY_PRESSURE:
      case CONTROL_CHANGE:
      case PITCH_BEND:
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

  void Dump() const {
    printf("%s", GetEventTypeName(event_type()));
    switch (event_type()) {
      case NOTE_ON:
        printf(" channel = %d note = %d velocity = %d\n", channel(), note(), velocity());
        break;
      case NOTE_OFF:
        printf(" channel = %d note = %d\n", channel(), note());
        break;
      default:
        printf("\n");
        break;
    }
  }

  double GetAbsoluteTimeInSeconds(const MIDIHeader& header, uint32_t tempo) const {
    // division = ticks/quarter-note
    // tempo = ms/quarter-note
    // ticks / division * tempo = ms
    return 1.0 * absolute_time / header.division * tempo / 1000.0 / 1000.0;
  }

  MIDIEventType event_type() const {
    if (status == METADATA)
      return METADATA;
    return static_cast<MIDIEventType>(status & 0xF0);
  }

  MIDIMetadataType metadata_type() const {
    return static_cast<MIDIMetadataType>(data1);
  }

  int channel() const {
    return status & 0x0F;
  }

  int note() const {
    return data1 & 0x7F;
  }

  int velocity() const {
    return data2 & 0x7F;
  }

  int tempo() const {
    int tempo = 0;
    for (int i = 0; i < 3; ++i) {
      tempo <<= 8;
      tempo += metadata[i];
    }
    return tempo;
  }

  int program() const {
    return data1 & 0x7F;
  }

  const bool operator<(const MIDIEvent& rhs) const {
    return absolute_time < rhs.absolute_time;
  }
};

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("usage: %s input.mid output.wav\n", argv[0]);
    return 1;
  }
  FILE *fp = fopen(argv[1], "rb");
  MIDIHeader header;
  header.Read(fp);

  std::vector<MIDIEvent> events;
  for (int i = 0; i < header.ntrks; ++i) {
    MIDITrack track;
    track.Read(fp);
    uint32_t rem_bytes = track.length;
    uint8_t prev_status = 0;
    uint32_t current_time = 0;
    while (rem_bytes > 0) {
      MIDIEvent event;
      event.Read(fp, prev_status);
      prev_status = event.status;
      rem_bytes -= event.length;
      current_time += event.delta_time;
      event.absolute_time = current_time;
      events.push_back(event);
    }
  }
  fclose(fp);

  std::sort(events.begin(), events.end());
  const uint32_t tempo =
    std::find_if(events.begin(), events.end(),
                 [](const MIDIEvent& event) {
                   return event.event_type() == METADATA &&
                          event.metadata_type() == SET_TEMPO;
                 })->tempo();
  const double total_time =
    events.rbegin()->GetAbsoluteTimeInSeconds(header, tempo);

  std::vector<int16_t> raw(static_cast<size_t>(kSampleRate * total_time));

  auto it = events.begin();

  const double pi = acos(-1);

  std::map<int, double> volumes;
  std::map<int, int> notes;
  std::map<int, int> programs;
  double skip_until = 0.0;
  for (size_t i = 0; i < raw.size(); ++i) {
    const double t = 1.0 * i / kSampleRate;
    if (t >= skip_until && it != events.end()) {
      const double event_t = it->GetAbsoluteTimeInSeconds(header, tempo);
      skip_until = event_t;
      if (t >= event_t) {
        // The event is triggered.
        if (it->event_type() == NOTE_ON) {
          if (it->channel() != 10) {
            volumes[it->channel()] = 8192.0 * it->velocity() / 0x7F;
            notes[it->channel()] = it->note();
          }
        } else if (it->event_type() == NOTE_OFF) {
          volumes.erase(it->channel());
          notes.erase(it->channel());
        } else if (it->event_type() == PROGRAM_CHANGE) {
          programs[it->channel()] = it->program();
          printf("%lf: channel %d program changed to %d\n", t, it->channel(), it->program());
        }
        ++it;
      }
    }
    double result = 0.0;
    for (const auto& p : volumes) {
      const double carrier = MidiFreq(notes[p.first]) * 2.0 * pi;
      result += p.second * sin(carrier * t);
    }
    raw[i] = result;
  }

  fp = fopen(argv[2], "wb");
  WaveHeader wav_header(sizeof(int16_t) * raw.size());
  fwrite(&wav_header, sizeof(WaveHeader), 1, fp);
  fwrite(&raw[0], sizeof(int16_t), raw.size(), fp);
  fclose(fp);
  return 0;
}
