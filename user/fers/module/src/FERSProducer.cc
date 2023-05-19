/////////////////////////////////////////////////////////////////////
//                         2023 May 08                             //
//                   authors: R. Persiani & F. Tortorici           //
//                email: rinopersiani@gmail.com                    //
//                email: francesco.tortorici@ct.infn.it            //
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

#include "FERS_EUDAQ.h"
#include "configure.h"
#include "JanusC.h"
RunVars_t RunVars;
int SockConsole;	// 0: use stdio console, 1: use socket console
char ErrorMsg[250];	
//int NumBrd=2; // number of boards

Config_t WDcfg;

struct shmseg *shmp;
int shmid;

//#include<sys/ipc.h>
//#include<sys/shm.h>
//#include<sys/types.h>
//#define SHM_KEY 0x1234
//struct shmseg {
//	int connectedboards = 0;
//};

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
		std::string fers_id;
		int handle =-1;		 	// Board handle
		float fers_hv_vbias;
		float fers_hv_imax;
		int fers_acq_mode;
		int vhandle[FERSLIB_MAX_NBRD];
		// staircase params
		uint8_t stair_do;
		uint16_t stair_start, stair_stop, stair_step, stair_shapingt;
		uint32_t stair_dwell_time;


		//struct shmseg *shmp;
		//int shmid;
		int brd; // current board

};
//----------DOC-MARK-----END*DEC-----DOC-MARK----------
//----------DOC-MARK-----BEG*CON-----DOC-MARK----------
namespace{
	auto dummy0 = eudaq::Factory<eudaq::Producer>::
		Register<FERSProducer, const std::string&, const std::string&>(FERSProducer::m_id_factory);
}
//----------DOC-MARK-----END*REG-----DOC-MARK----------

FERSProducer::FERSProducer(const std::string & name, const std::string & runcontrol)
	:eudaq::Producer(name, runcontrol), m_file_lock(0), m_exit_of_run(false)
{  
}

//----------DOC-MARK-----BEG*INI-----DOC-MARK----------
void FERSProducer::DoInitialise(){
	// see https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_shared_memory.htm
	shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
	if (shmid == -1) {
		perror("Shared memory");
	}
	EUDAQ_WARN("producer constructor: shmid = "+std::to_string(shmid));

	// Attach to the segment to get a pointer to it.
	shmp = (shmseg*)shmat(shmid, NULL, 0);
	if (shmp == (void *) -1) {
		perror("Shared memory attach");
	}

	initshm( shmid );
	//shmp->connectedboards = 0;


	auto ini = GetInitConfiguration();
	std::string lock_path = ini->Get("FERS_DEV_LOCK_PATH", "ferslockfile.txt");
	m_file_lock = fopen(lock_path.c_str(), "a");
#ifndef _WIN32
	if(flock(fileno(m_file_lock), LOCK_EX|LOCK_NB)){ //fail
		EUDAQ_THROW("unable to lock the lockfile: "+lock_path );
	}
#endif

	fers_ip_address = ini->Get("FERS_IP_ADDRESS", "1.0.0.0");
	fers_id = ini->Get("FERS_ID","");	
	//memset(&handle, -1, sizeof(handle));
	for (int i=0; i<FERSLIB_MAX_NBRD; i++)
		vhandle[i] = -1;
	char ip_address[20];
	char connection_path[40];
	strcpy(ip_address, fers_ip_address.c_str());
	sprintf(connection_path,"eth:%s",ip_address);
	std::cout <<"----3333---- "<<connection_path<<std::endl;
	int ret = FERS_OpenDevice(connection_path, &handle);

	std::cout <<"-------- ret= "<<ret<<" handle = "<<handle<<std::endl;
	if(ret == 0){
		std::cout <<"Connected to: "<< connection_path<<std::endl;
		vhandle[WDcfg.NumBrd] = handle;
		brd=shmp->connectedboards;
		shmp->connectedboards++;
		shmp->handle[brd] = handle;
		//WDcfg.NumBrd++;
	} else
		EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);

	// Readout Mode
	// 0	// Disable sorting 
	// 1	// Enable event sorting by Trigger Tstamp 
	// 2	// Enable event sorting by Trigger ID
	int ROmode = ini->Get("FERS_RO_MODE",0);
	int allocsize;
	FERS_InitReadout(handle,ROmode,&allocsize);

	// fill shared struct
	std::string fers_prodid = ini->Get("FERS_PRODID","no prod ID");	
	strcpy(shmp->IP[brd],       fers_ip_address.c_str());
	strcpy(shmp->desc[brd],     std::to_string(FERS_pid(handle)).c_str());
	strcpy(shmp->location[brd], fers_id.c_str());
	strcpy(shmp->producer[brd], fers_prodid.c_str());

	std::cout <<" ------- RINO ----------   "<<fers_ip_address
		<<" handle "<<handle
		<<" ROmode "<<ROmode<<"  allocsize "<<allocsize
		<<"Connected to: "<< connection_path 
		<< " "<<fers_id<<std::endl;
	EUDAQ_INFO("Connected to handle "+std::to_string(handle)
			+" ip "+fers_ip_address+" "+fers_id
			+" connectedboards "+std::to_string(shmp->connectedboards)
		  );
	EUDAQ_WARN("check shared on board "+std::to_string(brd)+": "
			+std::string(shmp->IP[brd])
			+"*"+std::string(shmp->desc[brd])
			+"*"+std::string(shmp->location[brd])
			+"*"+std::string(shmp->producer[brd])
		  );

}

//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
	auto conf = GetConfiguration();
	conf->Print(std::cout);
	m_plane_id = conf->Get("EX0_PLANE_ID", 0);
	m_ms_busy = std::chrono::milliseconds(conf->Get("EX0_DURATION_BUSY_MS", 1000));
	m_flag_ts = conf->Get("EX0_ENABLE_TIMESTAMP", 0);
	m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
	if(!m_flag_ts && !m_flag_tg){
		EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
		m_flag_ts = false;
		m_flag_tg = true;
	}
	//std::string fers_conf_dir = conf->Get("FERS_CONF_DIR",".");
	std::string fers_conf_filename= conf->Get("FERS_CONF_FILE","NOFILE");
	//std::string conf_filename = fers_conf_dir + fers_conf_file;

	fers_acq_mode = conf->Get("FERS_ACQ_MODE",0);
	std::cout<<"in FERSProducer::DoConfigure, handle = "<< handle<< std::endl;

	int ret = -1; // to store return code from calls to fers
	//EUDAQ_WARN(fers_conf_dir);
	//EUDAQ_WARN(fers_conf_filename);
	FILE* conf_file = fopen(fers_conf_filename.c_str(),"r");
	if (conf_file == NULL) 
	{
		EUDAQ_THROW("unable to open config file "+fers_conf_filename);
	} else {
		ret = ParseConfigFile(conf_file,&WDcfg, 1);
		if (ret != 0)
		{ 
			fclose(conf_file);
			EUDAQ_THROW("Parsing failed");
		}
	}
	fclose(conf_file);

	ret = ConfigureFERS(handle, 0); // 0 = hard, 1 = soft (no acq restart)
	if (ret != 0)
	{
		EUDAQ_THROW("ConfigureFERS failed");
	}

	fers_hv_vbias = conf->Get("FERS_HV_Vbias", 0);
	fers_hv_imax = conf->Get("FERS_HV_IMax", 0);
	float fers_dummyvar = 0;
	int ret_dummy = 0;
	std::cout << "\n**** FERS_HV_Imax from config: "<< fers_hv_imax <<  std::endl; 
	ret = HV_Set_Imax( handle, fers_hv_imax);
	ret = HV_Set_Imax( handle, fers_hv_imax);
	ret_dummy = HV_Get_Imax( handle, &fers_dummyvar); // read back from fers
	if (ret == 0) {
		EUDAQ_INFO("I max set!");
		std::cout << "**** readback Imax value: "<< fers_dummyvar << std::endl;
	} else {
		EUDAQ_THROW("I max NOT set");
	}
	std::cout << "\n**** FERS_HV_Vbias from config: "<< fers_hv_vbias << std::endl;
	ret = HV_Set_Vbias( handle, fers_hv_vbias); // send to fers
	ret = HV_Set_Vbias( handle, fers_hv_vbias); // send to fers
	ret_dummy = HV_Get_Vbias( handle, &fers_dummyvar); // read back from fers
	if (ret == 0) {
		EUDAQ_INFO("HV bias set!");
		std::cout << "**** readback HV value: "<< fers_dummyvar << std::endl;
	} else {
		EUDAQ_THROW("HV bias NOT set");
	}

	stair_do = (bool)(conf->Get("stair_do",0));
	stair_shapingt = (uint16_t)(conf->Get("stair_shapingt",0));
	stair_start = (uint16_t)(conf->Get("stair_start",0));
	stair_stop  = (uint16_t)(conf->Get("stair_stop",0));
	stair_step  = (uint16_t)(conf->Get("stair_step",0));
	stair_dwell_time  = (uint32_t)(conf->Get("stair_dwell_time",0));


	// put things in shared structure
	shmp->HVbias[brd] = fers_hv_vbias;
	std::string temp=conf->Get("EUDAQ_DC","no data collector");
	strcpy(shmp->collector[brd],temp.c_str());
	EUDAQ_WARN("check shared in board "+std::to_string(brd)+": HVbias = "+std::to_string(shmp->HVbias[brd])+" collector="+std::string(shmp->collector[brd]));



	sleep(1);
	HV_Set_OnOff(handle, 1); // set HV on

}

//----------DOC-MARK-----BEG*RUN-----DOC-MARK----------
void FERSProducer::DoStartRun(){
	m_exit_of_run = false;
	// here the hardware is told to startup
	FERS_SendCommand( handle, CMD_ACQ_START );
	EUDAQ_INFO("FERS_ReadoutStatus (0=idle, 1=running) = "+std::to_string(FERS_ReadoutStatus));
}

//----------DOC-MARK-----BEG*STOP-----DOC-MARK----------
void FERSProducer::DoStopRun(){
	m_exit_of_run = true;
	FERS_SendCommand( handle, CMD_ACQ_STOP );
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
	FERS_CloseDevice(handle);	

	// free shared memory
	if (shmdt(shmp) == -1) {
		perror("shmdt");
	}
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

		// staircase?
		static bool stairdone = false;
		if (stair_do)
		{	
			std::vector<uint8_t> hit(x_pixel*y_pixel, 0);
			hit[position(gen)] = signal(gen);

			int nchan = x_pixel*y_pixel;
			int DataQualifier = -1;
			void *Event;


			auto ev = eudaq::Event::MakeUnique("FERSProducer");
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


			if (!stairdone)
			{
				uint32_t Q_DiscrMask0 = 0xFFFFFFFF;
				uint32_t Q_DiscrMask1 = 0xFFFFFFFF;
				uint32_t Tlogic_Mask0 = 0xFFFFFFFF;
				uint32_t Tlogic_Mask1 = 0xFFFFFFFF;
				float HV;
				HV_Get_Vbias( handle, &HV);
				std::cout<<"handle           :"<< handle           << std::endl;
				std::cout<<"stair_shapingt   :"<< stair_shapingt   << std::endl;
				std::cout<<"stair_start      :"<< stair_start      << std::endl;
				std::cout<<"stair_stop       :"<< stair_stop       << std::endl;
				std::cout<<"stair_step       :"<< stair_step       << std::endl;
				std::cout<<"stair_dwell_time :"<< stair_dwell_time << std::endl;
				std::cout<<"HV               :"<< HV               << std::endl;

				StaircaseEvent_t StaircaseEvent;
				std::vector<uint8_t> data;

				int i, s, board;
				uint32_t thr;
				uint32_t hitcnt[FERSLIB_MAX_NCH], Tor_cnt, Qor_cnt;

				board = FERS_INDEX(handle);
				uint16_t nstep = (stair_stop - stair_start)/stair_step + 1;
				float dwell_s = (float)stair_dwell_time / 1000;
				std::cout<<"dwell_s :"<< dwell_s << std::endl;

				FERS_WriteRegister(handle, a_acq_ctrl, ACQMODE_COUNT);
				FERS_WriteRegisterSlice(handle, a_acq_ctrl, 27, 29, 0);  // Set counting mode = singles
				FERS_WriteRegister(handle, a_dwell_time, (uint32_t)(dwell_s * 1e9 / CLK_PERIOD)); 
				FERS_WriteRegister(handle, a_qdiscr_mask_0, Q_DiscrMask0); 
				FERS_WriteRegister(handle, a_qdiscr_mask_1, Q_DiscrMask1);  
				FERS_WriteRegister(handle, a_tdiscr_mask_0, Tlogic_Mask0);  
				FERS_WriteRegister(handle, a_tdiscr_mask_1, Tlogic_Mask1);  
				FERS_WriteRegister(handle, a_citiroc_cfg, 0x00070f20); // Q-discr direct (not latched)
				FERS_WriteRegister(handle, a_lg_sh_time, stair_dwell_time); // Shaping Time LG
				FERS_WriteRegister(handle, a_hg_sh_time, stair_dwell_time); // Shaping Time HG
				FERS_WriteRegister(handle, a_trg_mask, 0x1); // SW trigger only
				FERS_WriteRegister(handle, a_t1_out_mask, 0x10); // PTRG (for debug)
				FERS_WriteRegister(handle, a_t0_out_mask, 0x04); // T-OT (for debug)

				// Start Scan
				Sleep(100);
				std::cout<< "            --------- Rate (cps) ---------"<<std::endl;
				std::cout<< " Adv  Thr     ChMean       T-OR       Q-OR"<<std::endl;
				for(s = nstep; s >= 0; s--) {

					thr = stair_start + s * stair_step;
					FERS_WriteRegister(handle, a_qd_coarse_thr, thr);	// Threshold for Q-discr
					FERS_WriteRegister(handle, a_td_coarse_thr, thr);	// Threshold for T-discr
					FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);		// set citiroc index = 0
					FERS_SendCommand(handle, CMD_CFG_ASIC);
					FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);		// set citiroc index = 1
					FERS_SendCommand(handle, CMD_CFG_ASIC);
					Sleep(500);
					FERS_WriteRegister(handle, a_trg_mask, 0x20); // enable periodic trigger
					FERS_SendCommand(handle, CMD_RES_PTRG);  // Reset period trigger counter and count for dwell time
					Sleep((int)(dwell_s/1000 + 200));  // wait for a complete dwell time (+ margin), then read counters
					FERS_ReadRegister(handle, a_t_or_cnt, &Tor_cnt);
					FERS_ReadRegister(handle, a_q_or_cnt, &Qor_cnt);
					if (s < nstep) {  // skip 1st pass 
						uint64_t chmean = 0;
						for(i=0; i<FERSLIB_MAX_NCH; i++) {
							FERS_ReadRegister(handle, a_hitcnt + (i << 16), &hitcnt[i]);
							chmean += (uint64_t)hitcnt[i];

							// fill structure
							StaircaseEvent.hitcnt[i] = hitcnt[i];
						}
						chmean /= FERSLIB_MAX_NCH;
						int perc = (100 * (nstep-s)) / nstep;
						if (perc > 100) perc = 100;
						std::cout<< perc <<" "<< thr <<" "<< chmean/dwell_s <<" "<< Tor_cnt/dwell_s <<" "<< Qor_cnt/dwell_s <<std::endl;

						// fill structure
						StaircaseEvent.threshold = (uint16_t)thr;
						StaircaseEvent.shapingt = stair_shapingt;
						StaircaseEvent.dwell_time = stair_dwell_time;
						StaircaseEvent.chmean = (uint32_t)chmean;
						StaircaseEvent.HV = (uint32_t)(1000*HV);
						StaircaseEvent.Tor_cnt = Tor_cnt;
						StaircaseEvent.Qor_cnt = Qor_cnt;


						Event = (void*)(&StaircaseEvent);
						//make_header(handle, x_pixel, y_pixel, DTQ_STAIRCASE, &data);
						make_header(brd, DTQ_STAIRCASE, &data);
						FERSpackevent(Event, DTQ_STAIRCASE, &data);


						uint32_t block_id = (nstep-1) - s; // starts at 0
						ev->AddBlock(block_id, data);

						std::this_thread::sleep_until(tp_end_of_busy);
					}
				}
				stairdone = true;
				SendEvent(std::move(ev));

			} else {
				FERSProducer::DoStopRun(); // just run once
				EUDAQ_INFO("Producer > staircase event sent");
				EUDAQ_WARN("*** *** PLEASE STOP THE RUN *** ***");
			}

		} else { // not staircase

			std::vector<uint8_t> hit(x_pixel*y_pixel, 0);
			hit[position(gen)] = signal(gen);

			int nchan = x_pixel*y_pixel;
			int DataQualifier = -1;
			void *Event;

			auto ev = eudaq::Event::MakeUnique("FERSProducer");
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

			double tstamp_us = -1;
			int nb = -1;
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
			//std::cout<<"TRYING to CREATE an HARDCODED spectroscopy event"<<std::endl;
			//DataQualifier = DTQ_SPECT;
			//SpectEvent_t EventSpect;// = (SpectEvent_t*)Event;
			////EventSpect.tstamp_us  = (double)(  1000);
			////EventSpect.trigger_id = (uint64_t) 100;
			//EventSpect.tstamp_us  = (double)trigger_n;
			//EventSpect.trigger_id = (uint64_t)trigger_n;
			//EventSpect.chmask     = (uint64_t) 120;
			//EventSpect.qdmask     = (uint64_t) 130;
			//for(uint16_t i=0; i<nchan; i++){
			//  EventSpect.energyHG[i] = (uint16_t)( 200 + i + trigger_n);
			//  EventSpect.energyLG[i] = (uint16_t)( 100 + i + trigger_n);
			//  EventSpect.tstamp[i]   = (uint32_t)(1000 + i); // used in TSPEC mode only
			//  EventSpect.ToT[i]      = (uint16_t)(  10 + i); // used in TSPEC mode only
			//}
			//Event =&EventSpect;
			//nb = sizeof(EventSpect);
			//std::cout<<"****** event structure filled: "<< sizeof(EventSpect) <<" bytes, pointer: "<< Event<<std::endl;



			// event creation
			if ( DataQualifier >0 ) {
				std::vector<uint8_t> data;
				//make_header(handle, x_pixel, y_pixel, DataQualifier, &data);
				make_header(brd, DataQualifier, &data);
				std::cout<<"producer > " << "x_pixel: " <<	x_pixel << " y_pixel: " << y_pixel << " DataQualifier : " <<	DataQualifier << std::endl;
				FERSpackevent(Event, DataQualifier, &data);
				uint32_t block_id = m_plane_id;
				ev->AddBlock(block_id, data);
				SendEvent(std::move(ev));
				trigger_n++;
				std::this_thread::sleep_until(tp_end_of_busy);
			}
		} // if (stair_do)
	}
}
//----------DOC-MARK-----END*IMP-----DOC-MARK----------

