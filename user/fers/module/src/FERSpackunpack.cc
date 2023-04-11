#include <stdlib.h>
#include <stdint.h>

#include "FERSpackunpack.h"
#include "FERSlib.h"

// puts a nbits (16, 32, 64) integer into an 8 bits vector
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
	uint32_t out = vec.at(index) 
                  +vec.at(index+1) *256 
                  +vec.at(index+2) *256*256 
                  +vec.at(index+3) *256*256*256;
	return out;
}
uint64_t FERSunpack64(int index, std::vector<uint8_t> vec)
{
	uint64_t out = vec.at(index) 
                  +vec.at(index+1) *256 
                  +vec.at(index+2) *256*256 
                  +vec.at(index+3) *256*256*256 
                  +vec.at(index+4) *256*256*256*256
                  +vec.at(index+5) *256*256*256*256*256
                  +vec.at(index+6) *256*256*256*256*256*256
                  +vec.at(index+7) *256*256*256*256*256*256*256;
	return out;
}



void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan = 64;
  SpectEvent_t *EventSpect = (SpectEvent_t*)Event;
  double   tstamp_us      = EventSpect->tstamp_us ;
  uint64_t trigger_id     = EventSpect->trigger_id;
  uint64_t chmask         = EventSpect->chmask    ;
  uint64_t qdmask         = EventSpect->qdmask    ;
  uint16_t energyHG[nchan];
  uint16_t energyLG[nchan];
  uint32_t tstamp[nchan]  ;
  uint16_t ToT[nchan]     ;

  FERSpack( sizeof(double), tstamp_us,  vec);
  FERSpack( 64,             trigger_id, vec);
  FERSpack( 64,             chmask,     vec);
  FERSpack( 64,             qdmask,     vec);
  for (size_t i = 0; i<nchan; i++){
    energyHG[i] = EventSpect->energyHG[i];
    FERSpack( 16,energyHG[nchan], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    energyLG[i] = EventSpect->energyLG[i];
    FERSpack( 16,energyLG[nchan], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    tstamp[i]   = EventSpect->tstamp[i]  ;
    FERSpack( 32,tstamp[nchan]  , vec);
  }
  for (size_t i = 0; i<nchan; i++){
    ToT[i]      = EventSpect->ToT[i]     ;
    FERSpack( 16,ToT[nchan]     , vec);
  }

  //      //dump on console
  //      std::cout<< "tstamp_us  "<< tstamp_us  <<std::endl;
  //      std::cout<< "trigger_id "<< trigger_id <<std::endl;
  //      //std::cout<< "chmask     "<< chmask     <<std::endl;
  //      //std::cout<< "qdmask     "<< qdmask     <<std::endl;
  //
  //      for(size_t i = 0; i < y_pixel; ++i) {
  //        for(size_t n = 0; n < 2*x_pixel; ++n){
  //          //std::cout<< (int)hit[n+i*x_pixel] <<"_";
  //          std::cout<< (int)energyHG[n+i*x_pixel] <<"_";
  //          //std::cout<< (int)energyLG[n+i*x_pixel] <<"_";
  //          //std::cout<< (int)tstamp  [n+i*x_pixel] <<"_";
  //          //std::cout<< (int)ToT     [n+i*x_pixel] <<"_";
  //        }
  //        std::cout<< "<<"<< std::endl;
  //      }
  //      std::cout<<std::endl;
}


void FERSunpack_spectevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan = 64;
  //int nelem = vec->size();
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  SpectEvent_t *EventSpect = (SpectEvent_t*)Event;
  //double   tstamp_us      ;
  //uint64_t trigger_id     ;
  //uint64_t chmask         ;
  //uint64_t qdmask         ;
  //uint16_t energyHG[nchan];
  //uint16_t energyLG[nchan];
  //uint32_t tstamp[nchan]  ;
  //uint16_t ToT[nchan]     ;

  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      EventSpect->tstamp_us = data.at(index); break;
    case 16:
      EventSpect->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      EventSpect->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  EventSpect->trigger_id = FERSunpack64( index,data); index += 8;
  EventSpect->chmask     = FERSunpack64( index,data); index += 8;
  EventSpect->qdmask     = FERSunpack64( index,data); index += 8;
  for (int i=0; i<nchan; ++i) {
    EventSpect->energyHG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    EventSpect->energyLG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    EventSpect->tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<nchan; ++i) {
    EventSpect->ToT[i] = FERSunpack16(index,data); index += 2;
  }
}


void FERSpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec)
{
  switch( dataqualifier )
  {
    case (DTQ_SPECT):
      FERSpack_spectevent(Event, vec);
      break;
  }
}


void FERSunpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec)
{
  switch( dataqualifier )
  {
    case (DTQ_SPECT):
      FERSunpack_spectevent(Event, vec);
      break;
  }
}

