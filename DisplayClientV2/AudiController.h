#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include "AudiCore.h"
#include "AudiMenuMap.h"
class AudiController {
 Audi::FuelCounter fuelUsage;MCP_CAN bus{AUDI_CS};Audi::State latest;Audi::Filter filter;
 bool have=false,ready=false;uint32_t lastData=0,retry=0,due[Audi::FRAME_COUNT]={};
 uint8_t counter[Audi::FRAME_COUNT]={},cursor=0;
 uint8_t lastMenu=31,pending=0,menuIndex=0,menuPhase=0,menuCounter=0;
 uint32_t menuSince=0,menuSent=0,physicalChanged=0;
 uint8_t physicalCandidate=0,physicalStable=0;
 uint8_t physicalMenu(uint32_t now){
  if(!AUDI_ENABLE_PHYSICAL_MENU)return 0;
  const uint8_t pins[]={AUDI_BUTTON_UP,AUDI_BUTTON_DOWN,AUDI_BUTTON_LEFT,AUDI_BUTTON_RIGHT,AUDI_BUTTON_OK};
  uint8_t mask=0;for(uint8_t i=0;i<5;i++)if(digitalRead(pins[i])==LOW)mask|=1<<i;
  if(mask!=physicalCandidate){physicalCandidate=mask;physicalChanged=now;}
  if((uint32_t)(now-physicalChanged)>=30)physicalStable=physicalCandidate;
  return physicalStable;
 }
 void initialize(){ready=bus.begin(MCP_ANY,AUDI_BITRATE,AUDI_CLOCK)==CAN_OK;if(ready)ready=bus.setMode(MCP_NORMAL)==CAN_OK;}
 bool transmit(Audi::Frame& f){if(bus.sendMsgBuf(f.id,0,f.dlc,f.data)!=CAN_OK){++txErrors;return false;}return true;}
 bool menu(bool fresh,uint32_t now){
  uint8_t mask=fresh?((uint8_t)latest.v[Audi::MENU]|physicalMenu(now)):0;
  uint8_t rising=mask&~lastMenu;lastMenu=fresh?mask:31;
  if(!AUDI_ENABLE_MENU){if(rising)++unsupportedMenu;pending=0;return false;}
  if(fresh)pending|=rising;else pending=0;
  if(menuPhase==0&&pending){
   for(menuIndex=0;menuIndex<5;++menuIndex)if(pending&(1<<menuIndex))break;
   pending&=~(1<<menuIndex);
   const Audi::MenuMap& m=Audi::MENU_MAP[menuIndex];
   if(!m.id||m.id>0x7FF||m.dlc<1||m.dlc>8||m.checksum>2||(m.checksum&&m.dlc<2)){++unsupportedMenu;return false;}
   menuPhase=1;menuSince=now;menuSent=now-20;
  }
  if(menuPhase==1&&(!fresh||(uint32_t)(now-menuSince)>=AUDI_MENU_PRESS_MS)){menuPhase=2;menuSince=now;}
  if(menuPhase==2&&(uint32_t)(now-menuSince)>=AUDI_MENU_RELEASE_MS){menuPhase=0;return false;}
  if(!menuPhase||(uint32_t)(now-menuSent)<20)return false;
  const Audi::MenuMap& m=Audi::MENU_MAP[menuIndex];
  Audi::Frame f={m.id,20,m.dlc,m.checksum,m.seed,true,{0}};
  memcpy(f.data,menuPhase==1?m.press:m.release,8);Audi::integrity(f,menuCounter);
  menuSent=now;if(transmit(f))menuCounter=(menuCounter+1)&15;
  return true;
 }
public:
 uint32_t accepted=0,rejected=0,txErrors=0,unsupportedMenu=0;
 uint8_t requestedMenu=0,cmdWarn0=0,cmdWarn1=0,cmdLow=0,cmdHigh=0,cmdTemp=254,cmdCEL=0,cmdDistance=0,cmdSuspension=0;uint16_t cmdSpeed=0;bool live=false;
 bool canIsReady()const{return ready;}
 void setup(){pinMode(SS,OUTPUT);digitalWrite(SS,HIGH);
 if(AUDI_ENABLE_PHYSICAL_MENU){const uint8_t pins[]={AUDI_BUTTON_UP,AUDI_BUTTON_DOWN,AUDI_BUTTON_LEFT,AUDI_BUTTON_RIGHT,AUDI_BUTTON_OK};for(uint8_t i=0;i<5;i++)pinMode(pins[i],INPUT_PULLUP);}
 initialize();}
 bool accept(char* text){if(!Audi::parse(text,latest)){++rejected;return false;}fuelUsage.update(latest,millis());lastData=millis();have=true;++accepted;return true;}
 void loop(){
  uint32_t now=millis();live=have&&(uint32_t)(now-lastData)<=AUDI_TIMEOUT_MS&&latest.v[Audi::RUN];
  if(live)filter.update(latest,now);else {filter.reset();fuelUsage.previous=-1;fuelUsage.clock=false;}
  filter.stable.fuelCounter=fuelUsage.counter;filter.stable.fuelOverflow=fuelUsage.overflow;
  requestedMenu=live?latest.v[Audi::MENU]:0;
  if(!ready){if((uint32_t)(now-retry)>=2000){retry=now;initialize();}return;}
  if(!have)return;
  if(menu(live,now))return;
  Audi::Frame frames[Audi::FRAME_COUNT];Audi::build(filter.stable,frames,now);
  cmdWarn0=frames[Audi::WARN_F].data[0];cmdWarn1=frames[Audi::WARN_F].data[1];cmdSpeed=(uint16_t)frames[Audi::WARN_F].data[2]|((uint16_t)(frames[Audi::WARN_F].data[3]&0x7F)<<8);cmdLow=frames[Audi::HIGH_F].data[0];cmdHigh=frames[Audi::HIGH_F].data[2];cmdTemp=frames[Audi::TEMP_F].data[3];cmdCEL=frames[Audi::CEL_F].data[6];cmdDistance=frames[Audi::DISTANCE_WARN_F].data[4];cmdSuspension=frames[Audi::SUSPENSION_LEVEL_F].data[2];
  // One frame per loop call. Skip missed periods rather than burst catch-up.
  for(uint8_t n=0;n<Audi::FRAME_COUNT;++n){uint8_t i=cursor;cursor=(cursor+1)%Audi::FRAME_COUNT;
   if(!frames[i].enabled||(uint32_t)(now-due[i])<frames[i].period)continue;
   due[i]=now;Audi::integrity(frames[i],counter[i]);if(transmit(frames[i]))counter[i]=(counter[i]+1)&15;break;
  }
 }
};
