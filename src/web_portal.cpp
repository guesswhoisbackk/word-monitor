#include "web_portal.hpp"

#include <WiFi.h>

#include "app_config.hpp"

namespace wordmon {

namespace {

constexpr uint32_t kWifiScanCooldownMs = 10000;
constexpr int16_t kMaxWifiScanResults = 20;

// The setup page is streamed in small chunks. Building the whole page in one
// Arduino String can fragment the classic ESP32 heap while LVGL is active.
const char kPageStart[] PROGMEM = R"HTML(<!doctype html><html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WordMon 설정</title><style>
:root{color-scheme:dark;--bg:#07111f;--card:#0d1b2d;--line:#1d3550;--text:#e8f2ff;--muted:#8ba3ba;--green:#38e29d;--cyan:#27c7ff}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(145deg,#07111f,#0b2031);color:var(--text);font:15px/1.5 system-ui,sans-serif}
main{max-width:900px;margin:auto;padding:22px}.hero{display:flex;justify-content:space-between;gap:20px;align-items:end;margin-bottom:18px}
h1{margin:0;font-size:28px}.tag{color:var(--green);font-weight:800}.muted{color:var(--muted)}
.card{background:rgba(13,27,45,.94);border:1px solid var(--line);border-radius:16px;padding:18px;margin:14px 0;box-shadow:0 12px 40px #0005}
label{display:block;color:var(--muted);font-size:13px;margin:8px 0 4px}input{width:100%;background:#071523;color:var(--text);border:1px solid #29435f;border-radius:9px;padding:11px;font-size:16px}
select{width:100%;background:#071523;color:var(--text);border:1px solid #29435f;border-radius:9px;padding:11px;font-size:16px}
.warning{background:#392b13;border:1px solid #80652b;border-radius:10px;padding:12px;color:#ffdaa0}
button{width:100%;border:0;border-radius:11px;padding:15px;background:linear-gradient(90deg,var(--green),var(--cyan));color:#03131d;font-weight:900;font-size:17px;cursor:pointer;margin-top:12px}
.wifi-status{margin:12px 0;padding:12px;border:1px solid var(--line);border-radius:10px;background:#071523;color:var(--muted)}
.wifi-status.connected{border-color:#20795b;color:var(--green)}.wifi-status.setup{border-color:#176b88;color:var(--cyan)}.wifi-actions{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:end;margin-bottom:8px}
.wifi-actions button{width:auto;margin:0;padding:11px 16px;font-size:14px}.wifi-actions button:disabled{opacity:.55;cursor:wait}.scan-note{min-height:22px;color:var(--muted);font-size:13px}
@media(max-width:640px){.wifi-actions{grid-template-columns:1fr}.wifi-actions button{width:100%}.hero{display:block}}
</style></head><body><main><div class="hero"><div><div class="tag">WORDMON STUDIO</div><h1>영어 단어장 설정</h1></div><div class="muted">v)HTML";

const char kWifiStart[] PROGMEM = R"HTML(</div></div>
<div class="warning">이 설정은 기기 안에 저장됩니다. 신뢰할 수 있는 집이나 작업실 네트워크에서만 설정하세요.</div>
<form method="post" action="/save"><section class="card"><h2>Wi-Fi와 화면</h2>
<div id="wifiStatus" class="wifi-status">연결 상태 확인 중...</div>
<div class="wifi-actions"><div><label for="wifiNetworkSelect">주변 2.4GHz Wi-Fi</label><select id="wifiNetworkSelect"><option value="">검색 버튼을 눌러주세요</option></select></div><button type="button" id="wifiScanButton">Wi-Fi 검색</button></div>
<div id="wifiScanNote" class="scan-note">목록에 없어도 아래에서 직접 입력할 수 있습니다.</div>
<div class="grid"><div><label for="ssidInput">2.4GHz Wi-Fi 이름(SSID)</label><input id="ssidInput" name="ssid" required value=")HTML";

const char kWifiMiddle[] PROGMEM = R"HTML("></div><div><label>Wi-Fi 비밀번호</label><input type="password" name="wifiPassword" placeholder="처음 설정할 때는 반드시 입력"></div></div>
<label>화면 밝기(20~255, 추천 220)</label><input type="number" min="20" max="255" name="brightness" value=")HTML";

const char kWordbookStart[] PROGMEM = R"HTML("></section><section class="card"><h2>단어장 (인터넷)</h2>
<p class="muted">words.jsonl과 그림 PNG를 올려둔 공개 저장소 폴더의 주소를 입력하세요. 비워 두면 기기에 내장된 샘플 단어가 나옵니다. GitHub를 쓴다면 raw 주소보다 jsDelivr 주소를 권장합니다.</p>
<label for="wordbookUrl">단어장 기본 URL</label><input id="wordbookUrl" name="wordbookUrl" placeholder="https://cdn.jsdelivr.net/gh/사용자명/저장소@main" value=")HTML";

const char kAudioStart[] PROGMEM = R"HTML("><label>발음 음량 (0: 음소거, 1~60, 처음에는 20 권장)</label><input type="number" name="audioVolume" min="0" max="60" value=")HTML";

const char kPageEnd[] PROGMEM = R"HTML("></section><button type="submit">저장하고 다시 시작</button></form>
<script>
const wifiStatus=document.getElementById('wifiStatus');
const wifiSelect=document.getElementById('wifiNetworkSelect');
const wifiScanButton=document.getElementById('wifiScanButton');
const wifiScanNote=document.getElementById('wifiScanNote');
const ssidInput=document.getElementById('ssidInput');
function signalText(rssi){if(rssi>=-55)return '매우 좋음';if(rssi>=-67)return '좋음';if(rssi>=-75)return '보통';return '약함'}
function renderStatus(data){
  const setup=data.mode==='setup';
  wifiStatus.classList.toggle('connected',!setup&&data.connected);
  wifiStatus.classList.toggle('setup',setup);
  if(setup){wifiStatus.textContent=`설정 모드 · ${data.setupSsid} · ${data.ip}${data.connected?` · 홈 Wi-Fi ${data.ssid}도 연결됨`:' · 홈 Wi-Fi 미연결'}`}
  else if(data.connected){wifiStatus.textContent=`Wi-Fi 연결됨 · ${data.ssid} · ${data.ip} · ${data.rssi} dBm (${signalText(data.rssi)})`}
  else{wifiStatus.textContent='홈 Wi-Fi 연결 끊김'}
}
function renderNetworks(networks){
  wifiSelect.replaceChildren(new Option(networks.length?'Wi-Fi를 선택하세요':'검색된 Wi-Fi가 없습니다',''));
  for(const network of networks){
    const lock=network.secure?'🔒 ':'공개 ';
    wifiSelect.add(new Option(`${lock}${network.ssid} · ${network.rssi} dBm (${signalText(network.rssi)})`,network.ssid));
  }
  wifiScanNote.textContent=networks.length?`${networks.length}개 네트워크를 찾았습니다. 선택하거나 SSID를 직접 입력하세요.`:'검색된 네트워크가 없습니다. SSID를 직접 입력할 수 있습니다.';
}
async function loadWifi(scan=false){
  let polling=false;
  wifiScanButton.disabled=true;
  wifiScanButton.textContent=scan?'검색 시작 중...':'상태 확인 중...';
  try{
    const response=await fetch(scan?'/api/wifi?scan=1':'/api/wifi',{cache:'no-store'});
    if(!response.ok)throw new Error(`HTTP ${response.status}`);
    const data=await response.json();
    renderStatus(data);
    if(data.scanState==='running'){
      polling=true;
      wifiScanButton.textContent='검색 중...';
      wifiScanNote.textContent='주변 2.4GHz Wi-Fi를 찾고 있습니다...';
      setTimeout(()=>loadWifi(false),600);
      return;
    }
    if(data.scanState==='ready')renderNetworks(data.networks||[]);
    if(data.scanState==='failed')wifiScanNote.textContent='검색에 실패했습니다. 다시 시도하거나 SSID를 직접 입력하세요.';
    if(data.scanState==='cooldown')wifiScanNote.textContent=`연속 검색을 제한하고 있습니다. ${Math.max(1,Math.ceil(data.retryAfterMs/1000))}초 후 다시 눌러주세요.`;
  }catch(error){wifiScanNote.textContent=`Wi-Fi 상태를 읽지 못했습니다: ${error.message}`}
  finally{if(!polling){wifiScanButton.disabled=false;wifiScanButton.textContent='Wi-Fi 검색'}}
}
wifiSelect.addEventListener('change',()=>{if(wifiSelect.value)ssidInput.value=wifiSelect.value});
wifiScanButton.addEventListener('click',()=>loadWifi(true));
loadWifi(false);
</script>
</main></body></html>)HTML";

}  // namespace

WebPortal::WebPortal(AppSettings& settings, SettingsStore& store)
    : settings_(settings), store_(store) {}

void WebPortal::begin(bool accessPointMode) {
  accessPointMode_ = accessPointMode;
  if (accessPointMode_) {
    dns_.start(53, "*", WiFi.softAPIP());
  }

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/wifi", HTTP_GET, [this]() { handleWifiApi(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  Serial.printf("[web] config portal: http://%s/\n",
                accessPointMode_ ? WiFi.softAPIP().toString().c_str()
                                 : WiFi.localIP().toString().c_str());
}

void WebPortal::loop() {
  if (accessPointMode_) {
    dns_.processNextRequest();
  }
  server_.handleClient();
  if (restartAt_ != 0 && static_cast<int32_t>(millis() - restartAt_) >= 0) {
    ESP.restart();
  }
}

void WebPortal::handleRoot() {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "text/html; charset=utf-8", "");
  server_.sendContent_P(kPageStart);
  server_.sendContent(kVersion);
  server_.sendContent(F(" / " WORDMON_PANEL_NAME));
  server_.sendContent_P(kWifiStart);
  // In HTTP chunked transfer a zero-length chunk means "end of response".
  // On first boot the SSID is empty, so never pass that empty String to
  // sendContent; the following static chunk will close the value attribute.
  const String escapedSsid = escapeHtml(settings_.wifiSsid);
  if (!escapedSsid.isEmpty()) {
    server_.sendContent(escapedSsid);
  }
  server_.sendContent_P(kWifiMiddle);
  server_.sendContent(String(settings_.brightness));
  server_.sendContent_P(kAudioStart);
  server_.sendContent(String(settings_.audioVolume));
  server_.sendContent_P(kWordbookStart);
  const String escapedUrl = escapeHtml(settings_.wordbookUrl);
  if (!escapedUrl.isEmpty()) {
    server_.sendContent(escapedUrl);
  }
  server_.sendContent_P(kPageEnd);
  server_.sendContent(String());
}

void WebPortal::handleWifiApi() {
  int16_t scanResult = WiFi.scanComplete();
  const bool startScan =
      server_.hasArg("scan") && server_.arg("scan") == "1";
  bool scanThrottled = false;
  uint32_t retryAfterMs = 0;
  if (startScan && !scanRequested_) {
    const uint32_t elapsedSinceScan = millis() - lastScanFinishedAt_;
    if (lastScanFinishedAt_ != 0 &&
        elapsedSinceScan < kWifiScanCooldownMs) {
      scanThrottled = true;
      retryAfterMs = kWifiScanCooldownMs - elapsedSinceScan;
    } else {
      WiFi.scanDelete();
      scanRequested_ = true;
      scanResult = WiFi.scanNetworks(true, true);
    }
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  const String ipAddress =
      accessPointMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  const String networkName = connected ? WiFi.SSID() : String();
  const String setupNetworkName =
      accessPointMode_ ? WiFi.softAPSSID() : String();
  const int32_t signalStrength = connected ? WiFi.RSSI() : 0;

  const char* scanState = "idle";
  if (scanThrottled) {
    scanState = "cooldown";
  } else if (scanRequested_) {
    if (scanResult == WIFI_SCAN_RUNNING) {
      scanState = "running";
    } else if (scanResult >= 0) {
      scanState = "ready";
    } else {
      scanState = "failed";
    }
  }

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json; charset=utf-8", "");

  String header;
  header.reserve(220);
  header += F("{\"connected\":");
  header += connected ? F("true") : F("false");
  header += F(",\"mode\":\"");
  header += accessPointMode_ ? F("setup") : F("station");
  header += F("\",\"ssid\":\"");
  header += escapeJson(networkName);
  header += F("\",\"setupSsid\":\"");
  header += escapeJson(setupNetworkName);
  header += F("\",\"ip\":\"");
  header += escapeJson(ipAddress);
  header += F("\",\"rssi\":");
  header += String(signalStrength);
  header += F(",\"scanState\":\"");
  header += scanState;
  header += F("\",\"retryAfterMs\":");
  header += String(retryAfterMs);
  header += F(",\"networks\":[");
  server_.sendContent(header);

  bool firstNetwork = true;
  int16_t emittedNetworks = 0;
  if (scanRequested_ && scanResult >= 0) {
    for (int16_t i = 0; i < scanResult; ++i) {
      if (emittedNetworks >= kMaxWifiScanResults) break;
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;

      bool duplicate = false;
      for (int16_t earlier = 0; earlier < i; ++earlier) {
        if (WiFi.SSID(earlier) == ssid) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      String network;
      network.reserve(ssid.length() + 64);
      if (!firstNetwork) network += ',';
      firstNetwork = false;
      network += F("{\"ssid\":\"");
      network += escapeJson(ssid);
      network += F("\",\"rssi\":");
      network += String(WiFi.RSSI(i));
      network += F(",\"secure\":");
      network += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? F("false")
                                                          : F("true");
      network += '}';
      server_.sendContent(network);
      ++emittedNetworks;
    }
  }

  server_.sendContent(F("]}"));
  server_.sendContent(String());

  if (scanRequested_ && scanResult != WIFI_SCAN_RUNNING) {
    lastScanFinishedAt_ = millis();
    WiFi.scanDelete();
    scanRequested_ = false;
  }
}

void WebPortal::handleSave() {
  settings_.wifiSsid = server_.arg("ssid");
  const String wifiPassword = server_.arg("wifiPassword");
  if (!wifiPassword.isEmpty()) {
    settings_.wifiPassword = wifiPassword;
  }
  settings_.brightness = constrain(server_.arg("brightness").toInt(), 20, 255);
  if (server_.hasArg("audioVolume")) settings_.audioVolume = constrain(server_.arg("audioVolume").toInt(), 0, 60);
  settings_.wordbookUrl = server_.arg("wordbookUrl");
  settings_.wordbookUrl.trim();
  if (!settings_.wordbookUrl.isEmpty() &&
      !settings_.wordbookUrl.startsWith("http://") &&
      !settings_.wordbookUrl.startsWith("https://")) {
    server_.send(400, "text/html; charset=utf-8",
                 "<!doctype html><meta charset='utf-8'><style>body{font-family:sans-serif;background:#07111f;color:#e8f2ff;padding:32px}a{color:#38e29d}</style>"
                 "<h1>URL 오류</h1><p>단어장 URL은 http:// 또는 https://로 시작해야 합니다.</p><p><a href='/'>돌아가기</a></p>");
    return;
  }

  if (!store_.save(settings_)) {
    server_.send(500, "text/plain; charset=utf-8", "Settings could not be saved.");
    return;
  }

  server_.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta charset='utf-8'><style>body{font-family:sans-serif;background:#07111f;color:#e8f2ff;padding:32px}a{color:#38e29d}</style>"
               "<h1>Saved</h1><p>WordMon will restart and apply the new settings.</p>"
               "<p>Reconnect to your home Wi-Fi if this page was opened from the setup network.</p>");
  restartAt_ = millis() + 1500;
}

void WebPortal::handleNotFound() {
  if (accessPointMode_) {
    server_.sendHeader("Location", "http://192.168.4.1/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(404, "text/plain", "Not found");
}

String WebPortal::escapeHtml(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    switch (input[i]) {
      case '&': output += F("&amp;"); break;
      case '<': output += F("&lt;"); break;
      case '>': output += F("&gt;"); break;
      case '"': output += F("&quot;"); break;
      case '\'': output += F("&#39;"); break;
      default: output += input[i]; break;
    }
  }
  return output;
}

String WebPortal::escapeJson(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const uint8_t character = static_cast<uint8_t>(input[i]);
    switch (character) {
      case '"': output += F("\\\""); break;
      case '\\': output += F("\\\\"); break;
      case '\b': output += F("\\b"); break;
      case '\f': output += F("\\f"); break;
      case '\n': output += F("\\n"); break;
      case '\r': output += F("\\r"); break;
      case '\t': output += F("\\t"); break;
      default:
        if (character < 0x20) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04X", character);
          output += escaped;
        } else {
          output += static_cast<char>(character);
        }
        break;
    }
  }
  return output;
}

}  // namespace wordmon
