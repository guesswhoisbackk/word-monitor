#include "study_store.hpp"
#include <Preferences.h>
#include <LittleFS.h>
#include <cstring>
#include <cstdio>
#include <new>

namespace wordmon {
namespace {
constexpr const char* kState = "/study-v2.bin";
constexpr const char* kTemp = "/study-v2.tmp";
struct Header {
  uint32_t magic = 0x32534D57;
  uint32_t count = 0;
  int32_t day = -1;
  uint32_t introduced = 0;
  uint32_t checksum = 0;
};
static_assert(sizeof(ReviewRecord) == 16, "v1 migration layout must stay stable");
uint32_t checksum(const void* data, size_t size, uint32_t hash = 2166136261UL) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 16777619UL; }
  return hash;
}
uint64_t wordKey(const char* word) {
  uint64_t hash = 14695981039346656037ULL;
  while (*word) { hash ^= static_cast<uint8_t>(*word++); hash *= 1099511628211ULL; }
  return hash ? hash : 1;
}
bool readState(const char* path, Header& header, ReviewRecord* records = nullptr) {
  File file = LittleFS.open(path, "r");
  if (!file || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
      header.magic != 0x32534D57 || header.count > kMaxStudyWords || header.introduced > 20 ||
      file.size() != sizeof(header) + header.count * sizeof(ReviewRecord)) return false;
  uint32_t hash = checksum(&header, offsetof(Header, checksum));
  for (uint32_t i = 0; i < header.count; ++i) {
    ReviewRecord record;
    if (file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record)) != sizeof(record) ||
        !record.key || record.stage > 5 || record.due < 1700000000) return false;
    hash = checksum(&record, sizeof(record), hash);
    if (records) records[i] = record;
  }
  return hash == header.checksum;
}
}
StudyStore::~StudyStore() { delete[] records_; }
void StudyStore::begin() {
  if (records_) return;
  records_ = new (std::nothrow) ReviewRecord[kCapacity]{};
  if (!records_ || !LittleFS.begin(false)) return;
  LittleFS.remove(kTemp); // An interrupted transaction never becomes saved history.
  // Startup runs before either worker can own a download transaction.
  LittleFS.remove("/voice.tmp");
  LittleFS.remove("/wb/words.tmp");
  if (LittleFS.exists(kState)) {
    Header header;
    if (!readState(kState, header, records_)) return; // Fail closed on corruption.
    count_ = static_cast<uint16_t>(header.count);
    newDay_ = header.day;
    newCount_ = static_cast<uint16_t>(header.introduced);
    ready_ = true;
    return;
  }
  Preferences prefs;
  if (!prefs.begin("wm-study", false)) return;
  constexpr size_t legacyBytes = 128 * sizeof(ReviewRecord);
  const size_t size = prefs.getBytesLength("reviews-v1");
  if (size && (size != legacyBytes || prefs.getBytes("reviews-v1", records_, size) != size)) {
    prefs.end(); return;
  }
  prefs.end();
  // Compact the old sparse array without changing word identities or due dates.
  for (size_t i = 0; i < 128; ++i) {
    if (!records_[i].key) continue;
    if (records_[i].stage > 5 || records_[i].due < 1700000000) return;
    records_[count_++] = records_[i];
  }
  ready_ = !count_ || save();
}
const ReviewRecord* StudyStore::find(const char* word) const {
  if (!ready_ || !word || !*word) return nullptr;
  const uint64_t key = wordKey(word);
  for (size_t i = 0; i < count_; ++i) if (records_[i].key == key) return &records_[i];
  return nullptr;
}
uint16_t StudyStore::newToday(uint32_t now) const {
  const int32_t day = studyDay(now);
  return day >= 0 && day == newDay_ ? newCount_ : 0;
}
bool StudyStore::allowsNew(uint32_t now, uint8_t limit) const {
  return ready_ && studyDay(now) >= 0 && newToday(now) < limit && count_ < kCapacity;
}
bool StudyStore::save() {
  const size_t required = sizeof(Header) + count_ * sizeof(ReviewRecord) + 8192;
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < required) {
    // Art/audio are downloadable. History and the word list are not evicted.
    LittleFS.remove("/voice.wav");
    LittleFS.remove("/voice.key");
    File root = LittleFS.open("/wb");
    File entry = root.openNextFile();
    while (entry) {
      const char* name = entry.name();
      const char* leaf = strrchr(name, '/');
      leaf = leaf ? leaf + 1 : name;
      const bool art = strncmp(leaf, "a_", 2) == 0;
      char path[64];
      if (art) snprintf(path, sizeof(path), "/wb/%s", leaf);
      entry.close();
      if (art) LittleFS.remove(path);
      entry = root.openNextFile();
    }
  }
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < required) return false;
  Header header;
  header.count = count_; header.day = newDay_; header.introduced = newCount_;
  header.checksum = checksum(records_, count_ * sizeof(ReviewRecord),
                             checksum(&header, offsetof(Header, checksum)));
  File out = LittleFS.open(kTemp, "w");
  if (!out) return false;
  const bool written = out.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
      out.write(reinterpret_cast<const uint8_t*>(records_), count_ * sizeof(ReviewRecord)) == count_ * sizeof(ReviewRecord);
  out.flush(); out.close();
  Header verified;
  const bool saved = written && readState(kTemp, verified) && LittleFS.rename(kTemp, kState);
  if (!saved) LittleFS.remove(kTemp);
  return saved;
}
bool StudyStore::grade(const char* word, uint32_t now, bool remembered, uint8_t limit) {
  if (!ready_ || !word || !*word || studyDay(now) < 0) return false;
  ReviewRecord* target = nullptr;
  const uint64_t key = wordKey(word);
  for (size_t i = 0; i < count_; ++i) if (records_[i].key == key) { target = &records_[i]; break; }
  const bool isNew = !target;
  if (isNew && !allowsNew(now, limit)) return false;
  if (!target) target = &records_[count_];
  const ReviewRecord before = *target;
  const int32_t oldDay = newDay_;
  const uint16_t oldCount = newCount_;
  if (isNew) *target = ReviewRecord{};
  target->key = key;
  gradeReview(*target, now, remembered);
  if (isNew) {
    ++count_;
    newCount_ = static_cast<uint16_t>(newToday(now) + 1);
    newDay_ = studyDay(now);
  }
  if (save()) return true;
  *target = before;
  if (isNew) --count_;
  newDay_ = oldDay; newCount_ = oldCount;
  return false;
}
}
