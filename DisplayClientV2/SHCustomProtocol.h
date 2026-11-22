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
#define TAYCAN_LIGHT_DIAGNOSTIC 0
#endif
#ifndef TAYCAN_ENABLE_970_CLOCK
#define TAYCAN_ENABLE_970_CLOCK 1
#endif
#ifndef TAYCAN_SEND_BMS_FRAMES
#define TAYCAN_SEND_BMS_FRAMES 1
#endif
const uint8_t CS_PIN = 10;
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
const bool ENABLE_PANAMERA_SIGN = false;  // Taycan sign output uses the bench-mapped 0x181.
const bool ENABLE_PANAMERA_LANEKEEP = true;
const bool ENABLE_PANAMERA_LANECHANGE = true;// 0x30F nearby traffic / blind-spot mirror icon.
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
const uint8_t REAR_FOG_OFF = 0x00;
const uint8_t REAR_FOG_ON = 0x01; // User working low+fog payload.
// Isolated cluster: emit the captured BCM_01 status with lamp warnings clear.
#ifndef TAYCAN_SEND_CLEAR_LAMP_STATUS
#define TAYCAN_SEND_CLEAR_LAMP_STATUS 1
#endif
const uint16_t BCM_LIGHT_STATUS_ID=0x65A;
const bool SEND_MQB_KOMBI_ESP_LAMP = true;  // 0x30B byte 1 bit 1
const bool USE_MQB_ESP21_STATUS_BITS = true; // 0x0FD byte 7 bits 0/1; byte 8 bit 5
inline constexpr uint8_t headlightByte(bool low,bool high,bool parking) {
  return (high?HEADLIGHT_HIGH:0) | ((low||high)?HEADLIGHT_LOW:HEADLIGHT_OFF) |
         (parking?0x10:0);
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
  int16_t signedPowerPermille=0;
  bool high=false,fog=false,left=false,right=false,handbrake=false,door=false;
  bool checkEngine=false,cruise=false,low=false,parking=false;
  bool autoLights=false,autoHigh=false,autoWhite=false,autoHold=false,absActive=false,absWarning=false,frontFog=false,noOvertake=false;
  bool combinedAutoLow=false;
  bool brakeWarning=false;
  bool isTruck=false;
  uint8_t driveMode=0; // 0=Normal, 1=Range/Eco, 2=Sport, 3=Sport Plus
  uint8_t signType=0;  // Optional TaycanSignType override (0..15)
  int16_t rangeKm=-1;
  int16_t batterySoc=-1, batteryTempTenths=-1000;
  int8_t charging=-1;
  bool escOff=false,tcOff=false,escActive=false,tcActive=false;
  bool psmSport=false, psmOff=false, tpmsWarning=false;
  uint16_t cruiseSetKmh=0, fullRangeKm=996;
  int16_t oilPressureKpa=-1;
  int16_t tpmsKpa[4]={-1,-1,-1,-1};
  int16_t speedLimitKmh=-1, prevSpeedLimitKmh=-1, oilTempC=-99;
  uint32_t limitClearedAt=0;
  uint8_t laneKeep=0, laneChange=0, doorMask=0;
  uint8_t fuelPct=0;
};

enum TaycanSignType : uint8_t {
  SIGN_NONE = 0,
  SIGN_NO_LIMIT = 1,
  SIGN_NO_OVERTAKE_CAR = 2,
  SIGN_NO_OVERTAKE_TRUCK = 3,
  SIGN_CITY_ENTER = 4,
  SIGN_CITY_EXIT = 5,
  SIGN_DO_NOT_ENTER = 6,
  SIGN_PRIORITY_ROAD = 7,
  SIGN_PRIORITY_END = 8,
  SIGN_HIGHWAY_BEGIN = 9,
  SIGN_HIGHWAY_END = 10,
  SIGN_MOTORWAY_CAR = 11,
  SIGN_MOTORWAY_CAR_END = 12,
  SIGN_LIVING_STREET = 13,
  SIGN_LEFT_TURN = 14
};

// Encodes 0x181 per verified cluster icon map:
// Byte 1 = style (0x00=round km/h, 0x01/0x02=yellow rect, 0x03=round no label, 0x05=yellow rect no label)
// Byte 2..3 = little-endian (code << 3), Byte 6 = 0x01 (hide 3 dots) / 0x02 (show 3 dots).
inline void buildSignFrame0x181(
    uint8_t out[8],
    int16_t speedLimitKmh,
    bool crossedOut,
    uint8_t styleByte,
    uint8_t extraSign,
    bool showThreeDots) {
  memset(out,0,8);
  out[0]=styleByte;
  out[5]=showThreeDots?0x02:0x01;
  uint16_t raw=0;
  if(extraSign!=SIGN_NONE) {
    out[0]=0x00;
    switch(extraSign) {
      case SIGN_NO_LIMIT:          raw=0x0340; break;
      case SIGN_NO_OVERTAKE_CAR:   raw=0x0348; break;
      case SIGN_NO_OVERTAKE_TRUCK: raw=0x0350; break;
      case SIGN_CITY_ENTER:        raw=0x0370; break;
      case SIGN_CITY_EXIT:         raw=0x0378; break;
      case SIGN_DO_NOT_ENTER:      raw=0x0380; break;
      case SIGN_PRIORITY_ROAD:     raw=0x03A0; break;
      case SIGN_PRIORITY_END:      raw=0x03A8; break;
      case SIGN_HIGHWAY_BEGIN:     raw=0x03B0; break;
      case SIGN_HIGHWAY_END:       raw=0x03B8; break;
      case SIGN_MOTORWAY_CAR:      raw=0x03C0; break;
      case SIGN_MOTORWAY_CAR_END:  raw=0x03C8; break;
      case SIGN_LIVING_STREET:     raw=0x03D0; break;
      case SIGN_LEFT_TURN:         raw=0x03FF; break;
      default:                     raw=0x0000; break;
    }
  } else if(speedLimitKmh>0) {
    uint16_t snapped=(uint16_t)(((speedLimitKmh+2)/5)*5);
    if(snapped<5) snapped=5;
    if(snapped>250) snapped=250;
    if(crossedOut) {
      raw=(snapped==5)?0x019F:(uint16_t)((50+(snapped/5))<<3);
    } else {
      if(snapped<10) snapped=10;
      raw=(uint16_t)((snapped/5)<<3);
    }
  }
  out[1]=(uint8_t)(raw&0xFF);
  out[2]=(uint8_t)((raw>>8)&0xFF);
}

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
  IGNITION,BACKLIGHT,LIGHTS,ESP03,ESP20,ESP21,KOMBI_ESP,MOTOR14,GATEWAY,TSK,STEERING,
  LWI,LH_EPS3,EPB01,ESP05,LWR_AFS,
  BMS01,BMS07,BMS14,HVK01,HVLM04,DCDC01,CHARISMA,ACC02,
  MOTORCODE,MOTOR,MOTOR18,COOLANT,ESP24,GEAR,RKA,AIRBAG,BLINKERS,PARKBRAKE,
  SPEEDLIMIT,TPMS,TPMS_BAP_P,TPMS_BAP_T,TPMS_BAP_SPEC,LIGHTREQUEST,REARLIGHTS,DOORS,OUTDOOR,POWER,HYBRID,RANGE,
  READY,PANAMERA_OILTEMP,PANAMERA_TPMS_MENU,PANAMERA_SIGNS,
  PANAMERA_LANE_KEEP,PANAMERA_LANE_CHANGE,CLOCK,CHECK_ENGINE,SEATBELT,SOUND_OFF,
  PANAMERA_WAKE,PANAMERA_KEEPALIVE,BCM_LIGHT_STATUS,STEERING_TAKEOVER_STATUS,SUSPENSION_STATUS,SLOT_COUNT
};
} // namespace Taycan

class SHCustomProtocol {
 private:
  mcp2515_can canBus{Taycan::CS_PIN};
  bool tryQueueFrame(uint32_t id,uint8_t length,uint8_t* bytes,bool ext=false) {
    // Cluster and comfort-bus VIN IDs discussed in r00li/CarCluster issue #102.
    if(!ext && (id==0x6B4 || id==0x65F)) return false;
    return canBus.trySendMsgBuf(id,ext?1:0,0,length,bytes)==CAN_OK;
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

  uint8_t smoothPower(uint32_t now) {
    const float request=(float)state.signedPowerPermille;
    const float target=request>=0.0f ?
      64.0f + request*(172.0f/1000.0f) :
      64.0f + request*(40.0f/1000.0f);
    if (!powerStepStarted) {
      powerStepStarted=true; lastPowerStep=now;
      return 0x40;
    }
    const uint32_t elapsed=now-lastPowerStep;
    lastPowerStep=now;
    const uint16_t dt=elapsed>100?100:(uint16_t)elapsed;
    const bool relaxing=(target>=64.0f && powerDisplayByte>target) ||
                         (target<=64.0f && powerDisplayByte<target);
    const float tau=relaxing?Taycan::POWER_RELEASE_MS:Taycan::POWER_ATTACK_MS;
    const float alpha=(float)dt/(tau+(float)dt);
    powerDisplayByte+=alpha*(target-powerDisplayByte);
    if (powerDisplayByte<24.0f) powerDisplayByte=24.0f;
    if (powerDisplayByte>236.0f) powerDisplayByte=236.0f;
    if (target==64.0f && fabsf(powerDisplayByte-64.0f)<0.49f)
      powerDisplayByte=64.0f;
    return (uint8_t)(powerDisplayByte+0.5f);
  }

  uint16_t period(uint8_t slot) {
    if(slot==Taycan::SUSPENSION_STATUS) return 100;
    if(slot==Taycan::REARLIGHTS) return 100;
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
    if(slot==Taycan::READY || slot==Taycan::KOMBI_ESP || slot==Taycan::PARKBRAKE ||
       slot==Taycan::EPB01 || slot==Taycan::ESP05 || slot==Taycan::LWR_AFS ||
       slot==Taycan::BMS01 || slot==Taycan::BMS07 || slot==Taycan::BMS14 ||
       slot==Taycan::HVK01 || slot==Taycan::DCDC01 || slot==Taycan::CHARISMA ||
       slot==Taycan::SPEEDLIMIT || slot==Taycan::PANAMERA_SIGNS) return 100;
    if(slot==Taycan::LWI || slot==Taycan::LH_EPS3 || slot==Taycan::ACC02 || slot==Taycan::ESP03) return 50;
    if(slot==Taycan::HVLM04 || slot==Taycan::TPMS_BAP_P ||
       slot==Taycan::TPMS_BAP_T || slot==Taycan::TPMS_BAP_SPEC ||
       slot==Taycan::MOTOR18 || slot==Taycan::SEATBELT || slot==Taycan::SOUND_OFF) return 500;
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
  bool launchControlActive() const {
    const bool inDrive=(state.gear==13 || (state.gear>=1 && state.gear<=9));
    const bool inSport=(state.driveMode==2 || state.driveMode==3 || state.psmSport);
    return inDrive && inSport && (state.escOff || state.psmOff) &&
           state.speed==0 && state.throttle>0 && state.key>=2;
  }
  bool frame(uint8_t slot,uint32_t& id,uint8_t& length,uint8_t* b,bool& ext) {
    using namespace Taycan;
    length=8; ext=false;
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
        b[2]=benchStandby()?BENCH_IGNITION_STATUS:ignitionStatusByte(state.key);
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
        b[2]=state.fog?REAR_FOG_ON:REAR_FOG_OFF;
        b[5]=0x01;
        b[6]=0x02;
        if(TAYCAN_MLBEVO_FRONT_LIGHTS) {
          const bool beamsOn=state.low||state.high;
          const bool showAuto=state.autoLights||state.autoHigh;
          uint8_t header=headlightByte(state.low,state.high,state.parking||beamsOn);
          if(showAuto) {
            // Single-sender bench results: AUTO uses byte 4 = 0x08 and byte 5 = 0x00.
            // Byte 3 carries the same rear-fog bit in both display profiles.
            b[3]=0x08;
            b[4]=0x00;
            header=state.high?0x70:state.autoWhite?0x00:0x20;
            if(state.combinedAutoLow && state.low && !state.high && !state.autoWhite)
              header|=0x10; // Trial: 0x30 with the confirmed AUTO payload and no defect bits.
          } else {
            b[4]=(beamsOn?4:state.parking?3:1)<<1;
          }
          protect(b,8,header|seq,LIGHT_FRONT_SALTS);
        } else {
          const bool autoRequested=state.autoLights||state.autoHigh;
          b[4]=(autoRequested?2:(state.low||state.high)?4:state.parking?3:1)<<1;
          if(autoRequested) b[2]|=0x08; // Legacy donor assist bit.
          b[0]=0x34;
          if(state.high)                                          b[1]=0x20; // Blue Auto High-Beam
          else if(state.low)                                      b[1]=0x30; // Green Auto High-Beam / Low-Beam
          else if(state.parking||state.autoLights||state.autoHigh) b[1]=0x28; // White Auto Standby
          else                                                    b[1]=0x00; // Off
        }
        break;
      case ESP03:
        id=0x103; memset(b,0,8);
        if(state.escOff || state.tcOff || state.psmOff) {
          memset(b,0xFF,8);
          b[1]=seq;
        }
        break;
      case ESP20:
        id=ESP_20_ID; seed(b,esp20Buf);
        b[3]=(state.escOff || state.tcOff || state.psmOff)?0x01:0x00;
        protect(b,8,0x30|seq,P_L_CC_KENNUNG_APV_ESP20); break;
      case SUSPENSION_STATUS:
        id=0x2C1; memset(b,0,8);
        // Isolated candidates for the user's blue/white suspension display.
        // 0xA8 also raised an aquaplaning warning and is deliberately absent.
        // CRC salt for this ID is unavailable, so this is a raw bench-derived trial.
        if(state.key>=2 && !(state.escOff || state.psmOff)) {
          b[3]=(state.driveMode==2 || state.driveMode==3)?0x08:0x04;
        }
        break;
      case ESP21:
        id=ESP_21_ID; seed(b,esp21Buf); b[4]=speedRaw; b[5]=speedRaw>>8;
        b[2]=esp21StatusTemplate();
        if(USE_MQB_ESP21_STATUS_BITS) {
          const bool anyEscTcOff=state.escOff || state.tcOff || state.psmOff;
          b[6]=(b[6]&static_cast<uint8_t>(~0x0F)) |
               ((anyEscTcOff||state.psmSport)?0x01:0) |
               (anyEscTcOff?0x02:0) |
               (state.psmSport?0x04:0) |
               (anyEscTcOff?0x08:0);
          b[7]=(b[7]&static_cast<uint8_t>(~0x23)) |
               ((state.escActive||state.tcActive)?0x20:0) |
               (state.tcActive?0x02:0) | (state.absActive?0x01:0);
        }
        protect(b,8,esp21CounterHeader(seq),P_L_CC_KENNUNG_APV_ESP21); break;
      case KOMBI_ESP:
        if(!SEND_MQB_KOMBI_ESP_LAMP) return false;
        id=KOMBI_01_ID; memset(b,0,8); b[1]=seq;
        b[0]=((state.escOff||state.tcOff||state.psmOff||state.escActive||state.tcActive)?0x02:0) |
             ((state.absActive || state.absWarning)?0x01:0);
        break;
      case MOTOR14:
        id=MOTOR_14_ID; seed(b,motor14Buf);
        if(launchControlActive()) b[6]|=0x01;
        break;
      case GATEWAY: id=GATEWAY_76_ID; seed(b,gateway76Buf); break;
      case ACC02:
        // Driver Assistance / ACC radar & distance bars in left gauge (CRC-protected).
        id=ACC_02_ID; memset(b,0,8);
        b[2]=state.cruise?0x03:0x01; // 0x01=ACC standby/road lines, 0x03=active
        b[3]=0x03;                   // Distance bar setting (3 bars)
        b[4]=state.laneChange>0?0x01:0x00;
        protect(b,8,seq,P_L_CC_KENNUNG_APV_ACC02); break;
      case TSK:
        id=TSK_07_ID; seed(b,tsk07Buf);
        if(ENABLE_PANAMERA_CRUISE && !benchStandby()) {
          memset(b,0,8);
          b[0]=0x31;
          b[7]=0x40;
          if(state.cruise && state.cruiseSetKmh>0) {
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
      case LWI:
        id=LWI_01_ID; seed(b,lwi01Buf);
        protect(b,8,seq,P_L_CC_KENNUNG_APV_LWI01); break;
      case LH_EPS3:
        id=LH_EPS_03_ID; seed(b,lhEps03Buf);
        protect(b,8,seq,P_L_CC_KENNUNG_APV_LHEPS03); break;
      case EPB01:
        id=EPB_01_ID; seed(b,epb01Buf);
        b[2]=state.handbrake?0x01:0x00;
        b[3]=state.autoHold?0x02:0x00;
        protect(b,8,seq,P_L_CC_KENNUNG_APV_EPB01); break;
      case ESP05:
        id=ESP_05_ID; seed(b,esp05Buf);
        b[2]=state.autoHold?0x01:0x00;
        b[3]=state.handbrake?0x01:0x00;
        b[4]=(state.escOff||state.psmOff)?0x02:0x00;
        protect(b,8,seq,P_L_CC_KENNUNG_APV_ESP05); break;
      case LWR_AFS:
        id=LWR_AFS_01_ID; seed(b,lwrAfs01Buf); break;
      case BMS01: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=BMS_01_ID;
        const uint8_t captured[]={0,0,0x7F,0xDB,0xEB,0x5E,0x9F,0x22};
        memcpy(b,captured,8);
        // SOC is 11 bits at bit 47, factor 0.05 percent.
        // Preserve captured voltage/current and retain 67.10% if telemetry is absent.
        if(state.batterySoc>=0) {
          const uint16_t socRaw=(uint16_t)state.batterySoc*20;
          b[5]=(b[5]&0x7F)|((socRaw&1)<<7);
          b[6]=(socRaw>>1)&0xFF;
          b[7]=(b[7]&0xFC)|((socRaw>>9)&3);
        }
        protect(b,8,0xF0|seq,P_L_CC_KENNUNG_APV_BMS01); break;
      }
      case BMS07: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=BMS_07_ID;
        const uint8_t captured[]={0,0,0x3D,0x94,0x08,0x86,0xFF,0x03};
        memcpy(b,captured,8);
        // Preserve captured energy fields. Bit 23 is charging active.
        if(state.charging==1) b[2]|=0x80;
        protect(b,8,0xF0|seq,P_L_CC_KENNUNG_APV_BMS07); break;
      }
      case BMS14: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=BMS_14_ID;
        const uint8_t captured[]={0,0,0,0xF8,0xCD,0xA1,0xCF,0xBD};
        memcpy(b,captured,8);
        protect(b,8,seq,P_L_CC_KENNUNG_APV_BMS14); break;
      }
      case HVK01: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=HVK_01_ID;
        const uint8_t captured[]={0,0,0x14,0x11,0x02,0x02,0,0};
        memcpy(b,captured,8);
        // User-confirmed HV status field. Keep the captured network-active bit.
        b[5]=(b[5]&0xE7)|((state.key>=2 || benchStandby())?0x10:0x08);
        protect(b,8,seq,TAYCAN_HVK01_SALTS); break;
      }
      case HVLM04: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=HVLM_04_ID;
        const uint8_t captured[]={0,0,0,0x20,0,0,0x09,0};
        memcpy(b,captured,8);
        protect(b,8,0x60|seq,TAYCAN_HVLM04_SALTS); break;
      }
      case DCDC01: {
        if(!TAYCAN_SEND_BMS_FRAMES) return false;
        id=DCDC_01_ID;
        const uint8_t captured[]={0,0,0xBE,0xFE,0xF1,0x07,0,0x84};
        memcpy(b,captured,8);
        protect(b,8,seq,TAYCAN_DCDC01_SALTS); break;
      }
      case CHARISMA: {
        // Drive Mode (Charisma_01 0x597): 1=Normal, 2=Sport, 3=Sport+, 4=Range/Eco.
        id=CHARISMA_01_ID; memset(b,0,8);
        uint8_t modeByte=0x01; // Normal
        if(state.driveMode==1) modeByte=0x04;      // Range
        else if(state.driveMode==2) modeByte=0x02; // Sport
        else if(state.driveMode==3) modeByte=0x03; // Sport Plus
        b[0]=seq;
        b[1]=modeByte;
        b[2]=modeByte;
        break;
      }
      case MOTORCODE:
        id=MOTOR_CODE_01_ID; seed(b,motorCode01Buf);
        protect(b,8,0x10|seq,P_L_CC_KENNUNG_APV_MCODE01); break;
      case MOTOR: {
        id=MOTOR_04_ID; seed(b,motor04Buf);
        const uint16_t raw=state.rpm/3; b[3]=raw; b[4]=raw>>8;
        if(launchControlActive()) b[6]|=0x02;
        break;
      }
      case MOTOR18:
        id=MOTOR_18_ID; seed(b,motor18Buf); break;
      case COOLANT:
        id=MOTOR_09_ID; seed(b,motor09Buf);
        b[0]=map(state.coolant,50,130,0x80,0xED); break;
      case ESP24: {
        id=ESP_24_ID; seed(b,esp24Buf);
        const uint16_t wheel=(uint32_t)speedRaw*135UL/100;
        const uint16_t distance=SEND_DISTANCE?(uint16_t)distanceUnits:0;
        b[2]=wheel; b[3]=wheel>>8; b[5]=distance; b[6]=distance>>8;
        // Keep the eleven distance bits separate from ESP_Off_Lampe at bit 55.
        b[6]=(b[6]&0x07) | ((state.tcOff || state.escOff || state.psmOff)?0x80:0);
        b[2]=(b[2]&0xFE) | (state.tcActive?1:0);
        const uint8_t lamps=((state.escActive || state.tcActive)?0x10:0) |
                            ((state.absActive || state.absWarning)?0x20:0) |
                            (state.brakeWarning?0x40:0);
        protect(b,8,(seq&0x0F)|lamps,P_L_CC_KENNUNG_APV_ESP24); break;
      }
      case GEAR:
        id=WBA_03_ID; seed(b,wba03Buf);
        if((state.gear>=1 && state.gear<=9) || state.gear==13) { b[1]=0x40; b[3]=0x04; }
        else { b[1]=(state.gear-9)*0x10; b[3]=0; }
        if(launchControlActive()) b[5]|=0x04;
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
      case SPEEDLIMIT: {
        id=SPEEDLIMIT_ID;
        uint8_t extra=state.signType;
        int16_t limitToShow=state.speedLimitKmh;
        bool crossed=false;
        if(extra==SIGN_NONE) {
          if(state.noOvertake && (limitToShow<=0 || ((millis()/1500UL)&1))) {
            extra=state.isTruck?SIGN_NO_OVERTAKE_TRUCK:SIGN_NO_OVERTAKE_CAR;
          } else if(limitToShow==0) {
            if(state.prevSpeedLimitKmh>0 && (uint32_t)(millis()-state.limitClearedAt)<3000UL) {
              limitToShow=state.prevSpeedLimitKmh;
              crossed=true;
            } else {
              extra=SIGN_NO_LIMIT;
            }
          }
        }
        buildSignFrame0x181(b,limitToShow,crossed,0x00,extra,false);
        break;
      }
      case TPMS: id=TPMS_ID; seed(b,tpmsBuff); break;
      case TPMS_BAP_P: {
        id=TPMS_BAP_ID; ext=true;
        b[0]=0x31; b[1]=209; b[2]=0; // 0 = bar
        for(uint8_t i=0;i<4;++i) {
          if(state.tpmsKpa[i]>=0) b[3+i]=(uint8_t)bounded((state.tpmsKpa[i]+5)/10,0,250);
          else b[3+i]=benchStandby()?25:0xFF;
        }
        b[7]=0x00; break;
      }
      case TPMS_BAP_T: {
        id=TPMS_BAP_ID; ext=true;
        b[0]=0x31; b[1]=210; b[2]=0; // 0 = Celsius
        b[3]=b[4]=b[5]=b[6]=85;      // 25 C (value + 60)
        b[7]=0x00; break;
      }
      case TPMS_BAP_SPEC: {
        id=TPMS_BAP_ID; ext=true;
        b[0]=0x31; b[1]=211; b[2]=0;
        b[3]=b[4]=b[5]=b[6]=25;      // 2.5 bar specified target
        b[7]=0x00; break;
      }
      case LIGHTREQUEST: {
        id=LICHT_ANF_ID; seed(b,lichtAnfBuff);
        uint8_t requestHeader=0xC0|seq;
        const bool useMlbevo=TAYCAN_MLBEVO_LIGHT_REQUEST && !benchStandby();
        if(useMlbevo) {
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
        id=LICHT_HINTEN_01_ID; seed(b,lichtHintenBuff); b[0]=seq;
        if(!benchStandby()) {
          b[1]=(b[1]&static_cast<uint8_t>(~0x11)) |
               ((state.parking||state.low||state.high)?0x01:0) | (state.fog?0x10:0);
        }
        break;
      case BCM_LIGHT_STATUS: {
        if(!TAYCAN_SEND_CLEAR_LAMP_STATUS) return false;
        const uint8_t normal[]={0,0,0,0x10,0,0x2E,0,0};
        id=BCM_LIGHT_STATUS_ID;
        memcpy(b,normal,8); break;
      }
      case DOORS: id=DOOR_STATUS_ID; seed(b,doorStatusBuff);
        b[3]=ENABLE_PANAMERA_DOOR_MASK?state.doorMask:(state.door?1:0); break;
      case OUTDOOR: id=OUTDOOR_TEMP_ID; seed(b,outdoorTempBuff);
        b[0]=bounded(111+2*state.outside,0,252); break;
      case CHECK_ENGINE:
        id=MOTOR_26_ID;
        memset(b,0,8); b[2]=0x10;
        b[6]=state.checkEngine?0x04:0x00;
        b[7]=(state.driveMode==1)?0x40:((state.key>=2||benchStandby())?0x20:0x00);
        break;
      case SEATBELT:
        // 0x520 Bytes 6 & 7 from CAN_Panamera_971.ino: 0xF0/0x03 = green (2 buckled), 0x00/0x00 = red (OFF).
        id=SECONDARY_SEATBELT_ID; memset(b,0,8);
        if(benchStandby() || state.key>=2) { b[5]=0xF0; b[6]=0x03; }
        else { b[5]=0x00; b[6]=0x00; }
        break;
      case SOUND_OFF:
        id=SECONDARY_SOUND_ID; memset(b,0,8); b[1]=0xFF; break;
      case POWER:
        id=POWERMETER_ID; seed(b,powermeter);
        b[7]=smoothPower(millis()); break;
      case HYBRID:
        id=HYBRID_ID; seed(b,hybrid);
        if(state.driveMode==1) b[2]=0x04;      // Range
        else if(state.driveMode==2) b[2]=0x02; // Sport
        else if(state.driveMode==3) b[2]=0x03; // Sport+
        else b[2]=0x00;                        // Normal
        break;
      case RANGE: {
        id=RANGE_ID; seed(b,range);
        const uint8_t percent=state.batterySoc>=0?state.batterySoc:state.fuelPct;
        const uint16_t value=state.rangeKm>=0?state.rangeKm:
          (uint32_t)state.fullRangeKm*percent/100UL;
        b[2]=value; b[3]=value>>8; break;
      }
      case READY:
        if(!SEND_SECONDARY_READY || (!benchStandby() && state.key<2)) return false;
        id=READY_ID; memset(b,0,8);
        b[1]=state.autoHold?0x40:0x80; // 0x80=READY, 0x40=HOLD when Auto Hold active
        break;
      case PANAMERA_OILTEMP:
        if(!ENABLE_PANAMERA_OIL_FRAME) return false;
        id=PANAMERA_OILTEMP_ID;
        {const uint8_t donor[8]={0xC0,0x8A,0xAA,0xA8,0x7E,0x60,0x59,0x00};
         memcpy(b,donor,8);}
        break;
      case PANAMERA_TPMS_MENU:
        id=PANAMERA_TPMS_MENU_ID; memset(b,0,8);
        b[2]=state.tpmsWarning?0x06:0x02;
        break;
      case STEERING_TAKEOVER_STATUS:
        id=STEERING_TAKEOVER_STATUS_ID; memset(b,0,8);
        break;
      case PANAMERA_SIGNS:
        if(!ENABLE_PANAMERA_SIGN) return false;
        id=PANAMERA_SIGNS_ID; memset(b,0,8);
        if(state.speedLimitKmh>0 || state.noOvertake) {
          b[2]=state.speedLimitKmh>0?bounded(state.speedLimitKmh,0,255):0;
          b[4]=state.speedLimitKmh>0?1:0;
          b[5]=1;
          b[6]=state.noOvertake?1:0;
          b[7]=1;
        }
        break;
      case PANAMERA_LANE_KEEP: {
        if(!ENABLE_PANAMERA_LANEKEEP) return false;
        id=PANAMERA_LANE_KEEP_ID; memset(b,0,8);
        b[0]=b[1]=seq;
        uint8_t laneByte=0xA0;
        if(!benchStandby() && !state.cruise) {
          if(state.laneKeep==0) laneByte=0x50;
          else if(state.laneKeep==1) laneByte=0x90;
          else laneByte=0xA0; // Avoid red lane-crossing values (0xE0/0xB0/0x70/0xD0) that chime
        }
        b[4]=laneByte;
        break;
      }
      case PANAMERA_LANE_CHANGE:
        if(!ENABLE_PANAMERA_LANECHANGE) return false;
        id=PANAMERA_LANE_CHANGE_ID; memset(b,0,8);
        b[2]=state.laneChange>0?0x03:0x10; break;
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
    if(buttonFrame[0]!=0) {
      char report[64];
      snprintf(report,sizeof(report),"MFD CAN 5BF %02X %02X %02X %02X",
               buttonFrame[0],buttonFrame[1],buttonFrame[2],buttonFrame[3]);
      FlowSerialDebugPrintLn(report);
    }
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
    const uint32_t status=extended?bounded(n[42],0,0x3FFFFL):0;
    const bool lightChanged=!haveData || stale || state.key!=bounded(n[4],0,3) ||
      state.high!=(n[5]!=0) || state.fog!=(n[6]!=0) || state.low!=(n[17]!=0) || state.parking!=(n[18]!=0) ||
      state.autoHigh!=((status&1)!=0) || state.autoLights!=((status&0x4000)!=0) ||
      state.autoWhite!=((status&0x8000)!=0) || state.frontFog!=((status&16)!=0) ||
      state.combinedAutoLow!=((status&0x10000UL)!=0);
    const bool brakeChanged=state.handbrake!=(n[9]!=0) || state.autoHold!=((status&2)!=0);
    const bool engineChanged=state.checkEngine!=(n[15]!=0);
    const bool absWarningChanged=state.absWarning!=((status&8)!=0);
    state.key=bounded(n[4],0,3); state.high=n[5]!=0; state.fog=n[6]!=0;
    state.left=n[7]!=0; state.right=n[8]!=0; state.handbrake=n[9]!=0;
    state.throttle=bounded(n[10],0,100); state.charge=bounded(n[11],0,100);
    state.signedPowerPermille=count>=41?
      bounded(n[40],-1000,1000):(int16_t)state.throttle*10;
    state.outside=bounded(n[12],-55,70); state.brightness=bounded(n[13],0,100);
    state.door=n[14]!=0; state.checkEngine=n[15]!=0; state.cruise=n[16]!=0;
    state.low=n[17]!=0; state.parking=n[18]!=0;
    state.autoLights=(status&16384)!=0;
    state.autoWhite=(status&0x8000)!=0;
    state.combinedAutoLow=(status&0x10000UL)!=0;
    state.autoHigh=status&1; state.autoHold=status&2;
    state.absActive=status&4; state.absWarning=status&8;
    state.brakeWarning=(status&0x20000UL)!=0; state.frontFog=status&16;
    state.driveMode=(status>>5)&3;
    state.noOvertake=(status&128)!=0;
    state.isTruck=(status&256)!=0;
    state.signType=(uint8_t)((status>>9)&0x0F);
    state.rangeKm=extended?bounded(n[43],-1,996):-1;
    state.batterySoc=extended?bounded(n[44],-1,100):-1;
    state.batteryTempTenths=extended?bounded(n[45],-1000,1500):-1000;
    state.charging=extended?bounded(n[46],-1,1):-1;
    state.escOff=count>=24 && n[20]!=0;
    state.tcOff=count>=24 && n[21]!=0;
    state.escActive=count>=24 && n[22]!=0;
    state.tcActive=count>=24 && n[23]!=0;
    if(count>=40) {
      state.cruiseSetKmh=bounded(n[24],0,260);
      state.fuelPct=bounded(n[25],0,100);
      state.fullRangeKm=bounded(n[26],0,996);
      const int16_t newLimit=bounded(n[27],-1,255);
      if(state.speedLimitKmh>0 && newLimit==0) {
        state.prevSpeedLimitKmh=state.speedLimitKmh;
        state.limitClearedAt=millis();
      } else if(newLimit>0) {
        state.prevSpeedLimitKmh=newLimit;
      }
      state.speedLimitKmh=newLimit;
      state.oilTempC=bounded(n[28],-99,200);
      state.oilPressureKpa=bounded(n[29],-1,2000);
      for(uint8_t i=0;i<4;i++) state.tpmsKpa[i]=bounded(n[30+i],-1,1000);
      state.tpmsWarning=n[34]!=0;
      state.laneKeep=bounded(n[35],0,4);
      state.laneChange=bounded(n[36],0,1);
      state.doorMask=bounded(n[37],0,255);
      state.psmSport=n[38]!=0; state.psmOff=(n[39]!=0)||(state.escOff && state.tcOff);
    } else {
      state.cruiseSetKmh=0; state.fuelPct=state.charge;
      state.fullRangeKm=996; state.speedLimitKmh=-1;
      state.oilTempC=-99; state.oilPressureKpa=0;
      state.tpmsWarning=false;
      for(uint8_t i=0;i<4;i++)state.tpmsKpa[i]=-1;
      state.laneKeep=0;state.laneChange=0;state.doorMask=state.door?1:0;
      state.psmSport=false;state.psmOff=false;
    }
    state.clockSeconds=count>=42 && n[41]>=0 && n[41]<86400?n[41]:-1;
    if(brakeChanged) {
      lastSent[Taycan::PARKBRAKE]=millis()-period(Taycan::PARKBRAKE);
      lastSent[Taycan::EPB01]=millis()-period(Taycan::EPB01);
      lastSent[Taycan::ESP05]=millis()-period(Taycan::ESP05);
    }
    if(engineChanged) lastSent[Taycan::CHECK_ENGINE]=millis()-period(Taycan::CHECK_ENGINE);
    if(absWarningChanged) lastSent[Taycan::KOMBI_ESP]=millis()-period(Taycan::KOMBI_ESP);
    if(lightChanged) {
      refreshLights(millis());
      lastSent[Taycan::LIGHTS]=millis()-period(Taycan::LIGHTS);
      if(TAYCAN_LIGHT_DIAGNOSTIC) {
        char log[110];
        snprintf(log,sizeof(log),"LIGHT INPUT t=%lu key=%u low=%u high=%u auto=%u autoHigh=%u rearFog=%u frontFog=%u park=%u",
          (unsigned long)millis(),state.key,state.low,state.high,state.autoLights,state.autoHigh,state.fog,state.frontFog,state.parking);
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
      FlowSerialDebugPrintLn("Taycan: SimHub timed out, telemetry reset");
    }
    const uint32_t elapsed=now-lastDistance;
    if(elapsed>=Taycan::FAST_MS) {
      lastDistance=now;
      if(!state.speed || !Taycan::SEND_DISTANCE) distanceUnits=0;
      else {
        const uint16_t raw=(uint32_t)state.speed*197UL/2;
        distanceUnits+=(raw/100.0f)*0.621371f*71950.0f*(elapsed/3600000.0f);
        if(distanceUnits>=30000.0f) distanceUnits=fmod(distanceUnits,30000.0f);
      }
    }
    // Up to 4 nonblocking attempts per loop; preserve each frame's own counter.
    uint8_t sentThisLoop=0;
    for(uint8_t checked=0;checked<Taycan::SLOT_COUNT;++checked) {
      const uint8_t slot=nextSlot; nextSlot=(nextSlot+1)%Taycan::SLOT_COUNT;
      if(benchStandby() && slot!=Taycan::IGNITION &&
         slot!=Taycan::GATEWAY && slot!=Taycan::MOTOR14 && slot!=Taycan::TSK &&
         slot!=Taycan::STEERING && slot!=Taycan::LWI && slot!=Taycan::LH_EPS3 &&
         slot!=Taycan::EPB01 && slot!=Taycan::ESP05 && slot!=Taycan::LWR_AFS &&
         slot!=Taycan::BMS01 && slot!=Taycan::BMS07 && slot!=Taycan::BMS14 &&
         slot!=Taycan::HVK01 && slot!=Taycan::HVLM04 && slot!=Taycan::DCDC01 &&
         slot!=Taycan::CHARISMA && slot!=Taycan::ACC02 && slot!=Taycan::LIGHTS && slot!=Taycan::BCM_LIGHT_STATUS &&
         slot!=Taycan::LIGHTREQUEST && slot!=Taycan::REARLIGHTS &&
         slot!=Taycan::AIRBAG && slot!=Taycan::KOMBI_ESP &&
         slot!=Taycan::RKA && slot!=Taycan::TPMS && slot!=Taycan::MOTOR18 &&
         slot!=Taycan::ESP03 && slot!=Taycan::PANAMERA_SIGNS &&
         slot!=Taycan::SEATBELT && slot!=Taycan::SOUND_OFF &&
         slot!=Taycan::TPMS_BAP_P && slot!=Taycan::TPMS_BAP_T && slot!=Taycan::TPMS_BAP_SPEC &&
         slot!=Taycan::PANAMERA_LANE_KEEP &&
         slot!=Taycan::BACKLIGHT && slot!=Taycan::PARKBRAKE && slot!=Taycan::STEERING_TAKEOVER_STATUS &&
         slot!=Taycan::PANAMERA_TPMS_MENU &&
         !(Taycan::PANAMERA_971_WAKE && (slot==Taycan::PANAMERA_WAKE ||
           slot==Taycan::PANAMERA_KEEPALIVE || slot==Taycan::READY))) continue;
      if((uint32_t)(now-lastSent[slot])<period(slot)) continue;
      uint32_t id; uint8_t length; uint8_t bytes[8]={}; bool ext=false;
      if(!frame(slot,id,length,bytes,ext)) { lastSent[slot]=now; continue; }
      if(tryQueueFrame(id,length,bytes,ext)) {
        if(slot==Taycan::SPEEDLIMIT) {
          static int previousLimit = -32768;
          static int previousCode = -1;
          const int signCode=bytes[1] | (uint16_t(bytes[2])<<8);
          if(previousLimit!=state.speedLimitKmh || previousCode!=signCode) {
            previousLimit=state.speedLimitKmh;
            previousCode=signCode;
            char report[100];
            snprintf(report,sizeof(report),
              "Sign input=%d km/h CAN 181 %02X %02X %02X %02X %02X %02X %02X %02X",
              state.speedLimitKmh,bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],
              bytes[5],bytes[6],bytes[7]);
            FlowSerialDebugPrintLn(report);
          }
        }
        if(slot==Taycan::ESP24) {
          static int previousAbs = -1;
          const bool absRequested=(bytes[1]&0x20)!=0;
          if(previousAbs!=absRequested) {
            previousAbs=absRequested;
            char report[120];
            snprintf(report,sizeof(report),
              "ABS=%u key=%u stale=%u CAN 31B %02X %02X %02X %02X %02X %02X %02X %02X",
              absRequested,state.key,stale,bytes[0],bytes[1],bytes[2],bytes[3],
              bytes[4],bytes[5],bytes[6],bytes[7]);
            FlowSerialDebugPrintLn(report);
          }
        }
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
        if(++sentThisLoop>=4) break;
      } else {
        ++queueErrors;
        break;
      }
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

