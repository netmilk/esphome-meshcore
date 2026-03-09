// MeshCore MainBoard implementation for Zephyr - Implementation
#include "zephyr_board.h"
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(meshcore_board, LOG_LEVEL_INF);

namespace meshcore_sensor {

ZephyrBoard::ZephyrBoard()
    : gpio0_(nullptr), gpio1_(nullptr),
      led_pin_(26), led_state_(false), gpio_state_(0), tx_led_pin_(26) {
}

void ZephyrBoard::begin() {
    gpio0_ = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    gpio1_ = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    if (!device_is_ready(gpio0_) || !device_is_ready(gpio1_)) {
        LOG_ERR("GPIO devices not ready");
        return;
    }

    // Configure LED pin (active low on XIAO)
    const struct device* port = getPort(led_pin_);
    gpio_pin_configure(port, getPin(led_pin_), GPIO_OUTPUT_HIGH);  // start OFF (active low)

    LOG_INF("Board initialized, LED on pin %u", led_pin_);
}

void ZephyrBoard::setLedPin(uint32_t pin) {
    led_pin_ = pin;
    tx_led_pin_ = pin;
}

void ZephyrBoard::setLed(bool on) {
    led_state_ = on;
    const struct device* port = getPort(led_pin_);
    // Active low: LOW = on, HIGH = off
    gpio_pin_set_raw(port, getPin(led_pin_), on ? 0 : 1);
}

const struct device* ZephyrBoard::getPort(uint32_t pin) {
    return (pin < 32) ? gpio0_ : gpio1_;
}

uint32_t ZephyrBoard::getPin(uint32_t pin) {
    return pin % 32;
}

uint16_t ZephyrBoard::getBattMilliVolts() {
    // TODO: implement ADC reading for battery voltage
    // For now return nominal voltage
    return 3700;
}

float ZephyrBoard::getMCUTemperature() {
    // TODO: read nRF52840 internal temperature sensor
    return 25.0f;
}

void ZephyrBoard::onBeforeTransmit() {
    // Flash LED during TX (active low)
    const struct device* port = getPort(tx_led_pin_);
    gpio_pin_set_raw(port, getPin(tx_led_pin_), 0);  // LED ON
}

void ZephyrBoard::onAfterTransmit() {
    // Restore LED state after TX
    if (!led_state_) {
        const struct device* port = getPort(tx_led_pin_);
        gpio_pin_set_raw(port, getPin(tx_led_pin_), 1);  // LED OFF
    }
}

void ZephyrBoard::reboot() {
    sys_reboot(SYS_REBOOT_COLD);
}

uint32_t ZephyrBoard::getGpio() {
    return gpio_state_;
}

void ZephyrBoard::setGpio(uint32_t values) {
    gpio_state_ = values;
    // Map bit 0 to LED
    setLed(values & 0x01);
}

// RNG implementation - uses stdlib rand() as fallback when no hardware entropy
void ZephyrRNG::random(uint8_t* dest, size_t sz) {
    for (size_t i = 0; i < sz; i++) {
        dest[i] = (uint8_t)(rand() & 0xFF);
    }
}

}  // namespace meshcore_sensor
