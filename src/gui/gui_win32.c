/* gui_win32.c — Windows GDI GUI backend (stub) */
#ifdef _WIN32

#include "gui/gui.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* TODO: Full Win32 implementation using GDI/GDI+.
 * For now, provide stub implementations. */

struct GuiWindow {
    HWND hwnd;
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
    /* TODO: RegisterClass, CreateWindowEx */
    return win;
}

void guiDestroyWindow(GuiWindow *win) { free(win); }
bool guiProcessEvents(GuiWindow *win) { return !win->shouldClose; }
void guiUpdateFramebuffer(GuiWindow *win, const u32 *rgba, int w, int h) {
    (void)win; (void)rgba; (void)w; (void)h;
}
void guiPresent(GuiWindow *win) { (void)win; }
void guiConsoleWrite(GuiWindow *win, const char *text, int length) {
    (void)win; (void)text; (void)length;
}
int guiConsoleRead(GuiWindow *win, char *buf, int maxLen) {
    (void)win; (void)buf; (void)maxLen; return 0;
}
void guiSetKeyCallback(GuiWindow *win, GuiKeyCallback cb, void *userData) {
    win->keyCb = cb; win->keyUserData = userData;
}
void guiSetTitle(GuiWindow *win, const char *title) { (void)win; (void)title; }

u64 guiGetTimeMs(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (u64)(counter.QuadPart * 1000 / freq.QuadPart);
}
void guiSleepMs(u32 ms) { Sleep(ms); }

#endif /* _WIN32 */
