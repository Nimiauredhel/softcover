#ifndef TERMINAL_TIME_H
#define TERMINAL_TIME_H

#include <stdint.h>
#include <time.h>

int64_t time_us_since_clock(struct timespec *start_clock);
int64_t time_ms_since_clock(struct timespec *start_clock);
float time_seconds_since_clock(struct timespec *start_clock);
void time_mark_cycle_start(void);
void time_mark_cycle_end(int64_t time_target_us);
int64_t time_get_delta_us(void);
int64_t time_get_leftover_us(void);

#endif
