 /***************************************************************/
/***                   Measuring processor time                ***/
 /***************************************************************/

#ifndef _ITIMING_H
#define _ITIMING_H


#include "iencoding.h"

/* Pentium-specific Time Stamp Counter registers */
/* This file can be empty for processors without specific hardware timers */
#if PENTIUM_INSNS   /* only when Pentium instructions are enabled */


#if HAVE_LONG_LONG
typedef PY_LONG_LONG pentium_tsc_t;
#else
typedef long pentium_tsc_t;
#endif

typedef pentium_tsc_t (*psyco_pentium_tsc_fn) (void);

EXTERNVAR psyco_pentium_tsc_fn  psyco_pentium_tsc;
#define CURRENT_TIME_READER   psyco_pentium_tsc
#define time_measure_t        pentium_tsc_t
#undef  measure_is_zero
#define measure_is_zero(m)    0  /* never */

#define PENTIUM_TSC  /* for iprocessor.c */


#endif /* PENTIUM_INSNS */
#endif /* _ITIMING_H */
