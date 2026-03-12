/* main.c — MC68000 Emulator entry point */
#include "common/types.h"
#include "common/log.h"
#include "machine/machine.h"
#include "gui/gui.h"
#include "devices/uart.h"
#include "devices/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *prog) {
    fprintf(stderr,
        "M68000 Emulator v0.1\n"
        "Usage: %s [options] <rom.bin>\n"
        "\n"
        "Options:\n"
        "  -d <disk.img>   Attach disk image\n"
        "  -c              Console-only mode (no GUI)\n"
        "  -v              Verbose logging\n"
        "  -h              Show this help\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *romPath  = NULL;
    const char *diskPath = NULL;
    bool consoleOnly = false;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) { printUsage(argv[0]); return 0; }
        else if (strcmp(argv[i], "-c") == 0) consoleOnly = true;
        else if (strcmp(argv[i], "-v") == 0) verbose = true;
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) diskPath = argv[++i];
        else if (argv[i][0] != '-') romPath = argv[i];
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!romPath) {
        printUsage(argv[0]);
        return 1;
    }

    logSetLevel(verbose ? LOG_TRACE : LOG_INFO);

    /* Create machine */
    Machine *machine = machineCreate();
    if (!machine) {
        fprintf(stderr, "Failed to create machine\n");
        return 1;
    }

    /* Load ROM */
    if (!machineLoadRom(machine, romPath)) {
        machineDestroy(machine);
        return 1;
    }

    /* Attach disk if provided */
    if (diskPath) {
        machineAttachDisk(machine, diskPath, false);
    }

    /* Reset to load initial SSP and PC from vector table */
    machineReset(machine);

    if (consoleOnly) {
        /* Console-only mode: just run and forward UART to stdout */
        logMessage(LOG_INFO, MOD_MACHINE, "Running in console mode. Press Ctrl-C to quit.");
        while (!machine->cpu.halted) {
            machineStep(machine);
            /* Pull UART output */
            u8 ch;
            while (uartPullOutput(machine->uart, &ch, 1) > 0) {
                putchar(ch);
                fflush(stdout);
            }
        }
        logMessage(LOG_INFO, MOD_MACHINE, "Emulator stopped. Total cycles: %llu",
                   (unsigned long long)machine->cpu.totalCycles);
    } else {
        /* GUI mode */
        GuiConfig guiCfg = {
            .width  = 640,
            .height = 480,
            .title  = "M68000 Emulator",
            .showConsole = true
        };
        GuiWindow *win = guiCreateWindow(&guiCfg);
        if (!win) {
            fprintf(stderr, "Failed to create GUI window\n");
            machineDestroy(machine);
            return 1;
        }

        u32 *smallbuf    = calloc(320 * 240, sizeof(u32));
        u32 *framebuffer = calloc(640 * 480, sizeof(u32));

        while (guiProcessEvents(win) && !machine->cpu.halted) {
            u64 frameStart = guiGetTimeMs();

            /* Run one frame worth of CPU cycles */
            machineRunFrame(machine);

            /* Render video at native 320x240 */
            videoRenderRGBA(machine->video, smallbuf, 320, 240);

            /* 2x nearest-neighbour upscale to 640x480 */
            for (int y = 0; y < 240; y++) {
                for (int x = 0; x < 320; x++) {
                    u32 p = smallbuf[y * 320 + x];
                    int dy = y * 2, dx = x * 2;
                    framebuffer[dy       * 640 + dx    ] = p;
                    framebuffer[dy       * 640 + dx + 1] = p;
                    framebuffer[(dy + 1) * 640 + dx    ] = p;
                    framebuffer[(dy + 1) * 640 + dx + 1] = p;
                }
            }

            guiUpdateFramebuffer(win, framebuffer, 640, 480);

            /* Forward UART output to console */
            char uartBuf[256];
            int uartLen = 0;
            u8 ch;
            while (uartPullOutput(machine->uart, &ch, 1) > 0 && uartLen < 255) {
                uartBuf[uartLen++] = (char)ch;
            }
            if (uartLen > 0) {
                guiConsoleWrite(win, uartBuf, uartLen);
            }

            guiPresent(win);

            /* Frame rate limiting (~60 fps) */
            u64 frameTime = guiGetTimeMs() - frameStart;
            if (frameTime < 16) guiSleepMs((u32)(16 - frameTime));
        }

        free(smallbuf);
        free(framebuffer);
        guiDestroyWindow(win);
    }

    logMessage(LOG_INFO, MOD_MACHINE, "Emulator stopped. Total cycles: %llu",
               (unsigned long long)machine->cpu.totalCycles);

    machineDestroy(machine);
    return 0;
}
