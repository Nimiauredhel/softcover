#ifndef TERMINAL_UTILS_H
#define TERMINAL_UTILS_H

#include <stdbool.h>
#include <time.h>

#include "softcover_platform.h"

/**
 * @brief Global flag set by OS termination signals
 * and polled by functions to allow graceful termination.
 */
extern bool should_terminate;

void initialize_signal_handler(void);
void initialize_random_seed(void);
void signal_handler(int signum);
int random_range(int min, int max);
float seconds_since_clock(struct timespec *start_clock);
void gfx_load_texture(char *name, TextureRGB_t *dest);

#endif
