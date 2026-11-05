#include <cassert>
#include <cstdio>
#include "network_policy.hpp"
using namespace wordmon;
int main() {
  NetworkRecovery recovery;
  assert(recovery.poll(0, true, false) == NetworkAction::Restored);
  assert(recovery.poll(100, true, false) == NetworkAction::None);
  assert(recovery.poll(200, false, false) == NetworkAction::None);
  assert(recovery.poll(15199, false, false) == NetworkAction::None);
  assert(recovery.poll(15200, false, false) == NetworkAction::Retry);
  assert(recovery.poll(15201, false, false) == NetworkAction::None);
  assert(recovery.poll(30200, false, false) == NetworkAction::Retry);
  assert(recovery.poll(30500, true, false) == NetworkAction::Restored);
  assert(recovery.poll(50000, true, false) == NetworkAction::None);
  // No repeated STA scans while the phone uses the setup AP.
  assert(recovery.poll(60000, false, true) == NetworkAction::None);
  assert(recovery.poll(120000, false, true) == NetworkAction::None);
  NetworkRecovery wrapped;
  assert(wrapped.poll(UINT32_MAX - 1000, false, false) == NetworkAction::None);
  assert(wrapped.poll(13998, false, false) == NetworkAction::None);
  assert(wrapped.poll(13999, false, false) == NetworkAction::Retry);
  puts("network recovery retry, restored edge, setup AP and rollover: passed");
}
