; ===========================================================================
;  rom.s — MiniMint ROM Bootstrap
;
;  Lives at $F00000 (64 KB ROM).  On reset the MC68000 reads the initial
;  SSP and PC from $000000-$000007.  A hardware ROM overlay mirrors ROM
;  content to that address range until software writes to the overlay
;  latch at $E00FFE.
;
;  Boot sequence:
;    1. CPU reads SSP=$0F0000, PC=$F00008 from overlay
;    2. ROM code initialises UART for debug output
;    3. Prints boot banner
;    4. Loads kernel from disk (sector 1+) to RAM $000800
;    5. Disables ROM overlay (write to $E00FFE)
;    6. Writes minimal vectors to RAM at $000000
;    7. Jumps to kernel entry at $000800
; ===========================================================================

; ── Hardware addresses ──
UART_STATUS:    EQU $E00000
UART_DATA:      EQU $E00002
OVERLAY_LATCH:  EQU $E00FFE

STOR_CMD:       EQU $E01000
STOR_SEC_HI:    EQU $E01004
STOR_SEC_LO:    EQU $E01006
STOR_DMA_HI:    EQU $E01008
STOR_DMA_LO:    EQU $E0100A
STOR_COUNT:     EQU $E0100C
STOR_CMD_READ:  EQU 1

KERNEL_LOAD:    EQU $000800
KERNEL_SECTORS: EQU 128
KERNEL_DISK_START: EQU 1

; ── ROM vectors — read from $000000 via overlay ──

    DC.L $000F0000                  ; Vector 0: Initial SSP
    DC.L $00F00008                  ; Vector 1: Initial PC

; ── ROM code begins here ($F00008) ──

rom_start:
    ; ── Print boot banner via UART ──
    LEA     banner(PC), A0
    BSR.W   rom_puts

    LEA     msg_loading(PC), A0
    BSR.W   rom_puts

    ; ── Load kernel from disk into RAM ──
    MOVE.W  #0, STOR_SEC_HI
    MOVE.W  #1, STOR_SEC_LO
    MOVE.W  #0, STOR_DMA_HI
    MOVE.W  #$0800, STOR_DMA_LO
    MOVE.W  #128, STOR_COUNT
    MOVE.W  #1, STOR_CMD

    ; ── Disable ROM overlay ──
    MOVE.B  #0, OVERLAY_LATCH

    ; ── Write minimal vectors to RAM ──
    MOVE.L  #$000F0000, $000000
    MOVE.L  #$00000800, $000004

    ; ── Print OK and jump to kernel ──
    LEA     msg_ok(PC), A0
    BSR.W   rom_puts

    JMP     $000800

; ===========================================================================
;  rom_puts — Print null-terminated string via UART
;  Input: A0 = pointer to string
; ===========================================================================
rom_puts:
    MOVE.B  (A0)+, D0
    BEQ.S   rp_done
rp_wait:
    MOVE.W  UART_STATUS, D1
    BTST    #2, D1
    BEQ.S   rp_wait
    MOVE.W  D0, UART_DATA
    BRA.S   rom_puts
rp_done:
    RTS

; ── String data ──
banner:
    DC.B $0A
    DC.B "MiniMint ROM v1.0", $0A
    DC.B "MC68000 @ 8 MHz", $0A
    DC.B 0
    EVEN

msg_loading:
    DC.B "Loading kernel... ", 0
    EVEN

msg_ok:
    DC.B "OK", $0A, 0
    EVEN

    END
