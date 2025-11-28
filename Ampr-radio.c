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
#include "heard.h"
#include "lcd.h"
#include "tdma.h"

#define E5500   //compilation switch

#ifdef E5500
#include "w5500.h"
#endif
// install Button callback and enable interrupts
//    GPIO_setCallback(CONFIG_GPIO_BUTTON0, gpioButton0Fxn);
//    GPIO_enableInt(CONFIG_GPIO_BUTTON0);
void w5500int(uint_least8_t index);

UART2_Handle uart1;
UART2_Params uartParams1;

#define BEA_LENGTH 255
#define Nsize 100




int EthEna = 0;
int LcdEna = 1;
int debug = 0;
int tdelay = 5;
int retrena = 0;
char cc;
int Txcount;      // the number of test packets
int Becount = 0;      // beacon counter
int Acktime;        // time after last imm segment received
char segbits = 0;
char dsegbits = 0;  // segbits at dropped time
char dropseg = 8;   // segment to drop to test retransmission
int dropctr = 0;
int rexmitctr = 0;
int rexrqctr =0;
char dseg;  // the first segment we detected as dropped
//uint8_t pktnumS,pktnumR;    // pktnum sent over radio, received from radio
uint8_t his_S, his_R, my_S, my_R, my_Q;
#define Ackdel 2  // assume 20 mS acktime
unsigned char Beseg = 0;
unsigned char pktnum = 0;
char beabuf[BEA_LENGTH];
static char cmdline[100];    // Nsize??
//static char xx[20];
int cmdptr = 0;             // pointer in string
int cmdbytes = 0;              // nr of bytes accumulated
//static char xy[20];
uint8_t m1,m2,m3,m4,m5,m6;
uint8_t my_hwaddr[6];
char rssi = 0x92;       // -110 dBm
char my_call[12] = {"MY0CALL-001\0"};
char version[] ="V 0.93e";
//settings
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
unsigned int onesec = 5;    // depends on Timer_per, 5 times 200 mS

uint8_t pktbuf[1518];
NVS_Handle nvsHandle;
NVS_Params nvsParams;
NVS_Attrs regionAttrs;

int tdma_ena = 0;   // tdma packet disabled for now

unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
unsigned long RRpkts,RSpkts,ERpkts,ESpkts,URpkts,USpkts,TRpkts,TSpkts,ARPreq,ARPans;
#ifdef CC1314R10
#define EBCOUNT 30
#else
#define EBCOUNT 22
#endif
    int eiidx = 0;
    int eoidx = 0;

    uint8_t ebufs[EBCOUNT][1514];
    int ebcount[EBCOUNT];
    uint8_t ebnumber[EBCOUNT];

uint8_t debbuf[100][30];


void test_epkt(uint8_t * pkt, int cnt);
void sendnack(char dseg); // send a status telling we lost x segments starting w dseg
void doretransmit(void);

static void ReceiveonUARTcallbac1(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
/* Initialize UART1 with callback read mode GW */

void init_uart_1(void) {
UART2_Params_init(&uartParams1);
uartParams1.baudRate = 115200;
uartParams1.readMode = UART2_Mode_CALLBACK;
uartParams1.readCallback = ReceiveonUARTcallbac1;
uartParams1.readReturnMode = UART2_ReadReturnMode_PARTIAL;
uart1 = UART2_open(CONFIG_UART2_1, &uartParams1);
}

char init_ether() {
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
/* void init_amprradio(void) {
    for(i=0;i<EBCOUNT;i++) {
        ebcount[i] = 0;
    }
} */
void parse_cmd(char *cline, int cnt) {
    int argc;
    int val;
//    int status;
    uint32_t AONBatMonBatteryVoltageGet();
    uint32_t ui32CurrentBattery;
    char* argv[10];             // maximum number ot args
    const char s[4] = " \r\n\0";
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
/*    for(int i =0; i<cnt+1; i++) {
        xprint_xchar(cline[i]);
        xprint(" ");
        if(cline[i] == 0) break;
    }
    xprint("\n"); */
    /* now, check for specific commands */


    if (strcmp(argv[0],("help")) == 0) {
          xprint("List of commands\n"
                  "\n"
          "help"
                  "\r\n"
         "frequency :\n\r"
          "myaddr :"
          "\r\n"
          "peeraddr :"
           "\r\n"
           "show :"
           "\r\n"
           "rssi :"
          "\r\n"
          "mode :"
          "\r\n"
          "beacon :"
          "\r\n"
          "save :"
           "\r\n"
          "count :"
          "\r\n");


          /*              "mode <pin> <mode>: pinMode()\r\n"
              "read <pin>: digitalRead()\r\n"
              "aread <pin>: analogRead()\r\n"
              "write <pin> <value>: digitalWrite()\r\n"
              "awrite <pin> <value>: analogWrite()\r\n"
              "echo <value>: set echo off (0) or on (1)"); */
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
    } else if (strcmp(cline, ("par")) == 0) {
        xprint("Changing parameters \n");
        parchange = 1; 
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
        sprintf(showbuf,"Temp : %d C Batt : %ld.%03ld V Secs : %d \r\n",Temperature_getTemperature(),ui32CurrentBattery/1000,ui32CurrentBattery%1000, Runtime/1000);
        xprint(showbuf);
        printMAC();
        xprint("Sent     Recvd      Bad      Loopctr\n");
        xprint_int(Sent);
        xprint("        ");
        xprint_int(Recd);
        xprint("        ");
        xprint_int(Bad);
        xprint("        ");
        xprint_int(loopctr);

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
       for(int i=0;i<12;i++) {
            buf[8+i] = my_call[i];
        }
        for(int i = 0;i<4;i++) {
            buf[20+i] = my_ip[i];
        }
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
        TSpkts = 0;
        TRpkts = 0;
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
            bool stat = w5500begin(my_hwaddr);
//            if((stat == true) && (link == true)) {
            if(stat == true)  {
              xprint("Ether initiated\n");
              EthEna = 1;
            } else {
                xprint("w5500 or wrong revision\n");
//                EthEna = 2;
            }

        } else
            xprint("Already enabled or broken\n");
        }
    } else if (strcmp(cline, ("link")) == 0) {
        char phy = getPHYCFGR();
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
        } 
        xprint("\n");
        xprint_xchar(phy); */
        xprint("\n");

#endif
#ifdef LCD
    } else if (strcmp(cline, ("lcd")) == 0) {
//        char blocks[] = "==========>>";
        if(LcdEna == 0) {
        LCD_Print(version);
        delay(1);
/*        LCD_Goto(1,2);
        delay(1);
        LCD_Print(&version);
        delay(1);
        LCD_Goto(1,3);
        delay(1);
        LCD_Print(&version);
        LCD_Goto(1,4);
        delay(1);
        LCD_Print(&blocks); */
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
        xprint("TDMA received             TDMA sent\n");
        xprint_long(TRpkts);
        xprint("                      ");
        xprint_long(TSpkts);
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
    } else if (strcmp(cline, ("ack")) == 0) {
        for (int i = 0; i<100;i++) {
            xprint("Idx ");
            xprint_int(i);
            xprint(" His_S ");
            xprint_char(debbuf[i][5]);
            xprint(" type ");
            xprint_char(debbuf[i][1]);
            xprint("\n");
        }
    } else if (strcmp(cline, ("dq")) == 0) {
        for (int i = 0;i<EBCOUNT;i++) {
            xprint_char(ebnumber[i]);
            xprint(" ");
            xprint_int(ebcount[i]);
            xprint("\n");
        }
    } else if (strcmp(cline, ("sn")) == 0) {
        char r;
        if (argc == 2)
            r = atoi(argv[1]);
        my_R = r;
        sendnack(r);
    } else if (strcmp(cline, ("heard")) == 0) {
        showheard();
    } else if (strcmp(cline, ("tdt")) == 0) {
         showtdma();
    } else if (strcmp(cline, ("tdma")) == 0) {
         if(argc >= 2)
             tdma_ena = atoi(argv[1]);
         xprint("tdma ");
         xprint_int(tdma_ena);
         xprint("\n");
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

    } else if (strcmp(cline, ("mycall")) == 0) {
      if(argc >= 2) {
          int siz = strlen(argv[1]);
          if((siz >= 15) || (siz < 4)) {
              xprint("Call length??\n");
          } else {
            strcpy(my_call, argv[1]);
            xprint(my_call);
            xprint("\n");
          }
      } else {
          xprint(my_call);
          xprint("\n");
      }

    } else if (strcmp(cline, ("ipadd")) == 0) {
        char *str = "192.168.0.1"; //, *str2;
        unsigned char value[4] = {0};
        size_t index = 0;
        if(argc >= 2) {
            str = argv[1];
//            str2 = str; /* save the pointer */
            while (*str) {
                if (isdigit((unsigned char)*str)) {
                    value[index] *= 10;
                    value[index] += *str - '0';
                } else {
                    index++;
                }
                str++;
            }
            xprint_int(value[0]);
            xprint(".");
            xprint_int(value[1]);
            xprint(".");
            xprint_int(value[2]);
            xprint(".");
            xprint_int(value[3]);
            xprint("\n");
            int siz = strlen(argv[1]);
            if((siz > 15) || (siz < 7)) {
                xprint("Call length??\n");
            } else {
                for(int i=0;i<4;i++) {
                    my_ip[i] = value[i];
                }
            }
        } else {
            for(int i=0;i<4;i++) {
                xprint_char(my_ip[i]);
                xprint(".");
            }
            xprint("\n");
        }

    }  else {
          if (argc > 0) {
             xprint("Illegal command\n");
          }
    }
//    xprint("\n");
}
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
                    for(int i=0;i<12;i++) {
                         my_call[i] = buff[8+i];
                    }
                    my_call[11] = 0;
                    for(int i=0;i<4;i++) {
                         my_ip[i] = buff[20+i];
                    }
            }
            xprint("Stored Frequency ");
            xprint_int(freq);
            xprint("\n");
            xprint("Ethernet ");
            xprint_char(EthEna);
            xprint("\n");
            NVS_close(nvsHandle);
    }
//    uint8_t Uartbuf[256];
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
    void xprint_xchar(char x) {
        char outbuf[6];
        sprintf(outbuf,"%02x",x);
        xprint(outbuf);
    }


    void checkcommand(void) {
        if (cmdbytes != 0) { // we have a cmdline in progress
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
            cmdbytes = 0;
            UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
        } else {    // cmdbytes == 0
            if(Txcount) {
                if(Timer0 == 0) {
                    Timer0 = 60000;
                    Txcount--;
//                    xprint("snd\n");
                }
            }
            if(Becount) {
                if(Timer0 == 0) {
                    Timer0 = 60000;
                    Becount--;
                    Send_beacon();
                }
            }
            if(Timer_per == 0) {
                Timer_per = 200;     //set to 200 mS
                if (LcdEna == 0) {
                   smeter(rssi);
                }
                if(--onesec == 0) {
                    onesec = 5;
//                 xprint("TDMA ttl\n");
                    tdma_ttl();
                }
            }
        }
    }
//}
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
    xprint("AmprNet.se-radio ");
    xprint(version);
    xprint("\n");
    cmdptr =0;
    cmdline[cmdptr]=0;
    UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
}



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
/* Decode Packet. If we have set BYTE_ADDR bit we have two bytes address field
 * . These are destination and source, and we check these against our own address
 * If the TYPE_BYTE bit is set in mode, we expect a byte explaining what is in the packet
 * The type byte is also a place for a segment counter (0-7) and a last segment bit.
 * This can allow us to transfer larger packets than 255 bytes, which is what the radio is set
 * up for in this Version. In this case the (larger) packet is segmented into maximum
 * 8 segments, and the last segment has the LAST_SEG bit set. Reassembly checks all
 * received buffers, and completes when the last segment is received.
 * THE FOLLOWING CODE IS WORK IN PROGRESS; AND NOT FINISHED
 */
/* void decode_packet(char * buffer, uint8_t count) {
    uint8_t sender_address;
    uint8_t * bufp;
    bufp = buffer;
    if (mode & BYTE_ADDR) { //we are using byte addressing
      if ((*bufp == myaddr) || (*bufp == 0xff)) {   // packet for me or for all
          bufp++;
          sender_address = *bufp++;
      } else {
          bufp++;
          bufp++;
      }
    }
    if (mode & TYPE_BYTE) { // we are using a type byte
         switch (*bufp) {
           case 0:
             break;
           case 1:
             break;
           case 4:
              break;
           default:
           break;
      }
      bufp++;
      }

} */
/*
void log_from_queue(char * buffer, uint8_t count) {
    xprint("Received : ");
    xprint_int(count);
    xprint(" Rssi ");
    xprint_schar(rssi);
    xprint("\n");
} */
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

void ReceiveonUARTcallbac1(UART2_Handle uart1, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status == UART2_STATUS_SUCCESS) {
        cmdbytes = count;
    }   else {
        while (1) {}
    }
};

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

void send_epkt(uint8_t *pktbuf, int reclen);
#define chunk 253
char ackbits;
uint8_t rexmitbuf[chunk+10];


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
//    xprint("rexmit seg : ");
//    xprint("\n");
}


// queue an ethernet packet. do not bother with overwrite
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
    int ethlen(void) {
         if(eoidx != eiidx) {
             return(ebcount[eoidx]);
         } else {
             return(0);
         }
     }

    int dequeue_eth(void) {
      uint8_t extrabuf[1500];
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
//                my_S = ebnumber[eoidx];
//                send_epkt(&ebufs[eoidx][0], ebcount[eoidx]);
//                count = ebcount[eoidx];

                if (memcmp (&ebufs[eoidx][0], my_hwaddr,6) == 0) {
                    proc_eth(&ebufs[eoidx][0], ebcount[eoidx]);
                } else {

//                    GPIO_write(sigpin2,1);
//                    GPIO_toggle(sigpin2);
                    my_S = ebnumber[eoidx];
                    send_epkt(&ebufs[eoidx][0], ebcount[eoidx]);
                    count = ebcount[eoidx];
                    test_epkt(&ebufs[eoidx][0],count);
                    if(ebufs[eoidx][0] == 0xff) {
                       memcpy(extrabuf,&ebufs[eoidx][0],ebcount[eoidx]); // copy broadcast contents
                       proc_eth(extrabuf, count);
                    }
                }

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


//uint8_t savepacket[1514];
//int savelen;
//uint8_t savepktnr;
// we get here when we are checking for ethernet packets
// we do this before serving the ethernet for more accurate packet order
// retran should be 1 and wantednum the wanted packet
void doretransmit(void) {

//    xprint("doretransmit\n");
//    if(retran != 0) {
      if(debug & 1024)  {
        wantednum++;    // wantednum up til now is his_R,
        xprint("Retran want: ");
        xprint_char(wantednum);
//        xprint_char(his_S);
//        xprint(" Our R ");
//        xprint_char(my_R);
//        xprint(" Our_S ");
//        xprint_char(my_S);
        xprint(" His R ");
        xprint_char(his_R);
//        xprint(" want ");
//        xprint(" len ");
//        xprint_int(savelen);
        xprint("\n");
/*        if(my_R != his_S) {
            xprint(" we want :");
            xprint_char(my_R + 1);
            xprint("\n");
        } */
      } // debug & 8
      char found = 0;
      for(int i=0;i<EBCOUNT;i++) {
//          if(his_R + 1 == ebnumber[i]) {
          if(wantednum == ebnumber[i]) {
              if(debug & 8 ) {
                xprint("Lookup match ");
                xprint_char(ebnumber[i]);
                xprint("\n");
                found=1;
//                rexmitctr++;
              }
//              my_S = ebnumber[i];       // set our S in synch GW 241227
              if(ebcount[i] != 0) {
                my_S++;
                send_epkt(&ebufs[i][0],ebcount[i]);
              }
/*              xprint("S : ");
              xprint_char(ebnumber[i]);
              xprint("\n"); */
              break;
          }
      }
      if ((debug & 8) && (found == 0)) {
          xprint("Not Found ");
          xprint_char(wantednum );
          xprint("\n");
      }
      retran = 0;
//    } // if retran != 0
}

uint8_t ackbuf[30];

#define ACK 11
#define REX 22

uint8_t radio_buff[chunk+10];

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
//          RX_OFF();
          send_tdma_packet();       // for now, just send before data
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
//          RX_ON();
//          GPIO_write(sigpin2,0);
/*         if(ackbuf[11] != 0x55) {
              xprint(" S bad ");
              xprint_xchar(ackbuf[11]);
              xprint("\n");
          } */
          Timer_def = tdelay; //set defer timer
      }
   } // ethena = 1
}
void check_ethernet() {
   int reclen;
//   GPIO_toggle(sigpin); // just to check loop time

   if (EthEna == 1) {
   // first do any requested retransmissions
   // retran should be 1
   // wantednum should be desired packet = his received + 1

     do {
//         GPIO_write(sigpin,1);
         reclen = w5500readFrame(pktbuf, sizeof(pktbuf));
//         GPIO_write(sigpin,0);

         if(reclen != 0) {
//           GPIO_write(sigpin,1);
//             my_S++;
//             queue_eth(pktbuf,reclen,my_S);
             my_Q++;
             queue_eth(pktbuf,reclen,my_Q);
//           GPIO_toggle(sigpin);
//           send_epkt(pktbuf, reclen);
//           GPIO_toggle(sigpin);
//             test_epkt(pktbuf,reclen);
//           GPIO_toggle(sigpin);
//           xprint("X\n");
         }
     } while (reclen != 0);
/* now we have queued all frames from the w5500
 * now dequeueu them one at the time
 */
   } // if ethena
   if(Timer_def == 0) {
      if (EthEna == 1) {
           if((uartlen() != 0) || (ethlen() != 0)) {
            current_defer = 80 + uartlen() + ethlen(); //
//            snapshot = Runtime;    //
//             snapbytes = current_defer;
//             snapshots[snapctr]
            GPIO_write(sigpin2,1);
            RX_OFF();
//            send_tdma_packet();
            dequeue_uart();
            if (EthEna == 1) {
                dequeue_eth();
 //               xprint("Q-");
            }
            RX_ON();
            GPIO_write(sigpin2,0);

 //             Timer_def = 160; // why was this set???
        } else {  // nothing to send, just send tdma
            if(Timer_tdm == 0) {
//                  xprint(".");
                GPIO_write(sigpin3,1);
                RX_OFF();
                send_tdma_packet();
                RX_ON();
                GPIO_write(sigpin3,0);
                Timer_tdm = TDMAPERIOD;
                  Timer_def = tdelay; // do not transmit immediately
 //               Timer_def = 1; // do not transmit immediately
            }
          }
       }
   }
}
int dbgptr;
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
        wantednum = his_R;
        if( debug & 8 ) {
          xprint("REX Rcv his R ");
          xprint_char(his_R);
          xprint(" his_S ");
          xprint_char(his_S);
          xprint("\n");
        }
    }
}
char old_R;
void sendnack(char dseg) { // send a status telling we lost x segments starting w dseg
    if(old_R != my_R) {
      old_R = my_R;     // enabled 250211 GW
      my_S++;
      ackbuf[0] = PETH | 7 | FINFLAG;
      ackbuf[1] = REX;
      ackbuf[2] = segbits;
      ackbuf[3] = dseg;
      ackbuf[4] = my_R;
      ackbuf[5] = my_S;
      memcpy(&ackbuf[6],my_hwaddr,6);    //mac id of packet
//      GPIO_write(sigpin,1);
      SendPacket(ackbuf,20);
//      GPIO_write(sigpin,0);
//      rexrqctr++;
      if(debug & 4) {
        xprint("S REX REQ pkt ");
        xprint_char(my_R);
        xprint("\n");
      }
    }
}
void sendack(char dseg) { // send a status telling we lost x segments starting w dseg
//      my_S++;
      ackbuf[0] = PETH | 7 | FINFLAG;
      ackbuf[1] = ACK;
      ackbuf[2] = 0;        //debug 22-feb-2025 GW
      ackbuf[3] = 0;
      ackbuf[4] = my_R;
      ackbuf[5] = my_S;
      memcpy(&ackbuf[6],my_hwaddr,6);    //mac id of packet
//      GPIO_write(sigpin,1);
      SendPacket(ackbuf,20);
//      GPIO_write(sigpin,0);
//      rexrqctr++;
      if(debug & 4) {
        xprint("S ACK pkt ");
        xprint_char(my_R);
        xprint("\n");
      }
}


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
    RRbytes += count;
    seg = buffer[0] & 7;
    if(debug & 1) {
      xprint("Rin : ");
      xprint_int(count);
      xprint(" sg ");
      xprint_char(seg);
      xprint("\n");
    }
// just debugging option
    if (seg == dropseg) {
        dropseg = 16;   //only drop once
        return; // just a test to see retrans works
    }
    /* if we got a 0 segment set up segbits, and clear dropped */
    if (seg == 0) {
//        GPIO_write(sigpin3,1);
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
          if ((debug & 0x1) != 0) {
            xprint("Eth out : ");
            xprint_int(ecount);
            xprint(" segbits : ");
            xprint_xchar(segbits);
            xprint("\n");
          }
          ESbytes += ecount;
          ESpkts++;
          dump_packet(xmitbuffer,ecount);
          expseg = 7;
        } // this was a final packet
      } else {      //dropped was 1 or more
        dropctr++;
      }
  } else {    // we get here on only seg 6 & 7
/*
 * we do not know if this is an ACK packet or a REX packet or a type 6 packet
 */
      if(seg == 7) {
//        GPIO_write(sigpin3,0);
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
              GPIO_write(sigpin,1);
              w5500sendFrame(xmitbuffer,ecount);
              GPIO_write(sigpin,0);
              test_epkt(xmitbuffer,ecount);
              ecount = 0;
            }
        }
        dropped = 0;
        expseg = 0;   // and begin from 0 again
        segbits = 0;
      }
  }
}
#endif  // ethernet

void whatpacket(uint8_t * buffer, char length) {
    uint8_t pktype;
//    Timer1 = 20;        // mark recv in progress, do not interfere
    pktype = buffer[0] & 0xf0;
    switch(pktype) {
    case PETH:
        if(EthEna) {
//            GPIO_write(sigpin3,1);
            send_ether(buffer,length);
//            GPIO_write(sigpin3,0);
        }
        break;
    case PTXT:  // this is a "uart" packet, send it out
        SendText(&buffer[1], length - 1);
        URpkts++;
        URbytes += length -1;
        break;
    case PPTP:
        xprint("PTP pktype\n");
        break;
    case PXXX:
        xprint("XXX pktype\n");
        break;
    case PTDMA:
        proc_tdma_packet(buffer,length);
        break;
    default:
//        xprint("Bad pktype : ");
//        xprint_char(pktype);
//        xprint("\n");
        break;
    }
}
