#pragma once

#include <Arduino.h>

struct BenchMenuButton {
  byte pin;
  byte key;
  byte event;
  byte trailer;
  bool repeat;
};

// Wire normally open buttons from each pin to GND.
static const BenchMenuButton benchMenuButtons[] = {
  {3, 0x07, 0x01, 0x2F, false},  // OK
  {4, 0x06, 0x0F, 0x2F, true},   // Scroll down
  {5, 0x06, 0x01, 0x2F, true},   // Scroll up
  {6, 0x08, 0x01, 0x2F, false},  // Back
  {7, 0x27, 0x01, 0x21, false},  // Yellow ring selection, exact bench-confirmed payload.
  {8, 0x09, 0x01, 0x2F, false},  // Main menu
  {22, 0x01, 0x01, 0x2F, false}, // Context menu
};

class BenchMenuButtons {
  static const byte buttonCount = sizeof(benchMenuButtons) / sizeof(benchMenuButtons[0]);
  bool raw[buttonCount] = {};
  bool stable[buttonCount] = {};
  uint32_t changedAt[buttonCount] = {};
  byte held = buttonCount;
  byte pending = buttonCount;
  bool needsRelease = false;
  bool hasSentPress = false;
  byte lastTrailer = 0x2F;
  uint32_t pressedAt = 0;
  uint32_t lastNeutral = 0;
  uint16_t repeatDelay = 350;

public:
  void setup() {
    for (const auto& button : benchMenuButtons) pinMode(button.pin, INPUT_PULLUP);
  }

  void poll(uint32_t now) {
    byte selected = buttonCount;
    for (byte i = 0; i < buttonCount; ++i) {
      const bool pressed = digitalRead(benchMenuButtons[i].pin) == LOW;
      if (pressed != raw[i]) {
        raw[i] = pressed;
        changedAt[i] = now;
      }
      if (now - changedAt[i] >= 25) stable[i] = pressed;
      if (stable[i] && selected == buttonCount) selected = i;
    }
    if (selected != held) {
      held = selected;
      if (held < buttonCount) pending = held;
      hasSentPress = false;
      repeatDelay = 350;
    }
  }

  bool frame(uint32_t now, byte (&data)[4]) const {
    data[0] = data[1] = data[2] = 0;
    data[3] = lastTrailer;
    const uint32_t elapsed = now - pressedAt;
    if (needsRelease) return elapsed >= 10;
    const bool repeat = hasSentPress && held < buttonCount &&
                        benchMenuButtons[held].repeat && elapsed >= repeatDelay;
    if (pending == buttonCount && !repeat) {
      return held == buttonCount && now - lastNeutral >= 50;
    }
    const auto& button = benchMenuButtons[pending < buttonCount ? pending : held];
    data[0] = button.key;
    data[2] = button.event;
    data[3] = button.trailer;
    return true;
  }

  void sent(uint32_t now, const byte (&data)[4]) {
    if (data[0] == 0) {
      needsRelease = false;
      lastNeutral = now;
      return;
    }
    repeatDelay = hasSentPress ? 120 : 350;
    hasSentPress = held < buttonCount && (pending == held || pending == buttonCount);
    pending = buttonCount;
    needsRelease = true;
    lastTrailer = data[3];
    pressedAt = now;
  }
};
