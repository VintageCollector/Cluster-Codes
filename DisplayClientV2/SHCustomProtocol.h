#pragma once
#include "AudiController.h"
#include <stdio.h>
#ifdef __AVR__
#define AUDI_DEBUG_FORMAT(s) PSTR(s)
#define AUDI_DEBUG_SNPRINTF snprintf_P
#else
#define AUDI_DEBUG_FORMAT(s) s
#define AUDI_DEBUG_SNPRINTF snprintf
#endif
class SHCustomProtocol {
 AudiController cluster;uint32_t lastDebug=0;
public:
 void setup(){cluster.setup();}
 void read(){String line=FlowSerialReadStringUntil('\n');if(line.length()>=192){++cluster.rejected;return;}char text[192];line.toCharArray(text,sizeof(text));cluster.accept(text);}
 void loop(){cluster.loop();uint32_t now=millis();if(AUDI_DEBUG_MS&&(uint32_t)(now-lastDebug)>=AUDI_DEBUG_MS){lastDebug=now;char text[192];
 AUDI_DEBUG_SNPRINTF(text,sizeof(text),AUDI_DEBUG_FORMAT("Audi rx=%lu bad=%lu ready=%u live=%u txErr=%lu menu=%u unmappedMenu=%lu cmd308=%04X/%02X/%02X cmd662=%02X/%02X temp640=%02X cel440=%02X dist324=%02X susp325=%02X"),
 (unsigned long)cluster.accepted,(unsigned long)cluster.rejected,(unsigned)cluster.canIsReady(),(unsigned)cluster.live,
 (unsigned long)cluster.txErrors,(unsigned)cluster.requestedMenu,(unsigned long)cluster.unsupportedMenu,(unsigned)cluster.cmdSpeed,(unsigned)cluster.cmdWarn0,(unsigned)cluster.cmdWarn1,(unsigned)cluster.cmdLow,(unsigned)cluster.cmdHigh,(unsigned)cluster.cmdTemp,(unsigned)cluster.cmdCEL,(unsigned)cluster.cmdDistance,(unsigned)cluster.cmdSuspension);FlowSerialDebugPrintLn(text);}}
 void idle(){}
};
