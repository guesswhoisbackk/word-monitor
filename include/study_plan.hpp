#pragma once
#include "study_policy.hpp"
#include <cstddef>
namespace wordmon {
class StudyPlan {
 public:
  static constexpr uint16_t none = UINT16_MAX;
  uint16_t dueCount = 0;
  uint32_t nextDue = UINT32_MAX;
  void consider(uint16_t index, const ReviewRecord* record, uint32_t now) {
    if (record) {
      if (!reviewDue(*record, now)) { if (record->due > now && record->due < nextDue) nextDue = record->due; return; }
      ++dueCount;
      size_t at = 0;
      while (at < dueSize_ && (due_[at].time < record->due ||
             (due_[at].time == record->due && due_[at].index < index))) ++at;
      if (at >= 5) return;
      if (dueSize_ < 5) ++dueSize_;
      for (size_t j = dueSize_ - 1; j > at; --j) due_[j] = due_[j-1];
      due_[at] = {index, record->due};
    } else if (newSize_ < 5) fresh_[newSize_++] = index;
  }
  int32_t best(bool allowNew) const { return dueSize_ ? due_[0].index : allowNew && newSize_ ? fresh_[0] : -1; }
  uint16_t prefetch(size_t at) const {
    if (at < dueSize_) return due_[at].index;
    at -= dueSize_;
    return at < newSize_ && at + dueSize_ < 5 ? fresh_[at] : none;
  }
 private:
  struct Due { uint16_t index; uint32_t time; } due_[5]{};
  uint16_t fresh_[5]{};
  size_t dueSize_ = 0, newSize_ = 0;
};
}
