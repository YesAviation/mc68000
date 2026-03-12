/* ===========================================================================
 *  uart.c — UART implementation
 * =========================================================================== */
#include "devices/uart.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>

static u8 uartReadByte(Device *dev, u32 offset) {
    UartState *state = (UartState *)dev->data;
    switch (offset) {
        case UART_REG_STATUS:
            return state->status;
        case UART_REG_DATA:
            if (state->rxHead != state->rxTail) {
                u8 ch = state->rxBuffer[state->rxTail];
                state->rxTail = (state->rxTail + 1) & 255;
                if (state->rxHead == state->rxTail) {
                    state->status &= ~UART_STATUS_RXRDY;
                }
                return ch;
            }
            return 0;
        default:
            return 0;
    }
}

static u16 uartReadWord(Device *dev, u32 offset) {
    return (u16)uartReadByte(dev, offset);
}

static void uartWriteByte(Device *dev, u32 offset, u8 value) {
    UartState *state = (UartState *)dev->data;
    switch (offset) {
        case UART_REG_DATA:
            state->txBuffer[state->txHead] = value;
            state->txHead = (state->txHead + 1) & 255;
            break;
        case UART_REG_CONTROL:
            state->control = value;
            break;
        default:
            break;
    }
}

static void uartWriteWord(Device *dev, u32 offset, u16 value) {
    uartWriteByte(dev, offset, (u8)value);
}

static void uartReset(Device *dev) {
    UartState *state = (UartState *)dev->data;
    memset(state->rxBuffer, 0, sizeof(state->rxBuffer));
    memset(state->txBuffer, 0, sizeof(state->txBuffer));
    state->rxHead = state->rxTail = 0;
    state->txHead = state->txTail = 0;
    state->status = UART_STATUS_TXRDY | UART_STATUS_TXEMPTY;
    state->control = 0;
}

static int uartGetInterruptLevel(Device *dev) {
    UartState *state = (UartState *)dev->data;
    if (state->status & UART_STATUS_RXRDY) {
        return state->interruptLevel;
    }
    return 0;
}

static void uartDestroy(Device *dev) {
    free(dev->data);
    free(dev);
}

Device *uartCreate(u32 baseAddress, int interruptLevel) {
    Device *dev = calloc(1, sizeof(Device));
    UartState *state = calloc(1, sizeof(UartState));

    dev->name        = "UART";
    dev->baseAddress = baseAddress;
    dev->size        = 8;
    dev->data        = state;
    dev->readByte    = uartReadByte;
    dev->readWord    = uartReadWord;
    dev->writeByte   = uartWriteByte;
    dev->writeWord   = uartWriteWord;
    dev->reset       = uartReset;
    dev->destroy     = uartDestroy;
    dev->getInterruptLevel = uartGetInterruptLevel;

    state->interruptLevel = interruptLevel;
    state->status = UART_STATUS_TXRDY | UART_STATUS_TXEMPTY;

    return dev;
}

void uartPushInput(Device *dev, const u8 *data, int count) {
    UartState *state = (UartState *)dev->data;
    for (int i = 0; i < count; i++) {
        state->rxBuffer[state->rxHead] = data[i];
        state->rxHead = (state->rxHead + 1) & 255;
    }
    state->status |= UART_STATUS_RXRDY;
}

int uartPullOutput(Device *dev, u8 *buffer, int maxCount) {
    UartState *state = (UartState *)dev->data;
    int count = 0;
    while (state->txTail != state->txHead && count < maxCount) {
        buffer[count++] = state->txBuffer[state->txTail];
        state->txTail = (state->txTail + 1) & 255;
    }
    return count;
}
