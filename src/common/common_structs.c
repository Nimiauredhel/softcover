#include "common_structs.h"
#include <string.h>

UniformRing_t* ring_create(uint32_t capacity, uint8_t unit_size)
{
    UniformRing_t *ring_ptr = malloc(sizeof(UniformRing_t) + (capacity * unit_size));
    ring_init(ring_ptr, capacity, unit_size);
    return ring_ptr;
}

void ring_init(UniformRing_t *ring, uint32_t capacity, uint8_t unit_size)
{
    ring->capacity = capacity;
    ring->unit_size = unit_size;
    ring->head = 0;
    ring->length = 0;
    bzero(ring->buffer, ring->capacity * ring->unit_size);
}

uint32_t ring_push(UniformRing_t *ring, void *chunk, uint32_t len)
{
    uint8_t *cast_chunk = (uint8_t *)chunk;
    uint16_t start_idx = (ring->head + ring->length) % ring->capacity;
    uint32_t pushed_count = 0;

    for (uint32_t i = 0; i < len; i++)
    {
        if (ring->length >= ring->capacity) break;

        uint32_t idx = start_idx + i;
        if (idx >= ring->capacity) idx -= ring->capacity;
        memcpy(ring->buffer+(idx*ring->unit_size), cast_chunk+(i*ring->unit_size), ring->unit_size);

        pushed_count++;
        ring->length++;
    }

    return pushed_count;
}

bool ring_pop(UniformRing_t *ring, void *out)
{
    uint8_t *cast_out = (uint8_t *)out;

    if (ring->length > 0)
    {
        memcpy(cast_out, ring->buffer+(ring->head*ring->unit_size), ring->unit_size);
        ring->head = (ring->head + 1) % ring->capacity;
        if (ring->head >= ring->capacity) ring->head = 0;
        ring->length--;
        return true;
    }

    return false;
}

/**
 * @brief Writes a value from a given offset into the ring buffer to a given 'out' location, without modifying the contents of the ring buffer.
 *
 * @details
 * Lenient! Will happily provide 'dead' values that have already been 'popped' as well as wrap around the buffer's capacity indefinitely.
 * The caller is responsible to check the return value which indicates whether the value received is fit for purpose.
 *
 * @param [in]  ring     The ring buffer to be read.
 * @param [in]  offset   The offset at which to read into the ring buffer.
 * @param [out] out      The destination to which the read value will be written.
 * @param [in]  absolute If true, the offset will absolute from index 0. If false, the offset will be from the current 'head'.
 *
 * @retval true  The value written to 'out' originated from the "live" section of the ring and hasn't been "popped" yet.
 * @retval false The value written to 'out' originated from the "dead" section of the ring and has already been "popped".
 */
bool ring_peek(const UniformRing_t *ring, uint32_t offset, void *out, bool absolute)
{
    uint8_t *cast_out = (uint8_t *)out;

    uint32_t peek_idx = (absolute ? offset : (ring->head + offset)) % ring->capacity;
    memcpy(cast_out, ring->buffer+(peek_idx*ring->unit_size), ring->unit_size);

    return (peek_idx - ring->head) < ring->length;
}

/**
 * @brief Writes the address of a given offset into the ring buffer to a given 'out' pointer, without modifying the contents of the ring buffer.
 *
 * @details
 * Lenient! Will happily point to 'dead' values that have already been 'popped' as well as wrap around the buffer's capacity indefinitely.
 * The caller is responsible to check the return value which indicates whether the value received is fit for purpose.
 *
 * @param [in]  ring     The ring buffer to be read.
 * @param [in]  offset   The offset at which to read into the ring buffer.
 * @param [out] out      The destination pointer to which the requested value's address will be written.
 * @param [in]  absolute If true, the offset will absolute from index 0. If false, the offset will be from the current 'head'.
 *
 * @retval true  The address written to 'out' pointed to to the "live" section of the ring which hasn't been "popped" yet.
 * @retval false The address written to 'out' pointed to to the "dead" section of the ring which has already been "popped".
 */
bool ring_peek_ptr(const UniformRing_t *ring, uint32_t offset, void **out_pptr, bool absolute)
{
    uint32_t peek_idx = (absolute ? offset : (ring->head + offset)) % ring->capacity;
    *out_pptr = (void *)(ring->buffer+(peek_idx*ring->unit_size));

    return (peek_idx - ring->head) < ring->length;
}
