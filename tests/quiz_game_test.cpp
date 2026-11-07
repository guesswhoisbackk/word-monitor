#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include "Preferences.h"
#include "LittleFS.h"
#include "quiz_game.hpp"
#include "quiz_content.hpp"
#include "content_corrections.hpp"
std::vector<unsigned char> Preferences::bytes;
bool Preferences::failWrite = false;
bool File::failWrite = false;
TestLittleFS LittleFS;

int main() {
  using namespace wordmon;
  StudyStore study; study.begin();
  QuizGame game(study); game.begin();
  assert(!game.start(true,1));
  assert(!game.answer(0) && !game.next());
  constexpr uint32_t now=1800000000;
  for(const char* word : {"pencil","eraser","ruler"}) assert(study.grade(word,now,true));
  const auto history=*LittleFS.files["/study-v2.bin"];
  const auto revision=study.revision();
  assert(game.start(true,42) && game.count()==3);
  std::set<std::string> targets;
  targets.insert(game.word());
  assert(!game.answer(6));
  const auto correct=game.correctChoice();
  File::failWrite=true;
  assert(!game.answer(correct) && !game.answered() && game.totals().points==0);
  assert(!game.answer((correct+1)%6)); // Failed save cannot switch the first answer.
  File::failWrite=false; LittleFS.failRename=true;
  assert(!game.answer(correct) && game.totals().points==0);
  LittleFS.failRename=false;
  assert(game.answer(correct) && game.totals().points==10);
  for(int n=0;n<100;++n) assert(!game.answer(correct));
  assert(game.totals().points==10 && game.totals().answered==1);
  QuizGame reboot(study); reboot.begin();
  assert(reboot.storageReady() && reboot.totals().points==10);
  assert(game.next() && targets.insert(game.word()).second);
  assert(game.answer((game.correctChoice()+1)%6));
  assert(!game.answer(game.correctChoice()) && game.totals().points==10);
  assert(game.next() && targets.insert(game.word()).second);
  assert(game.answer(game.correctChoice()) && game.next() && game.finished());
  assert(game.roundCorrect()==2 && game.totals().points==20 && game.totals().answered==3);
  assert(!game.answer(0) && !game.next());
  game.stop(); assert(!game.active());
  const auto pointsFile=*LittleFS.files["/quiz-v1.bin"];
  std::set<uint16_t> coverage;
  unsigned slots[6]{};
  for(uint32_t seed=1;seed<=300;++seed) {
    assert(game.start(false,seed,true) && game.count()==10);
    std::set<uint16_t> round;
    while(!game.finished()) {
      assert(round.insert(game.targetIndex()).second);
      coverage.insert(game.targetIndex()); ++slots[game.correctChoice()];
      assert(!strcmp(game.choice(game.correctChoice()),kQuizWords[game.targetIndex()].meaning));
      for(uint8_t i=0;i<6;++i) {
        assert(*game.choice(i));
        for(uint8_t j=0;j<i;++j) {
          assert(!strstr(game.choice(i),game.choice(j)));
          assert(!strstr(game.choice(j),game.choice(i)));
        }
      }
      assert(game.answer(game.correctChoice()));
      assert(!game.answer(game.correctChoice()));
      assert(game.next());
    }
    assert(game.roundCorrect()==10 && game.totals().points==20);
  }
  assert(coverage.size()>800);
  for(auto n:slots) assert(n>300);
  assert(*LittleFS.files["/quiz-v1.bin"]==pointsFile);
  assert(*LittleFS.files["/study-v2.bin"]==history && study.revision()==revision);
  assert(game.start(true,77,true,true));
  std::set<uint16_t> missed;
  while(!game.finished()) {
    assert(!strcmp(game.choice(game.correctChoice()),game.word()));
    assert(*game.meaning()); missed.insert(game.targetIndex());
    assert(game.answer((game.correctChoice()+1)%6)); assert(game.next());
  }
  assert(game.wrongCount()==3 && game.retryWrong() && game.retryRound() && game.pictureMode());
  assert(game.count()==3 && game.roundCorrect()==0);
  while(!game.finished()) {
    assert(missed.erase(game.targetIndex())==1);
    assert(game.answer(game.correctChoice())); assert(game.next());
  }
  assert(missed.empty() && game.roundCorrect()==3 && !game.wrongCount() && !game.retryWrong());
  assert(*LittleFS.files["/quiz-v1.bin"]==pointsFile);
  assert(*LittleFS.files["/study-v2.bin"]==history);
  // Scheduled review grades the current learned word, not the points game.
  const uint32_t due = now + 86400;
  assert(!game.startReview("pencil", "연필", now, 2));
  assert(!game.startReview("unknown", "처음", due, 2));
  assert(game.startReview("pencil", "연필", due, 2));
  assert(!strcmp(game.word(), "pencil"));
  assert(!strcmp(game.choice(game.correctChoice()), "연필"));
  const uint8_t wrong = (game.correctChoice() + 1) % 6;
  File::failWrite = true;
  assert(!game.answer(wrong, due) && !game.answered());
  File::failWrite = false;
  assert(!game.answer(game.correctChoice(), due));
  assert(game.answer(wrong, due) && !game.answer(wrong, due));
  assert(study.find("pencil")->stage == 0 && study.find("pencil")->due == due + 600);
  assert(game.startReview("eraser", "지우개", due, 3));
  assert(game.answer(game.correctChoice(), due));
  assert(study.find("eraser")->stage == 2 && study.find("eraser")->due == due + 3 * 86400);
  assert(*LittleFS.files["/quiz-v1.bin"] == pointsFile);
  // An incomplete temporary transaction never replaces committed points.
  LittleFS.files["/quiz-v1.tmp"]=std::make_shared<std::vector<unsigned char>>(size_t(3),static_cast<unsigned char>(0));
  QuizStore interrupted; interrupted.begin();
  assert(interrupted.ready() && interrupted.totals().points==20 && !LittleFS.exists("/quiz-v1.tmp"));
  (*LittleFS.files["/quiz-v1.bin"])[5]^=1;
  QuizStore corrupt; corrupt.begin();
  assert(!corrupt.ready() && !corrupt.record(true));
  assert(!strcmp(correctedMeaning("moon","초승달"),"달"));
  assert(!strcmp(correctedMeaning("moon","custom"),"custom"));
  assert(!strcmp(correctedExample("ear","Music goes in one ear, stays."),"She held the phone to her ear."));
  assert(!strcmp(correctedExample("ear","My own example."),"My own example."));
  puts("quiz: 3000 questions, six unique choices, scoring, duplicate taps, save failure, reboot, corruption and history isolation passed");
}
