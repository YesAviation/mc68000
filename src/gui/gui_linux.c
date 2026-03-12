/* gui_linux.c — Linux X11 GUI backend (stub) */
#ifndef __APPLE__
#ifndef _WIN32

#include "gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* TODO: Full X11 implementation using Xlib (no external deps).
 * For now, provide stub implementations so the project links. */

struct GuiWindow {
    int  width;
    int  height;
    bool shouldClose;
    GuiKeyCallback keyCb;
    void *keyUserData;
};

GuiWindow *guiCreateWindow(const GuiConfig *config) {
    GuiWindow *win = calloc(1, sizeof(GuiWindow));
    win->width  = config->width;
    win->height = config->height;
    /* TODO: XOpenDisplay, XCreateSimpleWindow, etc. */
    return win;
}

void guiDestroyWindow(GuiWindow *win) { free(win); }

bool guiProcessEvents(GuiWindow *win) {
    /* TODO: XNextEvent loop */
    return !win->shouldClose;
}

void guiUpdateFramebuffer(GuiWindow *win, const u32 *rgba, int width, int height) {
    (void)win; (void)rgba; (void)width; (void)height;
    /* TODO: XPutImage */
}

void guiPresent(GuiWindow *win) { (void)win; }

void guiConsoleWrite(GuiWindow *win, const char *text, int length) {
    (void)win; (void)text; (void)length;
}

int guiConsoleRead(GuiWindow *win, char *buf, int maxLen) {
    (void)win; (void)buf; (void)maxLen;
    return 0;
}

void guiSetKeyCallback(GuiWindow *win, GuiKeyCallback cb, void *userData) {
    win->keyCb = cb; win->keyUserData = userData;
}

void guiSetTitle(GuiWindow *win, const char *title) {
    (void)win; (void)title;
}

u64 guiGetTimeMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

void guiSleepMs(u32 ms) { usleep(ms * 1000); }

#endif /* _WIN32 */
#endif /* __APPLE__ */
