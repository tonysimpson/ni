 /***************************************************************/
/***         Includes Python internal headers                  ***/
 /***************************************************************/


#ifndef _PYCINTERNAL_H
#define _PYCINTERNAL_H

#include <opcode.h>


/* Post-2.2 versions of Python introduced the following more explicit names.
   Map them to the old names if they do not exist. */
#ifndef PyCmp_IS
# define PyCmp_IS         IS
#endif
#ifndef PyCmp_IS_NOT
# define PyCmp_IS_NOT     IS_NOT
#endif
#ifndef PyCmp_IN
# define PyCmp_IN         IN
#endif
#ifndef PyCmp_NOT_IN
# define PyCmp_NOT_IN     NOT_IN
#endif
#ifndef PyCmp_EXC_MATCH
# define PyCmp_EXC_MATCH  EXC_MATCH
#endif


#endif /* _PYCINTERNAL_H */
