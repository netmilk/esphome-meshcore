// target.h stub for MeshCore ESPHome component
#ifndef MESHCORE_TARGET_H_STUB
#define MESHCORE_TARGET_H_STUB

#include <MeshCore.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

// board is declared as MainBoard& - actual type is ZephyrBoard (defined in meshcore_sensor.cpp)
extern mesh::MainBoard& board;
extern CustomSX1262Wrapper& radio_driver;
extern EnvironmentSensorManager sensors;

// Radio control functions
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
bool radio_init();
uint32_t radio_get_rng_seed();
mesh::LocalIdentity radio_new_identity();

#endif
