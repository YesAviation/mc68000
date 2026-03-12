/* ===========================================================================
 *  uart.h — UART (serial) device, modeled after the MC68681 DUART
 * =========================================================================== */
#ifndef M68K_UART_H
#define M68K_UART_H

#include "devices/device.h"

/* UART register offsets */
#define UART_REG_STATUS   0x00  /* Status register (read) */
#define UART_REG_DATA     0x02  /* RX/TX data register */
#define UART_REG_CONTROL  0x04  /* Control register (write) */

/* Status bits */
#define UART_STATUS_RXRDY  BIT(0)  /* Receive data ready */
#define UART_STATUS_TXRDY  BIT(2)  /* Transmit ready */
#define UART_STATUS_TXEMPTY BIT(3) /* Transmit empty */

/* UART state */
typedef struct {
    u8  rxBuffer[256];
    u8  txBuffer[256];
    int rxHead, rxTail;
    int txHead, txTail;
    u8  status;
    u8  control;
    int interruptLevel;
} UartState;

/* Create a UART device */
Device *uartCreate(u32 baseAddress, int interruptLevel);

/* Feed input data to the UART (from host terminal) */
void uartPushInput(Device *dev, const u8 *data, int count);

/* Read output data from the UART (to host terminal) */
int uartPullOutput(Device *dev, u8 *buffer, int maxCount);

#endif /* M68K_UART_H */
