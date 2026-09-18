#pragma once
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"
namespace fake {
struct Node { bool directory; std::vector<uint8_t> bytes; };
inline std::map<std::string, Node> files;
inline bool inserted = true, card_mounted = false, corrupt_reads = false;
inline size_t write_limit = SIZE_MAX;
inline uint32_t clock_ms = 0;
inline void valid(const char *path) {
  assert(path && path[0]=='/' && !std::strstr(path,"//") && !std::strstr(path,".."));
  assert(std::strcmp(path,"/")==0 || path[std::strlen(path)-1]!='/');
}
}
class File {
  struct Handle { std::string path; size_t offset=0, entry=0; bool writable=false; std::vector<std::string> children; };
  std::shared_ptr<Handle> handle;
public:
  File()=default;
  File(std::string path, bool writable=false) : handle(std::make_shared<Handle>()) {
    handle->path=std::move(path); handle->writable=writable;
    if(isDirectory()) for(const auto &entry:fake::files) {
      const auto prefix=handle->path=="/" ? "/" : handle->path+"/";
      if(entry.first.rfind(prefix,0)==0 && entry.first.size()>prefix.size() && entry.first.find('/',prefix.size())==std::string::npos)
        handle->children.push_back(entry.first);
    }
  }
  explicit operator bool() const { return handle && fake::inserted && fake::card_mounted && fake::files.count(handle->path); }
  bool isDirectory() const { return static_cast<bool>(*this) && fake::files.at(handle->path).directory; }
  const char *name() const { assert(handle); return handle->path.c_str(); }
  size_t size() const { return *this ? fake::files.at(handle->path).bytes.size() : 0; }
  void close() { handle.reset(); }
  void flush() {}
  bool seek(uint32_t offset) { if(!*this || isDirectory()) return false; handle->offset=offset; return true; }
  size_t write(const uint8_t *p, size_t count) {
    if(!*this || !handle->writable || isDirectory()) return 0;
    count=std::min(count,fake::write_limit);
    if(fake::write_limit!=SIZE_MAX) fake::write_limit-=count;
    auto &bytes=fake::files.at(handle->path).bytes;
    if(bytes.size()<handle->offset+count) bytes.resize(handle->offset+count);
    std::memcpy(bytes.data()+handle->offset,p,count);handle->offset+=count;return count;
  }
  size_t print(const char *text) { return write(reinterpret_cast<const uint8_t *>(text),std::strlen(text)); }
  size_t read(uint8_t *p, size_t count) {
    if(!*this || isDirectory()) return 0;
    const auto &bytes=fake::files.at(handle->path).bytes;
    count=handle->offset<bytes.size() ? std::min(count,bytes.size()-handle->offset) : 0;
    std::memcpy(p,bytes.data()+handle->offset,count);handle->offset+=count;
    if(fake::corrupt_reads && count) p[0]^=1;
    return count;
  }
  File openNextFile() { return handle && handle->entry<handle->children.size() ? File(handle->children[handle->entry++]) : File(); }
};
inline uint32_t millis() { return fake::clock_ms; }
