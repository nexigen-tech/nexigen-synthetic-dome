#include <stdint.h>

/* Chronos runtime time types — generated from nxd-runtime-chronos. */


#ifndef NXD_RUNTIME_CHRONOS_H
#define NXD_RUNTIME_CHRONOS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Logical simulation time.
 */
typedef struct {
    int64_t ns;
} SimTime;

/**
 * Host monotonic clock observation.
 */
typedef struct {
    int64_t ns;
} WallTime;

/**
 * Common hardware/FPGA time base when running HIL.
 */
typedef struct {
    int64_t ns;
} HwFpgaTime;

/**
 * Signed elapsed time between two instants.
 */
typedef struct {
    int64_t ns;
} Duration;

#endif  /* NXD_RUNTIME_CHRONOS_H */
