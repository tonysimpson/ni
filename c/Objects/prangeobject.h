 /***************************************************************/
/***            Psyco equivalent of rangeobject.h              ***/
 /***************************************************************/

#ifndef _PSY_RANGEOBJECT_H
#define _PSY_RANGEOBJECT_H


#include "pobject.h"
#include "pabstract.h"


/*  This is private in rangeobject.c
 */

struct rangeobject {
	PyObject_HEAD
	long	start;
	long	step;
	long	len;
};


#define RANGEOBJECT_start  DEF_FIELD(struct rangeobject, long, start, DF_ob_type)
#define RANGEOBJECT_step   DEF_FIELD(struct rangeobject, long, step,  RANGEOBJECT_start)
#define RANGEOBJECT_len    DEF_FIELD(struct rangeobject, long, len,   RANGEOBJECT_step)


EXTERNFN vinfo_t* PsycoRange_New(PsycoObject* po, vinfo_t* start,
                                 vinfo_t* len, vinfo_t* step);


#endif /* _PSY_LISTOBJECT_H */
