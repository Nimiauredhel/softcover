#include "softcover_debug.h"
#include "softcover_utils.h"
#include "softcover_sdl2.h"

static bool debug_is_break = false;
static DebugRing_t debug_ring = {0};

void debug_dump_log(void)
{
    char buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    if (gfx_is_initialized())
    {
        gfx_refresh_debug_window(&debug_ring, debug_is_break);
    }
    else
    {
        printf("== LOG DUMP ==\n");

        for (uint16_t i = 0; i < debug_ring.len; i++)
        {
            snprintf(buff, sizeof(buff), "%s", debug_ring.debug_messages[(debug_ring.head+i) % DEBUG_RING_CAPACITY]);
            printf("%s\n", buff);
        }

        printf("== END LOG DUMP ==\n");
    }
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
        debug_dump_log();
    }
    else
    {
        printf("DEBUG: %s\n", message);
    }
}

void debug_break(void)
{
    bool resume_for_termination = !should_terminate;

    if (gfx_is_initialized())
    {
        debug_is_break = true;
        debug_dump_log();
    }

    char c = '~';

    while(c != '\n')
    {
        if (resume_for_termination && should_terminate) break;
        c = gfx_is_initialized() ? input_read() : fgetc(stdin);
    }

    debug_is_break = false;
}

void debug_init(void)
{
    debug_ring.head = 0;
    debug_ring.len = 0;
}
