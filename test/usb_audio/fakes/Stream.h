#pragma once
#include <cstddef>
#include <cstdint>
struct Print {
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *, size_t) = 0;
};
struct Stream : Print {
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
  virtual int availableForWrite() = 0;
};
