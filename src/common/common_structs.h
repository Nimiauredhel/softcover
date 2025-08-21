#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include <stdlib.h>
#include <stdint.h>

typedef struct Memory Memory_t;
typedef struct Texture Texture_t;
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

struct FloatRing
{
    uint16_t capacity;
    uint16_t length;
    uint16_t head;
    float buffer[];
};

void float_ring_push(FloatRing_t *ring, float *chunk, uint16_t len);

#endif
