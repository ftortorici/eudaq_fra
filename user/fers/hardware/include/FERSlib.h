/******************************************************************************
* 
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the 
* software, documentation and results solely at his own risk.
******************************************************************************/

#ifndef _FERSLIB_H
#define _FERSLIB_H				// Protect against multiple inclusion

#include <stdint.h>
#include "MultiPlatform.h"

#ifndef WIN32
#include <stdbool.h>
#endif

#define FERSLIB_MAX_NCNC			4		// Max number of concentrators
#define FERSLIB_MAX_NBRD			16		// Max number of connected boards
#define FERSLIB_MAX_NCH				64		// Max number of channels per board
#define FERSLIB_MAX_NTDL			1		// Max number of TDlinks (chains) in a concentrator
#define FERSLIB_MAX_NNODES			16		// Max number of nodes in a TDL chain

#define FERS_CONNECT_TIMEOUT		3		// timeout connection in seconds

#define NODATA_TIMEOUT				100		// time to wait after there is no data from the board to consider it empty
#define STOP_TIMEOUT				500		// timeout for forcing the RX thread to go in idle state after the stop of the run in the boards

#define CLK_PERIOD					8		// clock period in ns (for reg settings)

#define MAX_WAVEFORM_LENGTH			2048
#define MAX_LIST_SIZE				128
#define MAX_TEST_NWORDS				4

// Debug Logs
#define DBLOG_FERSLIB_MSG			0x0001		// Enable FERSlib to print log messages to console
#define DBLOG_RAW_DATA_OUTFILE		0x0002		// Enable read data function to dump event data (32 bit words) into a text file
#define DBLOG_LL_DATADUMP			0x0004		// Enable low level lib to dump raw data (from usb, eth and tdl) into a text file
#define DBLOG_LL_MSGDUMP			0x0008		// Enable low level lib to log messages (from usb, eth and tdl) into a text file
#define DBLOG_QUEUES				0x0010		// Enable messages from queues (push and pop) used in event sorting
#define DBLOG_RAW_DECODE			0x0020		// Enable messages from raw data decoding

#define ENABLE_FERSLIB_LOGMSG		(DebugLogs & DBLOG_FERSLIB_MSG)

// Status of the data RX thread
#define RXSTATUS_OFF		0
#define RXSTATUS_IDLE		1
#define RXSTATUS_RUNNING	2
#define RXSTATUS_EMPTYING	3

// Error Codes
#define FERSLIB_ERR_GENERIC				-1
#define FERSLIB_ERR_COMMUNICATION		-2
#define FERSLIB_ERR_MAX_NBOARD_REACHED	-3
#define FERSLIB_ERR_INVALID_PATH		-4
#define FERSLIB_ERR_INVALID_HANDLE		-5
#define FERSLIB_ERR_READOUT_ERROR		-6
#define FERSLIB_ERR_MALLOC_BUFFERS		-7
#define FERSLIB_ERR_INVALID_BIC			-8
#define FERSLIB_ERR_READOUT_NOT_INIT	-9
#define FERSLIB_ERR_PEDCALIB_NOT_FOUND	-10
#define FERSLIB_ERR_INVALID_FWFILE		-11
#define FERSLIB_ERR_UPGRADE_ERROR		-12
#define FERSLIB_ERR_INVLID_PARAM		-13
#define FERSLIB_ERR_NOT_APPLICABLE		-14
#define FERSLIB_ERR_TDL_ERROR			-15
#define FERSLIB_ERR_QUEUE_OVERRUN		-16
#define FERSLIB_ERR_START_STOP_ERROR	-17
#define FERSLIB_ERR_OPER_NOT_ALLOWED	-18

// Acquisition Modes
#define ACQMODE_SPECT				0x01  // Spectroscopy Mode (Energy)
#define ACQMODE_TSPECT				0x03  // Spectroscopy + Timing Mode (Energy + Tstamp)
#define ACQMODE_TIMING_CSTART		0x02  // Timing Mode - Common Start (List)
#define ACQMODE_TIMING_CSTOP		0x12  // Timing Mode - Common Stop (List)
#define ACQMODE_TIMING_STREAMING	0x22  // Timing Mode - Streaming (List)
#define ACQMODE_COUNT				0x04  // Counting Mode (MCS)
#define ACQMODE_WAVE				0x08  // Waveform Mode

// Start/Stop Acquisition Modes
#define STARTRUN_ASYNC				0
#define STARTARUN_CHAIN_T0			1
#define STARTRUN_CHAIN_T1			2
#define STARTRUN_TDL				3

// Data Qualifier
#define DTQ_SPECT					0x01  // Spectroscopy Mode (Energy)
#define DTQ_TIMING					0x02  // Timing Mode 
#define DTQ_COUNT 					0x04  // Counting Mode (MCS)
#define DTQ_WAVE					0x08  // Waveform Mode
#define DTQ_TSPECT					0x03  // Spectroscopy + Timing Mode (Energy + Tstamp)
#define DTQ_TEST					0xFF  // Test Mode 
#define DTQ_START					0x0F  // Start Event

// Readout Mode
#define ROMODE_DISABLE_SORTING		0x0000	// Disable sorting 
#define ROMODE_TRGTIME_SORTING		0x0001	// Enable event sorting by Trigger Tstamp 
#define ROMODE_TRGID_SORTING		0x0002	// Enable event sorting by Trigger ID

// Readout Status
#define ROSTATUS_IDLE				0	// idle (acquisition not running)
#define ROSTATUS_RUNNING			1	// running (data readout)
#define ROSTATUS_EMPTYING			2	// boards stopped, reading last data in the pipes
#define ROSTATUS_FLUSHING			3	// flushing old data (clear)


// Data width for Energy, ToA and ToT
#define ENERGY_NBIT					14	
#define TOA_NBIT					16
#define TOA_LSB_ns					0.5
#define TOT_NBIT					9

#define FLASH_PAGE_SIZE				528	// flash page size for AT54DB321
#define FLASH_BIC_PAGE				0	// flash page with Board IDcard
#define FLASH_PEDCALIB_PAGE			4	// flash page with Pedestal calibration
#define FLASH_PEDCALIB_BCK_PAGE		5	// flash page with Pedestal calibration (backup)

// Handles and indexing
#define FERS_INDEX(handle)				((handle) & 0xFF)		// Board Index
#define FERS_CONNECTIONTYPE(handle)		((handle) & 0xF0000)	// Connection Type
#define FERS_CONNECTIONTYPE_ETH			0x00000
#define FERS_CONNECTIONTYPE_USB			0x10000
#define FERS_CONNECTIONTYPE_TDL			0x20000
#define FERS_CONNECTIONTYPE_CNC			0x80000
#define FERS_NODE(handle)				((handle >> 20) & 0xF)
#define FERS_CHAIN(handle)				((handle >> 24) & 0xF)
#define FERS_CNCINDEX(handle)			((handle >> 30) & 0xF)

//******************************************************************
// TDL-chain Info Struct
//******************************************************************
typedef struct {
	uint16_t Status;		// Chain Status (0=not enumerated, 1=enumerated, ...)
	uint16_t BoardCount;	// Num of boards in the chain
	float rrt;				//
	uint64_t EventCount;	// total number of transferred events
	uint64_t ByteCount;		// total number of transferred bytes
	float EventRate;		// Current event rate (cps)
	float Mbps;				// Current transfer rate (Megabit per second)
} FERS_TDL_ChainInfo_t;

//******************************************************************
// Concentrator Info Struct 
//******************************************************************
typedef struct {
	uint32_t pid;			// Board PID (5 decimal digits)
	uint8_t PCBrevision;	// PCB revision 
	char ModelCode[16];		// e.g. WDT5215XAAAA
	char ModelName[16];		// e.g. DT5215
	char FPGA_FWrev[20];	// FPGA FW revision 
	char SW_rev[20];		// Software Revision (embedded ARM)
	uint16_t NumLink;		// Number of links
	FERS_TDL_ChainInfo_t ChainInfo[8];	// TDL Chain info
} FERS_CncInfo_t;

//******************************************************************
// FERS Board Info Struct 
//******************************************************************
typedef struct {
	uint32_t pid;			// Board PID (5 decimal digits)
	uint16_t FERSCode;		// e.g. 5202
	uint8_t PCBrevision;	// PCB revision 
	char ModelCode[16];		// e.g. WA5202XAAAAA
	char ModelName[16];		// e.g. A5202
	uint8_t FormFactor;		// 0=naked version (A52XX), 1=boxed version (DT52XX)
	uint16_t NumCh;			// Number of channels
	uint32_t FPGA_FWrev;	// FPGA FW revision 
	uint32_t uC_FWrev;		// uC FW revision
} FERS_BoardInfo_t;

//******************************************************************
// Event Data Structures
//******************************************************************
// Spectroscopy Event (with or without timing)
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint64_t chmask;
	uint64_t qdmask;
	uint16_t energyHG[64];
	uint16_t energyLG[64];
	uint32_t tstamp[64];	// used in TSPEC mode only
	uint16_t ToT[64];		// used in TSPEC mode only
} SpectEvent_t;

// Counting Event
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint64_t chmask;
	uint32_t counts[64];
	uint32_t t_or_counts;
	uint32_t q_or_counts;
} CountingEvent_t;

// Waveform Event
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint16_t ns;
	uint16_t wave_hg[MAX_WAVEFORM_LENGTH];
	uint16_t wave_lg[MAX_WAVEFORM_LENGTH];
	uint8_t dig_probes[MAX_WAVEFORM_LENGTH];
} WaveEvent_t;

// List Event (timing mode only)
typedef struct {
	uint16_t nhits;
	uint8_t  channel[MAX_LIST_SIZE];
	uint32_t tstamp[MAX_LIST_SIZE];
	uint16_t ToT[MAX_LIST_SIZE];
} ListEvent_t;

// Test Mode Event (fixed data patterns)
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint16_t nwords;
	uint32_t test_data[MAX_TEST_NWORDS];
} TestEvent_t;


// *****************************************************************
// Global Variables
// *****************************************************************
extern FERS_BoardInfo_t *FERS_BoardInfo[FERSLIB_MAX_NBRD];	// pointers to the board info structs 
extern int FERS_TotalAllocatedMem;							// Total allocated memory 
extern int FERS_ReadoutStatus;								// Status of the readout processes (idle, running, flushing, etc...)
extern int FERS_RunningCnt;									// Num of running boards 
extern mutex_t FERS_RoMutex;								// Mutex for the access to FERS_ReadoutStatus
extern int DebugLogs;										// Debug Logs

// Macros for getting main parameters of the FERS units 
#define FERS_Model(handle)			((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->Model : 0)
#define FERS_pid(handle)			((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->pid : 0)
#define FERS_Code(handle)			((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->FERSCode : 0)
#define FERS_ModelName(handle)		((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->ModelName : "")
#define FERS_FPGA_FWrev(handle)		((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->FPGA_FWrev : 0)
#define FERS_uC_FWrev(handle)		((FERS_INDEX(handle) >= 0) ? FERS_BoardInfo[FERS_INDEX(handle)]->uC_FWrev : 0)


// *****************************************************************
// Messaging and errors
// *****************************************************************
int FERS_LibMsg(char *fmt, ...);
int FERS_SetDebugLogs(int DebugEnableMask);

// *****************************************************************
// Open/Close
// *****************************************************************
int FERS_OpenDevice(char *path, int *handle);
int FERS_IsOpen(char *path);
int FERS_CloseDevice(int handle);
int FERS_TotalAllocatedMemory();
int FERS_Reset_IPaddress(int handle);
int FERS_Get_CncPath(char *dev_path, char *cnc_path);
int FERS_InitTDLchains(int handle);
bool FERS_TDLchainsInitialized(int handle);

// *****************************************************************
// Register Read/Write
// *****************************************************************
int FERS_ReadRegister(int handle, uint32_t address, uint32_t *data);
int FERS_WriteRegister(int handle, uint32_t address, uint32_t data);
int FERS_WriteRegisterSlice(int handle, uint32_t address, uint32_t start_bit, uint32_t stop_bit, uint32_t data);
int FERS_SendCommand(int handle, uint32_t cmd);
int FERS_SendCommandBroadcast(int *handle, uint32_t cmd, uint32_t delay);
int FERS_ReadBoardInfo(int handle, FERS_BoardInfo_t *binfo);
int FERS_ReadConcentratorInfo(int handle, FERS_CncInfo_t *cinfo);
int FERS_WriteBoardInfo(int handle, FERS_BoardInfo_t binfo);
int FERS_WritePedestals(int handle, uint16_t *PedLG, uint16_t *PedHG, uint16_t *dco);
int FERS_ReadPedestalsFromFlash(int handle, char *date, uint16_t *PedLG, uint16_t *PedHG, uint16_t *dco);
int FERS_PedestalBackupPage(int handle, int EnBckPage);
int FERS_SetCommonPedestal(int handle, uint16_t Pedestal);
int FERS_EnablePedestalCalibration(int handle, int enable);
int FERS_GetChannelPedestalBeforeCalib(int handle, int ch, uint16_t *PedLG, uint16_t *PedHG);
int FERS_Get_FPGA_Temp(int handle, float *temp);

// *****************************************************************
// Flash Read/Write
// *****************************************************************
int FERS_ReadFlashPage(int handle, int pagenum, int size, uint8_t *data);
int FERS_WriteFlashPage(int handle, int pagenum, int size, uint8_t *data);

// *****************************************************************
// High Voltage Control
// *****************************************************************
int HV_Init(int handle);
int HV_WriteReg(int handle, uint32_t reg_addr, uint32_t dtype, uint32_t reg_data);
int HV_ReadReg(int handle, uint32_t reg_addr, uint32_t dtype, uint32_t *reg_data);
int HV_Set_OnOff(int handle, int OnOff);
int HV_Get_Status(int handle, int *OnOff, int *Ramping, int *OvC, int *OvV);
int HV_Get_SerNum(int handle, int* sernum);
int HV_Set_Vbias(int handle, float vbias);
int HV_Get_Vbias(int handle, float *vbias);
int HV_Get_Vmon(int handle, float *vmon);
int HV_Set_Imax(int handle, float imax);
int HV_Get_Imax(int handle, float *imax);
int HV_Get_Imon(int handle, float *imon);
int HV_Get_IntTemp(int handle, float *temp);
int HV_Get_DetectorTemp(int handle, float *temp);
int HV_Set_Tsens_Coeff(int handle, float Tsens_coeff[3]);
int HV_Set_TempFeedback(int handle, int enable, float Tsens_coeff);


// *****************************************************************
// TDC Config and readout (for test)
// *****************************************************************
int TDC_WriteReg(int handle, int tdc_id, uint32_t addr, uint32_t data);
int TDC_ReadReg(int handle, int tdc_id, uint32_t addr, uint32_t *data);
int TDC_Config(int handle, int tdc_id, int StartSrc, int StopSrc);
int TDC_DoStartStopMeasurement(int handle, int tdc_id, double *tof_ns);


// *****************************************************************
// Data Readout
// *****************************************************************
int FERS_InitReadoutMutex();
int FERS_InitReadout(int handle, int ROmode, int *AllocatedSize);
int FERS_CloseReadout(int handle);
int FERS_FlushData(int handle);
int FERS_StartAcquisition(int *handle, int NumBrd, int StartMode);
int FERS_StopAcquisition(int *handle, int NumBrd, int StartMode);
int FERS_GetEvent(int *handle, int *bindex, int *DataQualifier, double *tstamp_us, void **Event, int *nb);
int FERS_GetEventFromBoard(int handle, int *DataQualifier, double *tstamp_us, void **Event, int *nb);

// *****************************************************************
// Firmware Upgrade
// *****************************************************************
int FERS_FirmwareUpgrade(int handle, FILE *fp, void(*ptr)(char *msg, int progress));
int FERS_CheckBootloaderVersion(int handle, int* isInBootloader, uint32_t* version, uint32_t* release);

#endif
