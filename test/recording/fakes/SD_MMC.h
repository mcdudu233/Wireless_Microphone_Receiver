#pragma once
#include "FS.h"
#include <functional>
namespace fake { inline unsigned mounts=0;inline std::function<void()> during_mount; }
enum { GPIO_NUM_9=9, GPIO_NUM_10=10, GPIO_NUM_11=11, GPIO_NUM_12=12, GPIO_NUM_13=13, GPIO_NUM_14=14 };
enum { CARD_MMC=1, CARD_SD=2, CARD_SDHC=3 };
class FakeSD {
public:
  template<class... T> bool setPins(T...) { return true; }
  bool begin(const char *, bool, bool, unsigned) { ++fake::mounts;if(fake::during_mount)fake::during_mount();fake::card_mounted=fake::inserted;return fake::card_mounted; }
  void end() { fake::card_mounted=false; }
  int cardType() { return CARD_SDHC; }
  int sectorSize() { return 512; } int numSectors() { return 100000; }
  uint64_t cardSize() { return 51200000; } uint64_t totalBytes() { return cardSize(); }
  uint64_t usedBytes() { uint64_t n=0;for(const auto &file:fake::files)n+=file.second.bytes.size();return n; }
  bool exists(const char *path) { fake::valid(path);return fake::inserted && fake::card_mounted && fake::files.count(path); }
  File open(const char *path, const char *mode=FILE_READ) {
    fake::valid(path);
    if(!fake::inserted || !fake::card_mounted) return {};
    const bool writable=std::strcmp(mode,FILE_WRITE)==0;
    if(writable) {
      auto parent=std::string(path).substr(0,std::string(path).find_last_of('/'));if(parent.empty())parent="/";
      if(!fake::files.count(parent) || !fake::files.at(parent).directory)return {};
      fake::files[path]={false,{}};
    }
    return fake::files.count(path) ? File(path,writable) : File();
  }
  bool mkdir(const char *path) { fake::valid(path);if(!fake::inserted || !fake::card_mounted)return false;fake::files[path]={true,{}};return true; }
  bool remove(const char *path) { fake::valid(path);return fake::inserted && fake::files.erase(path); }
  bool rename(const char *from, const char *to) {
    fake::valid(from);fake::valid(to);
    if(!fake::inserted || !fake::card_mounted || !fake::files.count(from) || fake::files.count(to))return false;
    fake::files[to]=std::move(fake::files.at(from));fake::files.erase(from);return true;
  }
  bool readRAW(uint8_t *, uint32_t) { return fake::inserted && fake::card_mounted; }
  bool writeRAW(uint8_t *, uint32_t) { return fake::inserted && fake::card_mounted; }
};
inline FakeSD SD_MMC;
