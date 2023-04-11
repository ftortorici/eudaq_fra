/////////////////////////////////////////////////////////////////////
//                         2022 Jun 20                             //
//                   author: R. Persianiy                          //
//                email: rinopersiani@gmail.com                    //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////


#include "eudaq/Producer.hh"
#include "FERS_Registers.h"
#include "FERSlib.h"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#include "stdlib.h"
#ifndef _WIN32
#include <sys/file.h>
#endif

#include "FERSpackunpack.h"
#include "configure.h" // for ConfigureFERS
#include "JanusC.h"
Config_t WDcfg;	
RunVars_t RunVars;
int SockConsole;	// 0: use stdio console, 1: use socket console
char ErrorMsg[250];	
int NumBrd=1; // number of boards

//----------DOC-MARK-----BEG*DEC-----DOC-MARK----------
class FERSProducer : public eudaq::Producer {
  public:
  FERSProducer(const std::string & name, const std::string & runcontrol);
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoTerminate() override;
  void DoReset() override;
  void RunLoop() override;
  
  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSProducer");
private:
  bool m_flag_ts;
  bool m_flag_tg;
  uint32_t m_plane_id;
  FILE* m_file_lock;
  std::chrono::milliseconds m_ms_busy;
  bool m_exit_of_run;

  std::string fers_ip_address;  // IP address of the board
  int handle;		 	// Board handle
  float fers_hv_vbias;
  float fers_hv_imax;
  int fers_acq_mode;
  int vhandle[FERSLIB_MAX_NBRD];
};
//----------DOC-MARK-----END*DEC-----DOC-MARK----------
//----------DOC-MARK-----BEG*CON-----DOC-MARK----------
namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<FERSProducer, const std::string&, const std::string&>(FERSProducer::m_id_factory);
}
//----------DOC-MARK-----END*REG-----DOC-MARK----------

FERSProducer::FERSProducer(const std::string & name, const std::string & runcontrol)
  :eudaq::Producer(name, runcontrol), m_file_lock(0), m_exit_of_run(false){  
}

//----------DOC-MARK-----BEG*INI-----DOC-MARK----------
void FERSProducer::DoInitialise(){
  auto ini = GetInitConfiguration();
  std::string lock_path = ini->Get("FERS_DEV_LOCK_PATH", "ferslockfile.txt");
  m_file_lock = fopen(lock_path.c_str(), "a");
#ifndef _WIN32
  if(flock(fileno(m_file_lock), LOCK_EX|LOCK_NB)){ //fail
    EUDAQ_THROW("unable to lock the lockfile: "+lock_path );
  }
#endif

  fers_ip_address = ini->Get("FERS_IP_ADDRESS", "1.0.0.0");
  //memset(&handle, -1, sizeof(handle));
  for (int i=0; i<FERSLIB_MAX_NBRD; i++)
	  vhandle[i] = -1;
  char ip_address[20];
  char connection_path[40];
  strcpy(ip_address, fers_ip_address.c_str());
  sprintf(connection_path,"eth:%s",ip_address);
  std::cout <<"----3333---- "<<connection_path<<std::endl;
  int ret = FERS_OpenDevice(connection_path, &handle);
  std::cout <<"-------- ret= "<<ret<<std::endl;
  if(ret == 0){
    std::cout <<"Connected to: "<< connection_path<<std::endl;
    vhandle[0] = handle;
  } else
    EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);

  // try and init readout
  int ROmode = ini->Get("FERS_RO_MODE",0);
  int allocsize;
  FERS_InitReadout(handle,ROmode,&allocsize);

  std::cout <<" ------- RINO ----------   "<<fers_ip_address
	  <<" handle "<<handle
	  <<" ROmode "<<ROmode<<"  allocsize "<<allocsize<<std::endl;  

}

//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
  auto conf = GetConfiguration();
  conf->Print(std::cout);
  //std::string m_prova = conf->Get("PROVA", "pippo");
  //std::cout <<"######## RINO ###########  "<<m_prova<<std::endl;
  m_plane_id = conf->Get("EX0_PLANE_ID", 0);
  m_ms_busy = std::chrono::milliseconds(conf->Get("EX0_DURATION_BUSY_MS", 1000));
  m_flag_ts = conf->Get("EX0_ENABLE_TIMESTAMP", 0);
  m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
  if(!m_flag_ts && !m_flag_tg){
    EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
    m_flag_ts = false;
    m_flag_tg = true;
  }

  fers_acq_mode = conf->Get("FERS_ACQ_MODE",0);
  //ConfigureFERS(handle, 0); // 1 = soft cfg, no reset
  std::cout<<"in FERSProducer::DoConfigure, handle = "<< handle<< std::endl;

  fers_hv_vbias = conf->Get("FERS_HV_Vbias", 0);
  fers_hv_imax = conf->Get("FERS_HV_IMax", 0);
  int retcode = 0; // to store return code from calls to fers
  float fers_dummyvar = 0;
  int retcode_dummy = 0;
  std::cout << "\n**** FERS_HV_Imax from config: "<< fers_hv_imax <<  std::endl; 
  retcode = HV_Set_Imax( handle, fers_hv_imax);
  retcode = HV_Set_Imax( handle, fers_hv_imax);
  retcode_dummy = HV_Get_Imax( handle, &fers_dummyvar); // read back from fers
  if (retcode == 0) {
    EUDAQ_INFO("I max set!");
    std::cout << "**** readback Imax value: "<< fers_dummyvar << std::endl;
  } else {
    EUDAQ_THROW("I max NOT set");
  }
  std::cout << "\n**** FERS_HV_Vbias from config: "<< fers_hv_vbias << std::endl;
  retcode = HV_Set_Vbias( handle, fers_hv_vbias); // send to fers
  retcode = HV_Set_Vbias( handle, fers_hv_vbias); // send to fers
  retcode_dummy = HV_Get_Vbias( handle, &fers_dummyvar); // read back from fers
  if (retcode == 0) {
    EUDAQ_INFO("HV bias set!");
    std::cout << "**** readback HV value: "<< fers_dummyvar << std::endl;
  } else {
    EUDAQ_THROW("HV bias NOT set");
  }

  sleep(1);
  HV_Set_OnOff(handle, 1); // set HV on

}

//----------DOC-MARK-----BEG*RUN-----DOC-MARK----------
void FERSProducer::DoStartRun(){
  m_exit_of_run = false;
  // here the hardware is told to startup
  //int status = FERS_StartAcquisition(&handle, NumBrd, fers_acq_mode);
  FERS_SendCommand( handle, CMD_ACQ_START );
  //EUDAQ_INFO("status of FERS_StartAcquisition = "+std::to_string(status));
  EUDAQ_INFO("FERS_ReadoutStatus (0=idle, 1=running) = "+std::to_string(FERS_ReadoutStatus));
}

//----------DOC-MARK-----BEG*STOP-----DOC-MARK----------
void FERSProducer::DoStopRun(){
  m_exit_of_run = true;
// Inputs:		handle = device handles (af all boards)
// 				NumBrd = number of boards to start
//				StartMode = start mode (Async, T0/T1 chain, TDlink)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
  //int status = FERS_StopAcquisition(vhandle, NumBrd, fers_acq_mode);
  FERS_SendCommand( handle, CMD_ACQ_STOP );
  //EUDAQ_INFO("status of FERS_StopAcquisition = "+std::to_string(status));
  EUDAQ_INFO("FERS_ReadoutStatus (0=idle, 1=running) = "+std::to_string(FERS_ReadoutStatus));
}

//----------DOC-MARK-----BEG*RST-----DOC-MARK----------
void FERSProducer::DoReset(){
  m_exit_of_run = true;
  if(m_file_lock){
#ifndef _WIN32
    flock(fileno(m_file_lock), LOCK_UN);
#endif
    fclose(m_file_lock);
    m_file_lock = 0;
  }
  m_ms_busy = std::chrono::milliseconds();
  m_exit_of_run = false;
  HV_Set_OnOff( handle, 0); // set HV off
}

//----------DOC-MARK-----BEG*TER-----DOC-MARK----------
void FERSProducer::DoTerminate(){
  m_exit_of_run = true;
  if(m_file_lock){
    fclose(m_file_lock);
    m_file_lock = 0;
  }
  FERS_CloseReadout(handle);
  HV_Set_OnOff( handle, 0); // set HV off
}

//----------DOC-MARK-----BEG*LOOP-----DOC-MARK----------
void FERSProducer::RunLoop(){
  auto tp_start_run = std::chrono::steady_clock::now();
  uint32_t trigger_n = 0;
  uint8_t x_pixel = 8;
  uint8_t y_pixel = 8;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> position(0, x_pixel*y_pixel-1);
  std::uniform_int_distribution<uint32_t> signal(0, 63);
  while(!m_exit_of_run){
    auto ev = eudaq::Event::MakeUnique("FERSRaw");
    ev->SetTag("Plane ID", std::to_string(m_plane_id));
    auto tp_trigger = std::chrono::steady_clock::now();
    auto tp_end_of_busy = tp_trigger + m_ms_busy;
    if(m_flag_ts){
      std::chrono::nanoseconds du_ts_beg_ns(tp_trigger - tp_start_run);
      std::chrono::nanoseconds du_ts_end_ns(tp_end_of_busy - tp_start_run);
      ev->SetTimestamp(du_ts_beg_ns.count(), du_ts_end_ns.count());
    }
    if(m_flag_tg)
      ev->SetTriggerN(trigger_n);

    std::vector<uint8_t> hit(2*x_pixel*y_pixel, 0);
    hit[position(gen)] = signal(gen);

    int nchan = x_pixel*y_pixel;
    int DataQualifier = -1;
    double tstamp_us = -1;
    int nb = -1;
    void *Event;
    int bindex = -1;
    int status = -1;

    // real data
    // 
    status = FERS_GetEvent(vhandle, &bindex, &DataQualifier, &tstamp_us, &Event, &nb);
    //
    std::cout<<"--FERS_ReadoutStatus (0=idle, 1=running) = " << FERS_ReadoutStatus <<std::endl;
    std::cout<<"--status of FERS_GetEvent (0=No Data, 1=Good Data 2=Not Running, <0 = error) = "<< std::to_string(status)<<std::endl;
    std::cout<<"  --bindex = "<< std::to_string(bindex) <<" tstamp_us = "<< std::to_string(tstamp_us) <<std::endl;
    std::cout<<"  --DataQualifier = "<< std::to_string(DataQualifier) +" nb = "<< std::to_string(nb) <<std::endl;

    // simulated spectroscopy data
    // 
    DataQualifier = DTQ_SPECT;
    SpectEvent_t *EventSpect = (SpectEvent_t*)Event;
    EventSpect->tstamp_us  = (double)  1000;
    EventSpect->trigger_id = (uint64_t) 100;
    EventSpect->chmask     = (uint64_t) 120;
    EventSpect->qdmask     = (uint64_t) 130;
    for(int i=0; i<nchan; i++){
      EventSpect->energyHG[i] = (uint16_t)  200 + i;
      EventSpect->energyLG[i] = (uint16_t)  100 + i;
      EventSpect->tstamp[i]   = (uint32_t) 1000 + i;
      EventSpect->ToT[i]      = (uint16_t)   10 + i;
    }
    nb = sizeof(Event)/8;




    // event creation
    std::vector<uint8_t> data;
    data.push_back(x_pixel);
    data.push_back(y_pixel);
    //
    // metadata
    //
    // convert fers ip address to numbers
    char ip_address[20];
    uint8_t ip_temp = 0;
    char c;
    strcpy(ip_address, fers_ip_address.c_str());
    std::cout<<"*-*-* fers ip is "<< ip_address << std::endl;
    for (int i=0; i<20; i++){
	    c = ip_address[i];
	    if ( c == '.' ) {
		    data.push_back( ip_temp );
		    ip_temp = 0;
	    } else {
		    if ( (c >= '0') && ( c <= '9') ) {
			    ip_temp = 10*ip_temp + (int)c - '0';
		    }
	    }
    }
    data.push_back( ip_temp );
    // serial number
    int sernum=0;
    HV_Get_SerNum(handle, &sernum);
    data.push_back((uint8_t)sernum);
    //handle
    data.push_back((uint8_t)handle);
    // data qualifier
    data.push_back((uint8_t)DataQualifier);
    // number of byte of event raw data
    data.push_back((uint8_t)nb);

    //data.insert(data.end(), hit.begin(), hit.end());

    if ( DataQualifier == DTQ_SPECT) {
      FERSpackevent(Event, DataQualifier, &data);
      std::cout<<"Packed spectroscopy event"<<std::endl; 
//      SpectEvent_t *EventSpect = (SpectEvent_t*)Event;
//      tstamp_us  = EventSpect->tstamp_us ;
//      uint64_t trigger_id = EventSpect->trigger_id;
//      uint64_t chmask     = EventSpect->chmask    ;
//      uint64_t qdmask     = EventSpect->qdmask    ;
//      uint16_t energyHG[nchan];
//      uint16_t energyLG[nchan];
//      uint32_t tstamp[nchan]  ;
//      uint16_t ToT[nchan]     ;
//      for (size_t i = 0; i<nchan; i++){
//        energyHG[i] = EventSpect->energyHG[i];
//        energyLG[i] = EventSpect->energyLG[i];
//        tstamp[i]   = EventSpect->tstamp[i]  ;
//        ToT[i]      = EventSpect->ToT[i]     ;
//        //hit.at(  i) = energyHG[i];
//        hit.at(2*i) = energyHG[i];
//        hit.at(2*i+1) = energyHG[i]>>8;
//      }
//      //uint32_t data_raw_0; // chans 0..31
//      //uint32_t data_raw_1; // chans 32..63
//      //for (int ii=0; ii < nchan/2; ++ii) {
//      //	  FERS_ReadRegister(handle, INDIV_ADDR(a_channel_mask_0, ii), &data_raw_0);
//      //	  FERS_ReadRegister(handle, INDIV_ADDR(a_channel_mask_1, ii), &data_raw_1);
//      //	  hit.at( ii          ) = data_raw_0;
//      //	  hit.at( ii + nchan/2) = data_raw_1;
//      //    }
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


    uint32_t block_id = m_plane_id;
    ev->AddBlock(block_id, data);
    SendEvent(std::move(ev));
    trigger_n++;
    std::this_thread::sleep_until(tp_end_of_busy);
  }
}
//----------DOC-MARK-----END*IMP-----DOC-MARK----------
