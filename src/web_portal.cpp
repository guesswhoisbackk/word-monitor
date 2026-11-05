#include "web_portal.hpp"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>

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
:root{color-scheme:light;--bg:#f4f3ed;--card:#fff;--line:#dde4dc;--text:#182d30;--muted:#647371;--green:#216c55;--cyan:#216c55}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 system-ui,sans-serif}
main{max-width:600px;margin:auto;padding:22px}.hero{display:flex;justify-content:space-between;gap:20px;align-items:end;margin-bottom:18px}
h1{margin:0;font-size:28px}.tag{color:var(--green);font-weight:800}.muted{color:var(--muted)}
.card{background:var(--card);border:1px solid var(--line);border-radius:22px;padding:18px;margin:14px 0;box-shadow:none}
label{display:block;color:var(--muted);font-size:13px;margin:8px 0 4px}input{width:100%;background:#f7f9f4;color:var(--text);border:1px solid #ccd7cf;border-radius:9px;padding:11px;font-size:16px}
select{width:100%;background:#f7f9f4;color:var(--text);border:1px solid #ccd7cf;border-radius:9px;padding:11px;font-size:16px}
.warning{background:#392b13;border:1px solid #80652b;border-radius:10px;padding:12px;color:#ffdaa0}
button{width:100%;border:0;border-radius:11px;padding:15px;background:var(--green);color:#fff;font-weight:900;font-size:17px;cursor:pointer;margin-top:12px}
.wifi-status{margin:12px 0;padding:12px;border:1px solid var(--line);border-radius:10px;background:#f7f9f4;color:var(--muted)}
.wifi-status.connected{border-color:#20795b;color:var(--green)}.wifi-status.setup{border-color:#176b88;color:var(--cyan)}.wifi-actions{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:end;margin-bottom:8px}
.wifi-actions button{width:auto;margin:0;padding:11px 16px;font-size:14px}.wifi-actions button:disabled{opacity:.55;cursor:wait}.scan-note{min-height:22px;color:var(--muted);font-size:13px}
@media(max-width:640px){.wifi-actions{grid-template-columns:1fr}.wifi-actions button{width:100%}.hero{display:block}}
</style></head><body><main><div class="hero"><div><div class="tag">WORD / DAY</div><h1>매일 조금씩, 나만의 단어 시간.</h1></div><div class="muted">v)HTML";

const char kWifiStart[] PROGMEM = R"HTML(</div></div>
<p class="muted">Wi-Fi와 날씨 지역을 고르면 준비 끝. 설정은 이 기기에 저장됩니다.</p>
<form method="post" action="/save"><section class="card"><h2>Wi-Fi와 화면</h2>
<div id="wifiStatus" class="wifi-status">연결 상태 확인 중...</div>
<div class="wifi-actions"><div><label for="wifiNetworkSelect">주변 2.4GHz Wi-Fi</label><select id="wifiNetworkSelect"><option value="">검색 버튼을 눌러주세요</option></select></div><button type="button" id="wifiScanButton">Wi-Fi 검색</button></div>
<div id="wifiScanNote" class="scan-note">목록에 없어도 아래에서 직접 입력할 수 있습니다.</div>
<div class="grid"><div><label for="ssidInput">2.4GHz Wi-Fi 이름(SSID)</label><input id="ssidInput" name="ssid" required value=")HTML";

const char kWifiMiddle[] PROGMEM = R"HTML("></div><div><label>Wi-Fi 비밀번호</label><input type="password" name="wifiPassword" placeholder="같은 Wi-Fi라면 비워 두어도 돼요"></div></div>
<label>화면 밝기(20~255, 추천 220)</label><input type="number" min="20" max="255" name="brightness" value=")HTML";

const char kWordbookStart[] PROGMEM = R"HTML("></section><section class="card"><h2>단어 학습</h2><details><summary>단어장 주소 변경 · 기본 단어장이 준비되어 있어요</summary>
<p class="muted">기본 주소로 바로 시작할 수 있어요. 비우면 내장 샘플 단어를 보여드립니다.</p>
<label for="wordbookUrl">단어장 기본 URL</label><input id="wordbookUrl" name="wordbookUrl" placeholder="https://cdn.jsdelivr.net/gh/사용자명/저장소@main" value=")HTML";

const char kAudioStart[] PROGMEM = R"HTML("><label>발음 음량 (0: 음소거, 1~60, 처음에는 20 권장)</label><input type="number" name="audioVolume" min="0" max="60" value=")HTML";

const char kStudyStart[] PROGMEM = R"HTML("></details><label>하루 새 단어 (0~20, 처음에는 5개 권장)</label><input type="number" name="dailyNewLimit" min="0" max="20" value=")HTML";

const char kWeatherStart[] PROGMEM = R"HTML("><p class="muted">밀린 복습부터 보여드려요. 하루 5개부터 가볍게 시작해 보세요.</p></section><section class="card"><h2>오늘의 날씨</h2><label><input style="width:20px" type="checkbox" name="weatherEnabled" value="1")HTML";
const char kCityStart[] PROGMEM = R"HTML(> 날씨 표시하기</label><label>빠른 지역 선택</label><select id="cityPreset"><option value="">지역 선택</option><option value="37.6063,127.0928">서울 중랑구</option><option value="37.5665,126.9780">서울</option><option value="37.4563,126.7052">인천</option><option value="37.2636,127.0286">수원</option><option value="36.3504,127.3845">대전</option><option value="35.1796,129.0756">부산</option><option value="35.8714,128.6014">대구</option><option value="35.1595,126.8526">광주</option><option value="35.5384,129.3114">울산</option><option value="37.8813,127.7298">춘천</option><option value="37.7519,128.8761">강릉</option><option value="33.4996,126.5312">제주</option></select><details><summary>다른 도시 검색</summary><label>도시 이름</label><input id="cityQuery" placeholder="예: 성남 / Tokyo" maxlength="60"><button id="citySearch" type="button">도시 찾기</button><div id="cityResults" aria-live="polite"></div><p class="muted">검색은 휴대폰 인터넷 연결이 필요해요.</p></details><label>선택한 지역</label><input id="weatherCity" name="weatherCity" readonly value=")HTML";
const char kLatitude[] PROGMEM = R"HTML("><input type="hidden" id="weatherLatitude" name="weatherLatitude" value=")HTML";
const char kLongitude[] PROGMEM = R"HTML("><input type="hidden" id="weatherLongitude" name="weatherLongitude" value=")HTML";
const char kPageEnd[] PROGMEM = R"HTML("><p class="muted">아침 06~11시 · 점심 11~17시 · 저녁 17~24시<br>각 시간대 안의 최대 강수확률입니다. 우산 안내는 남은 시간의 예보를 사용합니다.<br>15분마다 자동 갱신 · <a href="https://open-meteo.com/" target="_blank" rel="noreferrer">날씨: Open-Meteo</a><br>30분 이상 갱신되지 않은 날씨에는 * 표시가 붙어요.</p></section><button type="submit">저장하고 시작하기</button><p class="muted">저장 후 기기가 다시 시작됩니다. 같은 Wi-Fi에서 설정 페이지를 다시 열 수 있어요.</p></form>
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
function chooseCity(name,lat,lon){document.getElementById('weatherCity').value=name;document.getElementById('weatherLatitude').value=lat;document.getElementById('weatherLongitude').value=lon}
document.getElementById('cityPreset').addEventListener('change',function(){if(this.value){const [lat,lon]=this.value.split(',');chooseCity(this.options[this.selectedIndex].textContent,lat,lon)}});
document.getElementById('citySearch').addEventListener('click',async function(){const out=document.getElementById('cityResults');const q=document.getElementById('cityQuery').value.trim();if(q.length<2){out.textContent='도시 이름을 두 글자 이상 입력해 주세요.';return}this.disabled=true;out.textContent='찾는 중...';try{const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&language=ko&name='+encodeURIComponent(q),{signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error();const data=await r.json();out.replaceChildren();if(!data.results?.length)out.textContent='찾은 도시가 없어요.';for(const city of data.results||[]){const b=document.createElement('button');b.type='button';b.textContent=[city.name,city.admin1,city.country].filter(Boolean).join(' · ');b.addEventListener('click',()=>{chooseCity(city.name,city.latitude,city.longitude);out.replaceChildren();document.getElementById('cityPreset').value=''});out.appendChild(b)}}catch{out.textContent='검색에 연결하지 못했어요. 휴대폰 인터넷을 확인하거나 빠른 지역을 선택해 주세요.'}finally{this.disabled=false}});
</script>
</main></body></html>)HTML";

}  // namespace

WebPortal::WebPortal(AppSettings& settings, SettingsStore& store, WeatherService& weather, Wordbook& wordbook, StudyStore& study)
    : settings_(settings), store_(store), weather_(weather), wordbook_(wordbook), study_(study) {}

void WebPortal::begin(bool accessPointMode) {
  accessPointMode_ = accessPointMode;
  if (accessPointMode_) {
    dns_.start(53, "*", WiFi.softAPIP());
  }

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/wifi", HTTP_GET, [this]() { handleWifiApi(); });
  server_.on("/api/learning", HTTP_GET, [this]() {
    JsonDocument doc;
    doc["word"]=wordbook_.card()?wordbook_.card()->word:"";
    doc["words"]=wordbook_.wordCount();
    doc["newToday"]=study_.newToday(static_cast<uint32_t>(time(nullptr)));
    doc["dailyLimit"]=settings_.dailyNewLimit;
    doc["due"]=wordbook_.dueCount();
    doc["cacheReady"]=wordbook_.cachedAssets(); doc["cachePlanned"]=wordbook_.plannedAssets();
    doc["downloading"]=wordbook_.busy();
    doc["hasArt"]=wordbook_.card() && wordbook_.card()->art;
    String body;serializeJson(doc,body);server_.sendHeader("Cache-Control","no-store");server_.send(200,"application/json",body);
  });
  server_.on("/api/weather", HTTP_GET, [this]() {
    JsonDocument doc;
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["enabled"] = settings_.weatherEnabled;
    doc["busy"] = weather_.busy();
    doc["valid"] = weather_.reading().valid;
    doc["attempts"] = weather_.attempts();
    doc["updates"] = weather_.updates();
    doc["lastHttpStatus"] = weather_.lastHttpStatus();
    doc["lastAttemptOk"] = weather_.lastAttemptOk();
    doc["freeHeap"] = ESP.getFreeHeap();
    if (weather_.reading().valid) doc["ageSeconds"] = uint32_t(millis() - weather_.reading().receivedAt) / 1000;
    else doc["ageSeconds"] = nullptr;
    doc["nextAttemptSeconds"] = weather_.nextAttemptInMs() / 1000;
    String body; serializeJson(doc, body);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", body);
  });
  server_.on("/api/weather/refresh", HTTP_POST, [this]() {
    weather_.requestRefresh();
    server_.send(202, "application/json", "{\"queued\":true}");
  });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  Serial.printf("[web] config portal: http://%s/\n",
                accessPointMode_ ? WiFi.softAPIP().toString().c_str()
                                 : WiFi.localIP().toString().c_str());
}

void WebPortal::setAccessPointMode(bool mode) {
  if(mode==accessPointMode_) return;
  accessPointMode_=mode;
  if(mode) dns_.start(53,"*",WiFi.softAPIP()); else dns_.stop();
}

void WebPortal::loop() {
  // USB diagnostic: exercise the real HTTP handler while the main loop keeps
  // serving it. Print only status/size, never saved Wi-Fi settings.
  if (Serial.available() && Serial.peek() == '?') {
    Serial.read();
    xTaskCreate([](void* context) {
      auto* self = static_cast<WebPortal*>(context);
      {
        WiFiClient client;
        HTTPClient http;
        const String ip = self->accessPointMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        http.setConnectTimeout(3000); http.setTimeout(5000);
        if (http.begin(client, "http://" + ip + "/")) {
          const int code = http.GET();
          const String page = code == 200 ? http.getString() : String();
          Serial.printf("[web] self-test HTTP=%d bytes=%u complete=%d stations=%u\n", code,
              static_cast<unsigned>(page.length()), page.endsWith("</html>"), WiFi.softAPgetStationNum());
          http.end();
        }
      }
      vTaskDelete(nullptr);
    }, "web-test", 4096, this, 1, nullptr);
  }
  if (accessPointMode_) {
    dns_.processNextRequest();
  }
  server_.handleClient();
  if (restartAt_ != 0 && static_cast<int32_t>(millis() - restartAt_) >= 0) {
    ESP.restart();
  }
}

void WebPortal::handleRoot() {
  Serial.printf("[web] GET / from %s heap=%u\n", server_.client().remoteIP().toString().c_str(), ESP.getFreeHeap());
  server_.sendHeader("Cache-Control", "no-store");
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
  server_.sendContent_P(kStudyStart);
  server_.sendContent(String(settings_.dailyNewLimit));
  server_.sendContent_P(kWeatherStart);
  if (settings_.weatherEnabled) server_.sendContent(F(" checked"));
  server_.sendContent_P(kCityStart);
  const String city = escapeHtml(settings_.weatherCity);
  if (!city.isEmpty()) server_.sendContent(city);
  server_.sendContent_P(kLatitude);
  server_.sendContent(String(settings_.weatherLatitude, 5));
  server_.sendContent_P(kLongitude);
  server_.sendContent(String(settings_.weatherLongitude, 5));
  server_.sendContent_P(kPageEnd);
  server_.sendContent(String());
  Serial.println("[web] setup page sent");
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
  AppSettings candidate = settings_;
  candidate.wifiSsid = server_.arg("ssid");
  const String wifiPassword = server_.arg("wifiPassword");
  if (!wifiPassword.isEmpty()) {
    candidate.wifiPassword = wifiPassword;
  }
  candidate.brightness = constrain(server_.arg("brightness").toInt(), 20, 255);
  if (server_.hasArg("audioVolume")) candidate.audioVolume = constrain(server_.arg("audioVolume").toInt(), 0, 60);
  if (server_.hasArg("dailyNewLimit")) candidate.dailyNewLimit = constrain(server_.arg("dailyNewLimit").toInt(), 0, 20);
  candidate.wordbookUrl = server_.arg("wordbookUrl");
  candidate.wordbookUrl.trim();
  if (!candidate.wordbookUrl.isEmpty() &&
      !candidate.wordbookUrl.startsWith("http://") &&
      !candidate.wordbookUrl.startsWith("https://")) {
    server_.send(400, "text/html; charset=utf-8",
                 "<!doctype html><meta charset='utf-8'><style>body{font-family:sans-serif;background:#07111f;color:#e8f2ff;padding:32px}a{color:#38e29d}</style>"
                 "<h1>URL 오류</h1><p>단어장 URL은 http:// 또는 https://로 시작해야 합니다.</p><p><a href='/'>돌아가기</a></p>");
    return;
  }

  candidate.weatherEnabled = server_.hasArg("weatherEnabled");
  candidate.weatherCity = server_.arg("weatherCity");
  const String latText = server_.arg("weatherLatitude"), lonText = server_.arg("weatherLongitude");
  char* latEnd = nullptr; char* lonEnd = nullptr;
  candidate.weatherLatitude = strtof(latText.c_str(), &latEnd);
  candidate.weatherLongitude = strtof(lonText.c_str(), &lonEnd);
  if (candidate.wifiSsid.isEmpty() || candidate.wifiSsid.length() > 32 || candidate.wifiPassword.length() > 63 ||
      candidate.weatherCity.isEmpty() || candidate.weatherCity.length() > 80 || candidate.wordbookUrl.length() > 250 ||
      latEnd == latText.c_str() || *latEnd || lonEnd == lonText.c_str() || *lonEnd ||
      !isfinite(candidate.weatherLatitude) || !isfinite(candidate.weatherLongitude) ||
      candidate.weatherLatitude < -90 || candidate.weatherLatitude > 90 || candidate.weatherLongitude < -180 || candidate.weatherLongitude > 180 ||
      (candidate.wifiSsid != settings_.wifiSsid && wifiPassword.isEmpty())) {
    server_.send(400, "text/plain; charset=utf-8", "Wi-Fi 이름·비밀번호와 날씨 지역을 확인해 주세요. 새 Wi-Fi에는 비밀번호가 필요합니다."); return;
  }
  if (!store_.save(candidate)) {
    server_.send(500, "text/plain; charset=utf-8", "Settings could not be saved.");
    return;
  }

  settings_ = candidate;
  server_.send(200, "text/html; charset=utf-8",
               "<!doctype html><meta charset='utf-8'><style>body{font-family:sans-serif;background:#07111f;color:#e8f2ff;padding:32px}a{color:#38e29d}</style>"
               "<h1>준비됐어요!</h1><p>기기가 다시 시작됩니다. 잠시 후 새 날씨와 단어를 만나보세요.</p>"
               "<p>휴대폰도 집 Wi-Fi로 다시 연결해 주세요.</p>");
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
