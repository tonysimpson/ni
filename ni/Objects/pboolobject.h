#ifndef _PSY_BOOLOBJECT_H
#define _PSY_BOOLOBJECT_H
#include "../psyco.h"
#include "pobject.h"
#include "pabstract.h"

/* XXX tony: reimplement an optimised version of this */
PSY_INLINE vinfo_t* PsycoBool_FromCondition(PsycoObject *po, condition_code_t cc)
{
  vinfo_t *vc, *result;
  vc = psyco_vinfo_condition(po, cc);
  result = psyco_generic_call(po, PyBool_FromLong, CfCommonNewRefPyObject, "v", vc);
  vinfo_decref(vc, po);
  return result;
}

#endif /* _PSY_INTOBJECT_H */
