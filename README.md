# ESPHome MeshCore Sensor Component

MeshCore mesh networking as an ESPHome component for the XIAO nRF52840 + SX1262 LoRa radio (MeshCore XIAO variant).

This is a **port** of the [MeshCore](https://github.com/rocketgarden/MeshCore) library running on ESPHome's Zephyr/nRF52 platform. The original MeshCore protocol code runs unmodified — we provide the Zephyr HAL (GPIO, SPI, interrupts) and ESPHome component glue.

## Hardware

- **MCU:** Seeed XIAO nRF52840 (Sense or non-Sense)
- **Radio:** SX1262 LoRa module connected via SPI (MeshCore XIAO variant pinout)
- **Companion app:** [MeshCore Companion](https://meshcore.co) (iOS/Android)

### Pin Mapping (XIAO nRF52840 → SX1262)

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

## Setup

### Prerequisites

- ESPHome with nRF52 support (`pip install esphome`)
- PlatformIO (installed automatically by ESPHome)

### Clone

```bash
git clone --recurse-submodules https://github.com/<your-org>/nrf-home.git
cd nrf-home

# If you already cloned without --recurse-submodules:
git submodule update --init
```

MeshCore source is included as a git submodule at `./MeshCore`, pinned to a known working commit.

### Project Structure

```
nrf-home/
├── xiao-meshcore.yaml              # ESPHome config
├── MeshCore/                       # Git submodule (pinned)
├── components/
│   └── meshcore_sensor/            # The component
│       ├── __init__.py             # Build config + codegen
│       ├── meshcore_sensor.h       # Component header
│       ├── meshcore_sensor.cpp     # Unity build + component impl
│       ├── zephyr_hal.h/cpp        # RadioLib HAL for Zephyr
│       └── zephyr_board.h/cpp      # MeshCore MainBoard for Zephyr
└── meshcore_stubs/                 # Arduino API stubs
    ├── Arduino.h
    ├── arduino_compat.h
    ├── Stream.h
    ├── target.h
    └── ...
```

### Build & Flash

```bash
# Compile
esphome compile xiao-meshcore.yaml

# Flash (first time: double-tap reset button, then copy UF2)
cp .esphome/build/xiao-meshcore/.pioenvs/xiao-meshcore/zephyr/zephyr.uf2 /Volumes/XIAO-SENSE/

# Subsequent flashes: DFU is automatic (1200 baud touch)
esphome upload xiao-meshcore.yaml
```

## Configuration

### Minimal Config

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
      type: local
      path: components

meshcore_sensor:
  name: "My Sensor"
  password: "mypassword"
  frequency: 869.432
  bandwidth: 62.5
  spreading_factor: 7
  coding_rate: 5
  tx_power: 22
```

### Configuration Options

| Key | Default | Description |
|-----|---------|-------------|
| `name` | `"ESPHome Sensor"` | Node name visible in companion app |
| `password` | `"password"` | Login password for remote management |
| `led_pin` | `26` | Onboard LED GPIO (P0.26, active low) |
| `meshcore_path` | `"MeshCore"` | Path to MeshCore source directory (submodule) |
| `frequency` | `869.432` | LoRa frequency in MHz |
| `bandwidth` | `62.5` | LoRa bandwidth in kHz |
| `spreading_factor` | `7` | LoRa SF (5-12) |
| `coding_rate` | `5` | LoRa CR (5-8) |
| `tx_power` | `22` | TX power in dBm (-9 to 22) |

### Radio Presets

| Preset | Frequency | BW | SF | CR |
|--------|-----------|-----|----|----|
| Czech/Narrow | 869.432 | 62.5 | 7 | 5 |
| US/Default | 915.0 | 250 | 10 | 5 |

## Command Handlers (`on_command`)

The `on_command` config lets you define MeshCore CLI command handlers as lambdas. When a logged-in user sends a command from the companion app, matching handlers execute and return a response.

Each handler has:
- **`command`**: prefix to match (e.g. `"led"` matches `"led on"`, `"led off"`, etc.)
- **`lambda`**: C++ lambda receiving `std::string command`, returning `std::string` response

```yaml
meshcore_sensor:
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

The lambda receives the full command string. Use it to parse arguments:

```yaml
    - command: "gpio"
      lambda: |-
        if (command == "gpio on") {
          id(my_switch).turn_on();
          return std::string("GPIO ON");
        } else if (command == "gpio off") {
          id(my_switch).turn_off();
          return std::string("GPIO OFF");
        }
        return std::string("Usage: gpio on|off");
```

## Examples

### LED Control with ESPHome Switch

Expose the onboard LED as an ESPHome [template switch](https://esphome.io/components/switch/template), controllable both locally and via MeshCore commands:

```yaml
switch:
  - platform: template
    id: led_switch
    name: "LED"
    optimistic: true
    turn_on_action:
      - lambda: id(meshcore).set_led(true);
    turn_off_action:
      - lambda: id(meshcore).set_led(false);

meshcore_sensor:
  id: meshcore
  name: "LED Sensor"
  password: "password"
  frequency: 869.432
  bandwidth: 62.5
  spreading_factor: 7
  coding_rate: 5
  tx_power: 22

  on_command:
    - command: "led on"
      lambda: |-
        id(led_switch).turn_on();
        return std::string("LED is ON");

    - command: "led off"
      lambda: |-
        id(led_switch).turn_off();
        return std::string("LED is OFF");

    - command: "led toggle"
      lambda: |-
        if (id(led_switch).state) {
          id(led_switch).turn_off();
          return std::string("LED toggled OFF");
        } else {
          id(led_switch).turn_on();
          return std::string("LED toggled ON");
        }

    - command: "led status"
      lambda: |-
        return id(led_switch).state
          ? std::string("LED: ON")
          : std::string("LED: OFF");
```

From the companion app, send `led on`, `led off`, `led toggle`, or `led status` as CLI commands.

### GPIO Switch (Relay)

Control an external relay via MeshCore:

```yaml
switch:
  - platform: gpio
    pin: 3  # your relay pin
    id: relay
    name: "Relay"

meshcore_sensor:
  id: meshcore
  # ... radio config ...

  on_command:
    - command: "relay on"
      lambda: |-
        id(relay).turn_on();
        return std::string("Relay ON");

    - command: "relay off"
      lambda: |-
        id(relay).turn_off();
        return std::string("Relay OFF");

    - command: "relay status"
      lambda: |-
        return id(relay).state
          ? std::string("Relay: ON")
          : std::string("Relay: OFF");
```

### Reading Sensors

Read ESPHome sensor values over MeshCore:

```yaml
sensor:
  - platform: dht
    pin: 7
    temperature:
      id: temp
      name: "Temperature"
    humidity:
      id: humidity
      name: "Humidity"

meshcore_sensor:
  id: meshcore
  # ... radio config ...

  on_command:
    - command: "status"
      lambda: |-
        char buf[64];
        snprintf(buf, sizeof(buf), "T=%.1fC H=%.0f%%",
                 id(temp).state, id(humidity).state);
        return std::string(buf);

    - command: "temp"
      lambda: |-
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f C", id(temp).state);
        return std::string(buf);
```

### H-Bridge Fan Control

Control an [H-Bridge fan](https://esphome.io/components/fan/hbridge) over MeshCore:

```yaml
output:
  - platform: gpio
    id: fan_fwd
    pin: 3
  - platform: gpio
    id: fan_rev
    pin: 4

fan:
  - platform: hbridge
    id: attic_fan
    name: "Attic Fan"
    pin_a: fan_fwd
    pin_b: fan_rev

meshcore_sensor:
  id: meshcore
  # ... radio config ...

  on_command:
    - command: "fan on"
      lambda: |-
        auto call = id(attic_fan).turn_on();
        call.perform();
        return std::string("Fan ON");

    - command: "fan off"
      lambda: |-
        auto call = id(attic_fan).turn_off();
        call.perform();
        return std::string("Fan OFF");

    - command: "fan status"
      lambda: |-
        return id(attic_fan).state
          ? std::string("Fan: ON")
          : std::string("Fan: OFF");
```

### External Component (e.g. JK-BMS)

Read values from any external ESPHome component like [esphome-jk-bms](https://github.com/syssi/esphome-jk-bms):

```yaml
uart:
  - id: bms_uart
    rx_pin: 7
    baud_rate: 115200

jk_bms:
  uart_id: bms_uart

sensor:
  - platform: jk_bms
    total_voltage:
      id: bms_voltage
      name: "BMS Voltage"
    current:
      id: bms_current
      name: "BMS Current"
    power:
      id: bms_power
      name: "BMS Power"
    state_of_charge:
      id: bms_soc
      name: "BMS SoC"

meshcore_sensor:
  id: meshcore
  # ... radio config ...

  on_command:
    - command: "bms"
      lambda: |-
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "V=%.1fV I=%.1fA P=%.0fW SoC=%.0f%%",
                 id(bms_voltage).state,
                 id(bms_current).state,
                 id(bms_power).state,
                 id(bms_soc).state);
        return std::string(buf);
```

## Component API

Methods available on the component for use in lambdas:

| Method | Description |
|--------|-------------|
| `id(meshcore).set_led(bool)` | Turn onboard LED on/off |
| `id(meshcore).get_led()` | Get current LED state |

Standard ESPHome `id()` references work in all lambdas — use them to read sensors, control switches, fans, lights, or any other ESPHome entity.

## Architecture

```
┌──────────────────────────────────────────────┐
│  ESPHome YAML Config                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐  │
│  │ Sensors  │ │ Switches │ │ Fans/Lights  │  │
│  │ (DHT,BMS)│ │ (GPIO)   │ │ (H-Bridge)   │  │
│  └────┬─────┘ └────┬─────┘ └──────┬───────┘  │
│       │id()        │id()          │id()       │
│  ┌────▼────────────▼──────────────▼────────┐  │
│  │  meshcore_sensor component              │  │
│  │                                         │  │
│  │  on_command:                            │  │
│  │    "status" → lambda: return sensor val │  │
│  │    "relay on" → lambda: switch.turn_on  │  │
│  │    "fan off"  → lambda: fan.turn_off    │  │
│  │                                         │  │
│  │  MeshCore SensorMesh (original code)    │  │
│  │  RadioLib + Zephyr HAL                  │  │
│  └─────────────────┬──────────────────────┘  │
│                    │ SX1262 LoRa              │
└────────────────────┼─────────────────────────┘
                     │
               MeshCore Mesh
                     │
            Companion App / Repeaters
```

## Troubleshooting

**Radio init fails (error -2):** SPI communication issue. Check pin connections match the config. The component does a pre-flight hardware reset of the SX1262 before init.

**Not visible in Discover Sensors:** Ensure the companion app is on the same radio preset (frequency, bandwidth, SF, CR). Try "Discover Sensors" (not just seeing adverts in contacts).

**Login fails:** Verify the password matches. The companion app sends the password during login — it must match the `password` config exactly.

**DFU not working:** The first flash requires manual double-tap reset. After that, DFU works automatically via 1200 baud touch (`esphome upload`).

**Identity changes on reboot:** Identity is persisted to flash automatically on first boot. If you see it changing, the flash settings may not be saving — check serial log for "Identity saved to flash".
