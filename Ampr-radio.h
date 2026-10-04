#ifndef AMPR_RADIO_H
#define AMPR_RADIO_H

#include <stdint.h>
#include <stdbool.h>
//#include <queue.h>


// #define TDDEBUG 1
/* ampr radio definitions */
#define HAM23CMRADIO 1
#define MEMLOG 1
#define LOGSIZE 2048
#define NETLOG 1

#define DHCPC 1     // if defined, includes dhcp code in ethproc.c
// #define N536RADIO 1 Set by build configuration
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
#define OLED
#define TSIZE 40
//#define SPISPEED 16000000
#define FSK4 1

#define PETH 0x00       // ethernet transport
#define PTXT 0x10       // text or actually uint8_t over serial line
#define PSER 0x30
#define PXXX 0x40
#define PPTP 0x50
#define PTDMA 0x80

#define LOGPKT 5
/* tdma definitions */
#define MASTER 1
#define SLAVE  0
//
#define MAXSLAVES 4 // number of slaves supported in one TDMA group
#define TSYNC 1     // tsync sent out by Master
#define TSYNCD 2    // provison to have a separate synch for Elected Master
#define TTDMA 5
#define TIDENT 6
#define TCONN 7
#define TDISC 8
#define TNACK 9
#define TSNACK 10     //provision for nack table for multiple nodes
#define TINVITE 11    // tell this is an invite slot
//
// define cstate possible values
#define CIDLE 0
#define CONNECTING 1
#define CONNECTED 2
#define DISCONNECTING 3
//#define TDMAPERIOD 2000
//#define TDMAPERIOD 500
#define TDMAPERIOD 15   // var 20
#define TTABTTL 25
#define CTABTTL 25
#define TTABSIZE 4

#define INVSLOT 2       // the timer controlling invite timeout
#define INVPERIOD 100    // invperiod is the number of tdma packets between invites, var 10


#define CC1314R10
#define Nsize 100

#define UBCOUNT 8
#define EBCOUNT 32      // number of buffers in queue
//#define EOBCOUNT 20
extern int TDMASENT;
extern int role;
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
extern unsigned long RRpkts,RSpkts,ERpkts,ESpkts,TSpkts,TRpkts,ARPreq,ARPans;
extern unsigned int freq, freqold;
extern unsigned int rfcollision;
extern int loopctr;
extern unsigned int eints;
extern int cmdbytes;
extern int LcdEna;
extern char version[];
extern uint8_t my_hwaddr[];
extern uint8_t my_ip[];
extern uint8_t lg_ip[];
extern int current_defer;
extern char my_call[12];
extern int parchange;
extern int EthEna;
extern uint8_t bcaddr[6];
extern uint8_t last_radio[6];
extern int debug,dropctr,retrena;
extern int tdelay;
extern int quedepth,rexmitctr;
extern char dropseg;
extern int quemax;
extern int ebcount[];
extern uint8_t ebnumber[];
extern int eidx,oidx,iidx,oidx;
extern unsigned long RRbytes,RSbytes,ERbytes,ESbytes,URbytes,USbytes;
extern unsigned long USpkts;
extern int ucount[UBCOUNT];
extern uint8_t ubuf[UBCOUNT][300];
extern int cstate;      // slave connection state
extern int autoconnect; //
extern int new_synch;   // tdma make master send new tdma list
extern int myslot;
extern uint16_t droppedEthPackets, droppedRadioPackets;
extern int ebufsused;
extern bool rebootRequest;
extern int dhcp_discovery(uint8_t *mac);
extern int radio_rec;
// pools and queues
extern uint16_t ampr_poolSize();
extern uint16_t ampr_ethQueueSize();
extern uint16_t ampr_QueueSize();
// debug variables for queues
extern int poolmin;
extern int radiomax;
extern int ethmax;

typedef enum
{
	PORT_ETH = 1,
	PORT_RADIO = 2,
} Port;
//extern RF_Handle rfHandle;
//extern RF_CmdHandle rfPostHandle;

void xprint(char *buf);
void xprint_int(int a);
void xprint_char(char x);
void xprint_xchar(char x);
void xprint_long(long v);
void xprintMAC(uint8_t * mac);
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
#ifdef OLED
#else
int LCD_Begin(void);
void LCD_Print(char * string);
#endif
void SendText(uint8_t *buf, int count);
void settdma(uint8_t * macaddr, uint8_t * call, uint8_t rssi, uint8_t volt);
void showtdma();
void showctab(void);
void showtlist(void);
void get_NVS(uint8_t * buf);
void smeter(signed char rssi);
char init_ether(void);
void proc_eth(uint8_t * buf,int count,char port);
void queue_idle_data(void);
void send_tdma_packet(void);
void proc_tdma_packet(uint8_t * buffer, char len);
void tdma_connect(void);
void ampr_initQueue();
void getMAC(uint8_t* mac);
void xprint(char *buf);
void dump_packet(uint8_t *buf,char blen);
int dequeue_eth(void);
void loginit(void);
void dolog(char *message, int size, int net);
void setfsk4(void);
void clearfsk4(void);
int dhcp_discovery(uint8_t *mac);
uint8_t getPHYCFGR(void);
void tdma_ttl(void);
void showlog(void);
#endif // AMPR_RADIO_H
