#pragma once
// Copy this AND LexusController.h, LexusCore.h, LexusConfig.h alongside the
// SimHub-generated DisplayClientV2.ino. Enable INCLUDE_CUSTOM_PROTOCOL there.
#include "LexusController.h"
#include <stdio.h>
class SHCustomProtocol {
 LexusController cluster;
 uint32_t lastDebug=0;
public:
 void setup() { cluster.setup(); }
 void read() {
  String message=FlowSerialReadStringUntil('\n');
  if(message.length()>=192) { ++cluster.rejected; return; }
  char text[192]; message.toCharArray(text,sizeof(text)); cluster.accept(text);
 }
 void loop() {
  cluster.loop();
  uint32_t now=millis();
  if(LEXUS_DEBUG_INTERVAL_MS && (uint32_t)(now-lastDebug)>=LEXUS_DEBUG_INTERVAL_MS) {
   lastDebug=now;char message[160];
   snprintf(message,sizeof(message),"Lexus rx=%lu bad=%lu ready=%u cmd622=%02X cmd0B4=%02X%02X cmd399=%02X cmd620B6B8=%02X/%02X txErr=%lu",
    (unsigned long)cluster.accepted,(unsigned long)cluster.rejected,(unsigned)cluster.canIsReady(),
    (unsigned)cluster.lightCommand,(unsigned)cluster.speedHighCommand,(unsigned)cluster.speedLowCommand,(unsigned)cluster.cruiseCommand,(unsigned)cluster.warningByte6,(unsigned)cluster.warningByte8,(unsigned long)cluster.txErrors);
   FlowSerialDebugPrintLn(message);
  }
 }
 void idle() {} // no CAN work in SimHub's byte-reading critical path
};
