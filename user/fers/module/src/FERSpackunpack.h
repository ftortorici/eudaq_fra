#ifndef _FERSpackunpack_h
#define _FERSpackunpack_h

void FERSpack(int nbits, uint32_t input, std::vector<uint8_t> *vec);
uint16_t FERSunpack16(int index, std::vector<uint8_t> vec);
uint32_t FERSunpack32(int index, std::vector<uint8_t> vec);

#endif
