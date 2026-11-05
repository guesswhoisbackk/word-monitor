#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "models.hpp"
#include "weather_policy.hpp"

namespace wordmon {
struct WeatherReading {
  bool valid = false;
  float temperature = 0;
  float apparentTemperature = 0;
  bool hasApparentTemperature = false;
  int code = 0;
  RainPeriods rain;
  float precipitationMm = 0;
  bool hasPrecipitation = false;
  uint32_t receivedAt = 0;
};
class WeatherService {
 public:
  explicit WeatherService(const AppSettings& settings) : settings_(settings) {}
  void loop(bool networkAvailable);
  bool busy() const { return busy_; }
  void requestRefresh() { refreshRequested_ = true; }
  uint32_t attempts() const { return attempts_; }
  uint32_t updates() const { return updates_; }
  int lastHttpStatus() const { return lastHttpStatus_; }
  bool lastAttemptOk() const { return lastAttemptOk_; }
  uint32_t nextAttemptInMs() const {
    const int32_t remaining = int32_t(nextAttempt_ - millis());
    return remaining > 0 ? uint32_t(remaining) : 0;
  }
  const WeatherReading& reading() const { return reading_; }
 private:
  struct FetchResult { WeatherReading reading; int httpStatus = -1; };
  const AppSettings& settings_;
  WeatherReading reading_;
  QueueHandle_t queue_ = nullptr;
  bool busy_ = false;
  bool refreshRequested_ = false, lastAttemptOk_ = false;
  uint32_t attempts_ = 0, updates_ = 0;
  int lastHttpStatus_ = 0;
  uint32_t nextAttempt_ = 20000;
  float latitude_ = 0, longitude_ = 0;
  static void fetch(void* context);
};
}
