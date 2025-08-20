#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_PLATFORM_BUFFER_LENGTH (4096)

typedef struct AudioBuffer
{
    uint16_t length;
    uint16_t head;
    uint16_t tail;
    float buffer[AUDIO_PLATFORM_BUFFER_LENGTH];
} AudioBuffer_t;

const AudioBuffer_t* audio_get_buffer_readonly(void);
void audio_play_chunk(float *chunk, uint16_t len);
void audio_init(void);
void audio_deinit(void);

#endif
