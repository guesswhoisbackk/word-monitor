#pragma once

#include <Arduino.h>

namespace wordmon {

constexpr char kAppName[] = "WordMon Studio";
constexpr char kVersion[] = "0.2.0";
constexpr char kPreferencesNamespace[] = "wordmon";
constexpr char kPreferencesKey[] = "config";
constexpr uint16_t kScreenWidth = 240;
constexpr uint16_t kScreenHeight = 320;
// Rotation 2 puts the USB power connector at the bottom of the portrait UI.
constexpr uint8_t kDisplayRotation = 2;
constexpr uint8_t kTouchRotation = 2;
constexpr uint8_t kBacklightPin = 21;

// Common ESP32-2432S028R display pins.
constexpr int8_t kTftSclk = 14;
constexpr int8_t kTftMosi = 13;
constexpr int8_t kTftMiso = 12;
constexpr int8_t kTftCs = 15;
constexpr int8_t kTftDc = 2;
constexpr int8_t kTftRst = -1;

// XPT2046 touch controller uses a separate SPI bus.
constexpr int8_t kTouchSclk = 25;
constexpr int8_t kTouchMosi = 32;
constexpr int8_t kTouchMiso = 39;
constexpr int8_t kTouchCs = 33;
constexpr int8_t kTouchIrq = 36;

// Starting calibration for the common resistive panel. These values are kept
// in one place because clone boards can require different min/max values.
constexpr int16_t kTouchMinX = 200;
constexpr int16_t kTouchMaxX = 3900;
constexpr int16_t kTouchMinY = 200;
constexpr int16_t kTouchMaxY = 3900;

// Timezone for the daily word selection. KST has no daylight saving shift.
constexpr char kTimezone[] = "KST-9";
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";

// Network wordbook (route B): the device downloads words.jsonl and art PNGs
// from this base URL (configured in the web portal) and caches them in
// LittleFS so the card survives reboots without internet.
constexpr char kWordbookDir[] = "/wb";
constexpr char kWordbookManifestPath[] = "/wb/words.jsonl";
constexpr char kWordbookTmpPath[] = "/wb/words.tmp";
constexpr char kWordbookArtPrefix[] = "/wb/a_";
// Cards rotate by the KST calendar day; sync timing is in study_policy.hpp.
constexpr uint32_t kWordbookHttpTimeoutMs = 10000;
constexpr size_t kWordbookMaxManifestBytes = 256 * 1024;
constexpr size_t kWordbookMaxArtBytes = 64 * 1024;
constexpr uint16_t kWordbookMaxArtWidth = 120;
// Art taller than the 88px card slot would collide with the hint label.
constexpr uint16_t kWordbookMaxArtHeight = 88;
// RGB565 blend background for transparent PNG art: 0x00BBGGRR of kCard.
constexpr uint32_t kWordbookArtBackground = 0x002D1B0D;

}  // namespace wordmon
