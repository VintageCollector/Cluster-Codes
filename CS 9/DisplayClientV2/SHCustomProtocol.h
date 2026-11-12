#ifndef W140_SH_CUSTOM_PROTOCOL_H
#define W140_SH_CUSTOM_PROTOCOL_H
#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include <stdlib.h>
#include <math.h>
#include "W140Speed.h"

// Drop into DisplayClientV2. Do not open Serial or print raw text here.
// Incoming: RPM;kmh;ignition;left;right;gear;flags;temp[;throttlePercent[;heartbeat[;fuelLph]]]\n
// Settings: check your Seeed shield CS/crystal and relay module polarity.
namespace W140Config {
const uint8_t CS = 9;
const uint8_t CLOCK = MCP_16MHZ;
const bool LAMP_ACTIVE_LOW = true; // Relay input HIGH is idle and LOW activates the lamp.
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
// IN1–IN8: high, right, left, low, low, spare, coolant, SRS.
const uint8_t HIGH_BEAM=22, RIGHT=23, LEFT=24, LOW_BEAM=25;
const uint8_t LOW_BEAM_IN5=26;
const uint8_t COOLANT_LAMP=28, SRS=29;
#else
const uint8_t HIGH_BEAM=5, RIGHT=6, LEFT=7, LOW_BEAM=8;
const uint8_t LOW_BEAM_IN5=A2;
const uint8_t COOLANT_LAMP=A0, SRS=A1;
// A0–A2 must be free of shield accessories and optional displays.
#endif
const float TEMP_WARNING_ON_C=110, TEMP_WARNING_OFF_C=105; // Simulation choice.
const bool USE_TEMP_WARNING_ON_COOLANT_LAMP=true; // Independent of LOW coolant level.
const bool SRS_PROVEOUT=true;
const uint32_t SRS_PROVEOUT_MS=3000;
const uint32_t PERIOD = 100;
// 0x200 only for speed + ABS. Excel: MSB byte3 OR5, LSB byte4.
// Defaults use one-based byte3/4 = zero-based indexes2/3.
const uint8_t SPEED_MSB_INDEX=2, SPEED_LSB_INDEX=3;
static_assert(SPEED_MSB_INDEX>0 && SPEED_MSB_INDEX<8 && SPEED_LSB_INDEX>0 &&
  SPEED_LSB_INDEX<8 && SPEED_MSB_INDEX!=SPEED_LSB_INDEX,"Speed bytes must not overlap ABS byte1");
const float SPEED_GAIN=1.0f;
// SimHub may send only on change. Default 0 supports original eight-field formula.
// Set 3000 ONLY with the supplied changing-heartbeat formula enabled.
const uint32_t TELEMETRY_TIMEOUT = 0;
const uint32_t RPM_ZERO_GRACE_MS = 400;
// 0: spreadsheet byte order, raw=RPM candidate; 1: original RPM*.004, LE.
const uint8_t RPM_MODE = 0;
const float RPM_GAIN = 1.0f;
const bool IMPERIAL_MPG = false; // US default; true for UK gallons.
// Empirical anchor: raw0x0D00 at60mph ~24MPG (and30mph ~12MPG).
// Approximation: the other readings do not yet prove a linear full-range curve.
const float CONSUMPTION_ANCHOR_RAW=3328, CONSUMPTION_ANCHOR_MPH=60;
const float CONSUMPTION_ANCHOR_MPG=24, CONSUMPTION_GAIN=1.0f;
// Fuel-flow fallback when game flow unavailable: chosen visual model in L/h.
const float IDLE_LPH=1.2f, LOAD_LPH_PER_1000RPM=8.0f;
// 0: workbook measured coolant points; 1: calculations document temp+40.
const uint8_t COOLANT_MODE = 0;
}

class SHCustomProtocol {
  MCP_CAN bus;
  bool ready = false, haveData = false, txFault = false;
  bool ignition = false, left = false, right = false;
  bool overTemperature=false;
  uint32_t ignitionOnAt=0;
  float rpm = 0, kmh = 0, temperature = 80, throttle = 0;
  bool zeroRpmPending = false;
  uint32_t zeroRpmSince = 0;
  bool hasThrottle = false;
  float fuelLph = -1; // Negative means unavailable; zero is valid fuel cut.
  String flags;
  uint32_t lastData = 0, lastSend = 0, retryAt = 0;
  uint32_t lastInputReport=0;
  uint32_t last200Report=0;
  uint8_t last200Mask=255;
  uint32_t badPackets = 0, goodPackets = 0, sendErrors = 0;

  static bool numeric(const String &s, float &v) {
    if (!s.length()) return false;
    char *end;
    v = strtod(s.c_str(), &end);
    while (*end == ' ' || *end == '\r' || *end == '\t') ++end;
    return end != s.c_str() && !*end && isfinite(v);
  }
  static bool inRange(const String &s, float &v, float lo, float hi) {
    return numeric(s, v) && v >= lo && v <= hi;
  }
  static bool tokenChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
  }
  bool flag(const char *name) const {
    int pos = 0;
    const int length = strlen(name);
    while ((pos = flags.indexOf(name, pos)) >= 0) {
      int end = pos + length;
      if ((pos == 0 || !tokenChar(flags[pos-1])) &&
          (end == (int)flags.length() || !tokenChar(flags[end]))) return true;
      pos = end;
    }
    return false;
  }
  static void output(uint8_t pin, bool on, bool activeLow) {
    digitalWrite(pin, on != activeLow ? HIGH : LOW);
  }
  void relays(bool enabled) {
    output(W140Config::LEFT, enabled && ignition && left, W140Config::LAMP_ACTIVE_LOW);
    output(W140Config::RIGHT, enabled && ignition && right, W140Config::LAMP_ACTIVE_LOW);
    const bool lowBeam = enabled && ignition && flag("DL_LOWBEAM");
    output(W140Config::LOW_BEAM, lowBeam, W140Config::LAMP_ACTIVE_LOW);
    output(W140Config::LOW_BEAM_IN5, lowBeam, W140Config::LAMP_ACTIVE_LOW);
    output(W140Config::HIGH_BEAM, enabled && ignition && flag("DL_FULLBEAM"), W140Config::LAMP_ACTIVE_LOW);
    output(W140Config::COOLANT_LAMP, enabled && ignition &&
      ((W140Config::USE_TEMP_WARNING_ON_COOLANT_LAMP && overTemperature) ||
       flag("DL_LOWCOOLANT") || flag("DL_COOLANT")), W140Config::LAMP_ACTIVE_LOW);
    bool srsTest=W140Config::SRS_PROVEOUT && uint32_t(millis()-ignitionOnAt)<W140Config::SRS_PROVEOUT_MS;
    output(W140Config::SRS, enabled && ignition && (flag("DL_SRS") || srsTest), W140Config::LAMP_ACTIVE_LOW);
  }
  static uint16_t word(float value) {
    if (value <= 0) return 0;
    if (value >= 65535) return 65535;
    return (uint16_t)(value + 0.5f);
  }
  static void be(uint8_t *d, uint8_t index, uint16_t raw) {
    d[index] = raw >> 8; d[index+1] = raw & 255;
  }
  static uint8_t coolant(float t) {
    if (W140Config::COOLANT_MODE == 1) return (uint8_t)constrain(t + 40, 0, 254);
    // Only measured 60..120 C points; interpolate and clamp outside them.
    const int temps[] = {60,80,100,120};
    const uint8_t values[] = {0x65,0x7b,0x9c,0xa0};
    if (t <= 60) return values[0];
    for (uint8_t i=1; i<4; ++i)
      if (t <= temps[i]) return values[i-1] + (t-temps[i-1]) *
        (values[i]-values[i-1]) / (temps[i]-temps[i-1]);
    return values[3];
  }
  bool send(uint16_t id, uint8_t *data) {
    if (bus.sendMsgBuf(id, 0, 8, data) == CAN_OK) return true;
    ++sendErrors;
    return false;
  }
  bool frames() {
    uint8_t speed[8] = {}, engine[8] = {}, temp[8] = {}, bas[8] = {};
    // True isolation: omit ALL other CAN frames and bypass speed calculation.
    // Diagnostic only; normal game behavior is unchanged without this token.
    if (flag("W140_ABS_ONLY")) {
      speed[0]=ignition ? 0x04 : 0;
      uint32_t now=millis();
      if (speed[0]!=last200Mask || uint32_t(now-last200Report)>=5000) {
        last200Mask=speed[0];last200Report=now;
        FlowSerialDebugPrintLn(ignition ?
          "W140 ABS-ONLY: ID200 04 00 00 00 00 00 00 00; other IDs omitted" :
          "W140 ABS-ONLY: ignition OFF; ID200 all zero");
      }
      return send(0x200,speed);
    }
    const bool on = ignition;
    if (on) {
      // Repurpose the CAN brake-pad lamp as the simulator parking indicator.
      // The physical red BRAKE lamp is not controlled by this substitution.
      speed[0]=(flag("DL_ABS") ? 0x04 : 0x00) |
        (flag("DL_HANDBRAKE") ? 0x01 : 0x00);
      // Bench-confirmed Master Warning triangle command for stability control off.
      if (flag("DL_ESC_OFF") || flag("DL_DSC_OFF") || flag("DL_DCS_OFF"))
        speed[0] |= 0x20;
      float mph=constrain(kmh/1.609344f*W140Config::SPEED_GAIN,0,160);
      uint16_t speedRaw=W140Speed::fromMilliMph(uint32_t(mph*1000.0f+0.5f));
      speed[W140Config::SPEED_MSB_INDEX]=speedRaw>>8;
      speed[W140Config::SPEED_LSB_INDEX]=speedRaw & 255;
      float r = rpm * W140Config::RPM_GAIN;
      if (W140Config::RPM_MODE == 0) be(engine, 1, word(r));
      else {
        uint16_t raw = (uint16_t)constrain(r * .004f, 0, 65535);
        engine[1] = raw & 255; engine[2] = raw >> 8;
      }
      // User bench confirmed ONE-based byte4 = array index3.
      // 00=OFF,02=check engine,04=oil warning,06=both.
      // Low-fuel location remains unverified; do not assume it shares this byte.
      if (flag("DL_CHECKENGINE")) engine[3] |= 0x02;
      if (flag("DL_LOWOIL")) engine[3] |= 0x04;
      if (flag("DL_BAS") || flag("DL_TC")) bas[0] = 0x01;
    }
    temp[0] = coolant(temperature);
    // Continuous fuel-rate signal; cluster combines it with measured speed.
    float flow=0;
    if (on) {
      if (fuelLph >= 0) flow=fuelLph;
      else if (rpm >= 300) flow=W140Config::IDLE_LPH + rpm/1000.0f *
        (hasThrottle ? throttle/100.0f : 0) * W140Config::LOAD_LPH_PER_1000RPM;
    }
    float litresPerGallon=W140Config::IMPERIAL_MPG ? 4.54609f : 3.7854118f;
    float rawPerLph=W140Config::CONSUMPTION_ANCHOR_RAW * W140Config::CONSUMPTION_ANCHOR_MPG /
      (W140Config::CONSUMPTION_ANCHOR_MPH * litresPerGallon);
    be(temp,5,word(flow * rawPerLph * W140Config::CONSUMPTION_GAIN));
    // Log the actual outgoing payload at lamp changes and every5seconds.
    uint32_t reportTime=millis();
    if (speed[0]!=last200Mask || uint32_t(reportTime-last200Report)>=5000) {
      last200Mask=speed[0]; last200Report=reportTime;
      char report[48];
      snprintf(report,sizeof(report),"W140 TX200 %02X %02X %02X %02X %02X %02X %02X %02X",
        speed[0],speed[1],speed[2],speed[3],speed[4],speed[5],speed[6],speed[7]);
      FlowSerialDebugPrintLn(report);
    }
    // The bench-tested BAS lamp requires the 0x200 frame to be active.
    return send(0x200,speed) && send(0x308,engine) && send(0x608,temp) && send(0x400,bas);
  }
public:
  SHCustomProtocol() : bus(W140Config::CS) {}
  void setup() {
    const uint8_t pins[] = {W140Config::LEFT, W140Config::RIGHT,
      W140Config::LOW_BEAM, W140Config::LOW_BEAM_IN5, W140Config::HIGH_BEAM,
      W140Config::COOLANT_LAMP, W140Config::SRS};
    relays(false); // Preload OFF before pinMode, especially for active-low modules.
    for (uint8_t i=0; i<sizeof(pins); ++i) pinMode(pins[i], OUTPUT);
    digitalWrite(SS, HIGH); pinMode(SS, OUTPUT);
    digitalWrite(4, HIGH); pinMode(4, OUTPUT); // SD deselect, if present.
    pinMode(2, INPUT); // MCP2515 interrupt output: never drive it.
    ready = bus.begin(MCP_ANY, CAN_500KBPS, W140Config::CLOCK) == CAN_OK;
    if (ready) ready = bus.setMode(MCP_NORMAL) == CAN_OK;
    FlowSerialDebugPrintLn(ready ? "W140 CAN ready; waiting for telemetry" : "W140 CAN INIT FAILED: check CS/crystal");
  }
  void read() {
    // Let SimHub/ARQ own Serial and framing. The existing source also expects LF.
    String input = FlowSerialReadStringUntil('\n');
    if (input.length() > 220) { ++badPackets; return; }
    String fields[11];
    uint8_t n=0; int start=0;
    for (unsigned int i=0; i<=input.length(); ++i) {
      if (i == input.length() || input[i] == ';') {
        if (n >= 11) { ++badPackets; return; }
        fields[n++] = input.substring(start, i); start=i+1;
      }
    }
    float r,s,ig,l,ri,t,th=0,flow=-1;
    if (n < 8 || !inRange(fields[0],r,0,20000) || !inRange(fields[1],s,0,500) ||
        !inRange(fields[2],ig,0,1) || (ig != 0 && ig != 1) ||
        !inRange(fields[3],l,0,1) || (l != 0 && l != 1) ||
        !inRange(fields[4],ri,0,1) || (ri != 0 && ri != 1) ||
        !inRange(fields[7],t,-40,180) ||
        (n>=9 && !inRange(fields[8],th,0,100)) ||
        (n>=11 && !inRange(fields[10],flow,-1,1000))) { ++badPackets; return; }
    // Commit only complete, validated input; gear is accepted but not transmitted.
    const uint32_t now = millis();
    bool holdEngineSample = false;
    if (ignition && rpm > 0 && r == 0 && s >= 3) {
      if (!zeroRpmPending) {
        zeroRpmPending = true;
        zeroRpmSince = now;
        FlowSerialDebugPrintLn("W140: zero RPM while moving, holding engine sample briefly");
      }
      holdEngineSample = uint32_t(now - zeroRpmSince) < W140Config::RPM_ZERO_GRACE_MS;
    }
    if (!holdEngineSample) {
      zeroRpmPending = false;
      if (ig == 1 && !ignition) ignitionOnAt = now;
      rpm = r;
      ignition = ig == 1;
      flags = fields[6];
    }
    kmh=s; left=l==1; right=ri==1;
    temperature=t; throttle=th; hasThrottle=n>=9;
    fuelLph=flow;
    if (!ignition) overTemperature=false;
    else if (temperature>=W140Config::TEMP_WARNING_ON_C) overTemperature=true;
    else if (temperature<=W140Config::TEMP_WARNING_OFF_C) overTemperature=false;
    lastData=now; haveData=true; ++goodPackets;
    if (goodPackets==1) FlowSerialDebugPrintLn("W140 telemetry received");
    if (goodPackets==1 || uint32_t(lastData-lastInputReport)>=5000) {
      lastInputReport=lastData;
      String report=String("W140 INPUT kmh=")+String(kmh,2)+" rpm="+String(rpm,0)+
        " ign="+String(ignition ? 1 : 0)+" flowLph="+String(fuelLph,3)+" flags="+flags;
      FlowSerialDebugPrintLn(report.c_str());
    }
  }
  void loop() {
    uint32_t now=millis();
    bool fresh=haveData && (!W140Config::TELEMETRY_TIMEOUT ||
      uint32_t(now-lastData)<W140Config::TELEMETRY_TIMEOUT);
    if (!fresh || !ready) { relays(false); return; }
    if (txFault && uint32_t(now-retryAt)<1000) { relays(false); return; }
    if (uint32_t(now-lastSend)<W140Config::PERIOD) return;
    lastSend=now;
    for (uint8_t i=0; i<4 && bus.checkReceive()==CAN_MSGAVAIL; ++i) {
      unsigned long id; uint8_t len,data[8]; bus.readMsgBuf(&id,&len,data);
    }
    if (!frames()) {
      bus.abortTX(); retryAt=now; relays(false);
      if (!txFault) FlowSerialDebugPrintLn("W140 CAN TX FAILED: check wiring/termination/ACK");
      txFault=true;
    } else {
      if (txFault) FlowSerialDebugPrintLn("W140 CAN TX recovered");
      txFault=false; relays(true);
    }
  }
  void idle() { loop(); }
};
#endif
