/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include <string.h>
//#include <stdlib.h>
//#include <stdint.h>
#include <stddef.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include "Ampr-radio.h"
#include "w5500.h"
/* the ethernet packet queue */
#define chunk 253
//#define EBCOUNT 30
/* indicators for queue depth */

int quedepth = 0;
int quemax = 0;

//input pointers
int eiidx = 0;
int eoidx = 0;

//output pointers
int eoiidx = 0;
int eooidx = 0;

/* the ethernet input packet queue */
uint8_t ebufs[EBCOUNT][1514];
int ebcount[EBCOUNT];
uint8_t ebnumber[EBCOUNT];

/* the ethernet output packet queue */
uint8_t eobufs[EOBCOUNT][1514];
int eobcount[EOBCOUNT];
uint8_t eobnumber[EOBCOUNT];

int retran = 0;
int rexmitctr = 0;


uint8_t his_S, his_R, my_S, my_R, my_Q;

uint8_t pktbuf[1518];
char segbits = 0;
char dseg;  // the first segment we detected as dropped

//#include <stdio.h>

uint8_t ackbuf[30];

#define ACK 11
#define REX 22

//SemaphoreHandle_t xSemaphore = NULL;


void send_epkt(uint8_t *pktbuf, int reclen) {
    if (EthEna == 1 ) {

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
//            dump_packet(radio_buff, chunk+1);
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
          } //while
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
          ackbuf[0] = 7 | PETH | FINFLAG;
          ackbuf[1] = ACK;
          ackbuf[2] = segbits;
          ackbuf[3] = dseg;
          ackbuf[4] = my_R;
          ackbuf[5] = my_S;
          memcpy(&ackbuf[6],my_hwaddr,6);    //mac id of packet
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
          RX_ON();
/*          if(ackbuf[11] != 0x55) {
              xprint(" S bad ");
              xprint_xchar(ackbuf[11]);
              xprint("\n");
          } */
          Timer1 = tdelay; //set defer timer
      }
   } // ethena = 1
}

// queue an ethernet packet on input . do not bother with overwrite
    void queue_eth(uint8_t *buffer,int count,uint8_t pnum) {
     quedepth++;
     if(count > 1514) {
         xprint("QUe ptr\n");
         while(1) {}
     }
     memcpy(&ebufs[eiidx][0], buffer, count);
     ebcount[eiidx] = count;
     ebnumber[eiidx] = pnum;
     if(++eiidx >= EBCOUNT) {
        eiidx = 0;
     }
}
    // queue an ethernet packet for output . do not bother with overwrite
        void queue_eth_out(uint8_t *buffer,int count,uint8_t pnum) {
//         quedepth++;
         if(count > 1514) {
             xprint("QUe ptr\n");
             while(1) {}
         }
         memcpy(&eobufs[eoiidx][0], buffer, count);
         eobcount[eoiidx] = count;
         eobnumber[eoiidx] = pnum;
         if(++eoiidx >= EOBCOUNT) {
            eoiidx = 0;
         }
    }
//  dequeue an ethernet packet for output
    int dequeue_eth_out(void) {
        int count;
        count = 0;
        if(eooidx != eoiidx) {              // if queue not empty
            if(eobcount[eooidx] != 0) {     // must be a valid count
                if(eobcount[eooidx] <= 1514) {
                    w5500sendFrame(&eobufs[eooidx][0],eobcount[eooidx]);
//                  send_epkt(&ebufs[eoidx][0], ebcount[eoidx]);
                  count = eobcount[eooidx];
                } else {
                    xprint("QUe count\n");
                    while (1);
                }
                if(++eooidx >= EOBCOUNT) {
                  eooidx = 0;
                }
            }
        }
        return(count);
    }
    //  dequeue an ethernet packet for input
    int dequeue_eth(void) {
      int count;
      char diff;
      if (quedepth > quemax) quemax = quedepth;
      count = 0;
      if(retran) {               // if peer did not ack my last sent, back up index one packet
          diff = my_S - his_R;
          if (diff == 1) {
            if(eoidx == 0) {      // if index will wrap
              eoidx = EBCOUNT-1;  // set it to biggest
            } else {
              eoidx--;          // just set to previous
            }
            quedepth++;         // adjust que depth, cause we add a apcket
          }
          if (debug & 256 ) {

            xprint("R ");
            xprint_char(diff);
//            xprint(" ");
//            xprint_char(his_R);
            xprint("\n");
          }
        rexmitctr++;
        retran = 0;             // and retran is done
      }
      if(eoidx != eiidx) {              // if queue not empty
          if(ebcount[eoidx] != 0) {     // must be a valid count
              if(ebcount[eoidx] <= 1514) {
                my_S = ebnumber[eoidx];
//                dump_packet(&ebufs[eoidx][0],ebcount[eoidx]);
                send_epkt(&ebufs[eoidx][0], ebcount[eoidx]);
                count = ebcount[eoidx];
              } else {
                  xprint("QUe count\n");
                  while (1);
              }
              if(++eoidx >= EBCOUNT) {
                eoidx = 0;
              }
          }
      }
      if(count != 0) quedepth--;
//      if((count != 0) && (diff = 0)) quedepth--;
      return(count);
    }

/*    int iidx,oidx;
     uint8_t ubuf[4][300];
     int ucount[4];
     void queue_uart(char * buffer, int count) {
         ubuf[iidx][0] = PTXT;
         memcpy(&ubuf[iidx][1], buffer, count);
         ucount[iidx] = count+1;
         if (++iidx >= 3) {
             iidx = 0;
         }
     } */

void dequeue_uart(void) {
        if(oidx != iidx) {
//          delay(20);
//            my_S++;
//            Uartbuf[0] = PTXT;
            USpkts++;
            USbytes += (ucount[oidx]-1); // size here includes pktype
            SendPacket(&ubuf[oidx][0],ucount[oidx]);
//            dump_packet(&ubuf[oidx],ucount[oidx]);
/*            xprint("Dequeue : ");
            xprint_int(ucount[oidx]);
            xprint("\n"); */
            if(++oidx >= 3) {
                oidx = 0;
            }
        }
}


void check_ethernet() {
   int reclen;
//   GPIO_toggle(sigpin); // just to check loop time

   if (EthEna == 1) {
   // first do any requested retransmissions
   // retran should be 1

     do {
//         GPIO_write(sigpin,1);
//         if( xSemaphoreTake( xSemaphore, ( TickType_t ) 1000 ) == pdTRUE ) {
//         GPIO_write(sigpin,1);
             reclen = w5500readFrame(pktbuf, sizeof(pktbuf));
//             GPIO_write(sigpin,0);
//             xSemaphoreGive( xSemaphore );
//         }
//         GPIO_write(sigpin,0);

         if(reclen != 0) {
             my_Q++;
             queue_eth(pktbuf,reclen,my_Q);
             test_epkt(pktbuf,reclen);
//             dump_packet(pktbuf,reclen);
         }
     } while (reclen != 0);
//     dequeue_eth_out();
   } // ethena
} // check_ethernet

void proc_type7(uint8_t *buffer, int len, char drop) {
    his_R = buffer[4];
    his_S = buffer[5];
    if(drop == 0)
        my_R = his_S;
/*    if(dbgptr++ < 100) {
        memcpy(&debbuf[dbgptr][0],buffer,20);
    } */
    setheard(&buffer[6]);
    if(buffer[1] == ACK) {  // an ack and something with rnum vs snum

//      my_R = his_S;     // resynchronize
      if(debug & 64) {
//        xprint("\n");
        xprint("ACK rcv his R ");
        xprint_char(his_R);
        xprint(" my S ");
        xprint_char(my_S);
        xprint(" my R ");
        xprint_char(my_R);
        xprint(" his S ");
        xprint_char(his_S);
        xprint(" len ");
        xprint_int(len);
        for(int i = 6;i<12;i++) {
            xprint(" ");
            xprint_xchar(buffer[i]);
        }
        xprint("\n");
      }
      if(his_R != my_S ) { // if his received is NOT my last sent
//          if(my_S - his_R == 1) {
          if (retrena) retran = 1;
/*          xprint("Re: ");
          xprint_char(his_R);
          xprint(" ");
          xprint_char(my_S);
          xprint("\n"); */
//          }
      }

    } // ACK end
/*
 * if we got a REX request find saved packet and send again
 */
    if(buffer[1] == REX) {
//        retran = 1;
//        wantednum = his_R;
        if( debug & 8 ) {
          xprint("REX Rcv his R ");
          xprint_char(his_R);
          xprint(" his_S ");
          xprint_char(his_S);
          xprint("\n");
        }
    }
}





uint8_t radio_buff[chunk+10];
uint8_t xmitbuffer[1514];
int ecount;
int offset = 0;
char expseg = 0;
char dropped = 0;
char dropsent = 0;
char dropseg = 8;   // segment to drop to test retransmission


TaskHandle_t thisprog;

uint8_t segbuf[255];
char seglen = 0;
void send_ether(unsigned char * buffer, char length) {  /* reassemble radio packets into an ethernet frame, and if ok send it */
    int count = length;
    char seg;
    RRpkts++;
    RRbytes += count;
    seg = buffer[0] & 7;
    if(debug & 1) {
      xprint("Rin : ");
      xprint_char(seg);
      xprint(" len ");
      xprint_int(count);
      xprint("\n");
    }
// just debugging option
    if (seg == dropseg) {
        dropseg = 16;   //only drop once
        return; // just a test to see retrans works
    }
    /* if we got a 0 segment set up segbits, and clear dropped */
    if (seg == 0) {
//        segbits = 1;  // mark we got 0
        dropped = 0;
    } // seg == 0
    if (seg < 6) {  // normal Imm or Fin segment
      if ((seg != expseg)) {
        if (dropped == 0) {
            dseg = expseg; // catch the 1st dropped segment
        }
        dropped = 1;
        if(expseg < 5)
           expseg++;  // assume only one dropped but only for immediate segments
    }

    segbits = segbits | (1<<seg);  // for all data packets set corr bit in segbits
      // see where segment should be plac
    offset = seg * chunk;
// just a debug printout
      if((debug & 0) != 0) {
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
            expseg = 7; // this was last
//            segbits = 0;
    }
    ecount = offset + count - 1; //offset should be computed from previous packet
      if(dropped == 0) {
        if (buffer[0] & FINFLAG) {
          ESbytes += ecount;
          ESpkts++;
          dump_packet(xmitbuffer,ecount);
          expseg = 7;
        } // this was a final packet
      } else {      //dropped was 1 or more
        dropctr++;
        if(debug & 4) {
          xprint("Drop ");
          xprint_char(dseg);
          xprint("\n");
        }
      }
  } else {    // we get here on only seg 6 & 7
/*
 * we do not know if this is an ACK packet or a REX packet or a type 6 packet
 */
      if(seg == 7) {
        proc_type7(buffer, count, dropped);
        if(dropped != 0) {   // if send nack requested
          if(retrena)  { //f retransmit enabled
 /*             xprint("D ");
              xprint_char(his_S);
              xprint(" ");
              xprint_xchar(segbits);
              xprint(" ");
              xprint_char(dseg);
              xprint("\n"); */
//              sendnack(my_R);   // it should be OK to send a REX now, we are receiving...
          }
        } else { //no drop detected
            if(ecount != 0) {
//              Timer1 = tdelay;
//              GPIO_write(sigpin2,0);
//              GPIO_write(sigpin,1);
//              if( xSemaphoreTake( xSemaphore, ( TickType_t ) 1000 ) == pdTRUE ) {
//                    w5500sendFrame(xmitbuffer,ecount);
                    queue_eth_out(xmitbuffer,ecount,0);
                    xTaskNotify(thisprog,1,eSetBits);
                    if ((debug & 0x1) != 0) {
                      xprint("Eth out : ");
                      xprint_int(ecount);
                      xprint(" segbits : ");
                      xprint_xchar(segbits);
                      xprint("\n");
                    }
//                    GPIO_write(sigpin,0);
    //              test_epkt(xmitbuffer,ecount);
                    ecount = 0;
//                    xSemaphoreGive( xSemaphore );
//              }
            }

          } // no drop detected

          dropped = 0;
          expseg = 0;   // and begin from 0 again
          segbits = 0;
      }
  }
}

char init_ether() {
    my_hwaddr[0] =8;
    my_hwaddr[5] = 7;
    bool stat = w5500begin(my_hwaddr);
//            if((stat == true) && (link == true)) {
    if(stat == true)  {
            xprint("Ether initiated\n");
            EthEna = 1;
    } else {
            xprint("No w5500 or wrong revision\n");
            EthEna = 0;
    }
    return EthEna;
}


void w5500int(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//    GPIO_toggle(sigpin2);
//    GPIO_write(sigpin,1);
//    vTaskNotifyGiveFromISR(thisprog, &xHigherPriorityTaskWoken);
    xTaskNotifyFromISR(thisprog,2,eSetBits,&xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Eth(void) {
    thisprog = xTaskGetCurrentTaskHandle();
    TaskStatus_t ethtask;
    uint8_t *op;
    uint32_t whoactivated;
    int olen;
    vTaskGetInfo(NULL, &ethtask, pdFALSE, eInvalid);    //get tsackspace set to false
    op = ethtask.pcTaskName;
    memcpy(op,"Eth\0",4);
    GPIO_setCallback(EINT, &w5500int);
    GPIO_enableInt(EINT);
//    int lcount = 0;
    init_ether();
//    xSemaphore = xSemaphoreCreateMutex();
  while(1) {
//    vTaskDelay(pdMS_TO_TICKS(1));
//      xTaskNotifyWait(0,0,NULL,0);
    whoactivated = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//    GPIO_write(sigpin,0);
    if(debug & 32) {
       xprint("Eth ");
       xprint_int(whoactivated);
       xprint("\n");
    }
//     if(++lcount == 100) {
//         lcount = 0;
//         GPIO_toggle(sigpin2); // just to check loop time
//     }
/*     xprint("W ");
     xprint_int(whoactivated);
     xprint("\n"); */
    if (whoactivated & 2) {
     check_ethernet();
    }
    if (whoactivated & 1) {
     do {
       olen = dequeue_eth_out();
     }      while (olen != 0);
    }
  }
}
