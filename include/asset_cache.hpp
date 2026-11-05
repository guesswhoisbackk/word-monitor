#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "cache_policy.hpp"
#include "audio_policy.hpp"
namespace wordmon {
inline String assetPathFromUrl(const String& url, bool voice) {
  char path[40];
  snprintf(path, sizeof(path), "/wb/%c_%016llx.%s", voice ? 'v' : 'a',
           static_cast<unsigned long long>(assetKey(url.c_str())), voice ? "wav" : "wmr");
  return String(path);
}
inline String assetUrl(String base, const String& name) {
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/" + name;
}
inline bool validVoiceCache(const String& path) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  uint8_t head[512];
  const size_t size = file.size(), got = file.read(head, sizeof(head));
  WavInfo info;
  return parseWavHead(head, got, size, info);
}
}
