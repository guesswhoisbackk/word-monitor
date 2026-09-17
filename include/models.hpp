#pragma once

#include <Arduino.h>

namespace wordmon {

struct AppSettings {
  String wifiSsid;
  String wifiPassword;
  uint8_t brightness = 220;
  // Base URL of the hosted wordbook (words.jsonl + art PNGs). Empty disables
  // network wordbook mode and the firmware falls back to the built-in pack.
  String wordbookUrl;
};

}  // namespace wordmon
