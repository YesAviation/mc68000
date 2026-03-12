/* optimizer.h — IR optimisation passes */
#ifndef M68K_CC_OPTIMIZER_H
#define M68K_CC_OPTIMIZER_H

#include "compiler/middle/ir.h"

/* Run all optimisation passes on the IR module */
void optimizerRun(IrModule *m, int optLevel);

/* Individual passes */
void optimizerConstantFold(IrModule *m);
void optimizerDeadCodeElim(IrModule *m);
void optimizerCopyPropagation(IrModule *m);

#endif /* M68K_CC_OPTIMIZER_H */
