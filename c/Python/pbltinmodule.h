 /***************************************************************/
/***               interface to bltinmodule.c                  ***/
 /***************************************************************/

#ifndef _PSY_BLTINMODULE_H
#define _PSY_BLTINMODULE_H


#include "pycompiler.h"


#define RANGE_LEN           iVAR_SIZE       /* for virtual range() only */
#define RANGE_START         LIST_TOTAL      /*       "                  */
/*#define RANGE_STEP        (RANGE_START+1)  *       "                  */
#define RANGE_TOTAL       /*(RANGE_STEP+1)*/    (RANGE_START+1)
/* XXX no support for steps currently. Needs implementation of division to
   figure out the length. */

EXTERNVAR source_virtual_t psyco_computed_range;


#endif /* _PSY_BLTINMODULE_H */
