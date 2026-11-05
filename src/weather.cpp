#include "weather.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>

namespace wordmon {
void WeatherService::loop(bool networkAvailable) {
  if (!queue_) queue_ = xQueueCreate(1, sizeof(FetchResult));
  FetchResult result;
  if (queue_ && xQueueReceive(queue_, &result, 0) == pdTRUE) {
    busy_ = false;
    lastHttpStatus_ = result.httpStatus;
    lastAttemptOk_ = result.reading.valid;
    if (result.reading.valid) { reading_ = result.reading; ++updates_; }
    nextAttempt_ = millis() + (result.reading.valid ? 900000 : 60000);
  }
  if (refreshRequested_ && !busy_) { nextAttempt_ = millis(); refreshRequested_ = false; }
  if (!queue_ || busy_ || !settings_.weatherEnabled || !networkAvailable ||
      WiFi.status() != WL_CONNECTED || int32_t(millis() - nextAttempt_) < 0) return;
  busy_ = true;
  ++attempts_;
  latitude_ = settings_.weatherLatitude;
  longitude_ = settings_.weatherLongitude;
  if (xTaskCreate(fetch, "weather", 6144, this, 1, nullptr) != pdPASS) {
    busy_ = false;
    nextAttempt_ = millis() + 60000;
  }
}
void WeatherService::fetch(void* context) {
  auto* self = static_cast<WeatherService*>(context);
  FetchResult envelope;
  WeatherReading& result = envelope.reading;
  {
  // Public weather only; HTTP avoids a concurrent TLS heap allocation on CYD.
  // Never send Wi-Fi credentials or device identifiers to this service.
  // Keep URL ownership inside this scope: vTaskDelete does not unwind C++
  // automatic objects, so its buffer must be destroyed before deleting self.
  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(self->latitude_, 5);
  url += "&longitude=" + String(self->longitude_, 5);
  url += "&current=temperature_2m,apparent_temperature,precipitation,weather_code&hourly=precipitation_probability&precipitation_unit=mm&timezone=Asia%2FSeoul&forecast_days=1";
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.useHTTP10(true);
    if (http.begin(client, url)) envelope.httpStatus = http.GET();
    if (envelope.httpStatus == 200 && http.getSize() <= 8192) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getStream())) {
        JsonVariantConst temp = doc["current"]["temperature_2m"];
        JsonVariantConst code = doc["current"]["weather_code"];
        if (temp.is<float>() && code.is<int>()) {
          result.temperature = temp.as<float>(); result.code = code.as<int>();
          JsonVariantConst precipitation = doc["current"]["precipitation"];
          if (precipitation.is<float>()) {
            result.precipitationMm = precipitation.as<float>();
            result.hasPrecipitation = std::isfinite(result.precipitationMm) && result.precipitationMm >= 0 && result.precipitationMm <= 500;
          }
          const char* at = doc["current"]["time"] | "";
          if (strlen(at) >= 16) {
            const int currentHour = (at[11] - '0') * 10 + at[12] - '0';
            JsonArrayConst times = doc["hourly"]["time"];
            JsonArrayConst probabilities = doc["hourly"]["precipitation_probability"];
            for (size_t i = 0; i < times.size() && i < probabilities.size(); ++i) {
              const char* stamp = times[i] | "";
              if (strlen(stamp) < 16 || strncmp(at, stamp, 10) || !probabilities[i].is<int>()) continue;
              const int hour = (stamp[11] - '0') * 10 + stamp[12] - '0';
              result.rain.consider(hour, probabilities[i].as<int>(), currentHour);
            }
          }
          JsonVariantConst apparent = doc["current"]["apparent_temperature"];
          if (apparent.is<float>()) {
            result.apparentTemperature = apparent.as<float>();
            result.hasApparentTemperature = std::isfinite(result.apparentTemperature) &&
                result.apparentTemperature >= -120 && result.apparentTemperature <= 80;
          }
          result.valid = std::isfinite(result.temperature) && result.temperature >= -100 && result.temperature <= 70 &&
                         result.code >= 0 && result.code <= 99;
          result.receivedAt = millis();
        }
      }
    }
    http.end();
  }
  Serial.printf("[weather] %s temp=%.1f code=%d rain=%d/%d/%d remaining=%d mm=%.1f heap=%u\n",
      result.valid ? "updated" : "retry", result.temperature, result.code, result.rain.morning, result.rain.noon,
      result.rain.evening, result.rain.remaining, result.precipitationMm, ESP.getFreeHeap());
  // All network allocations are released before publishing completion.
  xQueueOverwrite(self->queue_, &envelope);
  vTaskDelete(nullptr);
}
}
