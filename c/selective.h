#ifndef _SELECTIVE_H
#define _SELECTIVE_H

#include "psyco.h"
#include "codemanager.h"

#define FUN_BOUND -1
#define MAX_RECURSION 1

EXTERNVAR PyObject* funcs;
EXTERNVAR long ticks;
EXTERNFN int do_selective(void *, PyFrameObject *, int, PyObject *);

#endif
