#include <cassert>
#include <cstdio>
#include <string>
#include "Preferences.h"
#include "LittleFS.h"
#include "study_store.hpp"
std::vector<unsigned char> Preferences::bytes;
bool Preferences::failWrite = false;
bool File::failWrite = false;
TestLittleFS LittleFS;
int main() {
  using namespace wordmon;
  constexpr uint32_t now = 1800000000;
  StudyStore study;
  study.begin();
  assert(!study.grade("apple", 0, true));
  assert(!study.find("apple"));
  assert(study.grade("apple", now, false, 2));
  assert(study.grade("serene", now, true, 2));
  assert(study.newToday(now) == 2);
  assert(!study.grade("third", now, true, 2));
  assert(!study.find("third"));
  assert(study.grade("apple", now + 600, true, 2));
  assert(study.newToday(now) == 2);
  StudyStore reboot;
  reboot.begin();
  assert(reboot.find("apple")->due == now + 600 + 86400);
  assert(reboot.newToday(now) == 2);
  assert(reboot.newToday(now + 86400) == 0);
  ReviewChoice reordered;
  const char* deck[] = {"serene", "apple", "fresh"};
  for (uint16_t i = 0; i < 3; ++i)
    reordered.consider(i, reboot.allowsNew(now + 86400, 2), reboot.find(deck[i]), now + 86400);
  assert(reordered.index() == 0); // Due identity survives list reordering.
  ReviewChoice skippedDays;
  skippedDays.consider(0, reboot.allowsNew(now + 20 * 86400, 2), nullptr, now + 20 * 86400);
  skippedDays.consider(1, true, nullptr, now + 20 * 86400);
  assert(skippedDays.index() == 0); // A long break still starts at the first unseen.
  File::failWrite = true;
  assert(!reboot.grade("apple", now + 90000, true, 2));
  assert(reboot.find("apple")->stage == 1);
  assert(!reboot.grade("new", now + 86400, true, 2));
  assert(!reboot.find("new"));
  assert(reboot.newToday(now + 86400) == 0);
  File::failWrite = false;
  LittleFS.failRename = true;
  assert(!reboot.grade("new", now + 86400, true, 2));
  assert(reboot.newToday(now) == 2);
  LittleFS.failRename = false;
  assert(!reboot.grade("paused", now + 86400, true, 0));
  for (int i = 0; i < 978; ++i) {
    const uint32_t day = now + (1 + i / 20) * 86400;
    assert(reboot.grade(("word" + std::to_string(i)).c_str(), day, true, 20));
  }
  StudyStore large;
  large.begin();
  assert(large.find("word977"));
  assert(large.find("apple"));
  assert(large.newToday(now + 49 * 86400) == 18);
  for (int i = 0; i < 20; ++i)
    assert(large.grade(("last" + std::to_string(i)).c_str(), now + 60 * 86400, true, 20));
  assert(!large.grade("overflow", now + 61 * 86400, true, 20));
  auto& saved = *LittleFS.files.at("/study-v2.bin");
  saved.back() ^= 1;
  StudyStore corrupt;
  corrupt.begin();
  assert(!corrupt.grade("lost", now, true));
  LittleFS.files.clear();
  ReviewRecord legacy[128]{};
  legacy[0] = *large.find("apple");
  legacy[2] = *large.find("word977");
  legacy[2].stage = 5; // Sparse migration must not leak this into a new word.
  const auto* start = reinterpret_cast<const unsigned char*>(legacy);
  Preferences::bytes.assign(start, start + sizeof(legacy));
  StudyStore migrated;
  migrated.begin();
  assert(migrated.find("apple")->due == legacy[0].due);
  assert(migrated.newToday(now) == 0);
  assert(migrated.grade("new", now, true));
  assert(migrated.find("new")->stage == 1);
  assert(migrated.find("new")->due == now + 86400);
  StudyStore migratedReboot;
  migratedReboot.begin();
  assert(migratedReboot.find("apple"));
  assert(migratedReboot.newToday(now) == 1);
  LittleFS.capacity = 20000;
  LittleFS.files["/voice.wav"] = std::make_shared<std::vector<unsigned char>>(14000);
  LittleFS.files["/wb/a_apple.png"] = std::make_shared<std::vector<unsigned char>>(5000);
  LittleFS.files["/wb/words.jsonl"] = std::make_shared<std::vector<unsigned char>>(500);
  assert(migratedReboot.grade("low-space", now, true));
  assert(!LittleFS.exists("/voice.wav"));
  assert(!LittleFS.exists("/wb/a_apple.png"));
  assert(LittleFS.exists("/wb/words.jsonl"));
  StudyStore reclaimed;
  reclaimed.begin();
  assert(reclaimed.find("apple"));
  assert(reclaimed.find("low-space"));
  LittleFS.files["/voice.tmp"] = std::make_shared<std::vector<unsigned char>>(14000);
  LittleFS.files["/wb/words.tmp"] = std::make_shared<std::vector<unsigned char>>(5000);
  StudyStore orphanRecovery;
  orphanRecovery.begin();
  assert(!LittleFS.exists("/voice.tmp"));
  assert(!LittleFS.exists("/wb/words.tmp"));
  assert(orphanRecovery.grade("orphan-recovered", now, true));
  LittleFS.files["/wb/words.jsonl"] = std::make_shared<std::vector<unsigned char>>(19000);
  assert(!orphanRecovery.grade("cannot-fit", now, true));
  assert(!orphanRecovery.find("cannot-fit"));
  assert(orphanRecovery.newToday(now) == 3);
  LittleFS.files.clear();
  LittleFS.capacity = 128 * 1024;
  Preferences::bytes.clear();
  StudyStore midnight;
  midnight.begin();
  const uint32_t boundary = static_cast<uint32_t>(studyDay(now) + 1) * 86400 - 9 * 3600;
  assert(midnight.grade("before-midnight", boundary - 1, false, 1));
  assert(midnight.newToday(boundary - 1) == 1);
  assert(midnight.newToday(boundary) == 0);
  assert(midnight.grade("after-midnight", boundary, true, 1));
  assert(midnight.find("before-midnight")->due == boundary + 599);
  assert(midnight.newToday(boundary) == 1);
  puts("study persistence, quota, migration and 1000 records: all tests passed");
}
