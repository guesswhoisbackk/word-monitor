#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "models.hpp"
#include "settings_store.hpp"

namespace wordmon {

class WebPortal {
 public:
  WebPortal(AppSettings& settings, SettingsStore& store);

  void begin(bool accessPointMode);
  void loop();
  bool restartPending() const { return restartAt_ != 0; }

 private:
  AppSettings& settings_;
  SettingsStore& store_;
  WebServer server_{80};
  DNSServer dns_;
  bool accessPointMode_ = false;
  bool scanRequested_ = false;
  uint32_t lastScanFinishedAt_ = 0;
  uint32_t restartAt_ = 0;

  void handleRoot();
  void handleWifiApi();
  void handleSave();
  void handleNotFound();
  static String escapeHtml(const String& input);
  static String escapeJson(const String& input);
};

}  // namespace wordmon
