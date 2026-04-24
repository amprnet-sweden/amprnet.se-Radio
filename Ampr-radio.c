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
#include "lcd.h"
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

UART2_Handle uart1;
UART2_Params uartParams1;

#define BEA_LENGTH 255
#define Nsize 100



// radio parameters
unsigned int deviation = 350;
unsigned int bitrate = 0xC0000;
unsigned int rxBw = 100;
//
int role =0;        // node role, master, slave, autoconfig master (to be defined)
int myslot;          // if this is 1 it is our time to send
int EthEna = 0;
int LcdEna = 1;
int debug = 0;
int tdelay = 6;     // default delay afte tx
int retrena = 1;        // retransmit enabled by default
char cc;
int Txcount;      // the number of test packets
int Becount = 0;      // beacon counter
int Acktime;        // time after last imm segment received
char segbits = 0;
char dsegbits = 0;  // segbits at dropped time
char dropseg = 8;   // segment to drop to test retransmission
int dropctr = 0;
int rexmitctr = 0;
int rexmitOK = 0;
int rexrqctr =0;
char dseg;  // the first segment we detected as dropped
uint8_t his_S, his_R, my_S, my_R, my_Q;
//#define Ackdel 2  // assume 20 mS acktime
unsigned char Beseg = 0;
unsigned char pktnum = 0;
char beabuf[BEA_LENGTH];
// command line variables
static char cmdline[100];    // Nsize??
int cmdptr = 0;             // pointer in string
int cmdbytes = 0;              // nr of bytes accumulated
uint8_t m1,m2,m3,m4,m5,m6;
uint8_t my_hwaddr[6];
char rssi = 0x92;       // -110 dBm
char my_call[12] = {"MY0CALL-001\0"};
char version[] ="X 2.0o";
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
unsigned int onesec = 5;    // depends on Timer_per, 5 times 200 mS

uint8_t pktbuf[1518];
NVS_Handle nvsHandle;
NVS_Params nvsParams;
NVS_Attrs regionAttrs;

int tdma_ena = 0;   // tdma packet disabled for now

unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
unsigned long RRpkts,RSpkts,ERpkts,ESpkts,URpkts,USpkts,TRpkts,TSpkts,ARPreq,ARPans;


uint8_t debbuf[100][30];

bool rebootRequest = false;

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
    } else if (strcmp(cline, ("mode")) == 0) {
        if(argc == 2) {
          mode = atoi(argv[1]);
            switch(mode) {
              case 0:
                bitrate = 0xa0000;
                deviation = 350;
                break;
            case 10:
                bitrate = 0xa0000;
                deviation = 500;
                break;
            case 11:
                bitrate = 0xb0000;
                deviation = 550;
              break;
            case 12:
                bitrate = 0xc0000;
                deviation = 600;
                break;
            case 13:
                bitrate = 0xd0000;
                deviation = 650;
                break;
            case 14:
                bitrate = 0xe0000;
                deviation = 650;
                break;
            case 15:
                bitrate = 0xf0000;
                deviation = 650;
                break;
            case 16:
                bitrate = 0x100000;
                deviation = 650;
                break;
            default:
                xprint("Only values 0 and 10 - 15 supported for now\n");
                mode = 10;
            }
            parchange = 1;
          } else {
            xprint("Mode ");
            xprint_char(mode);
            xprint("\n");
          }
    } else if (strcmp(cline, ("par")) == 0) { // this is just to be able to test various parameter combinations, to be removed
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
        xprint("Role : ");
        if(role == 0) {
            xprint("Slave");
        } else {
            xprint("Master");
        }
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
        buf[24] = role;
        buf[25] = autoconnect;
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
    } else if (strcmp(cline, ("master")) == 0) {
        if(argc == 2) {
          role = atoi(argv[1]);
          if(role == MASTER && Timer_tdm == 0)
              Timer_tdm = TDMAPERIOD;
        }
        xprint("Node role = ");
        xprint_int(role);
        xprint("\n");
    } else if (strcmp(cline, ("conn")) == 0) {
        if(role == 0) {
            if (cstate == 0) {
               cstate = 1;
            } else {
                xprint("Conn in process\n");
            }
        } else {
            xprint("You're master\n");
        }
    } else if (strcmp(cline, ("disc")) == 0) {
         if(role == 0) {
            if (cstate != 0) {
               cstate = 3;
            } else {
                xprint("Already disconnected\n");
         }
        } else {
           xprint("You're master\n");
        }
    } else if (strcmp(cline, ("listen")) == 0) {
        if(argc == 2) {
          listener = atoi(argv[1]);
        }
        xprint("Listen mode = ");
        xprint_int(listener);
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
        rexmitOK = 0;
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
          dropseg = atoi(argv[1]) & 7;
        } else {
            xprint("drop = ");
            xprint_int(dropseg);
            xprint("\n");
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
        xprint("\n");

#endif
#ifdef LCD
    } else if (strcmp(cline, ("lcd")) == 0) {
//        char blocks[] = "==========>>";
        if(LcdEna == 0) {
        LCD_Print(version);
        delay(1);
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
        xprint(" corrected ");
        xprint_int(rexmitOK);
        xprint("\n");
        xprint("Que depth ");
        xprint_int(quedepth);
        xprint("\n");
        xprint("Que max ");
        xprint_int(quemax);
        xprint("\n");
// to debug ebufs
        xprint(" Internal dropped packets Ether Radio\n");
        xprint_int(droppedEthPackets);
        xprint("      ");
        xprint_int(droppedRadioPackets);
        xprint("   Get - Free = used      ");
        xprint_int(ebufsused);
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
    } else if (strcmp(cline, ("sy")) == 0) {
         new_synch = 1;
    } else if (strcmp(cline, ("tdt")) == 0) {
         showtdma();
    } else if (strcmp(cline, ("ctab")) == 0) {
         showctab();
    } else if (strcmp(cline, ("tlst")) == 0) {
         showtlist();
    } else if (strcmp(cline, ("tdma")) == 0) {
         if(argc >= 2)
             tdma_ena = atoi(argv[1]);
         xprint("tdma ");
         xprint_int(tdma_ena);
         xprint("\n");


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
    } else if (strcmp(cline, ("tasks")) == 0) {
        char outbuf[200];
        char *op = outbuf;
//        vTaskList( op);
        xprint("Name         State    Priority Stack   num\n");
        xprint(outbuf);
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
    } else if (strcmp(cline, ("deviation")) == 0) {
        if (argc == 2) {
          deviation = atoi(argv[1]);
        } else {
            xprint("deviation = ");
            xprint_int(deviation);
            xprint("\n");
        }
    } else if (strcmp(cline, ("rate")) == 0) {
        if (argc == 2) {
          bitrate = atoi(argv[1]) * 0x10000;
        } else {
            xprint("bitrate = ");
            xprint_int(bitrate/0x10000);
            xprint("\n");
        }
    } else if (strcmp(cline, ("rxbw")) == 0) {
            if (argc == 2) {
              rxBw = atoi(argv[1]);
            } else {
                xprint("rxBw = ");
                xprint_int(rxBw);
                xprint("\n");
            }
    } else if (strcmp(cline, ("auto")) == 0) {
        if (argc == 2) {
          autoconnect = atoi(argv[1]) & 1;
        } else {
            xprint("autoconnect = ");
            xprint_int(autoconnect);
            xprint("\n");
        }
    } else if (strcmp(cline, ("reboot")) == 0) {
        rebootRequest = true;
    }  else {
          if (argc > 0) {
             xprint("Illegal command\n");
          }
    }
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
                    switch(mode) {
                      case 0:
                        bitrate = 0xa0000;
                        deviation = 350;
                        break;
                    case 10:
                        bitrate = 0xa0000;
                        deviation = 500;
                        break;
                    case 11:
                        bitrate = 0xb0000;
                        deviation = 550;
                      break;
                    case 12:
                        bitrate = 0xc0000;
                        deviation = 600;
                        break;
                    case 13:
                        bitrate = 0xd0000;
                        deviation = 650;
                        break;
                    case 14:
                        bitrate = 0xe0000;
                        deviation = 650;
                        break;
                    case 15:
                        bitrate = 0xf0000;
                        deviation = 650;
                        break;
                    case 16:
                        bitrate = 0x100000;
                        deviation = 650;
                        break;
                    default:
                        xprint("Only values 0 and 10 - 15 supported for now\n");
                        mode = 10;
                    }
                    // EthEna = buff[7] & 1; Deprecated, EthEna is set by eth_if depending on if the Ethernet chip is found
                    for(int i=0;i<12;i++) {
                         my_call[i] = buff[8+i];
                    }
                    my_call[11] = 0;
                    for(int i=0;i<4;i++) {
                         my_ip[i] = buff[20+i];
                    }
                    role = buff[24];
                    autoconnect = buff[25];
            }
            xprint("Stored Frequency ");
            xprint_int(freq);
            xprint("\n");
            xprint("Role : ");
            if(role == 0) {
                xprint("Slave");
                if(autoconnect)
                    xprint(" autoconnect");
                else
                    xprint(" manual connect");
            } else {
                xprint("Master ");
//                xprint_int(nrconnects);
//                xprint(" connected");
            }
            xprint("\n");
            xprint("Radio mode ");
            xprint_char(mode);
            xprint("\n");
            xprint("Ethernet ");
            xprint_char(EthEna);
            xprint("\n");
            NVS_close(nvsHandle);
    }
//    uint8_t Uartbuf[256];
    void dequeue_uart(void) {
        if(oidx != iidx) {
            USpkts++;
            USbytes += (ucount[oidx]-1); // size here includes pktype
            SendPacket(&ubuf[oidx][0],ucount[oidx]);
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
                    if(autoconnect && (cstate == IDLE)) {
                        cstate = CONNECTING;
                    }
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
//            GPIO_write(sigpin4,0);
            ethBufHandle_t bufferHandle = ampr_dequeueEth();
            if(bufferHandle.bytesUsed != 0) {     // must be a valid count
                if(bufferHandle.bytesUsed <= 1514) {
//                    my_S = bufferHandle.packetNumber;
//                    send_epkt(bufferHandle.buffer, bufferHandle.bytesUsed);
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
      RX_ON();
//   } // ethena = 1
}

int dbgptr;
/* process tail packet, and check that we agree on sequence, drop is segments dropped on receive */
char rdrop = 0;
char droppkg;
void proc_type7(uint8_t *buffer, int len, char drop) {
    his_R = buffer[4];
    his_S = buffer[5];
    memcpy(last_radio, &buffer[6],6);  // save sender of this type7
    if(drop == 0) {  // this packet is valid
//        my_R = his_S;   // this is where good packets are incremented
        if((rdrop == 1) && (droppkg == his_S)) {
            if(debug & 32) {
              xprint("Rsyn + repair\n");
            }
            rexmitOK++;     // inc number succesful retransmits
            rdrop = 0;      // the "remember previous drop indicator"
            my_R = his_S;
        }
        my_R = his_S;       // we are now in synch
    } else {    // at least one segment was dropped, and packet is not valid
        rdrop = 1;
        droppkg = his_S;    // save number dropped
        if(debug & 32) {
          xprint("Rdrop ");
          xprint_char(his_S);
          xprint("\n");
        }
    }
    if(buffer[1] == ACK) {  // an ack and something with rnum vs snum
//      my_R = his_S;     // resynchronize
      if(debug & 64) {
//        xprint("\n");
        xprint("ACK rcv his R ");
        xprint_char(his_R);
        xprint(" my S ");
        xprint_char(my_S);
/*        xprint(" my R ");
        xprint_char(my_R);
        xprint(" his S ");
        xprint_char(his_S); */
        xprint(" len ");
        xprint_int(len);
/*        for(int i = 6;i<12;i++) {
            xprint(" ");
            xprint_xchar(buffer[i]);
        } */
        xprint("\n");
      }
      if(his_R != my_S ) { // if his received is NOT my last sent we need to retransmit

//          xprint("He lost ");
//          xprint_char(my_S);
//          xprint("\n");
//          if(my_S - his_R == 1) {
          if (retrena) retran = 1;
/*          if(ampr_ethQueueEmpty()) {
//              xprint("Send q empty\n");
              queue_idle_data();      // make an idle message
          }
          queue_idle_data();      // make an idle message */
/*          if(debug & 32) {
            xprint("Got Ret RQ ");
            xprint_char(his_R + 1);
            xprint(" Diff ");
            xprint_char(my_S - his_R);
            xprint("\n");
          } */
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
// char old_R;

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
//        GPIO_write(sigpin3,0);
        proc_type7(buffer, count, dropped);
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
//            GPIO_write(sigpin3,1);
        send_ether(buffer,length);
//            GPIO_write(sigpin3,0);
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
