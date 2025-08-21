#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#include "common_interface.h"
#include "common_structs.h"

void audio_init(PlatformSettings_t *settings, FloatRing_t **audio_buffer_pptr);
void audio_deinit(void);

#endif
