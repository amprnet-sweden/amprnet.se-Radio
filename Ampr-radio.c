
/*
 * Copyright (C) 2024 AMPRNet Sweden
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */ 
/* copyright Gullik Webjörn, SM4FBD 
This is the main ampr-radio file, containing most of the amprnet-se Radio functionality.
mailny this contains the command interpreter, and various functions called by the "big loop" */

/* functions for use by ampr-radio */
#include <ti/drivers/UART2.h>
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/NVS.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/Timer.h>


#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

/* */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/drivers/Temperature.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include "ti_drivers_config.h"
#include "Ampr-radio.h"
// #include "heard.h" // obsolete
#ifdef OLED
#else
#include "lcd.h"
#endif
#include "tdma.h"
#include "ethBuf.h"
#include "eth_if.h"
#include "Ampr-queue.h"

#define E5500   //compilation switch

#ifdef E5500
#include "w5500.h"
#endif
// install Button callback and enable interrupts
//    GPIO_setCallback(CONFIG_GPIO_BUTTON0, gpioButton0Fxn);
//    GPIO_enableInt(CONFIG_GPIO_BUTTON0);
void w5500int(uint_least8_t index);


#define BEA_LENGTH 255



// radio parameters
//unsigned int deviation = 350;
//unsigned int bitrate = 0xC0000;
//unsigned int rxBw = 100;
//
int role =0;        // node role, master, slave, autoconfig master (to be defined)
int myslot;          // if this is 1 it is our time to send
int EthEna = 0;
int LcdEna = 1;
int debug = 0;
int tdelay = 20;     // default delay afte tx
int retrena = 1;        // retransmit enabled by default
int Acktime;        // time after last imm segment received
char segbits = 0;
char dsegbits = 0;  // segbits at dropped time
char dropseg = 8;   // segment to drop to test retransmission
int dropctr = 0;
char dseg;  // the first segment we detected as dropped
uint8_t his_S, his_R, my_S, my_R, my_Q;
//#define Ackdel 2  // assume 20 mS acktime
unsigned char Beseg = 0;
unsigned char pktnum = 0;
char beabuf[BEA_LENGTH];
// command line variables
uint8_t m1,m2,m3,m4,m5,m6;
uint8_t my_hwaddr[6];
char rssi = 0x92;       // -110 dBm
char my_call[12] = {"MY0CALL-001\0"};
#ifdef N536RADIO
char version[] ="R2 v2.0";
#else
char version[] ="R1 v2.0";
#endif
//settings
int listener = 0;
#if defined HAM23CMRADIO
unsigned int freq = 1250;
unsigned int freqold = 1250;
#else
unsigned int freq = 868;
unsigned int freqold = 868;
#endif

unsigned char myaddr = 102;
unsigned char peeraddr = 255;
unsigned char mode = 0;
unsigned int rfcollision;
int loopctr = 0;
int retran = 0;
uint8_t wantednum;
int quedepth = 0;
int quemax = 0;

uint8_t pktbuf[1518];
NVS_Handle nvsHandle;
NVS_Params nvsParams;
NVS_Attrs regionAttrs;


unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
unsigned long RRpkts,RSpkts,ERpkts,ESpkts,URpkts,USpkts,TRpkts,TSpkts,ARPreq,ARPans;



bool rebootRequest = false;

void test_epkt(uint8_t * pkt, int cnt);
void sendnack(char dseg); // send a status telling we lost x segments starting w dseg
void doretransmit(void);


char init_ether() {
    bool stat = w5500begin(my_hwaddr);
    if(stat == true)  {
            xprint("Ether initiated\n");
            EthEna = 1;
    } else {
            xprint("No w5500 or wrong revision\n");
            EthEna = 0;
    }
    return EthEna;
}
/* void init_amprradio(void) {
    for(i=0;i<EBCOUNT;i++) {
        ebcount[i] = 0;
    }
} */
    int iidx,oidx;
#define UBCOUNT 8
    uint8_t ubuf[UBCOUNT][300];
    int ucount[UBCOUNT];
    int uartlen(void) {
         if(oidx != iidx) {
             return(ucount[oidx]);
         } else {
             return(0);
         }
     }
    void queue_uart(char * buffer, int count) {
        ubuf[iidx][0] = PTXT;
        memcpy(&ubuf[iidx][1], buffer, count);
        ucount[iidx] = count+1;
        if (++iidx >= 3) {
            iidx = 0;
        }
    }

unsigned char ptxtctr;
void Send_beacon() {
    int mlen;
    uint32_t AONBatMonBatteryVoltageGet();
    uint32_t ui32CurrentBattery;
    ui32CurrentBattery = AONBatMonBatteryVoltageGet();
    ui32CurrentBattery = (ui32CurrentBattery *125 >>5);

    beabuf[0] = (PTXT + ptxtctr);
    sprintf(&beabuf[1],"Seq %d AmprNet.se %s Node %d MAC %02x:%02x:%02x:%02x:%02x:%02x Bat %ld.%03ld Last RSSI %d dBm \r\n",Beseg,version, myaddr,m1,m2,m3,m4,m5,m6,ui32CurrentBattery/1000,ui32CurrentBattery%1000, (signed char) rssi);
    ptxtctr++;
    ptxtctr = ptxtctr & 0x0f;
//    xprint(beabuf);
    mlen = strlen(beabuf);
    if(mlen > 253) mlen = 253;  // do not try to send more than can be received
    SendPacket((uint8_t *)beabuf,mlen);
    Beseg++;
}





//const char * restrict format, ...
void xprint_long(long int a) {
//    int len;
    char outbuf[10];
    sprintf(outbuf,"%ld",a);
    xprint(outbuf);
}
void xprint_int(int a) {
//    int len;
    char outbuf[10];
    sprintf(outbuf,"%d",a);
    xprint(outbuf);
}
void xprint_char(char x) {
    xprint_int((int) x);
}
void xprint_schar(signed char x) {
    char outbuf[6];

    sprintf(outbuf,"%d",x);
    xprint(outbuf);
}
void getMAC(uint8_t* mac) {
    uint64_t macAddrLsb = HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_0);
    uint64_t macAddrMsb = HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_1);
    uint64_t macAddress = (uint64_t)(macAddrMsb << 32) + macAddrLsb;
    m1 = (macAddress>>56) & 255;
    m2 = (macAddress>>48) & 255;
    m3 = (macAddress>>40) & 255;
    m4 = (macAddress>>16) & 255;
    m5 = (macAddress>>8) & 255;
    m6 = (macAddress) & 255;
    uint8_t hwaddr[6];
    hwaddr[0] = (macAddress>>56) & 255;
    hwaddr[1] = (macAddress>>48) & 255;
    hwaddr[2] = (macAddress>>40) & 255;
    hwaddr[3] = (macAddress>> 16) & 255;
    hwaddr[4] = (macAddress>>8) & 255;
    hwaddr[5] = (macAddress) & 255;
    memcpy(mac, hwaddr, sizeof(hwaddr));
}
void printMAC(void) {
    char buff[40];
    getMAC(my_hwaddr);
    sprintf(buff,"MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",m1,m2,m3,m4,m5,m6);
    xprint(buff);
}


void delay(int msecs) {
    Timer1 = msecs;
    while (Timer1) {};
}
void dump_packet(uint8_t *buf,char blen) {
  if(debug & 16) {
    for(int i=0;i<blen;i++) {
        xprint_xchar(buf[i]);
        if(i%64 == 63) {
          xprint("\n");
        } else {
          xprint(" ");
        }
    }
    xprint("\n");
  }
}


#ifdef ETHERNET

char ackbits;
//uint8_t rexmitbuf[chunk+10];

/*
void rexmitseg(void) {
char ackmask;
char reseg;
    ackmask = 1;
    if(ackbits != 0) {
      for(int i = 0;i<6;i++) {
        if((ackbits & ackmask) == 0) {
            reseg = i;
            xprint("Rexmit seg ");
            xprint_char(reseg);
            xprint("\n");
        }
        ackmask = ackmask << 1;
      }
    }
    ackbits = 0;
}
*/
#define chunk 253
uint8_t radio_buff[chunk+10];

/* segment and send an ethernet packet */
void send_epkt(uint8_t *pktbuf, int reclen) {

//    if (EthEna == 1 ) {

      char segnum;


      if( reclen != 0) {
          ERbytes += reclen;
          ERpkts++;
          if(debug & 2) {
            xprint(">>>Eth in :");
            xprint_int(reclen);
            xprint("\n");
            dump_packet(pktbuf,reclen);
          }
          // now
          segnum = 0; //start all radio packets with seg 0
// check packet length
          RX_OFF();
//          send_tdma_packet();       // for now, just send before data
          while (reclen > chunk){
            int src = chunk * segnum;
//         xprint_int(src);
            memcpy(&radio_buff[1], &pktbuf[src], chunk); // make a radio buffer
            radio_buff[0] = segnum | PETH;
            reclen = reclen - chunk;    // update reclen and if zero this is FINAL
            if (reclen == 0) {
              radio_buff[0] |= FINFLAG;
//              segnum = 0;
            }
//            GPIO_write(sigpin,1);
            RF_XMIT(radio_buff, chunk + 1);
//            GPIO_write(sigpin,0);
            RSbytes += chunk;
            RSpkts ++;
            if (debug & 2) {
              xprint("Radio TX IMM seg : ");
              xprint_int(segnum);
              xprint(" size ");
              xprint_int(chunk+1);
              xprint("\n");
              dump_packet(radio_buff,chunk+1);
            } //debug & 2
            segnum++;
          } //while reclen > chunk
          if (reclen > 0) { // #1 if after sending chunks, we still have data
            int src = chunk * segnum;
            radio_buff[0] = segnum | PETH | FINFLAG;
            memcpy(&radio_buff[1] ,&pktbuf[src], reclen);
//            GPIO_write(sigpin,1);
            RF_XMIT(radio_buff, reclen + 1);
//            GPIO_write(sigpin,0);
            RSbytes += reclen;
            RSpkts++;
            if (debug & 2) {
              xprint("Radio TX Fin seg : ");
              xprint_int(segnum);
              xprint(" size ");
              xprint_int(reclen + 1);
              xprint("\n");
              dump_packet(radio_buff,chunk+1);
            } //debug & 2
          } //reclen > 0  #1
/*          ackbuf[0] = 7 | PETH | FINFLAG;
          ackbuf[1] = ACK;
          ackbuf[2] = segbits;
          ackbuf[3] = dseg;
          ackbuf[4] = my_R;
          ackbuf[5] = my_S;
          memcpy(&ackbuf[6],my_hwaddr,6);    //mac id of packet
          memcpy(&ackbuf[12],last_radio,6); // fill in last packet 7 src
          if(debug & 128) {
            xprint("Send Ack my_R : ");
            xprint_char(my_R);
            xprint(" my S ");
            xprint_char(my_S);
            xprint("\n");
          }
//          GPIO_write(sigpin,1);
          RF_XMIT(ackbuf, 21); // send a 21 byte buffer type 7
//          GPIO_write(sigpin,0);
//          RX_ON();
//          GPIO_write(sigpin2,0);
         if(ackbuf[11] != 0x55) {
              xprint(" S bad ");
              xprint_xchar(ackbuf[11]);
              xprint("\n");
          } */
//          Timer_def = tdelay; //set defer timer
      }
      send_tdma_packet();
      TDMASENT = 1;
      RX_ON();
#ifdef TDDEBUG
      GPIO_write(sigpin,0);
#endif
//   } // ethena = 1
}

// queue an ethernet packet. do not bother with overwrite
    void queue_eth(uint8_t *buffer,int count,uint8_t pnum) {

//     quedepth++;
     if(count > 1514) {
         xprint("QUe ptr\n");
         while(1) {}
     }
     // TODO temporary implementation until buffer handles are used in ethproc
     ethBufHandle_t handle;
     ethBuf_get(&handle);
     if(handle.buffer == NULL)
         return; // No free buffer, drop packet
     ebufsused++;
     memcpy(handle.buffer, buffer, count);
     handle.bytesUsed = count;
     handle.packetNumber = pnum;
     // Queue packet (or drop it if the queue is full)
     ampr_queueEth(&handle);
    }

    uint8_t extrabuf[1500];
    uint8_t last_radio[6];

    int dequeue_eth(void) {
        int count;
//        char diff;
        count = 0;
#ifdef TDDEBUG
        GPIO_write(sigpin4,1);
#endif
        if(retran) {               // if peer did not ack my last sent, back up index one packet
//          xprint("ReTX last OK ");
//          xprint_char(his_R);
//          xprint("\n");
            if(ampr_ethQueueEmpty()) {
//             send_eth_frame(uint8_t * buffer, uint8_t * dst, uint16_t type, uint8_t * payload, uint32_t len, int port) {
                xprint("Packet needed\n");
            }
//            diff = my_S - his_R;
/*          if(debug & 32) {
                xprint("Re Diff : ");
                xprint_char(diff);
                xprint("\n");
            } */

//          if (diff >= 5) xprint("Diff >= 5\n");
            if (debug & 32) {
/*                xprint("Ret his_R : ");
                xprint_char(his_R);
                xprint(" ");
                xprint_char(my_S);
                xprint(" ");
                xprint_char(diff);
                xprint(" Ret "); */
                xprint("ReTX : ");
//                xprint_char(bufferHandle.packetNumber); // TODO no valid packet number if queue is empty
                xprint("\n");
            }
            rexmitctr++;
            retran = 0;             // and retran will be done done
        } // if retran
        if(!ampr_ethQueueEmpty()) {              // if queue not empty
#ifdef TDDEBUG
            GPIO_write(sigpin,0);
            GPIO_write(sigpin,1);
#endif
            ethBufHandle_t bufferHandle = ampr_dequeueEth();
            if(bufferHandle.bytesUsed != 0) {     // must be a valid count
                if(bufferHandle.bytesUsed <= 1514) {
                    count = bufferHandle.bytesUsed;

                    my_S = bufferHandle.packetNumber;
                    send_epkt(bufferHandle.buffer,bufferHandle.bytesUsed);
                    count = bufferHandle.bytesUsed;
                } else {
                    xprint("QUE pktlen?\n");
                    while (1);
                }
            }
            ethBuf_free(&bufferHandle);
            ebufsused--;
        } else {
          // we get here if queue empty
/*          if(retran) {
              memcpy(extrabuf, my_hwaddr,6);
              memcpy(&extrabuf[6], last_radio, 6);
              extrabuf[12] = 0x60;
              extrabuf[13] = 0x06;
              count = 16;
              send_epkt(extrabuf,count);
          } */
        }
        return(count);
    }


uint8_t ackbuf[30];

#define ACK 11
#define REX 22


int dbgptr;
/* process tail packet, and check that we agree on sequence, drop is segments dropped on receive */
char rdrop = 0;
char droppkg;

uint8_t xmitbuffer[1514];
int ecount;
int offset = 0;
char expseg = 0;
char dropped = 0;
char dropsent = 0;


void send_ether(unsigned char * buffer, char length) {  /* reassemble radio packets into an ethernet frame, and if ok send it */
    int count = length;
    char seg;
    RRpkts++;
    RRbytes += (count-1);
    seg = buffer[0] & 7;
    if(debug & 1) {
      xprint("Radio rx ");
      if(buffer[0] & FINFLAG) {
          xprint("Fin seg :");
      } else
          xprint("Imm seg :");
      xprint_char(seg);
      xprint(" size ");
      xprint_int(count);
      xprint(" RR :");
      xprint_int(RRpkts);
      xprint(" Recd ");
      xprint_int(Recd - TRpkts);
      xprint("\n");
    }
    /* if we got a 0 segment set up segbits, and clear dropped */
    if (seg == 0) {
        dropped = 0;
    } // seg == 0
    // just debugging option
//    if (seg == dropseg) {
//            dropseg = 8;   //only drop once so  set it above segnr
//            return; // just a test to see retrans works
//            dropped = 1;
//    }
    if (seg < 6) {  // normal Imm or Fin segment
      if ((seg != expseg)) {
        if (dropped == 0) {
            dseg = expseg; // catch the 1st dropped segment
//            xprint("Exp ");
//            xprint_char(expseg);
//            xprint("\n");
        }
//        xprint("d\n");
//        dropped = 1;
        if(expseg < 5)
           expseg++;  // assume only one dropped but only for immediate segments
      }

      segbits = segbits | (1<<seg);  // for all data packets set corr bit in segbits
      // see where segment should be plac
      offset = seg * chunk;
// just a debug printout
      if((debug & 1) != 0) {
          xprint_xchar(seg);
          if (buffer[0] & FINFLAG) {
              xprint(" FIN ");
          } else {
              xprint(" Imm ");
          }
          xprint(" Offset : ");
          xprint_int(offset);
          xprint("\n");
      }
        // this copies the radio buffer into the ethernet packet , buffer[0] = ptype/seg, data begins in buffer[1]
      if ((buffer[0] & FINFLAG) == 0) {  //this is imm seg
            memcpy(&xmitbuffer[offset], &buffer[1], chunk); //copy data but not seg
            if (expseg < 5)
                   expseg++;
      } else {                        // this is final seg
            memcpy(&xmitbuffer[offset], &buffer[1], (count - 1));
              expseg = 0; //
      }
      if(dropped !=0) xprint("D\n");
    // we have now processed the whole ethernet packet
      ecount = offset + count - 1; //offset should be computed from previous packet
      if(dropped == 0) {
        if (buffer[0] & FINFLAG) {
          if ((debug & 0x1) != 0) {
            xprint("Eth out : ");
            xprint_int(ecount);
            xprint(" segbits : ");
            xprint_xchar(segbits);
            xprint("\n\n");
          }
          if (memcmp (xmitbuffer, my_hwaddr,6) == 0) { // was packet for me??
              proc_eth(xmitbuffer, ecount, PORT_RADIO); // mark packet came from radio
          } else {
              if(memcmp(xmitbuffer,bcaddr,6) == 0) { // have we assembled a broadcast message?
                 memcpy(extrabuf,xmitbuffer,ecount); // copy broadcast contents
                 proc_eth(extrabuf, ecount, PORT_RADIO);       // it might be for us to act on
              }
              if(EthEna) {
                ethIf_send(xmitbuffer,ecount);
              }
#ifdef TDDEBUG
              GPIO_write(sigpin,0);
#endif
              test_epkt(xmitbuffer,ecount);
          }
          ESbytes += ecount;
          ESpkts++;
          dump_packet(xmitbuffer,ecount);
        } // this was a final packet
       } else {      //dropped was 1 or more
          dropctr++;
       }     // test if dropped == 0
       dropped = 0;
       expseg = 0;
       ecount = 0;
       segbits = 0;
    } else {    // we get here on only seg 6 & 7
/*
 * we do not know if this is an ACK packet or a REX packet or a type 6 packet
 */
      if(seg == 7) {
//        proc_type7(buffer, count, dropped);  GW to compile
        if(dropped != 0) {   // if send nack requested
          if(retrena)  { //if retransmit enabled

/*              if(debug & 32) {
                xprint("Rcv Drop ");
                xprint_char(his_S);
                xprint(" ");
                xprint_xchar(segbits);
                xprint(" ");
                xprint_char(dseg);
                xprint("\n");
              } */
//              sendnack(my_R);   // it should be OK to send a REX now, we are receiving...
          }
        } else { //no drop detected
            if(ecount != 0) {
//              Timer1 = tdelay;
//              GPIO_write(sigpin2,0);
// see if the frame is for us
                if (memcmp (xmitbuffer, my_hwaddr,6) == 0) { // was packet for me??
                    proc_eth(xmitbuffer, ecount,2); // mark packet came from radio
                } else {
                    if(memcmp(xmitbuffer,bcaddr,6) == 0) { // have we assembled a broadcast message?
                       memcpy(extrabuf,xmitbuffer,ecount); // copy broadcast contents
                       proc_eth(extrabuf, ecount,2);       // it might be for us to act on
                    }
                    if(EthEna) {
                      ethIf_send(xmitbuffer,ecount);
                    }
                    test_epkt(xmitbuffer,ecount);
                    ecount = 0;
                }
            }
        }
        dropped = 0;
        expseg = 0;   // and begin from 0 again
        segbits = 0;
      } // was seg 7
  }
}
#endif  // ethernet

void whatpacket(uint8_t * buffer, char length) {
    uint8_t pktype;
//    Timer1 = 20;        // mark recv in progress, do not interfere
    pktype = buffer[0] & 0xf0;
    switch(pktype) {
    case PETH:
#ifdef TDDEBUG
        GPIO_write(sigpin3,1);
#endif
        send_ether(buffer,length);
#ifdef TDDEBUG
        GPIO_write(sigpin3,0);
#endif
        break;
    case PTXT:  // this is a "uart" packet, send it out
//        SendText(&buffer[1], length - 1);
        URpkts++;
        URbytes += length -1;
        break;
    case PPTP:
        xprint("PTP pktype\n");
        break;
    case PXXX:
        xprint("XXX pktype\n");
        break;
    case PTDMA: {
#ifdef TDDEBUG
//        GPIO_write(sigpin5,1);
#endif
        proc_tdma_packet(buffer,length);
#ifdef TDDEBUG
//        GPIO_write(sigpin5,0);
//        dolog("TR  \r\n", 6, 0);
#endif
        break; }
    default:
//        xprint("Bad pktype : ");
//        xprint_char(pktype);
//        xprint("\n");
        break;
    }
}
