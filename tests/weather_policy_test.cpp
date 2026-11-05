#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include "weather_policy.hpp"
using namespace wordmon;
int main() {
  assert(strcmp(weatherDescription(0), "맑음") == 0);
  assert(strcmp(weatherDescription(2), "구름 조금") == 0);
  assert(strcmp(weatherDescription(67), "어는 비") == 0);
  assert(strcmp(weatherDescription(98), "날씨 정보") == 0);
  // Snow and thunderstorms must not be replaced by a generic umbrella tip,
  // even when today's maximum precipitation probability is high.
  assert(strcmp(weatherAccessory(85, 100, -3), "눈길 조심") == 0);
  assert(strcmp(weatherAccessory(99, 100, 14), "뇌우 주의") == 0);
  assert(strcmp(weatherAccessory(0, 59, 14), "우산 챙기기") != 0);
  assert(strcmp(weatherAccessory(0, 60, 14), "우산 챙기기") == 0);
  assert(strcmp(weatherAccessory(61, 0, 14), "우산 챙기기") == 0);
  assert(strstr(clothingSuggestion(-5), "패딩") != nullptr);
  assert(strstr(clothingSuggestion(29), "반팔") != nullptr);
  assert(strcmp(clothingSuggestion(std::numeric_limits<float>::quiet_NaN()), "날씨 확인 후 선택") == 0);
  assert(!weatherStale(1800000, 0));
  assert(weatherStale(1800001, 0));
  const uint32_t receivedBeforeWrap = UINT32_MAX - 1000;
  assert(!weatherStale(1000, receivedBeforeWrap));
  assert(weatherStale(1800000, receivedBeforeWrap));
  puts("weather Korean labels, clothing, precipitation priority and stale rollover: passed");
}
