#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <functional>
namespace fake {
inline std::map<std::string,std::vector<uint8_t>> preferences;
inline bool nvs_write_fail=false;
inline std::function<void()> during_nvs_write;
}
class Preferences {
public:
 bool begin(const char *) { return true; }
 bool isKey(const char *key) { return fake::preferences.count(key); }
 size_t putBytes(const char *key,const void *p,size_t size) {
   if(fake::nvs_write_fail)return 0;
   if(fake::during_nvs_write)fake::during_nvs_write();
   const auto *bytes=static_cast<const uint8_t *>(p);fake::preferences[key]={bytes,bytes+size};return size;
 }
 size_t getBytesLength(const char *key) { return fake::preferences[key].size(); }
 size_t getBytes(const char *key,void *p,size_t size) {
   const auto &bytes=fake::preferences[key];if(size<bytes.size())return 0;
   std::memcpy(p,bytes.data(),bytes.size());return bytes.size();
 }
 size_t putUShort(const char *key,uint16_t value) { return putBytes(key,&value,sizeof(value)); }
 uint16_t getUShort(const char *key) { uint16_t value=0;getBytes(key,&value,sizeof(value));return value; }
 void clear() { fake::preferences.clear(); }
};
