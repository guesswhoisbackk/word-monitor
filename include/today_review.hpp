#pragma once
#include "study_policy.hpp"
namespace wordmon {
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
class TodayPlayback {
 public:
  bool paused = false;
  void reset(uint32_t now) { changedAt_ = now; }
  void toggle(uint32_t now) { paused = !paused; reset(now); }
  bool tick(uint32_t now, bool blocked) {
    if(blocked) { reset(now); return false; }
    if(paused || uint32_t(now-changedAt_) < 12000) return false;
    reset(now); return true;
  }
 private:
  uint32_t changedAt_ = 0;
};
}
