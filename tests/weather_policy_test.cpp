#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include "weather_policy.hpp"
using namespace wordmon;
int main() {
  RainPeriods periods;
  assert(periods.morning == -1 && periods.noon == -1 && periods.evening == -1);
  periods.consider(5, 100, 12);  // Overnight is not morning.
  periods.consider(7, 90, 12);   // Past morning rain must not drive noon's umbrella suggestion.
  periods.consider(10, 60, 12);
  periods.consider(11, 10, 12);
  periods.consider(12, 20, 12);
  periods.consider(16, 15, 12);
  periods.consider(17, 25, 12);
  periods.consider(23, 30, 12);
  periods.consider(24, 100, 12);
  periods.consider(19, -1, 12);
  periods.consider(19, 101, 12);
  assert(periods.morning == 90 && periods.noon == 20 && periods.evening == 30 && periods.remaining == 30);
  assert(strcmp(weatherAccessory(0, periods.remaining, 14), "우산 챙기기") != 0);
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
