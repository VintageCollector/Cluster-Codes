#ifndef BMW_BUTTONS_H
#define BMW_BUTTONS_H
#include <stdint.h>
namespace BMW {
// Active-low switch debouncing; holding preserves state, with no automatic repeats.
struct ButtonInput {
 bool raw=false,pressed=false;uint32_t changed=0;
 bool update(bool down,uint32_t now){
  if(down!=raw){raw=down;changed=now;}
  if((uint32_t)(now-changed)>=30 && pressed!=raw){pressed=raw;return true;}
  return false;
 }
};
}
#endif
