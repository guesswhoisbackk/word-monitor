#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
size_t testFreeBytes();
struct File {
  std::shared_ptr<std::vector<unsigned char>> data;
  size_t offset = 0;
  std::string path;
  std::vector<std::string> entries;
  size_t next = 0;
  bool directory = false;
  static bool failWrite;
  File() = default;
  File(std::shared_ptr<std::vector<unsigned char>> bytes, const std::string& name)
      : data(bytes), path(name) {}
  explicit operator bool() const { return bool(data) || directory; }
  const char* name() const { return path.c_str(); }
  File openNextFile();
  size_t size() const { return data ? data->size() : 0; }
  size_t read(uint8_t* out, size_t length) {
    if (!data) return 0;
    length = std::min(length, data->size() - offset);
    memcpy(out, data->data() + offset, length); offset += length; return length;
  }
  size_t write(const uint8_t* in, size_t length) {
    if (!data || failWrite || length > testFreeBytes()) return 0;
    data->insert(data->end(), in, in + length); return length;
  }
  void flush() {}
  void close() { data.reset(); }
};
struct TestLittleFS {
  std::map<std::string, std::shared_ptr<std::vector<unsigned char>>> files;
  bool failRename = false;
  size_t capacity = 128 * 1024;
  size_t totalBytes() const { return capacity; }
  size_t usedBytes() const {
    size_t used = 0;
    for (const auto& item : files) used += item.second->size();
    return used;
  }
  bool begin(bool = false) { return true; }
  bool exists(const char* path) { return files.count(path) != 0; }
  File open(const char* path, const char* mode = "r") {
    if (std::string(path) == "/wb") {
      File folder; folder.directory = true;
      for (const auto& item : files) if (item.first.find("/wb/") == 0) folder.entries.push_back(item.first);
      return folder;
    }
    if (*mode == 'w') files[path] = std::make_shared<std::vector<unsigned char>>();
    auto found = files.find(path);
    return found == files.end() ? File{} : File{found->second, path};
  }
  bool remove(const char* path) { return files.erase(path) != 0; }
  bool rename(const char* from, const char* to) {
    if (failRename || !exists(from)) return false;
    files[to] = files[from]; files.erase(from); return true;
  }
};
extern TestLittleFS LittleFS;
inline File File::openNextFile() {
  return next < entries.size() ? LittleFS.open(entries[next++].c_str()) : File{};
}
inline size_t testFreeBytes() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }
