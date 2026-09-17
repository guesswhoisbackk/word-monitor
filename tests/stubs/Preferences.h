#pragma once
#include <cstring>
#include <vector>
struct Preferences {
  static std::vector<unsigned char> bytes;
  static bool failWrite;
  bool begin(const char*, bool) { return true; }
  void end() {}
  size_t getBytesLength(const char*) { return bytes.size(); }
  size_t getBytes(const char*, void* out, size_t length) {
    if (length != bytes.size()) return 0;
    memcpy(out, bytes.data(), length);
    return length;
  }
  size_t putBytes(const char*, const void* data, size_t length) {
    if (failWrite) return 0;
    const auto* start = static_cast<const unsigned char*>(data);
    bytes.assign(start, start + length);
    return length;
  }
};
