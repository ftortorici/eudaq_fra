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
	if(m_en_print) 
		ev->Print(std::cout);

	float vmon, imon;

	// dump su log
	std::string printme="";
	size_t nblocks= ev->NumBlocks();
	EUDAQ_WARN("number of blocks: "+std::to_string(nblocks));
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
		//uint8_t nb   = block[9];

		std::vector<uint8_t> data(block.begin()+9, block.end());

		printme = "Monitor > received a " + std::to_string(x_pixel) + " x " + std::to_string(y_pixel) +" event from FERS @ ip "
			+ std::to_string(uip0) +"."
			+ std::to_string(uip1) +"."
			+ std::to_string(uip2) +"."
			+ std::to_string(uip3)
			+" serial# "+ std::to_string(sernum);
		EUDAQ_WARN(printme);

		printme = "handle = "+std::to_string(handle)
			+" data qualifier = "+std::to_string(dataq)
			//+" #bytes = "+std::to_string(nb)
			;
		EUDAQ_WARN(printme);

		// all the event types
		SpectEvent_t    EventSpect;
		ListEvent_t     EventList;
		CountingEvent_t EventCount;
		WaveEvent_t     EventWave;
		TestEvent_t     EventTest;
		// all struct vars
		// use them to print etc
		int nchan = x_pixel*y_pixel;
		double   tstamp_us                      ;
		uint64_t trigger_id                     ;
		uint64_t chmask                         ;
		uint64_t qdmask                         ;
		uint16_t energyHG[nchan]                ;
		uint16_t energyLG[nchan]                ;
		uint32_t tstamp[nchan]                  ;
		uint16_t ToT[nchan]                     ;
		uint32_t counts[nchan]                  ;
		uint32_t t_or_counts                    ;
		uint32_t q_or_counts                    ;
		uint16_t ns                             ;
		uint16_t wave_hg[MAX_WAVEFORM_LENGTH]   ;
		uint16_t wave_lg[MAX_WAVEFORM_LENGTH]   ;
		uint8_t  dig_probes[MAX_WAVEFORM_LENGTH];
		uint16_t nhits                          ;
		uint8_t  channel[MAX_LIST_SIZE]         ;
		uint16_t nwords                         ;
		uint32_t test_data[MAX_TEST_NWORDS]     ;

		std::vector<uint16_t> hit; // fill it with energyHG or whatever
		std::string hitname=""; // display name for hit
		int hitx, hity; // how many rows and columns to display

		EUDAQ_WARN("Monitor > ---------- start dumping");
		switch ( dataq ) {
			case DTQ_SPECT:
				EUDAQ_WARN("Trying to decode SPECT event");
				// fill the relevant vars
				EventSpect = FERSunpack_spectevent(&data);
				tstamp_us      = EventSpect.tstamp_us ;
				trigger_id     = EventSpect.trigger_id;
				chmask         = EventSpect.chmask    ;
				qdmask         = EventSpect.qdmask    ;
				for (int i=0; i<nchan; i++)
				{
					energyHG[i] = EventSpect.energyHG[i];
					energyLG[i] = EventSpect.energyLG[i];
					// hit is dumped below, for test purposes
					hit.push_back( energyHG[i] );
					hitname="energyHG";
				}
				hity = y_pixel;
				hitx = x_pixel;
				// dump the scalar ones
				EUDAQ_WARN("tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_WARN("trigger_id: "+std::to_string(trigger_id));
				EUDAQ_WARN("chmask    : "+std::to_string(chmask    ));
				EUDAQ_WARN("qdmask    : "+std::to_string(qdmask    ));
				break;
			case DTQ_TIMING:
				EUDAQ_WARN("Trying to decode TIMING event");
				EventList = FERSunpack_listevent(&data);
				nhits = EventList.nhits;
				for (int i=0; i<MAX_LIST_SIZE; i++)
				{
					channel[i] = EventList.channel[i];
					tstamp[i] = EventList.tstamp[i];
					ToT[i]  = EventList.ToT[i];
					// hit is dumped below,for test purposes
					hit.push_back( ToT[i] );
					hitname="ToT";
				}
				hity = y_pixel;
				hitx = MAX_LIST_SIZE / hity;
				// dump the scalar ones
				EUDAQ_WARN("nhits : "+std::to_string(nhits ));
				break;
			case DTQ_COUNT:
				EUDAQ_WARN("Trying to decode COUNTING event");
				EventCount = FERSunpack_countevent(&data);
				tstamp_us   = EventCount.tstamp_us;
				trigger_id  = EventCount.trigger_id;
				chmask      = EventCount.chmask;
				for (int i=0; i<nchan; i++)
				{
					counts[i]   = EventCount.counts[i];
					// hit is dumped below,for test purposes
					hit.push_back( counts[i] );
					hitname="counts";
				}
				hity = y_pixel;
				hitx = x_pixel;
				t_or_counts = EventCount.t_or_counts;
				q_or_counts = EventCount.q_or_counts;
				// dump the scalar ones
				EUDAQ_WARN("tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_WARN("trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_WARN("chmask : "+std::to_string(chmask ));
				EUDAQ_WARN("t_or_counts : "+std::to_string(t_or_counts ));
				EUDAQ_WARN("q_or_counts : "+std::to_string(q_or_counts ));
				break;
			case DTQ_WAVE:
				EUDAQ_WARN("Trying to decode WAVE event");
				EventWave = FERSunpack_waveevent(&data);
				tstamp_us = EventWave.tstamp_us;
				trigger_id= EventWave.trigger_id;
				ns        = EventWave.ns;
				for (int i=0; i<MAX_WAVEFORM_LENGTH; i++)
				{
					wave_hg[i]= EventWave.wave_hg[i];
					wave_lg[i]= EventWave.wave_lg[i];
					dig_probes[i] = EventWave.dig_probes[i];
				}
				hity = y_pixel;
				hitx = MAX_WAVEFORM_LENGTH/hity;
				// dump the scalar ones
				EUDAQ_WARN("tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_WARN("trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_WARN("ns : "+std::to_string(ns ));
				break;
			case DTQ_TSPECT:
				EUDAQ_WARN("Trying to decode TSPECT event");
				EventSpect = FERSunpack_tspectevent(&data);
				// fill the relevant vars
				tstamp_us      = EventSpect.tstamp_us;
				trigger_id     = EventSpect.trigger_id;
				chmask         = EventSpect.chmask;
				qdmask         = EventSpect.qdmask;
				for (int i=0; i<nchan; i++)
				{
					energyHG[i] = EventSpect.energyHG[i];
					energyLG[i] = EventSpect.energyLG[i];
					tstamp[i]   = EventSpect.tstamp[i]  ;
					ToT[i]      = EventSpect.ToT[i]     ;
					// hit is dumped below,for test purposes
					hit.push_back( energyHG[i] );
					hitname="energyHG";
				}
				hity = y_pixel;
				hitx = x_pixel;
				// dump the scalar ones
				EUDAQ_WARN("tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_WARN("trigger_id: "+std::to_string(trigger_id));
				EUDAQ_WARN("chmask    : "+std::to_string(chmask    ));
				EUDAQ_WARN("qdmask    : "+std::to_string(qdmask    ));
				break;
			case DTQ_TEST:
				EUDAQ_WARN("Trying to decode TEST event");
				EventTest = FERSunpack_testevent(&data);
				tstamp_us = EventTest.tstamp_us;
				trigger_id = EventTest.trigger_id;
				nwords = EventTest.nwords;
				for (int i=0; i<MAX_TEST_NWORDS; i++)
				{
					test_data[i] = EventTest.test_data[i];
					// hit is dumped below,for test purposes
					hit.push_back( test_data[i] );
					hitname="test_data";
				}
				hity = MAX_TEST_NWORDS;
				hitx = 1;
				// dump the scalar ones
				EUDAQ_WARN("tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_WARN("trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_WARN("nwords : "+std::to_string(nwords ));
		}


		// dumping of hit
		EUDAQ_WARN(hitname);
		for(size_t row = 0; row < hitx; ++row) {
			printme="";
			for(size_t col = 0; col < hity; ++col){
				printme += std::to_string(hit[col+row*x_pixel]) +" ";
			};
			EUDAQ_WARN(printme);
		}
		//EUDAQ_WARN(printme);

	}
	EUDAQ_WARN("Monitor > ---------- end dumping");



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

