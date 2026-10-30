#ifndef BMW_PROTOCOL_H
#define BMW_PROTOCOL_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "BMWConfig.h"
namespace BMW {
const uint8_t FIELD_COUNT=60;
const long PROFILE=3030;
inline long limit(long x,long a,long b){return x<a?a:(x>b?b:x);}
inline bool number(const char* p,long& v){
 if(!p||!*p||(*p!='-'&&(*p<'0'||*p>'9'))||(*p=='-'&&!p[1]))return false;
 for(const char* q=p+1;*q;q++)if(*q<'0'||*q>'9')return false;
 errno=0;char* end;v=strtol(p,&end,10);return errno!=ERANGE&&!*end;
}
inline uint8_t crc(const uint8_t* p,uint8_t n,uint8_t final,uint8_t initial=255){
 uint8_t c=initial;for(uint8_t i=0;i<n;i++){c^=p[i];for(uint8_t j=0;j<8;j++)c=(c&128)?uint8_t((c<<1)^0x1D):uint8_t(c<<1);}return c^final;
}
struct State {
 long v[FIELD_COUNT];char gear[5];
 State(){reset();}
 void reset(){memset(v,0,sizeof(v));v[15]=PROFILE;v[22]=-1000;v[23]=-1;v[33]=-1;v[34]=-32768;v[35]=-1;v[38]=-1000;v[40]=-1;v[41]=-10000;v[42]=-1;v[43]=-1;v[48]=-1;v[57]=7;v[58]=-1;strcpy(gear,"N");}
 bool parse(char* text){
  char* p[FIELD_COUNT];p[0]=text;uint8_t count=1;
  for(char* c=text;*c;c++)if(*c==';'){if(count>=FIELD_COUNT)return false;*c=0;p[count++]=c+1;}
  if(count!=FIELD_COUNT||strlen(p[2])>4)return false;
  State n;for(uint8_t i=0;i<FIELD_COUNT;i++)if(i!=2&&!number(p[i],n.v[i]))return false;
  if(n.v[15]!=PROFILE)return false;
  const uint8_t flags[]={5,6,7,8,9,10,12,16,18,20,21,24,25,26,27,28,29,37,39,44,45,46,47,49,51,52,53,54};
  for(uint8_t i=0;i<sizeof(flags);i++)if(n.v[flags[i]]<0||n.v[flags[i]]>1)return false;
  if(n.v[0]<0||n.v[0]>10000||n.v[1]<0||n.v[1]>500||n.v[3]<-40||n.v[3]>150||n.v[4]<0||n.v[4]>3||n.v[11]<0||n.v[11]>2||n.v[13]<0||n.v[13]>3||n.v[17]<0||n.v[17]>3||n.v[14]<0||n.v[14]>999999||n.v[19]<0||n.v[19]>255||n.v[22]<-1000||n.v[22]>200||n.v[23]<-1||n.v[23]>5000||n.v[30]<0||n.v[30]>2||n.v[31]<0||n.v[31]>11||n.v[32]<0||n.v[32]>4||n.v[33]<-1||n.v[33]>500||n.v[34]<-32768||n.v[34]>7160||n.v[35]<-1||n.v[35]>255||n.v[36]<0||n.v[36]>4||n.v[38]<-1000||n.v[38]>2000||n.v[40]<-1||n.v[40]>359||(n.v[41]!=-10000&&(n.v[41]<-1000||n.v[41]>1500))||n.v[42]<-1||n.v[42]>40000||n.v[43]<-1||n.v[43]>86399||n.v[48]<-1||n.v[48]>100||n.v[50]<0||n.v[50]>10||n.v[50]==6||n.v[55]<0||n.v[55]>31||n.v[56]<0||n.v[56]>4095)return false;
  if((n.v[57]!=7&&n.v[57]!=30&&n.v[57]!=80&&n.v[57]!=130)||n.v[58]<-1||n.v[58]>2550||n.v[59]<0||n.v[59]>8)return false;
  strcpy(n.gear,p[2]);for(char* c=n.gear;*c;c++)if(*c>='a'&&*c<='z')*c-=32;
  bool valid=!strcmp(n.gear,"P")||!strcmp(n.gear,"N")||!strcmp(n.gear,"R")||!strcmp(n.gear,"D")||!strcmp(n.gear,"L")||!strcmp(n.gear,"0")||!strcmp(n.gear,"-1");
  const char* g=n.gear;if(*g=='M'||*g=='S'||*g=='D'||*g=='A')g++;
  if(g[0]>='1'&&g[0]<='9'&&!g[1])valid=true;
  if(!valid)return false;*this=n;return true;
 }
};
// Automatic launch display latch, based on the final gear sent to the cluster.
// Missing ESC telemetry is treated as enabled; an explicit off state blocks launch.
struct LaunchLatch {
 bool active=false;
 void update(State& s){
  const bool sportGear=s.gear[0]=='S'&&s.gear[1]>='1'&&s.gear[1]<='9'&&!s.gear[2];
  if(s.v[4]<2||!sportGear||s.v[51]!=0||s.v[17]==3||s.v[1]>=105)active=false;
  else if(s.v[1]==0&&s.v[0]>1500)active=true;
  s.v[37]=active?1:0;
 }
};
struct Frame {uint16_t id;uint8_t len;uint16_t period;bool enabled;uint8_t data[8];};
enum Index {WAKE,IGN,SPEED,RPM,TEMP,PARK,ABS_BASE,GEAR,LIGHTS,TURNS,MODE,TRACTION,MIL,TPMS_LOSS,DSC_OFF,TC_ACTIVE,BRAKE_WARN,DOOR_FL,DOOR_FR,DOOR_RL,DOOR_RR,HOOD,BOOT,SEATBELT,OVERHEAT,FUEL,STEERING,SAFETY,RESTRAINT,BELT,BC,CRUISE_TEST,AUTO_H,AIRBAG,GENERIC_BRAKE,COOLANT_GAUGE,CLOCK,LANE_TEST,LAUNCH_MESSAGE,ECO_NAV,GEAR_RING,FLA_BASELINE,TPMS_STATUS,EPS_STATUS,SEATBELT_G02,SPEED_WARNING,SOS_CLEAR_FIRST,CC_PROBE_FIRST=SOS_CLEAR_FIRST+4,FRAME_COUNT=CC_PROBE_FIRST+BMWConfig::CC_PROBE_COUNT};
inline void raw(Frame& f,uint16_t id,uint8_t len,uint16_t period){f.id=id;f.len=len;f.period=period;f.enabled=true;memset(f.data,0,8);}
inline void protectedFrame(Frame& f,uint16_t id,const uint8_t* p,uint8_t n,uint8_t final,uint16_t period){raw(f,id,n+1,period);f.data[0]=crc(p,n,final);memcpy(f.data+1,p,n);}
inline void checkControl(Frame& f,uint16_t id,uint16_t code,bool on,uint8_t clear=0x28,uint8_t set=0x29){raw(f,id,8,100);f.data[0]=0x40;f.data[1]=code&255;f.data[2]=code>>8;f.data[3]=on?set:clear;for(uint8_t i=4;i<8;i++)f.data[i]=255;}
inline uint8_t gearNumber(const char* g){if(*g=='M'||*g=='S'||*g=='D'||*g=='A')g++;return g[0]>='1'&&g[0]<='9'&&!g[1]?g[0]-'0':0;}
inline uint8_t gearMode(const char* g){if(!strcmp(g,"P"))return 0x20;if(!strcmp(g,"R")||!strcmp(g,"-1"))return 0x40;if(!strcmp(g,"N")||!strcmp(g,"0"))return 0x60;if(g[0]=='M'&&gearNumber(g))return 2;if(g[0]=='S'&&gearNumber(g))return 0x81;return 0x80;}
inline void buildCCProbe(Frame* f,bool held,uint8_t choice,bool enabled,uint8_t clear,uint8_t set){
 for(uint8_t i=0;i<BMWConfig::CC_PROBE_COUNT;i++){
  checkControl(f[i],0x5C0,BMWConfig::CC_PROBE_CODES[i],held&&i==choice,clear,set);
  f[i].enabled=enabled;
 }
}
inline void build(const State& s,Frame* f,const uint8_t* seq,bool bc,bool ccHeld=false,uint8_t ccChoice=0){
 buildCCProbe(f+CC_PROBE_FIRST,ccHeld&&s.v[4]>=2,ccChoice,BMWConfig::ENABLE_CC_PROBE,BMWConfig::CC_PROBE_CLEAR,BMWConfig::CC_PROBE_SET);
 const long* v=s.v;const uint8_t n=seq[SPEED]%15;
 const uint8_t wake[4][8]={{0x40,0x10,0x04,0,0,0,1,0},{0x40,0x10,0x10,0,0,0x12,0x11,0},{0x40,0x10,0x40,0,3,0x97,0x11,0},{0x40,0x10,0,2,2,0x12,0x11,0}};
 const uint8_t terminal[4][7]={{0x30,0,0x10,1,0,0xE3,0xFF},{0x50,0,0x12,0x11,0,0xE5,0xFF},{0x70,3,0x97,0x11,0,0xE7,0xFF},{0xA0,2,0x12,0x11,0,0x2A,0xFF}};
 uint8_t key=uint8_t(limit(v[4],0,3));raw(f[WAKE],0x510,8,100);
 if(BMWConfig::USE_V1_2_WAKE_PROFILE){f[WAKE].data[0]=0x40;f[WAKE].data[1]=1;f[WAKE].data[2]=0x10;}else memcpy(f[WAKE].data,wake[key],8);
 if(BMWConfig::USE_V1_2_TERMINAL_PROFILE){raw(f[IGN],0x03C,8,100);f[IGN].data[1]=0x6E;f[IGN].data[6]=key>=2?0x0A:1;}
 else{uint8_t ignition[7];memcpy(ignition,terminal[key],7);ignition[0]|=seq[IGN]%15;protectedFrame(f[IGN],0x03C,ignition,7,0x91,100);}
 uint16_t speed=limit((v[1]*6401L)/100,0,65535);uint8_t sp[]={uint8_t(0xC0|n),uint8_t(speed),uint8_t(speed>>8),uint8_t(v[1]?0x91:0x81)};
 protectedFrame(f[SPEED],0x1A1,sp,4,0xA9,20);
 raw(f[RPM],0x0F3,8,20);uint16_t rpm=limit(v[0]/10,0,4095);
 uint8_t rp[]={0xF3,uint8_t((rpm&15)*16+seq[RPM]%15),uint8_t(rpm>>4),0xC0,0xF0,0xC4,0xFF,0xFF};
 if(BMWConfig::LEGACY_GOOFY_RPM){rpm=limit((v[0]*1557L)/1000,0,65535);rp[1]=rpm;rp[2]=rpm>>8;rp[5]=255;}
 f[RPM].data[0]=crc(rp,8,0x2C);memcpy(f[RPM].data+1,rp+1,7);
 uint8_t tp[]={uint8_t(0x10|seq[TEMP]%15),0x82,0x4E,uint8_t(limit(v[3]+46,0,255)),uint8_t(limit((v[22]==-1000?v[3]:v[22])+48,0,255)),5,0x89};
 protectedFrame(f[TEMP],0x3F9,tp,7,0xF1,100);
 uint8_t park[]={uint8_t(0xF0|seq[PARK]%15),0x38,0,uint8_t(v[12]?0x15:0x14)};protectedFrame(f[PARK],0x36F,park,4,0x17,50);
 uint8_t abs[]={uint8_t(0xF0|seq[ABS_BASE]%15),0xFE,0xFF,0x14};protectedFrame(f[ABS_BASE],0x36E,abs,4,0xD8,100);
 uint8_t mode=gearMode(s.gear),num=gearNumber(s.gear);uint8_t tr[]={uint8_t(num*16+seq[GEAR]%15),mode,0,0xFF};protectedFrame(f[GEAR],0x3FD,tr,4,0xD6,50);
 if(BMWConfig::F30_GEAR_LAYOUT){raw(f[GEAR],0x3FD,8,50);uint8_t t[]={uint8_t(seq[GEAR]%15),mode,0,0};f[GEAR].data[0]=crc(t,4,0x70,0);f[GEAR].data[1]=t[0];f[GEAR].data[2]=mode;}
 raw(f[LIGHTS],0x21A,3,50);f[LIGHTS].data[0]=(v[6]?4:0)|(v[8]?2:0)|(v[7]?(BMWConfig::REAR_FOG_WITH_FRONT?0x60:0x20):0);f[LIGHTS].data[1]=v[7]&&BMWConfig::REAR_FOG_WITH_FRONT?0xC0:((v[5]||v[6])?0x12:0);f[LIGHTS].data[2]=0xF7;
 raw(f[TURNS],0x1F6,2,100);f[TURNS].data[0]=0x01|(v[9]?0x10:0)|(v[10]?0x20:0);f[TURNS].data[1]=0xF0; // G30 source; conflicts with older 0x80 baseline documented.
 raw(f[MODE],0x3D8,8,100);f[MODE].enabled=v[50]!=0;
 if(BMWConfig::SOURCE_MODE_LAYOUT){f[MODE].data[0]=0xCF;f[MODE].data[1]=uint8_t(0xC0|v[50]);for(uint8_t i=2;i<8;i++)f[MODE].data[i]=0xFF;}
 else f[MODE].data[1]=v[50];
 raw(f[TRACTION],0x31B,8,100);f[TRACTION].data[2]=v[51]?1:(v[17]==2?4:0);
 checkControl(f[MIL],0x5C0,34,v[11]!=0,0x30,0x31); // G30-specific public bench source.
 checkControl(f[TPMS_LOSS],0x5C3,147,v[20]!=0,0x30,0x31);
 checkControl(f[DSC_OFF],0x5C0,36,v[51]!=0);
 checkControl(f[TC_ACTIVE],0x5C0,215,v[17]==1);
 checkControl(f[BRAKE_WARN],0x5C0,353,v[39]!=0); // Published CC list: brakes overheated; G30 bench verification pending.
 checkControl(f[GENERIC_BRAKE],0x5C0,42,v[27]!=0||v[16]!=0); // Confirmed combined ABS/brake warning; not an isolated ABS lamp.
 // User-tested G30 graphic combinations: never request a panel that is closed.
 checkControl(f[DOOR_FR],0x5C0,0x0E,v[45]!=0); // right front
 checkControl(f[DOOR_FL],0x5C0,0x0F,v[44]!=0&&v[45]!=0); // both fronts
 checkControl(f[DOOR_RL],0x5C0,0x10,false);f[DOOR_RL].enabled=false; // no tested mapping
 checkControl(f[DOOR_RR],0x5C0,0x11,v[46]!=0&&v[47]!=0); // both rears
 checkControl(f[HOOD],0x5C0,0x12,v[47]!=0&&v[29]!=0); // right rear + hood
 checkControl(f[BOOT],0x5C0,0x13,v[29]!=0&&v[28]!=0); // hood + boot
 checkControl(f[SEATBELT_G02],0x581,77,v[24]!=0&&v[4]>=2); // G02 source on29/off28, separate from old CC91.
 f[SEATBELT_G02].enabled=BMWConfig::ENABLE_G02_SEATBELT_MESSAGE;
 checkControl(f[SEATBELT],0x5C0,91,v[24]);checkControl(f[OVERHEAT],0x5C2,39,v[49],0x30,0x31);
 raw(f[FUEL],0x349,8,200);f[FUEL].enabled=v[48]>=0;
 uint16_t fuel=0x2500-(limit(v[48],0,100)*(0x2500L-0x0200))/100;
 f[FUEL].data[0]=f[FUEL].data[2]=fuel&255;f[FUEL].data[1]=f[FUEL].data[3]=fuel>>8;for(uint8_t i=4;i<8;i++)f[FUEL].data[i]=4;
 // G02 source normal-status examples; preserve real tyre warning on separate5C3.
 uint8_t tpmsStatus[]={uint8_t(0xF0|seq[TPMS_STATUS]%15),0xA0,0xA0,0xA0};
 protectedFrame(f[TPMS_STATUS],0x369,tpmsStatus,4,0xC5,100);
 f[TPMS_STATUS].enabled=BMWConfig::ENABLE_SOURCE_KEEPALIVES&&v[4]>=2;
 // CRC finalF9 independently matches all three supplied294 samples C7/CA/CC.
 uint8_t epsStatus[]={uint8_t(0xC0|seq[EPS_STATUS]%15),0xFE,0xFF,0x14};
 protectedFrame(f[EPS_STATUS],0x294,epsStatus,4,0xF9,100);
 f[EPS_STATUS].enabled=BMWConfig::ENABLE_SOURCE_KEEPALIVES&&v[4]>=2;
 uint8_t steering[]={uint8_t(0xF0|seq[STEERING]%15),0xFE,0xFF,0x14};protectedFrame(f[STEERING],0x2A7,steering,4,0x9E,100);
 raw(f[SAFETY],0xD7,2,100);f[SAFETY].data[0]=seq[SAFETY];f[SAFETY].data[1]=255;
 uint8_t restraint[]={uint8_t(0x40|seq[RESTRAINT]%15),0x40,0x55,0xFD,0xFF,0xFF,0xFF};protectedFrame(f[RESTRAINT],BMWConfig::RESTRAINT_STATUS_ID,restraint,7,0xFF,100);
 uint8_t belt[]={uint8_t(0xE0|seq[BELT]%15),0xF1,0xF0,0xF2,0xF2,0xFE};protectedFrame(f[BELT],0x297,belt,6,0x28,100);
 if(!BMWConfig::ENABLE_SOURCE_KEEPALIVES){f[ABS_BASE].enabled=f[STEERING].enabled=f[SAFETY].enabled=f[RESTRAINT].enabled=f[BELT].enabled=false;}
 raw(f[BC],0x1EE,2,50);f[BC].data[0]=bc?(BMWConfig::BC_X4M_LAYOUT?0x40:0x4C):0;f[BC].data[1]=BMWConfig::BC_X4M_LAYOUT?0xFF:0xFC;
 uint8_t cruise[]={uint8_t(0xF0|seq[CRUISE_TEST]%15),0xE0,0xE0,0xE1,0,0xEC,1};protectedFrame(f[CRUISE_TEST],0x289,cruise,7,0x82,100);f[CRUISE_TEST].enabled=BMWConfig::SEND_CRUISE_TEST_BASELINE;
 if(BMWConfig::ENABLE_EXPERIMENTAL_CRUISE){
  cruise[3]=v[13]==0?BMWConfig::CRUISE_OFF_STATE:(v[13]==1?BMWConfig::CRUISE_READY_STATE:BMWConfig::CRUISE_ACTIVE_STATE);
  cruise[4]=v[13]?limit(v[19]*BMWConfig::CRUISE_COUNTS_PER_KMH,0,255):0;
  protectedFrame(f[CRUISE_TEST],0x289,cruise,7,0x82,100);
 }
 uint8_t laneCounter=seq[LANE_TEST]%15;
 uint8_t lane[]={uint8_t(0x20|laneCounter),0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
 protectedFrame(f[LANE_TEST],0x327,lane,7,laneCounter,100);
 f[LANE_TEST].enabled=BMWConfig::ENABLE_EXPERIMENTAL_LANE&&v[54]!=0;
 checkControl(f[AUTO_H],0x5C0,57,v[4]>=2&&v[26]!=0);
 checkControl(f[AIRBAG],0x5C0,97,!BMWConfig::SUPPRESS_BENCH_RESTRAINT_WARNING&&v[25]!=0,BMWConfig::SUPPRESS_BENCH_RESTRAINT_WARNING&&(seq[AIRBAG]&1)?0x30:0x28);
 const uint16_t sosCodes[]={296,298,299,300};
 for(uint8_t i=0;i<4;i++){uint8_t idx=SOS_CLEAR_FIRST+i;checkControl(f[idx],0x5C0,sosCodes[i],false,(seq[idx]&1)?0x30:0x28);f[idx].enabled=BMWConfig::CLEAR_BENCH_EMERGENCY_CALL_WARNINGS;}
 // Restore the single source temperature variant; never send the conflicting MPG variant.
 uint8_t gauge[]={0x3E,uint8_t(limit(v[3],0,255)),0x64,0x64,0x64,1,0xF1};
 protectedFrame(f[COOLANT_GAUGE],0x2C4,gauge,7,0xB2,100);
 // Preserve supplied 2EC baseline and only its documented symbol/distance fields.
 // No checksum, approach-byte or left/right maneuver encoding is invented.
 raw(f[ECO_NAV],0x2EC,8,1000);f[ECO_NAV].enabled=BMWConfig::ENABLE_EXPERIMENTAL_ECO_NAV;
 uint8_t navCode=v[57];long navMeters=v[58];
 if(BMWConfig::ECO_NAV_TEST_CODE!=7){navCode=BMWConfig::ECO_NAV_TEST_CODE;navMeters=BMWConfig::ECO_NAV_TEST_DISTANCE_METERS;}
 bool eco=v[50]==7||v[50]==8||v[50]==9;
 bool navValid=v[4]>=2&&eco&&(navCode==30||navCode==80||navCode==130)&&navMeters>=0&&navMeters<=2550;
 f[ECO_NAV].data[0]=0xF0;f[ECO_NAV].data[1]=0x1C;
 f[ECO_NAV].data[3]=navValid?uint8_t((navMeters+5)/10):0;
 f[ECO_NAV].data[4]=0xFF;f[ECO_NAV].data[5]=0xFF;f[ECO_NAV].data[6]=navValid?navCode:7;
 raw(f[GEAR_RING],0x0DF,8,20);f[GEAR_RING].enabled=BMWConfig::ENABLE_GEAR_RING;
  uint8_t ringLevel=BMWConfig::GEAR_RING_TEST_LEVEL>=0?uint8_t(limit(BMWConfig::GEAR_RING_TEST_LEVEL,0,8)):uint8_t(v[59]);
 f[GEAR_RING].data[0]=v[4]>=2?uint8_t((BMWConfig::GEAR_RING_REQUEST_FLAG&0x10)|ringLevel):0;
 raw(f[FLA_BASELINE],0x36A,3,100);
 f[FLA_BASELINE].enabled=BMWConfig::ENABLE_EXPERIMENTAL_FLA_BASELINE&&v[4]>=2&&v[53]!=0;
 f[FLA_BASELINE].data[0]=0x02;f[FLA_BASELINE].data[1]=0xC0;f[FLA_BASELINE].data[2]=0xF7; // User supplied automatic high-beam payload.
 checkControl(f[LAUNCH_MESSAGE],0x5C0,446,v[37]!=0&&gearMode(s.gear)==0x81&&gearNumber(s.gear)>0,0x30,0x31); // Published Launch Control CC-ID; G30 display still a bench candidate.
 f[LAUNCH_MESSAGE].enabled=!BMWConfig::ENABLE_CC_PROBE; // Manual CC446 test must not compete with normal clear requests.
 bool overLimit=v[4]>=2&&v[33]>0&&v[1]>v[33]+BMWConfig::SPEED_WARNING_MARGIN_KMH;
 checkControl(f[SPEED_WARNING],0x5C0,BMWConfig::SPEED_WARNING_CC_ID,overLimit,0x30,0x31);
 f[SPEED_WARNING].enabled=BMWConfig::ENABLE_SPEED_WARNING&&!BMWConfig::ENABLE_CC_PROBE;
 raw(f[CLOCK],0x39E,8,1000);f[CLOCK].enabled=v[43]>=0&&v[55]>=1&&v[56]>=2000;
 if(f[CLOCK].enabled){f[CLOCK].data[0]=v[43]/3600;f[CLOCK].data[1]=(v[43]/60)%60;f[CLOCK].data[2]=v[43]%60;f[CLOCK].data[3]=v[55];f[CLOCK].data[4]=v[56]>>8;f[CLOCK].data[5]=v[56]&255;f[CLOCK].data[7]=2;}
}
}
#endif
