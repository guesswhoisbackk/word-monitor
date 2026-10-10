#pragma once
#include <Arduino.h>
#include <atomic>
#include <LittleFS.h>
#include "audio_policy.hpp"

namespace wordmon {
enum class AudioState { Idle, Loading, Playing, Done, Cancelled, Missing, Offline, Invalid, Failed, Muted };
class AudioPlayer {
 public:
  bool start(const String& word, const String& filename, const String& baseUrl, uint8_t volume);
  void cancel() { cancelled_.store(true); }
  bool busy() const { return busy_.load(); }
  AudioState state() const { return state_.load(); }
  const char* message() const;
  static bool available(const char* word, const char* filename);
 private:
  std::atomic<bool> busy_{false};
  std::atomic<bool> cancelled_{false};
  std::atomic<AudioState> state_{AudioState::Idle};
  String word_;
  String url_;
  uint8_t volume_ = 20;
  static void task(void* context);
  void run();
  // External clips stream from LittleFS in small blocks: no whole-file RAM
  // buffer exists, so heap fragmentation after TLS/Wi-Fi cannot break audio.
  bool openVoice(const char* path, File& file, WavInfo& info);
  bool openCached(File& file, WavInfo& info);
  bool download();
  void finalizeDownload();
  bool playPcmFile(File& file, const WavInfo& info);
  bool playPcm(const uint8_t* buffer, const WavInfo& info);  // built-in flash clips
};
}
