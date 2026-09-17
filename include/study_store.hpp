#pragma once
#include <Arduino.h>
#include "study_policy.hpp"

namespace wordmon {
class StudyStore {
 public:
  void begin();
  const ReviewRecord* find(const char* word) const;
  bool grade(const char* word, uint32_t now, bool remembered);
 private:
  static constexpr size_t kCapacity = 128;
  ReviewRecord records_[kCapacity]{};
  bool ready_ = false;
};
}
