#pragma once
#include <LittleFS.h>
#include <cstddef>
#include <cstdint>

namespace wordmon {
enum class ResumeMode : uint8_t { Home, Study, ReviewOnly, Replay, Missed };

struct ResumeMark {
  uint64_t deck = 0, wordHash = 0;
  int32_t day = -1;
  ResumeMode mode = ResumeMode::Home;
  uint8_t reserved[3]{};
  ResumeMark() = default;
  ResumeMark(uint64_t deckKey, uint64_t wordKey, int32_t date, ResumeMode view)
      : deck(deckKey), wordHash(wordKey), day(date), mode(view) {}
  bool matches(uint64_t deckKey, int32_t date) const {
    return deck && wordHash && deck == deckKey && day >= 0 && day == date;
  }
  bool operator==(const ResumeMark& other) const {
    return deck == other.deck && wordHash == other.wordHash && day == other.day && mode == other.mode;
  }
};

class ResumeStore {
 public:
  void begin() {
    if (LittleFS.exists("/resume-v1.tmp")) LittleFS.remove("/resume-v1.tmp");
    Record saved;
    if (read("/resume-v1.bin", saved)) mark_ = saved.mark;
  }
  const ResumeMark& mark() const { return mark_; }
  bool save(const ResumeMark& mark) {
    if (mark == mark_) return true;
    Record saved; saved.mark = mark; saved.checksum = checksum(saved);
    File out = LittleFS.open("/resume-v1.tmp", "w");
    if (!out) return false;
    const bool written = out.write(reinterpret_cast<const uint8_t*>(&saved), sizeof(saved)) == sizeof(saved);
    out.flush(); out.close();
    Record verified;
    if (!written || !read("/resume-v1.tmp", verified) || !LittleFS.rename("/resume-v1.tmp", "/resume-v1.bin")) {
      LittleFS.remove("/resume-v1.tmp");
      return false;
    }
    mark_ = mark;
    return true;
  }
 private:
  struct Record { ResumeMark mark; uint32_t magic = 0x31524D57, checksum = 0; };
  static_assert(sizeof(Record) == 32, "resume record layout changed");
  static uint32_t checksum(const Record& saved) {
    uint32_t hash = 2166136261UL;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&saved);
    for (size_t i = 0; i < offsetof(Record, checksum); ++i) { hash ^= bytes[i]; hash *= 16777619UL; }
    return hash;
  }
  static bool read(const char* path, Record& saved) {
    if (!LittleFS.exists(path)) return false;
    File file = LittleFS.open(path, "r");
    return file && file.size() == sizeof(saved) &&
        file.read(reinterpret_cast<uint8_t*>(&saved), sizeof(saved)) == sizeof(saved) &&
        saved.magic == 0x31524D57 && saved.checksum == checksum(saved) &&
        saved.mark.mode <= ResumeMode::Missed;
  }
  ResumeMark mark_;
};
}  // namespace wordmon
