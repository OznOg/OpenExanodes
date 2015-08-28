#ifndef EXAPERF_DURATION_H
#define EXAPERF_DURATION_H

#include "os/include/os_inttypes.h"

/** Duration sensor specific information */
typedef struct {
    bool interleaved;		/**< are the durations parallel or sequentiel */
    double begin_time_cumul; 	/**< cumul of duration begin events */
    double end_time_cumul; 	/**< cumul of duration end events */
    int begin_count;		/**< number of duration begin events */
    int end_count;		/**< number of duration end events */
} exaperf_duration_t;


#endif /* EXAPERF_DURATION_H */
