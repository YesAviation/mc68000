; ===========================================================================
;  kernel.s — MiniMint OS Kernel
;
;  Loaded by ROM bootloader to $000800.  Entry point is _start.
;
;  Memory map:
;    $000000-$0003FF   Exception vector table (1 KB, 256 vectors)
;    $000400-$0007FF   System variables / globals
;    $000800-$00FFFF   Kernel code + data
;    $0F0000-$0FFFFF   Stack (grows downward from $0FFFFF)
; ===========================================================================

; ── Hardware registers ──
UART_STATUS:    EQU $E00000
UART_DATA:      EQU $E00002

TIMER_COUNTER:  EQU $E00100
TIMER_CONTROL:  EQU $E00102

PARALLEL_A_DATA: EQU $E00300
PARALLEL_A_DIR:  EQU $E00301
PARALLEL_B_DATA: EQU $E00302
PARALLEL_B_DIR:  EQU $E00303
PARALLEL_CTRL:   EQU $E00304
PARALLEL_STATUS: EQU $E00305

VIDEO_CTRL:     EQU $E08000
VIDEO_STATUS:   EQU $E08002
VIDEO_PALETTE:  EQU $E08008
VIDEO_VRAM:     EQU $E09000

; ── Screen constants ──
SCREEN_W:       EQU 320
SCREEN_H:       EQU 240
FONT_W:         EQU 8
FONT_H:         EQU 8
MENUBAR_H:      EQU 12

; ── Palette indices ──
COL_BLACK:      EQU 0
COL_WHITE:      EQU 255
COL_GRAY_DESK:  EQU 170
COL_GRAY_LT:    EQU 200
COL_GRAY_DARK:  EQU 85
COL_TITLEBAR:   EQU 130

; ── Window layout ──
WIN_X:          EQU 30
WIN_Y:          EQU 20
WIN_W:          EQU 260
WIN_H:          EQU 200
WIN_TITLE_H:    EQU 12
WIN_TEXT_X:     EQU 34
WIN_TEXT_Y:     EQU 36

; ── System variables ($000400) ──
SYS_TICKS:      EQU $000400
SYS_KEYBUF:     EQU $000410
SYS_KEY_HEAD:   EQU $000450
SYS_KEY_TAIL:   EQU $000451
SYS_MOUSE_X:    EQU $000452
SYS_MOUSE_Y:    EQU $000454
SYS_MOUSE_BTN:  EQU $000456
SYS_TEXT_COL:   EQU $000458
SYS_TEXT_ROW:   EQU $00045A

KEYBUF_MASK:    EQU 63

; ── Console geometry (characters inside the window) ──
CON_COLS:       EQU 28
CON_ROWS:       EQU 21

; ===========================================================================
;  Kernel entry — jumped to by ROM bootloader
; ===========================================================================
_start:
    LEA     $0F0000, A7
    BSR.W   install_vectors
    BSR.W   init_sysvars

    LEA     msg_boot(PC), A0
    BSR.W   uart_puts

    BSR.W   video_init
    LEA     msg_video(PC), A0
    BSR.W   uart_puts

    BSR.W   input_init
    LEA     msg_input(PC), A0
    BSR.W   uart_puts

    BSR.W   timer_init
    LEA     msg_timer(PC), A0
    BSR.W   uart_puts

    ; ── Draw the desktop ──
    BSR.W   draw_desktop
    BSR.W   draw_menubar
    BSR.W   draw_window

    ; Print boot messages into the window console
    LEA     msg_wb1(PC), A0
    BSR.W   con_puts
    LEA     msg_wb2(PC), A0
    BSR.W   con_puts
    LEA     msg_wb3(PC), A0
    BSR.W   con_puts
    LEA     msg_wb4(PC), A0
    BSR.W   con_puts
    LEA     msg_wb5(PC), A0
    BSR.W   con_puts
    LEA     msg_wb6(PC), A0
    BSR.W   con_puts
    LEA     msg_wb7(PC), A0
    BSR.W   con_puts
    LEA     msg_wb8(PC), A0
    BSR.W   con_puts
    LEA     msg_wb9(PC), A0
    BSR.W   con_puts
    LEA     msg_wba(PC), A0
    BSR.W   con_puts

    ; Draw initial mouse cursor
    BSR.W   draw_cursor

    ; Enable VBlank IRQ
    MOVE.W  #$0003, VIDEO_CTRL

    LEA     msg_ready(PC), A0
    BSR.W   uart_puts

    ; Enable interrupts
    ANDI.W  #$F8FF, SR

    ; ── Main idle loop ──
main_loop:
    BSR.W   key_poll
    TST.B   D0
    BEQ.S   ml_nokey
    BSR.W   uart_putchar
    BSR.W   con_putchar
ml_nokey:
    BRA.S   main_loop


; ===========================================================================
;  VECTOR TABLE SETUP
; ===========================================================================
install_vectors:
    MOVEM.L D0/A0-A1, -(A7)

    LEA     $000000, A0
    LEA     default_handler(PC), A1
    MOVE.W  #255, D0
iv_loop:
    MOVE.L  A1, (A0)+
    DBRA    D0, iv_loop

    MOVE.L  #$000F0000, $000000
    MOVE.L  #$00000800, $000004

    LEA     panic_bus(PC), A1
    MOVE.L  A1, $000008
    LEA     panic_addr(PC), A1
    MOVE.L  A1, $00000C
    LEA     panic_illegal(PC), A1
    MOVE.L  A1, $000010
    LEA     panic_divzero(PC), A1
    MOVE.L  A1, $000014
    LEA     default_handler(PC), A1
    MOVE.L  A1, $000018
    MOVE.L  A1, $00001C
    LEA     panic_priv(PC), A1
    MOVE.L  A1, $000020

    LEA     isr_vblank(PC), A1
    MOVE.L  A1, $000064
    LEA     isr_keyboard(PC), A1
    MOVE.L  A1, $00006C
    LEA     isr_uart(PC), A1
    MOVE.L  A1, $000070
    LEA     isr_timer(PC), A1
    MOVE.L  A1, $000074
    LEA     trap_syscall(PC), A1
    MOVE.L  A1, $000080

    MOVEM.L (A7)+, D0/A0-A1
    RTS


; ===========================================================================
;  SYSTEM VARIABLES INIT
; ===========================================================================
init_sysvars:
    MOVEM.L D0/A0, -(A7)
    LEA     $000400, A0
    MOVE.W  #63, D0
isv_loop:
    CLR.L   (A0)+
    DBRA    D0, isv_loop
    MOVE.W  #160, SYS_MOUSE_X
    MOVE.W  #120, SYS_MOUSE_Y
    MOVE.W  #0, SYS_TEXT_COL
    MOVE.W  #0, SYS_TEXT_ROW
    MOVEM.L (A7)+, D0/A0
    RTS


; ===========================================================================
;  UART CONSOLE DRIVER
; ===========================================================================

uart_putchar:
    MOVEM.L D1, -(A7)
upc_wait:
    MOVE.W  UART_STATUS, D1
    BTST    #2, D1
    BEQ.S   upc_wait
    MOVE.W  D0, UART_DATA
    MOVEM.L (A7)+, D1
    RTS

uart_puts:
    MOVEM.L D0/A0, -(A7)
ups_loop:
    MOVE.B  (A0)+, D0
    BEQ.S   ups_done
    BSR.W   uart_putchar
    BRA.S   ups_loop
ups_done:
    MOVEM.L (A7)+, D0/A0
    RTS

uart_puthex8:
    MOVEM.L D0-D2, -(A7)
    MOVE.B  D0, D2
    LSR.B   #4, D0
    BSR.S   uh8_nibble
    MOVE.B  D2, D0
    ANDI.B  #$0F, D0
    BSR.S   uh8_nibble
    MOVEM.L (A7)+, D0-D2
    RTS
uh8_nibble:
    CMPI.B  #10, D0
    BCS.S   uh8_digit
    ADDI.B  #55, D0
    BRA.W   uart_putchar
uh8_digit:
    ADDI.B  #48, D0
    BRA.W   uart_putchar

uart_puthex16:
    MOVEM.L D0-D1, -(A7)
    MOVE.W  D0, D1
    LSR.W   #8, D0
    BSR.W   uart_puthex8
    MOVE.B  D1, D0
    BSR.W   uart_puthex8
    MOVEM.L (A7)+, D0-D1
    RTS

uart_puthex32:
    MOVEM.L D0-D1, -(A7)
    MOVE.L  D0, D1
    SWAP    D0
    BSR.W   uart_puthex16
    MOVE.W  D1, D0
    BSR.W   uart_puthex16
    MOVEM.L (A7)+, D0-D1
    RTS

uart_newline:
    MOVEM.L D0, -(A7)
    MOVE.B  #$0A, D0
    BSR.W   uart_putchar
    MOVEM.L (A7)+, D0
    RTS


; ===========================================================================
;  VIDEO DRIVER
; ===========================================================================
video_init:
    MOVEM.L D0-D2/A0, -(A7)

    ; Build grayscale palette: index N -> gray level N
    LEA     VIDEO_PALETTE, A0
    MOVE.W  #255, D0
vpal_loop:
    MOVE.W  D0, D1
    ANDI.W  #$FF, D1
    MOVE.W  D1, D2
    LSR.W   #3, D2
    MOVE.W  D2, -(A7)
    LSL.W   #8, D2
    LSL.W   #3, D2
    MOVE.W  D1, D1
    LSR.W   #2, D1
    LSL.W   #5, D1
    OR.W    D1, D2
    MOVE.W  (A7)+, D1
    OR.W    D1, D2
    MOVE.W  D2, (A0)+
    DBRA    D0, vpal_loop

    ; Force index 0 = black, index 255 = white
    MOVE.W  #$0000, VIDEO_PALETTE
    LEA     VIDEO_PALETTE, A0
    MOVE.W  #$FFFF, 510(A0)

    ; Clear VRAM
    LEA     VIDEO_VRAM, A0
    MOVE.W  #19199, D0
vclr_loop:
    CLR.L   (A0)+
    DBRA    D0, vclr_loop

    ; Enable display
    MOVE.W  #$0001, VIDEO_CTRL

    MOVEM.L (A7)+, D0-D2/A0
    RTS


; ===========================================================================
;  FRAMEBUFFER DRAWING ROUTINES
; ===========================================================================

; ── fb_pixel: D0.W=X, D1.W=Y, D2.B=colour ──
fb_pixel:
    MOVEM.L D3/A0, -(A7)
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D3
    MULU    #SCREEN_W, D3
    ADDA.L  D3, A0
    ADDA.W  D0, A0
    MOVE.B  D2, (A0)
    MOVEM.L (A7)+, D3/A0
    RTS

; ── fb_hline: D0.W=X, D1.W=Y, D2.W=width, D3.B=colour ──
fb_hline:
    MOVEM.L D0/D4/A0, -(A7)
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D4
    MULU    #SCREEN_W, D4
    ADDA.L  D4, A0
    ADDA.W  D0, A0
    MOVE.W  D2, D4
    SUBQ.W  #1, D4
    BMI.S   fhl_done
fhl_loop:
    MOVE.B  D3, (A0)+
    DBRA    D4, fhl_loop
fhl_done:
    MOVEM.L (A7)+, D0/D4/A0
    RTS

; ── fb_vline: D0.W=X, D1.W=Y, D2.W=height, D3.B=colour ──
fb_vline:
    MOVEM.L D4/A0, -(A7)
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D4
    MULU    #SCREEN_W, D4
    ADDA.L  D4, A0
    ADDA.W  D0, A0
    MOVE.W  D2, D4
    SUBQ.W  #1, D4
    BMI.S   fvl_done
fvl_loop:
    MOVE.B  D3, (A0)
    ADDA.W  #SCREEN_W, A0
    DBRA    D4, fvl_loop
fvl_done:
    MOVEM.L (A7)+, D4/A0
    RTS

; ── fb_fill_rect: D0.W=X, D1.W=Y, D2.W=W, D3.W=H, D4.B=colour ──
fb_fill_rect:
    MOVEM.L D0-D5/A0, -(A7)
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D5
    MULU    #SCREEN_W, D5
    ADDA.L  D5, A0
    ADDA.W  D0, A0
    SUBQ.W  #1, D3
    BMI.S   ffr_done
ffr_row:
    MOVE.W  D2, D5
    SUBQ.W  #1, D5
    MOVE.L  A0, -(A7)
ffr_col:
    MOVE.B  D4, (A0)+
    DBRA    D5, ffr_col
    MOVE.L  (A7)+, A0
    ADDA.W  #SCREEN_W, A0
    DBRA    D3, ffr_row
ffr_done:
    MOVEM.L (A7)+, D0-D5/A0
    RTS

; ── fb_draw_char: D0.W=X, D1.W=Y, D2.B=ASCII, D3.B=fg, D4.B=bg ──
fb_draw_char:
    MOVEM.L D0-D7/A0-A2, -(A7)
    MOVEQ   #0, D5
    MOVE.B  D2, D5
    SUBI.W  #32, D5
    BMI.W   fdc_done
    CMPI.W  #95, D5
    BHI.W   fdc_done
    LSL.W   #3, D5
    LEA     font_8x8(PC), A1
    ADDA.W  D5, A1
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D5
    MULU    #SCREEN_W, D5
    ADDA.L  D5, A0
    ADDA.W  D0, A0
    MOVEQ   #7, D6
fdc_row:
    MOVE.B  (A1)+, D7
    MOVEQ   #7, D5
fdc_bit:
    BTST    D5, D7
    BEQ.S   fdc_bg
    MOVE.B  D3, (A0)+
    BRA.S   fdc_next
fdc_bg:
    MOVE.B  D4, (A0)+
fdc_next:
    DBRA    D5, fdc_bit
    LEA     312(A0), A0
    DBRA    D6, fdc_row
fdc_done:
    MOVEM.L (A7)+, D0-D7/A0-A2
    RTS

; ── fb_draw_string: D0.W=X, D1.W=Y, A0=string, D3.B=fg, D4.B=bg ──
fb_draw_string:
    MOVEM.L D0-D2/A0, -(A7)
fds_loop:
    MOVE.B  (A0)+, D2
    BEQ.S   fds_done
    BSR.W   fb_draw_char
    ADDI.W  #FONT_W, D0
    BRA.S   fds_loop
fds_done:
    MOVEM.L (A7)+, D0-D2/A0
    RTS


; ===========================================================================
;  DESKTOP DRAWING
; ===========================================================================
draw_desktop:
    MOVEM.L D0-D3/A0, -(A7)

    ; Fill screen with checkerboard dither pattern
    LEA     VIDEO_VRAM, A0
    MOVEQ   #0, D0
    MOVE.W  #SCREEN_H-1, D1
dd_yloop:
    MOVE.W  #SCREEN_W-1, D2
dd_xloop:
    MOVE.W  D0, D3
    ADD.W   D1, D3
    ANDI.W  #1, D3
    BEQ.S   dd_lt
    MOVE.B  #COL_GRAY_DESK, (A0)+
    BRA.S   dd_xnext
dd_lt:
    MOVE.B  #COL_GRAY_LT, (A0)+
dd_xnext:
    ADDQ.W  #1, D0
    DBRA    D2, dd_xloop
    MOVEQ   #0, D0
    DBRA    D1, dd_yloop

    MOVEM.L (A7)+, D0-D3/A0
    RTS


; ── Draw menu bar ──
draw_menubar:
    MOVEM.L D0-D4/A0, -(A7)

    ; White menu bar
    MOVE.W  #0, D0
    MOVE.W  #0, D1
    MOVE.W  #SCREEN_W, D2
    MOVE.W  #MENUBAR_H, D3
    MOVE.B  #COL_WHITE, D4
    BSR.W   fb_fill_rect

    ; Black line under menu bar
    MOVE.W  #0, D0
    MOVE.W  #MENUBAR_H, D1
    MOVE.W  #SCREEN_W, D2
    MOVE.B  #COL_BLACK, D3
    BSR.W   fb_hline

    ; Logo box (apple-like)
    MOVE.W  #4, D0
    MOVE.W  #2, D1
    MOVE.W  #8, D2
    MOVE.W  #8, D3
    MOVE.B  #COL_BLACK, D4
    BSR.W   fb_fill_rect

    ; "File"
    MOVE.W  #16, D0
    MOVE.W  #2, D1
    LEA     str_file(PC), A0
    MOVE.B  #COL_BLACK, D3
    MOVE.B  #COL_WHITE, D4
    BSR.W   fb_draw_string

    ; "Edit"
    MOVE.W  #56, D0
    MOVE.W  #2, D1
    LEA     str_edit(PC), A0
    BSR.W   fb_draw_string

    ; "View"
    MOVE.W  #96, D0
    MOVE.W  #2, D1
    LEA     str_view(PC), A0
    BSR.W   fb_draw_string

    ; "Special"
    MOVE.W  #136, D0
    MOVE.W  #2, D1
    LEA     str_spec(PC), A0
    BSR.W   fb_draw_string

    MOVEM.L (A7)+, D0-D4/A0
    RTS


; ── Draw a window ──
draw_window:
    MOVEM.L D0-D4/A0, -(A7)

    ; ── Drop shadow ──
    MOVE.W  #WIN_X+WIN_W, D0
    MOVE.W  #WIN_Y+2, D1
    MOVE.W  #WIN_H, D2
    MOVE.B  #COL_GRAY_DARK, D3
    BSR.W   fb_vline
    MOVE.W  #WIN_X+WIN_W+1, D0
    BSR.W   fb_vline
    MOVE.W  #WIN_X+2, D0
    MOVE.W  #WIN_Y+WIN_H, D1
    MOVE.W  #WIN_W, D2
    MOVE.B  #COL_GRAY_DARK, D3
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+WIN_H+1, D1
    BSR.W   fb_hline

    ; ── Window border ──
    MOVE.W  #WIN_X, D0
    MOVE.W  #WIN_Y, D1
    MOVE.W  #WIN_W, D2
    MOVE.B  #COL_BLACK, D3
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+WIN_H-1, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_X, D0
    MOVE.W  #WIN_Y, D1
    MOVE.W  #WIN_H, D2
    BSR.W   fb_vline
    MOVE.W  #WIN_X+WIN_W-1, D0
    MOVE.W  #WIN_Y, D1
    BSR.W   fb_vline

    ; ── Title bar fill ──
    MOVE.W  #WIN_X+1, D0
    MOVE.W  #WIN_Y+1, D1
    MOVE.W  #WIN_W-2, D2
    MOVE.W  #WIN_TITLE_H, D3
    MOVE.B  #COL_TITLEBAR, D4
    BSR.W   fb_fill_rect

    ; Title bar stripes (classic Mac style)
    MOVE.W  #WIN_X+14, D0
    MOVE.W  #WIN_Y+2, D1
    MOVE.W  #WIN_W-28, D2
    MOVE.B  #COL_BLACK, D3
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+4, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+6, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+8, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+10, D1
    BSR.W   fb_hline

    MOVE.W  #WIN_X+14, D0
    MOVE.W  #WIN_Y+3, D1
    MOVE.W  #WIN_W-28, D2
    MOVE.B  #COL_WHITE, D3
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+5, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+7, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+9, D1
    BSR.W   fb_hline

    ; Close box
    MOVE.W  #WIN_X+3, D0
    MOVE.W  #WIN_Y+3, D1
    MOVE.W  #8, D2
    MOVE.W  #8, D3
    MOVE.B  #COL_WHITE, D4
    BSR.W   fb_fill_rect
    MOVE.W  #WIN_X+3, D0
    MOVE.W  #WIN_Y+3, D1
    MOVE.W  #8, D2
    MOVE.B  #COL_BLACK, D3
    BSR.W   fb_hline
    MOVE.W  #WIN_Y+10, D1
    BSR.W   fb_hline
    MOVE.W  #WIN_X+3, D0
    MOVE.W  #WIN_Y+3, D1
    MOVE.W  #8, D2
    BSR.W   fb_vline
    MOVE.W  #WIN_X+10, D0
    BSR.W   fb_vline

    ; ── Line under title bar ──
    MOVE.W  #WIN_X+1, D0
    MOVE.W  #WIN_Y+WIN_TITLE_H+1, D1
    MOVE.W  #WIN_W-2, D2
    MOVE.B  #COL_BLACK, D3
    BSR.W   fb_hline

    ; ── Title text ──
    MOVE.W  #WIN_X+90, D0
    MOVE.W  #WIN_Y+3, D1
    LEA     str_wtitle(PC), A0
    MOVE.B  #COL_WHITE, D3
    MOVE.B  #COL_TITLEBAR, D4
    BSR.W   fb_draw_string

    ; ── Content area (white) ──
    MOVE.W  #WIN_X+1, D0
    MOVE.W  #WIN_Y+WIN_TITLE_H+2, D1
    MOVE.W  #WIN_W-2, D2
    MOVE.W  #WIN_H-WIN_TITLE_H-3, D3
    MOVE.B  #COL_WHITE, D4
    BSR.W   fb_fill_rect

    MOVEM.L (A7)+, D0-D4/A0
    RTS


; ===========================================================================
;  WINDOW CONSOLE — text output inside the boot window
; ===========================================================================

con_putchar:
    MOVEM.L D0-D4/A0, -(A7)
    CMPI.B  #$0A, D0
    BEQ.S   cpc_nl

    MOVE.B  D0, D2
    MOVE.W  SYS_TEXT_COL, D0
    MULU    #FONT_W, D0
    ADDI.W  #WIN_TEXT_X, D0
    MOVE.W  SYS_TEXT_ROW, D1
    MULU    #FONT_H, D1
    ADDI.W  #WIN_TEXT_Y, D1
    MOVE.B  #COL_BLACK, D3
    MOVE.B  #COL_WHITE, D4
    BSR.W   fb_draw_char

    MOVE.W  SYS_TEXT_COL, D0
    ADDQ.W  #1, D0
    CMPI.W  #CON_COLS, D0
    BCS.S   cpc_sc
    MOVEQ   #0, D0
    MOVE.W  D0, SYS_TEXT_COL
    BRA.S   cpc_adv

cpc_sc:
    MOVE.W  D0, SYS_TEXT_COL
    BRA.S   cpc_done

cpc_nl:
    MOVE.W  #0, SYS_TEXT_COL
cpc_adv:
    MOVE.W  SYS_TEXT_ROW, D0
    ADDQ.W  #1, D0
    CMPI.W  #CON_ROWS, D0
    BCS.S   cpc_sr
    MOVEQ   #0, D0
cpc_sr:
    MOVE.W  D0, SYS_TEXT_ROW

cpc_done:
    MOVEM.L (A7)+, D0-D4/A0
    RTS

con_puts:
    MOVEM.L D0/A0, -(A7)
cps_loop:
    MOVE.B  (A0)+, D0
    BEQ.S   cps_done
    BSR.W   con_putchar
    BRA.S   cps_loop
cps_done:
    MOVEM.L (A7)+, D0/A0
    RTS


; ===========================================================================
;  MOUSE CURSOR — XOR arrow
; ===========================================================================

draw_cursor:
    MOVEM.L D0-D5/A0-A1, -(A7)
    LEA     cursor_data(PC), A1
    MOVE.W  SYS_MOUSE_X, D0
    MOVE.W  SYS_MOUSE_Y, D1
    MOVEQ   #9, D5
dc_row:
    MOVE.W  (A1)+, D4
    MOVEQ   #9, D3
    MOVE.W  D0, D2
dc_col:
    BTST    D3, D4
    BEQ.S   dc_skip
    CMP.W   #SCREEN_W, D2
    BCC.S   dc_skip
    CMP.W   #SCREEN_H, D1
    BCC.S   dc_skip
    MOVEM.L D0-D1, -(A7)
    LEA     VIDEO_VRAM, A0
    MOVE.W  D1, D0
    MULU    #SCREEN_W, D0
    ADDA.L  D0, A0
    ADDA.W  D2, A0
    MOVE.B  (A0), D1
    EORI.B  #$FF, D1
    MOVE.B  D1, (A0)
    MOVEM.L (A7)+, D0-D1
dc_skip:
    ADDQ.W  #1, D2
    DBRA    D3, dc_col
    ADDQ.W  #1, D1
    DBRA    D5, dc_row
    MOVEM.L (A7)+, D0-D5/A0-A1
    RTS

cursor_data:
    DC.W %1000000000
    DC.W %1100000000
    DC.W %1110000000
    DC.W %1111000000
    DC.W %1111100000
    DC.W %1111110000
    DC.W %1111111000
    DC.W %1111000000
    DC.W %1001100000
    DC.W %0000110000


; ===========================================================================
;  INPUT DRIVER
; ===========================================================================
input_init:
    MOVE.B  #$00, PARALLEL_A_DIR
    MOVE.B  #$00, PARALLEL_B_DIR
    MOVE.B  #$02, PARALLEL_CTRL
    RTS

key_poll:
    MOVE.B  SYS_KEY_HEAD, D0
    CMP.B   SYS_KEY_TAIL, D0
    BEQ.S   kp_empty
    MOVEM.L D1/A0, -(A7)
    LEA     SYS_KEYBUF, A0
    MOVEQ   #0, D1
    MOVE.B  SYS_KEY_TAIL, D1
    MOVE.B  0(A0,D1.W), D0
    ADDQ.B  #1, D1
    ANDI.B  #KEYBUF_MASK, D1
    MOVE.B  D1, SYS_KEY_TAIL
    MOVEM.L (A7)+, D1/A0
    RTS
kp_empty:
    MOVEQ   #0, D0
    RTS


; ===========================================================================
;  TIMER DRIVER
; ===========================================================================
timer_init:
    MOVE.W  #$FFFF, TIMER_COUNTER
    MOVE.W  #$0001, TIMER_CONTROL
    RTS


; ===========================================================================
;  INTERRUPT SERVICE ROUTINES
; ===========================================================================

isr_vblank:
    MOVEM.L D0, -(A7)
    MOVE.W  #$0001, VIDEO_STATUS
    ADDQ.L  #1, SYS_TICKS
    MOVEM.L (A7)+, D0
    RTE

isr_timer:
    MOVEM.L D0, -(A7)
    MOVE.W  #$0001, TIMER_CONTROL
    MOVEM.L (A7)+, D0
    RTE

isr_uart:
    MOVEM.L D0-D1/A0, -(A7)
    MOVE.W  UART_STATUS, D0
    BTST    #0, D0
    BEQ.S   isr_uart_done
    MOVE.W  UART_DATA, D0
    ANDI.W  #$00FF, D0
    LEA     SYS_KEYBUF, A0
    MOVEQ   #0, D1
    MOVE.B  SYS_KEY_HEAD, D1
    MOVE.B  D0, 0(A0,D1.W)
    ADDQ.B  #1, D1
    ANDI.B  #KEYBUF_MASK, D1
    MOVE.B  D1, SYS_KEY_HEAD
isr_uart_done:
    MOVEM.L (A7)+, D0-D1/A0
    RTE

isr_keyboard:
    MOVEM.L D0-D1/A0, -(A7)
    MOVE.B  PARALLEL_A_DATA, D0
    MOVE.B  #$03, PARALLEL_STATUS
    TST.B   D0
    BEQ.S   isr_kb_done
    LEA     SYS_KEYBUF, A0
    MOVEQ   #0, D1
    MOVE.B  SYS_KEY_HEAD, D1
    MOVE.B  D0, 0(A0,D1.W)
    ADDQ.B  #1, D1
    ANDI.B  #KEYBUF_MASK, D1
    MOVE.B  D1, SYS_KEY_HEAD
isr_kb_done:
    MOVEM.L (A7)+, D0-D1/A0
    RTE


; ===========================================================================
;  TRAP #0 SYSCALL DISPATCHER
; ===========================================================================
trap_syscall:
    CMPI.W  #0, D0
    BEQ.W   sys_exit
    CMPI.W  #1, D0
    BEQ.W   sys_putchar
    CMPI.W  #2, D0
    BEQ.W   sys_getchar
    RTE

sys_exit:
    STOP    #$2700
    RTE

sys_putchar:
    MOVEM.L D0, -(A7)
    MOVE.B  D1, D0
    BSR.W   uart_putchar
    MOVEM.L (A7)+, D0
    RTE

sys_getchar:
sgc_wait:
    BSR.W   key_poll
    TST.B   D0
    BEQ.S   sgc_wait
    RTE


; ===========================================================================
;  PANIC HANDLERS
; ===========================================================================

panic_bus:
    LEA     pmsg_bus(PC), A0
    BRA.S   panic_common

panic_addr:
    LEA     pmsg_addr(PC), A0
    BRA.S   panic_common

panic_illegal:
    LEA     pmsg_ill(PC), A0
    BRA.S   panic_common

panic_divzero:
    LEA     pmsg_div(PC), A0
    BRA.S   panic_common

panic_priv:
    LEA     pmsg_priv(PC), A0
    BRA.S   panic_common

panic_common:
    ORI.W   #$0700, SR
    MOVEM.L D0/A0, -(A7)
    LEA     pmsg_hdr(PC), A0
    BSR.W   uart_puts
    MOVEM.L (A7)+, D0/A0
    BSR.W   uart_puts
    BSR.W   uart_newline
    LEA     pmsg_d03(PC), A0
    BSR.W   uart_puts
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D1, D0
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D2, D0
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D3, D0
    BSR.W   uart_puthex32
    BSR.W   uart_newline
    LEA     pmsg_halt(PC), A0
    BSR.W   uart_puts
    STOP    #$2700


default_handler:
    RTE


; ===========================================================================
;  STRING DATA
; ===========================================================================

msg_boot:
    DC.B "MiniMint Kernel v0.1", $0A
    DC.B "Initialising...", $0A
    DC.B "  UART:  OK", $0A, 0
    EVEN

msg_video:
    DC.B "  Video: 320x240 OK", $0A, 0
    EVEN

msg_input:
    DC.B "  Input: OK", $0A, 0
    EVEN

msg_timer:
    DC.B "  Timer: OK", $0A, 0
    EVEN

msg_ready:
    DC.B $0A, "MiniMint ready.", $0A, 0
    EVEN

str_file:
    DC.B "File", 0
    EVEN
str_edit:
    DC.B "Edit", 0
    EVEN
str_view:
    DC.B "View", 0
    EVEN
str_spec:
    DC.B "Special", 0
    EVEN

str_wtitle:
    DC.B "Console", 0
    EVEN

msg_wb1:
    DC.B "MiniMint v0.1", $0A, 0
    EVEN
msg_wb2:
    DC.B "MC68000 @ 8 MHz", $0A, 0
    EVEN
msg_wb3:
    DC.B "1024 KB RAM", $0A, 0
    EVEN
msg_wb4:
    DC.B $0A, 0
    EVEN
msg_wb5:
    DC.B "Initialising...", $0A, 0
    EVEN
msg_wb6:
    DC.B "  UART:   OK", $0A, 0
    EVEN
msg_wb7:
    DC.B "  Video:  320x240", $0A, 0
    EVEN
msg_wb8:
    DC.B "  Input:  OK", $0A, 0
    EVEN
msg_wb9:
    DC.B "  Timer:  OK", $0A, 0
    EVEN
msg_wba:
    DC.B $0A, "Ready.", $0A, 0
    EVEN

pmsg_hdr:
    DC.B $0A, "*** KERNEL PANIC ***", $0A
    DC.B "Exception: ", 0
    EVEN

pmsg_bus:
    DC.B "Bus Error", 0
    EVEN

pmsg_addr:
    DC.B "Address Error", 0
    EVEN

pmsg_ill:
    DC.B "Illegal Instruction", 0
    EVEN

pmsg_div:
    DC.B "Division by Zero", 0
    EVEN

pmsg_priv:
    DC.B "Privilege Violation", 0
    EVEN

pmsg_d03:
    DC.B "D0-D3: ", 0
    EVEN

pmsg_halt:
    DC.B "System halted.", $0A, 0
    EVEN


; ===========================================================================
;  8x8 BITMAP FONT — ASCII 32..126 (95 glyphs, 760 bytes)
;  Each glyph: 8 bytes, one per row, MSB = leftmost pixel.
; ===========================================================================
font_8x8:
    ; Space (32)
    DC.B $00,$00,$00,$00,$00,$00,$00,$00
    ; ! (33)
    DC.B $18,$18,$18,$18,$18,$00,$18,$00
    ; " (34)
    DC.B $6C,$6C,$6C,$00,$00,$00,$00,$00
    ; # (35)
    DC.B $6C,$6C,$FE,$6C,$FE,$6C,$6C,$00
    ; $ (36)
    DC.B $18,$7E,$C0,$7C,$06,$FC,$18,$00
    ; % (37)
    DC.B $00,$C6,$CC,$18,$30,$66,$C6,$00
    ; & (38)
    DC.B $38,$6C,$38,$76,$DC,$CC,$76,$00
    ; ' (39)
    DC.B $18,$18,$30,$00,$00,$00,$00,$00
    ; ( (40)
    DC.B $0C,$18,$30,$30,$30,$18,$0C,$00
    ; ) (41)
    DC.B $30,$18,$0C,$0C,$0C,$18,$30,$00
    ; * (42)
    DC.B $00,$66,$3C,$FF,$3C,$66,$00,$00
    ; + (43)
    DC.B $00,$18,$18,$7E,$18,$18,$00,$00
    ; , (44)
    DC.B $00,$00,$00,$00,$00,$18,$18,$30
    ; - (45)
    DC.B $00,$00,$00,$7E,$00,$00,$00,$00
    ; . (46)
    DC.B $00,$00,$00,$00,$00,$18,$18,$00
    ; / (47)
    DC.B $06,$0C,$18,$30,$60,$C0,$80,$00
    ; 0 (48)
    DC.B $7C,$C6,$CE,$D6,$E6,$C6,$7C,$00
    ; 1 (49)
    DC.B $18,$38,$18,$18,$18,$18,$7E,$00
    ; 2 (50)
    DC.B $7C,$C6,$06,$1C,$30,$60,$FE,$00
    ; 3 (51)
    DC.B $7C,$C6,$06,$3C,$06,$C6,$7C,$00
    ; 4 (52)
    DC.B $1C,$3C,$6C,$CC,$FE,$0C,$0C,$00
    ; 5 (53)
    DC.B $FE,$C0,$FC,$06,$06,$C6,$7C,$00
    ; 6 (54)
    DC.B $38,$60,$C0,$FC,$C6,$C6,$7C,$00
    ; 7 (55)
    DC.B $FE,$C6,$0C,$18,$30,$30,$30,$00
    ; 8 (56)
    DC.B $7C,$C6,$C6,$7C,$C6,$C6,$7C,$00
    ; 9 (57)
    DC.B $7C,$C6,$C6,$7E,$06,$0C,$78,$00
    ; : (58)
    DC.B $00,$18,$18,$00,$00,$18,$18,$00
    ; ; (59)
    DC.B $00,$18,$18,$00,$00,$18,$18,$30
    ; < (60)
    DC.B $0C,$18,$30,$60,$30,$18,$0C,$00
    ; = (61)
    DC.B $00,$00,$7E,$00,$7E,$00,$00,$00
    ; > (62)
    DC.B $60,$30,$18,$0C,$18,$30,$60,$00
    ; ? (63)
    DC.B $7C,$C6,$0C,$18,$18,$00,$18,$00
    ; @ (64)
    DC.B $7C,$C6,$DE,$DE,$DC,$C0,$7C,$00
    ; A (65)
    DC.B $38,$6C,$C6,$C6,$FE,$C6,$C6,$00
    ; B (66)
    DC.B $FC,$C6,$C6,$FC,$C6,$C6,$FC,$00
    ; C (67)
    DC.B $7C,$C6,$C0,$C0,$C0,$C6,$7C,$00
    ; D (68)
    DC.B $F8,$CC,$C6,$C6,$C6,$CC,$F8,$00
    ; E (69)
    DC.B $FE,$C0,$C0,$F8,$C0,$C0,$FE,$00
    ; F (70)
    DC.B $FE,$C0,$C0,$F8,$C0,$C0,$C0,$00
    ; G (71)
    DC.B $7C,$C6,$C0,$CE,$C6,$C6,$7E,$00
    ; H (72)
    DC.B $C6,$C6,$C6,$FE,$C6,$C6,$C6,$00
    ; I (73)
    DC.B $7E,$18,$18,$18,$18,$18,$7E,$00
    ; J (74)
    DC.B $1E,$06,$06,$06,$C6,$C6,$7C,$00
    ; K (75)
    DC.B $C6,$CC,$D8,$F0,$D8,$CC,$C6,$00
    ; L (76)
    DC.B $C0,$C0,$C0,$C0,$C0,$C0,$FE,$00
    ; M (77)
    DC.B $C6,$EE,$FE,$D6,$C6,$C6,$C6,$00
    ; N (78)
    DC.B $C6,$E6,$F6,$DE,$CE,$C6,$C6,$00
    ; O (79)
    DC.B $7C,$C6,$C6,$C6,$C6,$C6,$7C,$00
    ; P (80)
    DC.B $FC,$C6,$C6,$FC,$C0,$C0,$C0,$00
    ; Q (81)
    DC.B $7C,$C6,$C6,$C6,$D6,$CC,$76,$00
    ; R (82)
    DC.B $FC,$C6,$C6,$FC,$D8,$CC,$C6,$00
    ; S (83)
    DC.B $7C,$C6,$C0,$7C,$06,$C6,$7C,$00
    ; T (84)
    DC.B $FE,$18,$18,$18,$18,$18,$18,$00
    ; U (85)
    DC.B $C6,$C6,$C6,$C6,$C6,$C6,$7C,$00
    ; V (86)
    DC.B $C6,$C6,$C6,$C6,$6C,$38,$10,$00
    ; W (87)
    DC.B $C6,$C6,$C6,$D6,$FE,$EE,$C6,$00
    ; X (88)
    DC.B $C6,$C6,$6C,$38,$6C,$C6,$C6,$00
    ; Y (89)
    DC.B $C6,$C6,$6C,$38,$18,$18,$18,$00
    ; Z (90)
    DC.B $FE,$06,$0C,$18,$30,$60,$FE,$00
    ; [ (91)
    DC.B $3C,$30,$30,$30,$30,$30,$3C,$00
    ; \ (92)
    DC.B $C0,$60,$30,$18,$0C,$06,$02,$00
    ; ] (93)
    DC.B $3C,$0C,$0C,$0C,$0C,$0C,$3C,$00
    ; ^ (94)
    DC.B $10,$38,$6C,$C6,$00,$00,$00,$00
    ; _ (95)
    DC.B $00,$00,$00,$00,$00,$00,$FE,$00
    ; ` (96)
    DC.B $30,$18,$0C,$00,$00,$00,$00,$00
    ; a (97)
    DC.B $00,$00,$7C,$06,$7E,$C6,$7E,$00
    ; b (98)
    DC.B $C0,$C0,$FC,$C6,$C6,$C6,$FC,$00
    ; c (99)
    DC.B $00,$00,$7C,$C6,$C0,$C6,$7C,$00
    ; d (100)
    DC.B $06,$06,$7E,$C6,$C6,$C6,$7E,$00
    ; e (101)
    DC.B $00,$00,$7C,$C6,$FE,$C0,$7C,$00
    ; f (102)
    DC.B $1C,$36,$30,$78,$30,$30,$30,$00
    ; g (103)
    DC.B $00,$00,$7E,$C6,$C6,$7E,$06,$7C
    ; h (104)
    DC.B $C0,$C0,$FC,$C6,$C6,$C6,$C6,$00
    ; i (105)
    DC.B $18,$00,$38,$18,$18,$18,$3C,$00
    ; j (106)
    DC.B $06,$00,$0E,$06,$06,$C6,$7C,$00
    ; k (107)
    DC.B $C0,$C0,$CC,$D8,$F0,$D8,$CC,$00
    ; l (108)
    DC.B $38,$18,$18,$18,$18,$18,$3C,$00
    ; m (109)
    DC.B $00,$00,$CC,$FE,$D6,$C6,$C6,$00
    ; n (110)
    DC.B $00,$00,$FC,$C6,$C6,$C6,$C6,$00
    ; o (111)
    DC.B $00,$00,$7C,$C6,$C6,$C6,$7C,$00
    ; p (112)
    DC.B $00,$00,$FC,$C6,$C6,$FC,$C0,$C0
    ; q (113)
    DC.B $00,$00,$7E,$C6,$C6,$7E,$06,$06
    ; r (114)
    DC.B $00,$00,$DC,$E6,$C0,$C0,$C0,$00
    ; s (115)
    DC.B $00,$00,$7E,$C0,$7C,$06,$FC,$00
    ; t (116)
    DC.B $30,$30,$7C,$30,$30,$36,$1C,$00
    ; u (117)
    DC.B $00,$00,$C6,$C6,$C6,$C6,$7E,$00
    ; v (118)
    DC.B $00,$00,$C6,$C6,$6C,$38,$10,$00
    ; w (119)
    DC.B $00,$00,$C6,$C6,$D6,$FE,$6C,$00
    ; x (120)
    DC.B $00,$00,$C6,$6C,$38,$6C,$C6,$00
    ; y (121)
    DC.B $00,$00,$C6,$C6,$C6,$7E,$06,$7C
    ; z (122)
    DC.B $00,$00,$FE,$0C,$38,$60,$FE,$00
    ; { (123)
    DC.B $0E,$18,$18,$70,$18,$18,$0E,$00
    ; | (124)
    DC.B $18,$18,$18,$18,$18,$18,$18,$00
    ; } (125)
    DC.B $70,$18,$18,$0E,$18,$18,$70,$00
    ; ~ (126)
    DC.B $76,$DC,$00,$00,$00,$00,$00,$00

    EVEN

    END
