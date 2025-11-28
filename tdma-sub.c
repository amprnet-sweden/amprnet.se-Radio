#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include "Ampr-radio.h"
#include "tdma.h"

#define ttabsize 4

struct tdmatable ttab[ttabsize];

void settdma(uint8_t *macaddr,uint8_t *call, int value, uint8_t hrssi, uint32_t volt) {
	int found = 0;
	int i;
/*	xprint("proc ");
    for (int j=0;j<6;j++) {
        xprint_xchar(macaddr[j]);
        if(j<5)
           xprint(":");
        else
           xprint(" ");
    }
    xprint("\n"); */
	  for(i=0;i<ttabsize;i++) {
 	 	if(memcmp(&ttab[i].macaddr,macaddr,6)==0) {	// if we find it
 	 	   found = 1;
 /*	 	   xprint("FND idx ");
 	 	   xprint_int(i);
 	 	   xprint("\n"); */
 	 	   ttab[i].ttl = 25;
 	 	   ttab[i].timer = value;
    	   ttab[i].mrssi = rssi;			// the rssi of the tdma packet
    	   ttab[i].hrssi = hrssi;			// and report his sig strength
    	   ttab[i].volt = volt;
 	    }
 	 }
 	 if (found == 0) {		// it was not in the table
// 		 xprint("not fnd");
 	 	 for (i=0;i<ttabsize;i++) {	// find emply slot
 	 	 	if(ttab[i].ttl == 0)	{	//found an empty slot
 /*	 	 		xprint("Empty ");
 	 	 		xprint_int(i);
 	 	 		xprint("\n"); */
              memcpy(&ttab[i].macaddr[0],macaddr,6);  //store mac addr
              memcpy(&ttab[i].call[0],call,12);    //store callsign
              ttab[i].timer = value;
              ttab[i].ttl =25;
    		  ttab[i].mrssi = rssi;			// the rssi of the tdma packet
    		  ttab[i].hrssi = hrssi;			// and report his sig strength
              ttab[i].volt = volt;
    		  break;
 	 	 	}
 	 	 }
 	 }
// 	 showtdma();
}


void flush_ttab(int index) {
	for(int j=0;j<6;j++) {
		ttab[index].macaddr[j] = 0;
	}
//	for(int j=0;j<11;j++) {
	    ttab[index].call[0] = 0;
//	}
	ttab[index].timer = 0;
	ttab[index].mrssi = 0;
    ttab[index].hrssi = 0;
    ttab[index].volt = 0;
}

void tdma_ttl(void) {
	for (int i=0;i<ttabsize;i++) {
	    if(ttab[i].ttl != 0) {      // if this entry is valid
		  ttab[i].ttl--;            // will it be?
		  if(ttab[i].ttl == 0) {    // if it became 0, flush whol entry
			flush_ttab(i);
		  }
	    }
	}
}
void proc_tdma_packet(uint8_t *buffer, char count) {
        TRpkts++;
        switch(buffer[1]) {
          case TTDMA: {
              int val = (buffer[8] | buffer [9] << 8 | buffer[10] << 16 | buffer[11] << 24);
              int32_t volt = (buffer[25] | buffer[26] << 8 | buffer[27] << 16 | buffer[28] << 24);
              settdma(&buffer[2],&buffer[12],val,buffer[24],volt);
//              Timer_def = val;         // we receive a peer packet with a defer indication
//              GPIO_write(sigpin,1);
    //        GPIO_toggle(sigpin2);
    //        xprint("TTDMA from ");
    //        for( int i = 0 ; i<6;i++) {
    //            xprint_xchar(buffer[i + 2]);
    //            if (i < 5) xprint(":");
    //        }
    /*        int val = (buffer[8] | buffer [9] << 8 | buffer[10] << 16 | buffer[11] << 24);
              xprint(" Val ");
              xprint_int(val);
              xprint("\n"); */
              break;
          }
          case TIDENT:
          case TCONN:
          case TACK:
          case TNACK: {
              for(int i=0;i<count;i++) {
                  xprint_xchar(buffer[i]);
                  if(i%64 == 63) {
                    xprint("\n");
                  } else {
                    xprint(" ");
                  }
              }
              xprint("\n");
              break;
          }
          default: {
              xprint("Undef TDMA\n");
              for(int i=0;i<count;i++) {
                  xprint_xchar(buffer[i]);
                  if(i%64 == 63) {
                      xprint("\n");
                  } else {
                      xprint(" ");
                  }
              }
              xprint("\n");
              break;
          }
        }
    }
int current_defer = 1500;
uint8_t tbuffer[TSIZE];

void send_tdma_packet(void) {
    uint32_t AONBatMonBatteryVoltageGet();
    uint32_t volt;
    volt = AONBatMonBatteryVoltageGet();
    volt = (volt * 125) >> 5;
    tbuffer[0] = PTDMA;
    tbuffer[1] = TTDMA;
    memcpy(&tbuffer[2],my_hwaddr,6);
    tbuffer[8] = current_defer & 255;
    tbuffer[9] = current_defer >> 8 & 255;
    tbuffer[10] = current_defer >> 16 & 255;
    tbuffer[11] = current_defer >> 24 & 255;
    for(int i=0;i<12;i++) {
        tbuffer[12+i] = my_call[i];
    }
    tbuffer[24] = rssi;
    tbuffer[25] = volt & 255;
    tbuffer[26] = volt >> 8 & 255;
    tbuffer[27] = volt >> 16 & 255;
    tbuffer[28] = volt >> 24 & 255;
    RF_XMIT(tbuffer, TSIZE);
    TSpkts++;
}
void showtdma(void) {
    char cbuf[12];
    xprint("TDMA table   MAC    Call    Timer   TTL Lrssi Rrssi  Batt\n");
    for (int i=0;i<ttabsize;i++) {
//  xprint("MAC ");
    for (int j=0;j<6;j++) {
        xprint_xchar(ttab[i].macaddr[j]);
        if(j<5)
           xprint(":");
        else
           xprint(" ");
    }
    for(int j=0;j<12;j++) {
        cbuf[j]=ttab[i].call[j];
    }
    xprint(cbuf);
    xprint(" ");
    xprint_int(ttab[i].timer);
    xprint("   ");
    xprint_int(ttab[i].ttl);
    xprint("  ");
    xprint_schar(ttab[i].mrssi);
    xprint("   ");
    xprint_schar(ttab[i].hrssi);
    xprint("    ");
    sprintf(cbuf,"%ld.%03ld",ttab[i].volt/1000,ttab[i].volt%1000);
//    sprintf(cbuf,"%ld.%03ld",ui32CurrentBatteryt/1000,ui32CurrentBatteryt%1000);
    xprint(cbuf);
    xprint("\n");
    }
}
