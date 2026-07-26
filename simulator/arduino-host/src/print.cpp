#include <Print.h>

#include <cstdio>
#include <cstring>

size_t Print::write(const uint8_t* buffer, size_t size) {
  size_t written = 0;
  for (size_t i = 0; i < size; i++) {
    if (write(buffer[i]) == 0) break;
    written++;
  }
  return written;
}

size_t Print::write(const char* s) {
  if (!s) return 0;
  return write(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
}

size_t Print::print(const char* s) { return write(s); }
size_t Print::print(char c) { return write(static_cast<uint8_t>(c)); }
size_t Print::print(const String& s) { return write(s.c_str()); }

size_t Print::print(int v) {
  char buf[16];
  int n = std::snprintf(buf, sizeof(buf), "%d", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(unsigned int v) {
  char buf[16];
  int n = std::snprintf(buf, sizeof(buf), "%u", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(long v) {
  char buf[24];
  int n = std::snprintf(buf, sizeof(buf), "%ld", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(unsigned long v) {
  char buf[24];
  int n = std::snprintf(buf, sizeof(buf), "%lu", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(long long v) {
  char buf[32];
  int n = std::snprintf(buf, sizeof(buf), "%lld", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(unsigned long long v) {
  char buf[32];
  int n = std::snprintf(buf, sizeof(buf), "%llu", v);
  return n > 0 ? write(buf, n) : 0;
}
size_t Print::print(double v, int decimals) {
  char buf[32];
  int n = std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
  return n > 0 ? write(buf, n) : 0;
}

size_t Print::println() { return write(static_cast<uint8_t>('\n')); }
size_t Print::println(const char* s) {
  size_t a = write(s);
  size_t b = println();
  return a + b;
}
size_t Print::println(const String& s) {
  size_t a = write(s.c_str());
  size_t b = println();
  return a + b;
}
size_t Print::println(int v) {
  size_t a = print(v);
  size_t b = println();
  return a + b;
}
size_t Print::println(unsigned long v) {
  size_t a = print(v);
  size_t b = println();
  return a + b;
}

size_t Print::printf(const char* format, ...) {
  std::va_list ap;
  va_start(ap, format);
  size_t n = vprintf(format, ap);
  va_end(ap);
  return n;
}

size_t Print::vprintf(const char* format, std::va_list ap) {
  char stackBuf[256];
  std::va_list ap2;
  va_copy(ap2, ap);
  int needed = std::vsnprintf(stackBuf, sizeof(stackBuf), format, ap);
  if (needed < 0) {
    va_end(ap2);
    return 0;
  }
  if (static_cast<size_t>(needed) < sizeof(stackBuf)) {
    va_end(ap2);
    return write(stackBuf, needed);
  }
  // Buffer too small — allocate exact size.
  size_t bigSize = static_cast<size_t>(needed) + 1;
  char* big = static_cast<char*>(std::malloc(bigSize));
  if (!big) {
    va_end(ap2);
    return 0;
  }
  std::vsnprintf(big, bigSize, format, ap2);
  va_end(ap2);
  size_t written = write(big, needed);
  std::free(big);
  return written;
}
