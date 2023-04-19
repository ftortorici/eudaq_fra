#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include "eudaq/Monitor.hh"

#include "FERSlib.h"
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
      tmpEvent.tstamp_us = FERSunpack32(index, data);
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
      tmpEvent.tstamp_us = FERSunpack32(index, data);
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
      tmpEvent.tstamp_us = FERSunpack32(index, data);
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
      tmpEvent.tstamp_us = FERSunpack32(index, data);
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
// Staircase
// da leggere dal conf
// SHAPING_TIME_25NS
// start
// stop
// step
// dwell time

// struttura staircase
// thr     -> uint16_t
// dwell_s -> uint16_t
// chmean  -> uint32_t
// shaping time usato -> uint8_t
// HV -> float, HV_Get_Vbias( handle, &fers_dummyvar);
// Tor_cnt
// Qor_cnt
// hitcnt[]

int FERS_EUDAQstaircase(int handle, uint16_t stair_shapingt, uint16_t stair_start, uint16_t stair_stop, uint16_t stair_step, float stair_dwell_time)
{
	float HV;
	HV_Get_Vbias( handle, &HV);
	std::cout<<"handle           :"<< handle           << std::endl;
	std::cout<<"stair_shapingt   :"<< stair_shapingt   << std::endl;
	std::cout<<"stair_start      :"<< stair_start      << std::endl;
	std::cout<<"stair_stop       :"<< stair_stop       << std::endl;
	std::cout<<"stair_step       :"<< stair_step       << std::endl;
	std::cout<<"stair_dwell_time :"<< stair_dwell_time << std::endl;
	std::cout<<"HV               :"<< HV               << std::endl;

//	int i, s, brd;
//	uint32_t thr;
//	uint32_t hitcnt[FERSLIB_MAX_NCH], Tor_cnt, Qor_cnt;
//	FILE *st;
//
//	brd = FERS_INDEX(handle);
//	uint16_t start = RunVars.StaircaseCfg[SCPARAM_MIN];
//	uint16_t step = RunVars.StaircaseCfg[SCPARAM_STEP];
//	uint16_t nstep = (RunVars.StaircaseCfg[SCPARAM_MAX] - RunVars.StaircaseCfg[SCPARAM_MIN])/RunVars.StaircaseCfg[SCPARAM_STEP] + 1;
//	float dwell_s = (float)RunVars.StaircaseCfg[SCPARAM_DWELL] / 1000;
//
//	Con_printf("CSm", "Scanning thresholds:\n");
//	Con_printf("Sa", "%02dRunning Staircase (0 %%)", ACQSTATUS_STAIRCASE);
//
//	st = fopen(SCAN_THR_FILENAME, "w");
//	FERS_WriteRegister(handle, a_acq_ctrl, ACQMODE_COUNT);
//	FERS_WriteRegisterSlice(handle, a_acq_ctrl, 27, 29, 0);  // Set counting mode = singles
//	FERS_WriteRegister(handle, a_dwell_time, (uint32_t)(dwell_s * 1e9 / CLK_PERIOD)); 
//	FERS_WriteRegister(handle, a_qdiscr_mask_0, WDcfg.Q_DiscrMask0[brd]); 
//	FERS_WriteRegister(handle, a_qdiscr_mask_1, WDcfg.Q_DiscrMask1[brd]);  
//	FERS_WriteRegister(handle, a_tdiscr_mask_0, WDcfg.Tlogic_Mask0[brd]);  
//	FERS_WriteRegister(handle, a_tdiscr_mask_1, WDcfg.Tlogic_Mask1[brd]);  
//	FERS_WriteRegister(handle, a_citiroc_cfg, 0x00070f20); // Q-discr direct (not latched)
//	FERS_WriteRegister(handle, a_lg_sh_time, SHAPING_TIME_25NS); // Shaping Time LG
//	FERS_WriteRegister(handle, a_hg_sh_time, SHAPING_TIME_25NS); // Shaping Time HG
//	FERS_WriteRegister(handle, a_trg_mask, 0x1); // SW trigger only
//	FERS_WriteRegister(handle, a_t1_out_mask, 0x10); // PTRG (for debug)
//	FERS_WriteRegister(handle, a_t0_out_mask, 0x04); // T-OT (for debug)
//
//	// Start Scan
//	Sleep(100);
//	Con_printf("CSm", "            --------- Rate (cps) ---------\n");
//	Con_printf("CSm", " Adv  Thr     ChMean       T-OR       Q-OR  \n");
//	for(s = nstep; s >= 0; s--) {
//		thr = start + s * step;
//		FERS_WriteRegister(handle, a_qd_coarse_thr, thr);	// Threshold for Q-discr
//		FERS_WriteRegister(handle, a_td_coarse_thr, thr);	// Threshold for T-discr
//		FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);		// set citiroc index = 0
//		FERS_SendCommand(handle, CMD_CFG_ASIC);
//		FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);		// set citiroc index = 1
//		FERS_SendCommand(handle, CMD_CFG_ASIC);
//		Sleep(500);
//		FERS_WriteRegister(handle, a_trg_mask, 0x20); // enable periodic trigger
//		FERS_SendCommand(handle, CMD_RES_PTRG);  // Reset period trigger counter and count for dwell time
//		Sleep((int)(dwell_s/1000 + 200));  // wait for a complete dwell time (+ margin), then read counters
//		FERS_ReadRegister(handle, a_t_or_cnt, &Tor_cnt);
//		FERS_ReadRegister(handle, a_q_or_cnt, &Qor_cnt);
//		if (s < nstep) {  // skip 1st pass 
//			uint64_t chmean = 0;
//			fprintf(st,ebff59c0f2ed23ccf8f1f75895f2d8c7539ec5e8 "%5d ", thr);
//			for(i=0; i<FERSLIB_MAX_NCH; i++) {
//				FERS_ReadRegister(handle, a_hitcnt + (i << 16), &hitcnt[i]);
//				chmean += (uint64_t)hitcnt[i];
//				fprintf(st, "%8.3e ", hitcnt[i]/dwell_s);
//				Stats.Staircase[FERS_INDEX(handle)][i][s] = hitcnt[i]/dwell_s; 
//			}
//			chmean /= FERSLIB_MAX_NCH;
//			int perc = (100 * (nstep-s)) / nstep;
//			if (perc > 100) perc = 100;
//			Con_printf("CSm", "%3d%%%5d  %8.3e  %8.3e  %8.3e\n", perc, thr, chmean/dwell_s, Tor_cnt/dwell_s, Qor_cnt/dwell_s);
//			Con_printf("Sa", "%02dRunning Staircase (%d %%)", ACQSTATUS_STAIRCASE, perc);
//			fprintf(st, "%10.3e %10.3e ", Tor_cnt/dwell_s, Qor_cnt/dwell_s);
//			fprintf(st, "\n");
//		}
//	}
//
//	Con_printf("Sa", "%02dDone\n", ACQSTATUS_STAIRCASE);
//	fclose(st);
    return 0;
}
