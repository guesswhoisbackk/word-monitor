#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "app_config.hpp"
#include "models.hpp"
#include "settings_store.hpp"
#include "web_portal.hpp"
#include "word_ui.hpp"
#include "wordbook.hpp"
#include "weather.hpp"
#include "network_policy.hpp"

namespace {

wordmon::AppSettings settings;
wordmon::SettingsStore settingsStore;
wordmon::StudyStore study;
wordmon::AudioPlayer audio;
wordmon::WordUi wordUi(settings, study, audio);
wordmon::Wordbook wordbook(settings, study);
wordmon::WeatherService weather(settings);
wordmon::WebPortal webPortal(settings, settingsStore, weather);
wordmon::NetworkRecovery networkRecovery;
uint32_t pushedWordbookRevision = 0;

bool accessPointMode = false;

String setupNetworkName() {
  const uint64_t chip = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X",
           static_cast<unsigned>(chip & 0xFFFF));
  return String("WordMon-") + suffix;
}

void startNetwork() {
  WiFi.persistent(false);
  WiFi.setSleep(false);

  if (!settings.wifiSsid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    Serial.printf("[wifi] connecting to %s", settings.wifiSsid.c_str());
    const uint32_t deadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<int32_t>(deadline - millis()) > 0) {
      Serial.print('.');
      wordUi.loop();
      delay(245);
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    accessPointMode = false;
    if (!MDNS.begin("wordmon")) {
      Serial.println("[mdns] could not start wordmon.local");
    }
    configTzTime(wordmon::kTimezone, wordmon::kNtpServer1,
                 wordmon::kNtpServer2);
    wordUi.setNetworkInfo(false, WiFi.localIP().toString());
    Serial.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    return;
  }

  accessPointMode = true;
  // Do not keep searching for an unavailable saved network: STA channel
  // changes can interrupt the phone's connection to the setup AP.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  const String ssid = setupNetworkName();
  if (!WiFi.softAP(ssid.c_str(), "wordmon1")) {
    Serial.println("[wifi] access point failed");
  }
  wordUi.setNetworkInfo(true, WiFi.softAPIP().toString());
  Serial.printf("[wifi] setup AP: %s / password: wordmon1 / %s\n",
                ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

void maintainNetwork() {
  // USB test command: disconnect only this ESP32, exercising app recovery even
  // when the SDK will not retry (e.g. an authentication failure).
  if (!accessPointMode && Serial.available() && Serial.peek() == 'x') {
    Serial.read();
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    Serial.println("[wifi] USB recovery test: disconnected");
  }
  const auto action = networkRecovery.poll(millis(), WiFi.status() == WL_CONNECTED,
                                           accessPointMode || settings.wifiSsid.isEmpty());
  if (action == wordmon::NetworkAction::Retry) {
    Serial.println("[wifi] recovery attempt");
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  } else if (action == wordmon::NetworkAction::Restored) {
    MDNS.end();
    MDNS.begin("wordmon");
    configTzTime(wordmon::kTimezone, wordmon::kNtpServer1, wordmon::kNtpServer2);
    wordUi.setNetworkInfo(false, WiFi.localIP().toString());
    weather.requestRefresh();
    Serial.printf("[wifi] connection restored rssi=%d; refreshing weather\n", WiFi.RSSI());
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n%s v%s | panel %s\n", wordmon::kAppName,
                wordmon::kVersion, WORDMON_PANEL_NAME);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("[wifi] phone joined setup AP");
    if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) Serial.println("[wifi] phone received DHCP address");
    if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("[wifi] phone left setup AP");
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) Serial.printf("[wifi] station disconnect reason=%u\n", info.wifi_sta_disconnected.reason);
  });

  if (!settingsStore.load(settings)) {
    // Reuse only network settings from the previous Dayboard. Tasks stay intact
    // in their original namespace and are not part of the Word / Day UI.
    Preferences previous;
    if (previous.begin("dayboard", true)) {
      settings.wifiSsid = previous.getString("ssid", "");
      settings.wifiPassword = previous.getString("password", "");
      previous.end();
      if (!settings.wifiSsid.isEmpty()) settingsStore.save(settings);
    }
  }
  wordbook.begin(); // Mount LittleFS before reading/migrating study records.
  study.begin();
  wordUi.begin();
  startNetwork();
  webPortal.begin(accessPointMode);
  if (accessPointMode) wordUi.showSettings();
  networkRecovery.poll(millis(), WiFi.status() == WL_CONNECTED, accessPointMode);
}

void loop() {
  maintainNetwork();
  webPortal.loop();
  wordUi.loop();
  if (wordUi.takeWeatherRefreshRequest()) weather.requestRefresh();
  // Audio worker owns network/FS while active; LVGL and portal keep running.
  // Avoid concurrent downloads/decoding and changing the active card mid-word.
  if (audio.busy()) { delay(5); return; }
  weather.loop(!audio.busy());
  wordUi.setWeather(weather.reading(), weather.busy());
  if (weather.busy()) { delay(5); return; }
  wordbook.loop();
  if (wordUi.takeReviewRequest()) {
    const bool found = wordbook.nextReview(study);
    wordUi.setWordbookCard(wordbook.card(), wordbook.state(), wordbook.wordCount());
    pushedWordbookRevision = wordbook.revision();
    wordUi.reviewFinished(found);
  }
  if (wordbook.revision() != pushedWordbookRevision) {
    pushedWordbookRevision = wordbook.revision();
    wordUi.setWordbookCard(wordbook.card(), wordbook.state(),
                           wordbook.wordCount());
  }
  delay(5);
}
