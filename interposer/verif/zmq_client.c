#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdatomic.h>
#include <sched.h>
#include <stdbool.h>

#include "vpidpi/spsc_queue.h"

struct timeval stop_time, start_time;

int rxfd;
int txfd;

int rxptr = 0;
int txptr = 0;

spsc_queue* rxq;
spsc_queue* txq;

void umi_init(int rx_port, int tx_port) {
    // determine RX URI
    char rx_uri[128];
    sprintf(rx_uri, "/tmp/feeds-%d", rx_port);
    rxq = spsc_open(rx_uri);

    // determine TX URI
    char tx_uri[128];
    sprintf(tx_uri, "/tmp/feeds-%d", tx_port);
    txq = spsc_open(tx_uri);
}

void umi_recv(int* rbuf) {
    while (spsc_recv(rxq, rbuf, &rxptr) == 0){
        sched_yield();
    }
}

void umi_send(const int* sbuf) {
    while (spsc_send(txq, sbuf, &txptr) == 0) {
        sched_yield();
    }
}

void dut_send(const uint32_t data, const uint32_t addr){
    int buf[PACKET_SIZE] = {0};

    buf[ 4] = (addr >>  0) & 0xff;
    buf[ 5] = (addr >>  8) & 0xff;
    buf[ 6] = (addr >> 16) & 0xff;
    buf[ 7] = (addr >> 24) & 0xff;

    buf[12] = (data >>  0) & 0xff;
    buf[13] = (data >>  8) & 0xff;
    buf[14] = (data >> 16) & 0xff;
    buf[15] = (data >> 24) & 0xff;

    umi_send(buf);
}

void dut_recv(uint32_t* data, uint32_t* addr){
    int buf[PACKET_SIZE];

    umi_recv(buf);

    uint32_t laddr = 0;
    laddr |= (buf[ 4] & 0xff) <<  0;
    laddr |= (buf[ 5] & 0xff) <<  8;
    laddr |= (buf[ 6] & 0xff) << 16;
    laddr |= (buf[ 7] & 0xff) << 24;
    *addr = laddr;

    uint32_t ldata = 0;
    ldata |= (buf[12] & 0xff) <<  0;
    ldata |= (buf[13] & 0xff) <<  8;
    ldata |= (buf[14] & 0xff) << 16;
    ldata |= (buf[15] & 0xff) << 24;
    *data = ldata;
}

int main(int argc, char* argv[]) {
    int rx_port = 5556;
    int tx_port = 5555;
    char* binfile = "build/sw/hello.bin";
    if (argc >= 2) {
        rx_port = atoi(argv[1]);
    }
    if (argc >= 3) {
        tx_port = atoi(argv[2]);
    }
    if (argc >= 4) {
        binfile = argv[3];
    }
    
    umi_init(rx_port, tx_port);
    dut_send(0, 0x20000000);

    FILE *ptr;
    ptr = fopen(binfile, "rb");
    uint8_t buf[4];
    int nread;
    uint32_t waddr = 0;
    while ((nread = fread(buf, 1, 4, ptr)) > 0){
        // fill in extra values with zeros
        for (int i=nread; i<sizeof(buf); i++){
            buf[i] = 0;
        }

        // format as an integer
        uint32_t outgoing = 0;
        outgoing |= (buf[0] <<  0);
        outgoing |= (buf[1] <<  8);
        outgoing |= (buf[2] << 16);
        outgoing |= (buf[3] << 24);

        // write value
        dut_send(outgoing, waddr);

        waddr += 4;
    }
    dut_send(1, 0x20000000);
    uint16_t exit_code;
    uint32_t data, addr;
    while(true){
        dut_recv(&data, &addr);
        if (addr == 0x10000000) {
            printf("%c", data & 0xff);
            fflush(stdout);
        } else if (addr == 0x10000008) {
            uint16_t kind = data & 0xffff;
            if (kind == 0x3333) {
                exit_code = (data >> 16) & 0xffff;
                break;
            } else if (kind == 0x5555) {
                exit_code = 0;
                break;
            }
        }
    }
    return exit_code;
}