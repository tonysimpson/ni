#include "psyco.h"
#if HEAVY_MEM_CHECK

#include <Python.h>
#undef PyCore_MALLOC
#undef PyCore_REALLOC
#undef PyCore_FREE

EXTERNFN void* memchk_ef_malloc(int size);
EXTERNFN void memchk_ef_free(void* data);
EXTERNFN void* memchk_ef_realloc(void* data, int nsize);

#define PyCore_MALLOC   memchk_ef_malloc
#define PyCore_REALLOC  memchk_ef_realloc
#define PyCore_FREE     memchk_ef_free

#endif
