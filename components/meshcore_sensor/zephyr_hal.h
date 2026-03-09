// RadioLib Hardware Abstraction Layer for Zephyr RTOS
// Implements RadioLibHal interface using Zephyr GPIO and SPI drivers
#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <RadioLib.h>

namespace meshcore_sensor {

class ZephyrHal : public RadioLibHal {
public:
    ZephyrHal();

    // Initialize with pin numbers
    void configure(uint32_t sclk, uint32_t miso, uint32_t mosi);

    // RadioLibHal pure virtual implementations
    void pinMode(uint32_t pin, uint32_t mode) override;
    void digitalWrite(uint32_t pin, uint32_t value) override;
    uint32_t digitalRead(uint32_t pin) override;
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;
    void detachInterrupt(uint32_t interruptNum) override;
    void delay(RadioLibTime_t ms) override;
    void delayMicroseconds(RadioLibTime_t us) override;
    RadioLibTime_t millis() override;
    RadioLibTime_t micros() override;
    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override;

    // SPI methods
    void spiBegin() override;
    void spiBeginTransaction() override;
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override;
    void spiEndTransaction() override;
    void spiEnd() override;

    // Optional overrides
    void init() override;
    void term() override;
    void yield() override;

private:
    const struct device* gpio0_;
    const struct device* gpio1_;
    const struct device* spi_dev_;
    struct spi_config spi_cfg_;
    uint32_t sclk_pin_, miso_pin_, mosi_pin_;
    bool spi_initialized_;

    const struct device* getGpioPort(uint32_t pin);
    uint32_t getGpioPin(uint32_t pin);

    static constexpr int MAX_INTERRUPTS = 8;
    struct InterruptEntry {
        uint32_t pin;
        void (*callback)(void);
        struct gpio_callback gpio_cb;
        bool active;
    };
    InterruptEntry interrupts_[MAX_INTERRUPTS];

    static void gpioInterruptHandler(const struct device* dev, struct gpio_callback* cb, uint32_t pins);
};

}  // namespace meshcore_sensor
