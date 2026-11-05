#pragma once
#include <cstdint>
#include <cstddef>
namespace wordmon {
constexpr size_t kAssetCacheBudget = 512 * 1024;
inline uint64_t assetKey(const char* url) {
  uint64_t hash = 14695981039346656037ULL;
  while (*url) { hash ^= static_cast<uint8_t>(*url++); hash *= 1099511628211ULL; }
  return hash ? hash : 1;
}
class AssetRetries {
  struct Entry { uint64_t key = 0; uint32_t next = 0; uint8_t failures = 0; } entries_[16];
  size_t cursor_ = 0;
 public:
  bool ready(uint64_t key, uint32_t now) const {
    for (const auto& e : entries_) if (e.key == key) return int32_t(now - e.next) >= 0;
    return true;
  }
  void succeeded(uint64_t key) { for (auto& e : entries_) if (e.key == key) e = {}; }
  void failed(uint64_t key, uint32_t now) {
    Entry* slot = nullptr;
    for (auto& e : entries_) if (e.key == key) { slot = &e; break; }
    if (!slot) { slot = &entries_[cursor_++ % 16]; *slot = {}; slot->key = key; }
    if (slot->failures < 3) ++slot->failures;
    slot->next = now + (slot->failures == 1 ? 30000u : slot->failures == 2 ? 120000u : 600000u);
  }
  void reset() { for (auto& e : entries_) e = {}; }
};
}
