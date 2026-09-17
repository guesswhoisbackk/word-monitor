#pragma once
#include <cstdint>

namespace wordmon {
constexpr uint32_t kWordbookSyncIntervalMs = 12UL * 60 * 60 * 1000;
constexpr uint32_t kWordbookRetryMs = 10UL * 60 * 1000;
inline int32_t studyDay(uint32_t now) {
  return now < 1700000000 ? -1 : static_cast<int32_t>((now + 9UL * 3600) / 86400);
}
struct ReviewRecord {
  uint64_t key = 0;
  uint32_t due = 0;
  uint8_t stage = 0;
  uint8_t reserved[3] = {};
};
inline bool reviewDue(const ReviewRecord& record, uint32_t now) {
  return now >= 1700000000 && record.due != 0 && record.due <= now;
}
class ReviewChoice {
 public:
  void consider(uint16_t index, int32_t today, const ReviewRecord* record,
                uint32_t now) {
    if (now < 1700000000) return;
    if (record && reviewDue(*record, now) && record->due < earliest_) {
      dueIndex_ = index;
      earliest_ = record->due;
    }
    if (!record && index == today) newIndex_ = index;
  }
  int32_t index() const { return dueIndex_ >= 0 ? dueIndex_ : newIndex_; }
 private:
  int32_t dueIndex_ = -1;
  int32_t newIndex_ = -1;
  uint32_t earliest_ = UINT32_MAX;
};
inline void gradeReview(ReviewRecord& record, uint32_t now, bool remembered) {
  if (now < 1700000000) return;
  static const uint8_t days[] = {1, 3, 7, 14, 30};
  if (!remembered) {
    record.stage = 0;
    record.due = now + 600;
  } else {
    const uint8_t stage = record.stage < 5 ? record.stage : 4;
    record.due = now + static_cast<uint32_t>(days[stage]) * 86400;
    record.stage = stage + 1;
  }
}
inline bool syncAllowed(uint32_t now, bool attempted, bool synced,
                        uint32_t lastAttempt, uint32_t lastSuccess) {
  if (attempted && now - lastAttempt < kWordbookRetryMs) return false;
  return !synced || now - lastSuccess >= kWordbookSyncIntervalMs;
}
}  // namespace wordmon
