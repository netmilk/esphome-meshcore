import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_add_overlay

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@netmilk"]
DEPENDENCIES = []
AUTO_LOAD = []

CONF_MESHCORE_SENSOR = "meshcore_sensor"
CONF_NODE_NAME = "name"
CONF_PASSWORD = "password"
CONF_LED_PIN = "led_pin"
CONF_MESHCORE_PATH = "meshcore_path"
CONF_ON_COMMAND = "on_command"
CONF_COMMAND_PREFIX = "command"

# LoRa pin config
CONF_LORA_CS = "lora_cs_pin"
CONF_LORA_RESET = "lora_reset_pin"
CONF_LORA_BUSY = "lora_busy_pin"
CONF_LORA_DIO1 = "lora_dio1_pin"
CONF_LORA_RXEN = "lora_rxen_pin"
CONF_LORA_SCLK = "lora_sclk_pin"
CONF_LORA_MISO = "lora_miso_pin"
CONF_LORA_MOSI = "lora_mosi_pin"

# I2C remap
CONF_I2C_SDA = "i2c_sda_pin"
CONF_I2C_SCL = "i2c_scl_pin"

# Radio params
CONF_FREQUENCY = "frequency"
CONF_BANDWIDTH = "bandwidth"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_CODING_RATE = "coding_rate"
CONF_TX_POWER = "tx_power"

meshcore_sensor_ns = cg.esphome_ns.namespace("meshcore_sensor")
MeshCoreSensorComponent = meshcore_sensor_ns.class_("MeshCoreSensorComponent", cg.Component)

COMMAND_HANDLER_SCHEMA = cv.Schema({
    cv.Required(CONF_COMMAND_PREFIX): cv.string,
    cv.Required("lambda"): cv.lambda_,
})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeshCoreSensorComponent),
        cv.Optional(CONF_NODE_NAME, default="ESPHome Sensor"): cv.string,
        cv.Optional(CONF_PASSWORD, default="password"): cv.string,
        cv.Optional(CONF_LED_PIN, default=26): cv.int_,
        cv.Optional(CONF_MESHCORE_PATH, default="MeshCore"): cv.string,
        # LoRa pins (nRF52840 GPIO numbers)
        cv.Optional(CONF_LORA_CS, default=4): cv.int_,        # P0.04 = D4
        cv.Optional(CONF_LORA_RESET, default=28): cv.int_,    # P0.28 = D2
        cv.Optional(CONF_LORA_BUSY, default=29): cv.int_,     # P0.29 = D3
        cv.Optional(CONF_LORA_DIO1, default=3): cv.int_,      # P0.03 = D1
        cv.Optional(CONF_LORA_RXEN, default=5): cv.int_,      # P0.05 = D5
        cv.Optional(CONF_LORA_SCLK, default=45): cv.int_,     # P1.13 = D8
        cv.Optional(CONF_LORA_MISO, default=46): cv.int_,     # P1.14 = D9
        cv.Optional(CONF_LORA_MOSI, default=47): cv.int_,     # P1.15 = D10
        # Czech/narrow preset defaults
        cv.Optional(CONF_FREQUENCY, default=869.432): cv.float_,
        cv.Optional(CONF_BANDWIDTH, default=62.5): cv.float_,
        cv.Optional(CONF_SPREADING_FACTOR, default=7): cv.int_range(min=5, max=12),
        cv.Optional(CONF_CODING_RATE, default=5): cv.int_range(min=5, max=8),
        cv.Optional(CONF_TX_POWER, default=22): cv.int_range(min=-9, max=22),
        # I2C remap (default I2C1 pins P0.04/P0.05 conflict with LoRa CS/RXEN)
        cv.Optional(CONF_I2C_SDA): cv.int_,
        cv.Optional(CONF_I2C_SCL): cv.int_,
        # Command handlers
        cv.Optional(CONF_ON_COMMAND): cv.ensure_list(COMMAND_HANDLER_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    import os
    import subprocess

    # Resolve paths relative to the component repo root (works for both
    # local and external_components usage — __file__ is always inside the repo)
    component_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(component_dir))

    meshcore_path = config[CONF_MESHCORE_PATH]
    if not os.path.isabs(meshcore_path):
        meshcore_path = os.path.join(repo_root, meshcore_path)

    # Auto-init MeshCore submodule if not populated
    if not os.path.exists(os.path.join(meshcore_path, "src")):
        _LOGGER.info("Initializing MeshCore submodule...")
        subprocess.check_call(
            ["git", "submodule", "update", "--init", "MeshCore"],
            cwd=repo_root,
        )

    # Set node name and password
    cg.add(var.set_node_name(config[CONF_NODE_NAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_led_pin(config[CONF_LED_PIN]))

    # Set LoRa pins
    cg.add(var.set_lora_cs(config[CONF_LORA_CS]))
    cg.add(var.set_lora_reset(config[CONF_LORA_RESET]))
    cg.add(var.set_lora_busy(config[CONF_LORA_BUSY]))
    cg.add(var.set_lora_dio1(config[CONF_LORA_DIO1]))
    cg.add(var.set_lora_rxen(config[CONF_LORA_RXEN]))
    cg.add(var.set_lora_sclk(config[CONF_LORA_SCLK]))
    cg.add(var.set_lora_miso(config[CONF_LORA_MISO]))
    cg.add(var.set_lora_mosi(config[CONF_LORA_MOSI]))

    # Set radio params
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))

    # Register on_command handlers
    for cmd_conf in config.get(CONF_ON_COMMAND, []):
        lambda_ = await cg.process_lambda(
            cmd_conf["lambda"],
            [(cg.std_string, "command")],
            return_type=cg.std_string,
        )
        cg.add(var.add_command_handler(cmd_conf[CONF_COMMAND_PREFIX], lambda_))

    # Stubs directory is at the repo root, next to components/
    stubs_dir = os.path.join(repo_root, "meshcore_stubs")
    cg.add_build_flag(f"-I{stubs_dir}")

    # MeshCore include paths
    cg.add_build_flag(f"-I{meshcore_path}/src")
    cg.add_build_flag(f"-I{meshcore_path}/src/helpers")
    cg.add_build_flag(f"-I{meshcore_path}/src/helpers/radiolib")
    cg.add_build_flag(f"-I{meshcore_path}/src/helpers/sensors")
    cg.add_build_flag(f"-I{meshcore_path}/examples/simple_sensor")
    cg.add_build_flag(f"-I{meshcore_path}/lib/ed25519")
    # Note: NOT including variants/xiao_nrf52 - our stubs dir provides target.h

    # RadioLib and MeshCore build flags
    cg.add_build_flag("-DRADIOLIB_STATIC_ONLY=1")
    cg.add_build_flag("-DRADIOLIB_GODMODE=1")
    cg.add_build_flag("-DZEPHYR_PLATFORM=1")
    cg.add_build_flag("-DMESHCORE_ESPHOME=1")
    cg.add_build_flag("-DNRF52_PLATFORM=1")

    # Exclude unused RadioLib modules to reduce build size
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_CC1101=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_RF69=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_SX1231=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_SI443X=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_RFM2X=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_SX128X=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_AFSK=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_AX25=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_HELLSCHREIBER=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_MORSE=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_APRS=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_BELL=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_RTTY=1")
    cg.add_build_flag("-DRADIOLIB_EXCLUDE_SSTV=1")

    # Radio hardware config (matches MeshCore XIAO nRF52 variant)
    cg.add_build_flag("-DSX126X_DIO2_AS_RF_SWITCH=1")
    cg.add_build_flag("-DSX126X_DIO3_TCXO_VOLTAGE=1.8")
    cg.add_build_flag("-DSX126X_CURRENT_LIMIT=140")
    cg.add_build_flag("-DSX126X_RX_BOOSTED_GAIN=1")

    # Add RadioLib as PlatformIO library (for header includes + source download)
    cg.add_library("RadioLib", "7.3.0")
    # Add Crypto library
    cg.add_library("rweather/Crypto", "0.4.0")
    # Add CayenneLPP
    cg.add_library("electroniccats/CayenneLPP", "1.6.1")

    # Force include paths for libraries that LDF doesn't auto-detect
    # (because includes are in unity-built MeshCore .cpp files)
    cg.add_platformio_option("lib_ldf_mode", "deep+")

    # Manually add library include paths (unity build hides them from LDF)
    from esphome.core import CORE
    device_name = CORE.name
    build_dir = CORE.build_path
    libdeps_base = os.path.join(build_dir, f".piolibdeps/{device_name}")

    crypto_path = os.path.join(libdeps_base, "Crypto")
    cayenne_path = os.path.join(libdeps_base, "CayenneLPP/src")
    radiolib_src = os.path.join(libdeps_base, "RadioLib/src")

    if os.path.exists(crypto_path):
        cg.add_build_flag(f"-I{crypto_path}")
    if os.path.exists(cayenne_path):
        cg.add_build_flag(f"-I{cayenne_path}")
    if os.path.exists(radiolib_src):
        cg.add_build_flag(f"-I{radiolib_src}")

    # Library source path defines for unity build (single defines, no quoting issues)
    # PlatformIO on Zephyr doesn't compile libraries, so we #include the .cpp files
    # The .cpp file uses preprocessor stringification to construct full paths
    cg.add_build_flag(f"-DRADIOLIB_SRC_PATH={radiolib_src}")
    cg.add_build_flag(f"-DCRYPTO_SRC_PATH={crypto_path}")
    cg.add_build_flag(f"-DCAYENNE_SRC_PATH={cayenne_path}")

    cg.add_build_flag("-w")  # suppress warnings from third-party code

    # Ignore Arduino-only libraries that MeshCore references
    cg.add_platformio_option("lib_ignore", [
        "SPI",
        "Wire",
        "RTClib",
        "Melopero RV3028",
        "Adafruit nRFCrypto",
    ])

    # Enable SPI in Zephyr via prj.conf (needed for Zephyr SPI driver API)
    zephyr_add_prj_conf("SPI", True)

    # Increase main thread stack size (ESPHome default 2KB is too small for MeshCore
    # call chains: RadioLib SPI + ed25519 crypto + File malloc(4KB) + settings flash I/O)
    # Override the existing value by patching the prj_conf dict directly
    from esphome.components.zephyr import zephyr_data
    from esphome.components.zephyr.const import KEY_PRJ_CONF
    prj_conf = zephyr_data()[KEY_PRJ_CONF]
    prj_conf["CONFIG_MAIN_STACK_SIZE"] = (8192, True)

    # Enable MPU stack guard for hard fault diagnostics
    zephyr_add_prj_conf("MPU_STACK_GUARD", True)
    zephyr_add_prj_conf("HW_STACK_PROTECTION", True)

    # Increase heap for File malloc(4KB) buffers
    zephyr_add_prj_conf("HEAP_MEM_POOL_SIZE", 16384)

    # Increase system workqueue stack (used by settings/flash writes)
    zephyr_add_prj_conf("SYSTEM_WORKQUEUE_STACK_SIZE", 4096)

    # I2C1 default pins P0.04/P0.05 conflict with LoRa CS/RXEN.
    # If user provides alternative pins, remap I2C1. Otherwise disable it.
    i2c_sda = config.get(CONF_I2C_SDA)
    i2c_scl = config.get(CONF_I2C_SCL)
    if i2c_sda is not None and i2c_scl is not None:
        sda_port = i2c_sda // 32
        sda_pin = i2c_sda % 32
        scl_port = i2c_scl // 32
        scl_pin = i2c_scl % 32
        zephyr_add_overlay(f"""
&pinctrl {{
    i2c1_default: i2c1_default {{
        group1 {{
            psels = <NRF_PSEL(TWIM_SDA, {sda_port}, {sda_pin})>,
                <NRF_PSEL(TWIM_SCL, {scl_port}, {scl_pin})>;
        }};
    }};
    i2c1_sleep: i2c1_sleep {{
        group1 {{
            psels = <NRF_PSEL(TWIM_SDA, {sda_port}, {sda_pin})>,
                <NRF_PSEL(TWIM_SCL, {scl_port}, {scl_pin})>;
            low-power-enable;
        }};
    }};
}};
""")
    else:
        zephyr_add_overlay("""
&i2c1 {
    status = "disabled";
};
""")
