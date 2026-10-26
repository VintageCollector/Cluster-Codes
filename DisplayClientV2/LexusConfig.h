#pragma once
// MCP_CAN (Cory Fowler API), matching the uploaded generator.
#define LEXUS_CS 10
#define LEXUS_CLOCK MCP_16MHZ
#define LEXUS_BITRATE CAN_500KBPS
// Left/right: maintained ground. Hazard: momentary grounding toggles ON/OFF.
#ifndef LEXUS_RELAYS_ENABLED
#define LEXUS_RELAYS_ENABLED 1
#endif
#ifndef LEXUS_RELAY_ACTIVE_LOW
#define LEXUS_RELAY_ACTIVE_LOW 1
#endif
#define LEXUS_RELAY_LEFT 4
#define LEXUS_RELAY_RIGHT 5
#define LEXUS_RELAY_HAZARD 6
#define LEXUS_PULSE_MS 100
#define LEXUS_RELAY_SETTLE_MS 150
// Hazard pulses on BOTH desired on and desired off transitions.
// Left/right contacts remain closed while their steady selection is active.
// Opt-in: source-comment mappings, not independently confirmed in this session.
#define LEXUS_SOURCE_CRUISE 1 // documented 0x399 enable/SET, enabled for bench validation
#define LEXUS_SOURCE_ABS_ESC 1 // source masks enabled for requested ABS/ESC bench testing
#define LEXUS_ABS_BLINK 1 // display effect during actual ABS activity, not a new CAN bit
#define LEXUS_ABS_BLINK_HALF_MS 250UL
#define LEXUS_ACTIVITY_HOLD_MS 350UL
#define LEXUS_SOURCE_CEL_ECO 1 // requested ECO indicator in live Comfort mode
#define LEXUS_SOURCE_SPORT 1 // only transmit known SPORT+ sample in Sport/Sport+
// Latest user request: 0x620 Byte6 hard-locked00; no door/airbag overrides.
#define LEXUS_ALLOW_SEATBELT_LAMP 0 // also keep the known Byte8 seatbelt40 mask clear
#define LEXUS_LDA_BEEP 0
#define LEXUS_LDA_BEEP_MASK 0x00 // position known, exact mask still needed
// Enabled provisional curve through user observations; above42kmh extrapolated.
#define LEXUS_SPEED_ENABLED 1
#define LEXUS_SPEED_CURVE 1
#define LEXUS_SPEED_MSB_INDEX 5
#define LEXUS_SPEED_LSB_INDEX 6
// 0 = exact user-observed low-speed lookup; other speeds remain unsupported.
// 1 = linear 16-bit encoder ONLY after measuring its full-range scale.
#define LEXUS_SPEED_LINEAR 0
#define LEXUS_SPEED_UNITS_PER_KMH 0.0f
#define LEXUS_DEBUG_INTERVAL_MS 2000UL // SimHub log: inputs/command bytes/TX errors
#define LEXUS_GEAR_ENABLED 1
// User-confirmed 0x3BC: B2 mode, B5 gear number, B6 plain Drive.
// Input -2=P,-1=R,0=N,1..18=Drive (Comfort plain D; Sport/+ numbered, cap8).
// 100=plain D without known gear number,101..108=explicit M1..M8.
#define LEXUS_COMFORT_MODE_BYTE 0x00 // latest user table explicitly supplies clear
#define LEXUS_SPORT_MODE_BYTE 0x07
// Confirmed display anchors. FIXED values, not game temperature conversion.
#define LEXUS_OUTSIDE_SAMPLE_ENABLED 1
#define LEXUS_OUTSIDE_FIXED_BYTE 0x3F // outside 59F / 15C (user observation)
#define LEXUS_ENGINE_TEMP_ENABLED 1
#define LEXUS_ENGINE_TEMP_FIXED_BYTE 0x99 // halfway gauge, NOT a known C/F value
// User-observed 0x3BB Byte 3 gauge anchors; no physical temperature scale implied.
#define LEXUS_ENGINE_TEMP_RED_ZONE_BYTE 0xF0 // red/overheat zone begins
#define LEXUS_ENGINE_TEMP_MAX_BYTE 0xFF // gauge maximum
// 0x3BB also carries ignition and battery status: bench holds ignition ON.
#define LEXUS_IGNITION_FIXED_BYTE 0x10
// Parking sensor payload remains an unverified optional sample.
#define LEXUS_PARK_SAMPLE_ENABLED 0
// User-confirmed 0x394 steering lamp baseline: B2=00 OFF,10 ON.
#define LEXUS_EPS_FRAME_ENABLED 1
#define LEXUS_EPS_WARNING_LAMP_ON 0
