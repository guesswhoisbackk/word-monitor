#include "audio_player.hpp"
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include "assets/speech_audio.hpp"

namespace wordmon {
namespace {
constexpr const char* kCache = "/voice.wav";
constexpr const char* kKey = "/voice.key";
constexpr const char* kTemp = "/voice.tmp";
// Leading bytes parsed for the streaming WAV header; canonical PCM files
// (and any reasonable metadata) fit well inside this.
constexpr size_t kHeadBytes = 512;
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
  if (url_.isEmpty()) {
    const auto* clip = speech::find(word_.c_str());
    if (!clip) { state_ = AudioState::Missing; return; }
    if (cancelled_) return;
    WavInfo info;
    if (!parseWav(clip->data, clip->size, info)) { state_ = AudioState::Invalid; return; }
    state_ = AudioState::Playing;
    state_ = playPcm(clip->data, info) ? AudioState::Done : AudioState::Failed;
    return;
  }
  File file;
  WavInfo info;
  if (!openCached(file, info)) {
    if (WiFi.status() != WL_CONNECTED) { state_ = AudioState::Offline; return; }
    const bool fresh = download();
    if (cancelled_) { if (fresh) LittleFS.remove(kTemp); return; }
    if (!fresh) { state_ = AudioState::Failed; return; }
    if (!openVoice(kTemp, file, info)) { LittleFS.remove(kTemp); state_ = AudioState::Invalid; return; }
    finalizeDownload();
  }
  if (cancelled_) return;
  state_ = AudioState::Playing;
  state_ = playPcmFile(file, info) ? AudioState::Done : AudioState::Failed;
}
bool AudioPlayer::openCached(File& file, WavInfo& info) {
  File key = LittleFS.open(kKey, "r");
  if (!key || key.size() != url_.length() || key.readString() != url_) return false;
  key.close();
  return openVoice(kCache, file, info);
}
bool AudioPlayer::openVoice(const char* path, File& file, WavInfo& info) {
  file = LittleFS.open(path, "r");
  if (!file) return false;
  const size_t size = file.size();
  if (size < 44 || size > kMaxAudioBytes) { file.close(); return false; }
  uint8_t head[kHeadBytes];
  const size_t got = file.read(head, kHeadBytes);
  if (!parseWavHead(head, got, size, info)) { file.close(); return false; }
  return true;
}
bool AudioPlayer::download() {
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
  File out = LittleFS.open(kTemp, "w");
  if (!out) { http.end(); return false; }
  auto& stream = http.getStream();
  size_t size = 0;
  const uint32_t started = millis();
  uint8_t chunk[512];
  bool writeOk = true;
  while (!cancelled_ && millis() - started < 10000 && (http.connected() || stream.available())) {
    const size_t available = stream.available();
    if (!available) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
    if (size == static_cast<size_t>(expected)) break;
    size_t count = available < sizeof(chunk) ? available : sizeof(chunk);
    if (count > static_cast<size_t>(expected) - size) count = static_cast<size_t>(expected) - size;
    const int got = stream.read(chunk, count);
    if (got <= 0) break;
    if (out.write(chunk, got) != static_cast<size_t>(got)) { writeOk = false; break; }
    size += static_cast<size_t>(got);
    vTaskDelay(1);
  }
  out.close();
  http.end();
  const bool complete = size == static_cast<size_t>(expected);
  if (!writeOk || cancelled_ || size == 0 || !complete) { LittleFS.remove(kTemp); return false; }
  return true;
}
void AudioPlayer::finalizeDownload() {
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
bool AudioPlayer::playPcmFile(File& file, const WavInfo& info) {
  if (!file.seek(info.offset, SeekSet)) return false;
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
  uint8_t chunk[512];
  uint16_t samples[256];  // Stereo frames duplicate mono, only GPIO26 is enabled.
  const size_t step = info.bits / 8;
  size_t position = 0;
  while (ok && !cancelled_ && position < info.bytes) {
    size_t want = sizeof(chunk);
    if (want > info.bytes - position) want = info.bytes - position;
    size_t got = 0;
    while (got < want) {
      const int read = file.read(chunk + got, want - got);
      if (read <= 0) break;
      got += static_cast<size_t>(read);
    }
    const size_t usable = got - (got % step);  // never split a sample frame
    if (usable == 0) break;
    for (size_t base = 0; ok && base < usable; base += 128 * step) {
      size_t frames = 0;
      while (frames < 128 && base + frames * step < usable) {
        const uint8_t* p = chunk + base + frames * step;
        const uint16_t raw = step == 1 ? *p : le16(p);
        const size_t sampleIndex = (position + base + frames * step) / step;
        const size_t remaining = (info.bytes - position - base - frames * step) / step - 1;
        const size_t fade = info.rate / 100;  // 10 ms envelope at word boundaries.
        const size_t edge = sampleIndex < remaining ? sampleIndex : remaining;
        const unsigned volume = edge < fade ? volume_ * edge / fade : volume_;
        samples[frames * 2] = samples[frames * 2 + 1] = dacSample(raw, info.bits, volume);
        ++frames;
      }
      size_t written = 0;
      const size_t bytes = frames * 4;
      ok = i2s_write(I2S_NUM_0, samples, bytes, &written, pdMS_TO_TICKS(100)) == ESP_OK &&
           written == bytes;
    }
    position += usable;
    if (got < want) break;  // EOF inside the data chunk
  }
  // Queue midpoint silence through the entire DMA ring to drain the last word.
  for (auto& sample : samples) sample = 0x8000;
  for (int i = 0; ok && !cancelled_ && i < 5; ++i) {
    size_t written = 0;
    ok = i2s_write(I2S_NUM_0, samples, sizeof(samples), &written, pdMS_TO_TICKS(100)) == ESP_OK &&
         written == sizeof(samples);
  }
  i2s_stop(I2S_NUM_0);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
  i2s_driver_uninstall(I2S_NUM_0);
  // Keep amplifier input at the PCM midpoint between words, not at full negative level.
  dac_output_enable(DAC_CHANNEL_2);
  dac_output_voltage(DAC_CHANNEL_2, 128);
  return ok;
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
