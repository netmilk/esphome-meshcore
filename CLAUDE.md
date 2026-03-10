# esphome-meshcore

ESPHome MeshCore sensor component for XIAO nRF52840 + SX1262 LoRa radio.

## Build & Flash

```bash
git submodule update --init                # fetch MeshCore submodule (if not cloned with --recurse-submodules)
esphome compile examples/xiao-meshcore.yaml         # compile
esphome upload examples/xiao-meshcore.yaml --device /dev/cu.usbmodem135401  # flash via DFU
esphome logs examples/xiao-meshcore.yaml --device /dev/cu.usbmodem135401    # serial monitor
```

Serial ports: `/dev/cu.usbmodem135401` (primary), `/dev/cu.usbmodem135403` (secondary). Opening primary resets the device.

**Entering DFU mode** — preferred method is software, no physical button needed:
1. **Software (preferred)**: `esphome upload` opens the serial port at 1200 baud, which triggers DFU. The Zephyr USB CDC callback writes magic `0x5A1AD5` to RAM address `0x20007F7C` and warm-reboots into the Adafruit bootloader. Implementation is in `meshcore_sensor.cpp` `loop()` — it polls for the magic value and calls `sys_reboot(SYS_REBOOT_WARM)`.
2. **Hardware (fallback)**: Double-tap the physical reset button.

**Why ESPHome's built-in nRF52 DFU doesn't work**: ESPHome's [nRF52 DFU](https://esphome.io/components/nrf52/#dfu-device-firmware-update) expects a `reset_pin` GPIO connected to the MCU's reset line to trigger a hardware reset. The XIAO nRF52840 has no such GPIO — pin 18 (configured in yaml) doesn't actually reset the MCU. So we bypass ESPHome's GPIO method and instead use the Adafruit bootloader's magic RAM convention directly.

Both methods mount the device as `XIAO-SENSE` USB drive. Then either:
- `esphome upload` flashes via nrfutil (may fail with serial errors if port re-enumerates), or
- Copy UF2 directly: `cp .esphome/build/xiao-meshcore/.pioenvs/xiao-meshcore/zephyr/zephyr.uf2 /Volumes/XIAO-SENSE/`

If `esphome upload` fails with serial errors after triggering DFU, the device is already in bootloader mode — just use the UF2 copy method.

No test suite exists — test by flashing and using the MeshCore companion app.

## Architecture

**Unity build**: All MeshCore, RadioLib, Crypto, and CayenneLPP `.cpp` files are `#include`d into `meshcore_sensor.cpp`. PlatformIO doesn't compile Zephyr libraries separately.

**Key files**:
- `xiao-meshcore.yaml` — ESPHome device config, radio params, CLI command handlers
- `components/meshcore_sensor/__init__.py` — ESPHome codegen, build flags, Zephyr prj.conf
- `components/meshcore_sensor/meshcore_sensor.cpp` — Main component (setup/loop), unity build includes
- `components/meshcore_sensor/meshcore_sensor.h` — Component class definition
- `components/meshcore_sensor/zephyr_board.cpp` — Board HAL (SPI, GPIO, interrupts, DFU)
- `meshcore_stubs/arduino_compat.h` — Arduino API shims for Zephyr (File, SPI, millis, min/max, settings-backed filesystem)
- `meshcore_stubs/target.h` — Hardware pin/radio defines (replaces MeshCore variant header)

**External dependency**: MeshCore source is a git submodule at `./MeshCore` (pinned to a specific commit). Auto-initialized at build time by `__init__.py` when used as external component. For local dev: `git clone --recurse-submodules` or `git submodule update --init`.

**External component usage**: Users don't need to clone this repo. They just reference it in their YAML:
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/netmilk/esphome-meshcore
      ref: main
    components: [meshcore_sensor]
```
ESPHome clones the repo, and `__init__.py` auto-inits the MeshCore submodule. All paths resolve relative to the component repo root via `__file__`, not CWD.

## Critical Gotchas

- **min/max**: Must be plain C macros, not C++ templates. ARM `decltype` deduction breaks with mixed types (ptrdiff_t vs int).
- **Stack size**: Main thread needs 8KB (`CONFIG_MAIN_STACK_SIZE=8192`). Default 2KB causes hard faults during deep MeshCore call chains.
- **SX1262 reset**: Needs explicit hardware reset (RST LOW->HIGH) + SPI pre-init before RadioLib `begin()`.
- **DIO1 interrupt**: Must call stored callback via `CONTAINER_OF`. A no-op handler breaks all RX.
- **I2C1 conflict**: Default I2C1 pins P0.04/P0.05 conflict with LoRa CS/RXEN. Disabled by default, or remapped via `i2c_sda_pin`/`i2c_scl_pin` config (generates DT overlay with new pinctrl).
- **DFU magic**: Address `0x20007F7C` stores `0x5A1AD5` for bootloader. Must clear in `setup()` to prevent reboot loops after warm reset.
- **No `CONFIG_ENTROPY_GENERATOR`**: Causes boot loop on this board. Use `stdlib rand()`.
- **Node prefs + identity**: Must be set BEFORE `SensorMesh::begin()` (begin calls `radio_set_params`).
- **Filesystem**: Settings-backed (`zephyr/settings.h`), not real LittleFS. Files buffered in RAM (4KB max), persisted to NVS flash on `close()`.
