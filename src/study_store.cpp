#include "study_store.hpp"
#include <Preferences.h>

namespace wordmon {
namespace {
uint64_t wordKey(const char* word) {
  uint64_t hash = 14695981039346656037ULL;
  while (*word) {
    hash ^= static_cast<uint8_t>(*word++);
    hash *= 1099511628211ULL;
  }
  return hash ? hash : 1;
}
}
void StudyStore::begin() {
  Preferences prefs;
  ready_ = prefs.begin("wm-study", false);
  if (!ready_) return;
  if (prefs.getBytesLength("reviews-v1") == sizeof(records_)) {
    prefs.getBytes("reviews-v1", records_, sizeof(records_));
  }
  prefs.end();
}
const ReviewRecord* StudyStore::find(const char* word) const {
  const uint64_t key = wordKey(word);
  for (const auto& record : records_) if (record.key == key) return &record;
  return nullptr;
}
bool StudyStore::grade(const char* word, uint32_t now, bool remembered) {
  if (!ready_ || !*word || now < 1700000000) return false;
  ReviewRecord* target = nullptr;
  const uint64_t key = wordKey(word);
  for (auto& record : records_) {
    if (record.key == key) { target = &record; break; }
    if (!record.key && !target) target = &record;
  }
  if (!target) return false;  // Never silently discard another word's history.
  const ReviewRecord before = *target;
  target->key = key;
  gradeReview(*target, now, remembered);
  Preferences prefs;
  bool saved = prefs.begin("wm-study", false);
  if (saved) {
    saved = prefs.putBytes("reviews-v1", records_, sizeof(records_)) == sizeof(records_);
    prefs.end();
  }
  if (!saved) *target = before;
  return saved;
}
}
