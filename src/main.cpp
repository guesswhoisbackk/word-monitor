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

namespace {

wordmon::AppSettings settings;
wordmon::SettingsStore settingsStore;
wordmon::StudyStore study;
wordmon::WordUi wordUi(settings, study);
wordmon::WebPortal webPortal(settings, settingsStore);
wordmon::Wordbook wordbook(settings);
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
  WiFi.mode(WIFI_AP_STA);
  const String ssid = setupNetworkName();
  if (!WiFi.softAP(ssid.c_str(), "wordmon1")) {
    Serial.println("[wifi] access point failed");
  }
  wordUi.setNetworkInfo(true, WiFi.softAPIP().toString());
  Serial.printf("[wifi] setup AP: %s / password: wordmon1 / %s\n",
                ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n%s v%s | panel %s\n", wordmon::kAppName,
                wordmon::kVersion, WORDMON_PANEL_NAME);

  settingsStore.load(settings);
  study.begin();
  wordUi.begin();
  startNetwork();
  wordbook.begin();
  webPortal.begin(accessPointMode);
}

void loop() {
  webPortal.loop();
  wordUi.loop();
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
