/* disassembler_main.c — Standalone disassembler tool */
#include "disassembler/disassembler.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *prog) {
    fprintf(stderr,
        "M68000 Disassembler v0.1\n"
        "Usage: %s [options] <input.bin>\n"
        "\n"
        "Options:\n"
        "  -o <file>    Output file (default: stdout)\n"
        "  -org <addr>  Base address (hex, default: 0)\n"
        "  -h           Help\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *inputPath  = NULL;
    const char *outputPath = NULL;
    u32 baseAddr = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0)      { printUsage(argv[0]); return 0; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outputPath = argv[++i];
        else if (strcmp(argv[i], "-org") == 0 && i + 1 < argc) {
            baseAddr = (u32)strtoul(argv[++i], NULL, 16);
        }
        else if (argv[i][0] != '-') inputPath = argv[i];
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!inputPath) { printUsage(argv[0]); return 1; }

    FILE *f = fopen(inputPath, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", inputPath); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8 *data = malloc((size_t)size);
    fread(data, 1, (size_t)size, f);
    fclose(f);

    disasmRange(data, baseAddr, (u32)size, outputPath);

    free(data);
    return 0;
}
