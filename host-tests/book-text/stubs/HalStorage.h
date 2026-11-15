#pragma once
#define HalStorage BookTextStorageBase
#include "../../book-dictionaries/stubs/HalStorage.h"
#undef HalStorage
#undef Storage

class HalStorage : public BookTextStorageBase {
 public:
  static HalStorage& getInstance() {
    static HalStorage storage;
    return storage;
  }
  bool mkdir(const char* path) { return !exists(path) && ensureDirectoryExists(path); }
};
#define Storage HalStorage::getInstance()
