#!/bin/bash
# ===========================================================================
#  build.sh — Build the MiniMint OS
#
#  Prerequisites: m68k_as and m68k_emu must be built first.
#    cd build && cmake .. && make -j8
#
#  This script:
#    1. Assembles rom.s → rom.bin
#    2. Assembles kernel.s → kernel.bin
#    3. Builds mkdisk tool
#    4. Creates disk.img with kernel installed
#    5. (Optional) Runs the emulator
# ===========================================================================

set -e

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
OS_DIR="$SCRIPT_DIR"
OUT_DIR="$OS_DIR/out"

ASM="$BUILD_DIR/m68k_as"
EMU="$BUILD_DIR/m68k_emu"

# Colours
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${CYAN}══════════════════════════════════════════${NC}"
echo -e "${BOLD}${CYAN}  MiniMint OS Build System${NC}"
echo -e "${BOLD}${CYAN}══════════════════════════════════════════${NC}"
echo ""

# Check prerequisites
if [ ! -f "$ASM" ]; then
    echo -e "${RED}Error: m68k_as not found at $ASM${NC}"
    echo "Build the emulator first: cd build && cmake .. && make -j8"
    exit 1
fi

# Create output directory
mkdir -p "$OUT_DIR"

# ── Step 1: Assemble ROM ──
echo -e "${CYAN}[1/4]${NC} Assembling ROM..."
"$ASM" -org F00000 "$OS_DIR/rom.s" -o "$OUT_DIR/rom.bin"
ROM_SIZE=$(wc -c < "$OUT_DIR/rom.bin" | tr -d ' ')
echo -e "  ${GREEN}✓${NC} rom.bin ($ROM_SIZE bytes)"

# ── Step 2: Assemble Kernel ──
echo -e "${CYAN}[2/4]${NC} Assembling kernel..."
"$ASM" -org 000800 "$OS_DIR/kernel.s" -o "$OUT_DIR/kernel.bin"
KERN_SIZE=$(wc -c < "$OUT_DIR/kernel.bin" | tr -d ' ')
echo -e "  ${GREEN}✓${NC} kernel.bin ($KERN_SIZE bytes)"

# ── Step 3: Build mkdisk tool ──
echo -e "${CYAN}[3/4]${NC} Building mkdisk..."
cc -Wall -Wextra -O2 -o "$OUT_DIR/mkdisk" "$OS_DIR/tools/mkdisk.c"
echo -e "  ${GREEN}✓${NC} mkdisk"

# ── Step 4: Create disk image ──
echo -e "${CYAN}[4/4]${NC} Creating disk image..."
"$OUT_DIR/mkdisk" "$OUT_DIR/disk.img" "$OUT_DIR/kernel.bin" 4
echo ""

# ── Summary ──
echo -e "${BOLD}${GREEN}Build complete!${NC}"
echo ""
echo "Files:"
echo "  ROM:    $OUT_DIR/rom.bin   ($ROM_SIZE bytes)"
echo "  Kernel: $OUT_DIR/kernel.bin ($KERN_SIZE bytes)"
echo "  Disk:   $OUT_DIR/disk.img  (4 MB)"
echo ""
echo "To run:"
echo "  $EMU -d $OUT_DIR/disk.img $OUT_DIR/rom.bin"
echo ""
echo "To run in console-only mode:"
echo "  $EMU -c -d $OUT_DIR/disk.img $OUT_DIR/rom.bin"
echo ""

# If first argument is "run", launch emulator
if [ "$1" = "run" ]; then
    echo -e "${CYAN}Launching emulator...${NC}"
    exec "$EMU" -c -d "$OUT_DIR/disk.img" "$OUT_DIR/rom.bin"
fi

# If first argument is "gui", launch with GUI
if [ "$1" = "gui" ]; then
    echo -e "${CYAN}Launching emulator (GUI)...${NC}"
    exec "$EMU" -d "$OUT_DIR/disk.img" "$OUT_DIR/rom.bin"
fi
