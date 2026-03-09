// MeshCore MainBoard implementation for Zephyr/ESPHome on XIAO nRF52840
#pragma once

#include <MeshCore.h>
#include <Dispatcher.h>
#include <Utils.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>

namespace meshcore_sensor {

class ZephyrBoard : public mesh::MainBoard {
public:
    ZephyrBoard();
    void begin();

    // LED control
    void setLedPin(uint32_t pin);
    void setLed(bool on);
    bool getLed() const { return led_state_; }

    // MainBoard interface
    uint16_t getBattMilliVolts() override;
    float getMCUTemperature() override;
    const char* getManufacturerName() const override { return "XIAO nRF52840 (Zephyr)"; }
    void onBeforeTransmit() override;
    void onAfterTransmit() override;
    void reboot() override;
    uint8_t getStartupReason() const override { return BD_STARTUP_NORMAL; }
    uint32_t getGpio() override;
    void setGpio(uint32_t values) override;

private:
    const struct device* gpio0_;
    const struct device* gpio1_;
    uint32_t led_pin_;      // nRF52840 GPIO number
    bool led_state_;
    uint32_t gpio_state_;   // for setGpio/getGpio
    uint32_t tx_led_pin_;   // TX activity LED

    const struct device* getPort(uint32_t pin);
    uint32_t getPin(uint32_t pin);
};

// Zephyr MillisecondClock implementation
class ZephyrMillis : public mesh::MillisecondClock {
public:
    unsigned long getMillis() override { return k_uptime_get_32(); }
};

// Zephyr RTC Clock (software-based, tracks time from millis)
class ZephyrRTCClock : public mesh::RTCClock {
    uint32_t base_time_;
    uint64_t accumulator_;
    unsigned long prev_millis_;
public:
    ZephyrRTCClock() : base_time_(1715770351), accumulator_(0) {
        prev_millis_ = k_uptime_get_32();
    }
    uint32_t getCurrentTime() override { return base_time_ + accumulator_ / 1000; }
    void setCurrentTime(uint32_t time) override {
        base_time_ = time;
        accumulator_ = 0;
        prev_millis_ = k_uptime_get_32();
    }
    void tick() override {
        unsigned long now = k_uptime_get_32();
        accumulator_ += (now - prev_millis_);
        prev_millis_ = now;
    }
};

// Simple RNG using Zephyr's random subsystem
class ZephyrRNG : public mesh::RNG {
public:
    void random(uint8_t* dest, size_t sz) override;
};

}  // namespace meshcore_sensor
