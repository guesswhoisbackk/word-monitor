#pragma once
#include <cstdint>

namespace wordmon {
enum class NetworkAction { None, Retry, Restored };
enum class SetupAction { None, Retry, Finish };
class SetupRecovery {
  bool waiting_ = false, connected_ = false;
  uint32_t lastAttempt_ = 0, connectedAt_ = 0;
 public:
  void reset() { waiting_ = connected_ = false; }
  SetupAction poll(uint32_t now, bool connected, bool saved, unsigned clients) {
    if (clients) { waiting_ = connected_ = false; return SetupAction::None; }
    if (connected) {
      if (!connected_) { connected_ = true; connectedAt_ = now; }
      return uint32_t(now - connectedAt_) >= 10000 ? SetupAction::Finish : SetupAction::None;
    }
    connected_ = false;
    if (!saved) return SetupAction::None;
    if (!waiting_) { waiting_ = true; lastAttempt_ = now; }
    if (uint32_t(now - lastAttempt_) < 60000) return SetupAction::None;
    lastAttempt_ = now;
    return SetupAction::Retry;
  }
};
class NetworkRecovery {
 public:
  NetworkAction poll(uint32_t now, bool connected, bool setupMode) {
    // Keep the configuration AP stable while a phone is setting up Wi-Fi.
    if (setupMode) { connected_ = waiting_ = false; return NetworkAction::None; }
    if (connected) {
      const bool restored = !connected_;
      connected_ = true; waiting_ = false;
      return restored ? NetworkAction::Restored : NetworkAction::None;
    }
    connected_ = false;
    if (!waiting_) { waiting_ = true; lastAttempt_ = now; }
    if (uint32_t(now - lastAttempt_) < 15000u) return NetworkAction::None;
    lastAttempt_ = now;
    return NetworkAction::Retry;
  }
 private:
  bool connected_ = false, waiting_ = false;
  uint32_t lastAttempt_ = 0;
};
}
