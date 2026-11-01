#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__
#ifndef FUSION_DASH_H
#define FUSION_DASH_H
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515_can.h>
#include <stdlib.h>
#include <string.h>
// Seeed CAN_BUS_Shield library, as in the Nissan DisplayClient package.
#define FUSION_CS 9
#define FUSION_CLOCK MCP_16MHz
// Optional relay DRIVER input. Never connect a Mega pin to cluster 12V.
#define FUSION_RELAY_PIN 40
#define FUSION_ENABLE_RELAY 0
// Buttons: active LOW, each switch wired between its pin and Mega GND.
// PDF: 7=OK, 6=Back, 3/4=Up or Down, 5 unknown.
// Provisional: 3=Up, 4=Down, 5=Right. Back uses the reference Left command.
static const byte FUSION_MENU_PINS[5]={3,4,5,6,7};
static const byte FUSION_MENU_CODES[5]={0x08,0x01,0x04,0x02,0x10};
class FusionDash {
  mcp2515_can can;
  char line[240]; uint16_t used; bool dropping, ready, received;
  uint32_t lastRx, lastTx, retry;
  byte menuRaw=0, menuStable=0; uint32_t menuChanged=0;
  void pollMenu(uint32_t now) {
    byte pressed=0, code=0;
    for(byte i=0;i<5;++i) if(digitalRead(FUSION_MENU_PINS[i])==LOW) { ++pressed; code=FUSION_MENU_CODES[i]; }
    // Multiple simultaneous buttons produce neutral, not a combined CAN command.
    if(pressed!=1) code=0;
    if(code!=menuRaw) { menuRaw=code; menuChanged=now; }
    if(uint32_t(now-menuChanged)>=25) menuStable=menuRaw;
  }
  long rpm, speed10, temp; char gear[8]; char flags[180];
  bool has(const char *token) const {
    size_t n=strlen(token); const char *p=flags;
    while (*p) { const char *e=strchr(p,','); size_t len=e ? size_t(e-p) : strlen(p);
      if(len==n && !strncmp(p,token,n)) return true;
      if(!e) break; p=e+1;
    } return false;
  }
  bool number(const char *p,long lo,long hi,long &v) {
    if(!*p) return false; char *end; v=strtol(p,&end,10);
    return !*end && v>=lo && v<=hi;
  }
  void send(uint16_t id, byte a=0,byte b=0,byte c=0,byte d=0,byte e=0,byte f=0,byte g=0,byte h=0) {
    byte data[8]={a,b,c,d,e,f,g,h};
    if(can.sendMsgBuf(id,0,8,data)!=CAN_OK) ++txErrors;
  }
  void parse() {
    char *parts[7]; byte count=0; parts[count++]=line;
    for(char *p=line;*p;++p) if(*p==';') { *p=0; if(count>=7) return; parts[count++]=p+1; }
    if(count!=7 || strcmp(parts[0],"F1") || strcmp(parts[6],"END")) return;
    long r,s,t; if(!number(parts[1],0,16000,r)||!number(parts[2],0,5000,s)||!number(parts[3],-40,215,t)) return;
    if(!*parts[4] || strlen(parts[4])>=sizeof(gear) || strlen(parts[5])>=sizeof(flags)) return;
    bool validGear=!strcmp(parts[4],"P")||!strcmp(parts[4],"R")||!strcmp(parts[4],"N")||!strcmp(parts[4],"D")||!strcmp(parts[4],"S");
    long gn; if(!validGear && !number(parts[4],1,24,gn)) return;
    rpm=r; speed10=s; temp=t; strcpy(gear,parts[4]); strcpy(flags,parts[5]);
    lastRx=millis(); received=true; ++packets;
  }
public:
  uint32_t packets, txErrors;
  FusionDash():can(FUSION_CS),used(0),dropping(false),ready(false),received(false),lastRx(0),lastTx(0),retry(0),rpm(0),speed10(0),temp(20),packets(0),txErrors(0) { strcpy(gear,"N"); flags[0]=0; }
  void setup() { pinMode(53,OUTPUT); digitalWrite(53,HIGH); if(FUSION_ENABLE_RELAY) { digitalWrite(FUSION_RELAY_PIN,LOW); pinMode(FUSION_RELAY_PIN,OUTPUT); } for(byte i=0;i<5;++i) pinMode(FUSION_MENU_PINS[i],INPUT_PULLUP); SPI.begin(); init(); }
  void init() { retry=millis(); ready=can.begin(CAN_500KBPS,FUSION_CLOCK)==CAN_OK; if(ready) ready=can.setMode(MODE_NORMAL)==CAN_OK; }
  void feed(char c) {
    if(c=='\r') return;
    if(c=='\n') { if(!dropping) { line[used]=0; parse(); } used=0; dropping=false; return; }
    if(dropping) return;
    if(used>=sizeof(line)-1) { dropping=true; used=0; return; }
    line[used++]=c;
  }
  void update() {
    uint32_t now=millis(); pollMenu(now); bool live=received && uint32_t(now-lastRx)<1000;
    bool ign=live && has("DL_IGNITION"); if(FUSION_ENABLE_RELAY) digitalWrite(FUSION_RELAY_PIN,ign?HIGH:LOW);
    if(!ready) { if(uint32_t(now-retry)>=2000) init(); return; }
    if(uint32_t(now-lastTx)<50) return; lastTx=now;
    // Periodic press/hold and explicit zero release; works without game telemetry.
    send(0x081,menuStable);
    bool low=ign&&has("DL_LOWBEAM"), high=ign&&has("DL_FULLBEAM"), park=ign&&has("DL_HANDBRAKE");
    bool sport=ign&&(has("DM_SPORT")||!strcmp(gear,"S"));
    uint16_t r=ign?rpm/2:0;
    // Speed encoding retained from Fusion_ets.ino: ~144 raw units / km/h.
    // The HS3 text conflicts with that sketch. Calibrate against YOUR cluster.
    uint16_t s=ign?uint32_t(speed10)*144/10:0;
    send(0x3B3,ign?0x40:0,0,0,ign?(low?0x12:0x0C):0,0,0,0,0);
    send(0x204,0,0x40,0,r>>8,r&255,0,0,0x3E);
    send(0x202,0,0xFF,0xFF,0,0x60,0,s>>8,s&255);
    send(0x156,byte((ign?temp:20)+40),0,0,0,3);
    byte sel=0x40, num=0x04;
    if(ign) { if(!strcmp(gear,"P")) sel=0; else if(!strcmp(gear,"R")) sel=0x20;
      else if(strcmp(gear,"N")) { sel=sport?0x80:0x60; int n=atoi(gear); if(n>=1&&n<=6) num=byte((n<<4)|5); } }
    send(0x171,num,sel);
    send(0x38A,0,0,sport?3:0,0,0,sport?0x80:0);
    send(0x3B2,ign&&has("DL_REARFOG")?2:0,ign?8:0,0,0,ign&&has("DL_RIGHT")?8:0,0,ign&&has("DL_LEFT")?0x40:0,ign&&has("DL_FOG")?1:0);
    send(0x3C3,high?2:0,0,park?0x10:0);
    send(0x213,0,0,0,park?0x20:0);
    byte esc=0; if(ign) { if(has("DL_ESC_OFF")) esc|=8; if(has("DL_ESC")||has("DL_TC")) esc|=4; }
    send(0x416,0,0,0,0,0,esc,ign&&has("DL_ABS")?0x40:0);
    byte cruise=0x10; if(ign) { if(has("DL_CRUISE")) cruise=0x70; if(has("DL_CRUISE_ACTIVE")) cruise=0xB0; }
    send(0x42C,cruise);
    // No odometer increment, settings writes, or unknown keepalive spam.
  }
};
#endif

// Alternative to standalone .ino: use inside SimHub's generated sketch.
class SHCustomProtocol {
  FusionDash dash;
public:
  void setup() { dash.setup(); }
  void read() {
    String message=FlowSerialReadStringUntil('\n');
    if(message.length()>=240) return;
    for(unsigned int i=0;i<message.length();++i) dash.feed(message[i]);
    dash.feed('\n');
  }
  void loop() { dash.update(); }
  void idle() { dash.update(); }
};
#endif

