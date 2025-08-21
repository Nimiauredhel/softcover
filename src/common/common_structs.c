#include "common_structs.h"

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
