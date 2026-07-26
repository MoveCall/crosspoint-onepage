#pragma once

#include <WString.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>

class Print {
 public:
  virtual ~Print() = default;

  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size);
  virtual void flush() {}

  size_t write(const char* s);
  size_t write(const char* s, size_t size) { return write(reinterpret_cast<const uint8_t*>(s), size); }

  size_t print(const char* s);
  size_t print(char c);
  size_t print(const String& s);
  size_t print(int v);
  size_t print(unsigned int v);
  size_t print(long v);
  size_t print(unsigned long v);
  size_t print(long long v);
  size_t print(unsigned long long v);
  size_t print(double v, int decimals = 2);

  size_t println();
  size_t println(const char* s);
  size_t println(const String& s);
  size_t println(int v);
  size_t println(unsigned long v);

  size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));
  size_t vprintf(const char* format, std::va_list ap);

 protected:
  int writeError = 0;
};
