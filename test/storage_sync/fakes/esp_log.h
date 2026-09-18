#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>
using vprintf_like_t=int (*)(const char *,va_list);
enum { ESP_LOG_WARN=2,ESP_LOG_INFO=3 };
namespace fake {
inline std::string serial;
inline int serial_sink(const char *fmt,va_list args) {
 char text[2048];int n=std::vsnprintf(text,sizeof(text),fmt,args);
 if(n>0)serial.append(text,size_t(n)<sizeof(text)?size_t(n):sizeof(text)-1);
 return n;
}
inline vprintf_like_t output=serial_sink;
inline int emit(const char *fmt,...) { va_list args;va_start(args,fmt);int n=output(fmt,args);va_end(args);return n; }
}
inline vprintf_like_t esp_log_set_vprintf(vprintf_like_t output) { auto old=fake::output;fake::output=output;return old; }
inline void esp_log_level_set(const char *,int) {}
