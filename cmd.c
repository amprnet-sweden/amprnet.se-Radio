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


#include <ti/drivers/Temperature.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include "ti_drivers_config.h"
#include "Ampr-radio.h"
#include "w5500.h"
//#include "heard.h"

unsigned char Beseg = 0;
unsigned char pktnum = 0;
#define BEA_LENGTH 255
#define Nsize 100

int EthEna = 0;
int LcdEna = 1;
int debug = 0;
int tdelay = 800;
int retrena = 0;

// #define PUTCHAR_PROTOTYPE int U


UART2_Handle uart1;
UART2_Params uartParams1;

//int Txcount;      // the number of test packets

/* beacon variables */
int Becount = 0;      // beacon counter
char beabuf[BEA_LENGTH];


/* the command line and its variables */
char cc;
static char cmdline[100];    // Nsize??
int cmdptr = 0;             // pointer in string
int cmdbytes = 0;              // nr of bytes accumulated

int dropctr = 0;
int rexrqctr =0;

NVS_Handle nvsHandle;
NVS_Params nvsParams;
NVS_Attrs regionAttrs;




/* all packet counters */

unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
unsigned long RRpkts,RSpkts,ERpkts,ESpkts,URpkts,USpkts;

/**/
uint8_t m1,m2,m3,m4,m5,m6;
uint8_t my_hwaddr[6];
char my_call[16] = {"MY0CALL 001-----"};


char rssi = 0x92;       // -110 dBm
char version[] ="X 2.0c";
#if defined HAM23CMRADIO
unsigned int freq = 1260;
unsigned int freqold = 1260;
#else
unsigned int freq = 868;
unsigned int freqold = 868;
#endif


unsigned char myaddr = 102;
unsigned char peeraddr = 255;
unsigned char mode = 0;
unsigned int rfcollision;
int loopctr = 0;

/* indicators for queue depth */


void ReceiveonUARTcallbac1(UART2_Handle uart1, void *buffer, size_t count, void *userArg, int_fast16_t status);


void xprint(char *buf) {
    int cnt = strlen(buf);
    UART2_write(uart1,buf,cnt,NULL);
    for(int i=0;i<cnt+1;i++) {
        if(buf[i] == 0x0a) {
            const char lfb[] = "\r";
            UART2_write(uart1,lfb,1,NULL);
        }
    }

}

void xprint_xchar(char x) {
    char outbuf[6];
    sprintf(outbuf,"%02x",x);
    xprint(outbuf);
}

void xprint_long(long int a) {
    char outbuf[10];
    sprintf(outbuf,"%ld",a);
    xprint(outbuf);
}

void xprint_int(int a) {
//    int len;
    char outbuf[16];
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

void printMAC(void) {
    char buff[40];
    uint64_t macAddrLsb = HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_0);
    uint64_t macAddrMsb = HWREG(FCFG1_BASE + FCFG1_O_MAC_15_4_1);
    uint64_t macAddress = (uint64_t)(macAddrMsb << 32) + macAddrLsb;
    m1 = (macAddress>>56) & 255;
    m2 = (macAddress>>48) & 255;
    m3 = (macAddress>>40) & 255;
    m4 = (macAddress>>16) & 255;
    m5 = (macAddress>>8) & 255;
    m6 = (macAddress) & 255;
    my_hwaddr[0] = (macAddress>>56) & 255;
    my_hwaddr[1] = (macAddress>>48) & 255;
    my_hwaddr[2] = (macAddress>>40) & 255;
    my_hwaddr[3] = (macAddress>> 16) & 255;
    my_hwaddr[4] = (macAddress>>8) & 255;
    my_hwaddr[5] = (macAddress) & 255;
    sprintf(buff,"MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",m1,m2,m3,m4,m5,m6);
    xprint(buff);
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

void get_NVS(uint8_t *buff) {
        uint8_t chk;
        uint16_t status;
        nvsHandle = NVS_open(CONFIG_INTERNAL, &nvsParams);
        NVS_getAttrs(nvsHandle, &regionAttrs);
        // Copy "Hello" from nvsRegion into local 'buf'
        status = NVS_read(nvsHandle, 0, buff, Nsize);
        if (status != NVS_STATUS_SUCCESS) {
            xprint("Could not read nvs\n");
        }
        chk = 0;
        for(int i=0;i<Nsize; i++){
            chk = chk + buff[i];
        }
/*        xprint("Stored chk ");
        xprint_xchar(buff[Nsize-1]);
        xprint(" calc ");
        xprint_xchar(chk);
        xprint("\n"); */
        if(chk != 6) {
                xprint("NVR checksum error\n");
                myaddr = m6;    // set myaddr to last byte of MAC if not saved
//                    EthStart = 0;
        } else {
               xprint("Restoring saved config\n");
                freq = buff[0] + (buff[1] * 256);
                myaddr = buff[4];
                peeraddr = buff[5];
                mode = buff[6];
                EthEna = buff[7] & 1;
        }
        xprint("Stored Frequency ");
        xprint_int(freq);
        xprint("\n");
        xprint("Ethernet ");
        xprint_char(EthEna);
        xprint("\n");
        NVS_close(nvsHandle);
}
/*
void send_log(char *buff) {
    int i;
    uint16_t chk = 0;
    char msgbuf[80];
    int a = strlen(buff);
    for(i=0;i<6;i++) {
            msgbuf[i] = 0xff;
    }
    for(i=0;i<6;i++) {
        msgbuf[i+6] = my_hwaddr[i];
    }
    msgbuf[12] = 0x08;
    msgbuf[13] = 0x00;
    memcpy(&msgbuf[16],buff,a);  //copy the string
    a = a + 16; // a is now total length
    msgbuf[14] = a & 0xff;
    msgbuf[15]= (a>>8) & 0xff;
//    checksum udp
//    fill in data
//    set length
} */

void parse_cmd(char *cline, int cnt) {
    int argc;
    int val;
//    int status;
    uint32_t AONBatMonBatteryVoltageGet();
    uint32_t ui32CurrentBattery;
    char* argv[10];             // maximum number ot args
    const char s[4] = " -\r\n";
    char* tok;
    char showbuf[60];

    /* parse the command line into argv argc */
    argc = 0;
    tok = strtok(cline, s);
    while (tok != 0) {
//        argc++;
        argv[argc++]=tok;
//        xprint(tok);
        tok = strtok(0,s);  // continue from last pos
    }
    /* now, check for specific commands */
    if (strcmp(argv[0],("help")) == 0) {
        xprint("List of commands\r\n"
                "\n"
        "help "
       "frequency "
        "myaddr  "
        "peeraddr "
         "show "
         "rssi "
        "mode "
        "beacon "
        "save "
        "count \r\n");
    } else if (strcmp(argv[0], ("frequency")) == 0) {
        xprint("F in Megaherz  ");
        xprint_int(freq);
        if (argc == 2) {
            val=atoi(argv[1]);
#ifdef HAM23CMRADIO
            if ((val < 1241) || (val > 1299)) {
#else
            if ((val < 862) || (val > 876)) {
#endif
            xprint(" Illegal value  ");
            xprint_int(val);
//            xprint("\n");
            xprint(" F in Megahertz  ");
          } else {
            freq = val;
            xprint(" Setting F to ");
            xprint_int(freq);
          }
        }
        xprint("\n");
        } else if (strcmp(cline, ("myaddr")) == 0) {
            xprint("My addr : ");
            xprint_char(myaddr);
            if (argc == 2) {
                val=atoi(argv[1]);
                if ((val < 1) || (val > 254)) {
                  xprint(" Illegal value  ");
                  xprint_int(val);
    //            xprint("\n");
                } else {
                  myaddr = val;
                  xprint(" Setting myaddr to ");
                  xprint_char(myaddr);
                }
    //            xprint("\n");
            }
            xprint("\n");
        } else if (strcmp(cline, ("peeraddr")) == 0) {
            xprint("Peer addr : ");
            xprint_char(peeraddr);
            if (argc == 2) {
                val=atoi(argv[1]);
                if ((val < 1) || (val > 254)) {
                  xprint(" Illegal value  ");
                  xprint_int(val);
    //            xprint("\n");
                } else {
                  peeraddr = val;
                  xprint(" Setting peeraddr to ");
                  xprint_char(peeraddr);
                }
    //            xprint("\n");
            }
            xprint("\n");
        } else if (strcmp(cline, ("mode")) == 0) {
            xprint("Mode \n");
             xprint_char(mode);
             xprint("\n");
        } else if (strcmp(cline, ("rssi")) == 0) {
            xprint("Rssi : ");
             xprint_schar(rssi);
             xprint(" dBm ");
             xprint(" idle rssi : ");
           xprint_schar(GetRssi());
              xprint(" dBm\n");
        } else if (strcmp(cline, ("show")) == 0) {
            xprint("AmprNet.se-radio ");
            xprint(version);
            xprint(" Status\n");
            xprint("\n");
            xprint("Frequency : ");
            xprint_int(freq);
            xprint("\n");
            xprint("Myaddr :  ");
            xprint_char(myaddr);
            xprint("\n");
            xprint("Peeraddr :  ");
            xprint_char(peeraddr);
            xprint("\n");
            ui32CurrentBattery = AONBatMonBatteryVoltageGet();
            ui32CurrentBattery = (ui32CurrentBattery *125 >>5);
            sprintf(showbuf,"Temp : %d C Batt : %ld.%03ld V Secs : %d \r\n",Temperature_getTemperature(),ui32CurrentBattery/1000,ui32CurrentBattery%1000, Runtime/100000);
            xprint(showbuf);
            printMAC();
            xprint("Sent     Recvd     Bad      Loopctr    Runtime\n");
            xprint_int(Sent);
            xprint("        ");
            xprint_int(Recd);
            xprint("        ");
            xprint_int(Bad);
            xprint("        ");
            xprint_int(loopctr);
            xprint("        ");
            xprint_int(Runtime/100000);
            xprint("\n");
        } else if (strcmp(cline, ("save")) == 0) {
            char buf[Nsize];
            char csum;
            uint16_t status;
            /* open a handle to nvs */
            nvsHandle = NVS_open(CONFIG_INTERNAL, &nvsParams);
            if (nvsHandle == NULL) {
                // Error handling code
                xprint("Could not open NVRAM\n");
            }
            NVS_getAttrs(nvsHandle, &regionAttrs);
            status = NVS_erase(nvsHandle, 0, regionAttrs.sectorSize);
            if (status != NVS_STATUS_SUCCESS) {
                // Error handling code
            }

            /* fill in the data to save */
            buf[0] = freq & 255;
            buf[1] = (freq >> 8) & 255;
            buf[4] = myaddr;
            buf[5] = peeraddr;
            buf[6] = mode;
            buf[7] = EthEna & 1;
            /* compute a checksum */
            csum = 0;
            for (int i=0; i<Nsize-1;i++) {
                csum = csum + buf[i];
            }
            buf[Nsize-1] = (~csum) + 7;     // add a constant to checksum
            /* write back the buffer */
            NVS_write(nvsHandle,0,buf,Nsize,NVS_WRITE_POST_VERIFY | NVS_WRITE_ERASE);
            NVS_close(nvsHandle);
            xprint("Config saved\n");
        } else if (strcmp(cline, ("tdel")) == 0) {
            if(argc == 2) {
              tdelay = atoi(argv[1]);
            }
            xprint("Tdelay = ");
            xprint_int(tdelay);
            xprint("\n");
        } else if (strcmp(cline, ("re")) == 0) {
            if(argc == 2) {
              retrena = atoi(argv[1]);
            }
            xprint("Retransmit = ");
            xprint_int(retrena);
            xprint("\n");
        } else if (strcmp(cline, ("beacon")) == 0) {
            Timer0 = 60000;
            if(argc == 2) {
                Becount = atoi(argv[1]);
                xprint("Starting beacon\n");
            } else {
                Becount = 0;
                xprint("Stopping beacon\n");
            }
        } else if (strcmp(cline, ("zero")) == 0) {
            ESpkts = 0;
            ERpkts = 0;
            ESbytes = 0;
            ERbytes = 0;
            RSpkts = 0;
            RRpkts = 0;
            RSbytes = 0;
            RRbytes = 0;
            USpkts = 0;
            URpkts = 0;
            USbytes = 0;
            URbytes = 0;
            dropctr = 0;
            rexmitctr =0;
            rexrqctr = 0;
            quedepth= 0;
            quemax = 0;
        } else if (strcmp(cline, ("debug")) == 0) {
            if (argc == 2) {
              debug = atoi(argv[1]);
            } else {
                xprint("debug = ");
                xprint_int(debug);
                xprint("\n");
            }
#ifdef ETHERNET
    } else if (strcmp(cline, ("drop")) == 0) {
        if (argc == 2) {
          dropseg = atoi(argv[1]);
        } else {
            xprint("drop = ");
            xprint_int(dropseg);
            xprint("\n");
        }
    } else if (strcmp(cline, ("ether")) == 0) {
        if(argc == 2) {
           if (atoi(argv[1]) == 0) {
               w5500end();
               EthEna = 0;
               xprint("Ether disabled\n");
           }
        } else {    // only one arg
        if(EthEna == 0) {
//            uint8_t tmpmac[] = {2,4,6,8,10,12};
//            uint8_t buffer[1518];
/*            for(int i =0 ; i<6; i++) {
                xprint_xchar(w55mac[i]);
                xprint(" ");
            }
            xprint("\n"); */
//            bool link = wizphy_getphylink();
//            bool stat = w5500begin(my_hwaddr);
            xprint("Ether disabled\n");
//            if((stat == true) && (link == true)) {
/*            if(stat == true)  {
              xprint("Ether initiated\n");
              EthEna = 1;
            } else {
                xprint("w5500 or wrong revision\n");
//                EthEna = 2;
            } */

        } else
            xprint("Already enabled or broken\n");
        }
    } else if (strcmp(cline, ("link")) == 0) {
//      char phy = getPHYCFGR();
        char phy = 17;
//        xprint("not impl");
        xprint("Link status ");
        if(phy & 1) {
            xprint("UP ");
        } else {
            xprint("DOWN ");
        }
        if(phy & 2) {
            xprint("100 M ");
        } else {
            xprint("10 M ");
        }
        if(phy & 4) {
            xprint("FDX ");
        } else {
            xprint("HDX ");
        }
/*        if(phy & 1) {
            xprint("UP ");
        } else {
            xprint("DOWN ");
        } */
        xprint("\n");
        xprint_xchar(phy);
        xprint("\n");

#endif
#ifdef LCD
    } else if (strcmp(cline, ("lcd")) == 0) {
        char blocks[] = "==========>>";
        if(LcdEna == 0) {
        LCD_Print(&version);
        delay(1);
        LCD_Goto(1,2);
        delay(1);
        LCD_Print(&version);
        delay(1);
        LCD_Goto(1,3);
        delay(1);
        LCD_Print(&version);
        LCD_Goto(1,4);
        delay(1);
        LCD_Print(&blocks);
         } else {
            xprint("LCD not present\n");
        }

#endif
    } else if (strcmp(cline, ("count")) == 0) {
        xprint("Ethernet sent                Ethernet Received\n");
        xprint("Pkts    ");
        xprint_long(ESpkts);
        xprint("                      ");
        xprint_long(ERpkts);
        xprint("\n");
        xprint("Bytes   ");
        xprint_long(ESbytes);
        xprint("                      ");
        xprint_long(ERbytes);
        xprint("\n");
        xprint("Radio sent                Radio Received\n");
        xprint("Pkts    ");
        xprint_long(RSpkts);
        xprint("                      ");
        xprint_long(RRpkts);
        xprint("\n");
        xprint("Bytes   ");
        xprint_long(RSbytes);
        xprint("                      ");
        xprint_long(RRbytes);
        xprint("\n");
        xprint("Uart sent                Uart Received\n");
        xprint("Pkts    ");
        xprint_long(USpkts);
        xprint("                      ");
        xprint_long(URpkts);
        xprint("\n");
        xprint("Bytes   ");
        xprint_long(USbytes);
        xprint("                      ");
        xprint_long(URbytes);
        xprint("\n");
        xprint("Packets dropped due to segments missing ");
        xprint_int(dropctr);
        xprint("\n");
        xprint("Rexmit requests ");
        xprint_int(rexrqctr);
        xprint("\n");
        xprint("Packets retransmitted ");
        xprint_int(rexmitctr);
        xprint("\n");
        xprint("Que depth ");
        xprint_int(quedepth);
        xprint("\n");
        xprint("Que max ");
        xprint_int(quemax);
        xprint("\n");
/*        xprint("Eth ints ");
        xprint_int(eints);
        xprint("\n"); */
/*    } else if (strcmp(cline, ("ack")) == 0) {
        for (int i = 0; i<100;i++) {
            xprint("Idx ");
            xprint_int(i);
            xprint(" His_S ");
            xprint_char(debbuf[i][5]);
            xprint(" type ");
            xprint_char(debbuf[i][1]);
            xprint("\n");
        } */
    } else if (strcmp(cline, ("dq")) == 0) {
        for (int i = 0;i<EBCOUNT;i++) {
            xprint_char(ebnumber[i]);
            xprint(" ");
            xprint_int(ebcount[i]);
            xprint("\n");
        }
    } else if (strcmp(cline, ("xx")) == 0) {
        send_log_packet("First message\r\n");
/*    } else if (strcmp(cline, ("sn")) == 0) {
        char r;
        if (argc == 2)
            r = atoi(argv[1]);
        my_R = r;
        sendnack(r);
    } else if (strcmp(cline, ("log")) == 0) {
        send_log("nc logger \r"); */
    } else if (strcmp(cline, ("heard")) == 0) {

//        xprint("Not impl");
        showheard();
        /*    } else if (strcmp(cline, ("ei")) == 0) {
                // test to read and write int registers
                              uint8_t sir = getSIR();
                              xprint("SIR = ");
                              xprint_xchar(sir);
                              setSIMR(1);   // enable socket 0 mask reg
                              uint8_t simr = getSIMR();
                              xprint(" SIMR = ");
                              xprint_xchar(simr);
                              uint8_t ir = getSn_IR();
                              xprint(" IR = ");
                              xprint_xchar(ir);
                              uint8_t imr = getSn_IMR();
                              xprint(" IMR = ");
                              xprint_xchar(imr);
                              xprint("\n"); */
    } else if (strcmp(cline, ("tasks")) == 0) {
        char outbuf[200];
        char *op = outbuf;
//        TaskStatus_t cmdtask;
//        TaskStatus_t myDetails;
//        void vTaskList( op );
/*        UBaseType_t Kalle;
        Kalle = uxTaskGetNumberOfTasks();
        xprint_int(Kalle);
        xprint(" tasks running \n"); */
        vTaskList( op);
        xprint("Name         State    Priority Stack   num\n");
        xprint(outbuf);
    }  else {
      if (argc > 0) {
        xprint("Illegal command\n");
      }
    }
}

    int iidx,oidx;
    uint8_t ubuf[4][300];
    int ucount[4];
void queue_uart(char * buffer, int count) {
        ubuf[iidx][0] = PTXT;
        memcpy(&ubuf[iidx][1], buffer, count);
        ucount[iidx] = count+1;
        if (++iidx >= 3) {
            iidx = 0;
        }
}


void checkcommand(void) {
    UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
//    if (cmdbytes != 0) { // we have a cmdline in progress
        cmdline[cmdptr++]=cc;
        cmdline[cmdptr] = 0;
        UART2_write(uart1,&cc,1,NULL); // echo one character
        if(cc == 0x0d) {               //if it was a cr
            cc = 0x0a;
            UART2_write(uart1,&cc,1,NULL); //echo a lf as well
            cmdptr--;                      // ERASE the cr
            cmdline[cmdptr] = 0;           // and terminate string
            if (cmdline[0] != 0)
                parse_cmd(cmdline, cmdptr);    // send off command
            cmdptr = 0;                    // empty cmdline
            xprint("$ ");
        }
//        cmdbytes = 0;
//        UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
//    } else {    // cmdbytes == 0
/*        if(Txcount) {
            if(Timer0 == 0) {
                Timer0 = 60000;
                Txcount--;
                xprint("snd\n");
            }
        } */
/*        if(Becount) {
            if(Timer0 == 0) {
                Timer0 = 60000;
                Becount--;
                Send_beacon();
            }
        } */
//        xprint("-");
//    }
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


void start_terminal(void) {
    xprint("\n");
    xprint("AmprNet.se-radio Freertos ");
    xprint(version);
    xprint("\n");
    cmdptr = 0;
    cmdline[cmdptr] = 0;
//    UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
}



/* Initialize UART1 with callback read mode GW */

void init_uart_1(void) {
    UART2_Params_init(&uartParams1);
    uartParams1.baudRate = 115200;
    // uartParams1.readMode = UART2_Mode_CALLBACK;
    uartParams1.readMode = UART2_Mode_BLOCKING;
    uartParams1.writeMode = UART2_Mode_BLOCKING;
//    uartParams1.readCallback = ReceiveonUARTcallbac1;
//    uartParams1.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    uart1 = UART2_open(CONFIG_UART2_1, &uartParams1);
    start_terminal();
//    xprint("\n");
//    xprint("$ ");
}

void Cmd(void) {
    TaskStatus_t cmdtask;
    uint8_t *op;
    vTaskGetInfo(NULL, &cmdtask, pdFALSE, eInvalid); // do not get stack info
    op = cmdtask.pcTaskName;
    memcpy(op,"Cmd\0",4);
    init_uart_1();
    printMAC();
    xprint("\n");
    xprint("$ ");

        while(1) {
//      vTaskDelay(pdMS_TO_TICKS(1000));
      checkcommand();
    }
}
/*
void ReceiveonUARTcallbac1(UART2_Handle uart1, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status == UART2_STATUS_SUCCESS) {
        cmdbytes = count;
    }   else {
        while (1) {}
    }
};
*/
