#ifndef BMW_CONFIG_H
#define BMW_CONFIG_H
#include <stdint.h>
// Bench defaults from the supplied Goofy/Alberto G30 examples.
namespace BMWConfig {
 const unsigned CS_PIN=9;
 const bool ENABLE_OIL_LOGIC_OUTPUT=true;
 const unsigned OIL_LOGIC_PIN=40; // Logic output only; no BMW cluster contact pin is assumed.
 const bool OIL_ACTIVE_HIGH=true;
 const unsigned BC_BUTTON_PIN=3; // Momentary switch to GND, internal pull-up.
 const unsigned TIMEOUT_MS=1500;
 const bool ENABLE_SOURCE_KEEPALIVES=true;
 // 0 supplied 64.01/kmh speed + G30 rpm/10 packing; 1 optional older Goofy RPM packing.
 const bool LEGACY_GOOFY_RPM=false;
 const bool F30_GEAR_LAYOUT=false; // false: G30 DLC5/XOR D6; true: DLC8/init0/XOR70.
 // No reliable active/set-speed fields have been established for 0x289.
 const bool SEND_CRUISE_TEST_BASELINE=false;
 // User-authorized bench candidates; unverified G30 state/speed/lane bits.
 const bool ENABLE_EXPERIMENTAL_CRUISE=true;
 const uint8_t CRUISE_OFF_STATE=0xE0,CRUISE_READY_STATE=0xE1,CRUISE_ACTIVE_STATE=0xE3;
 const unsigned CRUISE_COUNTS_PER_KMH=1;
 const bool ENABLE_EXPERIMENTAL_LANE=true;
 const bool REAR_FOG_WITH_FRONT=true; // Bench test both documented front/rear fog bits.
 // No generic random traffic, bus sweeps or diagnostic programming in this sketch.
}
#endif
