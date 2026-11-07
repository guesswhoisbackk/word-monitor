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
wordmon::QuizGame quiz(study);
wordmon::WordUi wordUi(settings, study, audio, quiz);
wordmon::Wordbook wordbook(settings, study);
wordmon::WeatherService weather(settings);
wordmon::WebPortal webPortal(settings, settingsStore, weather, wordbook, study, quiz);
wordmon::NetworkRecovery networkRecovery;
wordmon::SetupRecovery setupRecovery;
bool setupAttempt = false, setupOnline = false;
uint32_t pushedWordbookRevision = 0;

bool accessPointMode = false;
RTC_NOINIT_ATTR uint32_t setupBootTest;

String setupNetworkName() {
  const uint64_t chip = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X",
           static_cast<unsigned>(chip & 0xFFFF));
  return String("WordMon-") + suffix;
}

void startSetupAP() {
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
  setupRecovery.reset(); setupAttempt = setupOnline = false;
}

void stationRestored() {
  MDNS.end(); MDNS.begin("wordmon");
  configTzTime(wordmon::kTimezone, wordmon::kNtpServer1, wordmon::kNtpServer2);
  wordUi.setNetworkInfo(accessPointMode, accessPointMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  weather.requestRefresh(); wordbook.networkRestored();
  Serial.printf("[wifi] connection restored rssi=%d; refreshing weather\n", WiFi.RSSI());
}

void startNetwork() {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  const bool testSetup = esp_reset_reason() == ESP_RST_SW && setupBootTest == 0x574D4150;
  setupBootTest = 0;
  if (testSetup) { startSetupAP(); return; }

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

  startSetupAP();
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
  if (Serial.available() && Serial.peek() == 'z') {
    Serial.read(); setupBootTest = 0x574D4150;
    Serial.println("[wifi] USB setup recovery test: restarting into setup AP");
    ESP.restart();
  }
  if (accessPointMode) {
    const unsigned clients = WiFi.softAPgetStationNum();
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (clients && setupAttempt && !connected) {
      WiFi.disconnect(false, false); setupAttempt = false;
      Serial.println("[wifi] paused recovery for setup phone");
    }
    if (connected && !setupOnline) { setupOnline = true; stationRestored(); }
    if (!connected) setupOnline = false;
    const auto action = setupRecovery.poll(millis(), connected, !settings.wifiSsid.isEmpty(), clients);
    if (action == wordmon::SetupAction::Retry) {
      Serial.println("[wifi] setup idle: retry saved network");
      WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str()); setupAttempt = true;
    } else if (action == wordmon::SetupAction::Finish) {
      WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
      accessPointMode = false; webPortal.setAccessPointMode(false); wordUi.closeSettings();
      wordUi.setNetworkInfo(false, WiFi.localIP().toString());
      networkRecovery.poll(millis(), true, false);
      Serial.println("[wifi] setup recovery complete; AP closed");
    }
    return;
  }
  const auto action = networkRecovery.poll(millis(), WiFi.status() == WL_CONNECTED,
                                           accessPointMode || settings.wifiSsid.isEmpty());
  if (action == wordmon::NetworkAction::Retry) {
    Serial.println("[wifi] recovery attempt");
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  } else if (action == wordmon::NetworkAction::Restored) {
    stationRestored();
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
  wordbook.poll();
  weather.loop(false);
  wordUi.setWeather(weather.reading(), weather.busy());
  if (audio.busy()) { delay(5); return; }
  const uint8_t todayRequest=wordUi.takeTodayRequest(wordbook.readyForToday());
  if(todayRequest) {
    audio.cancel();
    if((todayRequest==1 || todayRequest==5) && !wordbook.beginToday(todayRequest==5)) wordUi.todayUnavailable(todayRequest==5);
    if(todayRequest==2) wordbook.endToday();
    if(todayRequest==3) wordbook.stepToday(-1);
    if(todayRequest==4) wordbook.stepToday(1);
  }
  wordbook.setQuizPreview(quiz.active() && !quiz.reviewing() && !quiz.finished()?quiz.word():nullptr);
  wordUi.setTodayState(wordbook.todayMode(),wordbook.todayPosition(),wordbook.todayCount(),todayRequest==3 || todayRequest==4);
  if (!wordbook.todayMode() && wordUi.takeReviewRequest()) {
    const bool found = wordbook.nextReview(study, wordUi.reviewOnly());
    wordUi.setWordbookCard(wordbook.card(), wordbook.state(), wordbook.wordCount());
    pushedWordbookRevision = wordbook.revision();
    wordUi.reviewFinished(found);
  }
  wordbook.updateHome(wordUi.homeSelected(), wordUi.takeHomeStep(), wordUi.homeHeld());
  if (wordbook.revision() != pushedWordbookRevision) {
    pushedWordbookRevision = wordbook.revision();
    wordUi.setWordbookCard(wordbook.card(), wordbook.state(), wordbook.wordCount());
  }
  wordUi.serviceAudio(!wordbook.busy() && !weather.busy());
  if (!audio.busy()) {
    if (!wordbook.busy()) weather.loop(true);
    wordbook.loop(!weather.busy());
  }
  wordUi.setStudyPlan(wordbook.studyPlan(), wordbook.studyPlanReady(wordmon::studyDay(static_cast<uint32_t>(time(nullptr)))));
  delay(5);
}
