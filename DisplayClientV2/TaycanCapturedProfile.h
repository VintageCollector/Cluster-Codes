#pragma once
#include <Arduino.h>
// Research snapshot: dalathegreat/EV-CANlogs, Porsche Taycan Turbo S,
// reviewed 2026-11-01. Payloads are evidence, not a decoded ignition state machine.
#ifndef TAYCAN_CAPTURED_IGNITION_PROFILE
#define TAYCAN_CAPTURED_IGNITION_PROFILE 1
#endif
#ifndef TAYCAN_CAPTURED_ESP21_PROFILE
#define TAYCAN_CAPTURED_ESP21_PROFILE 0
#endif
#ifndef TAYCAN_BENCH_ALWAYS_ON
#define TAYCAN_BENCH_ALWAYS_ON 1
#endif
#ifndef TAYCAN_PANAMERA_971_WAKE_PROFILE
#define TAYCAN_PANAMERA_971_WAKE_PROFILE 0
#endif
namespace Taycan {
const bool PANAMERA_971_WAKE=TAYCAN_BENCH_ALWAYS_ON && TAYCAN_PANAMERA_971_WAKE_PROFILE;
// Continuous ignition status used by the working isolated static sketch.
const uint8_t BENCH_IGNITION_STATUS=0x23;
// 00/21/23 were captured with valid KlemmenStatus01 CRCs. Their assignment
// to SimHub OFF/ACC/ON is a bench hypothesis. START holds ON: no unverified
// claim that captured 2B is a starter command. No 29 OFF burst is invented.
inline uint8_t ignitionStatusByte(uint8_t key) {
  if(!TAYCAN_CAPTURED_IGNITION_PROFILE)return key>=2?0x03:0x01;
  return key>=2?0x23:(key==1?0x21:0x00);
}
// Optional E0/3F framing matches every captured Taycan FD frame. Default is
// legacy D0/1F to preserve the user's working speed/status behavior.
inline uint8_t esp21CounterHeader(uint8_t sequence) {
  return (TAYCAN_CAPTURED_ESP21_PROFILE?0xE0:0xD0)|(sequence&0x0F);
}
inline uint8_t esp21StatusTemplate() {
  return TAYCAN_CAPTURED_ESP21_PROFILE?0x3F:0x1F;
}
}
