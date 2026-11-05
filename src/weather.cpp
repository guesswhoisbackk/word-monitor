#include "weather.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>

namespace wordmon {
void WeatherService::loop(bool networkAvailable) {
  if (!queue_) queue_ = xQueueCreate(1, sizeof(WeatherReading));
  WeatherReading result;
  if (queue_ && xQueueReceive(queue_, &result, 0) == pdTRUE) {
    busy_ = false;
    if (result.valid) reading_ = result;
    nextAttempt_ = millis() + (result.valid ? 900000 : 60000);
  }
  if (!queue_ || busy_ || !settings_.weatherEnabled || !networkAvailable ||
      WiFi.status() != WL_CONNECTED || int32_t(millis() - nextAttempt_) < 0) return;
  busy_ = true;
  latitude_ = settings_.weatherLatitude;
  longitude_ = settings_.weatherLongitude;
  if (xTaskCreate(fetch, "weather", 6144, this, 1, nullptr) != pdPASS) {
    busy_ = false;
    nextAttempt_ = millis() + 60000;
  }
}
void WeatherService::fetch(void* context) {
  auto* self = static_cast<WeatherService*>(context);
  WeatherReading result;
  // Public weather only; HTTP avoids a concurrent TLS heap allocation on CYD.
  // Never send Wi-Fi credentials or device identifiers to this service.
  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(self->latitude_, 5);
  url += "&longitude=" + String(self->longitude_, 5);
  url += "&current=temperature_2m,apparent_temperature,weather_code&daily=precipitation_probability_max&timezone=Asia%2FSeoul&forecast_days=1";
  {
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.useHTTP10(true);
    if (http.begin(client, url) && http.GET() == 200 && http.getSize() <= 8192) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getStream())) {
        JsonVariantConst temp = doc["current"]["temperature_2m"];
        JsonVariantConst code = doc["current"]["weather_code"];
        JsonVariantConst rain = doc["daily"]["precipitation_probability_max"][0];
        if (temp.is<float>() && code.is<int>() && rain.is<int>()) {
          result.temperature = temp.as<float>(); result.code = code.as<int>(); result.rain = rain.as<int>();
          JsonVariantConst apparent = doc["current"]["apparent_temperature"];
          if (apparent.is<float>()) {
            result.apparentTemperature = apparent.as<float>();
            result.hasApparentTemperature = std::isfinite(result.apparentTemperature) &&
                result.apparentTemperature >= -120 && result.apparentTemperature <= 80;
          }
          result.valid = std::isfinite(result.temperature) && result.temperature >= -100 && result.temperature <= 70 &&
                         result.code >= 0 && result.code <= 99 && result.rain >= 0 && result.rain <= 100;
          result.receivedAt = millis();
        }
      }
    }
    http.end();
  }
  Serial.printf("[weather] %s temp=%.1f code=%d rain=%d heap=%u\n", result.valid ? "updated" : "retry", result.temperature, result.code, result.rain, ESP.getFreeHeap());
  // All network allocations are released before publishing completion.
  xQueueOverwrite(self->queue_, &result);
  vTaskDelete(nullptr);
}
}
