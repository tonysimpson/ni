 /***************************************************************/
/***               interface to bltinmodule.c                  ***/
 /***************************************************************/

#ifndef _PSY_BLTINMODULE_H
#define _PSY_BLTINMODULE_H


#include "pycompiler.h"


#define RANGE_LEN           VAR_OB_SIZE       /* for virtual range() only */
#define RANGE_START         (LIST_OB_ITEM+1)  /*       "                  */
/*#define RANGE_STEP          (LIST_OB_ITEM+2)  *       "                  */
/* XXX no support for steps currently. Needs implementation of division to
   figure out the length. */

EXTERNVAR source_virtual_t psyco_computed_range;


EXTERNFN void psy_bltinmodule_init(void);

#endif /* _PSY_BLTINMODULE_H */
