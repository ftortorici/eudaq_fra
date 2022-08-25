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
  memset(&handle, -1, sizeof(handle));
  char ip_address[20];
  char connection_path[40];
  strcpy(ip_address, fers_ip_address.c_str());
  sprintf(connection_path,"eth:%s",ip_address);
  std::cout <<"----3333---- "<<connection_path<<std::endl;
  int ret = FERS_OpenDevice(connection_path, &handle);
  std::cout <<"-------- ret= "<<ret<<std::endl;
  if(ret == 0)
    std::cout <<"Connected to: "<< connection_path<<std::endl;
  else
    EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);
  std::cout <<" ------- RINO ----------   "<<fers_ip_address<<std::endl;  
}


//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
  auto conf = GetConfiguration();
  conf->Print(std::cout);
  std::string m_prova = conf->Get("PROVA", "pippo");
  std::cout <<"######## RINO ###########  "<<m_prova<<std::endl;
  m_plane_id = conf->Get("EX0_PLANE_ID", 0);
  m_ms_busy = std::chrono::milliseconds(conf->Get("EX0_DURATION_BUSY_MS", 1000));
  m_flag_ts = conf->Get("EX0_ENABLE_TIMESTAMP", 0);
  m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
  if(!m_flag_ts && !m_flag_tg){
    EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
    m_flag_ts = false;
    m_flag_tg = true;
  }

  fers_hv_vbias = conf->Get("FERS_HV_Vbias", 0);
  fers_hv_imax = conf->Get("FERS_HV_IMax", 0);
  int retcode = 0; // to store return code from calls to fers
  float fers_dummyvar = 0;
  int retcode_dummy = 0;
  std::cout << "\n**** FERS_HV_Vbias from config: "<< fers_hv_vbias << std::endl;
  retcode = HV_Set_Vbias( handle, fers_hv_vbias); // send to fers
  retcode_dummy = HV_Get_Vbias( handle, &fers_dummyvar); // read back from fers
  if (retcode == 0) {
    EUDAQ_INFO("HV bias set!");
    std::cout << "**** readback HV value: "<< fers_dummyvar << std::endl;
  } else {
    EUDAQ_THROW("HV bias NOT set");
  }
  std::cout << "\n**** FERS_HV_Imax from config: "<< fers_hv_imax <<  std::endl; 
  retcode = HV_Set_Imax( handle, fers_hv_imax);
  retcode_dummy = HV_Get_Imax( handle, &fers_dummyvar); // read back from fers
  if (retcode == 0) {
    EUDAQ_INFO("I max set!");
    std::cout << "**** readback Imax value: "<< fers_dummyvar << std::endl;
  } else {
    EUDAQ_THROW("I max NOT set");
  }

}

//----------DOC-MARK-----BEG*RUN-----DOC-MARK----------
void FERSProducer::DoStartRun(){
  m_exit_of_run = false;
  // here the hardware is told to startup
}

//----------DOC-MARK-----BEG*STOP-----DOC-MARK----------
void FERSProducer::DoStopRun(){
  m_exit_of_run = true;
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
}

//----------DOC-MARK-----BEG*TER-----DOC-MARK----------
void FERSProducer::DoTerminate(){
  m_exit_of_run = true;
  if(m_file_lock){
    fclose(m_file_lock);
    m_file_lock = 0;
  }
}

//----------DOC-MARK-----BEG*LOOP-----DOC-MARK----------
void FERSProducer::RunLoop(){
  auto tp_start_run = std::chrono::steady_clock::now();
  uint32_t trigger_n = 0;
  uint8_t x_pixel = 16;
  uint8_t y_pixel = 16;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> position(0, x_pixel*y_pixel-1);
  std::uniform_int_distribution<uint32_t> signal(0, 255);
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

    std::vector<uint8_t> hit(x_pixel*y_pixel, 0);
    hit[position(gen)] = signal(gen);
    std::vector<uint8_t> data;
    data.push_back(x_pixel);
    data.push_back(y_pixel);
    data.insert(data.end(), hit.begin(), hit.end());
    
    uint32_t block_id = m_plane_id;
    ev->AddBlock(block_id, data);
    SendEvent(std::move(ev));
    trigger_n++;
    std::this_thread::sleep_until(tp_end_of_busy);
  }
}
//----------DOC-MARK-----END*IMP-----DOC-MARK----------
