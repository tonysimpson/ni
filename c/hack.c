/* this file includes all other .c and .h files.
   Its purpose is to provide a version of _psycomodule.so whose only
   exported (non-static) symbol is init_psyco().
   It also seems that the GDB debugger doesn't locate too well non-static
   symbols in shared libraries.
*/

/* means that all .c files are meant to be compiler together,
    with all symbols 'static' */
#define ALL_STATIC

/* disable all debugging code */
/*#define DISABLE_DEBUG*/


#include "psyco.h"

#include "Python/pycompiler.c"
#include "Python/pbltinmodule.c"

#include "Objects/pabstract.c"
#include "Objects/pclassobject.c"
#include "Objects/pdescrobject.c"
#include "Objects/pdictobject.c"
#include "Objects/pfuncobject.c"
#include "Objects/pintobject.c"
#include "Objects/piterobject.c"
#include "Objects/plistobject.c"
#include "Objects/plongobject.c"
#include "Objects/pmethodobject.c"
#include "Objects/pobject.c"
#include "Objects/pstringobject.c"
#include "Objects/pstructmember.c"
#include "Objects/psycofuncobject.c"
#include "Objects/ptupleobject.c"

#include "codemanager.c"
#include "dispatcher.c"
#include "processor.c"
#include "vcompiler.c"
#include "mergepoints.c"
#include "pycencoding.c"
#include "linuxmemchk.c"
#include "selective.c"
#include "psyco.c"   /* must be the last one for CODE_DUMP_AT_END_ONLY to work */


 /***************************************************************/
/***   Debug dumping of state                                  ***/
 /***************************************************************/

/* #ifdef PSYCO_DUMP */
/* static int psyco_dump() */
/* { */
/*   int i; */
/*   PyObject* source = psyco_ge_mainloop.fatlist; */
/*   FILE* f = fopen("psyco.dump", "wb"); */
/*   if (f == NULL) */
/*     return -1; */

/*   for (i=0; i<PyList_Size(source); i++) */
/*     { */
/*       CodeBufferObject* codebuf = (CodeBufferObject*) PyList_GetItem(source, i); */
/*       int size; */
/*       assert(codebuf != NULL && CodeBuffer_Check(codebuf)); */
/*       size = codebuf->codesize; */
/*       if (size == -1) */
/*         size = BIG_BUFFER_SIZE - GUARANTEED_MINIMUM; */
/*       fprintf(f, "CodeBufferObject %p %d %d\n", */
/*               codebuf->codeptr, codebuf->codesize, size); */
/*       fwrite(codebuf->codeptr, 1, size, f); */
/*     } */
  
/*   return fclose(f); */
/* } */
/* void* psyco_just_a_dummy_pointer = &psyco_dump; */
/* #endif */
