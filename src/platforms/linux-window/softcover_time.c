#include "softcover_time.h"

static struct timespec last_cycle_start_clock = {0};
static int64_t last_cycle_elapsed_us = 0;
static int64_t last_cycle_leftover_us = 0;

/**
 * @brief Returns the microseconds elapsed since a given clock value.
 */
int64_t time_us_since_clock(struct timespec *start_clock)
{
    static const int64_t ns_to_us_divisor = 1000;
    static const int64_t s_to_us_multiplier = 1000000;

    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    int64_t elapsed_us =
      ((now_clock.tv_nsec - start_clock->tv_nsec) / ns_to_us_divisor)
    + ((now_clock.tv_sec - start_clock->tv_sec) * s_to_us_multiplier);
    return elapsed_us;
}

/**
 * @brief Returns the milliseconds elapsed since a given clock value.
 */
int64_t time_ms_since_clock(struct timespec *start_clock)
{
    static const int64_t ns_to_ms_divisor = 1000000;
    static const int64_t s_to_ms_multiplier = 1000;

    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    int64_t elapsed_ms =
      ((now_clock.tv_nsec - start_clock->tv_nsec) / ns_to_ms_divisor)
    + ((now_clock.tv_sec - start_clock->tv_sec) * s_to_ms_multiplier);
    return elapsed_ms;
}

/**
 * @brief Returns the seconds elapsed since a given clock value.
 */
float time_s_since_clock(struct timespec *start_clock)
{
    static const float ns_to_s_float_multiplier = 1000000000.0f;

    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    float elapsed_s_float =
      ((now_clock.tv_nsec - start_clock->tv_nsec) / ns_to_s_float_multiplier)
    + ((now_clock.tv_sec - start_clock->tv_sec));
    return elapsed_s_float;
}

void time_mark_cycle_start(void)
{
    clock_gettime(CLOCK_MONOTONIC, &last_cycle_start_clock);
}

void time_mark_cycle_end(int64_t time_target_us)
{
    last_cycle_elapsed_us = time_us_since_clock(&last_cycle_start_clock);
    last_cycle_leftover_us = time_target_us - last_cycle_elapsed_us;
}

int64_t time_get_delta_us(void)
{
    return last_cycle_elapsed_us;
}

int64_t time_get_leftover_us(void)
{
    return last_cycle_leftover_us;
}
