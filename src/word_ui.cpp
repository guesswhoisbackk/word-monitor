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

WordUi::WordUi(AppSettings& settings, StudyStore& study, AudioPlayer& audio, QuizGame& quiz)
    : settings_(settings), study_(study), audio_(audio), quiz_(quiz) {}

void WordUi::begin() {
  quiz_.begin();
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
    if (command == 'r') lv_obj_send_event(wordCard_, LV_EVENT_CLICKED, nullptr);
    if (command == 'f') { meaningVisible_ = false; refresh(); }
    // USB-only loading preview; does not grade or request another card.
    if (command == 'd') { reviewLoading_ = true; refresh(); }
    if (command == 'u') { reviewLoading_ = false; refresh(); }
    if ((command == 'n' || command == 't') && !quizOverlay_) todayRequest_=todayMode_?2:1;
    if (command == 'q') startQuiz(true);
    if (command == 'Q') startQuiz(false,true); // USB practice: never writes points/history.
    if (command == 'm') showLearningMenu();
    if (command == 'c') closeQuiz();
    if (command >= '1' && command <= '6' && quizOverlay_) quizAnswer(command-'1');
    if (command == 'v' && quizOverlay_) quizNext();
    if (command == 'j' && todayMode_) lv_obj_send_event(goodButton_, LV_EVENT_CLICKED, nullptr);
    if (command == 'k' && todayMode_) lv_obj_send_event(againButton_, LV_EVENT_CLICKED, nullptr);
    if (command == 'p' && todayMode_) { playback_.toggle(millis()); refresh(); }
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
  if(todayMode_ && !todayRequest_ && playback_.tick(millis(),settingsOverlay_ || quizOverlay_ || audio_.busy() || speakRequested_)) todayRequest_=4;
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

  clockLabel_ = makeLabel(screen, "--:--", 8, 5, 70, &font_ui_24, kText);
  lv_obj_t* setup = lv_button_create(screen);
  lv_obj_set_pos(setup, 198, 3); lv_obj_set_size(setup, 34, 32);
  lv_obj_set_style_bg_color(setup, lv_color_hex(kBorder), 0);
  lv_obj_t* gear = lv_label_create(setup); lv_label_set_text(gear, LV_SYMBOL_SETTINGS); lv_obj_center(gear);
  lv_obj_add_event_cb(setup, [](lv_event_t* e) { static_cast<WordUi*>(lv_event_get_user_data(e))->showSettings(); }, LV_EVENT_CLICKED, this);
  progressLabel_ = makeLabel(screen, "새0/5", 80, 12, 44, &font_kr_13, kGreen, LV_TEXT_ALIGN_RIGHT);
  lv_obj_add_flag(progressLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(progressLabel_, [](lv_event_t* e) {
    auto* self=static_cast<WordUi*>(lv_event_get_user_data(e));
    if(self->todayMode_) { self->playback_.toggle(millis()); self->refresh(); }
  }, LV_EVENT_CLICKED, this);
  lv_obj_t* weatherCard = weatherCard_ = makeCard(screen, 8, 38, 224, 112);
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
  lv_obj_t* card = wordCard_ = makeCard(screen, 8, 158, 224, 98);
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

  sentencePanel_=makeCard(screen,8,148,224,103);
  lv_obj_set_style_pad_all(sentencePanel_,8,0);
  lv_obj_set_style_pad_row(sentencePanel_,6,0);
  lv_obj_set_flex_flow(sentencePanel_,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(sentencePanel_,LV_DIR_VER);
  lv_obj_add_flag(sentencePanel_,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sentencePanel_,cardEvent,LV_EVENT_CLICKED,this);
  exampleLabel_=makeLabel(sentencePanel_,"",0,0,206,&font_kr_16,kText);
  translationLabel_=makeLabel(sentencePanel_,"",0,0,206,&font_kr_13,kCyan);
  for(auto* label:{exampleLabel_,translationLabel_}) {
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    lv_obj_set_height(label,LV_SIZE_CONTENT);
    lv_obj_add_flag(label,LV_OBJ_FLAG_EVENT_BUBBLE);
  }
  lv_obj_add_flag(sentencePanel_,LV_OBJ_FLAG_HIDDEN);

  studyLabel_ = makeLabel(screen, "단어를 떠올리고 평가해요", 8, 257, 224,
                         &font_kr_13, kMuted, LV_TEXT_ALIGN_CENTER);
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
  nextButton_ = button("더 배우기", 162, 274, 70);
  lv_obj_set_style_text_font(lv_obj_get_child(nextButton_,0), &font_kr_13,0);
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

  }

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
    snprintf(progress, sizeof(progress), "새%u/%u", study_.newToday(static_cast<uint32_t>(now)), settings_.dailyNewLimit);
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
    speakRequested_=false;
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
  setLabelTextIfChanged(speakLabel_, audio_.busy() || speakRequested_ ? "STOP" : "SPEAK");

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
      "";
  char progressText[100];
  if(!*message) {
    snprintf(progressText,sizeof(progressText),"새 단어 %u/%u · 복습 %u개",study_.newToday(static_cast<uint32_t>(now)),settings_.dailyNewLimit,dueCount_);
    message=progressText;
  }
  setLabelTextIfChanged(studyLabel_, message);
  if(audioFeedback_) setLabelTextIfChanged(studyLabel_,speakRequested_?"발음 준비 중":audio_.message());

  if(todayMode_) {
    setLabelTextIfChanged(progressLabel_,playback_.paused?"멈춤":"자동");
    for(auto* button:{againButton_,goodButton_,nextButton_}) lv_obj_remove_state(button,LV_STATE_DISABLED);
    char caption[100];
    snprintf(caption,sizeof(caption),"오늘 복습 %u/%u · %s",todayPosition_+1,todayCount_,
             playback_.paused?"멈춤":meaningVisible_?"다시 터치하면 멈춤":"터치해 뜻 확인");
    setLabelTextIfChanged(studyLabel_,caption);
    const char* example=externalCard_?externalCard_->example:"";
    const char* translation=externalCard_?externalCard_->translation:"";
    setLabelTextIfChanged(exampleLabel_,*example?example:"이 단어에는 예문이 없어요");
    setLabelTextIfChanged(translationLabel_,*translation?translation:"이 예문의 해석이 없어요");
    if(meaningVisible_) lv_obj_remove_flag(translationLabel_,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(translationLabel_,LV_OBJ_FLAG_HIDDEN);
  }

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
  if(self->todayMode_) {
    if(!self->meaningVisible_) self->meaningVisible_=true;
    else self->playback_.toggle(millis());
    self->playback_.reset(millis());
  } else self->meaningVisible_ = !self->meaningVisible_;
  self->refresh();
}

void WordUi::studyEvent(lv_event_t* event) {
  auto* self = static_cast<WordUi*>(lv_event_get_user_data(event));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
  if (self->reviewLoading_ || self->quizOverlay_) return;
  self->audioFeedback_ = false;
  if(self->todayMode_ && target!=self->speakButton_) {
    // Only the registered navigation buttons may change replay state.
    if(target!=self->againButton_ && target!=self->goodButton_ && target!=self->nextButton_) return;
    self->speakRequested_=false; self->audio_.cancel();
    self->todayRequest_=target==self->againButton_?3:target==self->goodButton_?4:2;
    Serial.printf("[today] navigation request=%u\n",self->todayRequest_);
    return;
  }
  if (target == self->speakButton_) {
    self->audioFeedback_ = true;
    if(self->audio_.busy() || self->speakRequested_) {self->audio_.cancel();self->speakRequested_=false;}
    else self->speakRequested_=true;
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
      self->manualReview_=false;
      self->speakRequested_=false;
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
  } else if (target == self->nextButton_) {
    self->speakRequested_=false; self->audio_.cancel();
    self->showLearningMenu();
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

void WordUi::setTodayState(bool active,uint16_t position,uint16_t count,bool reset) {
  if(!reset && active==todayMode_ && position==todayPosition_ && count==todayCount_) return;
  todayMode_=active; todayPosition_=position; todayCount_=count;
  meaningVisible_=false; audioFeedback_=false; speakRequested_=false;
  studyMessage_=""; reviewLoading_=false;
  playback_.reset(millis());
  if(active) { lv_obj_add_flag(weatherCard_,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(sentencePanel_,LV_OBJ_FLAG_HIDDEN); }
  else { lv_obj_remove_flag(weatherCard_,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(sentencePanel_,LV_OBJ_FLAG_HIDDEN); playback_.paused=false; }
  lv_obj_set_y(wordCard_,active?42:158);
  lv_label_set_text(lv_obj_get_child(againButton_,0),active?"이전":"AGAIN");
  lv_label_set_text(lv_obj_get_child(goodButton_,0),active?"다음":"GOT IT");
  lv_label_set_text(lv_obj_get_child(nextButton_,0),active?"나가기":"더 배우기");
  for(auto* button:{againButton_,goodButton_}) lv_obj_set_style_text_font(lv_obj_get_child(button,0),active?&font_kr_13:&font_ui_12,0);
  lv_obj_scroll_to_y(sentencePanel_,0,LV_ANIM_OFF);
  refresh();
}
void WordUi::todayUnavailable() {
  studyMessage_="오늘 학습한 단어가 없어요"; reviewLoading_=false; refresh();
}

void WordUi::closeSettings() {
  if(settingsOverlay_) {lv_obj_delete(settingsOverlay_);settingsOverlay_=nullptr;}
}
void WordUi::serviceAudio(bool canStart) {
  if(quizOverlay_ || !canStart || !speakRequested_ || audio_.busy()) return;
  speakRequested_=false;
  audio_.start(shownWord_,externalCard_?externalCard_->audio:"",settings_.wordbookUrl,settings_.audioVolume);
}
void WordUi::reviewFinished(bool found) {
  reviewLoading_ = false;
  if (found) {
    meaningVisible_ = false;
    graded_ = false;
    hintVisible_ = false;
    studyMessage_ = "";
  } else {
    studyMessage_ = "오늘 학습을 마쳤어요";
  }
  if(found && manualReview_ && selectionWord_==shownWord_) studyMessage_="현재 단어를 먼저 평가해 주세요";
  manualReview_=false;
  refresh();
}


void WordUi::closeQuiz() {
  quiz_.stop();
  if(quizOverlay_) { lv_obj_delete(quizOverlay_); quizOverlay_=nullptr; }
  playback_.reset(millis());
}

void WordUi::showLearningMenu() {
  closeQuiz(); closeSettings();
  audio_.cancel(); speakRequested_=false; todayRequest_=0;
  quizOverlay_=makeCard(lv_screen_active(),0,0,240,320);
  lv_obj_set_style_radius(quizOverlay_,0,0);
  makeLabel(quizOverlay_,"더 배우기",12,12,210,&font_kr_16,kText);
  char points[40]; snprintf(points,sizeof(points),"%lu P",static_cast<unsigned long>(quiz_.totals().points));
  makeLabel(quizOverlay_,points,12,43,216,&font_ui_24,kGreen);
  makeLabel(quizOverlay_,"정답 +10점 · 틀려도 괜찮아요",12,77,216,&font_kr_13,kMuted);
  const char* labels[]={"배운 단어 퀴즈","전체 단어 퀴즈","오늘 단어 다시 보기","돌아가기"};
  for(int i=0;i<4;++i) {
    auto* button=lv_button_create(quizOverlay_);lv_obj_set_pos(button,12,108+i*49);lv_obj_set_size(button,216,42);
    lv_obj_set_style_bg_color(button,lv_color_hex(i==0?kGreen:kBorder),0);
    auto* label=lv_label_create(button);lv_label_set_text(label,labels[i]);
    lv_obj_set_style_text_font(label,&font_kr_16,0);lv_obj_set_style_text_color(label,lv_color_hex(i==0?kBackground:kText),0);lv_obj_center(label);
    lv_obj_set_user_data(button,reinterpret_cast<void*>(static_cast<uintptr_t>(i+1)));
    lv_obj_add_event_cb(button,[](lv_event_t* e){
      auto* self=static_cast<WordUi*>(lv_event_get_user_data(e));
      const uint32_t index=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_current_target(e)))))-1;
      if(index==0 || index==1) self->startQuiz(index==0);
      else if(index==2) {self->closeQuiz();if(!self->todayMode_)self->todayRequest_=1;}
      else self->closeQuiz();
    },LV_EVENT_CLICKED,this);
  }
}

void WordUi::startQuiz(bool learnedOnly,bool practice) {
  closeSettings();
  audio_.cancel();speakRequested_=false;todayRequest_=0;
  if(!quizOverlay_) quizOverlay_=makeCard(lv_screen_active(),0,0,240,320);
  lv_obj_set_style_radius(quizOverlay_,0,0);
  quiz_.start(learnedOnly,esp_random(),practice);
  renderQuiz();
}

void WordUi::quizAnswer(uint8_t choice) {
  if(!quizOverlay_ || !quiz_.active() || quiz_.finished() || quiz_.answered()) return;
  quiz_.answer(choice);quizAnsweredAt_=millis();renderQuiz();
}

void WordUi::quizNext() {
  if(!quizOverlay_ || !quiz_.active() || uint32_t(millis()-quizAnsweredAt_)<500) return;
  if(quiz_.finished()) {startQuiz(quiz_.learnedOnly(),quiz_.practice());return;}
  if(!quiz_.answered() && quiz_.selected()>=0) {quizAnswer(static_cast<uint8_t>(quiz_.selected()));return;}
  if(quiz_.next()) renderQuiz();
}

void WordUi::renderQuiz() {
  lv_obj_clean(quizOverlay_);
  auto label=[&](const char* text,int x,int y,int w,int h,const lv_font_t* font,uint32_t color,lv_text_align_t align=LV_TEXT_ALIGN_LEFT) {
    auto* obj=makeLabel(quizOverlay_,text,x,y,w,font,color,align);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_WRAP);lv_obj_set_height(obj,h);return obj;
  };
  auto button=[&](const char* text,int x,int y,int w,int h,uint32_t color) {
    auto* obj=lv_button_create(quizOverlay_);lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);lv_obj_set_style_pad_all(obj,4,0);
    lv_obj_set_style_radius(obj,10,0);lv_obj_set_style_shadow_width(obj,0,0);
    auto* caption=lv_label_create(obj);lv_label_set_text(caption,text);lv_obj_set_width(caption,w-10);
    lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(caption,&font_kr_13,0);lv_obj_set_style_text_color(caption,lv_color_hex(kText),0);lv_obj_center(caption);
    return obj;
  };
  auto* exit=button("닫기",182,6,50,32,kBorder);
  lv_obj_add_event_cb(exit,[](lv_event_t* e){static_cast<WordUi*>(lv_event_get_user_data(e))->closeQuiz();},LV_EVENT_CLICKED,this);
  char text[96];
  if(!quiz_.active()) {
    label("배운 단어가 없어요",12,70,216,30,&font_kr_16,kText,LV_TEXT_ALIGN_CENTER);
    label("단어를 학습한 뒤 퀴즈를 풀거나\n전체 단어 퀴즈를 시작해 보세요.",12,112,216,60,&font_kr_13,kMuted,LV_TEXT_ALIGN_CENTER);
    auto* all=button("전체 단어 퀴즈",12,205,216,45,kBorder);
    lv_obj_add_event_cb(all,[](lv_event_t* e){static_cast<WordUi*>(lv_event_get_user_data(e))->startQuiz(false);},LV_EVENT_CLICKED,this);
    return;
  }
  if(quiz_.finished()) {
    label("퀴즈 완료!",12,50,216,30,&font_kr_16,kGreen,LV_TEXT_ALIGN_CENTER);
    snprintf(text,sizeof(text),"%u / %u",quiz_.roundCorrect(),quiz_.count());
    label(text,12,90,216,40,&font_ui_24,kText,LV_TEXT_ALIGN_CENTER);
    snprintf(text,sizeof(text),"이번 퀴즈 +%u점",quiz_.roundCorrect()*10);
    label(text,12,138,216,25,&font_kr_16,kGreen,LV_TEXT_ALIGN_CENTER);
    snprintf(text,sizeof(text),"누적 %lu점",static_cast<unsigned long>(quiz_.totals().points));
    label(quiz_.practice()?"체험 모드 · 포인트 저장 안 함":text,12,178,216,36,&font_kr_13,kMuted,LV_TEXT_ALIGN_CENTER);
    auto* again=button("한 번 더",12,232,216,42,kBorder);
    lv_obj_add_event_cb(again,[](lv_event_t* e){static_cast<WordUi*>(lv_event_get_user_data(e))->quizNext();},LV_EVENT_CLICKED,this);
    return;
  }
  snprintf(text,sizeof(text),"%u / %u",quiz_.position()+1,quiz_.count());
  label(text,10,12,70,22,&font_ui_18,kText);
  snprintf(text,sizeof(text),"%lu P",static_cast<unsigned long>(quiz_.totals().points));
  label(quiz_.practice()?"체험":text,82,14,94,22,&font_kr_13,kGreen,LV_TEXT_ALIGN_RIGHT);
  auto* word=label(quiz_.word(),12,49,216,65,&font_ui_24,kText,LV_TEXT_ALIGN_CENTER);
  if(strlen(quiz_.word())>20) lv_obj_set_style_text_font(word,&font_ui_18,0);
  const char* prompt="알맞은 뜻을 고르세요";
  if(quiz_.selected()>=0) {
    if(!quiz_.answered()) prompt="저장 실패 · 다시 저장해 주세요";
    else prompt=quiz_.selected()==quiz_.correctChoice()?"정답이에요! +10점":"괜찮아요! 초록색이 정답이에요";
  }
  label(prompt,8,115,224,22,&font_kr_13,quiz_.answered()?kGreen:kMuted,LV_TEXT_ALIGN_CENTER);
  for(uint8_t i=0;i<6;++i) {
    uint32_t color=kBorder;
    if(quiz_.selected()>=0) {
      if(i==quiz_.correctChoice()) color=0x235743;
      else if(i==quiz_.selected()) color=0x713C45;
      else color=0x122335;
    }
    auto* choice=button(quiz_.choice(i),8+(i%2)*116,142+(i/2)*43,108,42,color);
    if(quiz_.selected()>=0) lv_obj_remove_flag(choice,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(choice,reinterpret_cast<void*>(static_cast<uintptr_t>(i+1)));
    lv_obj_add_event_cb(choice,[](lv_event_t* e){
      auto* self=static_cast<WordUi*>(lv_event_get_user_data(e));
      auto* target=static_cast<lv_obj_t*>(lv_event_get_current_target(e));
      const uint8_t i=static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target))-1);
      self->quizAnswer(i);
    },LV_EVENT_CLICKED,this);
  }
  if(quiz_.selected()>=0) {
    auto* next=button(quiz_.answered()?(quiz_.position()+1==quiz_.count()?"결과 보기":"다음 문제"):"다시 저장",8,277,224,35,kBorder);
    lv_obj_add_event_cb(next,[](lv_event_t* e){static_cast<WordUi*>(lv_event_get_user_data(e))->quizNext();},LV_EVENT_CLICKED,this);
  } else label(quiz_.learnedOnly()?"배운 단어 · 정답마다 +10점":"전체 단어 · 정답마다 +10점",8,287,224,22,&font_kr_13,kMuted,LV_TEXT_ALIGN_CENTER);
}

}  // namespace wordmon
