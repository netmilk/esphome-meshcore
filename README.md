# ESPHome MeshCore Sensor

Control anything ESPHome can touch — switches, sensors, fans, lights — over long-range LoRa mesh, managed from [MeshCore Companion](https://meshcore.co).

> **XIAO nRF52840 only.** This component has been developed and tested exclusively on the Seeed XIAO nRF52840 with SX1262 LoRa radio, running on ESPHome's Zephyr/nRF52 platform. It will not work on ESP32/ESP8266 boards without significant modifications to the HAL, SPI driver, and build system.

## Quick Start

**Prerequisites:** ESPHome with nRF52 support (`pip install esphome`)

Create a YAML file — no need to clone this repo:

```yaml
esphome:
  name: my-sensor

nrf52:
  board: xiao_ble
  bootloader: adafruit
  dfu:
    reset_pin: 18

logger:
  level: DEBUG

external_components:
  - source:
      type: git
      url: https://github.com/netmilk/esphome-meshcore
      ref: main
    components: [meshcore_sensor]

meshcore_sensor:
  name: "My Sensor"
  password: "mypassword"
  frequency: 869.432      # see Radio Presets below
  bandwidth: 62.5
  spreading_factor: 7
  coding_rate: 5
  tx_power: 22

  on_command:
    - command: "uptime"
      lambda: |-
        uint32_t s = millis() / 1000;
        char buf[32];
        snprintf(buf, sizeof(buf), "%ud %uh %um %us",
                 s/86400, (s/3600)%24, (s/60)%60, s%60);
        return std::string(buf);
```

Build and flash:

```bash
esphome compile my-sensor.yaml

# First flash: double-tap reset button, device mounts as XIAO-SENSE drive
cp .esphome/build/my-sensor/.pioenvs/my-sensor/zephyr/zephyr.uf2 /Volumes/XIAO-SENSE/

# After that, DFU is automatic:
esphome upload my-sensor.yaml
```

Open the Companion, tap **Discover Sensors** — your sensor should appear.

## Configuration

| Key | Default | Description |
|-----|---------|-------------|
| `name` | `"ESPHome Sensor"` | Node name shown in Companion |
| `password` | `"password"` | Login password for remote management |
| `led_pin` | `26` | Onboard LED GPIO. Used as TX activity indicator (flashes on radio transmit) and controllable via `set_led()`/`get_led()` in lambdas |
| `frequency` | `869.432` | LoRa frequency in MHz |
| `bandwidth` | `62.5` | LoRa bandwidth in kHz |
| `spreading_factor` | `7` | LoRa SF (5-12) |
| `coding_rate` | `5` | LoRa CR (5-8) |
| `tx_power` | `22` | TX power in dBm (-9 to 22) |
| `i2c_sda_pin` | — | Remap I2C1 SDA to this GPIO. Default I2C1 pins (P0.04/P0.05) conflict with LoRa, so I2C1 is disabled unless you remap it |
| `i2c_scl_pin` | — | Remap I2C1 SCL to this GPIO. Must be set together with `i2c_sda_pin` |

### Radio Presets

All devices on the mesh must use the same radio preset.

| Preset | Frequency | BW | SF | CR |
|--------|-----------|-----|----|----|
| Czech/Narrow | 869.432 | 62.5 | 7 | 5 |
| US/Default | 915.0 | 250 | 10 | 5 |

## Commands

Define commands that users can send from the Companion. Each command has a prefix to match and a C++ lambda that returns a response string.

```yaml
meshcore_sensor:
  id: meshcore
  # ... radio config ...

  on_command:
    - command: "led on"
      lambda: |-
        id(meshcore).set_led(true);
        return std::string("LED is ON");

    - command: "led off"
      lambda: |-
        id(meshcore).set_led(false);
        return std::string("LED is OFF");
```

Lambdas receive the full command string and can reference any ESPHome entity via `id()`.

## Hardware

- **MCU:** Seeed XIAO nRF52840 (Sense or non-Sense)
- **Radio:** SX1262 LoRa module (MeshCore XIAO variant pinout)
- **Companion:** [MeshCore Companion](https://meshcore.co)

### Pin Mapping

| Function | XIAO Pin | nRF52840 GPIO | Config key |
|----------|----------|---------------|------------|
| SPI SCK  | D8       | P1.13 (45)    | `lora_sclk_pin` |
| SPI MISO | D9       | P1.14 (46)    | `lora_miso_pin` |
| SPI MOSI | D10      | P1.15 (47)    | `lora_mosi_pin` |
| CS       | D4       | P0.04 (4)     | `lora_cs_pin` |
| RESET    | D2       | P0.28 (28)    | `lora_reset_pin` |
| BUSY     | D3       | P0.29 (29)    | `lora_busy_pin` |
| DIO1     | D1       | P0.03 (3)     | `lora_dio1_pin` |
| RXEN     | D5       | P0.05 (5)     | `lora_rxen_pin` |

D0 (P0.02) is the only pin free by default. D6/D7 are used by UART0 (serial console). To free up more pins, you can remap I2C or UART via config/overlays.

## Troubleshooting

**Not visible in Discover Sensors:** Companion must use the same radio preset (frequency, bandwidth, SF, CR).

**Login fails:** Password must match the `password` config exactly.

**Radio init fails (error -2):** SPI issue. Check pin connections match the config.

**DFU not working:** First flash always requires double-tap reset. After that, `esphome upload` handles it automatically via 1200 baud touch.

## Development

```bash
git clone --recurse-submodules https://github.com/netmilk/esphome-meshcore.git
cd esphome-meshcore
esphome compile xiao-meshcore.yaml
```

MeshCore source is a git submodule at `./MeshCore`, pinned to a known working commit. When used as an external component, the submodule is fetched automatically at build time.
