#pragma once

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "app_config.hpp"
#include "display_driver.hpp"
#include "models.hpp"
#include "wordbook.hpp"

namespace wordmon {

class WordUi {
 public:
  WordUi(AppSettings& settings, StudyStore& study);

  void begin();
  void loop();
  void setNetworkInfo(bool accessPointMode, const String& ipAddress);
  // Swap the visible card source to the network wordbook (nullptr = use the
  // built-in sample pack) and show its sync status in the footer.
  void setWordbookCard(const WordbookCard* card, WordbookState state,
                       uint16_t wordCount);
  bool takeReviewRequest();
  void reviewFinished(bool found);

 private:
  AppSettings& settings_;
  StudyStore& study_;
  CydDisplay display_;
  SPIClass touchSpi_{VSPI};
  XPT2046_Touchscreen touch_{kTouchCs, kTouchIrq};
  lv_display_t* lvDisplay_ = nullptr;
  lv_indev_t* lvInput_ = nullptr;
  static constexpr size_t kDrawBufferPixels = kScreenWidth * 12;
  uint16_t* drawBuffer_ = nullptr;
  uint32_t lastRefresh_ = 0;
  bool accessPointMode_ = false;
  String ipAddress_;
  bool meaningVisible_ = false;
  bool graded_ = false;
  bool reviewRequested_ = false;
  bool hintVisible_ = false;
  int32_t sampleIndex_ = -1;
  int32_t sampleDay_ = -1;
  String shownWord_;
  String studyMessage_;
  lv_obj_t* studyLabel_ = nullptr;
  lv_obj_t* againButton_ = nullptr;
  lv_obj_t* goodButton_ = nullptr;
  lv_obj_t* hintButton_ = nullptr;
  lv_obj_t* answerPanel_ = nullptr;

  lv_obj_t* statusLabel_ = nullptr;
  lv_obj_t* dateLabel_ = nullptr;
  lv_obj_t* wordLabel_ = nullptr;
  lv_obj_t* meaningLabel_ = nullptr;
  lv_obj_t* exampleLabel_ = nullptr;
  lv_obj_t* footerLabel_ = nullptr;
  lv_obj_t* artImage_ = nullptr;
  lv_obj_t* letterLabel_ = nullptr;
  lv_obj_t* hintLabel_ = nullptr;
  const void* currentArt_ = nullptr;
  const WordbookCard* externalCard_ = nullptr;
  WordbookState wordbookState_ = WordbookState::Disabled;
  uint16_t wordbookCount_ = 0;

  void createLayout();
  void refresh();
  const char* wordbookSuffix() const;
  static void flushDisplay(lv_display_t* display, const lv_area_t* area,
                           uint8_t* pixels);
  static void readTouch(lv_indev_t* input, lv_indev_data_t* data);
  static void cardEvent(lv_event_t* event);
  static void studyEvent(lv_event_t* event);
};

}  // namespace wordmon
