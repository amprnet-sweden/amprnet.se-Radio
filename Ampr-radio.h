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
//#define LCD

#define PETH 0x00       // ethernet transport
#define PTXT 0x10       // text or actually uint8_t over serial line
#define PSER 0x30       //
#define PXXX 0x40
#define PPTP 0x50

#define CC1314R10

extern unsigned int Timer0, Timer1;
extern uint8_t m1,m2,m3,m4,m5,m6;
extern unsigned char myaddr;
extern unsigned char peeraddr;
extern unsigned char mode;
extern unsigned int freq, freqold;
extern char rssi;
extern unsigned int Runtime,Recd,Sent,Bad;
extern unsigned long RRbytes,RSbytes,ERbytes,ESbytes;
extern unsigned long ERpkts,ESpkts;
extern unsigned int freq, freqold;
extern unsigned int rfcollision;
extern int loopctr;
extern unsigned int eints;
extern int cmdbytes;
extern int LcdEna;
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
void LCD_init(void);
char init_ethernet(void);