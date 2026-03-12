/* network.h — Simple network interface (loopback / tap) */
#ifndef M68K_NETWORK_H
#define M68K_NETWORK_H

#include "devices/device.h"

/*
 * Minimal Ethernet-like frame interface.
 *
 * Register map (word):
 *   +0x00  CTRL       (W)  — bit0=enable, bit1=rx IRQ enable, bit2=tx start
 *   +0x02  STATUS     (R)  — bit0=rx ready, bit1=tx done, bit2=error
 *   +0x04  RX_LEN     (R)  — received frame length (bytes)
 *   +0x06  TX_LEN     (W)  — transmit frame length
 *   +0x08  DMA_HI     (W)  — DMA address high word (shared RX/TX)
 *   +0x0A  DMA_LO     (W)  — DMA address low word
 *   +0x0C  MAC[0..5]  (R/W) — 6-byte MAC address
 *
 * DMA transfers frames between bus memory and internal buffer.
 */

#define NET_MAX_FRAME  1518
#define NET_DEVICE_SIZE 0x14

typedef struct {
    u16  ctrl;
    u16  status;
    u16  rxLen;
    u16  txLen;
    u32  dmaAddress;
    u8   mac[6];
    u8   rxBuf[NET_MAX_FRAME];
    u8   txBuf[NET_MAX_FRAME];
    int  interruptLevel;
    struct Bus *bus;
} NetworkState;

Device *networkCreate(u32 baseAddress, int interruptLevel);

/* Set bus reference for DMA */
void networkSetBus(Device *dev, struct Bus *bus);

/* Host injects a received frame */
void networkInjectFrame(Device *dev, const u8 *frame, u16 length);

/* Host pulls a transmitted frame (returns length, 0 if none) */
u16 networkPullFrame(Device *dev, u8 *outFrame, u16 maxLen);

#endif /* M68K_NETWORK_H */
