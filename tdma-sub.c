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


struct tdmatable ttab[TTABSIZE];
struct conntable ctab[MAXSLAVES];
struct tdmalist  tlist[MAXSLAVES+1];    // latest tdma order, including master
int cstate = IDLE;                         // cstate 1 auto slave and connect
int autoconnect = 1;                        // autoconnect enable
int myidx = 0;                          // my index into tdmalist
uint8_t lastslave[6];

void xprintMAC(uint8_t * mac) {
    for(int i=0;i<6;i++) {
        xprint_xchar(mac[i]);
        if(i<5)
            xprint(":");
        else
            xprint(" ");
    }
}
void settdma(uint8_t * macaddr,uint8_t * call, uint8_t hrssi, uint8_t volt) {
	int found = 0;
	int i;
	for(i=0;i<TTABSIZE;i++) {
 	 	if(memcmp(&ttab[i].macaddr,macaddr,6)==0) {	// if we find it
 	 	   found = 1;
/* 	 	   xprint("FND idx ");
 	 	   xprint_int(i);
 	 	   xprint("\n"); */
 	 	   ttab[i].ttl = TTABTTL;
// 	 	   ttab[i].timer = 0;
    	   ttab[i].mrssi = rssi;			// the rssi of the tdma packet
    	   ttab[i].hrssi = hrssi;			// and report his sig strength
    	   ttab[i].volt = volt;
 	    }
 	 }
 	 if (found == 0) {		// it was not in the table
// 		 xprint("not fnd");
 	 	 for (i=0;i<TTABSIZE;i++) {	// find emply slot
 	 	 	if(ttab[i].ttl == 0)	{	//found an empty slot
/* 	 	 		xprint("Empty ");
 	 	 		xprint_int(i);
 	 	 		xprint("\n"); */
              memcpy(&ttab[i].macaddr[0],macaddr,6);  //store mac addr
              memcpy(&ttab[i].call[0],call,12);    //store callsign
//              ttab[i].timer = 0;
              ttab[i].ttl =TTABTTL;
    		  ttab[i].mrssi = rssi;			// the rssi of the tdma packet
    		  ttab[i].hrssi = hrssi;			// and report his sig strength
              ttab[i].volt = volt;
    		  break;
 	 	 	}
 	 	 }
 	 }
}


void flush_ttab(int index) {
	for(int j=0;j<6;j++) {
		ttab[index].macaddr[j] = 0;
	}
//	for(int j=0;j<11;j++) {
	    ttab[index].call[0] = 0;
//	}
//	ttab[index].timer = 0;
	ttab[index].mrssi = 0;
    ttab[index].hrssi = 0;
    ttab[index].volt = 0;
}

void flush_ctab(int index) {
    for (int i = 0;i<6;i++) {
        ctab[index].macaddr[i] = 0;
    }
    new_synch = 1;                  // we need a new synch if we removed a node from ctab
}
void tdma_ttl(void) {
	for (int i=0;i<TTABSIZE;i++) {
	    if(ttab[i].ttl != 0) {      // if this entry is valid
		  ttab[i].ttl--;            // will it be?
		  if(ttab[i].ttl == 0) {    // if it became 0, flush whol entry
			flush_ttab(i);
		  }
	    }
	}
	for(int i =0;i<MAXSLAVES;i++) {
	    if(ctab[i].ttl != 0) {
	        ctab[i].ttl--;          // enable this to get ttl flush
	        if(ctab[i].ttl == 0) {
	            flush_ctab(i);
	        }
	    }
	}

}

void dump_tdma(uint8_t * buffer,char count) {
    xprint("TDMA ");
    xprint_xchar(buffer[1]);
    xprint(" from ");
    xprintMAC(&buffer[2]);
    xprint(" call ");
    xprint_char(buffer[8]);
    xprint(" ");
    switch(buffer[1]) {
    case 1:
        xprint("SY ");
        xprint("\n");
        for(int i=0;i<5;i++) {
            xprintMAC(&buffer[22 + (i*6)]);
            xprint("\n");
        }
        break;
    case 5:
        xprint("TT ");
        break;
    case 6:
        xprint("TI ");
        break;
    case 7:
        xprint("CO ");
        break;
    case 11:
        xprint("IN ");
        break;
    default:
        xprint_char(buffer[1]);
        break;
    }
/*    for(int i=0;i<count;i++) {
        xprint_xchar(buffer[i]);
        if(i%64 == 63) {
          xprint("\n");
        } else {
          xprint(" ");
        }
    } */
    xprint("\n");
}

void update_ctab(uint8_t * addr) {
    int found,i;
    for(i=0;i<MAXSLAVES;i++) {
        if(memcmp(addr,ctab[i].macaddr,6) == 0) {
            ctab[i].ttl = CTABTTL;
        }
    }

}
/* since tdma packets normally comes AFTER a transmission */
void proc_tdma_packet(uint8_t *buffer, char count) {
    int i,j,found_empty,enternew,found;
    GPIO_write(sigpin2,1);
    if(debug & 512) {
        dump_tdma(buffer, count);
    }
    if((role == 0) && (buffer[1] != TINVITE)) { // if we are a slave, match packet for the ID before OUR, this means our slot is next
                                                // dont do this if invite, sync or ttdm will follow after invite slot
      if(memcmp(&buffer[2],&tlist[myidx-1].macaddr,6) == 0) { // was this the node befor ours?
          if(memcmp(my_hwaddr,&tlist[myidx].macaddr,6) ==0) {  // and we should be the next?
            if(debug & 512) {
              xprint("Ourslot ");
              xprint_int(myidx);
              xprint("\n");
            }
            myslot = 1;
            GPIO_write(sigpin,1);
          }
      }
    }
    TRpkts++;
    if(role != 0) {
           Timer_tdm = TDMAPERIOD;                //prevent interference when running tdma cycle
           update_ctab(&buffer[2]);        // all received packets should update ctab of master
    }
        switch(buffer[1]) {
          case TTDMA: {
              uint8_t volt = buffer[21];
              settdma(&buffer[2],&buffer[8],buffer[20],volt);
              if(role != 0) {
                  if(memcmp(&buffer[2],lastslave,6) == 0) {
//                    xprint("time for Master\n");
                      if(debug & 2048) {
                        xprint("L");  // debug we found last slot
                      }
                      myslot = 1;
                      GPIO_write(sigpin,1);
                  }
              }
          }
          break;
          case TCONN: {
              if(role != 0) {   // we are master
//                Timer_def = 0;
                new_synch = 1;  // Send synch even if the client is already in the table, since the client may have restarted and not know it was connected
                enternew = 1;   // assume it is not there
                for(i=0;i < MAXSLAVES;i++) {      //check if already in ctab
                    int found = memcmp(&ctab[i].macaddr,&buffer[2],6);          // is this connect known?
                    if(found == 0) {                                            // if 0 yes
                        enternew = 0;                                           // do not reenter ID
//                        xprint("In CTAB\n");
                        ctab[i].ttl = 25;                                       // just update ctab ttl
                    }
                }
                if(enternew) {     // no, it was not, then enter it
                  for(i=0;i<MAXSLAVES;i++) {       // find free entry in connect table
                    if(ctab[i].ttl == 0) {
//                      found_empty = 1;
                      xprint("Found empty ");
                      xprint_int(i);
                      xprint("\n");
                      memcpy(&ctab[i].macaddr,&buffer[2],6);
                      ctab[i].ttl = 25;
                      break;
                    }
                  }
                }
              }
          }
          break;
          case TDISC: {
              if(role != 0) {   // we are master
                 for(i=0;i<MAXSLAVES;i++) {
                     if(memcmp(&ctab[i].macaddr,&buffer[2],6) == 0) { // it is in ctab
                         ctab[i].ttl = 0;   // clear out its ttl
                         for(j=0;j<6;j++) {
                             ctab[i].macaddr[j] = 0;
                         }
                         xprint("Disconnected node ");
                         xprintMAC(&buffer[2]);
                         xprint("\n");
                         new_synch = 1;

                     }
                 }
              } //master
          }
          break;
/*          case TNACK: {
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
          } */
          case TSYNC: {
              dump_tdma(buffer, count);
              if(debug & 1024) {
              xprint("Master TSYNC\n");
                xprint("M ");
                for(j=0;j<6;j++) {
                    xprint_xchar(buffer[2+j]);
                    if(j<5)
                        xprint(":");
                    else
                        xprint(" ");
                }
                xprint("\n"); // PRINTMAC

                for(i=0;i<MAXSLAVES;i++) {
                  xprint_int(i);
                  xprint(" ");
                  for(j=0;j<6;j++) {
                      xprint_xchar(buffer[28 +j+(i*6)]);
                      if(j<5)
                          xprint(":");
                      else
                          xprint(" ");
                  }
                  xprint("\n");
                }
              } // debug
              uint8_t volt = buffer[21];
              settdma(&buffer[2],&buffer[8],buffer[20],volt);   // update tdma table
              memcpy(&tlist[0].macaddr, &buffer[22],30);          // save as recent tdma list
              if(role == 0) {                               // if I am a slave
                  found = 0;
                  for(i=0;i<MAXSLAVES+1;i++) {                     // look through synch packet to se if I am there, in that I am connected
                      j = memcmp(&tlist[i].macaddr,my_hwaddr,6);   // check for y id in tist
                      if((j == 0) && (cstate  == CONNECTING)) {                // if I am, I am conneclted
                       xprint("Connected index ");
                       xprint_int(i);
                       myidx = i;                                     // this is my slave index
                       xprint("\n");
                       found =1;
                       cstate = CONNECTED;                                    // change state to connected
                      }
                      if((found == 0)  && (cstate >= 2) ) {      //this disconnect us, either we did disc or we dropped out of ctab
                          xprint("Disc tlst\n");
/*                          if(autoconnect)
                              cstate = 1;
                          else */
                              cstate = IDLE; // state is now idle
                      }
                  }
              } // slaves only
          }
          break;
          case TINVITE: {
              if(role == SLAVE) {
              GPIO_write(sigpin3,1);
/*              if((autoconnect) && (cstate == 0))
                  cstate = 1; */
              i = rand() & 3;
              if((cstate == CONNECTING)&& i==3) {       // probability to connect 25%
//                xprint("INVITE rcvd");
                  myslot = 1;
                RX_OFF();
                  tdma_connect();
                RX_ON();
              }
              GPIO_write(sigpin3,0);
              }
          }
          break;
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
          }
          break;
        }
        GPIO_write(sigpin2,0);
}
int current_defer = 1500;
uint8_t tbuffer[TSIZE];


void tdma_header(char type) {
    tbuffer[0] = PTDMA;     // make a TDMA packet
    tbuffer[1] = type;
    memcpy(&tbuffer[2],my_hwaddr,6);
    for(int i=0;i<12;i++) {
      tbuffer[8+i] = my_call[i];
    }
}
void tdma_connect(void) {
//    int prob;             // the rand function does not seem ok in stdlib
//    prob = rand();
    if(cstate != IDLE) {
      tbuffer[0] = PTDMA;
      tbuffer[1] = TCONN;
      memcpy(&tbuffer[2],my_hwaddr,6);
      for(int i=0;i<12;i++) {
        tbuffer[8+i] = my_call[i];
      }
      cstate = CONNECTING;
      delay(1);
      RF_XMIT(tbuffer, 20);
      TSpkts++;
      if(debug & 1024) {
        xprint("Sent connect\n");
        for(int i=0;i<20;i++) {
            xprint_xchar(tbuffer[i]);
            xprint(" ");
        }
        xprint("\n");
      }
    }
}
void tdma_disconnect(void) {
//    int prob;             // the rand function does not seem ok in stdlib
//    prob = rand();
    tbuffer[0] = PTDMA;
    tbuffer[1] = TDISC;
    memcpy(&tbuffer[2],my_hwaddr,6);
    for(int i=0;i<12;i++) {
      tbuffer[8+i] = my_call[i];
    }
    RF_XMIT(tbuffer, 20);
    TSpkts++;
//  if(debug & 1024) {
        xprint("Disconnecting\n");
        for(int i=0;i<20;i++) {
            xprint_xchar(tbuffer[i]);
            xprint(" ");
        }
        xprint("\n");
//  }
}
/* send a TDMA packet periodicly */
int invctr;
int new_synch;
void send_tdma_packet(void) {
    uint32_t AONBatMonBatteryVoltageGet();
    uint32_t volt;
    uint8_t bvolt;
    int i,j,tlen;
    volt = AONBatMonBatteryVoltageGet();
    volt = (volt * 125) >> 5;
    bvolt = volt/20;
    tbuffer[0] = PTDMA;
     tbuffer[1] = TTDMA;
     // fill in your ID
     memcpy(&tbuffer[2],my_hwaddr,6);
     for(int i=0;i<12;i++) {
       tbuffer[8+i] = my_call[i];
     }
     tbuffer[20] = rssi;
     tbuffer[21] = bvolt & 255;
     tlen = 22;                        // assume NOT SYNCH packet
     if(role == MASTER) {              // only master does this
         if(invctr == 0) {             // if it is time for an invite
//           GPIO_write(sigpin,1);
           invctr = INVPERIOD;         // set up the invite period
           tbuffer[1] = TINVITE;       //
//           Timer_def = INVSLOT;
         } else {
             invctr--;
             if(new_synch == 1) {       // if it is time to send a new TDMA list
                 tbuffer[1] = TSYNC;    //
                 bzero(&tbuffer[22],30);      // clear out member list
                 memcpy(&tbuffer[22],my_hwaddr,6);        //store our addr in tlist to send
                 for(i=0;i< MAXSLAVES;i++) {         // iterate ctab
                   if(ctab[i].ttl != 0) {          // if this ctab entry is not timed out
                       memcpy(&tbuffer[28 + (i*6)],&ctab[i].macaddr,6);    // fill in ctab ID's
                       memcpy(lastslave,&ctab[i].macaddr,6);                // remember last slave
                       xprint("Last ");
                       xprintMAC(lastslave);
                       xprint("\n");
                   }
                   memcpy(&tlist[0].macaddr,&tbuffer[22],30);           // and to local list
                 }
                 new_synch = 0;
                 tlen = 52;  // same as normal PACKET + TDMA list
                 if(debug & 512) {
                   xprint("M ");
                   xprint_char(tbuffer[1]);
                   xprint("\n");
                 }
                 new_synch = 0;
             }
         }      // we are not sending INVITE
     } else {   // we are slave
         if(cstate == DISCONNECTING) {
             tdma_disconnect();
         }
     }
     RF_XMIT(tbuffer, tlen);
     TSpkts++;
 }



void showtdma(void) {
    char cbuf[12];
    int volt;
    xprint("TDMA table   MAC      Call   TTL  Lrssi Rrssi  Batt\n");
    for (int i=0;i<TTABSIZE;i++) {
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
    xprint_int(ttab[i].ttl);
    xprint("  ");
    xprint_schar(ttab[i].mrssi);
    xprint("   ");
    xprint_schar(ttab[i].hrssi);
    xprint("    ");
    sprintf(cbuf," %d.%d", (ttab[i].volt*20)/1000,(ttab[i].volt*20)%1000); // convert char * 20 mV to readable voltage
    xprint(cbuf);
    xprint("\n");
    }
}
void showtlist(void) {
//    struct tdmalist *t;
    xprintMAC(&tlist[0].macaddr);
    xprint("\n");
    xprintMAC(&tlist[1]);
    xprint("\n");
    xprintMAC(&tlist[2]);
    xprint("\n");
    xprintMAC(&tlist[3]);
    xprint("\n");
    xprintMAC(&tlist[4]);
    xprint("\n");
}
void showctab(void) {
    int i;
    xprint("CTAB  MAC  TTL\n");
    for(i=0;i<MAXSLAVES;i++) {
        for (int j=0;j<6;j++) {
            xprint_xchar(ctab[i].macaddr[j]);
            if(j<5)
               xprint(":");
            else
               xprint(" ");
        }
        xprint_int(ctab[i].ttl);
        xprint("\n");
    }
}
