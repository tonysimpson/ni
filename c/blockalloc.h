 /***************************************************************/
/***           Generic fixed-sized-blocks allocator            ***/
 /***************************************************************/

#ifndef _BLOCKALLOC_H
#define _BLOCKALLOC_H


#include "psyco.h"


/* Define two functions to allocate and free items of type 'type'.
   The functions are named psyco_llalloc_[name] and psyco_llfree_[name]. */
#define BLOCKALLOC_INTERFACE(name, type)\
	BLOCKALLOC_INTERN_DEF(name, type, EXTERNVAR, EXTERNFN)


/* Implements the required internal variables and functions.
   The allocator will reserve and subdivide blocks of the given size. */
#define BLOCKALLOC_IMPLEMENTATION(name, type, blocksize) \
	BLOCKALLOC_INTERN_IMPL(name, type, blocksize, DEFINEVAR, DEFINEFN)


/* Same as the combination of the above two when the result need only
   be local to a .c source (i.e. static). */
#define BLOCKALLOC_STATIC(name, type, blocksize) \
	BLOCKALLOC_INTERN_DEF(name, type, staticforward, static /*forward*/) \
	BLOCKALLOC_INTERN_IMPL(name, type, blocksize, statichere, static)


/***************************************************************/
 /***   dummy version, used in conjunction with a memory      ***/
  /***   checker like linuxmemchk                              ***/

#ifdef PSYCO_NO_LINKED_LISTS

#define BLOCKALLOC_INTERN_DEF(name, type, EVAR, EFN) \
PSY_INLINE type* psyco_llalloc_##name(void) { \
	type* vi = (type*) PyMem_MALLOC(sizeof(type)); \
	if (vi == NULL) \
		OUT_OF_MEMORY(); \
	return vi; \
} \
PSY_INLINE void psyco_llfree_##name(type* vi) { \
	PyMem_FREE((char*) vi); \
}

#define BLOCKALLOC_INTERN_IMPL(name, type, blocksize, DVAR, DFN)  /* nothing */


/***************************************************************/
 /***   full linked-list block-allocated version              ***/

#else /* if !PSYCO_NO_LINKED_LISTS */

#define BLOCKALLOC_INTERN_DEF(name, type, EVAR, EFN) \
EVAR void** psyco_linked_list_##name; \
EFN  void*  psyco_ll_newblock_##name(void); \
PSY_INLINE type* psyco_llalloc_##name(void) { \
	type* vi; \
	if (psyco_linked_list_##name == NULL) \
		vi = (type*) psyco_ll_newblock_##name(); \
	else { \
		vi = (type*) psyco_linked_list_##name; \
		psyco_linked_list_##name = *(void**) vi; \
	} \
	return vi; \
} \
PSY_INLINE void psyco_llfree_##name(type* vi) { \
	*(void**) vi = psyco_linked_list_##name; \
	psyco_linked_list_##name = (void**) vi; \
}

#define BLOCKALLOC_INTERN_IMPL(name, type, blocksize, DVAR, DFN) \
DVAR void** psyco_linked_list_##name = NULL; \
DFN  void* psyco_ll_newblock_##name() \
{ \
	int block_count = (blocksize)/sizeof(type); \
	size_t sze = block_count * sizeof(type); \
	type* p; \
	type* prev = (type*) psyco_linked_list_##name; \
	type* block = (type*) PyMem_MALLOC(sze); \
	psyco_memory_usage += sze; \
	extra_assert(block_count > 0 && sizeof(type) >= sizeof(void*)); \
	if (block == NULL) \
		OUT_OF_MEMORY(); \
	for (p=block+block_count; --p!=block; ) { \
		*(type**)p = prev; \
		prev = p; \
	} \
	psyco_linked_list_##name = *(void***) prev; \
	return prev; \
}

/***************************************************************/

#endif /* !PSYCO_NO_LINKED_LISTS */


#endif /* _BLOCKALLOC_H */
