 /***************************************************************/
/***             Psyco equivalent of abstract.h                ***/
 /***************************************************************/

#ifndef _PSY_ABSTRACT_H
#define _PSY_ABSTRACT_H


#include "pobject.h"
#include "../pycencoding.h"


/* #ifndef BINARY_FLOOR_DIVIDE */
/* # define PyNumber_FloorDivide          NULL */
/* # define PyNumber_TrueDivide           NULL */
/* # define PyNumber_InPlaceFloorDivide   NULL */
/* # define PyNumber_InPlaceTrueDivide    NULL */
/* #endif */


/* 'kw' may be NULL */
EXTERNFN vinfo_t* PsycoObject_Call(PsycoObject* po, vinfo_t* callable_object,
                                   vinfo_t* args, vinfo_t* kw);

EXTERNFN vinfo_t* PsycoObject_Size(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* PsycoObject_GetItem(PsycoObject* po, vinfo_t* o, vinfo_t* key);
EXTERNFN bool PsycoObject_SetItem(PsycoObject* po, vinfo_t* o, vinfo_t* key,
                                  vinfo_t* value);

EXTERNFN vinfo_t* PsycoSequence_GetItem(PsycoObject* po, vinfo_t* o, vinfo_t* i);
EXTERNFN bool     PsycoSequence_SetItem(PsycoObject* po, vinfo_t* o, vinfo_t* i,
                                        vinfo_t* value);
EXTERNFN vinfo_t* PsycoSequence_GetSlice(PsycoObject* po, vinfo_t* o,
                                         vinfo_t* ilow, vinfo_t* ihigh);
EXTERNFN bool     PsycoSequence_SetSlice(PsycoObject* po, vinfo_t* o,
                                         vinfo_t* ilow, vinfo_t* ihigh,
                                         vinfo_t* value);

EXTERNFN vinfo_t* PsycoNumber_Positive(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* PsycoNumber_Negative(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* PsycoNumber_Invert(PsycoObject* po, vinfo_t* vi);

EXTERNFN vinfo_t* PsycoNumber_Add(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Or(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Xor(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_And(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Lshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Rshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Subtract(PsycoObject* po, vinfo_t* v1, vinfo_t*v2);
EXTERNFN vinfo_t* PsycoNumber_Multiply(PsycoObject* po, vinfo_t* v1, vinfo_t*v2);
EXTERNFN vinfo_t* PsycoNumber_Divide(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Divmod(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* PsycoNumber_Remainder(PsycoObject* po, vinfo_t* v1,vinfo_t*v2);
EXTERNFN vinfo_t* PsycoNumber_Power(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,
                                    vinfo_t* v3);

EXTERNFN
vinfo_t* PsycoNumber_InPlaceAdd(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceOr(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceXor(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceAnd(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceLshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceRshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceSubtract(PsycoObject* po, vinfo_t* v1, vinfo_t*v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceMultiply(PsycoObject* po, vinfo_t* v1, vinfo_t*v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceDivide(PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlaceRemainder(PsycoObject* po, vinfo_t* v1,vinfo_t*v2);
EXTERNFN
vinfo_t* PsycoNumber_InPlacePower(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,
                                  vinfo_t* v3);

   /* Attention! This does not catch PyExc_StopIteration.
      As with all meta-functions, when it returns NULL there is
      an exception set. All iterators raise PyExc_StopIteration at
      the meta-level (because this is not time-consuming, the
      exceptions being virtualized and not really set at Python's
      eyes): */
EXTERNFN vinfo_t* PsycoIter_Next(PsycoObject* po, vinfo_t* iter);
EXTERNFN vinfo_t* PsycoObject_GetIter(PsycoObject* po, vinfo_t* vi);


/* generic len() function that reads the object's ob_size field;
   see e.g. plistobject.c */
EXTERNFN vinfo_t* psyco_generic_immut_ob_size(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* psyco_generic_mut_ob_size(PsycoObject* po, vinfo_t* vi);


inline void psy_abstract_init(void) { }  /* nothing */

#endif /* _PSY_ABSTRACT_H */
