#ifndef SH_BMW_G30_CUSTOM_PROTOCOL_H
#define SH_BMW_G30_CUSTOM_PROTOCOL_H
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515_can.h>
#include "BMWProtocol.h"
#include "BMWButtons.h"
class SHCustomProtocol {
 mcp2515_can canBus{BMWConfig::CS_PIN};
 BMW::State state;BMW::LaunchLatch launchLatch;BMW::Frame frames[BMW::FRAME_COUNT];
 uint8_t seq[BMW::FRAME_COUNT]={};uint8_t next=0;
 uint32_t sent[BMW::FRAME_COUNT]={},lastData=0,lastInit=0,lastBuild=0,lastLog=0,bcChanged=0;
 BMW::ButtonInput menuInputs[7];
 bool ready=false,haveData=false,bcRaw=false,bcStable=false;
 uint8_t ccProbeChoice=0;
 uint16_t errors=0;
 void oil(bool warning){if(BMWConfig::ENABLE_OIL_LOGIC_OUTPUT)digitalWrite(BMWConfig::OIL_LOGIC_PIN,warning==BMWConfig::OIL_ACTIVE_HIGH?HIGH:LOW);}
 void beginCan(){ready=canBus.begin(CAN_500KBPS,MCP_16MHz)==CAN_OK;if(ready)ready=canBus.setMode(MODE_NORMAL)==CAN_OK;lastInit=millis();FlowSerialDebugPrintLn(ready?"BMW G30 CAN ready":"BMW G30 CAN init failed");}
public:
 void setup(){for(uint8_t i=0;i<7;i++)pinMode(BMWConfig::MENU_BUTTON_PINS[i],INPUT_PULLUP);pinMode(53,OUTPUT);digitalWrite(53,HIGH);pinMode(BMWConfig::BC_BUTTON_PIN,INPUT_PULLUP);if(BMWConfig::ENABLE_OIL_LOGIC_OUTPUT){pinMode(BMWConfig::OIL_LOGIC_PIN,OUTPUT);oil(false);}beginCan();BMW::build(state,frames,seq,false);}
 void read(){
  String message=FlowSerialReadStringUntil('\n');message.trim();
  if(!message.length()||message.length()>=384)return;
  char text[384];message.toCharArray(text,sizeof(text));BMW::State incoming;
  if(!incoming.parse(text))return;
  launchLatch.update(incoming);state=incoming;lastData=millis();haveData=true;oil(state.v[21]!=0);
 }
 void loop(){
  uint32_t now=millis();
  if(haveData&&(uint32_t)(now-lastData)>BMWConfig::TIMEOUT_MS){state.reset();launchLatch.active=false;oil(false);lastData=now;}
  // Read every assigned switch independently. Unmapped controls emit diagnostics only.
  for(uint8_t i=0;i<7;i++)if(menuInputs[i].update(digitalRead(BMWConfig::MENU_BUTTON_PINS[i])==LOW,now)){
   const char* names[]={"BMW UP input (CAN unmapped)","BMW DOWN input (CAN unmapped)","BMW LEFT input (CAN unmapped)","BMW RIGHT input (CAN unmapped)","BMW OK input (CAN unmapped)","BMW BACK input (CAN unmapped)","BMW TRIP input (CAN unmapped)"};
   if(BMWConfig::ENABLE_CC_PROBE && menuInputs[i].pressed && !menuInputs[0].pressed && (i==1||i==2)){
    ccProbeChoice=(ccProbeChoice+(i==1?1:BMWConfig::CC_PROBE_COUNT-1))%BMWConfig::CC_PROBE_COUNT;
    const char* choices[]={"CC446: Launch Control","CC1: ACC disabled poor conditions","CC2: ACC disabled poor signal","CC3: ACC failure","CC26: Cruise defective","CC85: ACC disabled hill descent","CC176: ACC disabled sensor error","CC177: ACC malfunction","CC199: Cruise failed","CC337: Dynamic cruise failure","CC441: Cruise deactivated","CC435: Lane departure warning fault","CC78: Speed warning"};
    FlowSerialDebugPrintLn(choices[ccProbeChoice]);
   }else if(menuInputs[i].pressed&&BMWConfig::LOG_UNMAPPED_BUTTONS)FlowSerialDebugPrintLn(names[i]);
  }
  if(!ready){if((uint32_t)(now-lastInit)>=5000)beginCan();return;}
  bool value=digitalRead(BMWConfig::BC_BUTTON_PIN)==LOW;
  if(value!=bcRaw){bcRaw=value;bcChanged=now;}
  if((uint32_t)(now-bcChanged)>=30)bcStable=bcRaw;
  bool bc=bcStable||(haveData&&state.v[32]!=0);
  if((uint32_t)(now-lastBuild)>=10){lastBuild=now;BMW::build(state,frames,seq,bc,menuInputs[0].pressed,ccProbeChoice);}
  for(uint8_t checked=0;checked<BMW::FRAME_COUNT;checked++){
   uint8_t i=next;next=(next+1)%BMW::FRAME_COUNT;
   if(!haveData&&i!=BMW::BC&&i!=BMW::FLA_BASELINE)continue;
   if(!frames[i].enabled||(uint32_t)(now-sent[i])<frames[i].period)continue;
   if(canBus.trySendMsgBuf(frames[i].id,0,0,frames[i].len,frames[i].data)==CAN_OK){sent[i]=now;seq[i]=i==BMW::SAFETY?uint8_t(seq[i]+1):uint8_t((seq[i]+1)%15);}else ++errors;
   break;
  }
  if(errors&&(uint32_t)(now-lastLog)>=5000){lastLog=now;errors=0;FlowSerialDebugPrintLn("BMW CAN queue busy: check ACK/wiring/bitrate");}
 }
 void idle(){}
};
#endif
