#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/NVS.h>
#include <ti/drivers/Temperature.h>
#include <ti/devices/cc13x1_cc26x1/driverlib/aon_batmon.h>
#include "ti_drivers_config.h"
#include "Ampr-radio.h"

#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
UART2_Handle uart1;
UART2_Params uartParams1;

//NVS storage definitions
NVS_Handle nvsHandle;
NVS_Params nvsParams;
NVS_Attrs regionAttrs;

int tdma_ena = 0;   // tdma packet disabled for now

unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
unsigned long RRpkts,RSpkts,ERpkts,ESpkts,URpkts,USpkts,TRpkts,TSpkts,ARPreq,ARPans;

uint8_t debbuf[100][30];


static char cmdline[100];    // Nsize??
int cmdptr = 0;             // pointer in string
int cmdbytes = 0;              // nr of bytes accumulated
char cc;                    // char buffer for read

// various variables
int Txcount;      // the number of test packets
int Becount = 0;      // beacon counter

unsigned int onesec = 5;    // depends on Timer_per, 5 times 200 mS
unsigned int dhcptime = 5;  // initialy 1 second, later 30

// radio parameters related to mode
unsigned int deviation = 350;
unsigned int bitrate = 0xC0000;
unsigned int rxBw = 100;

// some variables not mandatory
int rexmitctr = 0;
int rexmitOK = 0;
int rexrqctr =0;


//static void ReceiveonUARTcallbac1(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
/* Initialize UART1 with callback read mode GW */

void init_uart_1(void) {
UART2_Params_init(&uartParams1);
uartParams1.baudRate = 115200;
//uartParams1.readMode = UART2_Mode_CALLBACK;
//uartParams1.readCallback = ReceiveonUARTcallbac1;
//uartParams1.readReturnMode = UART2_ReadReturnMode_PARTIAL;
uart1 = UART2_open(CONFIG_UART2_1, &uartParams1);
}

void start_terminal(void) {
    xprint("\n");
    xprint("AmprNet.se-radio ");
    xprint(version);
    xprint("\n");
    cmdptr =0;
    cmdline[cmdptr]=0;
//    UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
}

void vCmdIf_task(void* pvParameters)
{
	int status;

//    const TickType_t xDelay = 20 / portTICK_PERIOD_MS;

    for(;;) {
 //       vTaskDelay( xDelay );
  	    status = UART2_readTimeout(uart1,&cc,1,&cmdbytes,2000);
  	    if(status == 0 ) {
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
          if(Timer_per == 0) {
            Timer_per = 200;     //set to 200 mS
            if(--onesec == 0) {
                onesec = 5;
                tdma_ttl();
                if(autoconnect && (cstate == CIDLE)) {
                    cstate = CONNECTING;
                }
            }
#ifdef DHCPC
            if(--dhcptime == 0) {
                dhcptime = 150; // every 30 seconds
                if((my_ip[0] | my_ip[1] | my_ip[2] | my_ip[3] ) == 0) {
                  dhcp_discovery(&my_hwaddr);
                  xprint("Sent dhcp req \n");
                }
            }
#endif
          } // timer_per

        } else
        	if(status != UART2_STATUS_ETIMEOUT)
        	        xprint_int(status);
     }
}
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
#ifdef FSK4
            clearfsk4();
#endif
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
                deviation = 700;
                break;
              case 15:
                bitrate = 0xf0000;
                deviation = 750;
                break;
              case 16:
                bitrate = 0x100000;
                deviation = 800;
                break;
              case 20:
                bitrate = 0xa0000;
                deviation = 166;
#ifdef FSK4
                setfsk4();
#endif
                break;
              default:
                xprint("Only values 0 and 10 - 16 supported for now, default 12\n");
                mode = 12;
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
#ifdef DHCPC
    } else if (strcmp(cline, ("dhcp")) == 0) {
        dhcp_discovery( &my_hwaddr[0]);
#endif
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
        vTaskList( op);
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

#ifdef MEMLOG
    } else if (strcmp(cline, ("logger")) == 0) {
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
                        xprint("Log ip length??\n");
                    } else {
                        for(int i=0;i<4;i++) {
                            lg_ip[i] = value[i];
                        }
                    }
                } else {
                    xprint("Log ip : ");
                    for(int i=0;i<4;i++) {
                        xprint_char(lg_ip[i]);
                        xprint(".");
                    }
                    xprint("\n");
                    xprint("\n");
                    showlog();
          }
#endif
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
/*
void checkcommand(void) {
//    if (cmdbytes != 0) { // we have a cmdline in progress
	while(cmdbytes == 0) {
	  UART2_read(uart1,&cc,1,&cmdbytes);
	}
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
//        UART2_read(uart1, &cc, 1, NULL);    // issue a starting read
//    } else {    // cmdbytes == 0
//        if(Txcount) {
//            if(Timer0 == 0) {
//                Timer0 = 60000;
//                Txcount--;
//            }
//        }
    }
//}
*/
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
#ifdef FSK4
                clearfsk4();
#endif
                switch(mode) {
                  case 0:
                    bitrate = 0xa0000;
                    deviation = 350;
                    rxBw = 100;
                    break;
                case 10:
                    bitrate = 0xa0000;
                    deviation = 500;
                    rxBw = 100;
                    break;
                case 11:
                    bitrate = 0xb0000;
                    deviation = 550;
                    rxBw = 100;
                  break;
                case 12:
                    bitrate = 0xc0000;
                    deviation = 600;
                    rxBw = 100;
                    break;
                case 13:
                    bitrate = 0xd0000;
                    deviation = 650;
                    rxBw = 101;
                    break;
                case 14:
                    bitrate = 0xe0000;
                    deviation = 700;
                    rxBw = 101;
                    break;
                case 15:
                    bitrate = 0xf0000;
                    deviation = 750;
                    rxBw = 101;
                    break;
                case 16:
                    bitrate = 0x100000;
                    deviation = 800;
                    rxBw = 102;
                    break;
                case 20:
                    bitrate = 0xa0000;
                    deviation = 167;
                    rxBw = 100;
#ifdef FSK4
                    setfsk4();
#endif
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


//void ReceiveonUARTcallbac1(UART2_Handle uart1, void *buffer, size_t count, void *userArg, int_fast16_t status)
//{
//    if (status == UART2_STATUS_SUCCESS) {
//        cmdbytes = count;
//    }   else {
//        while (1) {}
//    }
//}
