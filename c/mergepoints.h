 /***************************************************************/
/***                Tables of code merge points                ***/
 /***************************************************************/


#ifndef _MERGEPOINTS_H
#define _MERGEPOINTS_H


#include "psyco.h"
#include <compile.h>

#define CHECK_ARRAY_BIT(s, n)   ((s)[(n)/8] &  (1<<((n)&7)))
#define SET_ARRAY_BIT(s, n)     ((s)[(n)/8] |= (1<<((n)&7)))

/* A merge point is a point reached by two or more control paths in
   the bytecode. They are a subset of the targets found by the standard
   module function dis.findlabels(). It may be a subset because some
   targets can only be reached by jumping from a single point, e.g. 'else:'.
   
   The following function detects the merge points and set them in a bit array.
*/
EXTERNFN char* psyco_get_merge_points(PyCodeObject* co);


#endif /* _MERGEPOINTS_H */
