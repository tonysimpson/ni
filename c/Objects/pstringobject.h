 /***************************************************************/
/***           Psyco equivalent of stringobject.h              ***/
 /***************************************************************/

#ifndef _PSY_STRINGOBJECT_H
#define _PSY_STRINGOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define STR_OB_SVAL         QUARTER(offsetof(PyStringObject, ob_sval))


#define PsycoString_Check(tp) PyType_TypeCheck(tp, &PyString_Type)
#ifdef Py_USING_UNICODE
# define PsycoUnicode_Check(tp) PyType_TypeCheck(tp, &PyUnicode_Type)
#else
# define PsycoUnicode_Check(tp)                 0
#endif


inline vinfo_t* PsycoString_AS_STRING(PsycoObject* po, vinfo_t* v)
{	/* no type check */
	return integer_add_i(po, v, offsetof(PyStringObject, ob_sval));
}
inline vinfo_t* PsycoString_GET_SIZE(PsycoObject* po, vinfo_t* v)
{	/* no type check */
	return get_array_item(po, v, VAR_OB_SIZE);
}


EXTERNFN vinfo_t* PsycoCharacter_New(vinfo_t* chrval);


EXTERNFN void psy_stringobject_init(void);  /* nothing */

#endif /* _PSY_STRINGOBJECT_H */
