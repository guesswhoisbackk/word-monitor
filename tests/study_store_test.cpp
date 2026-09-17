#include <cassert>
#include <cstdio>
#include <string>
#include "Preferences.h"
#include "study_store.hpp"
std::vector<unsigned char> Preferences::bytes;
bool Preferences::failWrite = false;
int main() {
  using namespace wordmon;
  constexpr uint32_t now = 1800000000;
  StudyStore study;
  study.begin();
  assert(!study.grade("apple", 0, true));
  assert(!study.find("apple"));
  assert(study.grade("apple", now, false));
  assert(study.grade("serene", now, true));
  StudyStore reboot;
  reboot.begin();
  assert(reboot.find("apple")->due == now + 600);
  assert(reboot.find("serene")->stage == 1);
  Preferences::failWrite = true;
  assert(!reboot.grade("apple", now, true));
  assert(reboot.find("apple")->due == now + 600);
  assert(!reboot.grade("new", now, true));
  assert(!reboot.find("new"));
  Preferences::failWrite = false;
  for (int i = 0; i < 126; ++i) {
    assert(reboot.grade(("word" + std::to_string(i)).c_str(), now, true));
  }
  assert(!reboot.grade("overflow", now, true));
  assert(reboot.find("apple")->due == now + 600);
  assert(reboot.grade("apple", now + 600, true));
  assert(reboot.find("apple")->stage == 1);
  puts("study persistence: all tests passed");
}
