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

void FERSpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec)
{
  switch( dataqualifier )
  {
    case (DTQ_SPECT):
      FERSpack_spectevent(Event, vec);
      break;
    case (DTQ_TIMING): // List Event (Timing Mode only)
      FERSpack_listevent(Event, vec);
      break;
    case (DTQ_TSPECT): // Spectroscopy + Timing Mode (Energy + Tstamp)
      FERSpack_tspectevent(Event, vec);
      break;
    case (DTQ_COUNT):	// Counting Mode (MCS)
      FERSpack_countevent(Event, vec);
      break;
    case (DTQ_WAVE): // Waveform Mode
      FERSpack_waveevent(Event, vec);
      break;
    case (DTQ_TEST): // Test Mode (fixed data patterns)
      FERSpack_testevent(Event, vec);
  }
}


void FERSunpackevent(void* Event, int dataqualifier, std::vector<uint8_t> *vec)
{
  switch( dataqualifier )
  {
    case (DTQ_SPECT): // Spectroscopy mode
      FERSunpack_spectevent(Event, vec);
      break;
    case (DTQ_TIMING): // List Event (Timing Mode only)
      FERSunpack_listevent(Event, vec);
      break;
    case (DTQ_TSPECT): // Spectroscopy + Timing Mode (Energy + Tstamp)
      FERSunpack_tspectevent(Event, vec);
      break;
    case (DTQ_COUNT):	// Counting Mode (MCS)
      FERSunpack_countevent(Event, vec);
      break;
    case (DTQ_WAVE): // Waveform Mode
      FERSunpack_waveevent(Event, vec);
      break;
    case (DTQ_TEST): // Test Mode (fixed data patterns)
      FERSunpack_testevent(Event, vec);
  }
}

//////////////////////////

// Spectroscopy event
void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan = 64;
  // temporary event, used to correctly interpret the Event.
  // The same technique is used in the other pack routines as well
  SpectEvent_t *tmpEvent = (SpectEvent_t*)Event;
  // the following group of vars is not really needed. Used for debug purposes.
  // This is valid also for the other pack routines
  double   tstamp_us      = tmpEvent->tstamp_us ;
  uint64_t trigger_id     = tmpEvent->trigger_id;
  uint64_t chmask         = tmpEvent->chmask    ;
  uint64_t qdmask         = tmpEvent->qdmask    ;
  uint16_t energyHG[nchan];
  uint16_t energyLG[nchan];
  uint32_t tstamp[nchan]  ;
  uint16_t ToT[nchan]     ;

  FERSpack( sizeof(double), tstamp_us,  vec);
  FERSpack( 64,             trigger_id, vec);
  FERSpack( 64,             chmask,     vec);
  FERSpack( 64,             qdmask,     vec);
  for (size_t i = 0; i<nchan; i++){
    energyHG[i] = tmpEvent->energyHG[i];
    FERSpack( 16,energyHG[i], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    energyLG[i] = tmpEvent->energyLG[i];
    FERSpack( 16,energyLG[i], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    tstamp[i]   = tmpEvent->tstamp[i]  ;
    FERSpack( 32,tstamp[i]  , vec);
  }
  for (size_t i = 0; i<nchan; i++){
    ToT[i]      = tmpEvent->ToT[i]     ;
    FERSpack( 16,ToT[i]     , vec);
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
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  SpectEvent_t *tmpEvent = (SpectEvent_t*)Event;

  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      tmpEvent->tstamp_us = data.at(index); break;
    case 16:
      tmpEvent->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  tmpEvent->trigger_id = FERSunpack64( index,data); index += 8;
  tmpEvent->chmask     = FERSunpack64( index,data); index += 8;
  tmpEvent->qdmask     = FERSunpack64( index,data); index += 8;
  for (int i=0; i<nchan; ++i) {
    tmpEvent->energyHG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->energyLG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->ToT[i] = FERSunpack16(index,data); index += 2;
  }
}



//////////////////////////


//// List Event (timing mode only)
void FERSpack_listevent(void* Event, std::vector<uint8_t> *vec)
{
  ListEvent_t *tmpEvent = (ListEvent_t*) Event;
  uint16_t nhits = tmpEvent->nhits;
  uint8_t  channel[MAX_LIST_SIZE];
  uint32_t tstamp[MAX_LIST_SIZE];
  uint16_t ToT[MAX_LIST_SIZE];

  FERSpack(16, nhits, vec);
  for (size_t i = 0; i<MAX_LIST_SIZE; i++){
    channel[i] = tmpEvent->channel[i];
    vec->push_back(channel[i]);
  }
  for (size_t i = 0; i<MAX_LIST_SIZE; i++){
    tstamp[i] = tmpEvent->tstamp[i];
    FERSpack( 32,tstamp[i], vec);
  }
  for (size_t i = 0; i<MAX_LIST_SIZE; i++){
    ToT[i] = tmpEvent->ToT[i];
    FERSpack( 16,ToT[i], vec);
  }
}
void FERSunpack_listevent(void* Event, std::vector<uint8_t> *vec)
{
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  ListEvent_t* tmpEvent = (ListEvent_t*) Event;
  int index = 0;
  tmpEvent->nhits = FERSunpack16(index, data); index +=2;
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent->channel[i] = data.at(index); index += 1;
  }
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent->tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent->ToT[i] = FERSunpack16(index,data); index += 2;
  }
}

//////////////////////////


//// Spectroscopy + Timing Mode (Energy + Tstamp)
void FERSpack_tspectevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan = 64;
  SpectEvent_t *tmpEvent = (SpectEvent_t*)Event;
  double   tstamp_us      = tmpEvent->tstamp_us ;
  uint64_t trigger_id     = tmpEvent->trigger_id;
  uint64_t chmask         = tmpEvent->chmask    ;
  uint64_t qdmask         = tmpEvent->qdmask    ;
  uint16_t energyHG[nchan];
  uint16_t energyLG[nchan];
  uint32_t tstamp[nchan]  ;
  uint16_t ToT[nchan]     ;

  FERSpack( sizeof(double), tstamp_us,  vec);
  FERSpack( 64,             trigger_id, vec);
  FERSpack( 64,             chmask,     vec);
  FERSpack( 64,             qdmask,     vec);
  for (size_t i = 0; i<nchan; i++){
    energyHG[i] = tmpEvent->energyHG[i];
    FERSpack( 16,energyHG[i], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    energyLG[i] = tmpEvent->energyLG[i];
    FERSpack( 16,energyLG[i], vec);
  }
  for (size_t i = 0; i<nchan; i++){
    tstamp[i]   = tmpEvent->tstamp[i]  ;
    FERSpack( 32,tstamp[i]  , vec);
  }
  for (size_t i = 0; i<nchan; i++){
    ToT[i]      = tmpEvent->ToT[i]     ;
    FERSpack( 16,ToT[i]     , vec);
  }

}
void FERSunpack_tspectevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan = 64;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  SpectEvent_t *tmpEvent = (SpectEvent_t*)Event;

  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      tmpEvent->tstamp_us = data.at(index); break;
    case 16:
      tmpEvent->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  tmpEvent->trigger_id = FERSunpack64( index,data); index += 8;
  tmpEvent->chmask     = FERSunpack64( index,data); index += 8;
  tmpEvent->qdmask     = FERSunpack64( index,data); index += 8;
  for (int i=0; i<nchan; ++i) {
    tmpEvent->energyHG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->energyLG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent->ToT[i] = FERSunpack16(index,data); index += 2;
  }
}


//////////////////////////

//// Counting Event
void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan=64;
  CountingEvent_t* tmpEvent = (CountingEvent_t*) Event;
  FERSpack( sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(	64, tmpEvent->trigger_id, vec);
  FERSpack(	64, tmpEvent->chmask, vec);
  for (size_t i = 0; i<nchan; i++){
    FERSpack( 32,tmpEvent->counts[i], vec);
  }
  FERSpack(	32, tmpEvent->t_or_counts, vec);
  FERSpack(	32, tmpEvent->q_or_counts, vec);
}
void FERSunpack_countevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan=64;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  CountingEvent_t* tmpEvent = (CountingEvent_t*) Event;
  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      tmpEvent->tstamp_us = data.at(index); break;
    case 16:
      tmpEvent->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  tmpEvent->trigger_id = FERSunpack64(index,data); index += 8;
  tmpEvent->chmask = FERSunpack64(index,data); index += 8;
  for (size_t i = 0; i<nchan; i++){
    tmpEvent->counts[i] = FERSunpack32(index,data); index += 4;
  }
  tmpEvent->t_or_counts = FERSunpack32(index,data); index += 4;
  tmpEvent->q_or_counts = FERSunpack32(index,data); index += 4;
}

//////////////////////////


//// Waveform Event
void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_WAVEFORM_LENGTH;
  WaveEvent_t* tmpEvent = (WaveEvent_t*) Event;
  FERSpack( sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(64, tmpEvent->trigger_id, vec);
  FERSpack(16, tmpEvent->ns, vec);
  for (int i=0; i<n; i++) FERSpack(16, tmpEvent->wave_hg[i], vec);
  for (int i=0; i<n; i++) FERSpack(16, tmpEvent->wave_lg[i], vec);
  for (int i=0; i<n; i++) vec->push_back(tmpEvent->dig_probes[i]);
}
void FERSunpack_waveevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_WAVEFORM_LENGTH;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  WaveEvent_t* tmpEvent = (WaveEvent_t*) Event;
  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      tmpEvent->tstamp_us = data.at(index); break;
    case 16:
      tmpEvent->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  tmpEvent->trigger_id = FERSunpack64(index,data); index += 8;
  tmpEvent->ns = FERSunpack16(index,data); index += 2;
  for (int i=0; i<n; i++){
    tmpEvent->wave_hg[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<n; i++){
    tmpEvent->wave_lg[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<n; i++){
    tmpEvent->dig_probes[i] = vec->at(index); index += 1;
  }
}

//////////////////////////


//// Test Mode Event (fixed data patterns)
void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_TEST_NWORDS;
  TestEvent_t* tmpEvent = (TestEvent_t*) Event;
  FERSpack( sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(64, tmpEvent->trigger_id, vec);
  FERSpack(16, tmpEvent->nwords, vec);
  for (int i=0; i<n; i++) FERSpack(32, tmpEvent->test_data[i], vec);
}
void FERSunpack_testevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_TEST_NWORDS;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  TestEvent_t* tmpEvent = (TestEvent_t*) Event;
  int index = 0;
  int sd = sizeof(double);
  switch(sd)
  {
    case 8:
      tmpEvent->tstamp_us = data.at(index); break;
    case 16:
      tmpEvent->tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent->tstamp_us = FERSunpack32(index, data);
  }
  index += sd/8; // each element of data is uint8_t
  tmpEvent->trigger_id  = FERSunpack64(index, data); index+=8; 
  tmpEvent->nwords      = FERSunpack16(index, data); index+=2;
  for (int i=0; i<n; i++){
    tmpEvent->test_data[i] = FERSunpack32(index, data); index+=4;
  }
}

