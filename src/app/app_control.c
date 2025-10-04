#include "app_control.h"
#include "app_common.h"
#include "app_memory.h"
#include "app_gfx.h"

static void input_process_state(AppControlIndex_t idx)
{
    int32_t latest = ephemerals->controller_state.latest[idx];
    int32_t prev = ephemerals->controller_state.prev[idx];

    /// 0b000000xx -> [7...2][1: is down][0: is changed]
    /// 0 nothing, 1 key up, 2 key held, 3 key down
    AppControlEventType_t event = (latest != prev) | ((latest > 0) << 1);

    /// since event type has been determined, we don't need prev anymore so we can set it to latest
    ephemerals->controller_state.prev[idx] = latest;

    switch (event)
    {
        case APPCTRLEVT_NONE:
            break;
        case APPCTRLEVT_DOWN:
            break;
        case APPCTRLEVT_UP:
            switch (idx)
            {
                case APPCTRLIDX_SWITCH:
                    serializables->controlled_entity_idx = serializables->controlled_entity_idx == 0 ? 1 : 0;
                    serializables->focal_entity_idx = serializables->focal_entity_idx == 0 ? 1 : 0;
                    break;
                case APPCTRLIDX_LOAD:
                    platform->storage_load_state("test_save");
                    break;
                case APPCTRLIDX_SAVE:
                    platform->storage_save_state("test_save");
                    break;
                case APPCTRLIDX_QUIT:
                    platform->set_should_terminate(true);
                    break;
                case APPCTRLIDX_VOLUME_UP:
                    platform->audio_set_volume(platform->audio_get_volume() + 0.1f);
                    break;
                case APPCTRLIDX_VOLUME_DOWN:
                    platform->audio_set_volume(platform->audio_get_volume() - 0.1f);
                    break;
                    /// DEBUG KEYS
                case APPCTRLIDX_DEBUG_GFX:
                    debug_gfx = !debug_gfx;
                    break;
                case APPCTRLIDX_RELOAD_EPH:
                    load_ephemerals();
                    break;
                default:
                    break;
            }
            break;
        case APPCTRLEVT_HOLD:
            switch (idx)
            {
                /// TODO: take entity_move calls out of here and use some sort of command queue instead
                case APPCTRLIDX_MOVE_UP:
                    entity_move(serializables->controlled_entity_idx, 0, -serializables->mov_speed);
                    break;
                case APPCTRLIDX_MOVE_LEFT:
                    entity_move(serializables->controlled_entity_idx, -serializables->mov_speed, 0);
                    break;
                case APPCTRLIDX_MOVE_DOWN:
                    entity_move(serializables->controlled_entity_idx, 0, serializables->mov_speed);
                    break;
                case APPCTRLIDX_MOVE_RIGHT:
                    entity_move(serializables->controlled_entity_idx, serializables->mov_speed, 0);
                    break;
                default:
                    break;
            }
            break;
    }
}

void input_process_all(void)
{
    for (AppControlIndex_t k = 0; k < APP_CONTROL_COUNT; k++)
    {
        input_process_state(k);
    }
}

bool input_read_from_buffer(UniformRing_t *input_buffer, InputEvent_t *out)
{
    if (input_buffer == NULL) return false;
    return ring_pop(input_buffer, out);
}

void input_read_all(void)
{
    if (platform == NULL || serializables == NULL) return;

    InputEvent_t input = {0};

    while(input_read_from_buffer(input_buffer, &input))
    {
        for (uint16_t k = 0; k < APP_CONTROL_COUNT; k++)
        {
            if (serializables->controller_mapping[k] == input.key)
            {
                ephemerals->controller_state.latest[k] = input.value;
                break;
            }
        }
    }
}
