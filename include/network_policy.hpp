#pragma once
#include <cstdint>

namespace wordmon {
enum class NetworkAction { None, Retry, Restored };
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
