 /***************************************************************/
/***           Psyco equivalent of stringobject.h              ***/
 /***************************************************************/

#ifndef _PSY_STRINGOBJECT_H
#define _PSY_STRINGOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define STR_ob_sval       FARRAY(UNSIGNED_FIELD(PyStringObject, char, ob_sval, \
                                                FIX_size))
#define iSTR_OB_SVAL      FIELD_INDEX(STR_ob_sval)

#define CHARACTER_char    UNSIGNED_FIELD(PyStringObject, char, ob_sval, \
                                         FIX_size)
#define iCHARACTER_CHAR   FIELD_INDEX(CHARACTER_char)
#define CHARACTER_TOTAL   FIELDS_TOTAL(CHARACTER_char)

#define CHARACTER_short   UNSIGNED_FIELD(PyStringObject, short,ob_sval, FIX_size)
#define CHARACTER_long    UNSIGNED_FIELD(PyStringObject, long, ob_sval, FIX_size)


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
