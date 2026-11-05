#include "word_ui.hpp"

#include <cctype>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <new>
#include <WiFi.h>
#include <src/misc/cache/instance/lv_image_cache.h>

#include "app_config.hpp"
#include "assets/apple_art.hpp"

// Paperlogy: medium Korean body, regular small captions, semibold word/numerals.
extern "C" const lv_font_t font_kr_16;
extern "C" const lv_font_t font_kr_13;
extern "C" const lv_font_t font_ui_12;
extern "C" const lv_font_t font_ui_18;
extern "C" const lv_font_t font_ui_24;

namespace wordmon {

namespace {

constexpr uint32_t kBackground = 0x07111F;
constexpr uint32_t kCard = 0x0D1B2D;
constexpr uint32_t kBorder = 0x1D3550;
constexpr uint32_t kText = 0xF4F0E6;
constexpr uint32_t kMuted = 0x8BA3BA;
constexpr uint32_t kGreen = 0xBCE8C8;
constexpr uint32_t kCyan = 0xA9D9D0;

constexpr int kArtY = 5;
constexpr int kArtSlotHeight = 88;
constexpr int kArtSlotWidth = 120;

// Offline sample pack; it uses the same review history as hosted words.
struct SampleWord {
  const char* word;
  const char* meaning;
  const lv_image_dsc_t* art;  // optional front-face illustration
};

lv_image_dsc_t makeArtDsc(const uint8_t* data, uint16_t width, uint16_t height) {
  lv_image_dsc_t dsc{};
  dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
  dsc.header.w = width;
  dsc.header.h = height;
  dsc.data_size = width * height * 4;
  dsc.data = data;
  return dsc;
}

lv_image_dsc_t appleArt = makeArtDsc(art::kApple, art::kAppleW, art::kAppleH);

const SampleWord kSampleWords[] = {
    {"resilient", "회복력이 있는", nullptr},
    {"diligent", "부지런한", nullptr},
    {"curious", "호기심이 많은", nullptr},
    {"vivid", "생생한", nullptr},
    {"apple", "사과", &appleArt},
    {"frugal", "검소한", nullptr},
    {"serene", "평온한", nullptr},
    {"profound", "뜻이 깊은", nullptr},
};
constexpr size_t kSampleWordCount = sizeof(kSampleWords) / sizeof(kSampleWords[0]);

constexpr const char* kMonthNames[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current == nullptr || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
  }
}

void setLabelColorIfChanged(lv_obj_t* label, uint32_t color) {
  const lv_color_t target = lv_color_hex(color);
  if (!lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), target)) {
    lv_obj_set_style_text_color(label, target, 0);
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int x, int y,
                    int width, const lv_font_t* font, uint32_t color,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  // Every label here is a single-line element. A fixed height prevents long
  // text from wrapping into the row below before LONG_DOT is applied.
  lv_obj_set_size(label, width, font->line_height + 4);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  return label;
}

lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int width, int height) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, width, height);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(kBorder), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

}  // namespace

WordUi::WordUi(AppSettings& settings, StudyStore& study, AudioPlayer& audio)
    : settings_(settings), study_(study), audio_(audio) {}

void WordUi::begin() {
  display_.init();
  display_.setRotation(kDisplayRotation);
  display_.setBrightness(settings_.brightness);
  display_.fillScreen(0x0000);

  touchSpi_.begin(kTouchSclk, kTouchMiso, kTouchMosi, kTouchCs);
  touch_.begin(touchSpi_);
  touch_.setRotation(kTouchRotation);

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Allocate the partial framebuffer at runtime. Keeping it out of static BSS
  // leaves enough linked DRAM for the ESP32 Wi-Fi stack.
  drawBuffer_ = new (std::nothrow) uint16_t[kDrawBufferPixels];
  if (drawBuffer_ == nullptr) {
    Serial.println("[ui] framebuffer allocation failed");
    ESP.restart();
  }

  lvDisplay_ = lv_display_create(kScreenWidth, kScreenHeight);
  lv_display_set_user_data(lvDisplay_, this);
  lv_display_set_color_format(lvDisplay_, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvDisplay_, flushDisplay);
  lv_display_set_buffers(lvDisplay_, drawBuffer_, nullptr,
                         kDrawBufferPixels * sizeof(uint16_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lvInput_ = lv_indev_create();
  lv_indev_set_type(lvInput_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_user_data(lvInput_, this);
  lv_indev_set_read_cb(lvInput_, readTouch);

  createLayout();
  refresh();
  Serial.printf("[ui] word card ready rotation=%u %ux%u words=%u\n",
                kDisplayRotation, kScreenWidth, kScreenHeight,
                static_cast<unsigned>(kSampleWordCount));
}

void WordUi::loop() {
  if (Serial.available()) {
    const char command = Serial.read();
    if (command == 's') showSettings();
    if (command == 'r') { meaningVisible_ = !meaningVisible_; refresh(); }
    if (command == 'f') { meaningVisible_ = false; refresh(); }
    // USB-only loading preview; does not grade or request another card.
    if (command == 'd') { reviewLoading_ = true; refresh(); }
    if (command == 'u') { reviewLoading_ = false; refresh(); }
    if (command == 'n') lv_obj_send_event(nextButton_, LV_EVENT_CLICKED, nullptr);
    if (command == 'b' && settingsOverlay_) { lv_obj_delete(settingsOverlay_); settingsOverlay_ = nullptr; }
    if (command == '!') {
      capture_ = true;
      lv_obj_invalidate(lv_screen_active());
      lv_refr_now(lvDisplay_);
      capture_ = false;
      Serial.println("WMEND");
    }
  }
  lv_timer_handler();
  const uint32_t now = millis();
  if (now - lastRefresh_ >= 500) {
    lastRefresh_ = now;
    refresh();
  }
}

void WordUi::setNetworkInfo(bool accessPointMode, const String& ipAddress) {
  accessPointMode_ = accessPointMode;
  ipAddress_ = ipAddress;
}

void WordUi::createLayout() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  clockLabel_ = makeLabel(screen, "--:--", 10, 5, 76, &font_ui_24, kText);
  lv_obj_t* setup = lv_button_create(screen);
  lv_obj_set_pos(setup, 198, 3); lv_obj_set_size(setup, 34, 32);
  lv_obj_set_style_bg_color(setup, lv_color_hex(kBorder), 0);
  lv_obj_t* gear = lv_label_create(setup); lv_label_set_text(gear, LV_SYMBOL_SETTINGS); lv_obj_center(gear);
  lv_obj_add_event_cb(setup, [](lv_event_t* e) { static_cast<WordUi*>(lv_event_get_user_data(e))->showSettings(); }, LV_EVENT_CLICKED, this);
  progressLabel_ = makeLabel(screen, "0/5", 86, 13, 38, &font_ui_12, kGreen, LV_TEXT_ALIGN_RIGHT);
  lv_obj_t* weatherCard = makeCard(screen, 8, 38, 224, 112);
  lv_obj_add_flag(weatherCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(weatherCard, [](lv_event_t* event) {
    static_cast<WordUi*>(lv_event_get_user_data(event))->weatherRefreshRequested_ = true;
  }, LV_EVENT_CLICKED, this);
  weatherTemperature_ = makeLabel(weatherCard, "--°", 10, 7, 72, &font_ui_24, kGreen);
  weatherDetail_ = makeLabel(weatherCard, "날씨 연결 중", 86, 5, 130, &font_kr_16, kText);
  weatherRain_ = makeLabel(weatherCard, "잠시 기다려요", 86, 27, 130, &font_kr_13, kMuted);
  auto* divider = lv_obj_create(weatherCard);
  lv_obj_set_pos(divider, 10, 48); lv_obj_set_size(divider, 204, 1);
  lv_obj_set_style_bg_color(divider, lv_color_hex(kBorder), 0);
  lv_obj_set_style_border_width(divider, 0, 0);
  lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE);
  morningRain_ = makeLabel(weatherCard, "아침 --", 8, 51, 68, &font_kr_13, kMuted);
  noonRain_ = makeLabel(weatherCard, "점심 --", 78, 51, 68, &font_kr_13, kMuted);
  eveningRain_ = makeLabel(weatherCard, "저녁 --", 148, 51, 68, &font_kr_13, kMuted);
  clothingLabel_ = makeLabel(weatherCard, "날씨를 기다리고 있어요", 10, 72, 204, &font_kr_16, kGreen);
  accessoryLabel_ = makeLabel(weatherCard, "", 10, 93, 204, &font_kr_13, kMuted);
  statusLabel_ = makeLabel(screen, "STARTING", 104, 10, 62,
                           &font_ui_12, kMuted,
                           LV_TEXT_ALIGN_RIGHT);
  dateLabel_ = makeLabel(screen, "TIME WAITING FOR WI-FI", 8, 34, 224,
                         &font_ui_12, kMuted);

  lv_obj_add_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(dateLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t* card = makeCard(screen, 8, 158, 224, 98);
  wordLabel_ = makeLabel(card, "--", 132, 37, 84, &font_ui_18,
                         kText, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(wordLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_line_space(wordLabel_, 2, 0);
  // Flashcard front face: illustration art, or a big first letter when the
  // word has no art yet. The meaning/example rows are back-face content.
  artImage_ = lv_image_create(card);
  lv_obj_add_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  letterLabel_ = makeLabel(card, "A", 8, 31, 120, &font_ui_24,
                           kBorder, LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  answerPanel_ = lv_obj_create(card);
  lv_obj_set_pos(answerPanel_, 132, 10);
  lv_obj_set_size(answerPanel_, 84, 78);
  lv_obj_set_style_bg_opa(answerPanel_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(answerPanel_, 0, 0);
  lv_obj_set_style_pad_all(answerPanel_, 2, 0);
  lv_obj_set_style_pad_row(answerPanel_, 5, 0);
  lv_obj_add_flag(answerPanel_, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_flex_flow(answerPanel_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(answerPanel_, LV_DIR_VER);
  meaningLabel_ = makeLabel(answerPanel_, "", 0, 0, 80,
                            &font_kr_16, kCyan,
                            LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(meaningLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(meaningLabel_, LV_SIZE_CONTENT);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, cardEvent, LV_EVENT_CLICKED, this);

  studyLabel_ = makeLabel(screen, "RECALL IT, THEN RATE", 8, 257, 224,
                         &font_ui_12, kMuted, LV_TEXT_ALIGN_CENTER);
  auto button = [&](const char* text, int x, int y, int width) {
    lv_obj_t* obj = lv_button_create(screen);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, 42);
    lv_obj_set_style_bg_color(obj, lv_color_hex(kBorder), 0);
    const lv_style_selector_t disabled = LV_PART_MAIN | LV_STATE_DISABLED;
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x09131D), disabled);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, disabled);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, disabled);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x354857), disabled);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, disabled);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x142331), disabled);
    lv_obj_set_style_border_width(obj, 1, disabled);
    lv_obj_set_style_shadow_width(obj, 0, disabled);
    // The default theme recolors disabled widgets light grey. Suppress that
    // overlay so the explicit dark background is the color actually drawn.
    lv_obj_set_style_recolor_opa(obj, LV_OPA_TRANSP, disabled);
    lv_obj_t* label = lv_label_create(obj);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_ui_12, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(obj, studyEvent, LV_EVENT_CLICKED, this);
    return obj;
  };
  againButton_ = button("AGAIN", 8, 274, 70);
  goodButton_ = button("GOT IT", 85, 274, 70);
  nextButton_ = button("NEXT", 162, 274, 70);
  hintButton_ = button("HINT", 176, 3, 56);
  lv_obj_set_height(hintButton_, 28);
  // Art is always on the front face now; the hint reveal button stays hidden
  // (kept wired so a future hide-art study mode can re-enable it).
  lv_obj_add_flag(hintButton_, LV_OBJ_FLAG_HIDDEN);
  speakButton_ = button("SPEAK", 130, 3, 62);
  lv_obj_set_height(speakButton_, 28);
  speakLabel_ = lv_obj_get_child(speakButton_, 0);

  lv_obj_set_style_bg_color(goodButton_, lv_color_hex(kGreen), 0);
  lv_obj_set_style_text_color(goodButton_, lv_color_hex(kBackground), 0);
  footerLabel_ = makeLabel(screen, "Starting...", 8, 302, 224,
                           &font_ui_12, kMuted,
                           LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(footerLabel_, LV_OBJ_FLAG_HIDDEN);
}

void WordUi::showSettings() {
  if (settingsOverlay_) return;
  settingsOverlay_ = makeCard(lv_screen_active(), 4, 4, 232, 312);
  makeLabel(settingsOverlay_, "PHONE SETUP", 12, 10, 200, &font_ui_18, kText);
  const String url = "http://" + ipAddress_ + "/";
  lv_obj_t* qr = lv_qrcode_create(settingsOverlay_);
  lv_qrcode_set_size(qr, 116);
  lv_qrcode_set_dark_color(qr, lv_color_hex(0x07111F));
  lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
  lv_qrcode_set_quiet_zone(qr, true);
  lv_qrcode_set_data(qr, url.c_str()); lv_obj_set_pos(qr, 58, 44);
  const String instructions = accessPointMode_ ? String("Join ") + WiFi.softAPSSID() + "\nPassword: wordmon1" : "Use the same Wi-Fi as this device";
  auto* info = makeLabel(settingsOverlay_, instructions.c_str(), 10, 172, 210, &font_ui_12, kText, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP); lv_obj_set_height(info, 38);
  makeLabel(settingsOverlay_, url.c_str(), 8, 216, 216, &font_ui_12, kGreen, LV_TEXT_ALIGN_CENTER);
  makeLabel(settingsOverlay_, wordbookSuffix(), 8, 239, 216, &font_ui_12, kMuted, LV_TEXT_ALIGN_CENTER);
  auto* close = lv_button_create(settingsOverlay_); lv_obj_set_pos(close, 16, 264); lv_obj_set_size(close, 198, 36);
  auto* text = lv_label_create(close); lv_label_set_text(text, "BACK TO WORDS"); lv_obj_center(text);
  lv_obj_add_event_cb(close, [](lv_event_t* e) {
    auto* self = static_cast<WordUi*>(lv_event_get_user_data(e));
    lv_obj_delete(self->settingsOverlay_); self->settingsOverlay_ = nullptr;
  }, LV_EVENT_CLICKED, this);
}

void WordUi::setWordbookCard(const WordbookCard* card, WordbookState state,
                             uint16_t wordCount) {
  externalCard_ = card;
  // The wordbook reuses the same image descriptor and pixel buffer.
  currentArt_ = nullptr;
  if (card && card->art) lv_image_cache_drop(card->art);
  wordbookState_ = state;
  wordbookCount_ = wordCount;
  refresh();
}

void WordUi::refresh() {
  char weatherText[160];
  auto periodLabel = [&](lv_obj_t* label, const char* name, int probability) {
    if (probability >= 0) snprintf(weatherText, sizeof(weatherText), "%s %d%%", name, probability);
    else snprintf(weatherText, sizeof(weatherText), "%s --", name);
    setLabelTextIfChanged(label, weatherText);
  };
  periodLabel(morningRain_, "아침", weather_.valid && settings_.weatherEnabled ? weather_.rain.morning : -1);
  periodLabel(noonRain_, "점심", weather_.valid && settings_.weatherEnabled ? weather_.rain.noon : -1);
  periodLabel(eveningRain_, "저녁", weather_.valid && settings_.weatherEnabled ? weather_.rain.evening : -1);
  if (!settings_.weatherEnabled) {
    setLabelTextIfChanged(weatherTemperature_, "--");
    setLabelTextIfChanged(weatherDetail_, "날씨 꺼짐");
    setLabelTextIfChanged(weatherRain_, "");
    setLabelTextIfChanged(clothingLabel_, "설정에서 날씨 켜기");
    setLabelTextIfChanged(accessoryLabel_, "");
  } else if (weather_.valid) {
    snprintf(weatherText, sizeof(weatherText), "%.0f°", weather_.temperature);
    setLabelTextIfChanged(weatherTemperature_, weatherText);
    const bool stale = weatherStale(millis(), weather_.receivedAt);
    snprintf(weatherText, sizeof(weatherText), "%s%s", weatherDescription(weather_.code), stale ? " *" : "");
    setLabelTextIfChanged(weatherDetail_, weatherText);
    if (weather_.hasPrecipitation) snprintf(weatherText, sizeof(weatherText), "현재 강수 %.1fmm", weather_.precipitationMm);
    else snprintf(weatherText, sizeof(weatherText), "현재 강수 --");
    setLabelTextIfChanged(weatherRain_, weatherText);
    if (stale) {
      setLabelTextIfChanged(clothingLabel_, WiFi.status() == WL_CONNECTED ? "날씨 갱신 재시도 중" : "Wi-Fi 다시 연결 중");
      snprintf(weatherText, sizeof(weatherText), "%u분 전 갱신 · 재시도 중",
               unsigned(uint32_t(millis() - weather_.receivedAt) / 60000));
      setLabelTextIfChanged(accessoryLabel_, weatherText);
    } else {
      const float comfort = weather_.hasApparentTemperature ? weather_.apparentTemperature : weather_.temperature;
      snprintf(weatherText, sizeof(weatherText), "%s", clothingSuggestion(comfort));
      setLabelTextIfChanged(clothingLabel_, weatherText);
      snprintf(weatherText, sizeof(weatherText), "%s %.0f° · %s", weather_.hasApparentTemperature ? "체감" : "기온",
               comfort, weatherAccessory(weather_.code, weather_.rain.remaining, comfort));
      setLabelTextIfChanged(accessoryLabel_, weatherText);
    }
  } else {
    setLabelTextIfChanged(weatherDetail_, "날씨 연결 중");
    setLabelTextIfChanged(weatherRain_, "잠시 기다려요");
    setLabelTextIfChanged(clothingLabel_, accessPointMode_ ? "먼저 Wi-Fi를 연결해 주세요" : "날씨를 기다리고 있어요");
    setLabelTextIfChanged(accessoryLabel_, "");
  }
  if (weatherBusy_) {
    setLabelTextIfChanged(accessoryLabel_, "날씨 새로 받는 중");
    lv_obj_add_state(speakButton_, LV_STATE_DISABLED);
  }
  else lv_obj_remove_state(speakButton_, LV_STATE_DISABLED);
  if (ipAddress_.isEmpty()) {
    setLabelTextIfChanged(statusLabel_, "STARTING");
  } else if (accessPointMode_) {
    setLabelTextIfChanged(statusLabel_, "SETUP AP");
  } else {
    setLabelTextIfChanged(statusLabel_, "WI-FI");
  }
  setLabelColorIfChanged(
      statusLabel_,
      ipAddress_.isEmpty() || accessPointMode_ ? (accessPointMode_ ? kCyan : kMuted)
                                               : kGreen);

  // KST dates reset the daily quota, not a word's position in the list.
  // Until the clock is valid show a stable preview without grading.
  const time_t now = time(nullptr);
  const bool timeValid = now > 1700000000;
  struct tm local {};
  size_t index = 0;
  if (timeValid) {
    localtime_r(&now, &local);
    const int32_t day = studyDay(static_cast<uint32_t>(now));
    if (sampleDay_ != day) {
      sampleDay_ = day;
      sampleIndex_ = -1;
      meaningVisible_ = false;
      hintVisible_ = false;
      graded_ = false;
      studyMessage_ = "";
      ReviewChoice choice;
      const bool allowNew = study_.allowsNew(static_cast<uint32_t>(now), settings_.dailyNewLimit);
      for (size_t i = 0; i < kSampleWordCount; ++i)
        choice.consider(static_cast<uint16_t>(i), allowNew,
                        study_.find(kSampleWords[i].word), static_cast<uint32_t>(now));
      sampleIndex_ = choice.index();
    }
    char clockText[16];
    snprintf(clockText, sizeof(clockText), "%02d:%02d", local.tm_hour, local.tm_min);
    setLabelTextIfChanged(clockLabel_, clockText);
    char dateText[32];
    snprintf(dateText, sizeof(dateText), "%s %02d - NEW %u/%u",
             kMonthNames[local.tm_mon], local.tm_mday,
             study_.newToday(static_cast<uint32_t>(now)), settings_.dailyNewLimit);
    setLabelTextIfChanged(dateLabel_, dateText);
    char progress[32];
    snprintf(progress, sizeof(progress), "%u/%u", study_.newToday(static_cast<uint32_t>(now)), settings_.dailyNewLimit);
    setLabelTextIfChanged(progressLabel_, progress);
  } else {
    setLabelTextIfChanged(dateLabel_, "TIME WAITING FOR WI-FI");
  }
  if (sampleIndex_ >= 0) index = static_cast<size_t>(sampleIndex_);

  // The network wordbook wins over the built-in sample pack whenever a card
  // was selected from it (fetched or cached); nullptr keeps the samples.
  const char* word = nullptr;
  const char* meaning = nullptr;
  const lv_image_dsc_t* art = nullptr;
  if (externalCard_ != nullptr) {
    word = externalCard_->word;
    meaning = externalCard_->meaning;
    art = externalCard_->art;
  } else {
    const SampleWord& entry = kSampleWords[index];
    word = entry.word;
    meaning = entry.meaning;
    art = entry.art;
  }
  const bool wordChanged = shownWord_ != word;
  if (wordChanged) {
    audio_.cancel();
    audioFeedback_ = false;
    shownWord_ = word;
    meaningVisible_ = false;
    hintVisible_ = false;
    graded_ = false;
    studyMessage_ = "";
    currentArt_ = nullptr;
    lv_obj_scroll_to_y(answerPanel_, 0, LV_ANIM_OFF);
  }
  if (wordChanged) {
    // Fixed right column, up to three stationary lines, vertically centered.
    // Dots are only a fallback for content exceeding the three-line area.
    lv_point_t size;
    const lv_font_t* font = &font_ui_18;
    for (const lv_font_t* candidate : {&font_ui_18, &font_kr_16, &font_ui_12}) {
      font = candidate;
      lv_text_get_size(&size, word, font, 0, 2, 84, LV_TEXT_FLAG_NONE);
      if (size.y <= font->line_height * 3 + 4) break;
    }
    lv_obj_set_style_text_font(wordLabel_, font, 0);
    lv_label_set_text(wordLabel_, word);
    const int height = std::min(int(size.y), int(font->line_height * 3 + 4));
    lv_obj_set_height(wordLabel_, height);
    lv_obj_set_y(wordLabel_, (98 - height) / 2);
  }
  setLabelTextIfChanged(speakLabel_, audio_.busy() ? "STOP" : "SPEAK");

  // The illustrated wordbook is used as picture cards: art shows on the
  // left side at all times. Tapping replaces the word with only its Korean meaning on the right.
  const bool front = !meaningVisible_;
  const bool showArt = art != nullptr;
  if (showArt && currentArt_ != art) {
    currentArt_ = art;
    lv_image_set_src(artImage_, art);
    const uint32_t scale = std::min(uint32_t(256), std::min(
        uint32_t(kArtSlotWidth * 256) / art->header.w,
        uint32_t(kArtSlotHeight * 256) / art->header.h));
    lv_image_set_pivot(artImage_, 0, 0);
    lv_image_set_scale(artImage_, scale);
    const int renderedWidth = art->header.w * scale / 256;
    const int renderedHeight = art->header.h * scale / 256;
    lv_obj_set_pos(artImage_, 8 + (kArtSlotWidth - renderedWidth) / 2,
                   kArtY + (kArtSlotHeight - renderedHeight) / 2);
    Serial.printf("[ui] show art %ux%u px0=%04x\n",
                  static_cast<unsigned>(art->header.w),
                  static_cast<unsigned>(art->header.h),
                  *reinterpret_cast<const uint16_t*>(art->data));
  }
  if (showArt) {
    lv_obj_clear_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  }

  if (!showArt) {
    char letter[2] = {
        static_cast<char>(toupper(static_cast<unsigned char>(word[0]))),
        '\0'};
    setLabelTextIfChanged(letterLabel_, letter);
    lv_obj_clear_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  }

  const ReviewRecord* progress = study_.find(word);
  const bool waiting = progress && !reviewDue(*progress, static_cast<uint32_t>(now));
  const bool eligible = study_.ready() && (progress ? !waiting :
      study_.allowsNew(static_cast<uint32_t>(now), settings_.dailyNewLimit));
  // Grading works from either face (picture decks get rated on sight); the
  // reveal is still one tap away for checking the meaning/example.
  const bool canGrade = !reviewLoading_ && !graded_ && timeValid && eligible;
  for (lv_obj_t* button : {againButton_, goodButton_}) {
    if (canGrade) lv_obj_remove_state(button, LV_STATE_DISABLED);
    else lv_obj_add_state(button, LV_STATE_DISABLED);
  }
  if (reviewLoading_) lv_obj_add_state(nextButton_, LV_STATE_DISABLED);
  else lv_obj_remove_state(nextButton_, LV_STATE_DISABLED);
  const char* message = reviewLoading_ ? "LOADING..." : !timeValid ? "CONNECT WI-FI TO SAVE REVIEWS" :
      !study_.ready() ? "STORAGE ERROR - CANNOT SAVE" :
      !studyMessage_.isEmpty() ? studyMessage_.c_str() :
      waiting ? "SAVED - TAP NEXT FOR MORE" :
      !eligible ? "DAILY LIMIT - TAP NEXT" :
      meaningVisible_ ? "SAY IT ALOUD, THEN RATE" : "RECALL IT, THEN RATE";
  setLabelTextIfChanged(studyLabel_, message);
  if (audioFeedback_) setLabelTextIfChanged(studyLabel_, audio_.message());

  if (front) lv_obj_clear_flag(wordLabel_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(wordLabel_, LV_OBJ_FLAG_HIDDEN);

  if (meaningVisible_) {
    lv_obj_remove_flag(answerPanel_, LV_OBJ_FLAG_HIDDEN);
    setLabelTextIfChanged(meaningLabel_, meaning);
    lv_obj_update_layout(meaningLabel_);
    const bool overflows = lv_obj_get_height(meaningLabel_) > lv_obj_get_content_height(answerPanel_);
    lv_obj_set_flex_align(answerPanel_, overflows ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  } else {
    lv_obj_add_flag(answerPanel_, LV_OBJ_FLAG_HIDDEN);
    setLabelTextIfChanged(meaningLabel_, "");
  }

  if (ipAddress_.isEmpty()) {
    setLabelTextIfChanged(footerLabel_, "Wi-Fi STARTING");
  } else if (accessPointMode_) {
    char footer[64];
    snprintf(footer, sizeof(footer), "AP %s%s", ipAddress_.c_str(),
             wordbookSuffix());
    setLabelTextIfChanged(footerLabel_, footer);
  } else {
    char footer[64];
    snprintf(footer, sizeof(footer), "Wi-Fi %s%s", ipAddress_.c_str(),
             wordbookSuffix());
    setLabelTextIfChanged(footerLabel_, footer);
  }
}

const char* WordUi::wordbookSuffix() const {
  switch (wordbookState_) {
    case WordbookState::Disabled:
      return "";
    case WordbookState::Syncing:
      return " - SYNC";
    case WordbookState::Ok: {
      // Static buffer is safe: refresh() runs on the single Arduino loop.
      static char suffix[20];
      snprintf(suffix, sizeof(suffix), " - WB %u",
               static_cast<unsigned>(wordbookCount_));
      return suffix;
    }
    case WordbookState::Cached:
      return " - WB CACHED";
    case WordbookState::Failed:
      return " - WB OFF";
  }
  return "";
}

void WordUi::flushDisplay(lv_display_t* display, const lv_area_t* area,
                          uint8_t* pixels) {
  auto* self = static_cast<WordUi*>(lv_display_get_user_data(display));
  const uint32_t width = area->x2 - area->x1 + 1;
  const uint32_t height = area->y2 - area->y1 + 1;
  lv_draw_sw_rgb565_swap(pixels, width * height);
  if (self->capture_) {
    const uint16_t rect[] = {static_cast<uint16_t>(area->x1), static_cast<uint16_t>(area->y1),
                             static_cast<uint16_t>(width), static_cast<uint16_t>(height)};
    Serial.write(reinterpret_cast<const uint8_t*>("WMFR"), 4);
    Serial.write(reinterpret_cast<const uint8_t*>(rect), sizeof(rect));
    Serial.write(pixels, width * height * 2);
  }
  self->display_.pushImage(area->x1, area->y1, width, height,
                           reinterpret_cast<uint16_t*>(pixels));
  lv_display_flush_ready(display);
}

void WordUi::readTouch(lv_indev_t* input, lv_indev_data_t* data) {
  auto* self = static_cast<WordUi*>(lv_indev_get_user_data(input));
  if (!self->touch_.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  TS_Point point = self->touch_.getPoint();
  const int32_t x = constrain(map(point.x, kTouchMinX, kTouchMaxX, 0,
                                  kScreenWidth - 1),
                              0, kScreenWidth - 1);
  const int32_t y = constrain(map(point.y, kTouchMinY, kTouchMaxY, 0,
                                  kScreenHeight - 1),
                              0, kScreenHeight - 1);
  data->state = LV_INDEV_STATE_PRESSED;
  data->point.x = x;
  data->point.y = y;
}

void WordUi::cardEvent(lv_event_t* event) {
  auto* self = static_cast<WordUi*>(lv_event_get_user_data(event));
  Serial.println(F("[ui] card tap"));
  self->audioFeedback_ = false;
  self->meaningVisible_ = !self->meaningVisible_;
  self->refresh();
}

void WordUi::studyEvent(lv_event_t* event) {
  auto* self = static_cast<WordUi*>(lv_event_get_user_data(event));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (self->reviewLoading_) return;
  self->audioFeedback_ = false;
  if (target == self->speakButton_) {
    self->audioFeedback_ = true;
    if (self->audio_.busy()) self->audio_.cancel();
    else if (!self->weatherBusy_) self->audio_.start(self->shownWord_, self->externalCard_ ? self->externalCard_->audio : "",
                           self->settings_.wordbookUrl, self->settings_.audioVolume);
  } else if (target == self->againButton_ || target == self->goodButton_) {
    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    const ReviewRecord* record = self->study_.find(self->shownWord_.c_str());
    if (self->graded_ || (record && !reviewDue(*record, now))) return;
    const bool good = target == self->goodButton_;
    self->reviewLoading_ = true;
    self->refresh();
    lv_refr_now(self->lvDisplay_);
    if (!self->study_.grade(self->shownWord_.c_str(), now, good, self->settings_.dailyNewLimit)) {
      self->reviewLoading_ = false;
      self->studyMessage_ = "SAVE FAILED / HISTORY FULL";
    } else {
      self->graded_ = true;
      Serial.printf("[ui] grade %s -> %s\n", self->shownWord_.c_str(),
                    good ? "GOT IT" : "AGAIN");
      // Advance straight to the next card: an explicit NEXT after every
      // grade only confused the picture-deck usage this device serves.
      self->audio_.cancel();
      self->reviewRequested_ = true;
    }
  } else if (target == self->hintButton_) {
    Serial.printf("[ui] hint press (art=%d)\n",
                  self->externalCard_ && self->externalCard_->art ? 1 : 0);
    self->hintVisible_ = true;
    self->meaningVisible_ = false;
    self->studyMessage_ = "HINT USED? CHOOSE AGAIN";
  } else {
    self->reviewLoading_ = true;
    self->audio_.cancel();
    self->reviewRequested_ = true;
  }
  self->refresh();
  if (self->reviewLoading_) lv_refr_now(self->lvDisplay_);
}

bool WordUi::takeReviewRequest() {
  if (!reviewRequested_) return false;
  reviewRequested_ = false;
  if (externalCard_) return true;
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  ReviewChoice choice;
  const bool allowNew = study_.allowsNew(now, settings_.dailyNewLimit);
  for (size_t i = 0; i < kSampleWordCount; ++i) {
    const ReviewRecord* record = study_.find(kSampleWords[i].word);
    choice.consider(static_cast<uint16_t>(i), allowNew, record, now);
  }
  const int32_t best = choice.index();
  if (best >= 0) sampleIndex_ = best;
  reviewFinished(best >= 0);
  return false;
}

void WordUi::reviewFinished(bool found) {
  reviewLoading_ = false;
  if (found) {
    meaningVisible_ = false;
    graded_ = false;
    hintVisible_ = false;
    studyMessage_ = "";
  } else {
    studyMessage_ = "ALL DONE - COME BACK LATER";
  }
  refresh();
}

}  // namespace wordmon
