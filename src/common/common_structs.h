#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Memory Memory_t;
typedef struct Texture Texture_t;
typedef struct AudioClip AudioClip_t;
typedef struct UniformRing UniformRing_t;

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

struct UniformRing
{
    uint32_t capacity;
    uint32_t length;
    uint32_t head;
    uint8_t unit_size;
    uint8_t buffer[];
};

UniformRing_t* ring_create(uint32_t capacity, uint8_t unit_size);
void ring_init(UniformRing_t *ring, uint32_t capacity, uint8_t unit_size);
uint32_t ring_push(UniformRing_t *ring, void *chunk, uint32_t len);
bool ring_pop(UniformRing_t *ring, void *out);
bool ring_peek(const UniformRing_t *ring, uint32_t offset, void *out, bool absolute);
bool ring_peek_ptr(const UniformRing_t *ring, uint32_t offset, void **out_pptr, bool absolute);

#endif
