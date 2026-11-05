#pragma once
#include <Arduino.h>
#include "study_policy.hpp"

namespace wordmon {
class StudyStore {
 public:
  ~StudyStore();
  StudyStore() = default;
  StudyStore(const StudyStore&) = delete;
  StudyStore& operator=(const StudyStore&) = delete;
  void begin();
  const ReviewRecord* find(const char* word) const;
  uint16_t newToday(uint32_t now) const;
  bool allowsNew(uint32_t now, uint8_t limit) const;
  bool ready() const { return ready_; }
  uint32_t revision() const { return revision_; }
  bool grade(const char* word, uint32_t now, bool remembered, uint8_t limit = 5);
 private:
  bool save();
  bool reserveRecords(size_t needed);
  static constexpr size_t kCapacity = kMaxStudyWords;
  ReviewRecord* records_ = nullptr;
  size_t capacity_ = 0;
  uint16_t count_ = 0;
  int32_t newDay_ = -1;
  uint16_t newCount_ = 0;
  bool ready_ = false;
  uint32_t revision_ = 0;
};
}
