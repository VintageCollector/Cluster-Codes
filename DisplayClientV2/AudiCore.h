#pragma once
// Bit definitions derived from comma.ai opendbc; V2 integrity from supplied A6 trace.
// Copyright (c) 2020, Comma.ai, Inc. MIT license: ../references/LICENSE.opendbc.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#define AUDI_FRAME_STORAGE PROGMEM
#else
#define AUDI_FRAME_STORAGE
#endif
#include "AudiConfig.h"
namespace Audi {
// v4;heartbeat;run;rpm;kmh;coolantC;oilC;low;high;frontFog;rearFog;
// parkingBrake;brakeWarning;absLamp;escActivity;escOff;cel;cruise;setKmh;
// left;right;hazard;menuMask (UP1 DOWN2 LEFT4 RIGHT8 OK16);gear;distanceWarning
enum Field { VER, HB, RUN, RPM, KMH, COOLANT, OIL, LOWBEAM, HIGHBEAM,
 FRONTFOG, REARFOG, PARKING, BRAKEWARN, ABS, ESC, ESCOFF, CEL, CRUISE,
 SETKMH, LEFT, RIGHT, HAZARD, MENU, GEAR, DISTWARN, COUNT };
struct State { long v[COUNT]; State(){memset(v,0,sizeof(v));v[COOLANT]=v[OIL]=-1000;} };
inline bool parse(char* text,State& out) {
 State next;char* p=text;
 for(uint8_t i=0;i<COUNT;++i) {
  if(!*p || (*p!='-'&&(*p<'0'||*p>'9')))return false;
  char* end;errno=0;long n=strtol(p,&end,10);
  if(errno==ERANGE||end==p)return false;
  if(i+1<COUNT){if(*end!=';')return false;p=end+1;}else if(*end)return false;
  next.v[i]=n;
 }
 if(next.v[VER]!=4||next.v[HB]<0||next.v[HB]>999999||next.v[RPM]<0||next.v[RPM]>12000||
 next.v[KMH]<0||next.v[KMH]>400||next.v[SETKMH]<0||next.v[SETKMH]>326)return false;
 for(uint8_t i=COOLANT;i<=OIL;++i)if(next.v[i]!=-1000&&(next.v[i]<-48||next.v[i]>(i==OIL?190:141)))return false;
 if(next.v[RUN]<0||next.v[RUN]>1)return false;
 for(uint8_t i=LOWBEAM;i<COUNT;++i){if(i==SETKMH||i==GEAR)continue;long max=i==MENU?31:(i==CRUISE?2:1);if(next.v[i]<0||next.v[i]>max)return false;}
 if(!((next.v[GEAR]>=-2&&next.v[GEAR]<=18)||(next.v[GEAR]>=100&&next.v[GEAR]<=108)))return false;
 out=next;return true;
}
inline void bits(uint8_t* d,uint8_t start,uint8_t len,uint32_t value) {
 for(uint8_t i=0;i<len;++i){uint8_t b=start+i,m=1U<<(b%8);if(value&(1UL<<i))d[b/8]|=m;else d[b/8]&=~m;}
}
// opendbc MLB fallback CRC8-H2F, init/xorout FF, poly2F.
// 0x3C0 adds sourced data ID C3; 0x040 adds40. No fabricated seeds for other IDs.
inline uint8_t crcStep(uint8_t c){for(uint8_t i=0;i<8;++i)c=(c&0x80)?(uint8_t)((c<<1)^0x2F):(uint8_t)(c<<1);return c;}
inline uint8_t h2f(const uint8_t* d,uint8_t dlc,bool addDataId=false,uint8_t dataId=0) {
 uint8_t c=0xFF;for(uint8_t i=1;i<dlc;++i)c=crcStep(c^d[i]);if(addDataId)c=crcStep(c^dataId);return c^0xFF;
}
struct Frame {uint16_t id,period;uint8_t dlc,checksum,seed;bool enabled;uint8_t data[8];};
// checksum 0 none, 1 H2F with optional ID-specific C3, 2 XOR with seed.
inline void integrity(Frame& f,uint8_t counter) {
 if(!f.checksum)return;
 f.data[1]=(f.data[1]&0xF0)|(counter&15);
 if(f.checksum==2){uint8_t c=f.seed;for(uint8_t i=1;i<f.dlc;++i)c^=f.data[i];f.data[0]=c;}
 else f.data[0]=h2f(f.data,f.dlc,f.id==0x3C0||f.id==0x040,f.id==0x040?0x40:0xC3);
}
enum { IGN_F,RPM_F,DISPLAY_RPM_F,SPEED_F,TEMP_F,CEL_F,WARN_F,PARK_F,LIGHT_F,HIGH_F,TURN_F,CRUISE_F,AIRBAG_F,ESP02_F,GEAR_F,WAKE_F,DIM_F,TPMS_F,DISTANCE_F,DISTANCE_WARN_F,SUSPENSION_F,SUSPENSION_LEVEL_F,FRAME_COUNT };
inline void build(const State& s,Frame* f,uint32_t now) {
 static const Frame base[FRAME_COUNT] AUDI_FRAME_STORAGE={
  {0x3C0,100,4,2,0xC3,true,{0}},
  {0x105,10,8,2,0x04,AUDI_ENABLE_RPM,{0}},
  {0x107,20,8,0,0,AUDI_ENABLE_RPM,{0}},
  {0x100,20,8,2,0xAB,AUDI_ENABLE_SPEED,{0,0xE0,0xBF,0xFF,0,0,0,0}},
  {0x640,500,8,0,0,AUDI_ENABLE_TEMPERATURE,{0xC0,0x8A,0,0,0x7F,0x02,0,0}},
  {0x440,100,8,0,0,AUDI_ENABLE_CEL,{0,0x10,0x28,0,0,0,0,0x08}},
  {0x308,40,8,0,0,AUDI_ENABLE_WARNINGS||AUDI_ENABLE_SPEED,{0,0,0,0,0,0,0x7E,0x30}},
  {0x30D,50,4,0,0,AUDI_ENABLE_WARNINGS,{0}},
  {0x644,100,8,0,0,AUDI_ENABLE_LIGHTS&&AUDI_ENABLE_GATEWAY_LIGHTS,{0}},
  {0x662,100,8,0,0,AUDI_ENABLE_LIGHTS&&AUDI_ENABLE_SOURCE_HIGHBEAM,{0}},
  {0x363,50,8,0,0,AUDI_ENABLE_TURNS,{0}},
  {0x312,50,8,2,0x11,AUDI_ENABLE_CRUISE,{0}},
  {0x040,50,8,2,0x40,AUDI_ENABLE_AIRBAG_OFF,{0}},
  {0x101,20,8,2,0xAA,AUDI_ENABLE_ESP02,{0,0,0x7F,0,0x02,0,0,0}},
  {0x393,80,8,2,0x90,AUDI_ENABLE_GEAR,{0,0,0x0C,0,0,0,0,0}},
  {0x6C0,1000,7,0,0,AUDI_ENABLE_WAKE,{0x04,0x02,0x05,0,0,0x1F,0}},
  {0x5F0,100,5,0,0,AUDI_ENABLE_DIMMING,{0xAD,0,0x64,0xAD,0}},
  {0x64A,100,8,0,0,AUDI_ENABLE_TPMS_SAMPLE,{0}},
  {0x30C,100,8,2,0x0F,AUDI_ENABLE_DISTANCE_DISPLAY,{0}},
  {0x324,100,8,2,0x27,AUDI_ENABLE_DISTANCE_DISPLAY,{0}},
  {0x396,100,8,0,0,AUDI_ENABLE_SUSPENSION_OFF_REQUESTS,{0}},
  {0x325,100,8,0,0,AUDI_ENABLE_SUSPENSION_OFF_REQUESTS,{0,0,0x10,0,0,0,0,0}}
 };
 #ifdef __AVR__
 memcpy_P(f,base,sizeof(base));
#else
 memcpy(f,base,sizeof(base));
#endif
 bool run=s.v[RUN];
 // RUN27 from supplied header and captured trace. Offline01 remains source ACC/OFF request.
 f[IGN_F].data[2]=run?0x27:0x01;
 bits(f[RPM_F].data,16,16,run?s.v[RPM]*4:0);
 bits(f[RPM_F].data,39,1,run&&s.v[RPM]>0);
 // DBC display RPM=raw*3, 12 bits, Byte4 plus low nibble Byte5.
 bits(f[DISPLAY_RPM_F].data,24,12,run?(s.v[RPM]+1)/3:0);
 bits(f[SPEED_F].data,32,16,run?s.v[KMH]*100:0);
 bits(f[SPEED_F].data,48,1,run&&s.v[ESCOFF]);
 bits(f[SPEED_F].data,49,1,run&&s.v[ESCOFF]);
 bits(f[SPEED_F].data,56,1,run&&s.v[ABS]);
 bits(f[SPEED_F].data,57,1,run&&s.v[ESC]&&!s.v[ESCOFF]);
 bool coolant=s.v[COOLANT]!=-1000,oil=s.v[OIL]!=-1000;
 // Source baseline intake8A is static, not a live game reading.
 bits(f[TEMP_F].data,1,1,!oil);bits(f[TEMP_F].data,2,1,!coolant);
 // Preserve source baseline intake8A when live; flags mark unknown oil/coolant.
 f[TEMP_F].data[2]=oil?(uint8_t)(s.v[OIL]+60):254;
 f[TEMP_F].data[3]=coolant?(uint8_t)(((s.v[COOLANT]+48)*4+1)/3):254;
 bits(f[CEL_F].data,49,1,run&&s.v[CEL]);
 // Explicit OFF defaults: no object, acoustic request, text or optical warning.
 // Only a real bound distance-warning input enables these display requests.
 bool distanceWarning=run&&s.v[DISTWARN];
 bits(f[DISTANCE_F].data,22,2,distanceWarning?2:0); // red color candidate, inferred from related FAS field
 bits(f[DISTANCE_F].data,40,1,distanceWarning); // ACC_Optischer_Fahrerhinweis
 bits(f[DISTANCE_F].data,61,3,distanceWarning?2:0); // passive display, never active longitudinal control
 bits(f[DISTANCE_WARN_F].data,35,1,distanceWarning); // ACC_Warnhinweis
 // Keep braking_guard text codes and all ACC actuator-command messages absent.
 bool blink=(now/250)%2==0;
 uint32_t speed308=run?(uint32_t)(s.v[KMH]*AUDI_SPEED_308_RAW_PER_KMH+0.5f):0;
 if(speed308>32767)speed308=32767;
 bits(f[WARN_F].data,16,15,speed308);
 bits(f[WARN_F].data,4,1,run&&s.v[ESC]&&!s.v[ESCOFF]&&blink);
 bits(f[WARN_F].data,5,1,run&&s.v[ABS]);
 bits(f[WARN_F].data,6,1,run&&s.v[BRAKEWARN]); // BK_Lampe, NOT parking
 bits(f[WARN_F].data,15,1,run&&s.v[ESCOFF]);
 bits(f[WARN_F].data,31,1,run&&s.v[ESC]&&!s.v[ESCOFF]&&blink);
 bits(f[PARK_F].data,2,1,run&&s.v[PARKING]); // EPB function lamp
 // No unexplained original Byte2=02: DBC identifies it as brake-pad warning.
 bits(f[LIGHT_F].data,27,1,run&&(s.v[LOWBEAM]||s.v[HIGHBEAM]));
 bits(f[LIGHT_F].data,30,1,run&&s.v[REARFOG]);
 bits(f[LIGHT_F].data,33,3,run&&(s.v[LOWBEAM]||s.v[HIGHBEAM])?4:1);
 if(AUDI_FRONT_FOG_AS_ALLWEATHER)bits(f[LIGHT_F].data,36,1,run&&s.v[FRONTFOG]);
 // User header explicitly identifies low=B1mask04 and high=B3mask04 on0x662.
 f[HIGH_F].data[0]=run&&(s.v[LOWBEAM]||s.v[HIGHBEAM])?4:0;
 f[HIGH_F].data[2]=run&&s.v[HIGHBEAM]?4:0;
 bits(f[ESP02_F].data,15,1,!run||s.v[KMH]==0); // standstill, neutral acceleration/yaw
 long gear=s.v[GEAR];f[GEAR_F].data[3]=gear==-2?2:(gear==-1?3:(gear==0?4:5));
 f[GEAR_F].data[1]=gear==-2?0x20:0;
 f[DIM_F].data[0]=f[DIM_F].data[3]=run&&(s.v[LOWBEAM]||s.v[HIGHBEAM])?0xFD:0xAD;
 f[WAKE_F].enabled=AUDI_ENABLE_WAKE&&run;
 f[DIM_F].enabled=AUDI_ENABLE_DIMMING&&run;
 f[TPMS_F].enabled=AUDI_ENABLE_TPMS_SAMPLE&&run;
 bool l=run&&(s.v[LEFT]||s.v[HAZARD]),r=run&&(s.v[RIGHT]||s.v[HAZARD]);
 bits(f[TURN_F].data,8,1,l);bits(f[TURN_F].data,9,1,r);bits(f[TURN_F].data,10,1,run&&s.v[HAZARD]);
 // DBC cluster phase bits, generated at2Hz from steady selected flags.
 bits(f[TURN_F].data,18,1,l&&blink);bits(f[TURN_F].data,19,1,r&&blink);
 uint8_t cruise=run?s.v[CRUISE]:0;
 bits(f[CRUISE_F].data,12,10,cruise==2?(uint32_t)(s.v[SETKMH]*100+16)/32:1022);
 bits(f[CRUISE_F].data,22,2,cruise?1:0); // documented typical green
 bits(f[CRUISE_F].data,48,7,cruise==2?52:0); // SET / GRA symbol
 bits(f[CRUISE_F].data,61,3,cruise==2?3:(cruise==1?2:0));
}
struct Filter {
 State stable;long candidate[COUNT];uint32_t since[COUNT];
 Filter(){memset(candidate,0,sizeof(candidate));memset(since,0,sizeof(since));}
 void reset(){*this=Filter();}
 void update(const State& in,uint32_t now){
  for(uint8_t i=0;i<COUNT;++i){
   if(i<LOWBEAM||i==MENU||i==SETKMH){stable.v[i]=in.v[i];continue;}
   if(i==ABS||i==ESC||i==BRAKEWARN||i==DISTWARN){if(in.v[i]){stable.v[i]=1;since[i]=now;}else if((uint32_t)(now-since[i])>=(i==BRAKEWARN?1000UL:300UL))stable.v[i]=0;continue;}
   if(in.v[i]!=candidate[i]){candidate[i]=in.v[i];since[i]=now;}
   if((uint32_t)(now-since[i])>=60)stable.v[i]=candidate[i];
  }
 }
};
}
