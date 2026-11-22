#pragma once
#include <Arduino.h>
#include "BenchMenuButtons.h"

namespace Taycan {
#ifndef TAYCAN_ENABLE_MFD_BUTTONS
#define TAYCAN_ENABLE_MFD_BUTTONS 1
#endif
const bool ENABLE_MFD_BUTTONS = TAYCAN_ENABLE_MFD_BUTTONS != 0;
const uint16_t MFD_ID = 0x5BF;

// Same pins, key codes and per-button trailers as the static bench tester.
class MfdButtons {
  BenchMenuButtons physical;
 public:
  void setup() {
    if (ENABLE_MFD_BUTTONS) physical.setup();
  }
  void poll(uint32_t now) {
    if (ENABLE_MFD_BUTTONS) physical.poll(now);
  }
  bool frame(uint32_t now, uint8_t* bytes) const {
    if (!ENABLE_MFD_BUTTONS) return false;
    byte frameBytes[4];
    if (!physical.frame(now, frameBytes)) return false;
    memcpy(bytes, frameBytes, sizeof(frameBytes));
    return true;
  }
  void queued(uint32_t now, const uint8_t* bytes) {
    byte frameBytes[4];
    memcpy(frameBytes, bytes, sizeof(frameBytes));
    physical.sent(now, frameBytes);
  }
};
}
