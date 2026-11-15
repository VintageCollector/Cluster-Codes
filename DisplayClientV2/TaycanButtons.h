#pragma once
#include <Arduino.h>
namespace Taycan {
// User-selected Macan MFD mapping; momentary switches connect each pin to GND.
// User corrected the working MFD ID to 0x5BF. Retain existing 970 wiring.
#ifndef TAYCAN_ENABLE_MFD_BUTTONS
#define TAYCAN_ENABLE_MFD_BUTTONS 1
#endif
const bool ENABLE_MFD_BUTTONS=TAYCAN_ENABLE_MFD_BUTTONS!=0;
const uint8_t MFD_PINS[4]={5,6,4,7}; // OK, Up, Down, Back: existing Panamera 970 wiring.
const uint16_t MFD_ID=0x5BF;
const uint8_t MFD_TRAILER=0x21; // User's working 971 test sketch.
const uint16_t MFD_DEBOUNCE_MS=25;
const uint16_t MFD_PULSE_MS=10; // Supplied VWMQBCluster press-to-neutral delay.
const uint16_t MFD_FIRST_REPEAT_MS=350;
const uint16_t MFD_REPEAT_MS=120;
const uint16_t MFD_IDLE_MS=50;
class MfdButtons {
  bool raw[4]={},stable[4]={};
  uint32_t changed[4]={};
  uint8_t held=4, pending=4;
  bool needsRelease=false, hasSentPress=false;
  uint32_t pressAt=0;
  uint32_t lastNeutral=0;
  uint16_t repeatDelay=MFD_FIRST_REPEAT_MS;
 public:
  void setup(){if(ENABLE_MFD_BUTTONS)for(uint8_t i=0;i<4;++i)pinMode(MFD_PINS[i],INPUT_PULLUP);}
  void poll(uint32_t now){
    if(!ENABLE_MFD_BUTTONS)return;
    for(uint8_t i=0;i<4;++i){
      bool pressed=digitalRead(MFD_PINS[i])==LOW;
      if(pressed!=raw[i]){raw[i]=pressed;changed[i]=now;}
      if((uint32_t)(now-changed[i])>=MFD_DEBOUNCE_MS)stable[i]=raw[i];
    }
    uint8_t selected=4;
    for(uint8_t i=0;i<4;++i) {
      if(stable[i]) { selected=i; break; }
    }
    if(selected!=held) {
      held=selected;
      if(held<4) pending=held;
      hasSentPress=false;
      repeatDelay=MFD_FIRST_REPEAT_MS;
    }
  }
  bool frame(uint32_t now,uint8_t* b) const {
    const uint32_t elapsed=now-pressAt;
    if(needsRelease && elapsed<MFD_PULSE_MS) return false;
    const bool repeat=hasSentPress && (held==1 || held==2) && elapsed>=repeatDelay;
    b[0]=b[1]=b[2]=0;b[3]=MFD_TRAILER;
    if(needsRelease) return true;
    if(pending==4 && !repeat)
      return held==4 && (uint32_t)(now-lastNeutral)>=MFD_IDLE_MS;
    const uint8_t button=pending<4?pending:held;
    const uint8_t codes[]={0x09,0x06,0x07,0x08};
    b[0]=codes[button]; b[2]=button==2?0x0F:0x01;
    return true;
  }
  void queued(uint32_t now, const uint8_t* b) {
    if(b[0]==0) { needsRelease=false; lastNeutral=now; return; }
    const uint8_t button=pending<4?pending:held;
    repeatDelay=hasSentPress?MFD_REPEAT_MS:MFD_FIRST_REPEAT_MS;
    hasSentPress=button==held;
    pending=4;
    needsRelease=true;
    pressAt=now;
  }
};
}
