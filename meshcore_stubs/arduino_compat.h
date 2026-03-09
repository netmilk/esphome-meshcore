// Arduino compatibility shim for MeshCore on Zephyr/ESPHome
// Provides Arduino-like functions using Zephyr RTOS APIs
#pragma once
#ifndef MESHCORE_ARDUINO_COMPAT_H_
#define MESHCORE_ARDUINO_COMPAT_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Stream class (needed by File and others)
#include "Stream.h"

// ---- Timing ----
static inline unsigned long millis() { return k_uptime_get_32(); }
static inline unsigned long micros() { return (unsigned long)(k_ticks_to_us_floor64(k_uptime_ticks())); }
static inline void delay(unsigned long ms) { k_msleep(ms); }
static inline void delayMicroseconds(unsigned long us) { k_busy_wait(us); }

// ---- GPIO ----
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2
#define LOW          0
#define HIGH         1

// Stub GPIO - actual GPIO done via Zephyr API in zephyr_board
static inline void pinMode(uint32_t pin, uint32_t mode) { (void)pin; (void)mode; }
static inline void digitalWrite(uint32_t pin, uint32_t val) { (void)pin; (void)val; }
static inline int digitalRead(uint32_t pin) { (void)pin; return 0; }
static inline int analogRead(uint32_t pin) { (void)pin; return 0; }
static inline void analogReadResolution(int bits) { (void)bits; }
static inline void analogReference(int ref) { (void)ref; }
#define AR_INTERNAL_3_0 0

// ---- Random ----
static inline long random(long min, long max) {
    if (min >= max) return min;
    return min + (rand() % (max - min));
}
static inline long random(long max) { return random(0, max); }
static inline void randomSeed(unsigned long seed) { srand(seed); }

// ---- Arduino utility functions ----
template<typename T, typename U, typename V>
static inline T constrain(T x, U lo, V hi) {
    if (x < (T)lo) return (T)lo;
    if (x > (T)hi) return (T)hi;
    return x;
}
// Note: avoid min/max/abs macros that conflict with C++ std:: versions
// Arduino defines these but they cause issues with ESPHome headers
#ifndef _ARDUINO_MIN_MAX_ABS
#define _ARDUINO_MIN_MAX_ABS
template<typename T, typename U>
static inline auto _arduino_min(T a, U b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template<typename T, typename U>
static inline auto _arduino_max(T a, U b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
// Only define min/max in MeshCore compilation context
#ifndef min
#define min(a,b) _arduino_min(a,b)
#endif
#ifndef max
#define max(a,b) _arduino_max(a,b)
#endif
#endif
static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
static inline char* ltoa(long val, char* buf, int base) {
    if (base == 10) snprintf(buf, 16, "%ld", val);
    else if (base == 16) snprintf(buf, 16, "%lx", val);
    else snprintf(buf, 16, "%ld", val);
    return buf;
}

// ---- PROGMEM stubs (for Crypto library) ----
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_byte_far(addr) pgm_read_byte(addr)
#define pgm_read_byte_near(addr) pgm_read_byte(addr)
#define PSTR(s) (s)
#define F(s) (s)
#define __FlashStringHelper char

// ---- NVIC / System ----
#ifndef NVIC_SystemReset
static inline void NVIC_SystemReset() { sys_reboot(SYS_REBOOT_COLD); }
#endif

// ---- Serial stub ----
class SerialStub : public Stream {
public:
    void begin(unsigned long baud) { (void)baud; }
    using Print::print;
    using Print::println;
    void print(float f) { printk("%d.%02d", (int)f, ((int)(f*100)%100 < 0 ? -((int)(f*100)%100) : (int)(f*100)%100)); }
    int available() override { return 0; }
    int read() override { return -1; }
    size_t write(uint8_t b) override { char c = b; printk("%c", c); return 1; }
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
    }
    void flush() override {}
    operator bool() { return true; }
};

#ifndef Serial
extern SerialStub Serial;
#endif

// ---- String class stub ----
// MeshCore mostly uses char arrays, but some deps may need String
class String {
    char* _buf;
    size_t _len;
public:
    String() : _buf(nullptr), _len(0) {}
    String(const char* s) {
        _len = strlen(s);
        _buf = (char*)malloc(_len + 1);
        strcpy(_buf, s);
    }
    String(int val) {
        _buf = (char*)malloc(16);
        _len = snprintf(_buf, 16, "%d", val);
    }
    ~String() { free(_buf); }
    String(const String& o) {
        _len = o._len;
        _buf = (char*)malloc(_len + 1);
        memcpy(_buf, o._buf, _len + 1);
    }
    String& operator=(const String& o) {
        if (this != &o) {
            free(_buf);
            _len = o._len;
            _buf = (char*)malloc(_len + 1);
            memcpy(_buf, o._buf, _len + 1);
        }
        return *this;
    }
    const char* c_str() const { return _buf ? _buf : ""; }
    size_t length() const { return _len; }
    operator const char*() const { return c_str(); }
};

// ---- SPI stub (actual SPI handled by RadioLib Zephyr HAL) ----
class SPIClass {
public:
    void begin() {}
    void end() {}
    void setPins(int miso, int sck, int mosi) { (void)miso; (void)sck; (void)mosi; }
    uint8_t transfer(uint8_t val) { (void)val; return 0; }
    void beginTransaction(void*) {}
    void endTransaction() {}
};

using SPIClassStub = SPIClass;  // backward compat

#ifndef SPI
extern SPIClass SPI;
#endif

// ---- Wire/I2C stub ----
class WireStub {
public:
    void begin() {}
    void setPins(int sda, int scl) { (void)sda; (void)scl; }
    void beginTransmission(uint8_t addr) { (void)addr; }
    uint8_t endTransmission() { return 0; }
    uint8_t requestFrom(uint8_t addr, uint8_t qty) { (void)addr; (void)qty; return 0; }
    int available() { return 0; }
    int read() { return 0; }
    void write(uint8_t val) { (void)val; }
};

#ifndef Wire
extern WireStub Wire;
#endif

// ---- Filesystem stubs ----
// MeshCore uses FILESYSTEM (Adafruit_LittleFS on nRF52)
// We provide a stub that stores nothing (prefs won't persist initially)

class File : public Stream {
public:
    operator bool() const { return false; }
    int available() override { return 0; }
    int read() override { return -1; }
    size_t read(uint8_t* buf, size_t sz) { (void)buf; return 0; }
    size_t write(uint8_t b) override { (void)b; return 0; }
    size_t write(const uint8_t* buf, size_t sz) override { (void)buf; return 0; }
    void close() {}
    size_t size() { return 0; }
    bool seek(size_t pos) { (void)pos; return false; }
};

class Adafruit_LittleFS {
public:
    bool begin() { return true; }
    bool format() { return true; }
    bool mkdir(const char* path) { (void)path; return true; }
    File open(const char* path, int mode = 0) { (void)path; (void)mode; return File(); }
    bool exists(const char* path) { (void)path; return false; }
    bool remove(const char* path) { (void)path; return true; }
};

#ifndef FILESYSTEM
#define FILESYSTEM Adafruit_LittleFS
#endif
#ifndef FILE_O_READ
#define FILE_O_READ  0
#endif
#ifndef FILE_O_WRITE
#define FILE_O_WRITE 1
#endif

namespace Adafruit_LittleFS_Namespace {
    // empty - used by MeshCore's using directive
}

// ---- InternalFileSystem stub (for NRF52_PLATFORM) ----
class InternalFileSystemClass : public Adafruit_LittleFS {
public:
    bool begin() { return true; }
};
extern InternalFileSystemClass InternalFS;

// ---- RTClib stub ----
class RTC_DS3231 {
public:
    bool begin() { return false; }
};

class DateTime {
    uint32_t _t;
public:
    DateTime(uint32_t t = 0) : _t(t) {}
    uint32_t unixtime() { return _t; }
    uint8_t hour() { return (_t % 86400) / 3600; }
    uint8_t minute() { return (_t % 3600) / 60; }
    uint8_t second() { return _t % 60; }
    uint8_t day() { /* simplified */ uint32_t d = _t / 86400; return (d % 31) + 1; }
    uint8_t month() { uint32_t d = _t / 86400; return ((d / 30) % 12) + 1; }
    uint16_t year() { return 1970 + _t / 31536000; }
};

// AutoDiscoverRTCClock is stubbed in compat/AutoDiscoverRTCClock.h

// EnvironmentSensorManager comes from MeshCore headers
// (declared extern in target.h)

#endif // MESHCORE_ARDUINO_COMPAT_H_
