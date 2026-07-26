#include <Stream.h>

String Stream::readStringUntil(char terminator) {
  String out;
  out.reserve(64);
  while (true) {
    int v = read();
    if (v < 0) break;
    char c = static_cast<char>(v);
    if (c == terminator) break;
    out += c;
  }
  return out;
}
