#include "app_audio.h"
#include "app_common.h"
#include "app_memory.h"

void audio_push_samples(float *samples, uint32_t len, uint8_t channels)
{
    if (audio_buffer == NULL) return;

    if (channels == platform->settings->audio_channels)
    {
        ring_push(audio_buffer, samples, len, false);
    }
    else
    {
        for (uint32_t i = 0; i < len; i+= channels)
        {
            for (uint8_t j = 0; j < platform->settings->audio_channels; j++)
            {
                ring_push(audio_buffer, samples+i+j, 1, false);
            }
        }
    }
}

void audio_push_clip(AudioClip_t *clip)
{
    audio_push_samples(clip->samples, clip->num_samples, clip->num_channels);
}
