 /***************************************************************/
/***                Tables of code merge points                ***/
 /***************************************************************/


#ifndef _MERGEPOINTS_H
#define _MERGEPOINTS_H


#include "psyco.h"
#include "dispatcher.h"
#include <compile.h>

/* A merge point is a point reached by two or more control paths in
   the bytecode. They are a subset of the targets found by the standard
   module function dis.findlabels(). It may be a subset because some
   targets can only be reached by jumping from a single point, e.g. 'else:'.
   Such targets are not merge points.
*/

struct mergepoint_s {
  int bytecode_position;
  global_entries_t entries;
};

/* 'bytecode_ptr' gives the position of the merge point in the bytecode.
   An array of mergepoint_t structures is sorted against this field.

   'entries' is a list of snapshots of the compiler states previously
   encountered at this point.
*/

/* The following function detects the merge points and builds an array of
   mergepoint_t structures. It returns a buffer object containing the array
   (actually a Python string), or Py_None if the bytecode uses unsupported
   instructions. Does not return a new reference. */
EXTERNFN PyObject* psyco_get_merge_points(PyCodeObject* co);

/* Get a pointer to the first mergepoint_t structure in the array whose
   'bytecode_ptr' is >= position. */
EXTERNFN mergepoint_t* psyco_next_merge_point(PyObject* mergepoints,
					      int position);

/* Get a pointer to the very first mergepoint_t structure in the array */
inline mergepoint_t* psyco_first_merge_point(PyObject* mergepoints)
{
  extra_assert(PyString_Check(mergepoints));
  return (mergepoint_t*) PyString_AS_STRING(mergepoints);
}

/* Same as psyco_next_merge_point() but returns NULL if bytecode_ptr!=position */
inline mergepoint_t* psyco_exact_merge_point(PyObject* mergepoints,
                                             int position)
{
  mergepoint_t* mp = psyco_next_merge_point(mergepoints, position);
  if (mp->bytecode_position != position)
    mp = NULL;
  return mp;
}


#define MP_FLAGS_HAS_EXCEPT      1   /* the code block has an 'except' clause */
#define MP_FLAGS_EXTRA      (~0xFF)

inline int psyco_mp_flags(PyObject* mergepoints)
{
  char* endptr = PyString_AS_STRING(mergepoints)+PyString_GET_SIZE(mergepoints);
  return ((int*) endptr)[-1];
}


#endif /* _MERGEPOINTS_H */
