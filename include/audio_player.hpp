#pragma once
#include <Arduino.h>
#include <atomic>
#include <memory>
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
  bool download(std::unique_ptr<uint8_t[]>& buffer, size_t& size);
  bool readCache(std::unique_ptr<uint8_t[]>& buffer, size_t& size);
  void saveCache(const uint8_t* buffer, size_t size);
  bool playPcm(const uint8_t* buffer, const WavInfo& info);
};
}
