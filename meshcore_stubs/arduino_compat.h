// Arduino compatibility shim for MeshCore on Zephyr/ESPHome
// Provides Arduino-like functions using Zephyr RTOS APIs
#pragma once
#ifndef MESHCORE_ARDUINO_COMPAT_H_
#define MESHCORE_ARDUINO_COMPAT_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
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
// Simple min/max macros matching Arduino behavior
// Template versions caused ptrdiff_t vs int return type issues on ARM
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
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

// ---- Settings-backed filesystem ----
// Implements Arduino File and Adafruit_LittleFS using Zephyr settings subsystem.
// Files are fully buffered in RAM, persisted to NVS flash on close().
// This makes MeshCore's ACL, NodePrefs, and IdentityStore persist across reboots.

#define _MC_FILE_MAX_SIZE 4096

// Convert MeshCore file path to Zephyr settings key: "/s_contacts" -> "mc/s_contacts"
static inline void _mc_path_to_key(const char* path, char* key, size_t key_sz) {
    const char* p = (path[0] == '/') ? path + 1 : path;
    snprintf(key, key_sz, "mc/%s", p);
}

// Settings load callback — reads stored blob into a RAM buffer
struct _McFileLoadCtx {
    uint8_t* buf;
    size_t capacity;
    size_t loaded;
    bool found;
};

static int _mc_file_load_cb(const char *key, size_t len,
                             settings_read_cb read_cb, void *cb_arg, void *param) {
    (void)key;
    _McFileLoadCtx* ctx = (_McFileLoadCtx*)param;
    size_t to_read = (len < ctx->capacity) ? len : ctx->capacity;
    ssize_t n = read_cb(cb_arg, ctx->buf, to_read);
    ctx->loaded = (n > 0) ? (size_t)n : 0;
    ctx->found = true;
    return 0;
}

// Settings exists callback — just checks if key has data
static int _mc_file_exists_cb(const char *key, size_t len,
                               settings_read_cb read_cb, void *cb_arg, void *param) {
    (void)key; (void)len; (void)read_cb; (void)cb_arg;
    *((bool*)param) = true;
    return 0;
}

#ifndef FILE_O_READ
#define FILE_O_READ  0
#endif
#ifndef FILE_O_WRITE
#define FILE_O_WRITE 1
#endif

class File : public Stream {
    mutable uint8_t* buf_;
    size_t capacity_;       // buffer allocation size
    mutable size_t size_;   // data length (read mode) or bytes written (write mode)
    mutable size_t pos_;    // current read position
    mutable bool valid_;
    bool write_mode_;
    char key_[48];

public:
    File() : buf_(nullptr), capacity_(0), size_(0), pos_(0), valid_(false), write_mode_(false) {
        key_[0] = 0;
    }

    ~File() { free(buf_); }

    // Move constructor
    File(File&& o) noexcept : buf_(o.buf_), capacity_(o.capacity_), size_(o.size_),
                               pos_(o.pos_), valid_(o.valid_), write_mode_(o.write_mode_) {
        memcpy(key_, o.key_, sizeof(key_));
        o.buf_ = nullptr;
        o.valid_ = false;
    }

    // Copy constructor — transfers ownership (for return-by-value)
    File(const File& o) : buf_(o.buf_), capacity_(o.capacity_), size_(o.size_),
                           pos_(o.pos_), valid_(o.valid_), write_mode_(o.write_mode_) {
        memcpy(key_, o.key_, sizeof(key_));
        o.buf_ = nullptr;
        o.valid_ = false;
    }

    File& operator=(const File& o) {
        if (this != &o) {
            free(buf_);
            buf_ = o.buf_; capacity_ = o.capacity_; size_ = o.size_;
            pos_ = o.pos_; valid_ = o.valid_; write_mode_ = o.write_mode_;
            memcpy(key_, o.key_, sizeof(key_));
            o.buf_ = nullptr;
            o.valid_ = false;
        }
        return *this;
    }

    // Initialize for reading — loads data from Zephyr settings into RAM buffer
    bool _initRead(const char* settings_key) {
        printk("[mc_fs] _initRead key=%s\n", settings_key);
        buf_ = (uint8_t*)malloc(_MC_FILE_MAX_SIZE);
        if (!buf_) return false;
        _McFileLoadCtx ctx = {buf_, _MC_FILE_MAX_SIZE, 0, false};
        settings_load_subtree_direct(settings_key, _mc_file_load_cb, &ctx);
        if (!ctx.found || ctx.loaded == 0) {
            free(buf_); buf_ = nullptr;
            return false;
        }
        capacity_ = _MC_FILE_MAX_SIZE;
        size_ = ctx.loaded;
        pos_ = 0;
        valid_ = true;
        write_mode_ = false;
        strncpy(key_, settings_key, sizeof(key_) - 1);
        key_[sizeof(key_) - 1] = 0;
        return true;
    }

    // Initialize for writing — allocates empty RAM buffer, persisted on close()
    bool _initWrite(const char* settings_key) {
        printk("[mc_fs] _initWrite key=%s\n", settings_key);
        buf_ = (uint8_t*)malloc(_MC_FILE_MAX_SIZE);
        if (!buf_) return false;
        capacity_ = _MC_FILE_MAX_SIZE;
        size_ = 0;
        pos_ = 0;
        valid_ = true;
        write_mode_ = true;
        strncpy(key_, settings_key, sizeof(key_) - 1);
        key_[sizeof(key_) - 1] = 0;
        return true;
    }

    operator bool() const { return valid_; }

    int available() override {
        return (valid_ && !write_mode_) ? (int)(size_ - pos_) : 0;
    }

    int read() override {
        if (!valid_ || write_mode_ || pos_ >= size_) return -1;
        return buf_[pos_++];
    }

    size_t read(uint8_t* dest, size_t sz) {
        if (!valid_ || write_mode_) return 0;
        size_t avail = size_ - pos_;
        size_t n = (sz < avail) ? sz : avail;
        memcpy(dest, buf_ + pos_, n);
        pos_ += n;
        return n;
    }

    size_t write(uint8_t b) override {
        if (!valid_ || !write_mode_ || size_ >= capacity_) return 0;
        buf_[size_++] = b;
        return 1;
    }

    size_t write(const uint8_t* data, size_t sz) override {
        if (!valid_ || !write_mode_) return 0;
        size_t avail = capacity_ - size_;
        size_t n = (sz < avail) ? sz : avail;
        memcpy(buf_ + size_, data, n);
        size_ += n;
        return n;
    }

    void close() {
        if (valid_ && write_mode_ && key_[0] && buf_) {
            printk("[mc_fs] save key=%s len=%u\n", key_, (unsigned)size_);
            int rc = settings_save_one(key_, buf_, size_);
            printk("[mc_fs] save rc=%d\n", rc);
        }
        free(buf_);
        buf_ = nullptr;
        valid_ = false;
    }

    size_t size() { return size_; }

    bool seek(size_t p) {
        if (!valid_ || p > size_) return false;
        pos_ = p;
        return true;
    }

    void flush() override {}
};

class Adafruit_LittleFS {
public:
    bool begin() { return true; }
    bool format() { return true; }
    bool mkdir(const char* path) { (void)path; return true; }

    File open(const char* path, int mode = 0) {
        char key[48];
        _mc_path_to_key(path, key, sizeof(key));
        File f;
        if (mode == FILE_O_WRITE) {
            f._initWrite(key);
        } else {
            f._initRead(key);
        }
        return f;
    }

    bool exists(const char* path) {
        char key[48];
        _mc_path_to_key(path, key, sizeof(key));
        printk("[mc_fs] exists key=%s\n", key);
        bool found = false;
        settings_load_subtree_direct(key, _mc_file_exists_cb, &found);
        return found;
    }

    bool remove(const char* path) {
        char key[48];
        _mc_path_to_key(path, key, sizeof(key));
        printk("[mc_fs] remove key=%s\n", key);
        settings_delete(key);
        return true;
    }
};

#ifndef FILESYSTEM
#define FILESYSTEM Adafruit_LittleFS
#endif

namespace Adafruit_LittleFS_Namespace {
    // empty — used by MeshCore's using directive
}

// ---- InternalFileSystem (for NRF52_PLATFORM) ----
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
