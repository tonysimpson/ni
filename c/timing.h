 /***************************************************************/
/***                   Measuring processor time                ***/
 /***************************************************************/

#ifndef _TIMING_H
#define _TIMING_H

#include "psyco.h"
#include "Python/pyver.h"


#define TIMING_WITH_TICK_COUNTER   1   /* requires Python 2.2.2 */
#define TIMING_WITH_PENTIUM_TSC    2   /* requires a Pentium */
#define TIMING_WITH_CLOCK          3   /* requires the clock() function */


/* does clock() return an absolute time, or per-process CPU time ?
   Let's try to guess. */
#ifdef MS_WINDOWS
# define CLOCK_IS_PER_PROCESS     0  /* at least on Win9x -- don't know on NT */
#else
# define CLOCK_IS_PER_PROCESS     1  /* by default -- only tested under Linux */
#endif


/* Selection of the timing method */
#if HAVE_PYTHON_SUPPORT
# define TIMING_WITH   TIMING_WITH_TICK_COUNTER
#elif defined(HAVE_CLOCK) && CLOCK_IS_PER_PROCESS
# define TIMING_WITH   TIMING_WITH_CLOCK
#else
# define TIMING_WITH   TIMING_WITH_PENTIUM_TSC
#endif

#define measure_is_zero(m)  ((m) == (time_measure_t) 0)


/***************************************************************/
#if TIMING_WITH == TIMING_WITH_TICK_COUNTER
/***************************************************************/

#define MEASURE_ALL_THREADS    1

typedef int time_measure_t;

inline time_measure_t get_measure(PyThreadState* tstate)
{
	int result = tstate->tick_counter;
	tstate->tick_counter = 0;
	return result;
}

/***************************************************************/
#else /* no tick_counter */
/***************************************************************/

/* without tick_counter, it is hard to tell the threads apart. */
#define MEASURE_ALL_THREADS    0

#if TIMING_WITH == TIMING_WITH_PENTIUM_TSC
#  include "processor.h"
EXTERNVAR psyco_pentium_tsc_fn  psyco_pentium_tsc; /* in processor.c */
#  define CURRENT_TIME_READER   psyco_pentium_tsc
#  define time_measure_t        pentium_tsc_t
#  undef  measure_is_zero
#  define measure_is_zero(m)    0  /* never */
#elif TIMING_WITH == TIMING_WITH_CLOCK
#  ifndef HAVE_CLOCK
#    error "no clock() function, select another timing method in timing.h"
#  endif
#  include <time.h>
#  define CURRENT_TIME_READER   clock
#  define time_measure_t        clock_t
#else
#  error "no valid TIMING_WITH method selected in timing.h"
#endif

/* 'tstate' parameter ignored */
inline time_measure_t get_measure(PyThreadState* tstate)
{
	static time_measure_t prevtime = (time_measure_t) 0;
	time_measure_t curtime = CURRENT_TIME_READER();
	time_measure_t result = curtime - prevtime;
	prevtime = curtime;
	return result;
}

/***************************************************************/
#endif /* tick_counter */
/***************************************************************/


#endif /* _TIMING_H */
