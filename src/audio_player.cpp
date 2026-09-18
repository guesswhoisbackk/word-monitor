#include "audio_player.hpp"
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include <memory>
#include <new>
#include "assets/speech_audio.hpp"

namespace wordmon {
namespace {
constexpr const char* kCache = "/voice.wav";
constexpr const char* kKey = "/voice.key";
constexpr const char* kTemp = "/voice.tmp";
}
bool AudioPlayer::available(const char* word, const char* filename) {
  return (filename && *filename) ? validAudioName(filename) : speech::find(word) != nullptr;
}
bool AudioPlayer::start(const String& word, const String& filename, const String& baseUrl, uint8_t volume) {
  if (busy()) return false;
  if (!volume) { state_ = AudioState::Muted; return false; }
  if (!available(word.c_str(), filename.c_str())) { state_ = AudioState::Missing; return false; }
  word_ = word;
  url_ = "";
  if (!filename.isEmpty()) {
    if (!(baseUrl.startsWith("https://") || baseUrl.startsWith("http://")) || baseUrl.length() > 240) {
      state_ = AudioState::Invalid; return false;
    }
    url_ = baseUrl;
    while (url_.endsWith("/")) url_.remove(url_.length() - 1);
    url_ += "/" + filename;
  }
  volume_ = volume > 60 ? 60 : volume;
  cancelled_ = false;
  Serial.printf("[audio] start word=%s heap=%u largest=%u volume=%u\n", word_.c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()), volume_);
  state_ = AudioState::Loading;
  busy_ = true;
  if (xTaskCreate(task, "word-audio", 8192, this, 1, nullptr) != pdPASS) {
    state_ = AudioState::Failed;
    busy_ = false;
    return false;
  }
  return true;
}
const char* AudioPlayer::message() const {
  switch (state()) {
    case AudioState::Loading: return "LOADING AUDIO - TAP TO STOP";
    case AudioState::Playing: return "LISTEN, THEN SAY IT ALOUD";
    case AudioState::Done: return "SAY IT ALOUD - TAP TO REPLAY";
    case AudioState::Cancelled: return "AUDIO STOPPED";
    case AudioState::Missing: return "NO AUDIO FOR THIS WORD";
    case AudioState::Offline: return "AUDIO NEEDS WI-FI";
    case AudioState::Invalid: return "INVALID AUDIO FILE";
    case AudioState::Failed: return "AUDIO FAILED - TRY AGAIN";
    case AudioState::Muted: return "AUDIO MUTED IN SETTINGS";
    default: return "";
  }
}
void AudioPlayer::task(void* context) {
  auto* self = static_cast<AudioPlayer*>(context);
  self->run();  // All buffers/HTTP clients/files destroyed before busy clears.
  if (self->cancelled_) self->state_ = AudioState::Cancelled;
  Serial.printf("[audio] end state=%d heap=%u stack-free=%u\n", static_cast<int>(self->state()),
                static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  self->busy_ = false;
  vTaskDelete(nullptr);
}
void AudioPlayer::run() {
  const uint8_t* data = nullptr;
  size_t size = 0;
  std::unique_ptr<uint8_t[]> buffer;
  bool downloaded = false;
  if (url_.isEmpty()) {
    const auto* clip = speech::find(word_.c_str());
    if (!clip) { state_ = AudioState::Missing; return; }
    data = clip->data;
    size = clip->size;
  } else {
    if (!readCache(buffer, size)) {
      buffer.reset();
      if (WiFi.status() != WL_CONNECTED) { state_ = AudioState::Offline; return; }
      if (!download(buffer, size)) { state_ = AudioState::Failed; return; }
      downloaded = true;
    }
    data = buffer.get();
  }
  if (cancelled_) return;
  WavInfo info;
  if (!parseWav(data, size, info)) { state_ = AudioState::Invalid; return; }
  if (downloaded) saveCache(data, size);
  if (cancelled_) return;
  state_ = AudioState::Playing;
  state_ = playPcm(data, info) ? AudioState::Done : AudioState::Failed;
}
bool AudioPlayer::readCache(std::unique_ptr<uint8_t[]>& buffer, size_t& size) {
  File key = LittleFS.open(kKey, "r");
  if (!key || key.size() != url_.length() || key.readString() != url_) return false;
  key.close();
  File file = LittleFS.open(kCache, "r");
  if (!file || file.size() > kMaxAudioBytes) return false;
  size = file.size();
  if (!size) return false;
  buffer.reset(new (std::nothrow) uint8_t[size]);
  if (!buffer || file.read(buffer.get(), size) != size) return false;
  WavInfo info;
  return parseWav(buffer.get(), size, info);
}
void AudioPlayer::saveCache(const uint8_t* buffer, size_t size) {
  File file = LittleFS.open(kTemp, "w");
  if (!file) return;
  const bool ok = file.write(buffer, size) == size;
  file.close();
  if (!ok || cancelled_) { LittleFS.remove(kTemp); return; }
  // Clear identity first: power loss must never associate old identity with new audio.
  if (LittleFS.exists(kKey) && !LittleFS.remove(kKey)) { LittleFS.remove(kTemp); return; }
  if (!LittleFS.rename(kTemp, kCache)) { LittleFS.remove(kTemp); return; }
  File key = LittleFS.open(kKey, "w");
  if (key) {
    const bool saved = key.print(url_) == url_.length();
    key.close();
    if (!saved) LittleFS.remove(kKey);
  }
}
bool AudioPlayer::download(std::unique_ptr<uint8_t[]>& buffer, size_t& size) {
  WiFiClient plain;
  WiFiClientSecure secure;
  secure.setInsecure();  // Same public-content TLS policy as the wordbook.
  secure.setHandshakeTimeout(4);
  WiFiClient& client = url_.startsWith("https://") ? static_cast<WiFiClient&>(secure) : plain;
  HTTPClient http;
  http.useHTTP10(true);
  http.setReuse(false);
  http.setConnectTimeout(4000);
  http.setTimeout(4000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(2);
  if (!http.begin(client, url_)) return false;
  if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  const int expected = http.getSize();
  if (expected == 0 || expected > static_cast<int>(kMaxAudioBytes)) { http.end(); return false; }
  const size_t capacity = expected > 0 ? static_cast<size_t>(expected) : kMaxAudioBytes;
  buffer.reset(new (std::nothrow) uint8_t[capacity]);
  if (!buffer) { http.end(); return false; }
  auto& stream = http.getStream();
  size = 0;
  const uint32_t started = millis();
  while (!cancelled_ && millis() - started < 10000 && (http.connected() || stream.available())) {
    const size_t available = stream.available();
    if (!available) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
    if (size == capacity) break;
    size_t count = available < 512 ? available : 512;
    if (count > capacity - size) count = capacity - size;
    const int got = stream.read(buffer.get() + size, count);
    if (got <= 0) break;
    size += static_cast<size_t>(got);
    if (expected >= 0 && size >= static_cast<size_t>(expected)) break;
    vTaskDelay(1);
  }
  const bool complete = expected >= 0 ? size == static_cast<size_t>(expected) : !http.connected() && !stream.available();
  http.end();
  return !cancelled_ && size > 0 && complete;
}
bool AudioPlayer::playPcm(const uint8_t* buffer, const WavInfo& info) {
  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  config.sample_rate = info.rate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 128;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  // Never i2s_set_pin(..., nullptr): that enables BOTH DACs, stealing touch GPIO25.
  bool ok = i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) == ESP_OK;
  uint16_t samples[256]; // Stereo frames duplicate mono, only GPIO26 is enabled.
  const size_t step = info.bits / 8;
  size_t position = 0;
  while (ok && !cancelled_ && position < info.bytes) {
    size_t frames = 0;
    while (frames < 128 && position < info.bytes) {
      const uint8_t* p = buffer + info.offset + position;
      const uint16_t raw = step == 1 ? *p : le16(p);
      const size_t sampleIndex = position / step;
      const size_t remaining = (info.bytes - position) / step - 1;
      const size_t fade = info.rate / 100; // 10 ms envelope at word boundaries.
      const size_t edge = sampleIndex < remaining ? sampleIndex : remaining;
      const unsigned volume = edge < fade ? volume_ * edge / fade : volume_;
      samples[frames * 2] = samples[frames * 2 + 1] = dacSample(raw, info.bits, volume);
      position += step;
      ++frames;
    }
    size_t written = 0;
    const size_t bytes = frames * 4;
    ok = i2s_write(I2S_NUM_0, samples, bytes, &written, pdMS_TO_TICKS(100)) == ESP_OK && written == bytes;
  }
  // Queue midpoint silence through the entire DMA ring to drain the last word.
  for (auto& sample : samples) sample = 0x8000;
  for (int i = 0; ok && !cancelled_ && i < 5; ++i) {
    size_t written = 0;
    ok = i2s_write(I2S_NUM_0, samples, sizeof(samples), &written, pdMS_TO_TICKS(100)) == ESP_OK && written == sizeof(samples);
  }
  i2s_stop(I2S_NUM_0);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
  i2s_driver_uninstall(I2S_NUM_0);
  // Keep amplifier input at the PCM midpoint between words, not at full negative level.
  dac_output_enable(DAC_CHANNEL_2);
  dac_output_voltage(DAC_CHANNEL_2, 128);
  return ok;
}
}
