// Arduino Stream.h stub for Zephyr
#pragma once

#ifndef ARDUINO_STREAM_H_STUB
#define ARDUINO_STREAM_H_STUB

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t b) { (void)b; return 1; }
    virtual size_t write(const uint8_t* buf, size_t sz) { (void)buf; return sz; }
    size_t print(const char* s) { size_t n = 0; while (s && *s) { write((uint8_t)*s++); n++; } return n; }
    size_t print(int val) { char buf[16]; snprintf(buf, sizeof(buf), "%d", val); return print(buf); }
    size_t print(unsigned int val) { char buf[16]; snprintf(buf, sizeof(buf), "%u", val); return print(buf); }
    size_t print(long val) { char buf[16]; snprintf(buf, sizeof(buf), "%ld", val); return print(buf); }
    size_t print(unsigned long val) { char buf[16]; snprintf(buf, sizeof(buf), "%lu", val); return print(buf); }
    size_t println(const char* s) { size_t n = print(s); n += print("\n"); return n; }
    size_t println(int val) { size_t n = print(val); n += print("\n"); return n; }
    size_t println() { return print("\n"); }
};

class Stream : public Print {
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
    size_t readBytes(uint8_t* buf, size_t len) {
        size_t count = 0;
        while (count < len) {
            int c = read();
            if (c < 0) break;
            buf[count++] = (uint8_t)c;
        }
        return count;
    }
    size_t readBytes(char* buf, size_t len) { return readBytes((uint8_t*)buf, len); }
};

#endif
