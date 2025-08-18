#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "portaudio.h"

typedef struct AudioBuffer
{
    uint16_t size;
    uint16_t head;
    uint16_t tail;
    float buffer[];
} AudioBuffer_t;

PaStream* init_audio(AudioBuffer_t *user_buffer);
void set_audio(PaStream *stream, bool active);
void write_audio(PaStream *stream, const float *input, const uint16_t len);
void deinit_audio(PaStream *stream);

#endif
