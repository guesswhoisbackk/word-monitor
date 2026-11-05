#include "wordbook.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_partition.h>

#include <cctype>
#include <cstring>
#include <ctime>

#include "app_config.hpp"
#include "audio_policy.hpp"
#include "asset_cache.hpp"
#include "sentence_translations.hpp"
#include "today_review.hpp"

namespace wordmon {

namespace {

// Art travels as WMR1: an 8-byte header (magic, uint16le width/height) plus
// RGB565 pixels pre-blended over the card color by scripts/build_illustrated_
// content.py. Loading is a bounded file read into the shared pixel buffer, so
// no decoder allocation is needed at runtime; the previous PNG path required
// a 46KiB contiguous heap block that fragmentation after TLS could starve.
constexpr char kArtMagic[] = {'W', 'M', 'R', '1'};
constexpr size_t kArtHeaderBytes = 8;
static_assert(6 * (kArtHeaderBytes + kWordbookMaxArtWidth * kWordbookMaxArtHeight * 2 + kMaxAudioBytes)
                  + kWordbookMaxArtBytes <= kAssetCacheBudget,
              "Six card pairs and one in-flight asset must fit the cache budget");

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
    uint8_t chunk[128];
    const size_t got = file.read(chunk, sizeof(chunk));
    if (!got) return -1;
    for (size_t i = 0; i < got; ++i) {
      const char c = static_cast<char>(chunk[i]);
      if (c == '\n') {
        line.trim();
        if (!line.isEmpty()) {
          if (i + 1 < got && !file.seek(file.position() - (got - i - 1))) return -1;
          return 1;
        }
      } else if (c != '\r') {
        if (line.length() >= 512) return -1;
        line += c;
      }
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

bool mountWordbookFilesystem() {
  if (LittleFS.begin(false)) return true;
  // Format only a completely erased partition on first installation. A failed
  // mount of an existing filesystem must never erase the review history.
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (!partition) return false;
  uint8_t bytes[256];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(bytes)) {
    const size_t length = partition->size - offset < sizeof(bytes)
        ? partition->size - offset : sizeof(bytes);
    if (esp_partition_read(partition, offset, bytes, length) != ESP_OK) return false;
    for (size_t i = 0; i < length; ++i) if (bytes[i] != 0xFF) return false;
  }
  return LittleFS.format() && LittleFS.begin(false);
}

}  // namespace

Wordbook::Wordbook(AppSettings& settings, StudyStore& study) : settings_(settings), study_(study) {}

void Wordbook::begin() {
  if (!mountWordbookFilesystem()) {
    Serial.println(F("[wb] LittleFS mount failed; wordbook cache disabled"));
    state_ = WordbookState::Failed;
    ++revision_;
    return;
  }
  // A fresh format has no /wb; downloads write into it, so create it up front.
  if (!LittleFS.exists(kWordbookDir) && !LittleFS.mkdir(kWordbookDir)) {
    Serial.println(F("[wb] could not create cache directory"));
  }
  // Reserve the art pixel buffer up front, before Wi-Fi/TLS fragment the
  // heap; loading art later must never depend on a large runtime allocation.
  if (artPixels_ == nullptr) {
    artPixels_ = static_cast<uint16_t*>(
        malloc(static_cast<size_t>(kWordbookMaxArtWidth) *
               kWordbookMaxArtHeight * 2));
    if (artPixels_ == nullptr) {
      Serial.println(F("[wb] art: pixel buffer alloc failed"));
    }
  }

  LittleFS.remove(kWordbookTmpPath);
  results_ = xQueueCreate(1, sizeof(Result));
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

void Wordbook::loop(bool allowNetwork) {
  poll();
  if (settings_.wordbookUrl.isEmpty()) return;
  const int32_t day = todayDay();
  if(todayMode_ && day >= 0 && todayDay_ != day) endToday();
  if (wordCount_ && (!cardValid_ || (day >= 0 && cardDay_ != day))) selectCard(day);
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (now >= 1700000000 && (planDay_ != day || plannedStudyRevision_ != study_.revision() || now >= planCheckAt_)) rebuildPlan();
  if (assetsDirty_) rebuildAssets();
  if (!allowNetwork || busy_ || !results_ || WiFi.status() != WL_CONNECTED) return;
  const bool manifestDue = !todayMode_ && (forceSync_ || syncDue(millis(), day));
  if (!wordCount_ && manifestDue) { startJob(1); return; }
  // No speculative pencil download while the clock is still synchronizing.
  if (now < 1700000000) { if (manifestDue) startJob(1); return; }
  // Active picture first, then manifest, then active pronunciation/upcoming assets.
  for (uint8_t i=0;i<assetCount_;++i) {
    auto& asset=assets_[i];
    if (asset.ready || !retries_.ready(assetKey(asset.path), millis())) continue;
    if (manifestDue && (asset.voice || String(asset.path) != cardArtPath_)) { startJob(1); return; }
    pruneCache();
    const size_t maxBytes=asset.voice?kMaxAudioBytes:kWordbookMaxArtBytes;
    if (LittleFS.totalBytes()-LittleFS.usedBytes() < kStudyStorageReserveBytes+maxBytes) {
      retries_.failed(assetKey(asset.path),millis()); continue;
    }
    startJob(asset.voice?3:2,asset.name,asset.path); return;
  }
  if (manifestDue) startJob(1);
}

bool Wordbook::syncDue(uint32_t now, int32_t day) {
  (void)day;
  return !busy_ && syncAllowed(now,everAttempted_,everSynced_,lastAttemptMs_,lastSyncOkMs_);
}
void Wordbook::attemptSync(int32_t day) { (void)day; forceSync_=true; }

void Wordbook::startJob(uint8_t type,const String& name,const String& path) {
  job_.type=type;
  job_.url=type==1?assetUrl(settings_.wordbookUrl,"words.jsonl"):assetUrl(settings_.wordbookUrl,name);
  job_.path=type==1?String(kWordbookManifestPath):path;
  if(type==1) { state_=WordbookState::Syncing; ++revision_; everAttempted_=true; lastAttemptMs_=millis(); forceSync_=false; }
  busy_=true;
  Serial.printf("[cache] job=%u heap=%u largest=%u\n",type,ESP.getFreeHeap(),ESP.getMaxAllocHeap());
  if(xTaskCreate(downloadTask,"wordbook-fetch",8192,this,1,nullptr)!=pdPASS) {
    busy_=false;
    if(type==1) state_=wordCount_?WordbookState::Cached:WordbookState::Failed;
    else retries_.failed(assetKey(path.c_str()),millis());
  }
}
void Wordbook::downloadTask(void* context) {
  auto* self=static_cast<Wordbook*>(context);
  Result result;
  const uint8_t type=self->job_.type;
  result.ok=self->fetchToFile(self->job_.url,self->job_.path,
      type==1?kWordbookMaxManifestBytes:type==2?kWordbookMaxArtBytes:kMaxAudioBytes,
      type==1?&result.count:nullptr);
  if(result.ok && type!=1) result.ok=self->validAsset(kWordbookTmpPath,type==3);
  // fetchToFile returned, so all HTTP/String/file owners have been destroyed.
  xQueueOverwrite(self->results_,&result);
  vTaskDelete(nullptr);
}
void Wordbook::poll() {
  Result result;
  if(!results_ || xQueueReceive(results_,&result,0)!=pdTRUE) return;
  busy_=false;
  const uint8_t type=job_.type;
  const bool ok=result.ok && LittleFS.rename(kWordbookTmpPath,job_.path);
  if(!ok) LittleFS.remove(kWordbookTmpPath);
  if(type==1) {
    if(ok) {
      wordCount_=result.count; everSynced_=true; lastSyncOkMs_=millis(); state_=WordbookState::Ok;
      Serial.printf("[wb] manifest synced: %u words\n",wordCount_);
      plannedStudyRevision_=UINT32_MAX;
      int32_t active=-1;
      if(cardValid_) {
        File file=LittleFS.open(kWordbookManifestPath,"r");String line;uint16_t index=0;
        while(file && readEntry(file,line)==1 && index<wordCount_) {
          JsonDocument doc;if(!deserializeJson(doc,line) && cardWord_==(doc["w"]|"")) {active=index;break;} ++index;
        }
      }
      selectCard(todayDay(),active);
    } else state_=wordCount_?WordbookState::Cached:WordbookState::Failed;
    ++revision_; assetsDirty_=true;
  } else {
    const uint64_t key=assetKey(job_.path.c_str());
    if(ok) retries_.succeeded(key); else retries_.failed(key,millis());
    for(uint8_t i=0;i<assetCount_;++i) if(job_.path==assets_[i].path) assets_[i].ready=ok;
    if(ok && job_.path==cardArtPath_ && loadArt(cardArtPath_)) ++revision_;
    Serial.printf("[cache] %s %s ready=%u/%u\n",type==2?"picture":"voice",ok?"saved":"retry scheduled",cachedAssets(),assetCount_);
  }
}
void Wordbook::rebuildPlan() {
  const uint32_t now=static_cast<uint32_t>(time(nullptr));
  if(now<1700000000 || !wordCount_) return;
  StudyPlan plan;
  File file=LittleFS.open(kWordbookManifestPath,"r"); String line; uint16_t index=0;
  while(file && readEntry(file,line)==1 && index<wordCount_) {
    JsonDocument doc;
    if(!deserializeJson(doc,line)) plan.consider(index,study_.find(doc["w"]|""),now);
    ++index;
  }
  const uint16_t previousDue=plan_.dueCount;
  plan_=plan; planDay_=todayDay(); plannedStudyRevision_=study_.revision();
  planCheckAt_=plan_.nextDue;
  assetsDirty_=true;
  if(previousDue!=plan_.dueCount) ++revision_;
}
void Wordbook::appendAsset(const String& name,bool voice) {
  if(assetCount_>=12 || (voice?!validAudioName(name.c_str()):!validArtName(name))) return;
  const String path=assetPathFromUrl(assetUrl(settings_.wordbookUrl,name),voice);
  for(uint8_t i=0;i<assetCount_;++i) if(path==assets_[i].path) return;
  auto& asset=assets_[assetCount_++];
  snprintf(asset.name,sizeof(asset.name),"%s",name.c_str());
  snprintf(asset.path,sizeof(asset.path),"%s",path.c_str());
  asset.voice=voice; asset.ready=validAsset(path,voice);
  if(!asset.ready && LittleFS.exists(path)) LittleFS.remove(path);
}
void Wordbook::rebuildAssets() {
  assetCount_=0;
  appendAsset(cardArtName_,false); appendAsset(cardAudio_,true);
  File file=LittleFS.open(kWordbookManifestPath,"r");String line;uint16_t index=0;
  while(file && readEntry(file,line)==1 && index<wordCount_) {
    bool wanted=false;for(size_t i=0;i<5;++i) if((todayMode_ ? todayIndices_[(todayPosition_+i+1)%todayCount_] : plan_.prefetch(i))==index) wanted=true;
    if(wanted) { JsonDocument doc; if(!deserializeJson(doc,line)) {appendAsset(doc["a"]|"",false);appendAsset(doc["s"]|"",true);} }
    ++index;
  }
  assetsDirty_=false;
  if(!busy_) pruneCache();
}
uint8_t Wordbook::cachedAssets() const {
  uint8_t count=0;for(uint8_t i=0;i<assetCount_;++i) if(assets_[i].ready) ++count;
  return count;
}
bool Wordbook::validAsset(const String& path,bool voice) {
  if(!LittleFS.exists(path)) return false;
  if(voice) return validVoiceCache(path);
  File file=LittleFS.open(path,"r"); uint8_t header[8];
  if(!file || file.read(header,8)!=8 || memcmp(header,kArtMagic,4)) return false;
  const uint16_t w=le16(header+4),h=le16(header+6);
  return w && h && w<=kWordbookMaxArtWidth && h<=kWordbookMaxArtHeight && file.size()==8+size_t(w)*h*2;
}
void Wordbook::pruneCache() {
  // Preserve the previous boot's prefetched cards until NTP establishes the
  // real study plan; an uninitialized clock temporarily selects card zero.
  if (planDay_ < 0) return;
  File root=LittleFS.open(kWordbookDir); File entry=root.openNextFile();
  while(entry) {
    String name=entry.name(); const int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
    const String path=String(kWordbookDir)+"/"+name;
    bool keep=false;for(uint8_t i=0;i<assetCount_;++i) if(path==assets_[i].path) keep=true;
    entry.close();
    if((name.startsWith("a_") || name.startsWith("v_")) && !keep) LittleFS.remove(path);
    entry=root.openNextFile();
  }
}

void Wordbook::selectCard(int32_t yday, int32_t requestedIndex) {
  if (wordCount_ == 0) {
    return;
  }
  if (requestedIndex < 0 && yday >= 0) {
    if (nextReview(study_)) return;
  }
  const uint16_t index =
      static_cast<uint16_t>(requestedIndex >= 0 ? requestedIndex : 0);

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
  cardTranslation_ = doc["t"] | sentenceTranslation(cardExample_.c_str());
  activeIndex_ = index;
  cardAudio_ = doc["s"] | "";
  card_.word = cardWord_.c_str();
  card_.meaning = cardMeaning_.c_str();
  card_.example = cardExample_.c_str();
  card_.translation = cardTranslation_.c_str();
  card_.audio = cardAudio_.c_str();
  card_.art = nullptr;

  cardDay_ = yday;
  cardValid_ = !cardWord_.isEmpty();

  cardArtName_ = doc["a"] | "";
  cardArtPath_ = validArtName(cardArtName_) ? assetPathFromUrl(assetUrl(settings_.wordbookUrl,cardArtName_),false) : String();
  if(!cardArtPath_.isEmpty() && validAsset(cardArtPath_,false)) loadArt(cardArtPath_);
  assetsDirty_=true;

  Serial.printf("[wb] card #%u: %s%s\n", static_cast<unsigned>(index),
                cardWord_.c_str(), card_.art != nullptr ? " (art)" : "");
  ++revision_;
}

bool Wordbook::beginToday() {
  const uint32_t now=static_cast<uint32_t>(time(nullptr));
  if(todayMode_ || studyDay(now)<0 || !wordCount_) return false;
  todayCount_=0;
  File file=LittleFS.open(kWordbookManifestPath,"r"); String line; uint16_t index=0;
  while(file && readEntry(file,line)==1 && index<wordCount_) {
    JsonDocument doc;
    if(!deserializeJson(doc,line)) {
      const auto* record=study_.find(doc["w"]|"");
      if(record && reviewedToday(*record,now)) todayIndices_[todayCount_++]=index;
    }
    ++index;
  }
  if(!todayCount_) return false;
  savedIndex_=activeIndex_; todayPosition_=0; todayDay_=studyDay(now); todayMode_=true;
  selectCard(todayDay_,todayIndices_[0]); return true;
}
void Wordbook::endToday() {
  if(!todayMode_) return;
  todayMode_=false;
  selectCard(todayDay(),savedIndex_<wordCount_?savedIndex_:0);
}
void Wordbook::stepToday(int direction) {
  if(!todayMode_ || !todayCount_) return;
  todayPosition_=static_cast<uint16_t>((int(todayPosition_)+int(todayCount_)+(direction<0?-1:1))%todayCount_);
  selectCard(todayDay_,todayIndices_[todayPosition_]);
}

bool Wordbook::nextReview(const StudyStore& study) {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (!wordCount_ || now < 1700000000 || settings_.wordbookUrl.isEmpty()) return false;
  rebuildPlan();
  const int32_t day=todayDay();
  const int32_t best=plan_.best(study.allowsNew(now,settings_.dailyNewLimit));
  if(best<0) return false;
  selectCard(day, best);
  return cardValid_;
}

bool Wordbook::fetchToFile(const String& url, const String& path,
                           size_t maxBytes, uint16_t* lineCount) {
  if (lineCount) {
    const size_t total = LittleFS.totalBytes();
    const size_t budget = total > kStudyStorageReserveBytes ? total - kStudyStorageReserveBytes : 0;
    if (maxBytes > budget) maxBytes = budget;
  }
  // NAS/home-server hosts often serve plain http:// on the LAN; pick the
  // transport by URL scheme. https:// skips cert validation: the wordbook is
  // read-only content and this keeps the CA bundle off the 4MB flash (and
  // lets self-signed NAS certificates work).
  const bool secure = url.startsWith(F("https://"));
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  if (secure) {
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(4);
  }
  WiFiClient& transport = secure ? static_cast<WiFiClient&>(secureClient)
                                 : static_cast<WiFiClient&>(plainClient);
  HTTPClient http;
  http.useHTTP10(true);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(kWordbookHttpTimeoutMs);
  http.setConnectTimeout(4000);
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
  // Main-loop poll() atomically publishes the validated temporary file.
  (void)path;
  if (lineCount != nullptr) {
    *lineCount = lines;
  }
  return true;
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

bool Wordbook::loadArt(const String& path) {
  if (artPixels_ == nullptr) {
    Serial.println(F("[wb] art: no pixel buffer"));
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println(F("[wb] art: cache open failed"));
    return false;
  }
  const size_t size = file.size();
  if (size <= kArtHeaderBytes || size > kWordbookMaxArtBytes) {
    Serial.printf("[wb] art: bad file size %u\n",
                  static_cast<unsigned>(size));
    file.close();
    return false;
  }
  uint8_t header[kArtHeaderBytes];
  if (file.read(header, sizeof(header)) != sizeof(header)) {
    file.close();
    return false;
  }
  const uint16_t width = static_cast<uint16_t>(header[4]) |
                         (static_cast<uint16_t>(header[5]) << 8);
  const uint16_t height = static_cast<uint16_t>(header[6]) |
                          (static_cast<uint16_t>(header[7]) << 8);
  if (memcmp(header, kArtMagic, sizeof(kArtMagic)) != 0 ||
      width == 0 || height == 0 || width > kWordbookMaxArtWidth ||
      height > kWordbookMaxArtHeight ||
      size != kArtHeaderBytes + static_cast<size_t>(width) * height * 2) {
    Serial.printf("[wb] art: bad WMR1 header %ux%u for %u bytes\n", width,
                  height, static_cast<unsigned>(size));
    file.close();
    return false;
  }
  const size_t pixels = static_cast<size_t>(width) * height * 2;
  if (file.read(reinterpret_cast<uint8_t*>(artPixels_), pixels) != pixels) {
    Serial.println(F("[wb] art: short pixel read"));
    file.close();
    return false;
  }
  file.close();

  artDsc_ = lv_image_dsc_t{};
  artDsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  artDsc_.header.cf = LV_COLOR_FORMAT_RGB565;
  artDsc_.header.w = width;
  artDsc_.header.h = height;
  artDsc_.data_size = pixels;
  artDsc_.data = reinterpret_cast<const uint8_t*>(artPixels_);
  card_.art = &artDsc_;
  return true;
}

}  // namespace wordmon
