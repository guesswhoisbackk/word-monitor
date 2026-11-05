#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "models.hpp"

namespace wordmon {
struct WeatherReading {
  bool valid = false;
  float temperature = 0;
  int code = 0;
  int rain = 0;
  uint32_t receivedAt = 0;
};
const char* weatherDescription(int code);
class WeatherService {
 public:
  explicit WeatherService(const AppSettings& settings) : settings_(settings) {}
  void loop(bool networkAvailable);
  bool busy() const { return busy_; }
  const WeatherReading& reading() const { return reading_; }
 private:
  const AppSettings& settings_;
  WeatherReading reading_;
  QueueHandle_t queue_ = nullptr;
  bool busy_ = false;
  uint32_t nextAttempt_ = 20000;
  float latitude_ = 0, longitude_ = 0;
  static void fetch(void* context);
};
}
