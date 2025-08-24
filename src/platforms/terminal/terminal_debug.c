#include "terminal_debug.h"
#include "terminal_ncurses.h"

static bool debug_is_break = false;
static DebugRing_t debug_ring = {0};

void debug_refresh_gfx(void)
{
    gfx_refresh_debug_window(&debug_ring, debug_is_break);
}

void debug_log(char *message)
{
    uint8_t idx = debug_ring.head + debug_ring.len % DEBUG_RING_CAPACITY;
    snprintf(debug_ring.debug_messages[idx], DEBUG_MESSAGE_MAX_LEN, "%s", message);

    if (debug_ring.len < DEBUG_RING_CAPACITY)
    {
        debug_ring.len++;
    }
    else
    {
        debug_ring.head++;
        if (debug_ring.head >= DEBUG_RING_CAPACITY) debug_ring.head = 0;
    }

    if (gfx_is_initialized())
    {
        debug_refresh_gfx();
    }
    else
    {
        printf("DEBUG: %s\n", message);
    }
}

void debug_break(void)
{
    if (gfx_is_initialized())
    {
        debug_is_break = true;
        debug_refresh_gfx();
    }

    char c = '~';

    while(c != '\n')
    {
        c = gfx_is_initialized() ? input_read() : fgetc(stdin);
    }

    debug_is_break = false;
}

void debug_init(void)
{
    debug_ring.head = 0;
    debug_ring.len = 0;
}
