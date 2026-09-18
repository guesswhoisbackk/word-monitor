#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wordmon {
constexpr size_t kMaxAudioBytes = 48 * 1024;
struct WavInfo { size_t offset = 0; size_t bytes = 0; uint32_t rate = 0; uint16_t bits = 0; };
inline uint16_t le16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t* p) { return le16(p) | (uint32_t(le16(p + 2)) << 16); }
inline bool parseWav(const uint8_t* data, size_t size, WavInfo& info) {
  info = {};
  if (!data || size < 44 || size > kMaxAudioBytes || memcmp(data, "RIFF", 4) ||
      memcmp(data + 8, "WAVE", 4) || le32(data + 4) != size - 8) return false;
  bool format = false;
  for (size_t pos = 12; pos + 8 <= size;) {
    const uint32_t bytes = le32(data + pos + 4);
    const uint8_t* id = data + pos;
    pos += 8;
    if (bytes > size - pos) return false;
    if (!memcmp(id, "fmt ", 4)) {
      if (bytes < 16 || le16(data + pos) != 1 || le16(data + pos + 2) != 1) return false;
      info.rate = le32(data + pos + 4);
      info.bits = le16(data + pos + 14);
      if ((info.bits != 8 && info.bits != 16) || info.rate < 8000 || info.rate > 24000 ||
          le16(data + pos + 12) != info.bits / 8 ||
          le32(data + pos + 8) != info.rate * (info.bits / 8)) return false;
      format = true;
    } else if (!memcmp(id, "data", 4)) {
      if (!format || !bytes || bytes % (info.bits / 8) || bytes / (info.bits / 8) > info.rate * 4) return false;
      info.offset = pos;
      info.bytes = bytes;
      return true;
    }
    pos += bytes + (bytes & 1U);
  }
  return false;
}
inline uint16_t dacSample(uint16_t raw, uint16_t bits, unsigned volume) {
  int32_t sample = bits == 8 ? (int32_t(raw & 255) - 128) * 256
                           : (raw >= 32768 ? int32_t(raw) - 65536 : int32_t(raw));
  sample = sample * int32_t(volume > 100 ? 100 : volume) / 100;
  return static_cast<uint16_t>((sample + 32768) & 0xff00);
}
inline bool validAudioName(const char* name) {
  if (!name) return false;
  const size_t size = strlen(name);
  if (size < 5 || size > 48 || strcmp(name + size - 4, ".wav") || strstr(name, "..")) return false;
  for (size_t i = 0; i < size; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
  }
  return true;
}
}
