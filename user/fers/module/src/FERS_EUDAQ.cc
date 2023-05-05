#include "eudaq/Producer.hh"
#include "FERS_Registers.h"
#include "FERSlib.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "eudaq/Monitor.hh"

#include "configure.h"

#include "DataSender.hh"
#include "FERS_EUDAQ.h"

//event_lengths[ DTQ_SPECT     ] = sizeof(SpectEvent_t)    ;
//event_lengths[ DTQ_TIMING    ] = sizeof(ListEvent_t)     ;
//event_lengths[ DTQ_COUNT     ] = sizeof(CountingEvent_t) ;
//event_lengths[ DTQ_WAVE      ] = sizeof(WaveEvent_t)     ;
//event_lengths[ DTQ_TSPECT    ] = sizeof(SpectEvent_t)    ;
//event_lengths[ DTQ_TEST      ] = sizeof(TestEvent_t)     ;
//event_lengths[ DTQ_STAIRCASE ] = sizeof(StaircaseEvent_t);

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
      //EUDAQ_INFO("*** FERSpackevent > received a staircase event");
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

  //      //dump on console
        //std::cout<< "tstamp_us  "<< tstamp_us  <<std::endl;
        //std::cout<< "trigger_id "<< trigger_id <<std::endl;
        //std::cout<< "chmask     "<< chmask     <<std::endl;
        //std::cout<< "qdmask     "<< qdmask     <<std::endl;
  
        //for(size_t i = 0; i < y_pixel; ++i) {
        //  for(size_t n = 0; n < x_pixel; ++n){
        //    //std::cout<< (int)hit[n+i*x_pixel] <<"_";
        //    std::cout<< (int)energyHG[n+i*x_pixel] <<"_";
        //    //std::cout<< (int)energyLG[n+i*x_pixel] <<"_";
        //    //std::cout<< (int)tstamp  [n+i*x_pixel] <<"_";
        //    //std::cout<< (int)ToT     [n+i*x_pixel] <<"_";
        //  }
        //  std::cout<< "<<"<< std::endl;
        //}
        //std::cout<<std::endl;
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
  //
  uint64_t sum=0;
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyHG[i] = FERSunpack16(index,data); index += 2;
    sum+=tmpEvent.energyHG[i];
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 energyHG[64], sum: "+std::to_string(sum));
  sum=0;
  for (int i=0; i<nchan; ++i) {
    tmpEvent.energyLG[i] = FERSunpack16(index,data); index += 2;
    sum+=tmpEvent.energyLG[i];
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 energyLG[64], sum: "+std::to_string(sum));
  sum=0;
  for (int i=0; i<nchan; ++i) {
    tmpEvent.tstamp[i] = FERSunpack32(index,data); index += 4;
    sum+=tmpEvent.tstamp[i];
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint32 tstamp[64], sum: "+std::to_string(sum));
  sum=0;
  for (int i=0; i<nchan; ++i) {
    tmpEvent.ToT[i] = FERSunpack16(index,data); index += 2;
    sum+=tmpEvent.ToT[i];
  }
  if (debug) EUDAQ_WARN("* index: "+std::to_string(index)+" read uint16 ToT[64], sum: "+std::to_string(sum));

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
//	uint16_t shapingt; // enum, see FERS_Registers.h
//	uint32_t dwell_time; // in seconds, divide hitcnt by this to get rate
//	uint32_t chmean; // over channels, no division by time
//	uint32_t HV; // 1000 * HV from HV_Get_Vbias( handle, &HV);
//	uint32_t Tor_cnt;
//	uint32_t Qor_cnt;
//	uint32_t hitcnt[FERSLIB_MAX_NCH];
//} StaircaseEvent_t;
void FERSpack_staircaseevent(void* Event, std::vector<uint8_t> *vec){
  //EUDAQ_INFO("*** FERSpack_staircaseevent > starting encoding");
  int n = FERSLIB_MAX_NCH;
  StaircaseEvent_t* tmpEvent = (StaircaseEvent_t*) Event;
  int index=0;
  FERSpack(16, tmpEvent->threshold, vec);  index+=2;
  FERSpack(16, tmpEvent->shapingt, vec);   index+=2;
  FERSpack(32, tmpEvent->dwell_time, vec); index+=4;
  FERSpack(32, tmpEvent->chmean, vec);     index+=4;
  FERSpack(32, tmpEvent->HV,  vec);        index+=4;
  FERSpack(32, tmpEvent->Tor_cnt, vec);    index+=4;
  FERSpack(32, tmpEvent->Qor_cnt, vec);    index+=4;
  for (int i=0; i<n; i++)
  {
	  FERSpack(32, tmpEvent->hitcnt[i], vec); index+=4;
  }
  //EUDAQ_INFO("*** FERSpack_staircaseevent > event cast: size " +std::to_string(sizeof(*tmpEvent)));
  //EUDAQ_INFO("*** FERSpack_staircaseevent > index: "+std::to_string(index));
  //EUDAQ_INFO("*** FERSpack_staircaseevent > threshold: "+std::to_string(tmpEvent->threshold));
};
StaircaseEvent_t FERSunpack_staircaseevent(std::vector<uint8_t> *vec){
  int n = FERSLIB_MAX_NCH;
  std::vector<uint8_t> data( vec->begin(), vec->end() );
  StaircaseEvent_t tmpEvent;
  int index = 0;
  tmpEvent.threshold  = FERSunpack16(index, data); index+=2;
  tmpEvent.shapingt   = FERSunpack16(index, data); index+=2;
  tmpEvent.dwell_time = FERSunpack32(index, data); index+=4;
  tmpEvent.chmean     = FERSunpack32(index, data); index+=4;
  tmpEvent.HV         = FERSunpack32(index, data); index+=4;
  tmpEvent.Tor_cnt    = FERSunpack32(index, data); index+=4; 
  tmpEvent.Qor_cnt    = FERSunpack32(index, data); index+=4; 
  for (int i=0; i<n; i++){
    tmpEvent.hitcnt[i] = FERSunpack32(index, data); index+=4;
  }
  //EUDAQ_WARN("FERSunpack_staircaseevent: sizeof vec: "+std::to_string(vec->size()) +" index: "+std::to_string(index) +" sizeof struct: "+std::to_string(sizeof(tmpEvent)));
  //dump_vec("FERSunpack_staircaseevent",vec, 16);
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
	//EUDAQ_WARN("read header > going to read " + std::to_string(index) +" bytes");

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
void dump_vec(std::string title, std::vector<uint8_t> *vec, int start, int stop){
	int n = vec->size();
	if (stop > 0) n = min(n, stop);
	std::string printme;
	for (int i=start; i<n; i++){
		printme = "--- "+title+" > vec[" + std::to_string(i) + "] = "+ std::to_string( vec->at(i) );
		//std::cout << printme <<std::endl;
		EUDAQ_WARN( printme );
	}
};




///////////////////////  FUNCTIONS IN ALPHA STATE  /////////////////////
///////////////////////  NO DEBUG IS DONE. AT ALL! /////////////////////


// init WDcfg structure with default settings
void initWDcfg(Config_t *WDcfg)
{
	memset(WDcfg, 0, sizeof(Config_t));
	/* Default settings */
	strcpy(WDcfg->DataFilePath, "DataFiles");
	WDcfg->NumBrd = 0;	
	WDcfg->NumCh = 64;
	WDcfg->GWn = 0;
	WDcfg->EHistoNbin = 4096;
	WDcfg->ToAHistoNbin = 4096;
	WDcfg->ToARebin = 1;
	WDcfg->ToAHistoMin = 0;
	WDcfg->ToTHistoNbin = 512;
	WDcfg->MCSHistoNbin = 4096;
	WDcfg->AcquisitionMode = 0;
	WDcfg->EnableToT = 1;
	WDcfg->TriggerMask = 0;
	WDcfg->TriggerLogic = 0;
	WDcfg->MajorityLevel = 2;
	WDcfg->T0_outMask = 0;
	WDcfg->T1_outMask = 0;
	WDcfg->Tref_Mask = 0;
	WDcfg->TrefWindow = 100;
	WDcfg->PtrgPeriod = 0;
	WDcfg->QD_CoarseThreshold = 0;
	//WDcfg->TD_CoarseThreshold = 0;
	WDcfg->HG_ShapingTime = 0;
	WDcfg->LG_ShapingTime = 0;
	WDcfg->Enable_HV_Adjust = 0;
	WDcfg->EnableChannelTrgout = 1;
	WDcfg->HV_Adjust_Range = 1;
	WDcfg->EnableQdiscrLatch = 1;
	WDcfg->GainSelect = GAIN_SEL_AUTO;
	WDcfg->AnalogProbe = 0;
	WDcfg->DigitalProbe = 0;
	WDcfg->WaveformLength = 800;
	WDcfg->Trg_HoldOff = 0;
	WDcfg->Pedestal = 100;
	WDcfg->TempSensCoeff[0] = 0;
	WDcfg->TempSensCoeff[1] = 50;
	WDcfg->TempSensCoeff[2] = 0;

	for (int b = 0; b < MAX_NBRD; b++) {
		WDcfg->TD_CoarseThreshold[b] = 0;	// new
		WDcfg->ChEnableMask0[b] = 0xFFFFFFFF;
		WDcfg->ChEnableMask1[b] = 0xFFFFFFFF;
		WDcfg->Q_DiscrMask0[b] = 0xFFFFFFFF;
		WDcfg->Q_DiscrMask1[b] = 0xFFFFFFFF;
		WDcfg->Tlogic_Mask0[b] = 0xFFFFFFFF;
		WDcfg->Tlogic_Mask1[b] = 0xFFFFFFFF;
		WDcfg->HV_Vbias[b] = 30;
		WDcfg->HV_Imax[b] = (float)0.01;
		// Default values for TMP37
		for (int i = 0; i < MAX_NCH; i++) {
			WDcfg->ZS_Threshold_LG[b][i] = 0;
			WDcfg->ZS_Threshold_HG[b][i] = 0;
			WDcfg->QD_FineThreshold[b][i] = 0;
			WDcfg->TD_FineThreshold[b][i] = 0;
			WDcfg->HG_Gain[b][i] = 0;
			WDcfg->LG_Gain[b][i] = 0;
			WDcfg->HV_IndivAdj[b][i] = 0;
		}
	}
	EUDAQ_INFO("WDcfg should be initialized");
}



// verbose 
// 0: fine: minimal set of params for first board only
// 1: tell me more: every param of first board, no channel details
// 2: are you sure?!?: as 2 + every channel
// 3: ok then: everything of every board
void dumpWDcfg(Config_t WDcfg, int verbose=0)
{
	//EUDAQ_WARN("WDcfg.DataFilePath        : "+std::to_string( WDcfg.DataFilePath ));
	EUDAQ_WARN("WDcfg.NumBrd              : "+std::to_string( WDcfg.NumBrd ));
	EUDAQ_WARN("WDcfg.NumCh               : "+std::to_string( WDcfg.NumCh ));

	if (verbose > 0){
	EUDAQ_WARN("WDcfg.GWn                 : "+std::to_string( WDcfg.GWn ));
	EUDAQ_WARN("WDcfg.EHistoNbin          : "+std::to_string( WDcfg.EHistoNbin ));
	EUDAQ_WARN("WDcfg.ToAHistoNbin        : "+std::to_string( WDcfg.ToAHistoNbin ));
	EUDAQ_WARN("WDcfg.ToARebin            : "+std::to_string( WDcfg.ToARebin ));
	EUDAQ_WARN("WDcfg.ToAHistoMin         : "+std::to_string( WDcfg.ToAHistoMin ));
	EUDAQ_WARN("WDcfg.ToTHistoNbin        : "+std::to_string( WDcfg.ToTHistoNbin ));
	EUDAQ_WARN("WDcfg.MCSHistoNbin        : "+std::to_string( WDcfg.MCSHistoNbin ));
	}

	EUDAQ_WARN("WDcfg.AcquisitionMode     : "+std::to_string( WDcfg.AcquisitionMode ));

	if (verbose > 0){
	EUDAQ_WARN("WDcfg.EnableToT           : "+std::to_string( WDcfg.EnableToT ));
	EUDAQ_WARN("WDcfg.TriggerMask         : "+std::to_string( WDcfg.TriggerMask ));
	EUDAQ_WARN("WDcfg.TriggerLogic        : "+std::to_string( WDcfg.TriggerLogic ));
	EUDAQ_WARN("WDcfg.MajorityLevel       : "+std::to_string( WDcfg.MajorityLevel ));
	EUDAQ_WARN("WDcfg.T0_outMask          : "+std::to_string( WDcfg.T0_outMask ));
	EUDAQ_WARN("WDcfg.T1_outMask          : "+std::to_string( WDcfg.T1_outMask ));
	EUDAQ_WARN("WDcfg.Tref_Mask           : "+std::to_string( WDcfg.Tref_Mask ));
	EUDAQ_WARN("WDcfg.TrefWindow          : "+std::to_string( WDcfg.TrefWindow ));
	EUDAQ_WARN("WDcfg.PtrgPeriod          : "+std::to_string( WDcfg.PtrgPeriod ));
	EUDAQ_WARN("WDcfg.QD_CoarseThreshold  : "+std::to_string( WDcfg.QD_CoarseThreshold ));
	}

	EUDAQ_WARN("WDcfg.HG_ShapingTime      : "+std::to_string( WDcfg.HG_ShapingTime ));
	EUDAQ_WARN("WDcfg.LG_ShapingTime      : "+std::to_string( WDcfg.LG_ShapingTime ));

	if (verbose > 0){
	EUDAQ_WARN("WDcfg.Enable_HV_Adjust    : "+std::to_string( WDcfg.Enable_HV_Adjust ));
	EUDAQ_WARN("WDcfg.EnableChannelTrgout : "+std::to_string( WDcfg.EnableChannelTrgout ));
	EUDAQ_WARN("WDcfg.HV_Adjust_Range     : "+std::to_string( WDcfg.HV_Adjust_Range ));
	EUDAQ_WARN("WDcfg.EnableQdiscrLatch   : "+std::to_string( WDcfg.EnableQdiscrLatch ));
	}

	EUDAQ_WARN("WDcfg.GainSelect          : "+std::to_string( WDcfg.GainSelect ));

	if (verbose > 0){
	EUDAQ_WARN("WDcfg.AnalogProbe         : "+std::to_string( WDcfg.AnalogProbe ));
	EUDAQ_WARN("WDcfg.DigitalProbe        : "+std::to_string( WDcfg.DigitalProbe ));
	EUDAQ_WARN("WDcfg.WaveformLength      : "+std::to_string( WDcfg.WaveformLength ));
	EUDAQ_WARN("WDcfg.Trg_HoldOff         : "+std::to_string( WDcfg.Trg_HoldOff ));
	EUDAQ_WARN("WDcfg.Pedestal            : "+std::to_string( WDcfg.Pedestal ));
	EUDAQ_WARN("WDcfg.TempSensCoeff[0]    : "+std::to_string( WDcfg.TempSensCoeff[0] ));
	EUDAQ_WARN("WDcfg.TempSensCoeff[1]    : "+std::to_string( WDcfg.TempSensCoeff[1] ));
	EUDAQ_WARN("WDcfg.TempSensCoeff[2]    : "+std::to_string( WDcfg.TempSensCoeff[2] ));
	}

if ( verbose > 0 )
{
	for (int b = 0; b < MAX_NBRD; b++) {

if ( (b == 0) || (verbose > 2) )
{
		EUDAQ_WARN("WDcfg.TD_CoarseThreshold["+std::to_string(b)+"]   : "+std::to_string( WDcfg.TD_CoarseThreshold[b] ));

		EUDAQ_WARN("WDcfg.ChEnableMask0["+std::to_string(b)+"]        : "+std::to_string( WDcfg.ChEnableMask0[b] ));
		EUDAQ_WARN("WDcfg.ChEnableMask1["+std::to_string(b)+"]        : "+std::to_string( WDcfg.ChEnableMask1[b] ));
		EUDAQ_WARN("WDcfg.Q_DiscrMask0["+std::to_string(b)+"]         : "+std::to_string( WDcfg.Q_DiscrMask0[b] ));
		EUDAQ_WARN("WDcfg.Q_DiscrMask1["+std::to_string(b)+"]         : "+std::to_string( WDcfg.Q_DiscrMask1[b] ));
		EUDAQ_WARN("WDcfg.Tlogic_Mask0["+std::to_string(b)+"]         : "+std::to_string( WDcfg.Tlogic_Mask0[b] ));
		EUDAQ_WARN("WDcfg.Tlogic_Mask1["+std::to_string(b)+"]         : "+std::to_string( WDcfg.Tlogic_Mask1[b] ));

		//EUDAQ_WARN(mask2string("WDcfg.ChEnableMask0",b,WDcfg.ChEnableMask0[b]));
		//EUDAQ_WARN(mask2string("WDcfg.ChEnableMask1",b,WDcfg.ChEnableMask1[b]));
		//EUDAQ_WARN(mask2string("WDcfg.Q_DiscrMask0" ,b,WDcfg.Q_DiscrMask0[b] ));
		//EUDAQ_WARN(mask2string("WDcfg.Q_DiscrMask1" ,b,WDcfg.Q_DiscrMask1[b] ));
		//EUDAQ_WARN(mask2string("WDcfg.Tlogic_Mask0" ,b,WDcfg.Tlogic_Mask0[b] ));
		//EUDAQ_WARN(mask2string("WDcfg.Tlogic_Mask1" ,b,WDcfg.Tlogic_Mask1[b] ));

		EUDAQ_WARN("WDcfg.HV_Vbias["+std::to_string(b)+"]             : "+std::to_string( WDcfg.HV_Vbias[b] ));
		EUDAQ_WARN("WDcfg.HV_Imax["+std::to_string(b)+"]              : "+std::to_string( WDcfg.HV_Imax[b] ));

if (verbose > 1)
{
		for (int i = 0; i < MAX_NCH; i++) {
			EUDAQ_WARN("WDcfg.ZS_Threshold_LG["+std::to_string(b)+"]["+std::to_string(i)+"]   : "+std::to_string( WDcfg.ZS_Threshold_LG[b][i] ));
			EUDAQ_WARN("WDcfg.ZS_Threshold_HG["+std::to_string(b)+"]["+std::to_string(i)+"]   : "+std::to_string( WDcfg.ZS_Threshold_HG[b][i] ));
			EUDAQ_WARN("WDcfg.QD_FineThreshold["+std::to_string(b)+"]["+std::to_string(i)+"]  : "+std::to_string( WDcfg.QD_FineThreshold[b][i] ));
			EUDAQ_WARN("WDcfg.TD_FineThreshold["+std::to_string(b)+"]["+std::to_string(i)+"]  : "+std::to_string( WDcfg.TD_FineThreshold[b][i] ));
			EUDAQ_WARN("WDcfg.HG_Gain["+std::to_string(b)+"]["+std::to_string(i)+"]           : "+std::to_string( WDcfg.HG_Gain[b][i] ));
			EUDAQ_WARN("WDcfg.LG_Gain["+std::to_string(b)+"]["+std::to_string(i)+"]           : "+std::to_string( WDcfg.LG_Gain[b][i] ));
			EUDAQ_WARN("WDcfg.HV_IndivAdj["+std::to_string(b)+"]["+std::to_string(i)+"]       : "+std::to_string( WDcfg.HV_IndivAdj[b][i] ));
		}
} // if (verbose > 1)

} // if ( (b == 0) || (verbose > 2) )
	}
} // if ( verbose > 0 )
}


std::string mask2string(std::string name, int b, int num)
{
char* temp;
sprintf(temp,"%s[%d] = %08X",name.c_str(),b,num);
return (std::string)temp;
}
