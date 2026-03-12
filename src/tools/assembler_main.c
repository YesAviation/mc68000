/* assembler_main.c — Standalone assembler tool */
#include "assembler/assembler.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *prog) {
    fprintf(stderr,
        "M68000 Assembler v0.1\n"
        "Usage: %s [options] <input.s> -o <output.bin>\n"
        "\n"
        "Options:\n"
        "  -o <file>    Output file (default: a.out)\n"
        "  -s <file>    Output S-record format\n"
        "  -l           Enable listing\n"
        "  -org <addr>  Set origin address (hex)\n"
        "  -v           Verbose\n"
        "  -h           Help\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *inputPath  = NULL;
    const char *outputPath = "a.out";
    const char *srecPath   = NULL;
    u32 origin = 0;
    bool listing = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0)      { printUsage(argv[0]); return 0; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outputPath = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) srecPath = argv[++i];
        else if (strcmp(argv[i], "-l") == 0) listing = true;
        else if (strcmp(argv[i], "-v") == 0) logSetLevel(LOG_TRACE);
        else if (strcmp(argv[i], "-org") == 0 && i + 1 < argc) {
            origin = (u32)strtoul(argv[++i], NULL, 16);
        }
        else if (argv[i][0] != '-') inputPath = argv[i];
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!inputPath) { printUsage(argv[0]); return 1; }

    Assembler *as = asmCreate();
    asmSetOrigin(as, origin);
    asmSetListing(as, listing);

    if (!asmAssembleFile(as, inputPath)) {
        int errCount = asmGetErrorCount(as);
        for (int i = 0; i < errCount; i++)
            fprintf(stderr, "Error: %s\n", asmGetError(as, i));
        fprintf(stderr, "%d error(s)\n", errCount);
        asmDestroy(as);
        return 1;
    }

    asmWriteBinary(as, outputPath);
    printf("Output: %s\n", outputPath);

    if (srecPath) {
        asmWriteSRecord(as, srecPath);
        printf("S-record: %s\n", srecPath);
    }

    u32 outSize;
    asmGetOutput(as, &outSize);
    printf("Assembled %u bytes\n", outSize);

    asmDestroy(as);
    return 0;
}
