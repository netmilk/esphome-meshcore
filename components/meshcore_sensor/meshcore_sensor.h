// ESPHome MeshCore Sensor Component
// Runs a MeshCore sensor node with LED control via CLI commands
#pragma once

#include "esphome/core/component.h"
#include <stdint.h>
#include <string>

// Forward declarations - full headers only in meshcore_sensor.cpp
namespace meshcore_sensor {
    class ZephyrHal;
    class ZephyrBoard;
    class ZephyrMillis;
    class ZephyrRTCClock;
    class ZephyrRNG;
}

namespace meshcore_sensor {

class MeshCoreSensorComponent : public esphome::Component {
public:
    MeshCoreSensorComponent() = default;

    // ESPHome Component interface
    void setup() override;
    void loop() override;
    void dump_config() override;
    float get_setup_priority() const override { return esphome::setup_priority::LATE; }

    // Config setters (called from generated code)
    void set_node_name(const std::string& name) { node_name_ = name; }
    void set_password(const std::string& pw) { password_ = pw; }
    void set_led_pin(uint32_t pin) { led_pin_ = pin; }

    void set_lora_cs(uint32_t pin) { lora_cs_ = pin; }
    void set_lora_reset(uint32_t pin) { lora_reset_ = pin; }
    void set_lora_busy(uint32_t pin) { lora_busy_ = pin; }
    void set_lora_dio1(uint32_t pin) { lora_dio1_ = pin; }
    void set_lora_rxen(uint32_t pin) { lora_rxen_ = pin; }
    void set_lora_sclk(uint32_t pin) { lora_sclk_ = pin; }
    void set_lora_miso(uint32_t pin) { lora_miso_ = pin; }
    void set_lora_mosi(uint32_t pin) { lora_mosi_ = pin; }

    void set_frequency(float freq) { frequency_ = freq; }
    void set_bandwidth(float bw) { bandwidth_ = bw; }
    void set_spreading_factor(uint8_t sf) { spreading_factor_ = sf; }
    void set_coding_rate(uint8_t cr) { coding_rate_ = cr; }
    void set_tx_power(int8_t pwr) { tx_power_ = pwr; }

protected:
    // Config
    std::string node_name_{"ESPHome Sensor"};
    std::string password_{"password"};
    uint32_t led_pin_{26};

    // LoRa pins (nRF52840 GPIO numbers)
    uint32_t lora_cs_{4};
    uint32_t lora_reset_{28};
    uint32_t lora_busy_{29};
    uint32_t lora_dio1_{3};
    uint32_t lora_rxen_{5};
    uint32_t lora_sclk_{45};
    uint32_t lora_miso_{46};
    uint32_t lora_mosi_{47};

    // Radio params (Czech/narrow defaults)
    float frequency_{869.432f};
    float bandwidth_{62.5f};
    uint8_t spreading_factor_{7};
    uint8_t coding_rate_{5};
    int8_t tx_power_{22};

    // Runtime objects (allocated in setup)
    ZephyrHal* hal_{nullptr};
    ZephyrBoard* board_{nullptr};
    ZephyrMillis* ms_clock_{nullptr};
    ZephyrRTCClock* rtc_clock_{nullptr};
    ZephyrRNG* rng_{nullptr};

    bool radio_ok_{false};
};

}  // namespace meshcore_sensor
