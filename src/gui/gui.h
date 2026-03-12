/* gui.h — Platform-agnostic GUI interface */
#ifndef M68K_GUI_H
#define M68K_GUI_H

#include "common/types.h"

/*
 * The GUI provides:
 *   - A window with a framebuffer display area
 *   - Keyboard and mouse input forwarded to the emulator
 *   - A serial console pane (UART I/O)
 *
 * Platform backends:
 *   macOS  → Cocoa / AppKit  (gui_macos.m)
 *   Linux  → X11             (gui_linux.c)
 *   Win32  → GDI             (gui_win32.c)
 */

typedef struct GuiWindow GuiWindow;

typedef struct {
    int         width;
    int         height;
    const char *title;
    bool        showConsole;   /* show serial console pane */
} GuiConfig;

/* Key event */
typedef struct {
    int  keyCode;
    bool pressed;
    bool shift;
    bool ctrl;
    bool alt;
} GuiKeyEvent;

/* Callback types */
typedef void (*GuiKeyCallback)(GuiKeyEvent *event, void *userData);

/* Lifecycle */
GuiWindow *guiCreateWindow(const GuiConfig *config);
void       guiDestroyWindow(GuiWindow *win);

/* Event processing — returns false if window was closed */
bool guiProcessEvents(GuiWindow *win);

/* Display */
void guiUpdateFramebuffer(GuiWindow *win, const u32 *rgba, int width, int height);
void guiPresent(GuiWindow *win);

/* Console */
void guiConsoleWrite(GuiWindow *win, const char *text, int length);
int  guiConsoleRead(GuiWindow *win, char *buf, int maxLen);

/* Input */
void guiSetKeyCallback(GuiWindow *win, GuiKeyCallback cb, void *userData);

/* Misc */
void guiSetTitle(GuiWindow *win, const char *title);
u64  guiGetTimeMs(void);
void guiSleepMs(u32 ms);

#endif /* M68K_GUI_H */
