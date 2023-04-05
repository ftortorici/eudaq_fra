#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include "FERSpackunpack.h"

// puts a nbits (16 or 32) integer into an 8 bits vector
void FERSpack(int nbits, uint32_t input, std::vector<uint8_t> *vec)
{
	uint8_t out;// = (uint8_t)( input & 0x00FF);

	int n;
	//vec->push_back( out );
	for (int i=0; i<nbits/8; i++){
		n = 8*i;
		out = (uint8_t)( (input >> n) & 0xFF ) ;
		vec->push_back( out );
	}
}

// reads a 16/32 bits integer from a 8 bits vector
uint16_t FERSunpack16(int index, std::vector<uint8_t> vec)
{
	uint16_t out = vec.at(index) + vec.at(index+1) * 256;
	return out;
}
uint32_t FERSunpack32(int index, std::vector<uint8_t> vec)
{
	uint32_t out = vec.at(index) + vec.at(index+1) * 256 + vec.at(index+2) * 256 * 256 + vec.at(index+3) * 256 * 256 * 256;
	return out;
}

//// to test:
//  uint16_t num16 = 0xabcd;
//  uint32_t num32 = 0x12345678;
//  std::vector<uint8_t> vec;
//  FERSpack(32, num32, &vec);
//  std::printf("FERSpack   : num32 = %x test32= %x %x %x %x\n", num32, vec.at(0), vec.at(1), vec.at(2), vec.at(3));
//  FERSpack(16, num16, &vec);
//  std::printf("FERSpack   : num16 = %x test16= %x %x\n", num16, vec.at(4), vec.at(5));
//
//  uint32_t num32r = FERSunpack32( 0, vec );
//  uint16_t num16r = FERSunpack16( 4, vec );
//  std::printf("FERSunpack : num32r = %x\n", num32r);
//  std::printf("FERSunpack : num16r = %x\n", num16r);
