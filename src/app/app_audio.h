#ifndef APP_AUDIO_H
#define APP_AUDIO_H

#include "app_common.h"

void audio_push_samples(float *samples, uint32_t len, uint8_t channels);
void audio_push_clip(AudioClip_t *clip);

#endif
