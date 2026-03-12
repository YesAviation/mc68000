; ===========================================================================
;  kernel.s — MiniMint OS Kernel
;
;  Loaded by ROM bootloader to $000800.  Entry point is _start.
;
;  Memory map:
;    $000000-$0003FF   Exception vector table (1 KB, 256 vectors)
;    $000400-$0007FF   System variables / globals
;    $000800-$00FFFF   Kernel code + data
;    $010000-$01FFFF   GUI + font + icon data (future)
;    $020000-$03FFFF   Framebuffer back-buffer (future)
;    $040000-$0BFFFF   Application heap (future)
;    $0C0000-$0EFFFF   Disk cache (future)
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

; ── System variables ($000400) ──
SYS_TICKS:      EQU $000400
SYS_KEYBUF:     EQU $000410
SYS_KEY_HEAD:   EQU $000450
SYS_KEY_TAIL:   EQU $000451
SYS_MOUSE_X:    EQU $000452
SYS_MOUSE_Y:    EQU $000454
SYS_MOUSE_BTN:  EQU $000456

KEYBUF_MASK:    EQU 63

; ===========================================================================
;  Kernel entry — jumped to by ROM bootloader
;  We are in supervisor mode, IPL=7 (all IRQs masked)
; ===========================================================================
_start:
    ; Set up stack
    LEA     $0F0000, A7

    ; Install exception vector table
    BSR.W   install_vectors

    ; Init system variables to zero
    BSR.W   init_sysvars

    ; Print kernel boot banner
    LEA     msg_boot(PC), A0
    BSR.W   uart_puts

    ; Init video — simple grayscale palette + clear screen
    BSR.W   video_init
    LEA     msg_video(PC), A0
    BSR.W   uart_puts

    ; Init keyboard/mouse
    BSR.W   input_init
    LEA     msg_input(PC), A0
    BSR.W   uart_puts

    ; Init timer
    BSR.W   timer_init
    LEA     msg_timer(PC), A0
    BSR.W   uart_puts

    ; Enable VBlank IRQ
    MOVE.W  #$0003, VIDEO_CTRL

    ; Print ready message
    LEA     msg_ready(PC), A0
    BSR.W   uart_puts

    ; Enable interrupts — drop IPL to 0
    ANDI.W  #$F8FF, SR

    ; ── Main idle loop ──
main_loop:
    BSR.W   key_poll
    TST.B   D0
    BEQ.S   ml_nokey
    ; Echo character to UART
    BSR.W   uart_putchar
ml_nokey:
    BRA.S   main_loop


; ===========================================================================
;  VECTOR TABLE SETUP
; ===========================================================================
install_vectors:
    MOVEM.L D0/A0-A1, -(A7)

    ; Fill all 256 vectors with default_handler
    LEA     $000000, A0
    LEA     default_handler(PC), A1
    MOVE.W  #255, D0
iv_loop:
    MOVE.L  A1, (A0)+
    DBRA    D0, iv_loop

    ; Reset vectors
    MOVE.L  #$000F0000, $000000         ; V0: SSP
    MOVE.L  #$00000800, $000004         ; V1: PC

    ; Bus error (vector 2)
    LEA     panic_bus(PC), A1
    MOVE.L  A1, $000008

    ; Address error (vector 3)
    LEA     panic_addr(PC), A1
    MOVE.L  A1, $00000C

    ; Illegal instruction (vector 4)
    LEA     panic_illegal(PC), A1
    MOVE.L  A1, $000010

    ; Division by zero (vector 5)
    LEA     panic_divzero(PC), A1
    MOVE.L  A1, $000014

    ; CHK (vector 6)
    LEA     default_handler(PC), A1
    MOVE.L  A1, $000018

    ; TRAPV (vector 7)
    MOVE.L  A1, $00001C

    ; Privilege violation (vector 8)
    LEA     panic_priv(PC), A1
    MOVE.L  A1, $000020

    ; VBlank — level 1 autovector (vector 25, offset $64)
    LEA     isr_vblank(PC), A1
    MOVE.L  A1, $000064

    ; Parallel/keyboard — level 3 (vector 27, offset $6C)
    LEA     isr_keyboard(PC), A1
    MOVE.L  A1, $00006C

    ; UART — level 4 (vector 28, offset $70)
    LEA     isr_uart(PC), A1
    MOVE.L  A1, $000070

    ; Timer — level 5 (vector 29, offset $74)
    LEA     isr_timer(PC), A1
    MOVE.L  A1, $000074

    ; TRAP #0 — syscalls (vector 32, offset $80)
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
    ; Mouse starts at screen center
    MOVE.W  #160, SYS_MOUSE_X
    MOVE.W  #120, SYS_MOUSE_Y
    MOVEM.L (A7)+, D0/A0
    RTS


; ===========================================================================
;  UART CONSOLE DRIVER
; ===========================================================================

; ── uart_putchar: D0.B = char to send ──
uart_putchar:
    MOVEM.L D1, -(A7)
upc_wait:
    MOVE.W  UART_STATUS, D1
    BTST    #2, D1
    BEQ.S   upc_wait
    MOVE.W  D0, UART_DATA
    MOVEM.L (A7)+, D1
    RTS

; ── uart_puts: A0 = null-terminated string ──
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

; ── uart_puthex8: D0.B → 2 hex digits ──
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

; ── uart_puthex16: D0.W → 4 hex digits ──
uart_puthex16:
    MOVEM.L D0-D1, -(A7)
    MOVE.W  D0, D1
    LSR.W   #8, D0
    BSR.W   uart_puthex8
    MOVE.B  D1, D0
    BSR.W   uart_puthex8
    MOVEM.L (A7)+, D0-D1
    RTS

; ── uart_puthex32: D0.L → 8 hex digits ──
uart_puthex32:
    MOVEM.L D0-D1, -(A7)
    MOVE.L  D0, D1
    SWAP    D0
    BSR.W   uart_puthex16
    MOVE.W  D1, D0
    BSR.W   uart_puthex16
    MOVEM.L (A7)+, D0-D1
    RTS

; ── uart_newline ──
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

    ; Build grayscale palette: index N → gray level N
    ; RGB565 gray: R5=val>>3, G6=val>>2, B5=val>>3
    LEA     VIDEO_PALETTE, A0
    MOVE.W  #255, D0
vpal_loop:
    MOVE.W  D0, D1
    ANDI.W  #$FF, D1
    ; R5 = D1 >> 3
    MOVE.W  D1, D2
    LSR.W   #3, D2                      ; R5 (also B5)
    MOVE.W  D2, -(A7)                   ; save B5 on stack
    LSL.W   #8, D2
    LSL.W   #3, D2                      ; R5 << 11
    MOVE.W  D1, D1
    LSR.W   #2, D1                      ; G6 = val >> 2
    LSL.W   #5, D1                      ; G6 << 5
    OR.W    D1, D2                      ; R|G
    MOVE.W  (A7)+, D1                   ; restore B5
    OR.W    D1, D2                      ; R|G|B
    MOVE.W  D2, (A0)+
    DBRA    D0, vpal_loop

    ; Force index 0 = black, index 255 = white
    MOVE.W  #$0000, VIDEO_PALETTE
    LEA     VIDEO_PALETTE, A0
    MOVE.W  #$FFFF, 510(A0)

    ; Clear VRAM to black (index 0)
    LEA     VIDEO_VRAM, A0
    MOVE.W  #19199, D0                  ; 76800/4 - 1 = 19199
vclr_loop:
    CLR.L   (A0)+
    DBRA    D0, vclr_loop

    ; Enable display, no VBlank IRQ yet
    MOVE.W  #$0001, VIDEO_CTRL

    MOVEM.L (A7)+, D0-D2/A0
    RTS


; ===========================================================================
;  INPUT DRIVER
; ===========================================================================
input_init:
    ; Ports as input, enable IRQ on input
    MOVE.B  #$00, PARALLEL_A_DIR
    MOVE.B  #$00, PARALLEL_B_DIR
    MOVE.B  #$02, PARALLEL_CTRL
    RTS

; ── key_poll: return char in D0.B, or 0 if empty ──
key_poll:
    MOVE.B  SYS_KEY_HEAD, D0
    CMP.B   SYS_KEY_TAIL, D0
    BEQ.S   kp_empty
    ; Read from ring buffer
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

; ── VBlank (Level 1, Vector 25) ──
isr_vblank:
    MOVEM.L D0, -(A7)
    MOVE.W  #$0001, VIDEO_STATUS        ; acknowledge VBlank
    ADDQ.L  #1, SYS_TICKS
    MOVEM.L (A7)+, D0
    RTE

; ── Timer (Level 5, Vector 29) ──
isr_timer:
    MOVEM.L D0, -(A7)
    MOVE.W  #$0001, TIMER_CONTROL       ; re-arm
    MOVEM.L (A7)+, D0
    RTE

; ── UART RX (Level 4, Vector 28) ──
isr_uart:
    MOVEM.L D0-D1/A0, -(A7)
    MOVE.W  UART_STATUS, D0
    BTST    #0, D0                      ; RXRDY?
    BEQ.S   isr_uart_done
    MOVE.W  UART_DATA, D0
    ANDI.W  #$00FF, D0
    ; Put into keyboard ring buffer
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

; ── Keyboard (Level 3, Vector 27) ──
isr_keyboard:
    MOVEM.L D0-D1/A0, -(A7)
    MOVE.B  PARALLEL_A_DATA, D0
    MOVE.B  #$03, PARALLEL_STATUS       ; acknowledge
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
;  D0 = syscall number, D1-D3 = args, A0-A1 = pointer args
; ===========================================================================
trap_syscall:
    CMPI.W  #0, D0
    BEQ.W   sys_exit
    CMPI.W  #1, D0
    BEQ.W   sys_putchar
    CMPI.W  #2, D0
    BEQ.W   sys_getchar
    ; More syscalls added later
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
    ORI.W   #$0700, SR                  ; mask all IRQs
    MOVEM.L D0/A0, -(A7)
    LEA     pmsg_hdr(PC), A0
    BSR.W   uart_puts
    MOVEM.L (A7)+, D0/A0
    BSR.W   uart_puts                   ; exception name
    BSR.W   uart_newline

    ; Print D0-D3
    LEA     pmsg_d03(PC), A0
    BSR.W   uart_puts
    BSR.W   uart_puthex32               ; D0
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

    ; Print D4-D7
    LEA     pmsg_d47(PC), A0
    BSR.W   uart_puts
    MOVE.L  D4, D0
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D5, D0
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D6, D0
    BSR.W   uart_puthex32
    MOVE.B  #32, D0
    BSR.W   uart_putchar
    MOVE.L  D7, D0
    BSR.W   uart_puthex32
    BSR.W   uart_newline

    LEA     pmsg_halt(PC), A0
    BSR.W   uart_puts
    STOP    #$2700


; ── Default handler ──
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
    DC.B $0A, "MiniMint ready.", $0A
    DC.B "Type to echo via serial.", $0A, $0A, 0
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

pmsg_d47:
    DC.B "D4-D7: ", 0
    EVEN

pmsg_halt:
    DC.B "System halted.", $0A, 0
    EVEN

    END
