 /***************************************************************/
/***         Support to manage the emitted code chunks         ***/
 /***************************************************************/

#ifndef _CODEMANAGER_H
#define _CODEMANAGER_H


#include "psyco.h"
#include "dispatcher.h"


/* define to store the end of the code in CodeBufferObjects */
/*#undef STORE_CODE_END*/

#ifdef CODE_DUMP_FILE
# define STORE_CODE_END   /* always needed by CODE_DUMP_FILE */
#endif


/* a CodeBufferObject is a pointer to emitted code.
   The 'state' PsycoObject records the state of the compiler at
   the start of the emission of code. Consider this field as private.
   Future versions of the code manager will probably encode the recorded
   states in a more sophisticated form than just a dump copy.
   (There are usually a lot of small CodeBufferObjects, so if each
   one has a full copy of the state big projects will explode the memory.)
*/
struct CodeBufferObject_s {
	PyObject_HEAD
	code_t* codeptr;
	FrozenPsycoObject snapshot;
  
#ifdef STORE_CODE_END
	code_t* codeend;
	char* codemode;
#ifdef CODE_DUMP_FILE
	CodeBufferObject* chained_list;
#endif
#endif
};

#ifdef CODE_DUMP_FILE
EXTERNVAR CodeBufferObject* psyco_codebuf_chained_list;
#endif


#define CodeBuffer_Check(v)	((v)->ob_type == &CodeBuffer_Type)
EXTERNVAR PyTypeObject CodeBuffer_Type;


/* starts a new code buffer, whose size is initially BIG_BUFFER_SIZE.
   'po' is the state of the compiler at this point, of which a
   frozen copy will be made. It can be NULL. If not, set 'ge' as in
   psyco_compile(). */
EXTERNFN
CodeBufferObject* psyco_new_code_buffer(PsycoObject* po, global_entries_t* ge);

/* creates a CodeBufferObject pointing to an already existing code target */
EXTERNFN
CodeBufferObject* psyco_proxy_code_buffer(PsycoObject* po, global_entries_t* ge);

/* shrink a buffer returned by new_code_buffer() */
EXTERNFN
void psyco_shrink_code_buffer(CodeBufferObject* obj, int nsize);

#ifdef STORE_CODE_END
#define SHRINK_CODE_BUFFER(obj, nsize, mode)    do {    \
      psyco_shrink_code_buffer(obj, nsize);             \
      (obj)->codemode = (mode);                         \
} while (0)
#else
# define SHRINK_CODE_BUFFER(obj, nsize, mode)     \
      psyco_shrink_code_buffer(obj, nsize)
#endif

/* a replacement for Py_XDECREF(obj) which does not release the object
   immediately, but only at the next call to psyco_trash_object() */
EXTERNFN
void psyco_trash_object(PyObject* obj);


#endif /* _CODEMANAGER_H */
