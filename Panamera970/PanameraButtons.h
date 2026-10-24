#ifndef PANAMERA_BUTTONS_H
#define PANAMERA_BUTTONS_H
#include <stdint.h>
namespace Panamera {
// Raw mask: Up, Down, OK, Back. One event per debounced press; release to repeat.
class Buttons {
 bool armed=true;
 uint8_t raw=0,stable=0;uint32_t changed=0;
public:
 uint8_t update(uint8_t value,uint32_t now){
  if(value!=raw){raw=value;changed=now;}
  if((uint32_t)(now-changed)<30||stable==raw)return 0;
  stable=raw;
  if(!stable){armed=true;return 0;}
  if(!armed)return 0;
  armed=false;
  if(stable&(stable-1))return 0;
  for(uint8_t i=0;i<4;i++)if(stable==(1<<i))return i+1;
  return 0;
 }
};
}
#endif
