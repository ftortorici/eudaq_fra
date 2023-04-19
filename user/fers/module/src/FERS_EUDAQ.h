#ifndef _FERS_EUDAQ_h
#define _FERS_EUDAQ_h

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
// use this to pack every kind of event
void FERSpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec);
//////////////////////////

// for each kind of event: the "pack" is used by FERSpackevent,
// whereas the "unpack" is meant to be used individually
void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_spectevent(std::vector<uint8_t> *vec);

void FERSpack_listevent(void* Event, std::vector<uint8_t> *vec);
ListEvent_t FERSunpack_listevent(std::vector<uint8_t> *vec);

void FERSpack_tspectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_tspectevent(std::vector<uint8_t> *vec);

void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec);
CountingEvent_t FERSunpack_countevent(std::vector<uint8_t> *vec);

void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec);
WaveEvent_t FERSunpack_waveevent(std::vector<uint8_t> *vec);

void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec);
TestEvent_t FERSunpack_testevent(std::vector<uint8_t> *vec);

/////////////////
int FERS_EUDAQstaircase(int handle, uint16_t stair_shapingt, uint16_t stair_start, uint16_t stair_stop, uint16_t stair_step, float stair_dwell_time);


#endif
