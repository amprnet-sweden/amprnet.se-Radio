#include <stdint.h>
struct tdmatable {
        char macaddr[6];
        char call[12];
        int     ttl;
        uint8_t mrssi;
        uint8_t hrssi;
        uint8_t volt;
};
struct conntable {
    uint8_t macaddr[6];
    int ttl;
};
struct tdmalist {
    uint8_t macaddr[6];  //5 mac adresses each 6 bytes
};
extern void dump_tdma_packet(uint8_t *buf, char len);
extern void showtdma(void);
extern void settdma(uint8_t * macaddr, uint8_t *call, uint8_t rssi, uint8_t volt);
extern void send_tdma_packet(void);
extern void tdma_ttl(void);
