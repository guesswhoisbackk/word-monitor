#include "word_ui.hpp"

#include <cctype>
#include <cstring>
#include <ctime>
#include <new>
#include <src/misc/cache/instance/lv_image_cache.h>

#include "app_config.hpp"
#include "assets/apple_art.hpp"

namespace wordmon {

namespace {

constexpr uint32_t kBackground = 0x07111F;
constexpr uint32_t kCard = 0x0D1B2D;
constexpr uint32_t kBorder = 0x1D3550;
constexpr uint32_t kText = 0xE8F2FF;
constexpr uint32_t kMuted = 0x8BA3BA;
constexpr uint32_t kGreen = 0x38E29D;
constexpr uint32_t kCyan = 0x27C7FF;

constexpr int kCardWidth = 224;
constexpr int kArtY = 62;
constexpr int kArtSlotHeight = 88;

// Offline sample pack; it uses the same review history as hosted words.
struct SampleWord {
  const char* word;
  const char* meaning;
  const char* example;
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
    {"resilient", "bounces back quickly", "The little shop survived it all.", nullptr},
    {"diligent", "steady, careful effort", "Five words a day, every day.", nullptr},
    {"curious", "eager to learn", "One more question before sleep.", nullptr},
    {"vivid", "strikingly clear", "The blue sea left a vivid memory.", nullptr},
    {"apple", "a round red fruit", "She ate an apple at lunch.", &appleArt},
    {"frugal", "careful with resources", "A frugal maker saves every scrap.", nullptr},
    {"serene", "calm and peaceful", "The workshop was serene at dawn.", nullptr},
    {"profound", "deep and thoughtful", "Simple words carry deep ideas.", nullptr},
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
  lv_obj_set_style_radius(card, 8, 0);
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

  statusLabel_ = makeLabel(screen, "STARTING", 104, 10, 62,
                           &lv_font_montserrat_12, kMuted,
                           LV_TEXT_ALIGN_RIGHT);
  dateLabel_ = makeLabel(screen, "TIME WAITING FOR WI-FI", 8, 34, 224,
                         &lv_font_montserrat_12, kMuted);

  lv_obj_t* card = makeCard(screen, 8, 58, 224, 168);
  makeLabel(card, "RECALL THE MEANING", 0, 10, 224, &lv_font_montserrat_12,
            kMuted, LV_TEXT_ALIGN_CENTER);
  wordLabel_ = makeLabel(card, "--", 0, 34, 224, &lv_font_montserrat_24,
                         kText, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(wordLabel_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  // Flashcard front face: illustration art, or a big first letter when the
  // word has no art yet. The meaning/example rows are back-face content.
  artImage_ = lv_image_create(card);
  lv_obj_add_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  letterLabel_ = makeLabel(card, "A", 0, 75, 224, &lv_font_montserrat_48,
                           kBorder, LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  hintLabel_ = makeLabel(card, "TAP TO REVEAL", 0, 150, 224,
                         &lv_font_montserrat_12, kMuted, LV_TEXT_ALIGN_CENTER);
  answerPanel_ = lv_obj_create(card);
  lv_obj_set_pos(answerPanel_, 4, 64);
  lv_obj_set_size(answerPanel_, 216, 100);
  lv_obj_set_style_bg_opa(answerPanel_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(answerPanel_, 0, 0);
  lv_obj_set_style_pad_all(answerPanel_, 2, 0);
  lv_obj_set_style_pad_row(answerPanel_, 8, 0);
  lv_obj_set_flex_flow(answerPanel_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(answerPanel_, LV_DIR_VER);
  meaningLabel_ = makeLabel(answerPanel_, "", 0, 0, 204,
                            &lv_font_montserrat_14, kCyan,
                            LV_TEXT_ALIGN_CENTER);
  exampleLabel_ = makeLabel(answerPanel_, "", 0, 0, 204, &lv_font_montserrat_12,
                            kMuted, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(meaningLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(meaningLabel_, LV_SIZE_CONTENT);
  lv_label_set_long_mode(exampleLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(exampleLabel_, LV_SIZE_CONTENT);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, cardEvent, LV_EVENT_CLICKED, this);

  studyLabel_ = makeLabel(screen, "THINK FIRST, THEN TAP", 8, 231, 224,
                         &lv_font_montserrat_12, kMuted, LV_TEXT_ALIGN_CENTER);
  auto button = [&](const char* text, int x, int y, int width) {
    lv_obj_t* obj = lv_button_create(screen);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, 42);
    lv_obj_set_style_bg_color(obj, lv_color_hex(kBorder), 0);
    lv_obj_t* label = lv_label_create(obj);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(obj, studyEvent, LV_EVENT_CLICKED, this);
    return obj;
  };
  againButton_ = button("AGAIN", 8, 252, 70);
  goodButton_ = button("GOT IT", 85, 252, 70);
  button("REVIEW", 162, 252, 70);
  hintButton_ = button("HINT", 176, 3, 56);
  lv_obj_set_height(hintButton_, 28);
  speakButton_ = button("SPEAK", 8, 3, 88);
  lv_obj_set_height(speakButton_, 28);
  speakLabel_ = lv_obj_get_child(speakButton_, 0);

  footerLabel_ = makeLabel(screen, "Starting...", 8, 302, 224,
                           &lv_font_montserrat_12, kMuted,
                           LV_TEXT_ALIGN_CENTER);
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

  // The daily word follows the local calendar date from NTP. Without valid
  // time (fresh boot, no Wi-Fi yet) keep a stable word instead of flickering.
  const time_t now = time(nullptr);
  const bool timeValid = now > 1700000000;
  struct tm local {};
  size_t index = 0;
  if (timeValid) {
    localtime_r(&now, &local);
    const int32_t day = studyDay(static_cast<uint32_t>(now));
    index = static_cast<size_t>(day) % kSampleWordCount;
    if (sampleDay_ != day) {
      sampleDay_ = day;
      sampleIndex_ = -1;
    }
    char dateText[32];
    snprintf(dateText, sizeof(dateText), "%s %02d %d - DAY %d",
             kMonthNames[local.tm_mon], local.tm_mday, local.tm_year + 1900,
             local.tm_yday + 1);
    setLabelTextIfChanged(dateLabel_, dateText);
  } else {
    setLabelTextIfChanged(dateLabel_, "TIME WAITING FOR WI-FI");
  }
  if (sampleIndex_ >= 0) index = static_cast<size_t>(sampleIndex_);

  // The network wordbook wins over the built-in sample pack whenever a card
  // was selected from it (fetched or cached); nullptr keeps the samples.
  const char* word = nullptr;
  const char* meaning = nullptr;
  const char* example = nullptr;
  const lv_image_dsc_t* art = nullptr;
  if (externalCard_ != nullptr) {
    word = externalCard_->word;
    meaning = externalCard_->meaning;
    example = externalCard_->example;
    art = externalCard_->art;
  } else {
    const SampleWord& entry = kSampleWords[index];
    word = entry.word;
    meaning = entry.meaning;
    example = entry.example;
    art = entry.art;
  }
  if (shownWord_ != word) {
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
  setLabelTextIfChanged(wordLabel_, word);
  setLabelTextIfChanged(speakLabel_, audio_.busy() ? "STOP" : "SPEAK");

  const bool front = !meaningVisible_;
  const bool showArt = front && hintVisible_ && art != nullptr;
  if (showArt && currentArt_ != art) {
    currentArt_ = art;
    lv_image_set_src(artImage_, art);
    const int artY = art->header.h >= kArtSlotHeight
                         ? kArtY
                         : kArtY + (kArtSlotHeight - art->header.h) / 2;
    lv_obj_set_pos(artImage_, (kCardWidth - art->header.w) / 2, artY);
  }
  if (showArt) {
    lv_obj_clear_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  }

  if (front && !showArt) {
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
  if (art && front && !hintVisible_ && !waiting && !graded_) {
    lv_obj_remove_state(hintButton_, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(hintButton_, LV_STATE_DISABLED);
  }
  const bool canGrade = meaningVisible_ && !graded_ && timeValid && !waiting;
  for (lv_obj_t* button : {againButton_, goodButton_}) {
    if (canGrade && !(button == goodButton_ && hintVisible_)) lv_obj_remove_state(button, LV_STATE_DISABLED);
    else lv_obj_add_state(button, LV_STATE_DISABLED);
  }
  const char* message = !timeValid ? "CONNECT WI-FI TO SAVE REVIEWS" :
      !studyMessage_.isEmpty() ? studyMessage_.c_str() :
      waiting ? "SAVED - TAP REVIEW FOR MORE" :
      meaningVisible_ ? "SAY IT ALOUD, THEN RATE" : "THINK FIRST, THEN TAP";
  setLabelTextIfChanged(studyLabel_, message);
  if (audioFeedback_) setLabelTextIfChanged(studyLabel_, audio_.message());

  if (front) {
    lv_obj_clear_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
  }

  if (meaningVisible_) {
    lv_obj_remove_flag(answerPanel_, LV_OBJ_FLAG_HIDDEN);
    setLabelTextIfChanged(meaningLabel_, meaning);
    setLabelTextIfChanged(exampleLabel_, example);
  } else {
    lv_obj_add_flag(answerPanel_, LV_OBJ_FLAG_HIDDEN);
    setLabelTextIfChanged(meaningLabel_, "");
    setLabelTextIfChanged(exampleLabel_, "");
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
  self->audioFeedback_ = false;
  self->meaningVisible_ = !self->meaningVisible_;
  self->refresh();
}

void WordUi::studyEvent(lv_event_t* event) {
  auto* self = static_cast<WordUi*>(lv_event_get_user_data(event));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  self->audioFeedback_ = false;
  if (target == self->speakButton_) {
    self->audioFeedback_ = true;
    if (self->audio_.busy()) self->audio_.cancel();
    else self->audio_.start(self->shownWord_, self->externalCard_ ? self->externalCard_->audio : "",
                           self->settings_.wordbookUrl, self->settings_.audioVolume);
  } else if (target == self->againButton_ || target == self->goodButton_) {
    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    const ReviewRecord* record = self->study_.find(self->shownWord_.c_str());
    if (!self->meaningVisible_ || self->graded_ ||
        (record && !reviewDue(*record, now))) return;
    const bool good = target == self->goodButton_;
    if (good && self->hintVisible_) return;
    if (!self->study_.grade(self->shownWord_.c_str(), now, good)) {
      self->studyMessage_ = "SAVE FAILED / HISTORY FULL";
    } else {
      self->graded_ = true;
      self->studyMessage_ = good ? "SAVED - TAP REVIEW" : "AGAIN IN 10 MIN - TAP REVIEW";
    }
  } else if (target == self->hintButton_) {
    self->hintVisible_ = true;
    self->meaningVisible_ = false;
    self->studyMessage_ = "HINT USED? CHOOSE AGAIN";
  } else {
    self->audio_.cancel();
    self->reviewRequested_ = true;
  }
  self->refresh();
}

bool WordUi::takeReviewRequest() {
  if (!reviewRequested_) return false;
  reviewRequested_ = false;
  if (externalCard_) return true;
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  ReviewChoice choice;
  const int32_t today = sampleDay_ >= 0 ? sampleDay_ % kSampleWordCount : -1;
  for (size_t i = 0; i < kSampleWordCount; ++i) {
    const ReviewRecord* record = study_.find(kSampleWords[i].word);
    choice.consider(static_cast<uint16_t>(i), today, record, now);
  }
  const int32_t best = choice.index();
  if (best >= 0) sampleIndex_ = best;
  reviewFinished(best >= 0);
  return false;
}

void WordUi::reviewFinished(bool found) {
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
