 /***************************************************************/
/***           Psyco equivalent of structmember.h              ***/
 /***************************************************************/

#ifndef _PSY_STRUCTMEMBER_H
#define _PSY_STRUCTMEMBER_H


#include "pobject.h"


EXTERNFN
vinfo_t* PsycoMember_GetOne(PsycoObject* po, vinfo_t* addr, PyMemberDef* l);


inline void psy_structmember_init(void) { }  /* nothing */

#endif /* _PSY_STRUCTMEMBER_H */
