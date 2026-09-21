
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
