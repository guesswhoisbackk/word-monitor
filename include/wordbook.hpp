#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "models.hpp"
#include "study_store.hpp"
#include "study_plan.hpp"
#include "cache_policy.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace wordmon {

// A card selected from the hosted wordbook. Pointers stay valid until the
// next revision bump (single-threaded loop, WordUi copies text on set).
struct WordbookCard {
  const char* word = "";
  const char* meaning = "";
  const char* example = "";
  const lv_image_dsc_t* art = nullptr;
  const char* audio = ""; // Optional same-origin WAV basename in manifest field "s".
};

enum class WordbookState {
  Disabled,  // no wordbook URL configured
  Syncing,   // download in progress
  Ok,        // manifest fetched from the network this boot
  Cached,    // showing the last manifest fetched to flash
  Failed,    // no manifest at all (device falls back to the built-in pack)
};

// Route-B wordbook: downloads words.jsonl and pre-rendered WMR1 art files
// from the configured base URL, caches both in LittleFS, and exposes the
// active study card. The manifest is JSON Lines (one {"w","m","e","a"}
// object per line). Daily selection and due/first-unseen scans read one
// bounded line at a time without a RAM word list.
class Wordbook {
 public:
  Wordbook(AppSettings& settings, StudyStore& study);

  void begin();
  void loop(bool allowNetwork = true);
  void poll();
  bool busy() const { return busy_; }
  void networkRestored() { retries_.reset(); if (!everSynced_) forceSync_ = true; }
  uint16_t dueCount() const { return plan_.dueCount; }
  uint8_t cachedAssets() const;
  uint8_t plannedAssets() const { return assetCount_; }

  const WordbookCard* card() const {
    return cardValid_ ? &card_ : nullptr;
  }
  WordbookState state() const { return state_; }
  uint16_t wordCount() const { return wordCount_; }
  // Bumped whenever state or card content changes; the app pushes it to the UI.
  uint32_t revision() const { return revision_; }
  bool nextReview(const StudyStore& study);

 private:
  bool syncDue(uint32_t now, int32_t yday);
  void attemptSync(int32_t yday);
  void selectCard(int32_t yday, int32_t requestedIndex = -1);
  bool fetchToFile(const String& url, const String& path, size_t maxBytes,
                   uint16_t* lineCount);
  bool readManifestLine(uint16_t index, String& out);
  void rebuildPlan();
  void rebuildAssets();
  void appendAsset(const String& name, bool voice);
  void pruneCache();
  void startJob(uint8_t type, const String& name = "", const String& path = "");
  static void downloadTask(void* context);
  bool validAsset(const String& path, bool voice);
  bool loadArt(const String& path);

  AppSettings& settings_;
  StudyStore& study_;
  WordbookState state_ = WordbookState::Disabled;
  uint32_t revision_ = 0;
  uint16_t wordCount_ = 0;
  bool cardValid_ = false;
  int32_t cardDay_ = -1;

  String cardWord_;
  String cardMeaning_;
  String cardExample_;
  String cardAudio_;
  WordbookCard card_;
  lv_image_dsc_t artDsc_{};
  uint16_t* artPixels_ = nullptr;

  struct Asset { char name[41] = {}; char path[40] = {}; bool voice = false; bool ready = false; } assets_[12];
  struct Job { uint8_t type = 0; String url; String path; } job_;
  struct Result { bool ok = false; uint16_t count = 0; };
  QueueHandle_t results_ = nullptr;
  bool busy_ = false, forceSync_ = false, assetsDirty_ = true;
  uint8_t assetCount_ = 0;
  String cardArtName_, cardArtPath_;
  StudyPlan plan_;
  uint32_t plannedStudyRevision_ = UINT32_MAX, planCheckAt_ = 0;
  int32_t planDay_ = -1;
  AssetRetries retries_;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastSyncOkMs_ = 0;
  bool everAttempted_ = false;
  bool everSynced_ = false;
};

}  // namespace wordmon
