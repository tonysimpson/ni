 /***************************************************************/
/***           Psyco equivalent of structmember.h              ***/
 /***************************************************************/

#ifndef _PSY_STRUCTMEMBER_H
#define _PSY_STRUCTMEMBER_H
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */


#include "pobject.h"


EXTERNFN
vinfo_t* PsycoMember_GetOne(PsycoObject* po, vinfo_t* addr, PyMemberDef* l);


#endif /* NEW_STYLE_TYPES */
#endif /* _PSY_STRUCTMEMBER_H */
