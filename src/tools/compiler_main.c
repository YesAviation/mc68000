/* compiler_main.c — Standalone C compiler tool */
#include "compiler/compiler.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

static void printUsage(const char *prog) {
    fprintf(stderr,
        "M68000 C Compiler v0.1\n"
        "Usage: %s [options] <input.c> -o <output.s>\n"
        "\n"
        "Options:\n"
        "  -o <file>    Output assembly file (default: out.s)\n"
        "  -O<n>        Optimisation level (0-2, default: 0)\n"
        "  -v           Verbose\n"
        "  -h           Help\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *inputPath  = NULL;
    const char *outputPath = "out.s";
    int optLevel = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0)      { printUsage(argv[0]); return 0; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outputPath = argv[++i];
        else if (strcmp(argv[i], "-v") == 0) logSetLevel(LOG_TRACE);
        else if (strncmp(argv[i], "-O", 2) == 0) optLevel = argv[i][2] - '0';
        else if (argv[i][0] != '-') inputPath = argv[i];
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!inputPath) { printUsage(argv[0]); return 1; }

    Compiler *cc = compilerCreate();
    compilerSetOptLevel(cc, optLevel);

    if (!compilerCompileFile(cc, inputPath, outputPath)) {
        int errCount = compilerGetErrorCount(cc);
        for (int i = 0; i < errCount; i++)
            fprintf(stderr, "Error: %s\n", compilerGetError(cc, i));
        fprintf(stderr, "%d error(s)\n", errCount);
        compilerDestroy(cc);
        return 1;
    }

    printf("Compiled: %s -> %s\n", inputPath, outputPath);
    compilerDestroy(cc);
    return 0;
}
