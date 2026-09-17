#include <cassert>
#include <cstdio>
#include "study_policy.hpp"

int main() {
  using namespace wordmon;
  ReviewRecord record{};
  const uint32_t now = 1800000000;
  assert(studyDay(0) == -1);
  const uint32_t midnight = 20000UL * 86400 - 9 * 3600;
  assert(studyDay(midnight - 1) == 19999);
  assert(studyDay(midnight) == 20000);
  assert(!reviewDue(record, now));
  gradeReview(record, now, true);
  assert(record.stage == 1 && record.due == now + 86400);
  assert(!reviewDue(record, record.due - 1));
  assert(reviewDue(record, record.due));
  gradeReview(record, record.due, true);
  assert(record.stage == 2 && record.due == now + 4 * 86400);
  gradeReview(record, now, false);
  assert(record.stage == 0 && record.due == now + 600);
  for (int i = 0; i < 20; ++i) gradeReview(record, now, true);
  assert(record.stage == 5 && record.due == now + 30 * 86400);
  ReviewRecord before = record;
  gradeReview(record, 0, false);
  assert(record.due == before.due && record.stage == before.stage);
  assert(!reviewDue(record, 0));
  ReviewChoice choice;
  ReviewRecord late{};
  late.due = now - 10;
  ReviewRecord older{};
  older.due = now - 100;
  ReviewRecord future{};
  future.due = now + 10;
  choice.consider(3, 3, nullptr, now);  // Today's unstudied word.
  assert(choice.index() == 3);
  choice.consider(0, 3, &future, now);
  assert(choice.index() == 3);
  choice.consider(1, 3, &late, now);
  choice.consider(2, 3, &older, now);
  assert(choice.index() == 2);  // Overdue reviews precede new words.
  ReviewChoice done;
  done.consider(0, 0, &future, now);
  done.consider(1, 0, nullptr, now);
  assert(done.index() == -1);
  ReviewChoice noClock;
  noClock.consider(0, 0, nullptr, 0);
  assert(noClock.index() == -1);
  assert(syncAllowed(1000, false, false, 0, 0));
  assert(!syncAllowed(1001, true, false, 1000, 0));
  assert(syncAllowed(601000, true, false, 1000, 0));
  assert(!syncAllowed(5000, true, true, 1000, 1000));
  assert(syncAllowed(43201000, true, true, 1000, 1000));
  assert(!syncAllowed(43202000, true, true, 43201000, 1000));
  assert(!syncAllowed(10, true, false, UINT32_MAX - 20, 0));
  puts("study policy: all tests passed");
}
