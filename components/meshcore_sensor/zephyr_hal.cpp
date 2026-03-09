// RadioLib Hardware Abstraction Layer for Zephyr RTOS - Implementation
// Uses Zephyr SPI driver API (devicetree-configured spi2)
#include "zephyr_hal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(meshcore_hal, LOG_LEVEL_INF);

namespace meshcore_sensor {

ZephyrHal::ZephyrHal()
    : RadioLibHal(GPIO_INPUT, GPIO_OUTPUT, 0, 1, GPIO_INT_EDGE_RISING, GPIO_INT_EDGE_FALLING),
      gpio0_(nullptr), gpio1_(nullptr), spi_dev_(nullptr),
      sclk_pin_(0), miso_pin_(0), mosi_pin_(0), spi_initialized_(false) {
    memset(interrupts_, 0, sizeof(interrupts_));
    memset(&spi_cfg_, 0, sizeof(spi_cfg_));
}

void ZephyrHal::configure(uint32_t sclk, uint32_t miso, uint32_t mosi) {
    sclk_pin_ = sclk;
    miso_pin_ = miso;
    mosi_pin_ = mosi;
}

void ZephyrHal::init() {
    gpio0_ = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    gpio1_ = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    if (!device_is_ready(gpio0_) || !device_is_ready(gpio1_)) {
        LOG_ERR("GPIO devices not ready");
    }
}

void ZephyrHal::term() {
    spiEnd();
}

const struct device* ZephyrHal::getGpioPort(uint32_t pin) {
    return (pin < 32) ? gpio0_ : gpio1_;
}

uint32_t ZephyrHal::getGpioPin(uint32_t pin) {
    return pin % 32;
}

void ZephyrHal::pinMode(uint32_t pin, uint32_t mode) {
    if (pin == RADIOLIB_NC) return;
    const struct device* port = getGpioPort(pin);
    uint32_t p = getGpioPin(pin);

    gpio_flags_t flags = 0;
    if (mode == GPIO_INPUT) {
        flags = GPIO_INPUT;
    } else if (mode == GPIO_OUTPUT) {
        flags = GPIO_OUTPUT;
    }
    gpio_pin_configure(port, p, flags);
}

void ZephyrHal::digitalWrite(uint32_t pin, uint32_t value) {
    if (pin == RADIOLIB_NC) return;
    const struct device* port = getGpioPort(pin);
    uint32_t p = getGpioPin(pin);
    gpio_pin_set_raw(port, p, value ? 1 : 0);
}

uint32_t ZephyrHal::digitalRead(uint32_t pin) {
    if (pin == RADIOLIB_NC) return 0;
    const struct device* port = getGpioPort(pin);
    uint32_t p = getGpioPin(pin);
    return gpio_pin_get_raw(port, p) ? 1 : 0;
}

void ZephyrHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
    if (interruptNum == RADIOLIB_NC) return;

    const struct device* port = getGpioPort(interruptNum);
    uint32_t p = getGpioPin(interruptNum);

    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        if (!interrupts_[i].active) {
            interrupts_[i].pin = interruptNum;
            interrupts_[i].callback = interruptCb;
            interrupts_[i].active = true;

            gpio_pin_configure(port, p, GPIO_INPUT);
            gpio_pin_interrupt_configure(port, p,
                (mode == GPIO_INT_EDGE_RISING) ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_EDGE_TO_INACTIVE);

            gpio_init_callback(&interrupts_[i].gpio_cb, gpioInterruptHandler, BIT(p));
            gpio_add_callback(port, &interrupts_[i].gpio_cb);
            return;
        }
    }
    LOG_WRN("No free interrupt slots");
}

void ZephyrHal::detachInterrupt(uint32_t interruptNum) {
    if (interruptNum == RADIOLIB_NC) return;

    const struct device* port = getGpioPort(interruptNum);
    uint32_t p = getGpioPin(interruptNum);

    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        if (interrupts_[i].active && interrupts_[i].pin == interruptNum) {
            gpio_pin_interrupt_configure(port, p, GPIO_INT_DISABLE);
            gpio_remove_callback(port, &interrupts_[i].gpio_cb);
            interrupts_[i].active = false;
            return;
        }
    }
}

void ZephyrHal::gpioInterruptHandler(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
    (void)dev;
    (void)pins;

    // Find matching interrupt entry and invoke the RadioLib callback
    // Use CONTAINER_OF to get the InterruptEntry from the gpio_callback member
    InterruptEntry* entry = CONTAINER_OF(cb, InterruptEntry, gpio_cb);
    if (entry && entry->active && entry->callback) {
        entry->callback();
    }
}

void ZephyrHal::delay(RadioLibTime_t ms) {
    k_msleep(ms);
}

void ZephyrHal::delayMicroseconds(RadioLibTime_t us) {
    k_busy_wait(us);
}

RadioLibTime_t ZephyrHal::millis() {
    return k_uptime_get_32();
}

RadioLibTime_t ZephyrHal::micros() {
    return (RadioLibTime_t)k_ticks_to_us_floor64(k_uptime_ticks());
}

long ZephyrHal::pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) {
    (void)pin; (void)state; (void)timeout;
    return 0;
}

void ZephyrHal::spiBegin() {
    if (spi_initialized_) return;

    // Use Zephyr SPI2 device (pins configured in board DTS: P1.13/P1.14/P1.15)
    spi_dev_ = DEVICE_DT_GET(DT_NODELABEL(spi2));
    if (!device_is_ready(spi_dev_)) {
        LOG_ERR("SPI2 device not ready");
        return;
    }

    // Configure SPI: 8 MHz, Mode 0, MSB first, 8-bit words
    spi_cfg_.frequency = 8000000;
    spi_cfg_.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER;
    // No CS in SPI config - RadioLib manages CS via GPIO (hal->digitalWrite)

    spi_initialized_ = true;
    LOG_INF("SPI initialized via Zephyr API (spi2)");
}

void ZephyrHal::spiBeginTransaction() {
    // Nothing needed - Zephyr SPI handles locking per-transfer
}

void ZephyrHal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
    if (!spi_initialized_ || !spi_dev_ || len == 0) return;

    struct spi_buf tx_buf = { .buf = out, .len = len };
    struct spi_buf rx_buf = { .buf = in, .len = len };

    // If no TX data, send zeros
    static uint8_t tx_zeros[256];
    if (!out) {
        memset(tx_zeros, 0x00, len < sizeof(tx_zeros) ? len : sizeof(tx_zeros));
        tx_buf.buf = tx_zeros;
        tx_buf.len = len < sizeof(tx_zeros) ? len : sizeof(tx_zeros);
    }

    // If no RX buffer, use dummy
    static uint8_t rx_dummy[256];
    if (!in) {
        rx_buf.buf = rx_dummy;
        rx_buf.len = len < sizeof(rx_dummy) ? len : sizeof(rx_dummy);
    }

    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive(spi_dev_, &spi_cfg_, &tx_set, &rx_set);
    if (ret < 0) {
        LOG_ERR("SPI transceive failed: %d", ret);
    }

    // Debug: log first few SPI transfers
    static int xfer_count = 0;
    if (xfer_count < 10) {
        uint8_t* rx = (uint8_t*)rx_buf.buf;
        uint8_t* tx = (uint8_t*)tx_buf.buf;
        LOG_INF("SPI[%d] len=%u tx=[%02x %02x %02x] rx=[%02x %02x %02x] ret=%d",
                xfer_count, (unsigned)len,
                len > 0 ? tx[0] : 0, len > 1 ? tx[1] : 0, len > 2 ? tx[2] : 0,
                len > 0 ? rx[0] : 0, len > 1 ? rx[1] : 0, len > 2 ? rx[2] : 0,
                ret);
        xfer_count++;
    }
}

void ZephyrHal::spiEndTransaction() {
    // Nothing needed
}

void ZephyrHal::spiEnd() {
    spi_initialized_ = false;
}

void ZephyrHal::yield() {
    k_yield();
}

}  // namespace meshcore_sensor
