#include "wordbook.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <PNGdec.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cctype>
#include <ctime>
#include <new>

#include "app_config.hpp"

namespace wordmon {

namespace {

// PNGdec is callback-driven; the library instance and the decode target are
// file-scope because the per-line callback has no user-data parameter. The
// PNG object embeds a 32KB zlib window (~46KB total), which does not fit the
// ESP32's static DRAM budget next to Wi-Fi + LVGL, so it lives on the heap,
// allocated once on first decode. The pixel buffer is likewise allocated
// once at the maximum art size and reused to avoid daily heap churn.
PNG* s_png = nullptr;
uint16_t* s_pngDest = nullptr;

int32_t pngLineDraw(PNGDRAW* draw) {
  if (s_png != nullptr && s_pngDest != nullptr) {
    s_png->getLineAsRGB565(
        draw, s_pngDest + static_cast<size_t>(draw->y) * draw->iWidth,
        PNG_RGB565_LITTLE_ENDIAN, kWordbookArtBackground);
  }
  // PNGdec stops decoding when the draw callback returns 0.
  return 1;
}

// PNGdec's bundled zlib defines a `local` macro; keep plain names here.
int32_t todayDay() {
  return studyDay(static_cast<uint32_t>(time(nullptr)));
}

bool validArtName(const String& name) {
  if (name.isEmpty() || name.length() > 40) {
    return false;
  }
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name[i];
    const bool ok = isalnum(static_cast<unsigned char>(c)) || c == '_' ||
                    c == '-' || c == '.';
    if (!ok) {
      return false;
    }
  }
  return name.indexOf(F("..")) < 0;
}

// Bound RAM use and give blank lines identical semantics in all readers.
int readEntry(File& file, String& line) {
  line = "";
  while (file.available()) {
    const int c = file.read();
    if (c == '\n') {
      line.trim();
      if (!line.isEmpty()) return 1;
      continue;
    }
    if (c != '\r') {
      if (line.length() >= 512) return -1;
      line += static_cast<char>(c);
    }
  }
  line.trim();
  return line.isEmpty() ? 0 : 1;
}

bool validateManifest(const String& path, uint16_t& count) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  count = 0;
  String line;
  int result;
  while ((result = readEntry(file, line)) == 1) {
    JsonDocument doc;
    if (deserializeJson(doc, line) || !doc["w"].is<const char*>() ||
        !doc["m"].is<const char*>() || !strlen(doc["w"].as<const char*>()) ||
        (!doc["e"].isNull() && !doc["e"].is<const char*>()) ||
        ++count > 1000) return false;
  }
  return result == 0 && count > 0;
}

}  // namespace

Wordbook::Wordbook(AppSettings& settings) : settings_(settings) {}

void Wordbook::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println(F("[wb] LittleFS mount failed; wordbook cache disabled"));
  }
  // A fresh format has no /wb; downloads write into it, so create it up front.
  if (!LittleFS.exists(kWordbookDir) && !LittleFS.mkdir(kWordbookDir)) {
    Serial.println(F("[wb] could not create cache directory"));
  }

  uint16_t lines = 0;
  if (!validateManifest(kWordbookManifestPath, lines)) lines = 0;
  wordCount_ = lines;

  if (settings_.wordbookUrl.isEmpty()) {
    state_ = WordbookState::Disabled;
  } else if (wordCount_ > 0) {
    state_ = WordbookState::Cached;
  } else {
    state_ = WordbookState::Failed;
  }
  ++revision_;
  Serial.printf("[wb] cached manifest: %u words, state %d\n",
                static_cast<unsigned>(wordCount_),
                static_cast<int>(state_));
}

void Wordbook::loop() {
  if (settings_.wordbookUrl.isEmpty()) {
    if (state_ != WordbookState::Disabled) {
      state_ = WordbookState::Disabled;
      cardValid_ = false;
      ++revision_;
    }
    return;
  }
  const uint32_t now = millis();
  const int32_t yday = todayDay();

  // Pick the card for a new day (or the first card) from the cached manifest
  // even when a sync is not due yet.
  if (wordCount_ > 0 && (!cardValid_ || (yday >= 0 && cardDay_ != yday))) {
    selectCard(yday);  // Cached text also works before Wi-Fi/NTP is ready.
  }

  if (WiFi.status() != WL_CONNECTED) return;
  if (syncDue(now, yday)) {
    attemptSync(yday);
  }
}

bool Wordbook::syncDue(uint32_t now, int32_t yday) {
  (void)yday;
  if (state_ == WordbookState::Syncing) {
    return false;
  }
  return syncAllowed(now, everAttempted_, everSynced_, lastAttemptMs_, lastSyncOkMs_);
}

void Wordbook::attemptSync(int32_t yday) {
  state_ = WordbookState::Syncing;
  ++revision_;
  lastAttemptMs_ = millis();
  everAttempted_ = true;

  uint16_t lines = 0;
  const bool ok = fetchToFile(settings_.wordbookUrl + F("/words.jsonl"),
                              kWordbookManifestPath, kWordbookMaxManifestBytes,
                              &lines);
  if (!ok) {
    Serial.println(F("[wb] manifest download failed"));
    state_ = wordCount_ > 0 ? WordbookState::Cached : WordbookState::Failed;
    ++revision_;
    return;
  }

  wordCount_ = lines;
  lastSyncOkMs_ = millis();
  everSynced_ = true;
  state_ = WordbookState::Ok;
  Serial.printf("[wb] manifest synced: %u words\n",
                static_cast<unsigned>(wordCount_));
  // Refresh content/art even on the same day, preserving the active review
  // word across manifest reordering. If it was removed, use today's slot.
  int32_t activeIndex = -1;
  if (cardValid_ && cardDay_ == yday) {
    File file = LittleFS.open(kWordbookManifestPath, "r");
    String line;
    for (uint16_t index = 0; file && index < wordCount_; ++index) {
      if (readEntry(file, line) != 1) break;
      JsonDocument doc;
      if (!deserializeJson(doc, line) && cardWord_ == (doc["w"] | "")) {
        activeIndex = index;
        break;
      }
    }
  }
  selectCard(yday, activeIndex);
  ++revision_;
}

void Wordbook::selectCard(int32_t yday, int32_t requestedIndex) {
  if (wordCount_ == 0) {
    return;
  }
  const uint16_t index =
      static_cast<uint16_t>(requestedIndex >= 0 ? requestedIndex :
                            (yday >= 0 ? yday % wordCount_ : 0));

  String line;
  if (!readManifestLine(index, line)) {
    Serial.printf("[wb] could not read card line %u\n",
                  static_cast<unsigned>(index));
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, line)) {
    Serial.println(F("[wb] card line is not valid JSON"));
    return;
  }

  cardWord_ = doc["w"] | "";
  cardMeaning_ = doc["m"] | "";
  cardExample_ = doc["e"] | "";
  card_.word = cardWord_.c_str();
  card_.meaning = cardMeaning_.c_str();
  card_.example = cardExample_.c_str();
  card_.art = nullptr;

  cardDay_ = yday;
  cardValid_ = !cardWord_.isEmpty();

  String artName = doc["a"] | "";
  if (cardValid_ && validArtName(artName)) {
    const String cachePath = String(kWordbookArtPrefix) + artName;
    if (!LittleFS.exists(cachePath) && WiFi.status() == WL_CONNECTED) {
      if (!fetchToFile(settings_.wordbookUrl + F("/") + artName, cachePath,
                       kWordbookMaxArtBytes, nullptr)) {
        Serial.printf("[wb] art download failed: %s\n", artName.c_str());
      }
    }
    if (LittleFS.exists(cachePath)) {
      if (!decodeArt(cachePath)) {
        Serial.printf("[wb] art decode failed: %s\n", artName.c_str());
      } else {
        evictOtherArt(artName);
      }
    }
  }

  Serial.printf("[wb] card #%u: %s%s\n", static_cast<unsigned>(index),
                cardWord_.c_str(), card_.art != nullptr ? " (art)" : "");
  ++revision_;
}

bool Wordbook::nextReview(const StudyStore& study) {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (!cardValid_ || now < 1700000000 || settings_.wordbookUrl.isEmpty()) return false;
  File file = LittleFS.open(kWordbookManifestPath, "r");
  if (!file) return false;
  ReviewChoice choice;
  uint16_t index = 0;
  const int32_t day = todayDay();
  const int32_t today = day >= 0 ? day % wordCount_ : -1;
  while (file.available() && index < wordCount_) {
    String line;
    if (readEntry(file, line) != 1) break;
    JsonDocument doc;
    if (!deserializeJson(doc, line)) {
      const char* word = doc["w"] | "";
      const ReviewRecord* record = study.find(word);
      if (*word) choice.consider(index, today, record, now);
    }
    ++index;
  }
  file.close();
  const int32_t best = choice.index();
  if (best < 0) return false;
  selectCard(day, best);
  return cardValid_;
}

bool Wordbook::fetchToFile(const String& url, const String& path,
                           size_t maxBytes, uint16_t* lineCount) {
  // NAS/home-server hosts often serve plain http:// on the LAN; pick the
  // transport by URL scheme. https:// skips cert validation: the wordbook is
  // read-only content and this keeps the CA bundle off the 4MB flash (and
  // lets self-signed NAS certificates work).
  const bool secure = url.startsWith(F("https://"));
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  if (secure) {
    secureClient.setInsecure();
  }
  WiFiClient& transport = secure ? static_cast<WiFiClient&>(secureClient)
                                 : static_cast<WiFiClient&>(plainClient);
  HTTPClient http;
  http.useHTTP10(true);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(kWordbookHttpTimeoutMs);
  if (!http.begin(transport, url)) {
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[wb] GET %s -> %d\n", url.c_str(), code);
    http.end();
    return false;
  }
  const int expected = http.getSize();
  // HTTP/1.0 requests avoid chunk framing in the raw response stream.
  if (expected > static_cast<int>(maxBytes)) { http.end(); return false; }

  WiFiClient& stream = http.getStream();
  File out = LittleFS.open(kWordbookTmpPath, "w");
  if (!out) {
    http.end();
    return false;
  }

  size_t total = 0;
  uint16_t lines = 0;
  bool writeOk = true;
  uint8_t chunk[256];
  const uint32_t deadline = millis() + kWordbookHttpTimeoutMs;
  while ((http.connected() || stream.available()) &&
         static_cast<int32_t>(deadline - millis()) > 0) {
    const size_t available = stream.available();
    if (available == 0) {
      delay(2);
      continue;
    }
    const size_t want = available < sizeof(chunk) ? available : sizeof(chunk);
    const size_t got = stream.readBytes(chunk, want);
    if (got == 0) {
      break;
    }
    if (out.write(chunk, got) != got) { writeOk = false; break; }
    total += got;
    if (total > maxBytes) {
      break;
    }
    if (expected >= 0 && total >= static_cast<size_t>(expected)) break;
  }
  const bool complete = expected >= 0 ? total == static_cast<size_t>(expected)
                                     : !http.connected() && !stream.available();
  out.close();
  http.end();

  if (!writeOk || !complete || total == 0 || total > maxBytes) {
    LittleFS.remove(kWordbookTmpPath);
    return false;
  }
  if (lineCount && !validateManifest(kWordbookTmpPath, lines)) {
    LittleFS.remove(kWordbookTmpPath);
    return false;
  }

  // LittleFS rename replaces atomically; retain the old cache on failure.
  if (!LittleFS.rename(kWordbookTmpPath, path)) {
    LittleFS.remove(kWordbookTmpPath);
    return false;
  }
  if (lineCount != nullptr) {
    *lineCount = lines;
  }
  return true;
}

// The 128KB LittleFS partition cannot hold a year of downloaded art; keep
// only the current card's file and delete other cached art.
void Wordbook::evictOtherArt(const String& keepName) {
  File root = LittleFS.open(kWordbookDir);
  if (!root || !root.isDirectory()) {
    return;
  }
  const String keep = String("a_") + keepName;
  File entry = root.openNextFile();
  while (entry) {
    String name = entry.name();
    entry.close();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) {
      name = name.substring(slash + 1);
    }
    if (name.startsWith("a_") && name != keep) {
      LittleFS.remove(String(kWordbookDir) + F("/") + name);
    }
    entry = root.openNextFile();
  }
}

bool Wordbook::readManifestLine(uint16_t index, String& out) {
  File file = LittleFS.open(kWordbookManifestPath, "r");
  if (!file) {
    return false;
  }

  for (uint16_t current = 0; current <= index; ++current) {
    if (readEntry(file, out) != 1) return false;
  }
  return true;
}

bool Wordbook::decodeArt(const String& path) {
  if (s_png == nullptr) {
    s_png = new (std::nothrow) PNG();
    if (s_png == nullptr) {
      Serial.printf("[wb] art: PNG alloc failed (heap %u)\n",
                    static_cast<unsigned>(ESP.getFreeHeap()));
      return false;
    }
  }
  // One buffer at the maximum art size, reused for every card.
  if (artPixels_ == nullptr) {
    artPixels_ = static_cast<uint16_t*>(
        malloc(static_cast<size_t>(kWordbookMaxArtWidth) *
               kWordbookMaxArtHeight * 2));
    if (artPixels_ == nullptr) {
      Serial.println(F("[wb] art: pixel buffer alloc failed"));
      return false;
    }
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println(F("[wb] art: cache open failed"));
    return false;
  }
  const size_t size = file.size();
  if (size == 0 || size > kWordbookMaxArtBytes) {
    Serial.printf("[wb] art: bad file size %u\n",
                  static_cast<unsigned>(size));
    file.close();
    return false;
  }

  // PNGdec needs the whole file in RAM; art files are a few KB.
  uint8_t* buffer = static_cast<uint8_t*>(malloc(size));
  if (buffer == nullptr) {
    file.close();
    return false;
  }
  const size_t read = file.read(buffer, size);
  file.close();
  if (read != size) {
    free(buffer);
    return false;
  }

  if (s_png->openRAM(buffer, size, pngLineDraw) != PNG_SUCCESS) {
    Serial.printf("[wb] art: openRAM failed (magic %02x%02x)\n",
                  buffer[0], size > 1 ? buffer[1] : 0);
    free(buffer);
    return false;
  }
  const int width = s_png->getWidth();
  const int height = s_png->getHeight();
  if (width <= 0 || height <= 0 || width > kWordbookMaxArtWidth ||
      height > kWordbookMaxArtHeight) {
    Serial.printf("[wb] art: %dx%d exceeds %ux%u\n", width, height,
                  static_cast<unsigned>(kWordbookMaxArtWidth),
                  static_cast<unsigned>(kWordbookMaxArtHeight));
    s_png->close();
    free(buffer);
    return false;
  }

  s_pngDest = artPixels_;
  const int result = s_png->decode(nullptr, 0);
  s_pngDest = nullptr;
  s_png->close();
  free(buffer);
  if (result != PNG_SUCCESS) {
    Serial.printf("[wb] art: decode rc=%d (heap %u)\n", result,
                  static_cast<unsigned>(ESP.getFreeHeap()));
    return false;
  }

  artDsc_ = lv_image_dsc_t{};
  artDsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  artDsc_.header.cf = LV_COLOR_FORMAT_RGB565;
  artDsc_.header.w = width;
  artDsc_.header.h = height;
  artDsc_.data_size = static_cast<size_t>(width) * height * 2;
  artDsc_.data = reinterpret_cast<const uint8_t*>(artPixels_);
  card_.art = &artDsc_;
  return true;
}

}  // namespace wordmon
