#ifndef SH_BMW_G30_CUSTOM_PROTOCOL_H

#define SH_BMW_G30_CUSTOM_PROTOCOL_H

#include <Arduino.h>

#include <SPI.h>

#include <mcp2515_can.h>

#include "BMWProtocol.h"

#include "BMWButtons.h"

class SHCustomProtocol {

 mcp2515_can canBus{BMWConfig::CS_PIN};

 BMW::SpeedEco speedEco;uint32_t lastEcoUpdate=0;
 BMW::State state;BMW::LaunchLatch launchLatch;BMW::RpmFilter rpmFilter;BMW::Frame frames[BMW::FRAME_COUNT];

 uint8_t seq[BMW::FRAME_COUNT]={};uint8_t next=0;

 uint32_t sent[BMW::FRAME_COUNT]={},lastData=0,lastInit=0,lastBuild=0,lastLog=0,bcChanged=0;

 BMW::ButtonInput menuInputs[7];

 bool ready=false,haveData=false,bcRaw=false,bcStable=false;

 uint8_t ccProbeChoice=0;

 uint16_t errors=0,distanceCounter=0;

 uint32_t lastRpmUpdate=0;

 uint32_t lastMotion=0,distanceRemainder=0,wakeUntil=0;

 bool keyWasOn=false;uint32_t busySince=0,lastRecovery=0;

 void oil(bool warning){if(BMWConfig::ENABLE_OIL_LOGIC_OUTPUT)digitalWrite(BMWConfig::OIL_LOGIC_PIN,warning==BMWConfig::OIL_ACTIVE_HIGH?HIGH:LOW);}

 void beginCan(){ready=canBus.begin(CAN_500KBPS,MCP_16MHz)==CAN_OK;if(ready)ready=canBus.setMode(MODE_NORMAL)==CAN_OK;lastInit=millis();FlowSerialDebugPrintLn(ready?"BMW F10 CAN ready":"BMW F10 CAN init failed");}

 // Dedicated nonblocking keepalive, safe duringARQserialwaits: no serialI/O here.

 void serviceMsa(){

  if(!ready||!haveData)return;

  uint32_t now=millis();
  if(uint32_t(now-lastEcoUpdate)>=10){uint32_t dt=lastEcoUpdate?now-lastEcoUpdate:10;lastEcoUpdate=now;BMW::ecoRequest()=speedEco.update(state.v[1],state.v[4]>=2,dt);}

  if(uint32_t(now-sent[BMW::AUTO_STOP])<BMWConfig::F10_MSA_INTERVAL_MS)return;

  BMW::Frame msa;BMW::f10Msa(state,msa,seq[BMW::AUTO_STOP]);

  if(canBus.trySendMsgBuf(msa.id,0,0,msa.len,msa.data)==CAN_OK){

   sent[BMW::AUTO_STOP]=now;seq[BMW::AUTO_STOP]=BMWConfig::F10_MSA_USE_STATIC_PATTERN?uint8_t(seq[BMW::AUTO_STOP]+1):uint8_t((seq[BMW::AUTO_STOP]+1)%15);

  }

 }

 // Keepneedleupdatesrunningduringserialwaits;filterdisplayRPMwithoutchangingstateRPM.

 void serviceRpm(){

  if(!ready||!haveData||state.v[4]<1)return;

  uint32_t now=millis();if(uint32_t(now-sent[BMW::RPM])<20)return;

  uint32_t dt=lastRpmUpdate?uint32_t(now-lastRpmUpdate):20;lastRpmUpdate=now;

  BMW::Frame rpmFrame;BMW::f10Rpm(rpmFrame,rpmFilter.update(state.v[0],state.v[4]>=2,dt),state.gear);

  if(canBus.trySendMsgBuf(rpmFrame.id,0,0,rpmFrame.len,rpmFrame.data)==CAN_OK){

   sent[BMW::RPM]=now;seq[BMW::RPM]=(seq[BMW::RPM]+1)%15;

  }

 }

 // Keepestablishedlane/ACCrequestscontinuousduringserialwaits, independentofcruise.

 void serviceAssistance(){

  if(!ready||!haveData||state.v[4]<2)return;

  uint32_t now=millis();

  const uint8_t ids[]={BMW::LANE_TEST,BMW::ACC_BASE};

  for(uint8_t which=0;which<2;which++){

   uint8_t i=ids[which];if(uint32_t(now-sent[i])<100)continue;

   BMW::Frame frame;

   if(which==0)BMW::f10Lane(state,frame,seq[i]);else BMW::f10Acc(state,frame,seq[i]);

   if(frame.enabled&&canBus.trySendMsgBuf(frame.id,0,0,frame.len,frame.data)==CAN_OK){

    sent[i]=now;seq[i]=which==0?uint8_t((seq[i]+1)%4):(BMWConfig::F10_ACC_CAPTURED_G01_BASELINE?uint8_t((seq[i]+1)%15):uint8_t(seq[i]+1));

   }

  }

 }

 // Dedicated fuel sender, including serial idle. No other owner sends349.
 void serviceFuel(){
  if(!ready||!haveData||state.v[4]<2||state.v[48]<0)return;
  uint32_t now=millis();if(uint32_t(now-sent[BMW::FUEL])<200)return;
  long pct=BMW::limit(state.v[48],0,100);uint8_t data[8]={};
  uint8_t fuel=pct<=50?uint8_t(37-pct*19/50):uint8_t(18-(pct-50)*14/50);
  data[1]=data[3]=fuel;
  if(canBus.trySendMsgBuf(0x349,0,0,5,data)==CAN_OK)sent[BMW::FUEL]=now;
 }
 void serviceCruise(){
  if(!ready||!haveData||state.v[4]<2)return;
  uint32_t now=millis();if(uint32_t(now-sent[BMW::CRUISE_TEST])<100)return;
  BMW::Frame frame;BMW::f10Cruise(state,frame,seq[BMW::CRUISE_TEST]);
  if(frame.enabled&&canBus.trySendMsgBuf(frame.id,0,0,frame.len,frame.data)==CAN_OK){
   sent[BMW::CRUISE_TEST]=now;seq[BMW::CRUISE_TEST]=(seq[BMW::CRUISE_TEST]+1)%15;
  }
 }
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

  bool requestOn=haveData&&state.v[4]>=2;

  if(requestOn&&!keyWasOn){

   // Reset MCP2515 on every OFF->ON edge: discard stale unacknowledged TX buffers.

   beginCan();lastRecovery=now;busySince=0;memset(sent,0,sizeof(sent));memset(seq,0,sizeof(seq));next=BMW::WAKE;

   wakeUntil=now+3000;lastBuild=now-10;

  }

  keyWasOn=requestOn;

  if(!ready){if((uint32_t)(now-lastInit)>=2000)beginCan();return;}

  if(busySince&&(uint32_t)(now-busySince)>=1000&&(uint32_t)(now-lastRecovery)>=2000){

   beginCan();lastRecovery=now;busySince=0;memset(sent,0,sizeof(sent));next=BMW::WAKE;wakeUntil=now+3000;lastBuild=now-10;

   FlowSerialDebugPrintLn("F10 CAN TX recovery; wake sequence restarted");if(!ready)return;

  }

  serviceMsa();

  serviceRpm();
  serviceFuel();
  serviceCruise();

  serviceAssistance();

  bool value=digitalRead(BMWConfig::BC_BUTTON_PIN)==LOW;

  if(value!=bcRaw){bcRaw=value;bcChanged=now;}

  if((uint32_t)(now-bcChanged)>=30)bcStable=bcRaw;

  bool bc=bcStable||(haveData&&state.v[32]!=0);

  if((uint32_t)(now-lastBuild)>=10){

   uint32_t dt=lastMotion?now-lastMotion:0;lastMotion=now;

   if(dt>100)dt=100; // Never integrate a long stall or time without telemetry.

   bool on=haveData&&state.v[4]>=2;

   // Wake transition is handled before transmission, including controller reset.

   if(on){distanceRemainder+=uint32_t(state.v[1])*BMWConfig::F10_DISTANCE_COUNTS_PER_KMH_SECOND*dt;distanceCounter+=distanceRemainder/1000;distanceRemainder%=1000;}

   lastBuild=now;BMW::build(state,frames,seq,bc,menuInputs[0].pressed,ccProbeChoice);

   // RPMhandledbydedicatedservice,includingidlecallbacks.

   // Keep the confirmed slow510 payload in every key state; no alternate burst.

   frames[BMW::GEAR_RING].data[2]=distanceCounter&255;frames[BMW::GEAR_RING].data[3]=distanceCounter>>8;

   frames[BMW::GEAR_RING].data[0]=BMW::crc(frames[BMW::GEAR_RING].data+1,4,0xDE);

  }

  for(uint8_t checked=0;checked<BMW::FRAME_COUNT;checked++){

   uint8_t i=next;next=(next+1)%BMW::FRAME_COUNT;

   if(i==BMW::CRUISE_TEST||i==BMW::FUEL||i==BMW::AUTO_STOP||i==BMW::RPM||i==BMW::LANE_TEST||i==BMW::ACC_BASE)continue; // Dedicatedserviceowns30B; no duplicates.

   if(!haveData&&i!=BMW::BC&&i!=BMW::IGN&&i!=BMW::WAKE)continue;

   if(state.v[4]<1&&i!=BMW::IGN&&i!=BMW::WAKE&&i!=BMW::AUTO_STOP&&!(i==BMW::BC&&bc))continue;

   if(!frames[i].enabled||(uint32_t)(now-sent[i])<frames[i].period)continue;

   if(canBus.trySendMsgBuf(frames[i].id,0,0,frames[i].len,frames[i].data)==CAN_OK){busySince=0;sent[i]=now;seq[i]=(i==BMW::SAFETY||i==BMW::COOLANT_GAUGE||i==BMW::IGN||i==BMW::DOOR_RR||i==BMW::ACC_BASE||i==BMW::AUTO_STOP)?uint8_t(seq[i]+1):uint8_t((seq[i]+1)%(i==BMW::LANE_TEST?4:15));}else {++errors;if(!busySince)busySince=now?now:1;}

   break;

  }

  if(errors&&(uint32_t)(now-lastLog)>=5000){lastLog=now;errors=0;FlowSerialDebugPrintLn("BMW CAN queue busy: check ACK/wiring/bitrate");}

 }

 void idle(){serviceCruise();serviceFuel();serviceMsa();serviceRpm();serviceAssistance();}

};

#endif

