// ESPHome MeshCore Sensor Component - Implementation
//
// This file ties together:
// - RadioLib with our Zephyr HAL
// - MeshCore protocol (Mesh, Dispatcher, Packet, etc.)
// - SensorMesh with custom LED CLI commands
// - ESPHome Component lifecycle

#include "meshcore_sensor.h"
#include "zephyr_hal.h"
#include "zephyr_board.h"
#include <arduino_compat.h>

#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

// ---- Provide Arduino global stubs ----
SerialStub Serial;
SPIClass SPI;
WireStub Wire;
InternalFileSystemClass InternalFS;

// ---- MeshCore build configuration ----
// These defines configure MeshCore for our Zephyr environment
// Radio params are set at runtime, but we need compile-time defaults
// for MeshCore's CustomSX1262::std_init()
#ifndef LORA_FREQ
#define LORA_FREQ  869.432f
#endif
#ifndef LORA_BW
#define LORA_BW    62.5f
#endif
#ifndef LORA_SF
#define LORA_SF    7
#endif
#ifndef LORA_CR
#define LORA_CR    5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 22
#endif

// Pin defines expected by MeshCore's CustomSX1262
// These are placeholders - actual pin control goes through ZephyrHal
#ifndef P_LORA_SCLK
#define P_LORA_SCLK  45
#endif
#ifndef P_LORA_MISO
#define P_LORA_MISO  46
#endif
#ifndef P_LORA_MOSI
#define P_LORA_MOSI  47
#endif
#ifndef P_LORA_NSS
#define P_LORA_NSS   4
#endif
#ifndef P_LORA_DIO_1
#define P_LORA_DIO_1 3
#endif
#ifndef P_LORA_RESET
#define P_LORA_RESET 28
#endif
#ifndef P_LORA_BUSY
#define P_LORA_BUSY  29
#endif
#ifndef SX126X_RXEN
#define SX126X_RXEN  5
#endif
#ifndef SX126X_TXEN
#define SX126X_TXEN  RADIOLIB_NC
#endif
#ifndef P_LORA_TX_LED
#define P_LORA_TX_LED 26
#endif

// ---- RadioLib source files (unity build - Zephyr PIO doesn't compile libs) ----
// RADIOLIB_SRC_PATH is set via -D build flag in __init__.py (unquoted path)
// We use preprocessor stringification to build #include paths
#define _RLSTR2(x) #x
#define _RLSTR(x)  _RLSTR2(x)
#define _RLSRC(f)  _RLSTR(RADIOLIB_SRC_PATH/f)

#include _RLSRC(Hal.cpp)
#include _RLSRC(Module.cpp)
#include _RLSRC(modules/SX126x/SX126x.cpp)
#include _RLSRC(modules/SX126x/SX126x_commands.cpp)
#include _RLSRC(modules/SX126x/SX126x_config.cpp)
#include _RLSRC(modules/SX126x/SX1262.cpp)
#include _RLSRC(modules/SX126x/SX126x_LR_FHSS.cpp)
#include _RLSRC(protocols/PhysicalLayer/PhysicalLayer.cpp)
#include _RLSRC(utils/CRC.cpp)
#include _RLSRC(utils/FEC.cpp)
#include _RLSRC(utils/Utils.cpp)

// ---- Crypto library source files (unity build) ----
// CRYPTO_SRC_PATH set via -D build flag
// MeshCore.h defines SEED_SIZE as a macro which collides with Crypto's RNGClass::SEED_SIZE
#undef SEED_SIZE
#define _CRSRC(f) _RLSTR(CRYPTO_SRC_PATH/f)
#include _CRSRC(Crypto.cpp)
#include _CRSRC(Hash.cpp)
#include _CRSRC(SHA256.cpp)
#include _CRSRC(SHA512.cpp)
#include _CRSRC(BlockCipher.cpp)
#include _CRSRC(AESCommon.cpp)
#include _CRSRC(AES128.cpp)
#include _CRSRC(Cipher.cpp)
#include _CRSRC(Ed25519.cpp)
#include _CRSRC(Curve25519.cpp)
#include _CRSRC(BigNumberUtil.cpp)
// RNG.cpp needs EEPROM/NVS which we don't have - skip it
// #include _CRSRC(RNG.cpp)
// #include _CRSRC(NoiseSource.cpp)
// Restore SEED_SIZE for MeshCore code
#define SEED_SIZE 32

// ---- CayenneLPP source files (unity build) ----
// CAYENNE_SRC_PATH set via -D build flag
// CayenneLPPPolyline uses std::max - undef our min/max macros temporarily
#undef min
#undef max
#define _CLSRC(f) _RLSTR(CAYENNE_SRC_PATH/f)
// Stub CayenneLPPPolyline before including CayenneLPP (Polyline has ARM32 type mismatch)
#include <CayenneLPPPolyline.h>
CayenneLPPPolyline::CayenneLPPPolyline(uint32_t size) : m_maxSize(size) {}
std::vector<uint8_t> CayenneLPPPolyline::encode(const std::vector<Point>&, uint8_t, Simplification) { return {}; }
std::vector<uint8_t> CayenneLPPPolyline::encode(const std::vector<Point>&, Precision, Simplification) { return {}; }
std::vector<std::pair<double, double>> CayenneLPPPolyline::decode(const std::vector<uint8_t>&) { return {}; }
CayenneLPPPolyline::Stats CayenneLPPPolyline::getEncodeStats() const { return {}; }

#include _CLSRC(CayenneLPP.cpp)
// Restore min/max for MeshCore code
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

// ---- Include MeshCore source files (unity build) ----
// Core protocol (mostly platform-agnostic)
#include "Packet.cpp"
#include "Identity.cpp"
#include "Utils.cpp"
#include "Dispatcher.cpp"
#include "Mesh.cpp"

// Helpers
#include "helpers/StaticPoolPacketManager.cpp"
#include "helpers/AdvertDataHelpers.cpp"
#include "helpers/ClientACL.cpp"
#include "helpers/CommonCLI.cpp"
#include "helpers/IdentityStore.cpp"
#include "helpers/TxtDataHelpers.cpp"
#include "helpers/radiolib/RadioLibWrappers.cpp"

// Ed25519 crypto (orlp/ed25519 from MeshCore/lib/ed25519)
#define ED25519_NO_SEED
extern "C" {
#include "fe.c"
#include "ge.c"
// sc.c has duplicate static load_3/load_4 (also in fe.c) - rename them
#define load_3 sc_load_3
#define load_4 sc_load_4
#include "sc.c"
#undef load_3
#undef load_4
#include "keypair.c"
#include "sign.c"
#include "verify.c"
#include "key_exchange.c"
#include "add_scalar.c"
// sha512.c has a static K[80] that collides with AESCommon.cpp's static K[8]
#define K ed25519_sha512_K
#include "sha512.c"
#undef K
}

// Sensor example
#include "SensorMesh.cpp"
#include "TimeSeriesData.cpp"

// Sensor manager
#include "sensors/EnvironmentSensorManager.cpp"

// ---- Now include MeshCore headers for our code ----
#include <RadioLib.h>
#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <SensorMesh.h>

// ---- Provide MeshCore globals ----
EnvironmentSensorManager sensors;

static const char *const TAG = "meshcore_sensor";

// ---- Globals required by SensorMesh.cpp (via target.h) ----
using namespace meshcore_sensor;
static ZephyrBoard s_board_instance;
mesh::MainBoard& board = s_board_instance;
static uint8_t s_radio_mem[sizeof(CustomSX1262)] __attribute__((aligned(8)));
static uint8_t s_radio_driver_mem[sizeof(CustomSX1262Wrapper)] __attribute__((aligned(8)));
CustomSX1262& radio_obj = reinterpret_cast<CustomSX1262&>(s_radio_mem);
CustomSX1262Wrapper& radio_driver = reinterpret_cast<CustomSX1262Wrapper&>(s_radio_driver_mem);

// Radio control functions used by SensorMesh.cpp
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
    radio_obj.setFrequency(freq);
    radio_obj.setBandwidth(bw);
    radio_obj.setSpreadingFactor(sf);
    radio_obj.setCodingRate(cr);
}

void radio_set_tx_power(int8_t dbm) {
    radio_obj.setOutputPower(dbm);
}

bool radio_init() { return true; }

uint32_t radio_get_rng_seed() {
    return radio_obj.random(0x7FFFFFFF);
}

mesh::LocalIdentity radio_new_identity() {
    RadioNoiseListener rng(radio_obj);
    return mesh::LocalIdentity(&rng);
}

// ---- Global pointer to command handlers (set during setup) ----
static std::vector<CommandHandler>* s_cmd_handlers = nullptr;

// ---- LED Sensor Mesh ----
class LedSensorMesh : public SensorMesh {
    ZephyrBoard* board_ref_;
    TimeSeriesData battery_data_;

public:
    LedSensorMesh(mesh::MainBoard& board, mesh::Radio& radio,
                  mesh::MillisecondClock& ms, mesh::RNG& rng,
                  mesh::RTCClock& rtc, mesh::MeshTables& tables,
                  ZephyrBoard* board_ref)
        : SensorMesh(board, radio, ms, rng, rtc, tables),
          board_ref_(board_ref),
          battery_data_(60, 60) {}

    void onSensorDataRead() override {
        float batt_voltage = getVoltage(1);
        battery_data_.recordData(getRTCClock(), batt_voltage);
    }

    int querySeriesData(uint32_t start_secs_ago, uint32_t end_secs_ago,
                        MinMaxAvg dest[], int max_num) override {
        if (max_num < 1) return 0;
        battery_data_.calcMinMaxAvg(getRTCClock(), start_secs_ago, end_secs_ago,
                                     &dest[0], 1, 0x74);  // channel=1, LPP_VOLTAGE
        return 1;
    }

    bool handleCustomCommand(uint32_t sender_timestamp, char* command, char* reply) override {
        ESP_LOGI(TAG, "CLI cmd: '%s'", command);
        // Dispatch to registered lambda handlers (from on_command YAML config)
        if (s_cmd_handlers) {
            std::string cmd(command);
            for (auto& h : *s_cmd_handlers) {
                if (cmd.rfind(h.prefix, 0) == 0) {  // starts_with
                    std::string resp = h.handler(cmd);
                    if (!resp.empty()) {
                        strncpy(reply, resp.c_str(), MAX_PACKET_PAYLOAD - 1);
                        reply[MAX_PACKET_PAYLOAD - 1] = '\0';
                    }
                    ESP_LOGI(TAG, "Command '%s' handled by lambda, reply: '%s'", command, reply);
                    return true;
                }
            }
        }
        return false;
    }
};

// ---- Static storage for mesh objects ----
static ZephyrHal* s_hal = nullptr;
static Module* s_module = nullptr;
static CustomSX1262* s_radio = nullptr;
static CustomSX1262Wrapper* s_radio_driver = nullptr;
static StaticPoolPacketManager* s_pkt_mgr = nullptr;
static SimpleMeshTables* s_tables = nullptr;
static LedSensorMesh* s_mesh = nullptr;
static Adafruit_LittleFS s_fs;

namespace meshcore_sensor {

// ---- Component Implementation ----

void MeshCoreSensorComponent::setup() {
    ESP_LOGI(TAG, "Setting up MeshCore Sensor...");
    ESP_LOGI(TAG, "  Node name: %s", node_name_.c_str());
    ESP_LOGI(TAG, "  Frequency: %.3f MHz", frequency_);
    ESP_LOGI(TAG, "  BW: %.1f kHz, SF: %d, CR: %d", bandwidth_, spreading_factor_, coding_rate_);
    ESP_LOGI(TAG, "  TX Power: %d dBm", tx_power_);
    ESP_LOGI(TAG, "  SPI pins: SCLK=%u MISO=%u MOSI=%u", lora_sclk_, lora_miso_, lora_mosi_);
    ESP_LOGI(TAG, "  Radio pins: CS=%u RST=%u BUSY=%u DIO1=%u RXEN=%u",
             lora_cs_, lora_reset_, lora_busy_, lora_dio1_, lora_rxen_);

    // Initialize board (using global 's_board_instance')
    s_board_instance.setLedPin(led_pin_);
    s_board_instance.begin();
    board_ = &s_board_instance;

    // Initialize clocks and RNG
    ms_clock_ = new ZephyrMillis();
    rtc_clock_ = new ZephyrRTCClock();
    rng_ = new ZephyrRNG();

    // Initialize RadioLib HAL
    hal_ = new ZephyrHal();
    hal_->configure(lora_sclk_, lora_miso_, lora_mosi_);
    hal_->init();

    // Create RadioLib Module and SX1262 radio
    s_hal = hal_;
    s_module = new Module(hal_, lora_cs_, lora_dio1_, lora_reset_, lora_busy_);
    // Placement-new radio into global storage
    s_radio = new (&radio_obj) CustomSX1262(s_module);

    // Pre-flight: manually call spiBegin and check BUSY pin
    ESP_LOGI(TAG, "Pre-flight: calling hal spiBegin()...");
    hal_->spiBegin();
    ESP_LOGI(TAG, "Pre-flight: spiBegin() done");

    // Check BUSY pin state (should be LOW when radio is idle)
    hal_->pinMode(lora_busy_, GPIO_INPUT);
    uint32_t busy_state = hal_->digitalRead(lora_busy_);
    ESP_LOGI(TAG, "Pre-flight: BUSY pin (GPIO %u) = %u (expect 0 for idle)", lora_busy_, busy_state);

    // Check CS pin
    hal_->pinMode(lora_cs_, GPIO_OUTPUT);
    hal_->digitalWrite(lora_cs_, 1);  // CS high (deselected)
    ESP_LOGI(TAG, "Pre-flight: CS pin (GPIO %u) set HIGH", lora_cs_);

    // Toggle reset to ensure clean radio state
    ESP_LOGI(TAG, "Pre-flight: resetting radio via RST pin (GPIO %u)...", lora_reset_);
    hal_->pinMode(lora_reset_, GPIO_OUTPUT);
    hal_->digitalWrite(lora_reset_, 0);  // RST low
    hal_->delay(10);
    hal_->digitalWrite(lora_reset_, 1);  // RST high
    hal_->delay(20);
    busy_state = hal_->digitalRead(lora_busy_);
    ESP_LOGI(TAG, "Pre-flight: after reset, BUSY = %u", busy_state);

    // Wait for BUSY to go low (radio ready)
    int wait_count = 0;
    while (hal_->digitalRead(lora_busy_) && wait_count < 100) {
        hal_->delay(1);
        wait_count++;
    }
    ESP_LOGI(TAG, "Pre-flight: BUSY settled after %d ms, state = %u", wait_count, hal_->digitalRead(lora_busy_));

    // Manual SPI test: read SX1262 register (GetStatus command)
    ESP_LOGI(TAG, "Pre-flight: manual SPI test...");
    hal_->digitalWrite(lora_cs_, 0);  // CS low
    uint8_t cmd_tx[2] = {0xC0, 0x00};  // GetStatus command
    uint8_t cmd_rx[2] = {0, 0};
    hal_->spiTransfer(cmd_tx, 2, cmd_rx);
    hal_->digitalWrite(lora_cs_, 1);  // CS high
    ESP_LOGI(TAG, "Pre-flight: GetStatus response: 0x%02X 0x%02X", cmd_rx[0], cmd_rx[1]);

    // Initialize SX1262 radio
    ESP_LOGI(TAG, "Initializing SX1262 radio...");

    float tcxo = 1.8f;
    int status = s_radio->begin(frequency_, bandwidth_, spreading_factor_, coding_rate_,
                                 RADIOLIB_SX126X_SYNC_WORD_PRIVATE, tx_power_, 16, tcxo);
    ESP_LOGI(TAG, "Radio begin() returned: %d", status);
    if (status == RADIOLIB_ERR_SPI_CMD_FAILED || status == RADIOLIB_ERR_SPI_CMD_INVALID) {
        ESP_LOGI(TAG, "SPI error, retrying without TCXO...");
        tcxo = 0.0f;
        status = s_radio->begin(frequency_, bandwidth_, spreading_factor_, coding_rate_,
                                 RADIOLIB_SX126X_SYNC_WORD_PRIVATE, tx_power_, 16, tcxo);
        ESP_LOGI(TAG, "Retry returned: %d", status);
    }

    if (status != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Radio init failed with code: %d", status);
        this->mark_failed();
        return;
    }

    s_radio->setCRC(1);
    s_radio->setCurrentLimit(140);
    s_radio->setDio2AsRfSwitch(true);
    s_radio->setRxBoostedGainMode(true);
    s_radio->setRfSwitchPins(lora_rxen_, RADIOLIB_NC);

    ESP_LOGI(TAG, "Radio initialized successfully");

    // Create radio driver wrapper (placement new into global storage)
    s_radio_driver = new (&radio_driver) CustomSX1262Wrapper(*s_radio, s_board_instance);

    // Create mesh infrastructure
    s_pkt_mgr = new StaticPoolPacketManager(32);  // 32 packet pool
    s_tables = new SimpleMeshTables();

    // Create the LED sensor mesh
    s_mesh = new LedSensorMesh(s_board_instance, *s_radio_driver, *ms_clock_, *rng_,
                                *rtc_clock_, *s_tables, &s_board_instance);

    // Set node preferences BEFORE begin() — begin() calls radio_set_params() using these
    NodePrefs* prefs = s_mesh->getNodePrefs();
    strncpy(prefs->node_name, node_name_.c_str(), sizeof(prefs->node_name) - 1);
    strncpy(prefs->password, password_.c_str(), sizeof(prefs->password) - 1);
    prefs->freq = frequency_;
    prefs->bw = bandwidth_;
    prefs->sf = spreading_factor_;
    prefs->cr = coding_rate_;
    prefs->tx_power_dbm = tx_power_;
    prefs->advert_interval = 1;  // 2 minutes
    prefs->flood_advert_interval = 1;  // 1 hour

    // Load or generate identity BEFORE begin() — begin() needs self_id for ACL
    {
        static const uint32_t IDENTITY_PREF_KEY = 0x4D435F49;  // "MC_I"
        auto pref = esphome::global_preferences->make_preference<uint8_t[PRV_KEY_SIZE + PUB_KEY_SIZE]>(IDENTITY_PREF_KEY);
        uint8_t key_data[PRV_KEY_SIZE + PUB_KEY_SIZE];

        if (pref.load(&key_data)) {
            s_mesh->self_id.readFrom(key_data, PRV_KEY_SIZE + PUB_KEY_SIZE);
            ESP_LOGI(TAG, "Loaded identity from flash");
        } else {
            ESP_LOGI(TAG, "No saved identity, generating new one...");
            RadioNoiseListener radio_rng(radio_obj);
            s_mesh->self_id = mesh::LocalIdentity(&radio_rng);

            size_t written = s_mesh->self_id.writeTo(key_data, sizeof(key_data));
            if (written > 0) {
                pref.save(&key_data);
                esphome::global_preferences->sync();
                ESP_LOGI(TAG, "Identity saved to flash (%u bytes)", (unsigned)written);
            }
        }

        // Log public key
        char pub_hex[PUB_KEY_SIZE * 2 + 1];
        for (int i = 0; i < PUB_KEY_SIZE; i++) {
            sprintf(&pub_hex[i * 2], "%02x", s_mesh->self_id.pub_key[i]);
        }
        pub_hex[PUB_KEY_SIZE * 2] = '\0';
        ESP_LOGI(TAG, "Public key: %s", pub_hex);
    }

    // Now initialize mesh (uses prefs for radio params, self_id for ACL)
    ESP_LOGI(TAG, "Filesystem begin...");
    s_fs.begin();
    ESP_LOGI(TAG, "Mesh begin (loads prefs + ACL from flash)...");
    s_mesh->begin(&s_fs);
    ESP_LOGI(TAG, "Mesh begin done");

    // Register command handlers for dispatch
    s_cmd_handlers = &command_handlers_;

    radio_ok_ = true;
    ESP_LOGI(TAG, "MeshCore Sensor ready! Node: %s", node_name_.c_str());

    // Send initial advertisement
    s_mesh->sendSelfAdvertisement(1000, true);
}

void MeshCoreSensorComponent::loop() {
    // Check if DFU magic was written (1200 baud touch from ESPHome CDC callback)
    // Skip on first loop iteration to avoid reboot-loop from stale magic after DFU flash
    static uint32_t loop_count = 0;
    if (loop_count > 10) {
        volatile uint32_t *dbl_reset_mem = (volatile uint32_t *) 0x20007F7C;
        if (*dbl_reset_mem == 0x5A1AD5) {
            ESP_LOGI(TAG, "DFU requested, rebooting into bootloader...");
            k_msleep(100);
            sys_reboot(SYS_REBOOT_WARM);
        }
    }
    loop_count++;

    if (!radio_ok_ || !s_mesh) return;

    // Tick the RTC
    rtc_clock_->tick();

    // Run the mesh loop (handles radio RX/TX, advertisements, etc.)
    s_mesh->loop();
}

void MeshCoreSensorComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "MeshCore Sensor:");
    ESP_LOGCONFIG(TAG, "  Node Name: %s", node_name_.c_str());
    ESP_LOGCONFIG(TAG, "  LED Pin: %u (P%u.%02u)", led_pin_, led_pin_ / 32, led_pin_ % 32);
    ESP_LOGCONFIG(TAG, "  LoRa CS: %u, RST: %u, BUSY: %u, DIO1: %u",
                  lora_cs_, lora_reset_, lora_busy_, lora_dio1_);
    ESP_LOGCONFIG(TAG, "  Radio: %.3f MHz, BW %.1f kHz, SF %d, CR %d, TX %d dBm",
                  frequency_, bandwidth_, spreading_factor_, coding_rate_, tx_power_);
    ESP_LOGCONFIG(TAG, "  Status: %s", radio_ok_ ? "OK" : "FAILED");
}

void MeshCoreSensorComponent::set_led(bool on) {
    s_board_instance.setLed(on);
}

bool MeshCoreSensorComponent::get_led() const {
    return s_board_instance.getLed();
}

}  // namespace meshcore_sensor
