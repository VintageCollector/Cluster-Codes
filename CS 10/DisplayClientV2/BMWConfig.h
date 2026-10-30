#ifndef BMW_CONFIG_H
#define BMW_CONFIG_H
#include <stdint.h>
// Bench defaults from the supplied Goofy/Alberto G30 examples.
namespace BMWConfig {
 // User gauge-range target; hot raw endpoint C0 confirmed on the physical cluster.
 const int COOLANT_GAUGE_COLD_C=50,COOLANT_GAUGE_HOT_C=130;
 const uint8_t COOLANT_GAUGE_COLD_RAW=0x32,COOLANT_GAUGE_HOT_RAW=0xC0;

#ifndef BMW_SOURCE_MODE_LAYOUT
#define BMW_SOURCE_MODE_LAYOUT 1
#endif
 const bool SOURCE_MODE_LAYOUT=BMW_SOURCE_MODE_LAYOUT!=0; // supplied header: CF C1/C2/C7, etc.

 const unsigned CS_PIN=10;
#ifndef BMW_LEGACY_TERMINAL
#define BMW_LEGACY_TERMINAL 0
#endif
#ifndef BMW_LEGACY_WAKE
#define BMW_LEGACY_WAKE 0
#endif
 const bool USE_V1_2_TERMINAL_PROFILE=BMW_LEGACY_TERMINAL!=0;
 const bool USE_V1_2_WAKE_PROFILE=BMW_LEGACY_WAKE!=0;
 const bool ENABLE_OIL_LOGIC_OUTPUT=true;
 const unsigned OIL_LOGIC_PIN=40; // Logic output only; no BMW cluster contact pin is assumed.
 const bool OIL_ACTIVE_HIGH=true;
 const unsigned BC_BUTTON_PIN=3; // Momentary switch to GND, internal pull-up.
 // Separate input pins. BC is the only established cluster command.
 const uint8_t MENU_BUTTON_PINS[7]={4,5,6,7,8,22,23}; // Up, Down, Left, Right, OK, Back, Trip; Mega digital pins
 const bool LOG_UNMAPPED_BUTTONS=true;
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
 // X4M/G02 bench candidates from supplied sketch; G30 acceptance unverified.
 const bool BC_X4M_LAYOUT=false; // true: 40 FF / 00 FF; false preserves working 4C FC / 00 FC.
 const bool ENABLE_EXPERIMENTAL_ECO_NAV=true;
 const uint8_t ECO_NAV_TEST_CODE=7; // 7 off; explicitly set 30 Z-turn, 80 Y-turn, 130 roundabout to bench-test.
 const unsigned ECO_NAV_TEST_DISTANCE_METERS=200;
 // Optional manual cruise/assistance CHECK-CONTROL test, not an active-cruise emulator.
#ifndef BMW_CC_PROBE_ENABLED
#define BMW_CC_PROBE_ENABLED 0
#endif
 const bool ENABLE_CC_PROBE=BMW_CC_PROBE_ENABLED!=0; // Set true for D4 hold / D5 next / D6 previous bench test.
 const uint8_t CC_PROBE_SET=0x31,CC_PROBE_CLEAR=0x30; // Alternate pair: 29/28.
 const uint8_t CC_PROBE_COUNT=13;
 const uint16_t CC_PROBE_CODES[CC_PROBE_COUNT]={446,1,2,3,26,85,176,177,199,337,441,435,78};
 // Colored shift-light arc around gear: source G02 uses byte0=7, remaining bytes0.
 const bool ENABLE_GEAR_RING=true;
 const uint8_t GEAR_RING_REQUEST_FLAG=0x00; // 0x10 caused persistent blinking on this cluster; use segment level only.
 const int8_t GEAR_RING_TEST_LEVEL=-1; // -1 live RPM; 8 requests full arc while ignition on, for isolation.
 // Level0..8 now comes from RPM-scaled JavaScript field60; cluster chooses colors.
 // Public G30 FLA baseline only; icon activation bits remain unverified.
 const bool ENABLE_EXPERIMENTAL_FLA_BASELINE=true;
 // G02 source uses0AB; older Alberto sketch uses19B. Send one selected layout.
 const uint16_t RESTRAINT_STATUS_ID=0x0AB;
 const bool ENABLE_G02_SEATBELT_MESSAGE=true;
 // Bench-only warning policy requested by user; this also disables game airbag CC97.
 const bool SUPPRESS_BENCH_RESTRAINT_WARNING=true;
 const bool CLEAR_BENCH_EMERGENCY_CALL_WARNINGS=true;
 const bool ENABLE_SPEED_WARNING=true;
 const uint16_t SPEED_WARNING_CC_ID=78; // Supplied G02 notes: yellow speed warning;62 is red alternative.
 const unsigned SPEED_WARNING_MARGIN_KMH=0;
 // No generic random traffic, bus sweeps or diagnostic programming in this sketch.
}
#endif

