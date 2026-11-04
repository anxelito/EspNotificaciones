// Host shim: just enough of Arduino.h for the UI code to compile on a PC.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include "pgmspace.h"

class String {
public:
    String(const char* s = "") : s_(s ? s : "") {}
    const char* c_str() const { return s_.c_str(); }
    unsigned length() const { return (unsigned)s_.size(); }
private:
    std::string s_;
};

struct HostSerial {
    void begin(unsigned long) {}
    void println(const char* s) { std::fprintf(stderr, "%s\n", s); }
    void print(const char* s) { std::fprintf(stderr, "%s", s); }
    template <typename... A> void printf(const char* f, A... a) { std::fprintf(stderr, f, a...); }
};
extern HostSerial Serial;

uint32_t millis();

#ifndef _GNU_SOURCE
inline size_t strlcpy(char* d, const char* s, size_t n) {
    size_t len = std::strlen(s);
    if (n) { size_t c = len < n - 1 ? len : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
    return len;
}
#endif
