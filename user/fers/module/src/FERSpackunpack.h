#ifndef _FERSpackunpack_h
#define _FERSpackunpack_h

#include <vector>

void FERSpack(int nbits, uint32_t input, std::vector<uint8_t> *vec);
uint16_t FERSunpack16(int index, std::vector<uint8_t> vec);
uint32_t FERSunpack32(int index, std::vector<uint8_t> vec);
uint64_t FERSunpack64(int index, std::vector<uint8_t> vec);
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

//////////////////////////
void FERSpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec);
void FERSunpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec);
//////////////////////////

void FERSpack_listevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_listevent(void* Event, std::vector<uint8_t> *vec);

void FERSpack_tspectevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_tspectevent(void* Event, std::vector<uint8_t> *vec);

void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_countevent(void* Event, std::vector<uint8_t> *vec);

void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_waveevent(void* Event, std::vector<uint8_t> *vec);

void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_testevent(void* Event, std::vector<uint8_t> *vec);

void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec);
void FERSunpack_spectevent(void* Event, std::vector<uint8_t> *vec);

#endif
