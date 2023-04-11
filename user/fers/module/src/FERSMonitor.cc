#include "eudaq/Monitor.hh"
#include "eudaq/StandardEvent.hh"
#include "eudaq/StdEventConverter.hh"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>

#include <stdio.h>
#include "eudaq/StdEventConverter.hh"
#include "eudaq/RawEvent.hh"
#include "FERSlib.h"

#include "FERSpackunpack.h"

class FERSEventConverter: public eudaq::StdEventConverter{
public:
  bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSRaw");
};
bool FERSEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
//bool FERSMonitor::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2) const{
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
  size_t nblocks= ev->NumBlocks();
  auto block_n_list = ev->GetBlockNumList();
  for(auto &block_n: block_n_list){
    std::vector<uint8_t> block = ev->GetBlock(block_n);
    if(block.size() < 2)
      EUDAQ_THROW("Unknown data");
    uint8_t x_pixel = block[0];
    uint8_t y_pixel = block[1];
    std::vector<uint8_t> hit(block.begin()+2, block.end());
    if(hit.size() != x_pixel*y_pixel)
      EUDAQ_THROW("Unknown data");
    eudaq::StandardPlane plane(block_n, "fers_plane", "fers_plane");
    plane.SetSizeZS(hit.size(), 1, 0);
    for(size_t i = 0; i < y_pixel; ++i) {
      for(size_t n = 0; n < x_pixel; ++n){
							plane.PushPixel(n, i , hit[n+i*x_pixel]);
      }
    }
    d2->AddPlane(plane);
  }
  return true;
}



//----------DOC-MARK-----BEG*DEC-----DOC-MARK----------
class FERSMonitor : public eudaq::Monitor {
public:
  FERSMonitor(const std::string & name, const std::string & runcontrol);
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoTerminate() override;
  void DoReset() override;
  void DoReceive(eudaq::EventSP ev) override;

  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSMonitor");

	bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2) const;
  
private:
  bool m_en_print;
  bool m_en_std_converter;
  bool m_en_std_print;
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Monitor>::
    Register<FERSMonitor, const std::string&, const std::string&>(FERSMonitor::m_id_factory);

  auto dummy1 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<FERSEventConverter>(FERSEventConverter::m_id_factory);
}

FERSMonitor::FERSMonitor(const std::string & name, const std::string & runcontrol)
  :eudaq::Monitor(name, runcontrol){  
}

void FERSMonitor::DoInitialise(){
  auto ini = GetInitConfiguration();
  ini->Print(std::cout);
}

void FERSMonitor::DoConfigure(){
  auto conf = GetConfiguration();
  conf->Print(std::cout);
  m_en_print = conf->Get("EX0_ENABLE_PRINT", 1);
  m_en_std_converter = conf->Get("EX0_ENABLE_STD_CONVERTER", 0);
  m_en_std_print = conf->Get("EX0_ENABLE_STD_PRINT", 0);

	char printme[200] = "";
	std::sprintf( printme, "monitor > print = %d std_converter = %d std_print = %d", m_en_print, m_en_std_converter, m_en_print);
	EUDAQ_WARN(printme);
}

void FERSMonitor::DoStartRun(){
}

void FERSMonitor::DoStopRun(){
}

void FERSMonitor::DoReset(){
}

void FERSMonitor::DoTerminate(){
}

void FERSMonitor::DoReceive(eudaq::EventSP ev){
	if(m_en_print) {
		ev->Print(std::cout);

		float vmon, imon;

		// dump su log
		std::string printme="";
		size_t nblocks= ev->NumBlocks();
		//EUDAQ_WARN("number of blocks: "+std::to_string(nblocks));
		auto block_n_list = ev->GetBlockNumList();
		
		for(auto &block_n: block_n_list){
			std::vector<uint8_t> block = ev->GetBlock(block_n);

			if(block.size() < 2)
							EUDAQ_THROW("Unknown data");
			uint8_t x_pixel = block[0];
			uint8_t y_pixel = block[1];

			uint8_t uip0 = block[2];
			uint8_t uip1 = block[3];
			uint8_t uip2 = block[4];
			uint8_t uip3 = block[5];
			uint8_t sernum=block[6];
			uint8_t handle=block[7];
			uint8_t dataq= block[8];
			uint8_t nb   = block[9];

			std::vector<uint8_t> data(block.begin()+10, block.end());
			//if(data.size() != 2*x_pixel*y_pixel)
			//				EUDAQ_THROW("Unknown data");
			printme = "Monitor > received a " + std::to_string(x_pixel) + " x " + std::to_string(y_pixel) +" event from FERS @ ip "
				+ std::to_string(uip0) +"."
				+ std::to_string(uip1) +"."
				+ std::to_string(uip2) +"."
				+ std::to_string(uip3)
			       	+" serial# "+ std::to_string(sernum);
			EUDAQ_WARN(printme);

			printme = "handle = "+std::to_string(handle)
				+" data qualifier = "+std::to_string(dataq)
				+" #bytes = "+std::to_string(nb);
			EUDAQ_WARN(printme);

      int nchan = 64;
      void *Event;
      FERSunpackevent(Event, dataq, &data);

			std::vector<uint16_t> hit; // energyHG etc
      if ( dataq == DTQ_SPECT )
      {
        SpectEvent_t *EventSpect = (SpectEvent_t*)Event;
        for (int i=0; i<nchan; i++)
        {
          hit.push_back( EventSpect->energyHG[i] );
        }
      }

			//for (int i = 0; i<data.size()/2; i++)
			//	hit.push_back(data.at( 2*i ) + data.at(2*i+1)*256);

			// just for fun and check connection
			HV_Get_Vmon( handle, &vmon);
			HV_Get_Vmon( handle, &vmon);
			HV_Get_Imon( handle, &imon);
			HV_Get_Imon( handle, &imon);
			EUDAQ_WARN("Current Vmon = " + std::to_string(vmon) + " V, Imon = " + std::to_string(imon) +" ??A");

			EUDAQ_WARN("Monitor > ---------- start dumping");
			for(size_t i = 0; i < y_pixel; ++i) {
			printme="";
				//for(size_t n = 0; n < 2*x_pixel; ++n){
					//printme += std::to_string(data[n+i*x_pixel]) +" ";
				for(size_t n = 0; n < x_pixel; ++n){
					printme += std::to_string(hit[n+i*x_pixel]) +" ";
				};
				EUDAQ_WARN(printme);
			}
			//EUDAQ_WARN(printme);

		}
		EUDAQ_WARN("Monitor > ---------- end dumping");
		


	}
	if(m_en_std_converter){
		auto stdev = std::dynamic_pointer_cast<eudaq::StandardEvent>(ev);
		if(!stdev){
			stdev = eudaq::StandardEvent::MakeShared();
			eudaq::StdEventConverter::Convert(ev, stdev, nullptr); //no conf
			//Converting(ev, stdev);
			EUDAQ_WARN("Monitor > event received!");
			if(m_en_std_print){
				stdev->Print(std::cout);

			}
		}
	}
}
