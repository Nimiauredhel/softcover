#ifndef TERMINAL_DEBUG_H
#define TERMINAL_DEBUG_H

#include <stdint.h>

void debug_log(char *message);
void debug_break(void);
void debug_init(void);

#define DEBUG_RING_CAPACITY (4)
#define DEBUG_MESSAGE_MAX_LEN (256)

typedef struct DebugRing
{
    uint8_t head;
    uint8_t len;
    char debug_messages[DEBUG_RING_CAPACITY][DEBUG_MESSAGE_MAX_LEN];
} DebugRing_t;

#endif
