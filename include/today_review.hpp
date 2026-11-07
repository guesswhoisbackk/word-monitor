#pragma once
#include "study_policy.hpp"
namespace wordmon {
inline bool nightScreenOff(int localHour) {
  return localHour >= 0 && localHour < 24 && (localHour >= 21 || localHour < 7);
}
class NightWake {
 public:
  bool screenOff(bool night, uint32_t now) {
    if (!night || uint32_t(now - lastTouch_) >= 30000) awake_ = false;
    return night && !awake_;
  }
  bool acceptTouch(bool off, bool pressed, uint32_t now) {
    if (!pressed) { swallow_ = false; return false; }
    if (off) { awake_ = true; swallow_ = true; }
    if (awake_) lastTouch_ = now;
    return !off && !swallow_;
  }
 private:
  uint32_t lastTouch_ = 0;
  bool awake_ = false, swallow_ = false;
};
// Derive the last successful grade time from the stable v2 scheduling fields.
// This also includes today's existing records without migrating/writing history.
inline uint32_t reviewedAt(const ReviewRecord& record) {
  static constexpr uint32_t intervals[] = {600, 86400, 3*86400, 7*86400, 14*86400, 30*86400};
  return record.stage <= 5 && record.due >= intervals[record.stage]
      ? record.due - intervals[record.stage] : 0;
}
inline bool reviewedToday(const ReviewRecord& record, uint32_t now) {
  const uint32_t at = reviewedAt(record);
  return at <= now && studyDay(now) >= 0 && studyDay(at) == studyDay(now);
}
inline bool missedToday(const ReviewRecord& record, uint32_t now) {
  return record.stage == 0 && reviewedToday(record, now);
}
class HomeDeck {
 public:
  explicit HomeDeck(uint16_t (&indices)[kMaxStudyWords]) : today_(indices) {}
  void reset() { todayCount_ = freshCount_ = 0; }
  void consider(uint16_t index, const ReviewRecord* record, uint32_t now, bool illustrated) {
    if (record && reviewedToday(*record, now) && todayCount_ < kMaxStudyWords)
      today_[todayCount_++] = index;
    else if (!record && illustrated && freshCount_ < 5)
      fresh_[freshCount_++] = index;
  }
  uint16_t count() const { return todayCount_ ? todayCount_ : freshCount_; }
  uint16_t index(uint16_t at) const {
    return count() ? (todayCount_ ? today_ : fresh_)[at % count()] : UINT16_MAX;
  }
 private:
  uint16_t* today_;
  uint16_t fresh_[5]{};
  uint16_t todayCount_ = 0, freshCount_ = 0;
};
class TodayPlayback {
 public:
  explicit TodayPlayback(uint32_t interval = 12000) : interval_(interval) {}
  bool paused = false;
  void reset(uint32_t now) { changedAt_ = now; }
  void toggle(uint32_t now) { paused = !paused; reset(now); }
  bool tick(uint32_t now, bool blocked) {
    if(blocked) { reset(now); return false; }
    if(paused || uint32_t(now-changedAt_) < interval_) return false;
    reset(now); return true;
  }
 private:
  uint32_t interval_;
  uint32_t changedAt_ = 0;
};
}
