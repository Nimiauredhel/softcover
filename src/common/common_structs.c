#include "common_structs.h"

void int_ring_push(IntRing_t *ring, int *chunk, uint16_t len)
{
    uint16_t start_idx = (ring->head + ring->length);
    if (start_idx >= ring->capacity) start_idx -= ring->capacity;

    for (int i = 0; i < len; i++)
    {
        if (ring->length >= ring->capacity) break;

        uint16_t idx = start_idx + i;
        if (idx >= ring->capacity) idx -= ring->capacity;
        ring->buffer[idx] = chunk[i];

        ring->length++;
    }
}

bool int_ring_pop(IntRing_t *ring, int *out)
{
    if (ring->length > 0)
    {
        *out = ring->buffer[ring->head];
        ring->head++;
        if (ring->head >= ring->capacity) ring->head = 0;
        ring->length--;
        return true;
    }

    return false;
}

void byte_ring_push(ByteRing_t *ring, uint8_t *chunk, uint16_t len)
{
    uint16_t start_idx = (ring->head + ring->length);
    if (start_idx >= ring->capacity) start_idx -= ring->capacity;

    for (int i = 0; i < len; i++)
    {
        if (ring->length >= ring->capacity) break;

        uint16_t idx = start_idx + i;
        if (idx >= ring->capacity) idx -= ring->capacity;
        ring->buffer[idx] = chunk[i];

        ring->length++;
    }
}

bool byte_ring_pop(ByteRing_t *ring, uint8_t *out)
{
    if (ring->length > 0)
    {
        *out = ring->buffer[ring->head];
        ring->head++;
        if (ring->head >= ring->capacity) ring->head = 0;
        ring->length--;
        return true;
    }

    return false;
}

void float_ring_push(FloatRing_t *ring, float *chunk, uint16_t len)
{
    uint16_t start_idx = (ring->head + ring->length);
    if (start_idx >= ring->capacity) start_idx -= ring->capacity;

    for (int i = 0; i < len; i++)
    {
        if (ring->length >= ring->capacity) break;

        uint16_t idx = start_idx + i;
        if (idx >= ring->capacity) idx -= ring->capacity;
        ring->buffer[idx] = chunk[i];

        ring->length++;
    }
}

bool float_ring_pop(FloatRing_t *ring, float *out)
{
    if (ring->length > 0)
    {
        *out = ring->buffer[ring->head];
        ring->head++;
        if (ring->head >= ring->capacity) ring->head = 0;
        ring->length--;
        return true;
    }

    return false;
}
