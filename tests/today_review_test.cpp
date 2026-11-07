#include "today_review.hpp"
#include "sentence_translations.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace wordmon;
int main() {
  constexpr uint32_t now=1793840400;
  for(unsigned stage=0;stage<=5;++stage) {
    static constexpr uint32_t interval[]={600,86400,3*86400,7*86400,14*86400,30*86400};
    ReviewRecord record;record.stage=static_cast<uint8_t>(stage);record.due=now+interval[stage];
    assert(reviewedAt(record)==now && reviewedToday(record,now));
    assert(missedToday(record,now) == (stage == 0));
    assert(!missedToday(record,now+86400));
    assert(!reviewedToday(record,now+86400));
    assert(!reviewedToday(record,now-1));
  }
  ReviewRecord invalid; assert(!reviewedToday(invalid,now));
  invalid.stage=6;invalid.due=now;assert(!reviewedToday(invalid,now));
  const uint32_t midnight=static_cast<uint32_t>(studyDay(now))*86400-9*3600;
  ReviewRecord record;record.due=midnight-1+600;
  assert(reviewedToday(record,midnight-1) && !reviewedToday(record,midnight));
  TodayPlayback play;play.reset(100);
  assert(!play.tick(12099,false) && play.tick(12100,false));
  play.toggle(12101);assert(!play.tick(90000,false));
  play.toggle(90000);assert(!play.tick(100000,false));
  assert(!play.tick(110000,true));assert(!play.tick(121999,false));
  assert(play.tick(122000,false));
  play.reset(UINT32_MAX-100);assert(!play.tick(100,false));assert(play.tick(12000,false));
  assert(*sentenceTranslation("I write my notes with a pencil."));
  assert(!*sentenceTranslation("A custom sentence never in this pack."));
  assert(strcmp(sentenceTranslation("Draw the line with a ruler."),sentenceTranslation("I write my notes with a pencil.")));
  puts("today membership, midnight, auto/pause, rollover and sentence lookup: passed");
}
