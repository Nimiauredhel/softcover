#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Memory Memory_t;
typedef struct Texture Texture_t;
typedef struct AudioClip AudioClip_t;
typedef struct ByteRing ByteRing_t;
typedef struct FloatRing FloatRing_t;

struct Memory
{
    size_t size_bytes;
    uint8_t buffer[];
};

struct Texture
{
    uint16_t width;
    uint16_t height;
    uint8_t pixel_size_bytes;
    uint8_t pixels[];
};

struct AudioClip
{
    uint32_t num_samples;
    float samples[];
};

struct ByteRing
{
    uint16_t capacity;
    uint16_t length;
    uint16_t head;
    uint8_t buffer[];
};

struct FloatRing
{
    uint16_t capacity;
    uint16_t length;
    uint16_t head;
    float buffer[];
};

void byte_ring_push(ByteRing_t *ring, uint8_t *chunk, uint16_t len);
bool byte_ring_pop(ByteRing_t *ring, uint8_t *out);
void float_ring_push(FloatRing_t *ring, float *chunk, uint16_t len);
bool float_ring_pop(FloatRing_t *ring, float *out);

#endif
