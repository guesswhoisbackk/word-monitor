#pragma once
#include <cmath>
#include <cstdint>

namespace wordmon {
// WMO interpretation codes returned by Open-Meteo.
inline bool snowyWeather(int code) {
  return code == 71 || code == 73 || code == 75 || code == 77 || code == 85 || code == 86;
}
inline bool rainyWeather(int code) {
  return code == 51 || code == 53 || code == 55 || code == 56 || code == 57 ||
         code == 61 || code == 63 || code == 65 || code == 66 || code == 67 ||
         code == 80 || code == 81 || code == 82;
}
inline bool stormyWeather(int code) { return code == 95 || code == 96 || code == 99; }
inline const char* weatherDescription(int code) {
  switch (code) {
    case 0: return "맑음";
    case 1: return "대체로 맑음";
    case 2: return "구름 조금";
    case 3: return "흐림";
    case 45: case 48: return "안개";
    case 51: case 53: case 55: return "이슬비";
    case 56: case 57: return "어는 이슬비";
    case 61: case 63: case 65: return "비";
    case 66: case 67: return "어는 비";
    case 71: case 73: case 75: return "눈";
    case 77: return "싸락눈";
    case 80: case 81: case 82: return "소나기";
    case 85: case 86: return "눈 소나기";
    case 95: return "뇌우";
    case 96: case 99: return "우박·뇌우";
    default: return "날씨 정보";
  }
}
// Lightweight comfort suggestions, not an official forecast product. Use the
// provider's apparent temperature where available, otherwise air temperature.
inline const char* clothingSuggestion(float temperature) {
  if (!std::isfinite(temperature)) return "날씨 확인 후 선택";
  if (temperature >= 28) return "반팔·반바지";
  if (temperature >= 23) return "반팔·얇은 긴바지";
  if (temperature >= 20) return "긴팔·얇은 긴바지";
  if (temperature >= 17) return "긴팔·가디건";
  if (temperature >= 12) return "니트·가벼운 겉옷";
  if (temperature >= 9) return "니트·재킷";
  if (temperature >= 5) return "코트·도톰한 옷";
  if (temperature >= 0) return "패딩·목도리";
  return "두꺼운 패딩·장갑";
}
inline const char* weatherAccessory(int code, int rain, float temperature) {
  if (stormyWeather(code)) return "뇌우 주의";
  if (snowyWeather(code)) return "눈길 조심";
  if (rainyWeather(code) || rain >= 60) return "우산 챙기기";
  if (code == 45 || code == 48) return "안개 조심";
  if (temperature >= 23) return "햇볕엔 모자";
  if (temperature < 5) return "따뜻하게 입기";
  return "겹쳐 입기";
}
inline bool weatherStale(uint32_t now, uint32_t receivedAt) {
  return uint32_t(now - receivedAt) > 1800000u;
}
}
