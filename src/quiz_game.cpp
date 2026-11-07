#include "quiz_game.hpp"
#include "quiz_content.hpp"
#include <LittleFS.h>
#include <cstddef>
#include <climits>

namespace wordmon {
namespace {
constexpr const char* kPath = "/quiz-v1.bin";
constexpr const char* kTemp = "/quiz-v1.tmp";
struct SavedQuiz {
  uint32_t magic = 0x31514D57;
  QuizTotals totals;
  uint32_t checksum = 0;
};
uint32_t checksum(const SavedQuiz& state) {
  uint32_t hash = 2166136261UL;
  auto* bytes = reinterpret_cast<const uint8_t*>(&state);
  for(size_t i=0; i<offsetof(SavedQuiz,checksum); ++i) { hash ^= bytes[i]; hash *= 16777619UL; }
  return hash;
}
bool read(const char* path, SavedQuiz& state) {
  File file = LittleFS.open(path,"r");
  return file && file.size()==sizeof(state) &&
      file.read(reinterpret_cast<uint8_t*>(&state),sizeof(state))==sizeof(state) &&
      state.magic==0x31514D57 && state.checksum==checksum(state) &&
      state.totals.correct<=state.totals.answered &&
      uint64_t(state.totals.correct)*10==state.totals.points;
}
bool compatible(uint16_t a, uint16_t b) {
  const char* x=kQuizWords[a].meaning; const char* y=kQuizWords[b].meaning;
  // Also reject broad/narrow meanings such as 기타/통기타 or 공/축구공.
  return a!=b && !strstr(x,y) && !strstr(y,x);
}
}
void QuizStore::begin() {
  if(!LittleFS.begin(false)) return;
  LittleFS.remove(kTemp);
  if(!LittleFS.exists(kPath)) { ready_=true; return; }
  SavedQuiz saved;
  if(read(kPath,saved)) { totals_=saved.totals; ready_=true; }
}
bool QuizStore::record(bool correct) {
  if(!ready_ || totals_.answered==UINT32_MAX || (correct && totals_.points>UINT32_MAX-10)) return false;
  SavedQuiz saved;
  saved.totals=totals_; ++saved.totals.answered;
  if(correct) { ++saved.totals.correct; saved.totals.points+=10; }
  saved.checksum=checksum(saved);
  File file=LittleFS.open(kTemp,"w");
  if(!file) return false;
  const bool written=file.write(reinterpret_cast<const uint8_t*>(&saved),sizeof(saved))==sizeof(saved);
  file.flush(); file.close();
  SavedQuiz verified;
  if(!written || !read(kTemp,verified) || !LittleFS.rename(kTemp,kPath)) { LittleFS.remove(kTemp); return false; }
  totals_=saved.totals; return true;
}
uint32_t QuizGame::randomBelow(uint32_t bound) {
  rng_^=rng_<<13; rng_^=rng_>>17; rng_^=rng_<<5;
  return bound ? rng_%bound : 0;
}
bool QuizGame::start(bool learnedOnly,uint32_t seed,bool practice,bool pictureMode) {
  active_=false; count_=position_=roundCorrect_=0; selected_=-1; answered_=false;
  reviewMode_=false;
  pictureMode_=pictureMode; retryRound_=false; wrongCount_=0;
  practice_=practice; learnedOnly_=learnedOnly; rng_=seed?seed:1;
  uint32_t seen=0;
  // Reservoir sampling: no 893-word heap allocation and no duplicate questions.
  for(uint16_t i=0;i<kQuizWordCount;++i) {
    if(learnedOnly && !study_.find(kQuizWords[i].word)) continue;
    ++seen;
    if(count_<10) targets_[count_++]=i;
    else { const uint32_t slot=randomBelow(seen); if(slot<10) targets_[slot]=i; }
  }
  for(uint8_t i=count_;i>1;--i) {
    const uint8_t j=static_cast<uint8_t>(randomBelow(i));
    const uint16_t temp=targets_[i-1]; targets_[i-1]=targets_[j]; targets_[j]=temp;
  }
  active_=count_ && prepare(); return active_;
}
bool QuizGame::startReview(const char* word, const char* meaning, uint32_t now, uint32_t seed) {
  stop();
  const auto* record = study_.find(word);
  if (!record || !reviewDue(*record, now) || !*meaning) return false;
  reviewWord_ = word; reviewMeaning_ = meaning;
  rng_ = seed ? seed : 1;
  correctChoice_ = static_cast<uint8_t>(randomBelow(6));
  choices_[correctChoice_] = UINT16_MAX;
  uint8_t used = 0;
  for (uint16_t n = 0; n < kQuizWordCount + 128 && used < 5; ++n) {
    const uint16_t candidate = static_cast<uint16_t>(n < 128 ? randomBelow(kQuizWordCount) : n - 128);
    const auto& item = kQuizWords[candidate];
    if (reviewWord_ == item.word || strstr(meaning, item.meaning) || strstr(item.meaning, meaning)) continue;
    bool allowed = true;
    for (uint8_t j = 0; j < used; ++j)
      if (!compatible(candidate, choices_[j + (j >= correctChoice_)])) allowed = false;
    if (allowed) { choices_[used + (used >= correctChoice_)] = candidate; ++used; }
  }
  if (used != 5) return false;
  reviewMode_ = true; practice_ = false; pictureMode_ = false; learnedOnly_ = true;
  count_ = 1; position_ = roundCorrect_ = wrongCount_ = 0;
  selected_ = -1; answered_ = retryRound_ = false; active_ = true;
  return true;
}

bool QuizGame::prepare() {
  selected_=-1; answered_=false;
  const uint16_t target=targets_[position_]; choices_[0]=target;
  uint8_t used=1;

  // A complete bounded traversal guarantees termination even for custom data.
  for(uint16_t n=0;n<kQuizWordCount+128 && used<6;++n) {
    const uint16_t candidate=static_cast<uint16_t>(n<128?randomBelow(kQuizWordCount):n-128);
    bool allowed=true;
    for(uint8_t j=0;j<used;++j) if(!compatible(candidate,choices_[j])) allowed=false;
    if(allowed) choices_[used++]=candidate;
  }
  if(used!=6) return false;
  for(uint8_t i=6;i>1;--i) {
    const uint8_t j=static_cast<uint8_t>(randomBelow(i));
    const uint16_t tmp=choices_[i-1]; choices_[i-1]=choices_[j]; choices_[j]=tmp;
  }
  for(uint8_t i=0;i<6;++i) if(choices_[i]==target) correctChoice_=i;
  return true;
}
bool QuizGame::answer(uint8_t choice, uint32_t now) {
  if(!active_ || finished() || answered_ || choice>=6 || (selected_>=0 && selected_!=choice)) return false;
  selected_=static_cast<int8_t>(choice);
  const bool correct=choice==correctChoice_;
  if (reviewMode_) {
    const auto* record = study_.find(reviewWord_.c_str());
    if (!record || !reviewDue(*record, now) || !study_.grade(reviewWord_.c_str(), now, correct, 0)) return false;
    answered_ = true;
    return true;
  }
  if(!practice_ && !store_.record(correct)) return false;
  answered_=true; if(correct) ++roundCorrect_;
  else if(wrongCount_<10) wrong_[wrongCount_++]=targetIndex();
  return true;
}
bool QuizGame::next() {
  if(!active_ || reviewMode_ || finished() || !answered_) return false;
  ++position_;
  if(!finished() && !prepare()) { active_=false; return false; }
  return true;
}
bool QuizGame::retryWrong() {
  if(!finished() || !wrongCount_) return false;
  count_=wrongCount_;
  for(uint8_t i=0;i<count_;++i) targets_[i]=wrong_[i];
  wrongCount_=0; position_=0; roundCorrect_=0; retryRound_=true;
  active_=prepare(); return active_;
}
const char* QuizGame::meaning() const { if(reviewing()) return reviewMeaning_.c_str(); return active_ && !finished()?kQuizWords[targetIndex()].meaning:""; }
const char* QuizGame::word() const { if(reviewing()) return reviewWord_.c_str(); return active_ && !finished()?kQuizWords[targetIndex()].word:""; }
const char* QuizGame::choice(uint8_t index) const { if(reviewing() && index==correctChoice_) return meaning(); return active_ && !finished() && index<6?(pictureMode_?kQuizWords[choices_[index]].word:kQuizWords[choices_[index]].meaning):""; }
}
