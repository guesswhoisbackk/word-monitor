#pragma once

#include <Preferences.h>

#include "models.hpp"

namespace wordmon {

class SettingsStore {
 public:
  bool load(AppSettings& settings);
  bool save(const AppSettings& settings);
  void clear();

 private:
  Preferences preferences_;
};

}  // namespace wordmon
