#include "settings_store.hpp"

#include <ArduinoJson.h>

#include "app_config.hpp"

namespace wordmon {

namespace {

void applyDefaults(AppSettings& settings) {
  settings = AppSettings{};
}

}  // namespace

bool SettingsStore::load(AppSettings& settings) {
  applyDefaults(settings);

  if (!preferences_.begin(kPreferencesNamespace, true)) {
    return false;
  }
  const String json = preferences_.getString(kPreferencesKey, "");
  preferences_.end();

  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.printf("[settings] invalid config: %s\n", error.c_str());
    return false;
  }

  settings.wifiSsid = doc["wifi"]["ssid"] | "";
  settings.wifiPassword = doc["wifi"]["password"] | "";
  settings.brightness = constrain(doc["display"]["brightness"] | 220, 20, 255);
  settings.audioVolume = constrain(doc["audio"]["volume"] | 20, 0, 60);
  settings.wordbookUrl = doc["wordbook"]["url"] | "";
  return true;
}

bool SettingsStore::save(const AppSettings& settings) {
  JsonDocument doc;
  doc["schema"] = 1;
  doc["wifi"]["ssid"] = settings.wifiSsid;
  doc["wifi"]["password"] = settings.wifiPassword;
  doc["display"]["brightness"] = settings.brightness;
  doc["audio"]["volume"] = settings.audioVolume;
  doc["wordbook"]["url"] = settings.wordbookUrl;

  String json;
  serializeJson(doc, json);
  if (!preferences_.begin(kPreferencesNamespace, false)) {
    return false;
  }
  const size_t written = preferences_.putString(kPreferencesKey, json);
  preferences_.end();
  return written == json.length();
}

void SettingsStore::clear() {
  if (preferences_.begin(kPreferencesNamespace, false)) {
    preferences_.clear();
    preferences_.end();
  }
}

}  // namespace wordmon
