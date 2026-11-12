#ifndef W140_SPEED_H
#define W140_SPEED_H
#include <stdint.h>
namespace W140Speed {
// Workbook ABS|BRAKE_PAD|BRAKE, speed columns. ONE raw word, MSB/LSB.
// Input is milli-mph. Preserve all supplied points exactly, interpolate between.
// Zero is an explicit standstill assumption; above160mph clamps to sheet range.
constexpr uint16_t interpolate(uint32_t x,uint32_t x0,uint32_t x1,
                               uint16_t y0,uint16_t y1) {
  return y0 + ((x-x0)*uint32_t(y1-y0)+(x1-x0)/2)/(x1-x0);
}
constexpr uint16_t fromMilliMph(uint32_t m) {
  return m<=5000 ? interpolate(m,0,5000,0,0x00ff) :
    m<=10000 ? interpolate(m,5000,10000,0x00ff,0x01ff) :
    m<=15000 ? interpolate(m,10000,15000,0x01ff,0x02ff) :
    m<=20000 ? interpolate(m,15000,20000,0x02ff,0x03ff) :
    m<160000 ? interpolate(m,20000,160000,0x03ff,0x1eff) : 0x1eff;
}
}
#endif
