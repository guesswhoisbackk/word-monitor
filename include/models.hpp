#pragma once

#include <Arduino.h>

namespace wordmon {

struct AppSettings {
  String wifiSsid;
  String wifiPassword;
  uint8_t brightness = 220;
  uint8_t audioVolume = 20;  // 0=mute, conservative maximum 60 for the 1W speaker.
  // Base URL of the hosted wordbook (words.jsonl + art PNGs). Empty disables
  // network wordbook mode and the firmware falls back to the built-in pack.
  String wordbookUrl;
};

}  // namespace wordmon
