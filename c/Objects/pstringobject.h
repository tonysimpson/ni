 /***************************************************************/
/***           Psyco equivalent of stringobject.h              ***/
 /***************************************************************/

#ifndef _PSY_STRINGOBJECT_H
#define _PSY_STRINGOBJECT_H


#include "pobject.h"
#include "pabstract.h"


/* all flavors of virtual strings */
#define VIRTUALSTR_FIRST  FIELDS_TOTAL(FIX_size)
#define VIRTUALSTR_LAST   (VIRTUALSTR_FIRST+1)   /* <-- keep up-to-date !! */

/* virtual string slices */
#define STRSLICE_SOURCE   VIRTUALSTR_FIRST
#define STRSLICE_START    (STRSLICE_SOURCE+1)
#define STRSLICE_TOTAL    (STRSLICE_START+1)

/* virtual string concatenations */
#define CATSTR_LIST       VIRTUALSTR_FIRST
#define CATSTR_TOTAL      (CATSTR_LIST+1)

/* all string representations end with the actual character data: */
#define STR_ob_sval       FARRAY(UNSIGNED_FIELD(PyStringObject, char, ob_sval, \
                                                (defield_t) VIRTUALSTR_LAST))
#define iSTR_OB_SVAL      FIELD_INDEX(STR_ob_sval)

#define CHARACTER_char    UNSIGNED_FIELD(PyStringObject, char, ob_sval, \
                                         (defield_t) VIRTUALSTR_LAST)
#define iCHARACTER_CHAR   FIELD_INDEX(CHARACTER_char)
#define CHARACTER_TOTAL   FIELDS_TOTAL(CHARACTER_char)

#define CHARACTER_short   FARRAY(UNSIGNED_FIELD(PyStringObject, short,ob_sval, \
                                         (defield_t) VIRTUALSTR_LAST))
#define CHARACTER_long    FARRAY(UNSIGNED_FIELD(PyStringObject, long, ob_sval, \
                                         (defield_t) VIRTUALSTR_LAST))


#define PsycoString_Check(tp) PyType_TypeCheck(tp, &PyString_Type)
#ifdef Py_USING_UNICODE
# define PsycoUnicode_Check(tp) PyType_TypeCheck(tp, &PyUnicode_Type)
#else
# define PsycoUnicode_Check(tp)                 0
#endif


inline vinfo_t* PsycoString_AS_STRING(PsycoObject* po, vinfo_t* v)
{	/* no type check */
	return integer_add_i(po, v, offsetof(PyStringObject, ob_sval), false);
}
inline vinfo_t* PsycoString_GET_SIZE(PsycoObject* po, vinfo_t* v)
{	/* no type check */
	return psyco_get_const(po, v, FIX_size);
}


EXTERNFN vinfo_t* PsycoCharacter_New(vinfo_t* chrval);


#endif /* _PSY_STRINGOBJECT_H */
