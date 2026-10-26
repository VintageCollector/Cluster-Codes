#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "LexusConfig.h"
namespace Lexus {
enum Field { VER, HB, RUN, RPM, KMH, GEAR, LOWBEAM, HIGHBEAM, FOG, BRAKE,
 SEATBELT, DOORS, ABS, TC, PCS, LDA, CRUISE, MODE, CEL, LEFT, RIGHT, HAZARD,
 PARK, FLASH_HIGH, COUNT };
// 24 decimal fields; legacy 23-field messages default flash-to-pass to false.
struct State { long v[COUNT]; State() { memset(v,0,sizeof(v)); v[MODE]=3; } };
inline bool parse(char* text, State& out) {
 State next; char* p=text;
 for(uint8_t i=0;i<COUNT;++i) {
  if(!*p || (*p!='-' && (*p<'0'||*p>'9'))) return false;
  char* end; errno=0; long n=strtol(p,&end,10);
  if(errno==ERANGE || end==p) return false;
  if(i+1<COUNT) {
   if(*end!=';') { if(i==PARK && *end=='\0') break; return false; }
   p=end+1;
  }
  else if(*end) return false;
  next.v[i]=n;
 }
 if(next.v[VER]!=1 || next.v[HB]<0 || next.v[HB]>999999 ||
    next.v[RPM]<0 || next.v[RPM]>12000 || next.v[KMH]<0 || next.v[KMH]>400 ||
    !((next.v[GEAR]>=-2 && next.v[GEAR]<=18) ||
      (next.v[GEAR]>=100 && next.v[GEAR]<=108))) return false;
 for(uint8_t i=RUN;i<COUNT;++i) {
  if(i==RPM||i==KMH||i==GEAR) continue;
  long max=i==MODE?4:((i==TC||i==CRUISE)?2:((i==PCS)?3:((i==DOORS)?255:1)));
  if(next.v[i]<0 || next.v[i]>max) return false;
 }
 out=next; return true;
}
struct Frame { uint16_t id, period; uint8_t data[8]; bool enabled; };
inline uint8_t lightingByte(bool lowBeam,bool highBeam,bool frontFog,bool flashHighBeam) {
 if(highBeam && !flashHighBeam) lowBeam=true;
 uint8_t lighting=0x00;
 if(lowBeam && highBeam && frontFog) lighting=0x68;
 else if(lowBeam && highBeam) lighting=0x60;
 else if(lowBeam && frontFog) lighting=0x28;
 else if(highBeam && frontFog && flashHighBeam) lighting=0x48;
 else if(highBeam && frontFog) lighting=0x08;
 else if(lowBeam) lighting=0x20;
 else if(highBeam) lighting=0x40;
 else if(frontFog) lighting=0x08;
 else lighting=0x00;
 return lighting;
}
inline bool speedRaw(uint16_t kmh,uint16_t& raw) {
 if(LEXUS_SPEED_LINEAR) {
  if(LEXUS_SPEED_UNITS_PER_KMH<=0) return false;
  float scaled=kmh*LEXUS_SPEED_UNITS_PER_KMH+0.5f;
  raw=scaled>65535?65535:(uint16_t)scaled; return true;
 }
 if(LEXUS_SPEED_CURVE) {
  // Exact observations preserved; interpolation/extrapolation is PROVISIONAL.
  static const uint16_t k[]={0,3,4,5,6,7,42};
  static const uint16_t r[]={0,0x0100,0x0101,0x0200,0x0201,0x0202,0x1000};
  uint8_t segment=0;
  while(segment<5 && kmh>k[segment+1])++segment;
  uint32_t width=k[segment+1]-k[segment];
  uint32_t encoded=r[segment]+((uint32_t)(kmh-k[segment])*(r[segment+1]-r[segment])+width/2)/width;
  raw=encoded>65535?65535:(uint16_t)encoded;return true;
 }
 // User's displayed km/h observations, not an inferred linear scale.
 // Zero is an unverified idle assumption, tested during bench acceptance.
 switch(kmh) { case 0:raw=0;return true; case 3:raw=0x0100;return true;
  case 4:raw=0x0101;return true; case 5:raw=0x0200;return true;
  case 6:raw=0x0201;return true; case 7:raw=0x0202;return true;
  case 42:raw=0x1000;return true; }
 return false;
}
enum { LIGHT_F, WARN_F, RPM_F, PCS_F, ABS_F, CRUISE_F, MODE_F, GEAR_F,
 SPEED_F, LDA_F, TEMP_F, PARK_F, IGN_F, EPS_F, FRAME_COUNT };
inline void build(const State& s, Frame* f, uint32_t now=0) {
 const Frame base[FRAME_COUNT] = {
  {0x622,100,{0x12,0,0x88,0,0,0,0,0},true}, {0x620,100,{0x10,0x80,0xFF,0xFF,0x80,0,0,0x80},true},
  {0x1C4,20,{0},true}, {0x411,100,{0},true},
  {0x3B7,100,{0},LEXUS_SOURCE_ABS_ESC},
  {0x399,100,{0x11,0,0,0,0,0,0,0},LEXUS_SOURCE_CRUISE},
  {0x395,100,{0,0,0,0,0,0x80,0,0},LEXUS_SOURCE_SPORT},
  {0x3BC,100,{0},false}, {0x0B4,20,{0},LEXUS_SPEED_ENABLED},
  {0x412,100,{0},LEXUS_LDA_BEEP},
  {0x3B0,500,{0,0,0,LEXUS_OUTSIDE_FIXED_BYTE,0,0,0,0},LEXUS_OUTSIDE_SAMPLE_ENABLED},
  {0x396,100,{3,0,0,0,0,0,1,0},false},
  {0x3BB,100,{LEXUS_IGNITION_FIXED_BYTE,0,LEXUS_ENGINE_TEMP_FIXED_BYTE,0,0,0,0,0},LEXUS_ENGINE_TEMP_ENABLED},
  {0x394,100,{0x01,0x00,0x61,0xA7,0x03,0,0,0},LEXUS_EPS_FRAME_ENABLED}
 }; memcpy(f,base,sizeof(base));
 bool live=s.v[RUN]!=0;
 bool assistsOff=live && (s.v[MODE]==4 || s.v[TC]==2);
 bool comfort=live && s.v[MODE]==0 && !assistsOff;
 bool sport=live && (s.v[MODE]==1 || s.v[MODE]==2) && !assistsOff;
 f[EPS_F].data[1]=LEXUS_EPS_WARNING_LAMP_ON?0x10:0x00;
 f[LIGHT_F].data[3]=lightingByte(s.v[LOWBEAM],s.v[HIGHBEAM],s.v[FOG],s.v[FLASH_HIGH]);
 f[WARN_F].data[7]|=(s.v[BRAKE]?0x10:0)|((LEXUS_ALLOW_SEATBELT_LAMP && s.v[SEATBELT])?0x40:0);
 f[WARN_F].data[5]=0x00; // unconditional Byte6 OFF, never overwritten by doors/airbag
 f[RPM_F].data[0]=(uint8_t)((s.v[RPM]+100)/200);
 if(LEXUS_SOURCE_CEL_ECO) {
  f[RPM_F].data[5]=s.v[CEL]?1:0;
  f[RPM_F].data[6]=comfort?0xC0:0;
 }
 f[PCS_F].data[0]=(s.v[PCS]&1?0xC0:0)|(s.v[PCS]&2?0x10:0);
 bool absLamp=s.v[ABS] && (!LEXUS_ABS_BLINK || ((now/LEXUS_ABS_BLINK_HALF_MS)%2)==0);
 f[ABS_F].data[0]=absLamp?0x10:0;
 f[ABS_F].data[1]=assistsOff?0x18:(live && s.v[TC]==1?0x02:0x00);
 f[CRUISE_F].data[1]=s.v[CRUISE]==2?0x12:(s.v[CRUISE]==1?0x10:0);
 f[MODE_F].enabled=LEXUS_SOURCE_SPORT;
 f[MODE_F].data[0]=sport?0x07:0x00; // transmit explicit clear in every other mode
 f[GEAR_F].enabled=LEXUS_GEAR_ENABLED;
 long gear=s.v[GEAR];
 if(gear==-2) f[GEAR_F].data[1]=0x20;
 else if(gear==-1) f[GEAR_F].data[1]=0x10;
 else if(gear==0) f[GEAR_F].data[1]=0x0F;
 else if(gear==100 || (gear>=1 && gear<=18 && !sport))
  f[GEAR_F].data[5]=0x80; // Comfort: plain D; gear number intentionally hidden.
 else {
  bool manual=gear>=101;
  uint8_t number=(uint8_t)(manual?gear-100:(gear>8?8:gear));
  f[GEAR_F].data[1]=manual?0x02:0x03;
  f[GEAR_F].data[4]=(uint8_t)(number<<4);
 }
 uint16_t rawSpeed=0;
 f[SPEED_F].enabled=speedRaw((uint16_t)s.v[KMH],rawSpeed) && LEXUS_SPEED_ENABLED;
 f[SPEED_F].data[LEXUS_SPEED_MSB_INDEX]=(uint8_t)(rawSpeed>>8);
 f[SPEED_F].data[LEXUS_SPEED_LSB_INDEX]=(uint8_t)rawSpeed;
 f[LDA_F].data[3]=s.v[LDA]?LEXUS_LDA_BEEP_MASK:0;
 f[LDA_F].enabled=LEXUS_LDA_BEEP && LEXUS_LDA_BEEP_MASK!=0;
 f[PARK_F].enabled=LEXUS_PARK_SAMPLE_ENABLED && s.v[PARK];
}
// Stable-state filter: a changed level must persist 60ms. Never filters heartbeat.
struct Filter {
 long candidate[COUNT]; uint32_t since[COUNT]; State stable;
 Filter() { memset(candidate,0,sizeof(candidate)); memset(since,0,sizeof(since)); }
 void update(const State& input,uint32_t now) {
  for(uint8_t i=0;i<COUNT;++i) {
   if(i<LOWBEAM) { stable.v[i]=input.v[i]; continue; }
   if(i==HIGHBEAM && (input.v[FLASH_HIGH] || stable.v[FLASH_HIGH])) {
    stable.v[i]=input.v[i]; continue; // no delayed normal-high tail after a flash
   }
   if(i==FLASH_HIGH) { stable.v[i]=input.v[i]; continue; } // momentary command
   // Explicit disabled state wins immediately; release it immediately on re-enable.
   if(i==TC && (input.v[i]==2 || stable.v[i]==2)) {
    stable.v[i]=input.v[i]; since[i]=now; continue;
   }
   // Do not debounce away short intervention events. Hold activity for visibility.
   if(i==ABS || i==TC) {
    if(input.v[i]) { stable.v[i]=input.v[i]; since[i]=now; }
    else if((uint32_t)(now-since[i])>=LEXUS_ACTIVITY_HOLD_MS) stable.v[i]=0;
    continue;
   }
   if(input.v[i]!=candidate[i]) { candidate[i]=input.v[i]; since[i]=now; }
   if((uint32_t)(now-since[i])>=60) stable.v[i]=candidate[i];
  }
 }
 void reset() { *this=Filter(); }
};
}
