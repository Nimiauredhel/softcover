#ifndef SOFTCOVER_DEBUG_H
#define SOFTCOVER_DEBUG_H

#include <stdint.h>

#include "common_interface.h"

#define DEBUG_RING_CAPACITY (256)

typedef struct DebugRing
{
    uint16_t head;
    uint16_t len;
    char debug_messages[DEBUG_RING_CAPACITY][DEBUG_MESSAGE_MAX_LEN];
} DebugRing_t;

void debug_dump_log(void);
void debug_log(char *message);
void debug_break(void);
void debug_init(void);

#endif
