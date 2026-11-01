#ifndef CRC8_H
#define CRC8_H

#include "Arduino.h"

typedef uint8_t crc;
#define POLYNOMIAL 0x1D
#define WIDTH (8 * sizeof(crc))
#define TOPBIT (1 << (WIDTH - 1))

class CRC8 {
	public:
	  CRC8();
	  void begin();
    uint8_t get_transmission_crc8(uint8_t crc, uint8_t const *buf, uint32_t len);
	  crc get_crc8(uint8_t const message[], int nBytes, uint8_t final);
	 
	private:
	  uint8_t crcTable[256];
};

#endif
