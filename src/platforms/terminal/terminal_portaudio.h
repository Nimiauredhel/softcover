#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#include "common_interface.h"
#include "common_structs.h"

float audio_get_volume(void);
void audio_set_volume(float value);
void audio_init(PlatformSettings_t *settings, UniformRing_t **audio_buffer_pptr);
void audio_deinit(void);

#endif
