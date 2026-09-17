#include "word_ui.hpp"

#include <cctype>
#include <cstring>
#include <ctime>
#include <new>

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

// Bring-up placeholder pack. The real word source (built-in list or the web
// portal editor) replaces this table. Keep meanings and examples short: both
// labels are single-line elements using LV_LABEL_LONG_DOT.
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

// apple sits at index 4 so the 2026-09-17 bring-up flash (day-of-year 260,
// 260 % 8 = 4) shows the illustrated card on first boot.
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

WordUi::WordUi(AppSettings& settings) : settings_(settings) {}

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

  makeLabel(screen, "WORDMON", 8, 8, 100, &lv_font_montserrat_14, kGreen);
  statusLabel_ = makeLabel(screen, "STARTING", 112, 10, 120,
                           &lv_font_montserrat_12, kMuted,
                           LV_TEXT_ALIGN_RIGHT);
  dateLabel_ = makeLabel(screen, "TIME WAITING FOR WI-FI", 8, 34, 224,
                         &lv_font_montserrat_12, kMuted);

  lv_obj_t* card = makeCard(screen, 8, 58, 224, 168);
  makeLabel(card, "WORD OF THE DAY", 0, 10, 224, &lv_font_montserrat_12,
            kMuted, LV_TEXT_ALIGN_CENTER);
  wordLabel_ = makeLabel(card, "--", 0, 34, 224, &lv_font_montserrat_24,
                         kText, LV_TEXT_ALIGN_CENTER);
  // Flashcard front face: illustration art, or a big first letter when the
  // word has no art yet. The meaning/example rows are back-face content.
  artImage_ = lv_image_create(card);
  lv_obj_add_flag(artImage_, LV_OBJ_FLAG_HIDDEN);
  letterLabel_ = makeLabel(card, "A", 0, 75, 224, &lv_font_montserrat_48,
                           kBorder, LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  hintLabel_ = makeLabel(card, "TAP TO REVEAL", 0, 150, 224,
                         &lv_font_montserrat_12, kMuted, LV_TEXT_ALIGN_CENTER);
  meaningLabel_ = makeLabel(card, "", 0, 82, 224,
                            &lv_font_montserrat_14, kCyan,
                            LV_TEXT_ALIGN_CENTER);
  exampleLabel_ = makeLabel(card, "", 0, 112, 224, &lv_font_montserrat_12,
                            kMuted, LV_TEXT_ALIGN_CENTER);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, cardEvent, LV_EVENT_CLICKED, this);

  makeLabel(screen, "TAP CARD TO SEE THE MEANING", 8, 238, 224,
            &lv_font_montserrat_12, kMuted, LV_TEXT_ALIGN_CENTER);

  footerLabel_ = makeLabel(screen, "Starting...", 8, 302, 224,
                           &lv_font_montserrat_12, kMuted,
                           LV_TEXT_ALIGN_CENTER);
}

void WordUi::setWordbookCard(const WordbookCard* card, WordbookState state,
                             uint16_t wordCount) {
  externalCard_ = card;
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
    index = static_cast<size_t>(local.tm_yday) % kSampleWordCount;
    char dateText[32];
    snprintf(dateText, sizeof(dateText), "%s %02d %d - DAY %d",
             kMonthNames[local.tm_mon], local.tm_mday, local.tm_year + 1900,
             local.tm_yday + 1);
    setLabelTextIfChanged(dateLabel_, dateText);
  } else {
    setLabelTextIfChanged(dateLabel_, "TIME WAITING FOR WI-FI");
  }

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
  setLabelTextIfChanged(wordLabel_, word);

  const bool front = !meaningVisible_;
  const bool showArt = front && art != nullptr;
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

  if (front && art == nullptr) {
    char letter[2] = {
        static_cast<char>(toupper(static_cast<unsigned char>(word[0]))),
        '\0'};
    setLabelTextIfChanged(letterLabel_, letter);
    lv_obj_clear_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(letterLabel_, LV_OBJ_FLAG_HIDDEN);
  }

  if (front) {
    lv_obj_clear_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(hintLabel_, LV_OBJ_FLAG_HIDDEN);
  }

  if (meaningVisible_) {
    setLabelTextIfChanged(meaningLabel_, meaning);
    setLabelTextIfChanged(exampleLabel_, example);
  } else {
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
  self->meaningVisible_ = !self->meaningVisible_;
  self->refresh();
}

}  // namespace wordmon
