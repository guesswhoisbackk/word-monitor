#pragma once
#include "study_store.hpp"

namespace wordmon {
struct QuizTotals {
  uint32_t points = 0, answered = 0, correct = 0;
};

class QuizStore {
 public:
  void begin();
  bool record(bool correct);
  const QuizTotals& totals() const { return totals_; }
  bool ready() const { return ready_; }
 private:
  QuizTotals totals_;
  bool ready_ = false;
};

// Quiz scoring never calls StudyStore::grade: spaced-review history is unchanged.
class QuizGame {
 public:
  explicit QuizGame(StudyStore& study) : study_(study) {}
  void begin() { store_.begin(); }
  bool start(bool learnedOnly, uint32_t seed, bool practice = false);
  bool answer(uint8_t choice);
  bool next();
  void stop() { active_ = false; }
  bool active() const { return active_; }
  bool finished() const { return active_ && position_ >= count_; }
  bool answered() const { return answered_; }
  bool practice() const { return practice_; }
  bool learnedOnly() const { return learnedOnly_; }
  int selected() const { return selected_; }
  uint8_t position() const { return position_; }
  uint8_t count() const { return count_; }
  uint8_t roundCorrect() const { return roundCorrect_; }
  uint8_t correctChoice() const { return correctChoice_; }
  uint16_t targetIndex() const { return targets_[position_ < count_ ? position_ : 0]; }
  const char* word() const;
  const char* choice(uint8_t index) const;
  const QuizTotals& totals() const { return store_.totals(); }
  bool storageReady() const { return store_.ready(); }
 private:
  bool prepare();
  uint32_t randomBelow(uint32_t bound);
  StudyStore& study_;
  QuizStore store_;
  uint32_t rng_ = 1;
  uint16_t targets_[10]{}, choices_[6]{};
  uint8_t count_ = 0, position_ = 0, correctChoice_ = 0, roundCorrect_ = 0;
  int8_t selected_ = -1;
  bool active_ = false, answered_ = false, practice_ = false, learnedOnly_ = true;
};
}
