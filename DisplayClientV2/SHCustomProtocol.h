#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__
// SimHub Mega/Seeed port of the user-supplied Taycan-modified CarCluster code.
// Original CarCluster by Andrej Rolih: https://github.com/r00li/CarCluster
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515_can.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "TaycanReference.h"
#include "TaycanCRC.h"
#include "TaycanButtons.h"
#include "TaycanCapturedProfile.h"
#include "TaycanLightingProfile.h"

namespace Taycan {
#ifndef TAYCAN_LIGHT_DIAGNOSTIC
#define TAYCAN_LIGHT_DIAGNOSTIC 2
#endif
#ifndef TAYCAN_ENABLE_970_CLOCK
#define TAYCAN_ENABLE_970_CLOCK 0
#endif
const uint8_t CS_PIN = 9;
const uint16_t FAST_MS = 10; // Source used 50 ms; user requested 10 ms.
const uint16_t SLOW_MS = 500;
// Transmit the power needle at ~67 Hz; interpolation continues even when
// SimHub telemetry is slower. HYBRID remains at its original 50 ms cadence.
const uint16_t POWER_MS = 15;
const uint16_t HYBRID_MS = 50;
// First-order, time-correct interpolation (milliseconds). Smaller = snappier.
// These values deliberately avoid abrupt raw-byte jumps through neutral.
const uint16_t POWER_ATTACK_MS = 105;
const uint16_t POWER_RELEASE_MS = 145;
const uint16_t TIMEOUT_MS = 1500;
const uint16_t MAX_SPEED_KMH = 260;
const uint16_t MAX_RPM = 8000;
const float SPEED_CORRECTION = 1.0f;
const float RPM_CORRECTION = 1.0f;
const bool SEND_DISTANCE = true; // Source ESP24 distance; may advance mileage.
// Toggle one optional donor experiment at a time; default OFF avoids unexpected
// symbols, warning lights, and interference with an already working cluster.
const bool ENABLE_PANAMERA_CRUISE = true;
const bool ENABLE_PANAMERA_SIGN = false;   // 0x539 sign candidate.
const bool ENABLE_PANAMERA_LANEKEEP = true;
const bool ENABLE_PANAMERA_LANECHANGE = false;// 0x30F status candidate.
#ifndef TAYCAN_PANAMERA_EPB_DLC
#define TAYCAN_PANAMERA_EPB_DLC 8
#endif
static_assert(TAYCAN_PANAMERA_EPB_DLC==4 || TAYCAN_PANAMERA_EPB_DLC==8, "Choose DLC 4 or 8");
const uint16_t STEERING_TAKEOVER_STATUS_ID=0x2A8;
const bool ENABLE_PANAMERA_OIL_FRAME = false;// 0x640 fixed donor placeholder only.
const bool ENABLE_PANAMERA_DOOR_MASK = false;// 0x583 donor door mask.
const bool SEND_SECONDARY_READY = true; // Candidate 0x32F from CAN_Taycan.ino.
const uint8_t HEADLIGHT_OFF = 0x00; // Off value requires bench confirmation.
const uint8_t HEADLIGHT_LOW = 0x20; // Supplied MQB non-high-beam value.
const uint8_t HEADLIGHT_HIGH = 0x40; // Supplied MQB high-beam value.
// 0x658 byte 3: the old inactive value (0x01) appears to keep the amber
// rear-fog indicator illuminated on this bench cluster. 0x00 is a candidate
// OFF value, *not* a verified Taycan encoding. ON uses the user's working 0x01.
const uint8_t REAR_FOG_OFF = 0x00;
const uint8_t REAR_FOG_ON = 0x01; // User working low+fog payload.
// Isolated cluster: emit the captured BCM_01 status with lamp warnings clear.
#ifndef TAYCAN_SEND_CLEAR_LAMP_STATUS
#define TAYCAN_SEND_CLEAR_LAMP_STATUS 1
#endif
const uint16_t BCM_LIGHT_STATUS_ID=0x65A;
// Experimental Volkswagen MQB signals: hardware Taycan response unverified.
// Toggle either independently if the wrong physical symbol lights up.
const bool SEND_MQB_KOMBI_ESP_LAMP = true;  // 0x30B byte 1 bit 1
const bool USE_MQB_ESP21_STATUS_BITS = true; // 0x0FD byte 7 bits 0/1; byte 8 bit 5
inline constexpr uint8_t headlightByte(bool low,bool high,bool parking) {
  return (high?HEADLIGHT_HIGH:0) | ((low||high)?HEADLIGHT_LOW:HEADLIGHT_OFF) |
         ((parking||low||high)?0x10:0);
}
// IDs retained from secondary sketch, not enabled sweeps:
const uint16_t READY_ID = 0x32F;
const uint16_t SECONDARY_KEEPALIVE_ID = 0x510;
const uint16_t SECONDARY_IGNITION_ID = 0x12F;
const uint16_t SECONDARY_SEATBELT_ID = 0x520;
const uint16_t SECONDARY_SOUND_ID = 0x67C;
const uint16_t SECONDARY_TRANSPORT_ID = 0x585;
const uint16_t SECONDARY_CLOCK_ID = 0x6B2;

inline long bounded(long x,long low,long high) {
  return x<low?low:(x>high?high:x);
}
inline bool number(const char* text,long& result) {
  if (!text || !*text) return false;
  char* end;
  result = strtol(text,&end,10);
  return *end == '\0';
}
inline uint8_t gearCode(char* text) {
  for(char* p=text;*p;++p) if(*p>='a'&&*p<='z') *p-=32;
  if (!strcmp(text,"P")) return 10;
  if (!strcmp(text,"R") || !strcmp(text,"-1")) return 11;
  if (!strcmp(text,"N") || !strcmp(text,"0")) return 12;
  if (!strcmp(text,"D") || !strcmp(text,"S")) return 13;
  if (*text=='M'||*text=='A') ++text;
  long gear;
  if (!number(text,gear) || gear<1) return 12;
  return gear<=9?gear:13; // Source maps gears 10+ to D.
}

struct State {
  uint16_t rpm=0, speed=0;
  int32_t clockSeconds=-1;
  int16_t coolant=50, outside=23;
  uint8_t gear=12,key=0,throttle=0,charge=0,brightness=80;
  // -1000=full recuperation, 0=neutral, +1000=full power.
  // This is normalized SimHub telemetry, not a measured Taycan motor kW.
  int16_t signedPowerPermille=0;
  bool high=false,fog=false,left=false,right=false,handbrake=false,door=false;
  bool checkEngine=false,cruise=false,low=false,parking=false;
  bool autoHigh=false,autoHold=false,absActive=false,absWarning=false,frontFog=false;
  int16_t rangeKm=-1;
  int16_t batterySoc=-1, batteryTempTenths=-1000;
  int8_t charging=-1;
  bool escOff=false,tcOff=false,escActive=false,tcActive=false;
  bool psmSport=false, psmOff=false, tpmsWarning=false;
  uint16_t cruiseSetKmh=0, fullRangeKm=500;
  int16_t oilPressureKpa=-1;
  int16_t tpmsKpa[4]={-1,-1,-1,-1};
  int16_t speedLimitKmh=-1, oilTempC=-99;
  uint8_t laneKeep=0, laneChange=0, doorMask=0;
  uint8_t fuelPct=0;
};

// Returns the same LUT-based CRC used by the supplied VWMQBCluster.cpp.
// Caller must fill byte 1 (counter plus selector) and all payload bytes first.
inline uint8_t crc(const uint8_t* data,uint8_t length,const uint8_t* salts) {
  uint8_t value=data[1]^0xFF;
  for(uint8_t i=2;i<length;++i)
    value=pgm_read_byte(&P_L_CC_CRC_LUT_APV[value])^data[i];
  value=pgm_read_byte(&P_L_CC_CRC_LUT_APV[value])^
        pgm_read_byte(&salts[data[1]&0x0F]);
  return (uint8_t)~pgm_read_byte(&P_L_CC_CRC_LUT_APV[value]);
}
enum Slot : uint8_t {
  IGNITION,BACKLIGHT,LIGHTS,ESP20,ESP21,KOMBI_ESP,MOTOR14,GATEWAY,TSK,STEERING,
  MOTORCODE,MOTOR,COOLANT,ESP24,GEAR,RKA,AIRBAG,BLINKERS,PARKBRAKE,
  SPEEDLIMIT,TPMS,LIGHTREQUEST,REARLIGHTS,DOORS,OUTDOOR,POWER,HYBRID,RANGE,
  READY,PANAMERA_OILTEMP,PANAMERA_TPMS_MENU,PANAMERA_SIGNS,
  PANAMERA_LANE_KEEP,PANAMERA_LANE_CHANGE,CLOCK,CHECK_ENGINE,
  PANAMERA_WAKE,PANAMERA_KEEPALIVE,BCM_LIGHT_STATUS,STEERING_TAKEOVER_STATUS,SLOT_COUNT
};
} // namespace Taycan

class SHCustomProtocol {
 private:
  mcp2515_can canBus{Taycan::CS_PIN};
  bool tryQueueFrame(uint32_t id,uint8_t length,uint8_t* bytes) {
    // Cluster and comfort-bus VIN IDs discussed in r00li/CarCluster issue #102.
    if(id==0x6B4 || id==0x65F) return false;
    return canBus.trySendMsgBuf(id,0,0,length,bytes)==CAN_OK;
  }
  Taycan::State state;
  bool ready=false,haveData=false,stale=false;
  uint8_t nextSlot=0;
  uint8_t sequence[Taycan::SLOT_COUNT]={};
  uint32_t lastSent[Taycan::SLOT_COUNT]={};
  uint32_t lastData=0,lastDistance=0,lastLog=0;
  float distanceUnits=0;
  uint16_t queueErrors=0;
  uint8_t lightSupportSequence=0;
  bool supportRearPending=false;
  bool benchStandby() const {
    return TAYCAN_BENCH_ALWAYS_ON && !haveData;
  }
  Taycan::MfdButtons buttons;
  uint32_t refreshStarted=0;
  bool refreshActive=false;
  uint32_t lightQueued[3]={};
  uint8_t lastLightBytes[3][8]={};
  void debugNumber(long value) {
    char text[16]; ltoa(value,text,10); FlowSerialDebugPrintLn(text);
  }
  void refreshLights(uint32_t now) {
    if(TAYCAN_LIGHT_DIAGNOSTIC!=2) return;
    refreshStarted=now; refreshActive=true;
    const uint8_t slots[]={Taycan::LIGHTS,Taycan::LIGHTREQUEST,Taycan::REARLIGHTS};
    for(uint8_t i=0;i<3;++i) lastSent[slots[i]]=now-10;
  }

  float powerDisplayByte=0x40; // 0x40 neutral, 0x18 full recup, 0xEC full power
  uint32_t lastPowerStep=0;
  bool powerStepStarted=false;

  // Smooth only the gauge CAN byte, not the other gauges or binary indicators.
  // This runs on each POWER frame with elapsed-time compensation, so a
  // temporarily busy CAN controller will not cause one large needle jump.
  uint8_t smoothPower(uint32_t now) {
    const float request=(float)state.signedPowerPermille;
    const float target=request>=0.0f ?
      64.0f + request*(172.0f/1000.0f) :
      64.0f + request*(40.0f/1000.0f);
    if (!powerStepStarted) {
      powerStepStarted=true; lastPowerStep=now;
      // A cold start begins at 0 kW. Don't immediately jump to the first
      // nonzero throttle sample as the prior version did.
      return 0x40;
    }
    const uint32_t elapsed=now-lastPowerStep;
    lastPowerStep=now;
    // Cap long pauses (e.g. a suspended computer) rather than suddenly
    // snapping the gauge all the way across on the first frame after resume.
    const uint16_t dt=elapsed>100?100:(uint16_t)elapsed;
    const bool relaxing=(target>=64.0f && powerDisplayByte>target) ||
                         (target<=64.0f && powerDisplayByte<target);
    const float tau=relaxing?Taycan::POWER_RELEASE_MS:Taycan::POWER_ATTACK_MS;
    const float alpha=(float)dt/(tau+(float)dt);
    powerDisplayByte+=alpha*(target-powerDisplayByte);
    if (powerDisplayByte<24.0f) powerDisplayByte=24.0f;
    if (powerDisplayByte>236.0f) powerDisplayByte=236.0f;
    // Eliminate sub-byte neutral dithering after the needle settles.
    if (target==64.0f && fabsf(powerDisplayByte-64.0f)<0.49f)
      powerDisplayByte=64.0f;
    return (uint8_t)(powerDisplayByte+0.5f);
  }

  uint16_t period(uint8_t slot) {
    if(slot==Taycan::REARLIGHTS && supportRearPending) return 0;
    if(slot==Taycan::BCM_LIGHT_STATUS) return 100;
    if(slot==Taycan::STEERING_TAKEOVER_STATUS || slot==Taycan::PANAMERA_TPMS_MENU) return 100;
    if(Taycan::PANAMERA_971_WAKE) {
      if(slot==Taycan::IGNITION || slot==Taycan::PANAMERA_WAKE) return 10;
      if(slot==Taycan::PANAMERA_KEEPALIVE) return 50;
    }
    if(benchStandby()) {
      if(slot==Taycan::LIGHTREQUEST || slot==Taycan::REARLIGHTS) return 500;
      return 50;
    }
    if(slot==Taycan::CLOCK) return 1000;
    if(slot==Taycan::LIGHTS && TAYCAN_MLBEVO_FRONT_LIGHTS)
      return refreshActive?Taycan::FAST_MS:100;
    if(slot==Taycan::LIGHTREQUEST || slot==Taycan::REARLIGHTS) {
      if(TAYCAN_LIGHT_DIAGNOSTIC==2 && refreshActive) return 10;
      if(TAYCAN_LIGHT_DIAGNOSTIC) return 50;
    }
    if(slot==Taycan::READY || slot==Taycan::KOMBI_ESP || slot==Taycan::PARKBRAKE) return 100;
    if(slot==Taycan::CHECK_ENGINE) return 80;
    if(slot==Taycan::PANAMERA_LANE_KEEP) return 50;
    if(slot==Taycan::POWER) return Taycan::POWER_MS;
    if(slot==Taycan::HYBRID) return Taycan::HYBRID_MS;
    if(slot>=Taycan::TPMS) return Taycan::SLOW_MS;
    return Taycan::FAST_MS;
  }
  void seed(uint8_t* destination,const uint8_t* source,uint8_t length=8) {
    memcpy_P(destination,source,length);
  }
  void protect(uint8_t* data,uint8_t length,uint8_t header,const uint8_t* salts) {
    data[1]=header;
    data[0]=Taycan::crc(data,length,salts);
  }
  bool frame(uint8_t slot,uint32_t& id,uint8_t& length,uint8_t* b) {
    using namespace Taycan;
    length=8;
    const uint8_t seq=(slot==LIGHTREQUEST || slot==REARLIGHTS)?lightSupportSequence:sequence[slot];
    const uint16_t speedRaw=(uint32_t)state.speed*197UL/2;
    switch(slot) {
      case IGNITION:
        id=KLEMMEN_STATUS_01_ID;
        if(PANAMERA_971_WAKE) {
          const uint8_t donor[]={0x01,0x03,0x03,0x10,0,0,0,0};
          memcpy(b,donor,8); break;
        }
        length=4; seed(b,kStatusBuf,4);
        b[2]=TAYCAN_BENCH_ALWAYS_ON?BENCH_IGNITION_STATUS:ignitionStatusByte(state.key);
        protect(b,4,seq,P_L_CC_KENNUNG_APV_KlemmenStatus01); break;
      case PANAMERA_WAKE: {
        if(!PANAMERA_971_WAKE) return false;
        id=SECONDARY_IGNITION_ID;
        const uint8_t donor[]={0x12,0,0x8A,0xDD,0xF1,0x15,0x30,0x02};
        memcpy(b,donor,8); break;
      }
      case PANAMERA_KEEPALIVE: {
        if(!PANAMERA_971_WAKE) return false;
        id=SECONDARY_KEEPALIVE_ID;
        const uint8_t donor[]={0,0,0,0,0xFF,0xFF,0,0x10};
        memcpy(b,donor,8); break;
      }
      case BACKLIGHT: {
        id=DIMMUNG_01_ID; seed(b,dimmungBuf);
        const bool lampsOn=!benchStandby() &&
          (state.low || state.high || state.parking || state.fog || state.frontFog);
        b[0]=lampsOn?0x7F:0xFD;
        b[1]=b[2]=lampsOn?50:100;
        break;
      }
      case LIGHTS:
        id=LICHT_VORNE_01_ID; seed(b,lichtVorne01Buff);
        b[2]=(state.fog?REAR_FOG_ON:REAR_FOG_OFF)|(state.autoHigh?0x08:0);
        // Older MLB switch-position layout, bench candidate on 0x658:
        // Auto=2, position=3, dipped=4 in byte 5 bits 1..3.
        b[4]=(state.autoHigh?2:(state.low||state.high)?4:state.parking?3:1)<<1;
        b[5]=0x01;
        b[6]=0x02; // User's working 658 template.
        if(TAYCAN_MLBEVO_FRONT_LIGHTS) {
          const uint8_t header=seq | headlightByte(state.low,state.high,state.parking) |
            (state.frontFog?0x80:0);
          protect(b,8,header,LIGHT_FRONT_SALTS);
        } else {
          b[0]=0x34;
          b[1]=headlightByte(state.low,state.high,state.parking);
        }
        break;
      case ESP20:
        id=ESP_20_ID; seed(b,esp20Buf);
        protect(b,8,0x30|seq,P_L_CC_KENNUNG_APV_ESP20); break;
      case ESP21:
        id=ESP_21_ID; seed(b,esp21Buf); b[4]=speedRaw; b[5]=speedRaw>>8;
        b[2]=esp21StatusTemplate();
        if(USE_MQB_ESP21_STATUS_BITS) {
          // MQB DBC: ASR_Tastung_passiv bit 48, ESP_Tastung_passiv
          // bit 49, ESP_Eingriff bit 61. Interpretation of "passiv"
          // as OFF and Taycan applicability are NOT confirmed.
          b[6]=(b[6]&static_cast<uint8_t>(~0x03)) |
               ((state.tcOff||state.psmSport||state.psmOff)?0x01:0) |
               ((state.escOff||state.psmOff)?0x02:0);
          b[7]=(b[7]&static_cast<uint8_t>(~0x23)) |
               ((state.escActive||state.tcActive)?0x20:0) |
               (state.tcActive?0x02:0) | (state.absActive?0x01:0);
        }
        protect(b,8,esp21CounterHeader(seq),P_L_CC_KENNUNG_APV_ESP21); break;
      case KOMBI_ESP:
        // Kombi_01: DBC bit 0 ABS warning, bit 1 ESC warning.
        // This cluster-status path still needs a physical response test.
        if(!SEND_MQB_KOMBI_ESP_LAMP) return false;
        id=KOMBI_01_ID; memset(b,0,8); b[1]=seq;
        b[0]=((state.escOff||state.tcOff||state.psmOff||state.escActive||state.tcActive)?0x02:0) |
             (state.absWarning?0x01:0);
        break;
      case MOTOR14: id=MOTOR_14_ID; seed(b,motor14Buf); break;
      case GATEWAY: id=GATEWAY_76_ID; seed(b,gateway76Buf); break;
      case TSK:
        id=TSK_07_ID; seed(b,tsk07Buf);
        if(ENABLE_PANAMERA_CRUISE && !benchStandby()) {
          // 971 donor display encoding. 0x40 is the documented grey state.
          memset(b,0,8);
          b[0]=0x31;
          b[7]=0x40;
          if(state.cruise && state.cruiseSetKmh>0) {
            // SimHub sends km/h. This cluster displays the donor target in mph.
            const uint16_t targetMph=(state.cruiseSetKmh*100000UL+80467UL)/160934UL;
            b[1]=(targetMph+1)/3;
            b[2]=(targetMph*10UL+16)/32;
            b[7]=0x70;
          }
        } else protect(b,8,0xE0|seq,P_L_CC_KENNUNG_APV_TSK07);
        break;
      case STEERING:
        id=LH_EPS_01_ID; seed(b,lhEps01Buf);
        protect(b,8,seq,P_L_CC_KENNUNG_APV_LH_EPS01); break;
      case MOTORCODE:
        id=MOTOR_CODE_01_ID; seed(b,motorCode01Buf);
        protect(b,8,0x10|seq,P_L_CC_KENNUNG_APV_MCODE01); break;
      case MOTOR: {
        id=MOTOR_04_ID; seed(b,motor04Buf);
        const uint16_t raw=state.rpm/3; b[3]=raw; b[4]=raw>>8; break;
      }
      case COOLANT:
        id=MOTOR_09_ID; seed(b,motor09Buf);
        b[0]=map(state.coolant,50,130,0x80,0xED); break;
      case ESP24: {
        id=ESP_24_ID; seed(b,esp24Buf);
        const uint16_t wheel=(uint32_t)speedRaw*135UL/100;
        const uint16_t distance=SEND_DISTANCE?(uint16_t)distanceUnits:0;
        b[2]=wheel; b[3]=wheel>>8; b[5]=distance; b[6]=distance>>8;
        // ESP_24 candidate from the MEB/MLBevo-family DBC. The TC flag
        // occupies the lowest bit of the old wheel-speed word.
        b[2]=(b[2]&0xFE) | ((state.tcActive || state.tcOff || state.escOff || state.psmOff)?1:0);
        protect(b,8,seq|(state.escActive?0x10:0),P_L_CC_KENNUNG_APV_ESP24); break;
      }
      case GEAR:
        id=WBA_03_ID; seed(b,wba03Buf);
        if(state.gear>=1 && state.gear<=9) { b[1]=0x60; b[3]=state.gear; }
        else { b[1]=(state.gear-9)*0x10; b[3]=0; }
        protect(b,8,b[1]|seq,P_L_CC_KENNUNG_APV_WBA03); break;
      case RKA: id=RKA_01_ID; seed(b,rka01Buf); break;
      case AIRBAG:
        id=AIRBAG_01_ID; seed(b,airbag01Buf);
        protect(b,8,seq,P_L_CC_KENNUNG_APV_AIRBAG01); break;
      case BLINKERS:
        id=BLINKMODI_02_ID; seed(b,blinkerBuff); b[1]=seq;
        b[3]=(state.left?0x0A:0)|(state.right?0x14:0); break;
      case PARKBRAKE:
        id=PARKBRAKE_ID; length=TAYCAN_PANAMERA_EPB_DLC; memset(b,0,8);
        b[0]=state.handbrake?0x04:0;
        b[1]=state.autoHold?0x01:0; break;
      case SPEEDLIMIT: id=SPEEDLIMIT_ID; seed(b,speedlimit); b[3]=0x40; break;
      case TPMS: id=TPMS_ID; seed(b,tpmsBuff); break;
      case LIGHTREQUEST: {
        if(supportRearPending) return false;
        // 0x3D5 (Licht_Anf_01), VW MQB DBC:
        // byte 2 bit 6 (DBC start bit 14) parking request;
        // byte 2 bit 7 (15) dipped-beam request;
        // byte 3 bit 0 (16) high-beam request;
        // byte 3 bit 1 (17) front-fog request, bit 5 (21) rear-fog request.
        // Preserve the other source-template bits; CRC is regenerated below.
        id=LICHT_ANF_ID; seed(b,lichtAnfBuff);
        uint8_t requestHeader=0xC0|seq; // Original: parking/low always set.
        const bool useMlbevo=TAYCAN_MLBEVO_LIGHT_REQUEST && !benchStandby();
        if(useMlbevo) {
          // Header is written by protect(): set parking/low here, NOT in b[1]
          // (protect() would overwrite b[1] immediately afterward).
          const bool dipped=state.low || state.high;
          const bool position=state.parking || dipped;
          requestHeader=seq | (position?0x40:0) | (dipped?0x80:0);
          b[2]=(b[2]&static_cast<uint8_t>(~0x63)) |
               (state.high?0x01:0) | (state.frontFog?0x02:0) |
               (state.fog?0x20:0) | (position?0x40:0);
        }
        protect(b,8,requestHeader,useMlbevo?
          LIGHT_REQUEST_SALTS:P_L_CC_KENNUNG_APV_LICHT_ANF); break;
      }
      case REARLIGHTS:
        if(!supportRearPending) return false;
        // 0x3D6 (Licht_hinten_01), VW MQB DBC: byte 2 bit 0
        // rear parking lamps; byte 2 bit 4 rear fog. Retain other bits.
        id=LICHT_HINTEN_01_ID; seed(b,lichtHintenBuff); b[0]=seq;
        if(!benchStandby()) {
          b[1]=(b[1]&static_cast<uint8_t>(~0x11)) |
               ((state.parking||state.low||state.high)?0x01:0) | (state.fog?0x10:0);
        }
        break;
      case BCM_LIGHT_STATUS: {
        if(!TAYCAN_SEND_CLEAR_LAMP_STATUS) return false;
        // Most common captured Taycan 65A payload. MQB DBC light warning
        // bit 14 and warning text bits 38-39 are already clear in this frame.
        const uint8_t normal[]={0,0,0,0x10,0,0x2E,0,0};
        id=BCM_LIGHT_STATUS_ID;
        memcpy(b,normal,8); break;
      }
      case DOORS: id=DOOR_STATUS_ID; seed(b,doorStatusBuff);
        b[3]=ENABLE_PANAMERA_DOOR_MASK?state.doorMask:(state.door?1:0); break;
      case OUTDOOR: id=OUTDOOR_TEMP_ID; seed(b,outdoorTempBuff);
        // Donor anchor: 0x9A represents 21.5 C. Each count is 0.5 C.
        b[0]=bounded(111+2*state.outside,0,252); break;
      case CHECK_ENGINE:
        id=MOTOR_26_ID;
        // Observed normal Taycan payload; MO_OBD2_Lampe bit 49 from MQB DBC.
        memset(b,0,8); b[2]=0x10;
        b[6]=state.checkEngine?0x02:0; break;
      case POWER:
        id=POWERMETER_ID; seed(b,powermeter);
        b[7]=smoothPower(millis()); break;
      case HYBRID: id=HYBRID_ID; seed(b,hybrid); break;
      case RANGE: {
        id=RANGE_ID; seed(b,range);
        const uint8_t percent=state.batterySoc>=0?state.batterySoc:state.fuelPct;
        const uint16_t value=state.rangeKm>=0?state.rangeKm:
          (uint32_t)state.fullRangeKm*percent/100UL;
        b[2]=value; b[3]=value>>8; break;
      }
      case READY:
        if(!SEND_SECONDARY_READY || (!TAYCAN_BENCH_ALWAYS_ON && state.key<2)) return false;
        id=READY_ID; memset(b,0,8); b[1]=0x80; break;
      case PANAMERA_OILTEMP:
        if(!ENABLE_PANAMERA_OIL_FRAME) return false;
        id=PANAMERA_OILTEMP_ID;
        {const uint8_t donor[8]={0xC0,0x8A,0xAA,0xA8,0x7E,0x60,0x59,0x00};
         memcpy(b,donor,8);} // STATIC. There is no verified mapping for dynamic oil C/pressure.
        break;
      case PANAMERA_TPMS_MENU:
        id=PANAMERA_TPMS_MENU_ID; memset(b,0,8);
        b[2]=0x02; // Panamera static status with the warning lamp bits clear.
        break;
      case STEERING_TAKEOVER_STATUS:
        id=STEERING_TAKEOVER_STATUS_ID; memset(b,0,8);
        break;
      case PANAMERA_SIGNS:
        if(!ENABLE_PANAMERA_SIGN || state.speedLimitKmh<1) return false;
        id=PANAMERA_SIGNS_ID; memset(b,0,8);
        b[2]=bounded(state.speedLimitKmh,0,255); b[4]=1; b[5]=1; b[7]=1;
        break;
      case PANAMERA_LANE_KEEP: {
        if(!ENABLE_PANAMERA_LANEKEEP) return false;
        id=PANAMERA_LANE_KEEP_ID; memset(b,0,8);
        // 971 static sketch: both lane lines green.
        b[0]=b[1]=seq;
        b[4]=0xA0;
        break;
      }
      case PANAMERA_LANE_CHANGE:
        if(!ENABLE_PANAMERA_LANECHANGE || state.laneChange==0) return false;
        id=PANAMERA_LANE_CHANGE_ID; memset(b,0,8);
        // Donor 0x30F has documented 'unavailable' statuses, not a
        // proven green-active LCA icon. Only test mode 1=unavailable.
        b[2]=0x10; break;
      case CLOCK: {
        if(!TAYCAN_ENABLE_970_CLOCK || state.clockSeconds<0) return false;
        id=SECONDARY_CLOCK_ID; memset(b,0,8);
        const uint8_t hour=state.clockSeconds/3600;
        const uint8_t minute=(state.clockSeconds/60)%60;
        b[4]=seq; b[5]=(hour&15)<<4;
        b[6]=(minute<<1)|(hour>>4); b[7]=(state.clockSeconds%60)/2;
        break;
      }
      default: return false;
    }
    return true;
  }

  bool serviceButtons(uint32_t now) {
    if(!ready || !Taycan::ENABLE_MFD_BUTTONS) return false;
    buttons.poll(now);
    uint8_t buttonFrame[4];
    if(!buttons.frame(now,buttonFrame)) return false;
    if(!tryQueueFrame(Taycan::MFD_ID,4,buttonFrame)) {
      ++queueErrors;
      return false;
    }
    buttons.queued(now,buttonFrame);
    return true;
  }

 public:
  void setup() {
    pinMode(53,OUTPUT); digitalWrite(53,HIGH);
    buttons.setup();
    ready=canBus.begin(CAN_500KBPS,MCP_16MHz)==CAN_OK;
    if(ready) ready=canBus.setMode(MODE_NORMAL)==CAN_OK;
    FlowSerialDebugPrintLn(ready?"Taycan CAN ready":"Taycan CAN init failed");
    if(ready) {
      if(TAYCAN_BENCH_ALWAYS_ON) {
        FlowSerialDebugPrintLn(Taycan::PANAMERA_971_WAKE?
          "Taycan CS9: Panamera 971 startup profile":"Taycan CS9: continuous ignition 3C0/23");
        const uint32_t now=millis();
        lastSent[Taycan::IGNITION]=now-period(Taycan::IGNITION);
        lastSent[Taycan::LIGHTREQUEST]=now-period(Taycan::LIGHTREQUEST);
        lastSent[Taycan::PANAMERA_LANE_KEEP]=now-period(Taycan::PANAMERA_LANE_KEEP);
        if(Taycan::PANAMERA_971_WAKE) {
          lastSent[Taycan::PANAMERA_WAKE]=now-period(Taycan::PANAMERA_WAKE);
          lastSent[Taycan::PANAMERA_KEEPALIVE]=now-period(Taycan::PANAMERA_KEEPALIVE);
          lastSent[Taycan::READY]=now-period(Taycan::READY);
        }
      }
      FlowSerialDebugPrintLn(TAYCAN_MLBEVO_FRONT_LIGHTS?
        "Taycan v10: rolling MLBevo 658 CRC":"Taycan v10: legacy fixed 658");
    }
    lastDistance=millis();
  }
  void read() {
    String message=FlowSerialReadStringUntil('\n');
    message.trim();
    if(message.length()==0 || message.length()>=384) return;
    char buffer[384]; message.toCharArray(buffer,sizeof(buffer));
    // Retain the older protocols. v10 appends flags, range, SOC, temperature
    // and the charging state to the original 42 fields.
    const uint8_t extendedFieldCount=47;
    char* fields[extendedFieldCount]; fields[0]=buffer; uint8_t count=1;
    for(char* p=buffer;*p;++p) if(*p==';') {
      if(count>=extendedFieldCount) return;
      *p=0; fields[count++]=p+1;
    }
    if(count!=20 && count!=24 && count!=40 && count!=41 && count!=42 &&
       count!=extendedFieldCount) return;
    long n[extendedFieldCount]={};
    for(uint8_t i=0;i<count;++i) if(i!=2 && !Taycan::number(fields[i],n[i])) return;
    using namespace Taycan;
    state.rpm=bounded(bounded(n[0],0,MAX_RPM)*RPM_CORRECTION,0,MAX_RPM);
    state.speed=bounded(bounded(n[1],0,MAX_SPEED_KMH)*SPEED_CORRECTION,0,MAX_SPEED_KMH);
    state.gear=gearCode(fields[2]); state.coolant=bounded(n[3],50,130);
    const bool extended=count==extendedFieldCount;
    const uint8_t status=extended?bounded(n[42],0,31):0;
    const bool lightChanged=!haveData || stale || state.key!=bounded(n[4],0,3) ||
      state.high!=(n[5]!=0) || state.fog!=(n[6]!=0) || state.low!=(n[17]!=0) || state.parking!=(n[18]!=0) ||
      state.autoHigh!=((status&1)!=0) || state.frontFog!=((status&16)!=0);
    const bool brakeChanged=state.handbrake!=(n[9]!=0) || state.autoHold!=((status&2)!=0);
    const bool engineChanged=state.checkEngine!=(n[15]!=0);
    const bool absWarningChanged=state.absWarning!=((status&8)!=0);
    state.key=bounded(n[4],0,3); state.high=n[5]!=0; state.fog=n[6]!=0;
    state.left=n[7]!=0; state.right=n[8]!=0; state.handbrake=n[9]!=0;
    state.throttle=bounded(n[10],0,100); state.charge=bounded(n[11],0,100);
    // Old formula fallback: positive throttle only. The previous firmware
    // *invented* regeneration from road speed while coasting. Remove that.
    state.signedPowerPermille=count>=41?
      bounded(n[40],-1000,1000):(int16_t)state.throttle*10;
    state.outside=bounded(n[12],-55,70); state.brightness=bounded(n[13],0,100);
    state.door=n[14]!=0; state.checkEngine=n[15]!=0; state.cruise=n[16]!=0;
    state.low=n[17]!=0; state.parking=n[18]!=0;
    state.autoHigh=status&1; state.autoHold=status&2;
    state.absActive=status&4; state.absWarning=status&8; state.frontFog=status&16;
    state.rangeKm=extended?bounded(n[43],-1,2000):-1;
    state.batterySoc=extended?bounded(n[44],-1,100):-1;
    state.batteryTempTenths=extended?bounded(n[45],-1000,1500):-1000;
    state.charging=extended?bounded(n[46],-1,1):-1;
    // Missing extension values from legacy 20-field formula default OFF.
    state.escOff=count>=24 && n[20]!=0;
    state.tcOff=count>=24 && n[21]!=0;
    state.escActive=count>=24 && n[22]!=0;
    state.tcActive=count>=24 && n[23]!=0;
    if(count>=40) {
      state.cruiseSetKmh=bounded(n[24],0,260);
      state.fuelPct=bounded(n[25],0,100);
      state.fullRangeKm=bounded(n[26],0,2000);
      state.speedLimitKmh=bounded(n[27],-1,255);
      state.oilTempC=bounded(n[28],-99,200);
      state.oilPressureKpa=bounded(n[29],-1,2000);
      for(uint8_t i=0;i<4;i++) state.tpmsKpa[i]=bounded(n[30+i],-1,1000);
      state.tpmsWarning=n[34]!=0;
      state.laneKeep=bounded(n[35],0,4);
      state.laneChange=bounded(n[36],0,1);
      state.doorMask=bounded(n[37],0,255);
      state.psmSport=n[38]!=0; state.psmOff=n[39]!=0;
    } else {
      state.cruiseSetKmh=0; state.fuelPct=state.charge;
      state.fullRangeKm=500; state.speedLimitKmh=-1;
      state.oilTempC=-99; state.oilPressureKpa=0;
      state.tpmsWarning=false;
      for(uint8_t i=0;i<4;i++)state.tpmsKpa[i]=-1;
      state.laneKeep=0;state.laneChange=0;state.doorMask=state.door?1:0;
      state.psmSport=false;state.psmOff=false;
    }
    state.clockSeconds=count>=42 && n[41]>=0 && n[41]<86400?n[41]:-1;
    if(brakeChanged) lastSent[Taycan::PARKBRAKE]=millis()-period(Taycan::PARKBRAKE);
    if(engineChanged) lastSent[Taycan::CHECK_ENGINE]=millis()-period(Taycan::CHECK_ENGINE);
    if(absWarningChanged) lastSent[Taycan::KOMBI_ESP]=millis()-period(Taycan::KOMBI_ESP);
    if(lightChanged) {
      refreshLights(millis());
      lastSent[Taycan::LIGHTS]=millis()-period(Taycan::LIGHTS);
      if(TAYCAN_LIGHT_DIAGNOSTIC) {
        char log[100];
        snprintf(log,sizeof(log),"INPUT t=%lu key=%u low=%u high=%u fog=%u park=%u brake=%u ESCoff=%u TCoff=%u",
          (unsigned long)millis(),state.key,state.low,state.high,state.fog,state.parking,state.handbrake,state.escOff,state.tcOff);
        FlowSerialDebugPrintLn(log);
      }
    }
    if(!haveData || stale) {
      lastDistance=millis();
      FlowSerialDebugPrintLn("Taycan: valid SimHub telemetry received");
    }
    lastData=millis(); haveData=true; stale=false;
  }
  void loop() {
    if(!ready) return;
    const uint32_t now=millis();
    if(serviceButtons(now)) return;
    if(!haveData && !TAYCAN_BENCH_ALWAYS_ON) return;
    if(refreshActive && (uint32_t)(now-refreshStarted)>=2000) refreshActive=false;
    if(haveData && (uint32_t)(now-lastData)>Taycan::TIMEOUT_MS && !stale) {
      state=Taycan::State(); stale=true; distanceUnits=0; refreshLights(now);
      // Continue neutral frames after a dropout so the cluster receives
      // cleared indicators and zero speed even if the last input was active.
      FlowSerialDebugPrintLn("Taycan: SimHub timed out, telemetry reset");
    }
    const uint32_t elapsed=now-lastDistance;
    if(elapsed>=Taycan::FAST_MS) {
      lastDistance=now;
      if(!state.speed || !Taycan::SEND_DISTANCE) distanceUnits=0;
      else {
        // Same units as supplied ESP24, scaled to real elapsed time.
        const uint16_t raw=(uint32_t)state.speed*197UL/2;
        distanceUnits+=(raw/100.0f)*0.621371f*71950.0f*(elapsed/3600000.0f);
        if(distanceUnits>=30000.0f) distanceUnits=fmod(distanceUnits,30000.0f);
      }
    }
    // One nonblocking attempt per loop; preserve each frame's own counter.
    for(uint8_t checked=0;checked<Taycan::SLOT_COUNT;++checked) {
      const uint8_t slot=nextSlot; nextSlot=(nextSlot+1)%Taycan::SLOT_COUNT;
      // Send bench startup frames even before the first game packet.
      if(benchStandby() && slot!=Taycan::IGNITION &&
         slot!=Taycan::GATEWAY && slot!=Taycan::MOTOR14 && slot!=Taycan::TSK &&
         slot!=Taycan::STEERING && slot!=Taycan::LIGHTREQUEST &&
         slot!=Taycan::REARLIGHTS && slot!=Taycan::PANAMERA_LANE_KEEP &&
         slot!=Taycan::BACKLIGHT && slot!=Taycan::PARKBRAKE && slot!=Taycan::STEERING_TAKEOVER_STATUS &&
         slot!=Taycan::PANAMERA_TPMS_MENU &&
         !(Taycan::PANAMERA_971_WAKE && (slot==Taycan::PANAMERA_WAKE ||
           slot==Taycan::PANAMERA_KEEPALIVE || slot==Taycan::READY))) continue;
      if((uint32_t)(now-lastSent[slot])<period(slot)) continue;
      uint32_t id; uint8_t length; uint8_t bytes[8]={};
      if(!frame(slot,id,length,bytes)) { lastSent[slot]=now; continue; }
      if(tryQueueFrame(id,length,bytes)) {
        lastSent[slot]=now;
        if(slot==Taycan::LIGHTREQUEST) supportRearPending=true;
        if(slot==Taycan::REARLIGHTS) {
          supportRearPending=false;
          lightSupportSequence=(lightSupportSequence+1)&0x0F;
        }
        sequence[slot]=(slot==Taycan::CLOCK || slot==Taycan::PANAMERA_LANE_KEEP)?
          uint8_t(sequence[slot]+1):(sequence[slot]+1)&0x0F;
        if(TAYCAN_LIGHT_DIAGNOSTIC && (slot==Taycan::LIGHTS || slot==Taycan::LIGHTREQUEST || slot==Taycan::REARLIGHTS)) {
          uint8_t i=slot==Taycan::LIGHTS?0:(slot==Taycan::LIGHTREQUEST?1:2);
          if(i==0 && (lightQueued[i]==0 || ((lastLightBytes[i][1]^bytes[1])&0xF0) ||
                      lastLightBytes[i][2]!=bytes[2] || lastLightBytes[i][4]!=bytes[4])) {
            char log[110];
            snprintf(log,sizeof(log),"LIGHT QUEUED 658 t=%lu data=%02X %02X %02X %02X %02X %02X %02X %02X",
              (unsigned long)now,bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);
            FlowSerialDebugPrintLn(log);
          }
          ++lightQueued[i]; memcpy(lastLightBytes[i],bytes,8);
        }
      } else ++queueErrors;
      break;
    }
    if(TAYCAN_LIGHT_DIAGNOSTIC && (uint32_t)(now-lastLog)>=5000) {
      lastLog=now;
      FlowSerialDebugPrintLn("Taycan diag: millis, mode, stale, limit kmh, clock seconds, queue failures");
      debugNumber(now); debugNumber(TAYCAN_LIGHT_DIAGNOSTIC);
      debugNumber(stale); debugNumber(state.speedLimitKmh);
      debugNumber(state.clockSeconds); debugNumber(queueErrors);
      FlowSerialDebugPrintLn("Taycan diag: SOC %, battery temp tenths C, charging (0/1)");
      debugNumber(state.batterySoc); debugNumber(state.batteryTempTenths); debugNumber(state.charging);
      FlowSerialDebugPrintLn("Taycan diag: parking brake, tyre kPa FL FR RL RR (-1 unavailable)");
      debugNumber(state.handbrake);
      for(uint8_t i=0;i<4;++i) debugNumber(state.tpmsKpa[i]);
      FlowSerialDebugPrintLn("Taycan diag: cruise active, cruise target km/h, game speed km/h");
      debugNumber(state.cruise); debugNumber(state.cruiseSetKmh); debugNumber(state.speed);
      const char* ids[]={"0x658","0x3D5","0x3D6"};
      for(uint8_t i=0;i<3;++i) {
        FlowSerialDebugPrintLn(ids[i]); debugNumber(lightQueued[i]);
        for(uint8_t j=0;j<8;++j) debugNumber(lastLightBytes[i][j]);
      }
    }
    if(!TAYCAN_LIGHT_DIAGNOSTIC && queueErrors && (uint32_t)(now-lastLog)>=5000) {
      lastLog=now; queueErrors=0;
      FlowSerialDebugPrintLn("Taycan CAN queue busy: check bitrate/wiring/ACK");
    }
  }
  void idle() { serviceButtons(millis()); }
};
#endif
