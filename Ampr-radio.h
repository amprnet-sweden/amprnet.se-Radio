#ifndef AMPR_RADIO_H
#define AMPR_RADIO_H

#include <stdint.h>

/* ampr radio definitions */
#define HAM23CMRADIO 1
// mode byte bit definitions
#define BYTE_ADDR 0x1  // we are using two bytes, dst, src, BC=0xff
#define TYPE_BYTE 0x2  // packet contains a type field
// type byte definitions
#define SEG_MASK 0x07   // bits 2-0 contain segment number
#define LAST_SEG 0x8    // if segmenting packets, last segment has this bit set, bits b2-b0 = seg number
#define TYPE_MASK 0xf0  // there is protocol space for 16 packet types
#define FINFLAG 0x8
#define ETHERNET 1
#define LCD
#define TSIZE 40
//#define SPISPEED 16000000

#define PETH 0x00       // ethernet transport
#define PTXT 0x10       // text or actually uint8_t over serial line
#define PSER 0x30       //
#define PXXX 0x40
#define PPTP 0x50
#define PTDMA 0x80

#define LOGPKT 5

#define TTDMA 5
#define TIDENT 6
#define TCONN 7
#define TACK 8
#define TNACK 9
#define TDMAPERIOD 2000

#define CC1314R10

extern unsigned int Timer0, Timer1, Timer_per, Timer_def, Timer_tdm;
extern uint8_t m1,m2,m3,m4,m5,m6;
extern uint8_t my_Q;
extern unsigned char myaddr;
extern unsigned char peeraddr;
extern unsigned char mode;
extern unsigned int freq, freqold;
extern char rssi;
extern unsigned int Runtime,Recd,Sent,Bad;
extern unsigned long RRbytes,RSbytes,ERbytes,ESbytes;
extern unsigned long ERpkts,ESpkts,TSpkts,TRpkts;
extern unsigned int freq, freqold;
extern unsigned int rfcollision;
extern int loopctr;
extern unsigned int eints;
extern int cmdbytes;
extern int LcdEna;
extern char version[];
extern uint8_t my_hwaddr[];
extern uint8_t my_ip[];
extern int current_defer;
extern char my_call[12];
extern int parchange;
extern int EthEna;
extern uint8_t bcaddr[6];
extern uint8_t last_radio[6];

//extern RF_Handle rfHandle;
//extern RF_CmdHandle rfPostHandle;

void xprint(char *buf);
void xprint_int(int a);
void xprint_char(char x);
void xprint_xchar(char x);
void xprint_long(long v);
void init_uart_1(void);
void checkcommand(void);
void Send_beacon(void);
void printMAC(void);
void xprint_schar(signed char x);
void SendPacket(uint8_t *msg, char cnt);
void log_from_queue(char * buffer, uint8_t count);
void decode_packet(char * buffer, uint8_t count);
void start_terminal(void);
void delay(int time);
void queue_uart(char * b, int c);
char GetRssi(void);
void whatpacket(uint8_t *pkt, char len);
void check_ethernet(void);
void dequeue_uart(void);
void start_terminal(void);
void RX_ON(void);
void RX_OFF(void);
void RF_XMIT(uint8_t *message, char count);
void sendack(char dseg);
void queue_eth(uint8_t *buffer,int count,uint8_t pnum);
int LCD_Begin(void);
void LCD_Print(char * string);
void SendText(uint8_t *buf, int count);
void settdma(uint8_t *macaddr, uint8_t *call, int value, uint8_t rssi, uint32_t volt);
void showtdma();
void get_NVS(uint8_t * buf);
void smeter(signed char rssi);
char init_ether(void);
void proc_tdma_packet(uint8_t * buffer, char len);
void proc_eth(uint8_t * buf,int count,char port);
void queue_idle_data(void);

void ampr_initQueue();
void getMAC(uint8_t* mac);
void xprint(char *buf);

#endif // AMPR_RADIO_H
