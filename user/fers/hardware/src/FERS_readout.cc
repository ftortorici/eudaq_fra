/*******************************************************************************
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
#include "MultiPlatform.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>

#include "FERS_LL.h"
#include "FERSlib.h"
#include "FERS_Registers.h"

#define LLBUFF_SIZE			(16*1024)
#define LLBUFF_CNC_SIZE		(64*1024)
#define EVBUFF_SIZE			(16*1024)

#define FERSLIB_QUEUE_SIZE			(64*1024)	// queue size in words
#define FERSLIB_QUEUE_MAX_EVSIZE	512			// Maximum event size (in words) that can be pushed into the queue
#define FERSLIB_QUEUE_FULL_LEVEL	90			// max queue occupancy (percent) to set the busy condition and stop pushing events
#define FERSLIB_QUEUE_TIMEOUT_MS	100			// timeout in ms 

#define INCR_PNT(p)			((p) = ((p)+1) % LLBUFF_SIZE)
#define MOVE_PNT(p, offs)	((p) = ((p)+(offs)) % LLBUFF_SIZE)
#define BUFF_NB(rp, wp)		(((uint32_t)(wp-rp) % LLBUFF_SIZE))

// *********************************************************************************************************
// Global Variables
// *********************************************************************************************************
static char *LLBuff[FERSLIB_MAX_NBRD] = { NULL };			// buffers for raw data coming from LL protocol. One per board in direct mode (eth/usb), one per concentrator with TDLink
static uint32_t *EvBuff[FERSLIB_MAX_NBRD] = { NULL };		// buffers for event data, before decoding. One per board
static int EvBuff_nb[FERSLIB_MAX_NBRD] = { 0 };				// Number of bytes currently present in EvBuff
static uint64_t Tstamp[FERSLIB_MAX_NBRD] = { 0 };			// Tstamp of the event in EvBuff

static int RO_NumBoards = 0;								// Total num of boards connected for the readout
static int Cnc_NumBoards[FERSLIB_MAX_NCNC] = { 0 };			// Num of boards connected to one concentrator
static int tdl_handle[FERSLIB_MAX_NCNC][8][16] = { -1 };	// TDL handle map (gives the handle of the board connected to a specific cnc/chain/node)
static int Cnc_Flushed[FERSLIB_MAX_NCNC] = { 0 };			// Flag indicating that the concentrator has been flushed
 
static SpectEvent_t SpectEvent[FERSLIB_MAX_NBRD];			// Decoded event (spectroscopy mode)
static CountingEvent_t CountingEvent[FERSLIB_MAX_NBRD];		// Decoded event (counting mode)
static ListEvent_t ListEvent[FERSLIB_MAX_NBRD];				// Decoded event (timing mode)
static WaveEvent_t WaveEvent[FERSLIB_MAX_NBRD];				// Decoded event (waveform mode)
static TestEvent_t TestEvent[FERSLIB_MAX_NBRD];				// Decoded event (test mode)

static int ReadoutMode = 0;									// Readout Mode
static int EnableStartEvent[FERSLIB_MAX_NBRD] = { 0 };		// Enable Start Event 

// Gloabal Variables for the queues (for sorting)
static uint32_t* queue[FERSLIB_MAX_NBRD] = { NULL };	// memory buffers for queues
static uint64_t q_tstamp[FERSLIB_MAX_NBRD];				// tstamp of the oldest event in the queue (0 means empty queue)
static uint64_t q_trgid[FERSLIB_MAX_NBRD];				// trgid of the oldest event in the queue (0 means empty queue)
static int q_wp[FERSLIB_MAX_NBRD];						// Write Pointer
static int q_rp[FERSLIB_MAX_NBRD];						// Read Pointer
static int q_nb[FERSLIB_MAX_NBRD];						// num of words in the queue 
static int q_full[FERSLIB_MAX_NBRD];					// queue full (nb > max occupancy)
static int q_busy = 0;									// num of full queues


// *********************************************************************************************************
// Queue functions (push and pop)
// *********************************************************************************************************
int q_push(int qi, uint32_t *buff, int size) {
	int tsize = size + 1;  // event data + size
	static FILE *ql = NULL;
	if ((DebugLogs & DBLOG_QUEUES) && (ql == NULL)) ql = fopen("queue_log.txt", "w");
	if (q_full[qi] || (tsize > FERSLIB_QUEUE_MAX_EVSIZE)) {
		return FERSLIB_ERR_QUEUE_OVERRUN;
		if (ql != NULL) fprintf(ql, "Queue Overrun (writing while busy)!!!\n");
	}
	if ((q_wp[qi] < (q_rp[qi] - tsize - 1)) || (q_wp[qi] >= q_rp[qi])) {
		if (q_tstamp[qi] == 0) q_tstamp[qi] = ((uint64_t)buff[4] << 32) | (uint64_t)buff[3];
		if (q_trgid[qi] == 0) q_trgid[qi] = ((uint64_t)buff[2] << 32) | (uint64_t)buff[1];
		*(queue[qi] + q_wp[qi]) = size;
		memcpy(queue[qi] + q_wp[qi] + 1, buff, size);
		if (ql != NULL) fprintf(ql, "Push Q%d: %12.3f %8" PRIu64 "\n", qi, (((uint64_t)buff[4] << 32) | (uint64_t)buff[3])*CLK_PERIOD/1000.0, ((uint64_t)buff[2] << 32) | (uint64_t)buff[1]);
		q_wp[qi] += tsize;
		if ((q_wp[qi] + FERSLIB_QUEUE_MAX_EVSIZE) > FERSLIB_QUEUE_SIZE) q_wp[qi] = 0;

		q_nb[qi] += tsize;
		if (q_nb[qi] > (FERSLIB_QUEUE_SIZE * 100 / FERSLIB_QUEUE_FULL_LEVEL)) {
			q_full[qi] = 1;
			q_busy++;
		}
		return 1;
	}
	return 0;
}

int q_pop(int qi, uint32_t *buff, int *size) {
	uint32_t* nextev;
	if (q_tstamp[qi] == 0) return 0;
	*size = *(queue[qi] + q_rp[qi]);
	memcpy(buff, queue[qi] + q_rp[qi] + 1, *size);
	q_rp[qi] += *size + 1;
	if ((q_rp[qi] + FERSLIB_QUEUE_MAX_EVSIZE) > FERSLIB_QUEUE_SIZE) q_rp[qi] = 0;
	q_nb[qi] -= (*size + 1);
	if (q_rp[qi] == q_wp[qi]) { // it was last event => queue is now empty 
		q_tstamp[qi] = 0;  
		q_trgid[qi] = 0;  
	} else {
		nextev = queue[qi] + q_rp[qi] + 1;  // Get tstamp of next event in the queue
		q_tstamp[qi] = ((uint64_t)nextev[4] << 32) | (uint64_t)nextev[3];
		q_trgid[qi] = ((uint64_t)nextev[2] << 32) | (uint64_t)nextev[1];
	}
	if (q_full[qi]) {
		q_full[qi] = 0;
		q_busy--;
	}
	return 1;
}


// *********************************************************************************************************
// Read Raw Event (from board, either Eth or USB)
// Event data are written into the EvBuff of the relevant board.
// *********************************************************************************************************
/* 
Data Format from PIC:
0   0x81000000_00000000
1   0x00000000_DQDSDSDS   DQ = data qualifier, DS: data size (num of 32 bit words in the payload)
2   0x00TDTDTD_TDTDTDTD   TD: Trigger-id
3   0x00TCTCTC_TCTCTCTC	  TC: Time-Stamp
4   0x00PLPLPL_PLPLPLPL   PL: payload -> 32 bit word, aligned to 56 bit
5   0x00PLPLPL_PLPLPLPL
6   0x00PLPLPL_PLPLPLPL
7   0x00PLPLPL_PLPLPLPL   
8   0xC0000000_SZSZSZSZ   SZ: data size in 56 bit words -> can be used to check data integrity
*/
static int eth_usb_ReadRawEvent(int handle, int *nb)
{
	static int wp[FERSLIB_MAX_NBRD] = { 0 }, rp[FERSLIB_MAX_NBRD] = { 0 };  // Read and write pointers
	static int wpnt[FERSLIB_MAX_NBRD] = { 0 }, rpp[FERSLIB_MAX_NBRD] = { 0 };
	static int footer_cnt[FERSLIB_MAX_NBRD] = { 0 }, NeedMoreData[FERSLIB_MAX_NBRD] = { 0 };
	static uint32_t size[FERSLIB_MAX_NBRD] = { 0 }, bcnt[FERSLIB_MAX_NBRD] = { 0 };
	static int numev[FERSLIB_MAX_NBRD] = { 0 };
	int i, rvb, ret, nbb, reqsize;
	uint32_t d32, dtq;
	uint64_t footer;
	static FILE *rdlog = NULL;  // readout log file
	int h = FERS_INDEX(handle);
	char *EvBuff8 = (char *)EvBuff[h];

	if ((DebugLogs & DBLOG_RAW_DECODE) && (rdlog == NULL))  {
		char fname[100];
		sprintf(fname, "RawDecodeLog_%d.txt", h);
		rdlog = fopen(fname, "w");
	}

	*nb = 0;
	if (FERS_ReadoutStatus == ROSTATUS_FLUSHING) {
		wp[h]=0;
		rp[h]=0;
		size[h]=0;
		NeedMoreData[h] = 0;
		if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH) LLeth_ReadData(h, LLBuff[h], LLBUFF_SIZE, nb);
		else LLusb_ReadData(h, LLBuff[h], LLBUFF_SIZE, nb);
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Flushing Data\n", h);
		return 0;
	}

	while (1) {
		nbb = 0;
		if ((wp[h] == rp[h]) || NeedMoreData[h]) {
			// read data from device 
			//reqsize = ((rp[h] == 0) && (wp[h] == 0)) ? LLBUFF_SIZE / 2 : LLBUFF_SIZE - wp[h];
			if ((rp[h] == 0) && (wp[h] == 0)) reqsize = LLBUFF_SIZE / 2;
			else if (wp[h] < rp[h]) reqsize = rp[h] - wp[h] - 1;
			else reqsize = LLBUFF_SIZE - wp[h];
			if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH)
				ret = LLeth_ReadData(h, LLBuff[h] + wp[h], reqsize, &nbb);
			else
				ret = LLusb_ReadData(h, LLBuff[h] + wp[h], reqsize, &nbb);
			if (ret < 0) 
				return FERSLIB_ERR_READOUT_ERROR;
			if (ret == 2) return 2;
			if (nbb > 0) {
				MOVE_PNT(wp[h], nbb);
				NeedMoreData[h] = 0;
				if (rdlog != NULL) {
					fprintf(rdlog, "recv: %d bytes. rp=%d, wp=%d\n", nbb, rp[h], wp[h]);
					fflush(rdlog);
					//for (int bb=0; bb<nbb; bb++) fprintf(rdlog, "%02X\n", (uint8_t)LLBuff[h][bb]);
				}
			} else if (NeedMoreData[h]) 
				continue;
		}
		if (wp[h] == rp[h]) return 0;  // No data

		// get size of the payload and transfer header to EvBuff
		if (size[h] == 0) {
			int skip=0;
			while (BUFF_NB(rp[h], wp[h]) >= 4) {
				d32 = *((uint32_t *)(LLBuff[h]+rp[h]));
				if (d32 == 0x00000081) break;  // advance read pointer until 0x81000000 is found (converted to big endian)
				INCR_PNT(rp[h]);
				skip++;
			} 
			if ((skip > 0) && (ENABLE_FERSLIB_LOGMSG)) FERS_LibMsg("[ERROR][BRD %02d]: unexpected RX data format\n", h);
			if (BUFF_NB(rp[h], wp[h]) < 32) {
				NeedMoreData[h] = 1;
				continue;  // wait for header to complete (read more data if NB<32)
			}
			MOVE_PNT(rp[h], 12);
			dtq = (uint32_t)LLBuff[h][rp[h]];
			MOVE_PNT(rp[h], 2);
			size[h] = 256 * (uint32_t)((uint8_t *)LLBuff[h])[rp[h]];
			INCR_PNT(rp[h]);
			size[h] += (uint32_t)((uint8_t *)LLBuff[h])[rp[h]];
			INCR_PNT(rp[h]);
			size[h] *= 4;  // payload size (in bytes)
			bcnt[h] = 0;
			rpp[h] = 0;
			wpnt[h] = 0;
			// event size in 32 bit words = payload + 5 (1 word for event size, 2 words for tstamp, 2 words for trg ID)
			EvBuff8[wpnt[h]++] = (size[h]/4 + 5) & 0xFF;
			EvBuff8[wpnt[h]++] = ((size[h]/4 + 5) >> 8) & 0xFF;
			EvBuff8[wpnt[h]++] = 0;
			EvBuff8[wpnt[h]++] = dtq;
			for (i=0; i<16; i++) {
				EvBuff8[wpnt[h]+(i/8)*8+(7-i%8)] = LLBuff[h][rp[h]];  // get two 32 bit words (tstamp and trigger ID). Convert big/little endian
				INCR_PNT(rp[h]);
			}
			Tstamp[h] = *(uint64_t *)(EvBuff8 + wpnt[h] + 8);
			wpnt[h] += 16;
			if (rdlog != NULL) fprintf(rdlog, "Header complete: size = %d bytes. rp=%d, wp=%d\n", size[h], rp[h], wp[h]);
		}

		// transfer payload to EvBuff
		for (; BUFF_NB(rp[h], wp[h]) > 0; INCR_PNT(rp[h])) {
			if (bcnt[h] < size[h]) {
				if ((rpp[h]++ & 0x7) == 0) continue;  // skip 1st byte of each quartet (data are 56 bit aligned)
				rvb = 3 - (bcnt[h] & 0x3) * 2;
				if ((wpnt[h]+rvb) > EVBUFF_SIZE) {
					if (rdlog != NULL) fprintf(rdlog, "EvBuffer overflow!\n");
					return FERSLIB_ERR_READOUT_ERROR;
				}
				EvBuff8[wpnt[h]+rvb] = LLBuff[h][rp[h]];  // convert big/little endian
				bcnt[h]++;
				wpnt[h]++;
			} else if ((bcnt[h] == size[h]) && (footer_cnt[h] == 0)) {
				footer_cnt[h] = (size[h] % 7 == 0) ? 8 : 7 - (size[h] % 7) + 8;
				if (footer_cnt[h] == 8) footer = (uint64_t)LLBuff[h][rp[h]];
				else footer = 0;
				if (rdlog != NULL) {
					fprintf(rdlog, "Payload complete: PLsize = %d bytes. rp=%d, wp=%d\n", bcnt[h], rp[h], wp[h]);
					for(i=0; i<(wpnt[h]/4); i++) 
						fprintf(rdlog, "%3d : %08X\n", i, EvBuff[h][i]);
					fflush(rdlog);
				}
			} else if (footer_cnt[h] > 0) {
				footer_cnt[h]--;
				if ((footer_cnt[h] <= 9) && (footer_cnt[h] > 0)) footer = (footer << 8) | (uint64_t)LLBuff[h][rp[h]];
				if (footer_cnt[h] == 0) {
					if (rdlog != NULL) {
						fprintf(rdlog, "Footer complete: rp=%d, wp=%d\n", rp[h], wp[h]);
						fflush(rdlog);
					}
					if (((footer & 0xFF00000000000000) != 0xC000000000000000) && (ENABLE_FERSLIB_LOGMSG)) 
						FERS_LibMsg("[ERROR][BRD %02d]: unexpected footer = %016X\n", h, footer);
					size[h] = 0;
					EvBuff_nb[h] = wpnt[h];
					*nb = wpnt[h];
					return 0;
				}
			}
		}
	}
	return 0;
}



// *********************************************************************************************************
// Read Raw Event (from Concentrator)
// *********************************************************************************************************
/* 
Data Format from TDlink:
    0  1  2  3 
0   FF FF FF FF    Fixed Header Tag
1   FF FF FF FF    Fixed Header Tag
2   aa aa aa aa    a: cluster trigger id (0..31)
3   aa aa aa aa    a: cluster trigger id (32..63)
4   bb bb bb bb    b: cluster time stamp (0..31)
5   bb bb bb bb    b: cluster time stamp (32..63)
6   cc cc cc cc    c: num events in the cluster
7   dd dd dd dd    d: event size
-------------------------------------------------------
8   ee ee ff ff    e: chain, f: node
9   qq ss ss ss    q: data qualifier, s: size in byte of the payload of the event
10  hh hh hh hh    h: trigger id (0..31) (counted by the board)
11  hh hh hh hh    h: trigger id (32..63) 
12  ii ii ii ii    i: timestamp (0..31)
13  ii ii ii ii    i: timestamp (32..63)
14  ++ ++ ++ ++
15  ++ ++ ++ ++    +: payload
16  ++ ++ ++ ++
...
*/

static int tdl_ReadRawEvent(int cindex, int *bindex, int *nb)
{
	static uint32_t wp[FERSLIB_MAX_NBRD] = { 0 }, rp[FERSLIB_MAX_NBRD] = { 0 };  // Read and write pointers
	int nbr, ret;
	uint32_t size, evsize, numev, chain, node, last, rp32;
	uint32_t *buff32 = (uint32_t *)LLBuff[cindex];
	static int ncyc = 0;
	static FILE *rdlog = NULL;  // readout log file

	if ((DebugLogs & DBLOG_RAW_DECODE) && (rdlog == NULL))  {
		char fname[100];
		sprintf(fname, "ReadDecodeLog_%d.txt", cindex);
		rdlog = fopen(fname, "w");
	}

	if (FERS_ReadoutStatus == ROSTATUS_FLUSHING) {
		wp[cindex]=0;
		rp[cindex]=0;
		LLtdl_ReadData(cindex, LLBuff[cindex], LLBUFF_CNC_SIZE, nb);
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Flushing Data\n", cindex);
		return 0;
	}

	*nb=0;
	*bindex=0;
	rp32 = rp[cindex];
	if (wp[cindex] == 0) {  // No data in LLBuff_Cnc, take data from the buffer of data_receiver
		while (wp[cindex] < 32) {  // Read cluster header
			ret = LLtdl_ReadData(cindex, LLBuff[cindex] + wp[cindex], 32 - wp[cindex], &nbr);
			if (ret == 2) return 2;
			if ((wp[cindex] == 0) && (nbr == 0)) {
				return 0;  // No data
			}
			wp[cindex] += nbr;
			ncyc++;
		}
		rp32 += 6; // skip 6 words (tag, cluster id, cluster tstamp)
		numev = buff32[rp32++];  // Number of events in the cluster
		size = buff32[rp32++];   // total size (excluding header)
		if ((size > (LLBUFF_SIZE - 32)) || (size == 0) || (numev == 0)) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Wrong cluster size (%d)\n", cindex, size);
			return FERSLIB_ERR_READOUT_ERROR;
		}
		if (rdlog != NULL) fprintf(rdlog, "Cluster Header: size=%d, NumEv=%d\n", size, numev);

		while (wp[cindex] < (size+32)) {  // Read cluster data
			ret = LLtdl_ReadData(cindex, LLBuff[cindex] + wp[cindex], size + 32 - wp[cindex], &nbr);
			if (ret == 2) return 2;
			wp[cindex] += nbr;
		}
	}
	chain = (buff32[rp32] >> 24) & 0xFF;
	node = (buff32[rp32] >> 16) & 0xFF;
	*bindex = FERS_INDEX(tdl_handle[cindex][chain][node]);
	rp32++;  // rp32 is now pointing the 1st word of the event
	evsize = (buff32[rp32] & 0x00FFFFFF) + 20;  // event size in bytes = payload + size (4) + trg-id (8) + tstamp (8)
	Tstamp[*bindex] = (uint64_t)buff32[rp32 + 3] | ((uint64_t)buff32[rp32 + 4] << 32);
	if ((evsize > 1000) || ((rp32 + evsize) > LLBUFF_SIZE)) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Event size too big (%d)\n", cindex, evsize);
		return FERSLIB_ERR_READOUT_ERROR;
	}
	memcpy(EvBuff[*bindex], buff32 + rp32, evsize);
	if (rdlog != NULL) fprintf(rdlog, "Read Event: chain=%d, niode=%d, size=%d\n", chain, node, evsize);
	rp[cindex] += rp32*4 + evsize;
	EvBuff_nb[*bindex] = evsize;
	*nb = evsize;
	if (rp[cindex] >= wp[cindex]) {  // Data in LLBuff_Cnc consumed. Next time take new data from data_receiver
		wp[cindex] = 0; 
		rp[cindex] = 0; 
		last = 1;
	}
	return 0;
}


// *********************************************************************************************************
// Read and Decode Event Data
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Decode raw data from a buffer into the relevant event data structure
// Inputs:		handle = device handle
//				EvBuff = buffer with raw event data
//				nb = num of bytes in EvBuff
// Outputs:		DataQualifier = data qualifier (type of data, used to determine the struct for event data)
//				tstamp_us = time stamp in us (the information is also reported in the event data struct)
//				Event = pointer to the event data struct
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_DecodeEvent(int handle, uint32_t *EvBuff, int nb, int *DataQualifier, double *tstamp_us, void **Event)
{
	uint32_t i, size, ch, hl=0, en, pnt=0;
	int h = FERS_INDEX(handle);
	static FILE *raw = NULL;

	if (EvBuff == NULL) return FERSLIB_ERR_READOUT_NOT_INIT;
	if (nb == 0) return 0;

	// decode event data structure
	size = EvBuff[0] & 0xFFFF;
	*DataQualifier = (EvBuff[0] >> 24) & 0xFF;

	if (*DataQualifier == DTQ_TEST) {  // test Mode 
		*tstamp_us = (double)(((uint64_t)EvBuff[4] << 32) | (uint64_t)EvBuff[3]) * CLK_PERIOD / 1000.0;
		TestEvent[h].tstamp_us = *tstamp_us;
		TestEvent[h].trigger_id = ((uint64_t)EvBuff[2] << 32) | (uint64_t)EvBuff[1];
		TestEvent[h].nwords = size - 5;
		for (i = 0; i < TestEvent[h].nwords; i++) {
			if (i > MAX_TEST_NWORDS) break;
			TestEvent[h].test_data[i] = EvBuff[i + 5];
		}
		*Event = (void*)&TestEvent[h];
	} else if (*DataQualifier & DTQ_SPECT) {
		uint32_t nhits, both_g;
		*tstamp_us = (double)(((uint64_t)EvBuff[4] << 32) | (uint64_t)EvBuff[3]) * CLK_PERIOD / 1000.0;
		SpectEvent[h].tstamp_us = *tstamp_us;
		SpectEvent[h].trigger_id = ((uint64_t)EvBuff[2] << 32) | (uint64_t)EvBuff[1];
		SpectEvent[h].chmask = ((uint64_t)EvBuff[5] << 32) | (uint64_t)EvBuff[6];
		both_g = (*DataQualifier >> 4) & 0x01;  // both gain (LG and HG) are present
		pnt = 7;
		SpectEvent[h].qdmask = 0;
		for (ch=0; ch<64; ch++) {
			uint16_t pedhg = EnablePedCal ? CommonPedestal - PedestalHG[FERS_INDEX(handle)][ch] : 0;
			uint16_t pedlg = EnablePedCal ? CommonPedestal - PedestalLG[FERS_INDEX(handle)][ch] : 0;
			SpectEvent[h].energyLG[ch] = 0;
			SpectEvent[h].energyHG[ch] = 0;
			SpectEvent[h].tstamp[ch] = 0;
			SpectEvent[h].ToT[ch] = 0;
			if ((SpectEvent[h].chmask >> ch) & 0x1) {
				if (both_g) {
					if (EvBuff[pnt] & 0x8000) SpectEvent[h].qdmask |= ((uint64_t)1 << ch);
					SpectEvent[h].energyHG[ch] = (EvBuff[pnt] & 0x3FFF) + pedhg;
					SpectEvent[h].energyLG[ch] = ((EvBuff[pnt]>>16) & 0x3FFF) + pedlg;
					pnt++;
				} else {
					en = (hl == 0) ? EvBuff[pnt] & 0xFFFF : (EvBuff[pnt++]>>16) & 0xFFFF;
					hl ^= 1;
					if (en & 0x8000) SpectEvent[h].qdmask |= ((uint64_t)1 << ch);
					if (en & 0x4000) SpectEvent[h].energyLG[ch] = (en & 0x3FFF) + pedlg;
					else SpectEvent[h].energyHG[ch] = (en & 0x3FFF) + pedhg;
				}
			}
		}
		if ((*DataQualifier & DTQ_TIMING) && (pnt < size)) {
			nhits = size - pnt - 1;  
			for (i=0; i<nhits; i++) { 
				ch = (EvBuff[pnt + i + 1] >> 25) & 0xFF;
				if (ch >= 64) continue;
				if (SpectEvent[h].tstamp[ch] == 0) SpectEvent[h].tstamp[ch] = EvBuff[pnt + i + 1] & 0xFFFF;  // take 1st hit only
				if (SpectEvent[h].ToT[ch] == 0) SpectEvent[h].ToT[ch] = (EvBuff[pnt + i + 1] >> 16) & 0x1FF;  
			}
		}
		*Event = (void *)&SpectEvent[h];
	} else if (*DataQualifier == DTQ_COUNT) {
		*tstamp_us = (double)(((uint64_t)EvBuff[4] << 32) | (uint64_t)EvBuff[3]) * CLK_PERIOD / 1000.0;
		CountingEvent[h].tstamp_us = *tstamp_us;
		CountingEvent[h].trigger_id = ((uint64_t)EvBuff[2] << 32) | (uint64_t)EvBuff[1];
		CountingEvent[h].chmask = 0;
		for (i=5; i<size; i++) {
			ch = (EvBuff[i] >> 24) & 0xFF;
			if (ch < 64) {
				CountingEvent[h].counts[ch] = EvBuff[i] & 0xFFFFFF;
				CountingEvent[h].chmask |= (uint64_t)(UINT64_C(1) << ch);
			} else if (ch == 64) {
				CountingEvent[h].t_or_counts = EvBuff[i] & 0xFFFFFF;
			} else if (ch == 65) {
				CountingEvent[h].q_or_counts = EvBuff[i] & 0xFFFFFF;
			}
		}
		*Event = (void *)&CountingEvent[h];

	//} else if ((*DataQualifier == DTQ_TIMING_CSTART) || (*DataQualifier == DTQ_TIMING_CSTOP) || (*DataQualifier == DTQ_TIMING_STREAMING)) {
	} else if ((*DataQualifier & 0x0F) == DTQ_TIMING) {
		*tstamp_us = (double)(((uint64_t)EvBuff[4] << 32) | (uint64_t)EvBuff[3]) * CLK_PERIOD / 1000.0;
		ListEvent[h].nhits = size - 6;  // 5 word for header + 1 word for time stamp of Tref CTIN: need to take fine time stamp of Tref
		for (i=0; i<ListEvent[h].nhits; i++) { 
			ListEvent[h].channel[i] = (EvBuff[i+6] >> 25) & 0xFF;
			if ((*DataQualifier & 0xF0) == 0x20) {
				ListEvent[h].tstamp[i] = EvBuff[i + 6] & 0x1FFFFFF;
				ListEvent[h].ToT[i] = 0;
			}
			else {
				ListEvent[h].tstamp[i] = EvBuff[i + 6] & 0xFFFF;    // CTIN: if ToT is not present, tstamp takes 25 bits (mask = 0x1FFFFFF)
				ListEvent[h].ToT[i] = (EvBuff[i + 6] >> 16) & 0x1FF;  // CTIN: set ToT = 0 if not present
			}
		}
		*Event = (void *)&ListEvent[h];

	} else if (*DataQualifier == DTQ_WAVE) {
		*tstamp_us = (double)(((uint64_t)EvBuff[4] << 32) | (uint64_t)EvBuff[3]) * CLK_PERIOD / 1000.0;
		WaveEvent[h].tstamp_us = *tstamp_us;
		WaveEvent[h].trigger_id = ((uint64_t)EvBuff[2] << 32) | (uint64_t)EvBuff[1];
		WaveEvent[h].ns = size - 5;
		for (i=0; i < (uint32_t)(WaveEvent[h].ns); i++) {
			WaveEvent[h].wave_hg[i] = (uint16_t)(EvBuff[i+5] & 0x3FFF);
			WaveEvent[h].wave_lg[i] = (uint16_t)((EvBuff[i+5] >> 14) & 0x3FFF);
			WaveEvent[h].dig_probes[i] = (uint8_t)((EvBuff[i+5] >> 28) & 0xF);
		}
		*Event = (void *)&WaveEvent[h];
	}

	// Dump raw data to file (debug mode)
	if (DebugLogs & DBLOG_RAW_DATA_OUTFILE) {
		if (raw == NULL) raw = fopen("RawEvents.txt", "w");
		fprintf(raw, "Brd %02d: Tstamp = %.3f us\n", FERS_INDEX(handle), *tstamp_us);
		for(i=0; i<(uint32_t)(nb/4); i++)
			fprintf(raw, "%08X\n", EvBuff[i]);
		fprintf(raw, "\n");
	}
	return 0;
}




// *********************************************************************************************************
// Allocate/free memory buffers
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Init readout for one board (allocate buffers and initialize variables)
// Inputs:		handle = device handle
// Outputs:		AllocatedSize = tot. num. of bytes allocated for data buffers and descriptors
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_InitReadout(int handle, int ROmode, int *AllocatedSize) {
	uint32_t ctrl;
	static uint8_t mutex_inizialized = 0;

 	*AllocatedSize = 0;
	ReadoutMode = ROmode;
	if (!mutex_inizialized) {
		initmutex(FERS_RoMutex);
		mutex_inizialized = 1;
	}
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
		if (Cnc_NumBoards[FERS_CNCINDEX(handle)] == 0) {  // 1st board connected to this cnc
			LLBuff[FERS_CNCINDEX(handle)] = (char *)malloc(LLBUFF_CNC_SIZE);
			if (LLBuff[FERS_CNCINDEX(handle)] == NULL) return FERSLIB_ERR_MALLOC_BUFFERS;
			*AllocatedSize += LLBUFF_CNC_SIZE;
			FERS_TotalAllocatedMem += LLBUFF_CNC_SIZE;
		}
		tdl_handle[FERS_CNCINDEX(handle)][FERS_CHAIN(handle)][FERS_NODE(handle)] = handle;
		Cnc_NumBoards[FERS_CNCINDEX(handle)]++;
		tdl_handle[FERS_CNCINDEX(handle)][FERS_CHAIN(handle)][FERS_NODE(handle)] = handle;
	} else {
		LLBuff[FERS_INDEX(handle)] = (char *)malloc(LLBUFF_SIZE);
		if (LLBuff[FERS_INDEX(handle)] == NULL) return FERSLIB_ERR_MALLOC_BUFFERS;
		*AllocatedSize += LLBUFF_SIZE;
		FERS_TotalAllocatedMem += LLBUFF_SIZE;
	}
	EvBuff[FERS_INDEX(handle)] = (uint32_t *)malloc(EVBUFF_SIZE);
	if (EvBuff[FERS_INDEX(handle)] == NULL) return FERSLIB_ERR_MALLOC_BUFFERS;
	*AllocatedSize += EVBUFF_SIZE;
	FERS_TotalAllocatedMem += EVBUFF_SIZE;
	//WaveEvent[FERS_INDEX(handle)].wave_hg = (uint16_t *)malloc(MAX_WAVEFORM_LENGTH * sizeof(uint16_t));
	//WaveEvent[FERS_INDEX(handle)].wave_lg = (uint16_t *)malloc(MAX_WAVEFORM_LENGTH * sizeof(uint16_t));
	//WaveEvent[FERS_INDEX(handle)].dig_probes = (uint8_t *)malloc(MAX_WAVEFORM_LENGTH * sizeof(uint8_t));
	//uint16_t *WaveEvent[FERS_INDEX(handle)]->wave_hg[MAX_WAVEFORM_LENGTH];
	//uint16_t *WaveEvent[FERS_INDEX(handle)]->wave_lg[MAX_WAVEFORM_LENGTH];
	//uint8_t *WaveEvent[FERS_INDEX(handle)]->dig_probes[MAX_WAVEFORM_LENGTH];
	*AllocatedSize += (MAX_WAVEFORM_LENGTH * (2 * sizeof(uint16_t) + sizeof(uint8_t)));
	FERS_TotalAllocatedMem += (MAX_WAVEFORM_LENGTH * (2 * sizeof(uint16_t) + sizeof(uint8_t)));

	// If event sorting is required, create queues
	if (ReadoutMode != ROMODE_DISABLE_SORTING) {
		queue[FERS_INDEX(handle)] = (uint32_t*)malloc(FERSLIB_QUEUE_SIZE * sizeof(uint32_t));
		FERS_TotalAllocatedMem += FERSLIB_QUEUE_SIZE * sizeof(uint32_t);
		q_wp[FERS_INDEX(handle)] = 0;
		q_rp[FERS_INDEX(handle)] = 0;
		q_nb[FERS_INDEX(handle)] = 0;
		q_full[FERS_INDEX(handle)] = 0;
		q_tstamp[FERS_INDEX(handle)] = 0;
		q_trgid[FERS_INDEX(handle)] = 0;
	}

	FERS_ReadRegister(handle, a_acq_ctrl, &ctrl);
	if (ctrl & (1 << 15)) 
		EnableStartEvent[FERS_INDEX(handle)] = 1;

	RO_NumBoards++;
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Init Readout. Tot Alloc. size = %.2f MB\n", FERS_INDEX(handle), (float)(FERS_TotalAllocatedMem)/(1024*1024));
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: De-init readoout (free buffers)
// Inputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_CloseReadout(int handle) {
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
		Cnc_NumBoards[FERS_CNCINDEX(handle)]--;
		tdl_handle[FERS_CNCINDEX(handle)][FERS_CHAIN(handle)][FERS_NODE(handle)] = -1;
		if ((LLBuff[FERS_INDEX(handle)] != NULL) && (Cnc_NumBoards[FERS_CNCINDEX(handle)] == 0)) {  // Last board disconnected
			free(LLBuff[FERS_INDEX(handle)]);
			LLBuff[FERS_INDEX(handle)] = NULL;
		}
	} else {
		if (LLBuff[FERS_INDEX(handle)] != NULL)  {  
			free(LLBuff[FERS_INDEX(handle)]);	// DNIN: it trows an acception after usb fw upgrader
			LLBuff[FERS_INDEX(handle)] = NULL;
		}
	}
	if (EvBuff[FERS_INDEX(handle)] != NULL) {
		free(EvBuff[FERS_INDEX(handle)]);
		EvBuff[FERS_INDEX(handle)] = NULL;
	}
	if (queue[FERS_INDEX(handle)] != NULL) free(queue[FERS_INDEX(handle)]);
	RO_NumBoards--;
	if ((RO_NumBoards == 0) && (WaveEvent[FERS_INDEX(handle)].wave_hg != NULL)) {  // Last board connected => can free waveform buffers
		//free(WaveEvent[FERS_INDEX(handle)].wave_hg);
		//free(WaveEvent[FERS_INDEX(handle)].wave_lg);
		//free(WaveEvent[FERS_INDEX(handle)].dig_probes);
		//WaveEvent[FERS_INDEX(handle)].wave_hg = NULL;
	}
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Close Readout\n", FERS_INDEX(handle));
	return 0;
}


// *********************************************************************************************************
// Start/Stop Acquisition, Flush Data
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Set ReadoutState = RUNNING, wait until all threads are running, then send the run command  
//				to the boards, according to the given start mode
// Inputs:		handle = device handles (af all boards)
// 				NumBrd = number of boards to start
//				StartMode = start mode (Async, T0/T1 chain, TDlink)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_StartAcquisition(int *handle, int NumBrd, int StartMode) {
	int ret=0, b, tdl = 1, rc;
	for(b = 0; b < NumBrd; b++) {
		if (handle[b] == -1) continue;
		ret |= FERS_FlushData(handle[b]);
		if (FERS_CONNECTIONTYPE(handle[b]) != FERS_CONNECTIONTYPE_TDL) tdl = 0;
	}
	// Check that all RX-threads are in idle state (not running)
	for (int i=0; i<100; i++) {
		lock(FERS_RoMutex);
		rc = FERS_RunningCnt;
		unlock(FERS_RoMutex);
		if (rc == 0) break;
		Sleep(10);
	}
	if (rc > 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] %d RX-thread still running while starting a new run\n", rc);
		return FERSLIB_ERR_START_STOP_ERROR;
	}

	lock(FERS_RoMutex);
	FERS_ReadoutStatus = ROSTATUS_RUNNING;
	unlock(FERS_RoMutex);
	// Check that all RX-threads are running
	for (int i=0; i<100; i++) {
		lock(FERS_RoMutex);
		rc = FERS_RunningCnt;
		unlock(FERS_RoMutex);
		if ((tdl && (rc == 1)) || (rc == NumBrd)) break;  // CTIN: count connected concentrators...
		Sleep(10);
	}
	if ((tdl && (rc == 0)) || (!tdl && (rc < NumBrd))) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] %d RX-thread are not running after the run start\n", rc);
		return FERSLIB_ERR_START_STOP_ERROR;
	}

	if (StartMode == STARTRUN_TDL) {
		ret |= FERS_SendCommandBroadcast(handle, CMD_RES_PTRG, 0); 
		ret |= FERS_SendCommandBroadcast(handle, CMD_TIME_RESET, 0);
		ret |= FERS_SendCommandBroadcast(handle, CMD_ACQ_START, 0);
	} else if ((StartMode == STARTARUN_CHAIN_T0) || (StartMode == STARTRUN_CHAIN_T1)) {
		ret |= FERS_SendCommand(handle[0], CMD_TIME_RESET);	
		ret |= FERS_SendCommand(handle[0], CMD_ACQ_START);
	} else {
		for(b = 0; b < NumBrd; b++) {
			if (handle[b] == -1) continue;
			ret |= FERS_SendCommand(handle[b], CMD_TIME_RESET);
			ret |= FERS_SendCommand(handle[b], CMD_ACQ_START);	
		}
	}
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO] Start Run (ret=%d)\n", ret);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Send the stop command to the boards (according to the same mode used to start them).
//				NOTE: this function stops the run in the hardware (thus stopping the data flow) but it
//				doesn't stop the RX threads because they need to empty the buffers and the pipes.
//				The threads will stop automatically when there is no data or when a flush command is sent.
// Inputs:		handle = device handles (af all boards)
// 				NumBrd = number of boards to start
//				StartMode = start mode (Async, T0/T1 chain, TDlink)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_StopAcquisition(int *handle, int NumBrd, int StartMode) {
	int ret=0, b;
	if (StartMode == STARTRUN_TDL) {
		ret |= FERS_SendCommandBroadcast(handle, CMD_ACQ_STOP, 0);
	} else if ((StartMode == STARTARUN_CHAIN_T0) || (StartMode == STARTRUN_CHAIN_T1)) {
		ret |= FERS_SendCommand(handle[0], CMD_ACQ_STOP);
	} else {
		for(b = 0; b < NumBrd; b++) {
			if (handle[b] == -1) continue;
			ret |= FERS_SendCommand(handle[b], CMD_ACQ_STOP);	
		}
	}
	FERS_ReadoutStatus = ROSTATUS_EMPTYING;
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO] Stop Run (ret=%d)\n", ret);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: flush data (Read and discard data until the RX buffer is empty)
// Inputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_FlushData(int handle) {
	if (FERS_ReadoutStatus == ROSTATUS_RUNNING) return FERSLIB_ERR_OPER_NOT_ALLOWED;
	FERS_ReadoutStatus = ROSTATUS_FLUSHING;
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
		int cindex = FERS_CNCINDEX(handle);
		if (!Cnc_Flushed[cindex]) {
			LLtdl_Flush(cindex);  // Flush concentrator (that will flush all FERS board by hardware commmands)
			Sleep(100);
			Cnc_Flushed[cindex] = 1;
		}
	} else {
		int nb;
		FERS_SendCommand(handle, CMD_CLEAR);
		eth_usb_ReadRawEvent(handle, &nb);  // make a read to let all processes to reset buffers
	}
	EvBuff_nb[FERS_INDEX(handle)] = 0;
	if (ReadoutMode != ROMODE_DISABLE_SORTING) {
		q_wp[FERS_INDEX(handle)] = 0;
		q_rp[FERS_INDEX(handle)] = 0;
		q_nb[FERS_INDEX(handle)] = 0;
		q_full[FERS_INDEX(handle)] = 0;
		q_tstamp[FERS_INDEX(handle)] = 0;
		q_trgid[FERS_INDEX(handle)] = 0;
	}
	Sleep(100);
	FERS_ReadoutStatus = ROSTATUS_IDLE;
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Read and decode one event from the readout buffers. There are two readout modes: sorted or unsorted
//              If sorting is requested, the readout init function will allocate queues for sorting
// Inputs:		handle = pointer to the array of handles (of all boards)
// Outputs:		bindex = index of the board from which the event comes
//				DataQualifier = data qualifier (type of data, used to determine the struct for event data)
//				tstamp_us = time stamp in us (the information is also reported in the event data struct)
//				Event = pointer to the event data struct
//				nb = size of the event (in bytes)
// Return:		0=No Data, 1=Good Data 2=Not Running, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_GetEvent(int *handle, int *bindex, int *DataQualifier, double *tstamp_us, void **Event, int *nb) {
	int ret, h, h_found = -1, i, nbb;
	uint64_t oldest_ev = 0;

	*nb = 0;
	*tstamp_us = 0;
	*DataQualifier = 0;
	*bindex = 0;

	// ---------------------------------------------------------------------
	// SORTED
	// ---------------------------------------------------------------------
	if (ReadoutMode != ROMODE_DISABLE_SORTING) {
		int qsel, qi;
		static int nodata_cnt[FERSLIB_MAX_NBRD] = { 0 };
		static uint64_t nodata_time[FERSLIB_MAX_NBRD] = { 0 };
		static int timed_out[FERSLIB_MAX_NBRD] = { 0 };

		// Check if there are empty queues and try to fill them
		for (i = 0; i < FERSLIB_MAX_NBRD; i++) {
			if (handle[i] == -1) break;
			if ((q_tstamp[FERS_INDEX(handle[i])] == 0) && !q_busy) {  // queue is empty => try to read new data
				if (FERS_CONNECTIONTYPE(handle[i]) == FERS_CONNECTIONTYPE_TDL) {
					ret = tdl_ReadRawEvent(FERS_CNCINDEX(handle[i]), &qi, &nbb);
				} else {
					qi = FERS_INDEX(handle[i]);
					ret = eth_usb_ReadRawEvent(handle[i], &nbb);
				}
				if (ret == 2) return 2;
				if (ret < 0) return ret;
				if ((nbb == 0) && !timed_out[qi]) {  // No data from this board; if timeout not reached, continue to wait (the funtion returns 0)
					if (nodata_cnt[qi] == 0) {
						nodata_time[qi] = get_time();
					} else if ((nodata_cnt[qi] & 0xFF) == 0) {
						uint64_t tnd = get_time() - nodata_time[qi];  // time since no data
						if ((get_time() - nodata_time[qi]) > FERSLIB_QUEUE_TIMEOUT_MS) 
							timed_out[qi] = 1;
					}
					nodata_cnt[qi]++;
					if (!timed_out[qi]) return 0;
				} else {
					nodata_cnt[qi] = 0;
				}
				if (nbb > 0) 
					ret = q_push(qi, EvBuff[qi], EvBuff_nb[qi]);
				if (ret < 0 ) {
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][BRD %02d] Queue Push error (ret = %d)\n", qi, ret);
					return ret;
				}
				EvBuff_nb[qi] = 0;
			}
		}

		// Search for oldest tstamp
		oldest_ev = -1;
		qsel = -1;
		for (i = 0; i < FERSLIB_MAX_NBRD; i++) {
			if (handle[i] == -1) break;
			qi = FERS_INDEX(handle[i]);
			if (ReadoutMode == ROMODE_TRGTIME_SORTING) {
				if ((oldest_ev == -1) || (q_tstamp[qi] < oldest_ev)) {
					oldest_ev = q_tstamp[qi];
					qsel = qi;
				}
			} else {
				if ((oldest_ev == -1) || (q_trgid[qi] < oldest_ev)) {
					oldest_ev = q_trgid[qi];
					qsel = qi;
				}
			}
		}

		// get and decode oldest event 
		ret = q_pop(qsel, EvBuff[qsel], &EvBuff_nb[qsel]);
		if (ret < 0) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][BRD %02d] Queue Pop error (ret = %d)\n", qi, ret);
			return ret;
		}
		FERS_DecodeEvent(handle[qsel], EvBuff[qsel], EvBuff_nb[qsel], DataQualifier, tstamp_us, Event);
		*nb = EvBuff_nb[qsel];
		*bindex = qsel;
		EvBuff_nb[qsel] = 0;


	// ---------------------------------------------------------------------
	// UNSORTED
	// ---------------------------------------------------------------------
	} else {
		static int init = 1;
		static int NumCnc = 0;				// Total number of concentrators
		static int NumDir = 0;				// Total number of boards with direct connection (no concentrator)
		static int curr_cindex = -1;		// Index of the concentrator being read

		// First call: find number of concentrators and direct connections
		if (init) {
			int ci = -1;
			for(i=0; (i < FERSLIB_MAX_NBRD) && (handle[i] >= 0); i++) {
				if (FERS_CONNECTIONTYPE(handle[i]) == FERS_CONNECTIONTYPE_TDL) {
					if (FERS_CNCINDEX(handle[i]) > ci) ci = FERS_CNCINDEX(handle[i]);  // highest cnc index
				} else {
					NumDir++;
				}
			}
			NumCnc = ci + 1;
			if (NumCnc > 0) curr_cindex = 0;
			init = 0;
		}

		if (curr_cindex >= 0) {
			for(i=0; i<NumCnc; i++) {  // CTIN: add time sorting between concentrators. For the moment it is a simple round robin
				h = FERS_CNCINDEX(handle[i]);
				ret = tdl_ReadRawEvent(curr_cindex, bindex, nb);
				if (ret < 0) return ret;
				if (ret == 2) return 2;
				curr_cindex = (curr_cindex == (NumCnc-1)) ? 0 : curr_cindex + 1;
				if (*nb > 0) {
					FERS_DecodeEvent(handle[i], EvBuff[*bindex], EvBuff_nb[*bindex], DataQualifier, tstamp_us, Event);
					EvBuff_nb[*bindex] = 0;
					break;
				}
			}
		} else {
			for(i=0; (i < FERSLIB_MAX_NBRD) && (handle[i] >= 0); i++) {
				h = FERS_INDEX(handle[i]);
				if (EvBuff_nb[h] == 0) {
					ret = eth_usb_ReadRawEvent(handle[i], nb);
					if (ret < 0) return ret;
					if (ret == 2) return 2;
				}
				if ((EvBuff_nb[h] > 0) && ((oldest_ev == 0) || (oldest_ev > Tstamp[h]))) {
					oldest_ev = Tstamp[h];
					h_found = h;
				}
			}
			if (h_found >= 0) {
				FERS_DecodeEvent(handle[h_found], EvBuff[h_found], EvBuff_nb[h_found], DataQualifier, tstamp_us, Event);
				*nb = EvBuff_nb[h_found];
				*bindex = h_found;
				EvBuff_nb[h_found] = 0;
			}
		}
	}

	return 1;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Read and decode one event from one specific board
// Inputs:		handle = board handle
// Outputs:		DataQualifier = data qualifier (type of data, used to determine the struct for event data)
//				tstamp_us = time stamp in us (the information is also reported in the event data struct)
//				Event = pointer to the event data struct
//				nb = size of the event (in bytes)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_GetEventFromBoard(int handle, int *DataQualifier, double *tstamp_us, void **Event, int *nb) {
	int h, ret=0;
	uint64_t oldest_ts = 0;
	h = FERS_INDEX(handle);
	if (EvBuff_nb[h] == 0) {
		if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
			int bindex;
			int cindex = FERS_CNCINDEX(handle);
			ret = tdl_ReadRawEvent(cindex, &bindex, nb);
		} else {
			ret = eth_usb_ReadRawEvent(handle, nb);
		}
		if (ret < 0) return ret;
		if (ret == 2) return 2;
	}
	if (EvBuff_nb[h] > 0) {
		ret |= FERS_DecodeEvent(handle, EvBuff[h], EvBuff_nb[h], DataQualifier, tstamp_us, Event);
		*nb = EvBuff_nb[h];
		EvBuff_nb[h] = 0;
	}
	if (*nb == 0) return 1; // originally 0, shouldn't it be 1?
	else return 0;
}


