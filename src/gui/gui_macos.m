/* gui_macos.m — macOS Cocoa GUI backend */
#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "gui/gui.h"
#include <mach/mach_time.h>

/* ── GuiWindow struct ────────────────────────────────── */

struct GuiWindow {
    NSWindow       *nsWindow;
    NSImageView    *imageView;
    NSTextView     *consoleView;
    NSScrollView   *scrollView;
    bool            shouldClose;
    GuiKeyCallback  keyCb;
    void           *keyUserData;
    int             width;
    int             height;
};

/* ── Window delegate to catch close ──────────────────── */

@interface M68kWindowDelegate : NSObject <NSWindowDelegate>
@property (assign) GuiWindow *guiWin;
@end

@implementation M68kWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    self.guiWin->shouldClose = YES;
    return YES;
}
@end

/* ── Lifecycle ───────────────────────────────────────── */

GuiWindow *guiCreateWindow(const GuiConfig *config) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        GuiWindow *win = calloc(1, sizeof(GuiWindow));
        win->width  = config->width;
        win->height = config->height;

        int totalHeight = config->height + (config->showConsole ? 200 : 0);

        NSRect frame = NSMakeRect(100, 100, config->width, totalHeight);
        win->nsWindow = [[NSWindow alloc]
            initWithContentRect:frame
            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
            backing:NSBackingStoreBuffered
            defer:NO];

        [win->nsWindow setTitle:[NSString stringWithUTF8String:config->title]];

        /* Framebuffer image view */
        NSRect imgFrame = NSMakeRect(0, config->showConsole ? 200 : 0,
                                     config->width, config->height);
        win->imageView = [[NSImageView alloc] initWithFrame:imgFrame];
        [win->imageView setImageScaling:NSImageScaleProportionallyUpOrDown];
        [[win->nsWindow contentView] addSubview:win->imageView];

        /* Console text view */
        if (config->showConsole) {
            NSRect consoleFrame = NSMakeRect(0, 0, config->width, 200);
            win->scrollView = [[NSScrollView alloc] initWithFrame:consoleFrame];
            [win->scrollView setHasVerticalScroller:YES];
            win->consoleView = [[NSTextView alloc] initWithFrame:consoleFrame];
            [win->consoleView setEditable:NO];
            [win->consoleView setFont:[NSFont fontWithName:@"Menlo" size:12]];
            [win->consoleView setBackgroundColor:[NSColor blackColor]];
            [win->consoleView setTextColor:[NSColor greenColor]];
            [win->scrollView setDocumentView:win->consoleView];
            [[win->nsWindow contentView] addSubview:win->scrollView];
        }

        /* Delegate */
        M68kWindowDelegate *del = [[M68kWindowDelegate alloc] init];
        del.guiWin = win;
        [win->nsWindow setDelegate:del];

        [win->nsWindow makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        return win;
    }
}

void guiDestroyWindow(GuiWindow *win) {
    if (!win) return;
    @autoreleasepool {
        [win->nsWindow close];
        free(win);
    }
}

/* ── Events ──────────────────────────────────────────── */

bool guiProcessEvents(GuiWindow *win) {
    @autoreleasepool {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                               untilDate:nil
                               inMode:NSDefaultRunLoopMode
                               dequeue:YES])) {
            if ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp) {
                if (win->keyCb) {
                    GuiKeyEvent ke = {0};
                    ke.keyCode = [event keyCode];
                    ke.pressed = ([event type] == NSEventTypeKeyDown);
                    ke.shift   = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
                    ke.ctrl    = ([event modifierFlags] & NSEventModifierFlagControl) != 0;
                    ke.alt     = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
                    win->keyCb(&ke, win->keyUserData);
                }
            }
            [NSApp sendEvent:event];
        }
    }
    return !win->shouldClose;
}

/* ── Display ─────────────────────────────────────────── */

void guiUpdateFramebuffer(GuiWindow *win, const u32 *rgba, int width, int height) {
    @autoreleasepool {
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
            pixelsWide:width
            pixelsHigh:height
            bitsPerSample:8
            samplesPerPixel:4
            hasAlpha:YES
            isPlanar:NO
            colorSpaceName:NSCalibratedRGBColorSpace
            bytesPerRow:width * 4
            bitsPerPixel:32];

        memcpy([rep bitmapData], rgba, (size_t)(width * height * 4));

        NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
        [img addRepresentation:rep];
        [win->imageView setImage:img];
    }
}

void guiPresent(GuiWindow *win) {
    @autoreleasepool {
        [[win->nsWindow contentView] setNeedsDisplay:YES];
    }
}

/* ── Console ─────────────────────────────────────────── */

void guiConsoleWrite(GuiWindow *win, const char *text, int length) {
    if (!win->consoleView) return;
    @autoreleasepool {
        NSString *str = [[NSString alloc] initWithBytes:text length:(NSUInteger)length
                                               encoding:NSUTF8StringEncoding];
        NSTextStorage *ts = [win->consoleView textStorage];
        [ts beginEditing];
        [ts appendAttributedString:[[NSAttributedString alloc]
            initWithString:str
            attributes:@{
                NSForegroundColorAttributeName: [NSColor greenColor],
                NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:12]
            }]];
        [ts endEditing];
        [win->consoleView scrollRangeToVisible:NSMakeRange(ts.length, 0)];
    }
}

int guiConsoleRead(GuiWindow *win, char *buf, int maxLen) {
    (void)win; (void)buf; (void)maxLen;
    /* TODO: keyboard input queue */
    return 0;
}

/* ── Input ───────────────────────────────────────────── */

void guiSetKeyCallback(GuiWindow *win, GuiKeyCallback cb, void *userData) {
    win->keyCb = cb;
    win->keyUserData = userData;
}

void guiSetTitle(GuiWindow *win, const char *title) {
    @autoreleasepool {
        [win->nsWindow setTitle:[NSString stringWithUTF8String:title]];
    }
}

/* ── Timing ──────────────────────────────────────────── */

u64 guiGetTimeMs(void) {
    static mach_timebase_info_data_t info;
    if (info.denom == 0) mach_timebase_info(&info);
    u64 t = mach_absolute_time();
    return (t * info.numer / info.denom) / 1000000;
}

void guiSleepMs(u32 ms) {
    usleep(ms * 1000);
}

#endif /* __APPLE__ */
