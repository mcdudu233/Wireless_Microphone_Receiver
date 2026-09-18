#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace logger
{
  // Synchronization is supplied by the logging hook/consumer. Complete records
  // are dropped on overflow; existing records are never partially overwritten.
  struct Buffer
  {
    static constexpr size_t CAPACITY = 16384;
    uint8_t data[CAPACITY] = {};
    size_t head = 0, count = 0;
    uint32_t dropped = 0;
    void append(const char *text, size_t size)
    {
      if (size > CAPACITY - count) { ++dropped; return; }
      size_t tail = (head + count) % CAPACITY;
      const size_t first = size < CAPACITY - tail ? size : CAPACITY - tail;
      std::memcpy(data + tail, text, first);
      std::memcpy(data, text + first, size - first);
      count += size;
    }
    size_t take(uint8_t *out, size_t capacity, uint32_t &lost)
    {
      const size_t size = count < capacity ? count : capacity;
      const size_t first = size < CAPACITY - head ? size : CAPACITY - head;
      std::memcpy(out, data + head, first);
      std::memcpy(out + first, data, size - first);
      head = (head + size) % CAPACITY;
      count -= size;
      lost = dropped; dropped = 0;
      return size;
    }
  };
}
