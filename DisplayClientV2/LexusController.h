#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include "LexusCore.h"
class LexusController {
 MCP_CAN bus{LEXUS_CS};
 Lexus::State latest;
 Lexus::Filter filter;
 uint32_t lastData=0, due[Lexus::FRAME_COUNT]={}, retry=0;
 bool have=false,ready=false; uint8_t cursor=0;
 void initializeCan() {
  ready=bus.begin(MCP_ANY,LEXUS_BITRATE,LEXUS_CLOCK)==CAN_OK;
  if(ready)ready=bus.setMode(MCP_NORMAL)==CAN_OK;
 }
 struct Relay { bool known=false, state=false, pending=false, target=false, active=false;
  uint32_t end=0, changed=0; } relay[3];
 void pin(uint8_t i,bool on) {
  const uint8_t pins[]={LEXUS_RELAY_LEFT,LEXUS_RELAY_RIGHT,LEXUS_RELAY_HAZARD};
  digitalWrite(pins[i],(on ^ !!LEXUS_RELAY_ACTIVE_LOW)?HIGH:LOW);
 }
 void relays(const Lexus::State& s,bool fresh,uint32_t now) {
  if(!LEXUS_RELAYS_ENABLED) return;
  // Hazard takes exclusive priority over left/right.
  bool want[]={fresh && !s.v[Lexus::HAZARD] && !!s.v[Lexus::LEFT],
               fresh && !s.v[Lexus::HAZARD] && !!s.v[Lexus::RIGHT],
               fresh && !!s.v[Lexus::HAZARD]};
  if(want[0] && want[1]) want[0]=want[1]=false;
  for(uint8_t i=0;i<3;++i) {
   Relay& r=relay[i];
   // An orderly telemetry stop requests OFF using another pulse, if state is known.
   // At startup state is unknown: release contacts without blind toggle pulses.
   if(!fresh && !r.known) { pin(i,false); continue; }
   if(!r.known) { r.known=true; r.state=false; } // requires initial physical OFF
   // Left/right require continuous grounding; only hazard is a toggle.
   if(i<2) { pin(i,want[i]); r.state=want[i]; continue; }
   if(r.active && (int32_t)(now-r.end)>=0) { pin(i,false); r.active=false; }
   if(want[i]!=r.target) { r.target=want[i]; r.changed=now; r.pending=true; }
   if(r.pending && !r.active && (uint32_t)(now-r.changed)>=LEXUS_RELAY_SETTLE_MS) {
    r.pending=false;
    if(r.state!=r.target) { r.state=r.target; pin(i,true); r.active=true; r.end=now+LEXUS_PULSE_MS; }
   }
  }
 }
public:
 uint32_t txErrors=0, rejected=0, accepted=0;
 uint8_t lightCommand=0,speedHighCommand=0,speedLowCommand=0,cruiseCommand=0,warningByte6=0,warningByte8=0x80;
 bool canIsReady() const { return ready; }
 void setup() {
  // AVR hardware SS must stay output even when CS is 9.
  pinMode(SS,OUTPUT); digitalWrite(SS,HIGH);
  if(LEXUS_RELAYS_ENABLED) {
   const uint8_t pins[]={LEXUS_RELAY_LEFT,LEXUS_RELAY_RIGHT,LEXUS_RELAY_HAZARD};
   for(uint8_t i=0;i<3;++i) { pin(i,false); pinMode(pins[i],OUTPUT); }
  }
  initializeCan();
 }
 bool accept(char* line) {
  Lexus::State next;
  if(!Lexus::parse(line,next)) { ++rejected; return false; }
  latest=next; lastData=millis(); have=true; ++accepted; return true;
 }
 void loop() {
  uint32_t now=millis();
  bool fresh=have && (uint32_t)(now-lastData)<=1500 && latest.v[Lexus::RUN];
  if(fresh) filter.update(latest,now); else filter.reset();
  relays(filter.stable,fresh,now);
  if(!ready) { if((uint32_t)(now-retry)>=2000) { retry=now; initializeCan(); } return; }
  if(!have) return; // no unsolicited warning traffic at startup
  Lexus::Frame frames[Lexus::FRAME_COUNT]; Lexus::build(filter.stable,frames,now);
  lightCommand=frames[Lexus::LIGHT_F].data[3];
  cruiseCommand=frames[Lexus::CRUISE_F].data[1];
  warningByte6=frames[Lexus::WARN_F].data[5];warningByte8=frames[Lexus::WARN_F].data[7];
  speedHighCommand=frames[Lexus::SPEED_F].data[LEXUS_SPEED_MSB_INDEX];
  speedLowCommand=frames[Lexus::SPEED_F].data[LEXUS_SPEED_LSB_INDEX];
  // At most one driver send per call; no catch-up bursts after a stall.
  for(uint8_t n=0;n<Lexus::FRAME_COUNT;++n) {
   uint8_t i=cursor; cursor=(cursor+1)%Lexus::FRAME_COUNT;
   if(!frames[i].enabled || (uint32_t)(now-due[i])<frames[i].period) continue;
   due[i]=now;
   if(bus.sendMsgBuf(frames[i].id,0,8,frames[i].data)!=CAN_OK) ++txErrors;
   break;
  }
 }
};
