#pragma once
// User a8.ino hardware: Cory Fowler MCP_CAN API, CS10, 16MHz, 500kbps.
#define AUDI_CS 10
#define AUDI_CLOCK MCP_16MHZ
#define AUDI_BITRATE CAN_500KBPS
#define AUDI_TIMEOUT_MS 1500UL
#define AUDI_DEBUG_MS 2000UL
// V2 source/trace-backed candidates. User reports RPM/parking working on v1; others need bench verification.
#define AUDI_ENABLE_RPM 1
#define AUDI_ENABLE_SPEED 1
#define AUDI_ENABLE_TEMPERATURE 1
#define AUDI_ENABLE_CEL 1
#define AUDI_ENABLE_CRUISE 1
#define AUDI_ENABLE_LIGHTS 1
#define AUDI_ENABLE_WARNINGS 1
#define AUDI_ENABLE_TURNS 1
#define AUDI_ENABLE_AIRBAG_OFF 1 // sourced0x040 lamp/error/belt requests OFF; no DTC clear
// Source sketch highbeam 0x662 B3=04. Keep this distinct from Gateway_11.
#define AUDI_ENABLE_SOURCE_HIGHBEAM 1
// Front fog -> all-weather request is NOT a confirmed front fog indicator.
#define AUDI_FRONT_FOG_AS_ALLWEATHER 0
// Menu payload candidates now supplied in user SHCustomProtocol.h.
// Right was commented in that source; all still need physical A8 confirmation.
#ifndef AUDI_ENABLE_MENU
#define AUDI_ENABLE_MENU 1
#endif
#define AUDI_MENU_PRESS_MS 120UL
#define AUDI_MENU_RELEASE_MS 120UL

// V2: source-header and A6-trace profile. All still need A8 bench validation.
#define AUDI_SPEED_308_RAW_PER_KMH 64.0f
#define AUDI_ENABLE_GATEWAY_LIGHTS 0
#define AUDI_ENABLE_WAKE 1
#define AUDI_ENABLE_DIMMING 1
#define AUDI_ENABLE_TPMS_SAMPLE 1
#define AUDI_ENABLE_ESP02 1
#define AUDI_ENABLE_GEAR 1
#ifndef AUDI_ENABLE_PHYSICAL_MENU
#define AUDI_ENABLE_PHYSICAL_MENU 1
#endif
// Buttons connect these Arduino inputs to Arduino GND, never cluster12V pins.
#define AUDI_BUTTON_UP 5
#define AUDI_BUTTON_DOWN 6
#define AUDI_BUTTON_LEFT 4
#define AUDI_BUTTON_RIGHT 7
#define AUDI_BUTTON_OK 3

#define AUDI_ENABLE_DISTANCE_DISPLAY 1
#define AUDI_ENABLE_SUSPENSION_OFF_REQUESTS 1 // source candidates; physical clear unverified
