 /***************************************************************/
/***           Psyco equivalent of methodobject.h              ***/
 /***************************************************************/

#ifndef _PSY_METHODOBJECT_H
#define _PSY_METHODOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define CFUNC_M_ML          QUARTER(offsetof(PyCFunctionObject, m_ml))
#define CFUNC_M_SELF        QUARTER(offsetof(PyCFunctionObject, m_self))


EXTERNFN void psy_methodobject_init(void);

#endif /* _PSY_METHODOBJECT_H */
