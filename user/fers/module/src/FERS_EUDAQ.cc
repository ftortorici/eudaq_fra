#include "eudaq/Producer.hh"
#include "FERS_Registers.h"
#include "FERSlib.h"
#include <iostream>
//#include <ratio>
#include <chrono>
#include <thread>
//#include <random>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "eudaq/Monitor.hh"

#include "configure.h"
//#include "JanusC.h"

#include "DataSender.hh"
#include "FERS_EUDAQ.h"

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
                  +vec.at(index+2) *65536 
                  +vec.at(index+3) *16777216;
	return out;
}
uint64_t FERSunpack64(int index, std::vector<uint8_t> vec)
{
	uint64_t out = vec.at(index) 
		+vec.at(index+1) *256 
		+vec.at(index+2) *65536
		+vec.at(index+3) *16777216
		+( vec.at(index+4) 
		  +vec.at(index+5) *256
		  +vec.at(index+6) *65536
		  +vec.at(index+7) *16777216
		 )*4294967296;
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
      break;
    case (DTQ_STAIRCASE): // staircase event
      FERSpack_staircaseevent(Event, vec);
  }
}

//////////////////////////

// Spectroscopy event
void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec)
{
  int x_pixel = 8;
  int y_pixel = 8;
  int nchan = x_pixel*y_pixel;
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

  //std::cout<<"***FERSpack: initial size of vec: "<< vec->size() <<std::endl;
  FERSpack( 8*sizeof(double), tstamp_us,  vec);
  //std::cout<<"***FERSpack: current size of vec: "<< vec->size() <<std::endl;
  FERSpack( 64,             trigger_id, vec);
  //std::cout<<"***FERSpack: current size of vec: "<< vec->size() <<std::endl;
  FERSpack( 64,             chmask,     vec);
  //std::cout<<"***FERSpack: current size of vec: "<< vec->size() <<std::endl;
  FERSpack( 64,             qdmask,     vec);
  //std::cout<<"***FERSpack: current size of vec: "<< vec->size() <<std::endl;
  for (size_t i = 0; i<nchan; i++){
    energyHG[i] = tmpEvent->energyHG[i];
    FERSpack( 16,energyHG[i], vec);
  }
  //std::cout<<"***FERSpack: filled "<< nchan <<" * 2 bytes, current size of vec: "<< vec->size() <<std::endl;
  for (size_t i = 0; i<nchan; i++){
    energyLG[i] = tmpEvent->energyLG[i];
    FERSpack( 16,energyLG[i], vec);
  }
  //std::cout<<"***FERSpack: filled "<< nchan <<" * 2 bytes, current size of vec: "<< vec->size() <<std::endl;
  for (size_t i = 0; i<nchan; i++){
    tstamp[i]   = tmpEvent->tstamp[i]  ;
    FERSpack( 32,tstamp[i]  , vec);
  }
  //std::cout<<"***FERSpack: filled "<< nchan <<" * 4 bytes, current size of vec: "<< vec->size() <<std::endl;
  for (size_t i = 0; i<nchan; i++){
    ToT[i]      = tmpEvent->ToT[i]     ;
    FERSpack( 16,ToT[i]     , vec);
  }
  //std::cout<<"***FERSpack: filled "<< nchan <<" * 2 bytes, current size of vec: "<< vec->size() <<std::endl;

  //      //dump on console
  //      std::cout<< "tstamp_us  "<< tstamp_us  <<std::endl;
  //      std::cout<< "trigger_id "<< trigger_id <<std::endl;
  //      std::cout<< "chmask     "<< chmask     <<std::endl;
  //      std::cout<< "qdmask     "<< qdmask     <<std::endl;
  //
  //      for(size_t i = 0; i < y_pixel; ++i) {
  //        for(size_t n = 0; n < x_pixel; ++n){
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


//void FERSunpack_spectevent(SpectEvent_t* Event, std::vector<uint8_t> *vec)
SpectEvent_t FERSunpack_spectevent(std::vector<uint8_t> *vec)
{
  int nchan = 64;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  SpectEvent_t tmpEvent;

  int dsize = data.size();
  int tsize = sizeof( tmpEvent );
  if ( tsize != dsize ){
	  EUDAQ_THROW("*** WRONG DATA SIZE FOR SPECT EVENT! "+std::to_string(tsize)+" vs "+std::to_string(dsize)+" ***");
  } else {
	  EUDAQ_WARN("Size of data is right");
  }

  bool debug = false;
  int index = 0;
  int sd = sizeof(double);
  switch(8*sd)
  {
    case 8:
      tmpEvent.tstamp_us = data.at(index); break;
    case 16:
      tmpEvent.tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.tstamp_us = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.tstamp_us = FERSunpack64(index, data);
  }
  index += sd; // each element of data is uint8_t
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read double tstamp_us: "+std::to_string(tmpEvent.tstamp_us));
  tmpEvent.trigger_id = FERSunpack64( index,data); index += 8;
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint64_t trigger_id: "+std::to_string(tmpEvent.trigger_id));
  tmpEvent.chmask     = FERSunpack64( index,data); index += 8;
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint64_t chmask: "+std::to_string(tmpEvent.chmask));
  tmpEvent.qdmask     = FERSunpack64( index,data); index += 8;
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint64_t qdmask: "+std::to_string(tmpEvent.qdmask));
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyHG[i] = FERSunpack16(index,data); index += 2;
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 energyHG[64] ");
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyLG[i] = FERSunpack16(index,data); index += 2;
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 energyLG[64] ");
  for (int i=0; i<nchan; ++i) {
    tmpEvent.tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint32 tstamp[64] ");
  for (int i=0; i<nchan; ++i) {
    tmpEvent.ToT[i] = FERSunpack16(index,data); index += 2;
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 ToT[64] ");

  return tmpEvent;
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
ListEvent_t FERSunpack_listevent(std::vector<uint8_t> *vec)
{
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  ListEvent_t tmpEvent;
  int index = 0;
  tmpEvent.nhits = FERSunpack16(index, data); index +=2;
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent.channel[i] = data.at(index); index += 1;
  }
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent.tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<MAX_LIST_SIZE; ++i) {
    tmpEvent.ToT[i] = FERSunpack16(index,data); index += 2;
  }
  return tmpEvent;
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

  FERSpack( 8*sizeof(double), tstamp_us,  vec);
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
SpectEvent_t FERSunpack_tspectevent(std::vector<uint8_t> *vec)
{
  int nchan = 64;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  SpectEvent_t tmpEvent;

  int index = 0;
  int sd = sizeof(double);
  switch(8*sd)
  {
    case 8:
      tmpEvent.tstamp_us = data.at(index); break;
    case 16:
      tmpEvent.tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.tstamp_us = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.tstamp_us = FERSunpack64(index, data);
  }
  index += sd;
  tmpEvent.trigger_id = FERSunpack64( index,data); index += 8;
  tmpEvent.chmask     = FERSunpack64( index,data); index += 8;
  tmpEvent.qdmask     = FERSunpack64( index,data); index += 8;
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyHG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyLG[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent.tstamp[i] = FERSunpack32(index,data); index += 4;
  }
  for (int i=0; i<nchan; ++i) {
    tmpEvent.ToT[i] = FERSunpack16(index,data); index += 2;
  }
  return tmpEvent;
}


//////////////////////////

//// Counting Event
void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec)
{
  int nchan=64;
  CountingEvent_t* tmpEvent = (CountingEvent_t*) Event;
  FERSpack( 8*sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(	64, tmpEvent->trigger_id, vec);
  FERSpack(	64, tmpEvent->chmask, vec);
  for (size_t i = 0; i<nchan; i++){
    FERSpack( 32,tmpEvent->counts[i], vec);
  }
  FERSpack(	32, tmpEvent->t_or_counts, vec);
  FERSpack(	32, tmpEvent->q_or_counts, vec);
}
CountingEvent_t FERSunpack_countevent(std::vector<uint8_t> *vec)
{
  int nchan=64;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  CountingEvent_t tmpEvent;
  int index = 0;
  int sd = sizeof(double);
  switch(8*sd)
  {
    case 8:
      tmpEvent.tstamp_us = data.at(index); break;
    case 16:
      tmpEvent.tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.tstamp_us = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.tstamp_us = FERSunpack64(index, data);
  }
  index += sd;
  tmpEvent.trigger_id = FERSunpack64(index,data); index += 8;
  tmpEvent.chmask = FERSunpack64(index,data); index += 8;
  for (size_t i = 0; i<nchan; i++){
    tmpEvent.counts[i] = FERSunpack32(index,data); index += 4;
  }
  tmpEvent.t_or_counts = FERSunpack32(index,data); index += 4;
  tmpEvent.q_or_counts = FERSunpack32(index,data); index += 4;
  return tmpEvent;
}

//////////////////////////


//// Waveform Event
void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_WAVEFORM_LENGTH;
  WaveEvent_t* tmpEvent = (WaveEvent_t*) Event;
  FERSpack( 8*sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(64, tmpEvent->trigger_id, vec);
  FERSpack(16, tmpEvent->ns, vec);
  for (int i=0; i<n; i++) FERSpack(16, tmpEvent->wave_hg[i], vec);
  for (int i=0; i<n; i++) FERSpack(16, tmpEvent->wave_lg[i], vec);
  for (int i=0; i<n; i++) vec->push_back(tmpEvent->dig_probes[i]);
}
WaveEvent_t FERSunpack_waveevent(std::vector<uint8_t> *vec)
{
  int n = MAX_WAVEFORM_LENGTH;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  WaveEvent_t tmpEvent;
  int index = 0;
  int sd = sizeof(double);
  switch(8*sd)
  {
    case 8:
      tmpEvent.tstamp_us = data.at(index); break;
    case 16:
      tmpEvent.tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.tstamp_us = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.tstamp_us = FERSunpack64(index, data);
  }
  index += sd;
  tmpEvent.trigger_id = FERSunpack64(index,data); index += 8;
  tmpEvent.ns = FERSunpack16(index,data); index += 2;
  for (int i=0; i<n; i++){
    tmpEvent.wave_hg[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<n; i++){
    tmpEvent.wave_lg[i] = FERSunpack16(index,data); index += 2;
  }
  for (int i=0; i<n; i++){
    tmpEvent.dig_probes[i] = vec->at(index); index += 1;
  }
  return tmpEvent;
}

//////////////////////////


//// Test Mode Event (fixed data patterns)
void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec)
{
  int n = MAX_TEST_NWORDS;
  TestEvent_t* tmpEvent = (TestEvent_t*) Event;
  FERSpack( 8*sizeof(double), tmpEvent->tstamp_us,  vec);
  FERSpack(64, tmpEvent->trigger_id, vec);
  FERSpack(16, tmpEvent->nwords, vec);
  for (int i=0; i<n; i++) FERSpack(32, tmpEvent->test_data[i], vec);
}
TestEvent_t FERSunpack_testevent(std::vector<uint8_t> *vec)
{
  int n = MAX_TEST_NWORDS;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  TestEvent_t tmpEvent;
  int index = 0;
  int sd = sizeof(double);
  switch(8*sd)
  {
    case 8:
      tmpEvent.tstamp_us = data.at(index); break;
    case 16:
      tmpEvent.tstamp_us = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.tstamp_us = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.tstamp_us = FERSunpack64(index, data);
  }
  index += sd;
  tmpEvent.trigger_id  = FERSunpack64(index, data); index+=8; 
  tmpEvent.nwords      = FERSunpack16(index, data); index+=2;
  for (int i=0; i<n; i++){
    tmpEvent.test_data[i] = FERSunpack32(index, data); index+=4;
  }
  return tmpEvent;
}

//////////////////////////
//	uint16_t threshold;
//	uint16_t dwell_time; // in seconds, divide hitcnt by this to get rate
//	uint32_t chmean; // over channels, no division by time
//	uint16_t shapingt; // enum, see FERS_Registers.h
//	float    HV;
//	uint32_t Tor_cnt;
//	uint32_t Qor_cnt;
//	uint32_t hitcnt[FERSLIB_MAX_NCH];
//} StaircaseEvent_t;
void FERSpack_staircaseevent(void* Event, std::vector<uint8_t> *vec){
  int n = FERSLIB_MAX_NCH;
  StaircaseEvent_t* tmpEvent = (StaircaseEvent_t*) Event;
  FERSpack(16, tmpEvent->threshold, vec);
  FERSpack(16, tmpEvent->dwell_time, vec);
  FERSpack(32, tmpEvent->chmean, vec);
  FERSpack(16, tmpEvent->shapingt, vec);
  FERSpack( 8*sizeof(float), tmpEvent->HV,  vec);
  FERSpack(32, tmpEvent->Tor_cnt, vec);
  FERSpack(32, tmpEvent->Qor_cnt, vec);
  for (int i=0; i<n; i++) FERSpack(32, tmpEvent->hitcnt[i], vec);

  //std::cout<<"FERSpack_staircaseevent : tmpEvent->threshold  = "+std::to_string(tmpEvent->threshold)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->dwell_time = "+std::to_string(tmpEvent->dwell_time)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->chmean     = "+std::to_string(tmpEvent->chmean)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->shapingt   = "+std::to_string(tmpEvent->shapingt)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->HV         = "+std::to_string(tmpEvent->HV)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->Tor_cnt    = "+std::to_string(tmpEvent->Tor_cnt)<<std::endl;
  //std::cout<<"FERSpack_staircaseevent : tmpEvent->Qor_cnt    = "+std::to_string(tmpEvent->Qor_cnt)<<std::endl;
};
StaircaseEvent_t FERSunpack_staircaseevent(std::vector<uint8_t> *vec){
  int n = FERSLIB_MAX_NCH;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  StaircaseEvent_t tmpEvent;
  int index = 0;
  int sd = sizeof(float);
  tmpEvent.threshold  = FERSunpack16(index, data); index+=2;
  tmpEvent.dwell_time = FERSunpack16(index, data); index+=2;
  tmpEvent.chmean     = FERSunpack32(index, data); index+=4;
  tmpEvent.shapingt   = FERSunpack16(index, data); index+=2;
  switch(8*sd)
  {
    case 8:
      tmpEvent.HV = data.at(index); break;
    case 16:
      tmpEvent.HV = FERSunpack16(index, data); break;
    case 32:
      tmpEvent.HV = FERSunpack32(index, data); break;
    case 64:
      tmpEvent.HV = FERSunpack64(index, data);
  }
  index += sd;
  tmpEvent.Tor_cnt  = FERSunpack32(index, data); index+=4; 
  tmpEvent.Qor_cnt  = FERSunpack32(index, data); index+=4; 
  for (int i=0; i<n; i++){
    tmpEvent.hitcnt[i] = FERSunpack32(index, data); index+=4;
  }
  dump_vec("FERSunpack_staircaseevent",vec, 20);
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.threshold  = "+std::to_string(tmpEvent.threshold) );
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.dwell_time = "+std::to_string(tmpEvent.dwell_time));
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.chmean     = "+std::to_string(tmpEvent.chmean)    );
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.shapingt   = "+std::to_string(tmpEvent.shapingt)  );
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.HV         = "+std::to_string(tmpEvent.HV)        );
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.Tor_cnt    = "+std::to_string(tmpEvent.Tor_cnt)   );
  //EUDAQ_WARN("FERSunpack_staircaseevent : tmpEvent.Qor_cnt    = "+std::to_string(tmpEvent.Qor_cnt)   );
  return tmpEvent;

};


//////////////////////////
// fill "data" with some info
void make_header(int handle, uint8_t x_pixel, uint8_t y_pixel, int DataQualifier, std::vector<uint8_t> *data)
{
	uint8_t n=0;
	std::vector<uint8_t> vec;

	// this are needed
	vec.push_back(x_pixel);
	vec.push_back(y_pixel);
	vec.push_back((uint8_t)DataQualifier);
	n+=3;

	// ID info:

	// serial number
	int sernum=0;
	HV_Get_SerNum(handle, &sernum);
	vec.push_back((uint8_t)sernum);
	n++;

	//handle
	vec.push_back((uint8_t)handle);
	n++;

	// put everything in data, prefixing header with its size
	data->push_back(n);
	//std::cout<<"***** make header > going to write "<< std::to_string(n) <<" bytes" <<std::endl;
	for (int i=0; i<n; i++)
	{
		//std::cout<<"***** make header > writing "<< std::to_string(vec.at(i)) <<std::endl;
		data->push_back( vec.at(i) );
	}
}
// reads back essential header info (see params)
// prints them w/ board ID info with EUDAQ_WARN
// returns index at which raw data starts
int read_header(std::vector<uint8_t> *vec, uint8_t *x_pixel, uint8_t *y_pixel, uint8_t *DataQualifier)
{
	std::vector<uint8_t> data(vec->begin(), vec->end());
	int index = data.at(0);
	EUDAQ_WARN("read header > going to read " + std::to_string(index) +" bytes");

	if(data.size() < index+1)
		EUDAQ_THROW("Unknown data");
	
	*x_pixel = data.at(1);
	*y_pixel = data.at(2);
	*DataQualifier = data.at(3);

	uint8_t sernum=data.at(4);
	uint8_t handle=data.at(5);

	std::string printme = "Monitor > received from FERS serial# "
		+ std::to_string(sernum)
		+" handle "+std::to_string(handle)
		+" n "+std::to_string(index)
		+" x "+std::to_string(*x_pixel)
		+" y "+std::to_string(*y_pixel)
		;
	EUDAQ_WARN(printme);

	return index+1; // first header byte is header size, then index bytes
};

// dump a vector
void dump_vec(std::string title, std::vector<uint8_t> *vec, int limit){
	int n = vec->size();
	if (limit > 0) n = std::min(n, limit);
	std::string printme;
	for (int i=0; i<n; i++){
		printme = "--- "+title+" > vec[" + std::to_string(i) + "] = "+ std::to_string( vec->at(i) );
		//std::cout << printme <<std::endl;
		EUDAQ_WARN( printme );
	}
};
