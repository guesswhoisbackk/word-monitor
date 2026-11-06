#pragma once

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "app_config.hpp"
#include "display_driver.hpp"
#include "models.hpp"
#include "wordbook.hpp"
#include "audio_player.hpp"
#include "weather.hpp"
#include "today_review.hpp"
#include "quiz_game.hpp"

namespace wordmon {

class WordUi {
 public:
  WordUi(AppSettings& settings, StudyStore& study, AudioPlayer& audio, QuizGame& quiz);

  void begin();
  void loop();
  void setWeather(const WeatherReading& reading, bool busy) { weather_ = reading; weatherBusy_ = busy; }
  void showSettings();
  void closeSettings();
  void setDueCount(uint16_t count) { dueCount_=count; }
  void serviceAudio(bool canStart);
  bool takeWeatherRefreshRequest() {
    const bool requested = weatherRefreshRequested_;
    weatherRefreshRequested_ = false; return requested;
  }
  void setNetworkInfo(bool accessPointMode, const String& ipAddress);
  // Swap the visible card source to the network wordbook (nullptr = use the
  // built-in sample pack) and show its sync status in the footer.
  void setWordbookCard(const WordbookCard* card, WordbookState state,
                       uint16_t wordCount);
  uint8_t takeTodayRequest(bool ready) { if(!ready) return 0; const uint8_t request=todayRequest_; todayRequest_=0; return request; }
  void setTodayState(bool active, uint16_t position, uint16_t count, bool reset = false);
  void todayUnavailable();
  bool takeReviewRequest();
  void reviewFinished(bool found);

 private:
  AppSettings& settings_;
  StudyStore& study_;
  AudioPlayer& audio_;
  QuizGame& quiz_;
  lv_obj_t* quizOverlay_ = nullptr;
  uint32_t quizAnsweredAt_ = 0;
  void showLearningMenu();
  void startQuiz(bool learnedOnly, bool practice = false);
  void renderQuiz();
  void closeQuiz();
  void quizAnswer(uint8_t choice);
  void quizNext();
  lv_obj_t* speakButton_ = nullptr;
  lv_obj_t* speakLabel_ = nullptr;
  bool audioFeedback_ = false;
  bool speakRequested_ = false;
  bool manualReview_ = false;
  String selectionWord_;
  uint16_t dueCount_ = 0;
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
  bool reviewLoading_ = false;
  bool hintVisible_ = false;
  int32_t sampleIndex_ = -1;
  int32_t sampleDay_ = -1;
  String shownWord_;
  String studyMessage_;
  lv_obj_t* studyLabel_ = nullptr;
  lv_obj_t* againButton_ = nullptr;
  lv_obj_t* goodButton_ = nullptr;
  lv_obj_t* nextButton_ = nullptr;
  lv_obj_t* hintButton_ = nullptr;
  lv_obj_t* answerPanel_ = nullptr;

  bool todayMode_ = false;
  uint16_t todayPosition_ = 0, todayCount_ = 0;
  uint8_t todayRequest_ = 0;
  TodayPlayback playback_;
  lv_obj_t* weatherCard_ = nullptr;
  lv_obj_t* wordCard_ = nullptr;
  lv_obj_t* sentencePanel_ = nullptr;
  lv_obj_t* exampleLabel_ = nullptr;
  lv_obj_t* translationLabel_ = nullptr;
  WeatherReading weather_;
  bool weatherBusy_ = false;
  bool weatherRefreshRequested_ = false;
  bool capture_ = false;
  lv_obj_t* progressLabel_ = nullptr;
  lv_obj_t* weatherTemperature_ = nullptr;
  lv_obj_t* weatherDetail_ = nullptr;
  lv_obj_t* weatherRain_ = nullptr;
  lv_obj_t* morningRain_ = nullptr;
  lv_obj_t* noonRain_ = nullptr;
  lv_obj_t* eveningRain_ = nullptr;
  lv_obj_t* clothingLabel_ = nullptr;
  lv_obj_t* accessoryLabel_ = nullptr;
  lv_obj_t* clockLabel_ = nullptr;
  lv_obj_t* settingsOverlay_ = nullptr;
  lv_obj_t* statusLabel_ = nullptr;
  lv_obj_t* dateLabel_ = nullptr;
  lv_obj_t* wordLabel_ = nullptr;
  lv_obj_t* meaningLabel_ = nullptr;
  lv_obj_t* footerLabel_ = nullptr;
  lv_obj_t* artImage_ = nullptr;
  lv_obj_t* letterLabel_ = nullptr;
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
