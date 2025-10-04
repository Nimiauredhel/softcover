#include "app_common.h"

#include "app_memory.h"
#include "app_entity.h"
#include "app_scene.h"
#include "app_gfx.h"
#include "app_control.h"

/// must match prototype @ref AppSetupFunc
void app_setup(Platform_t *interface)
{
    static char debug_buff[128] = {0};

    platform = interface;

    platform->settings->gfx_frame_time_target_us = 16666;

    platform->settings->gfx_pixel_size_bytes = 3;
    platform->settings->gfx_buffer_width = APP_GFX_TILE_WIDTH_PX * APP_GFX_VIEWPORT_WIDTH_TILES;
    platform->settings->gfx_buffer_height = APP_GFX_TILE_HEIGHT_PX * APP_GFX_VIEWPORT_HEIGHT_TILES;

    size_t required_gfx_memory = 
    platform->settings->gfx_pixel_size_bytes *
    platform->settings->gfx_buffer_width *
    platform->settings->gfx_buffer_height;

    platform->settings->audio_channels = 2;
    platform->settings->audio_buffer_capacity = 32768;

    platform->settings->input_buffer_capacity = 128;

    size_t required_state_memory = sizeof(AppSerializableState_t) + sizeof(AppEphemeralState_t);

    size_t required_memory_total = required_state_memory + required_gfx_memory
        + sizeof(UniformRing_t) + (sizeof(float) * platform->settings->audio_buffer_capacity)
        + sizeof(UniformRing_t) + (sizeof(int) * platform->settings->input_buffer_capacity);
    
    bool sufficient = platform->capabilities->app_memory_max_bytes >= required_memory_total;

    snprintf(debug_buff, sizeof(debug_buff), "App requires %lu bytes of memory, Platform offers up to %lu bytes - %s.",
            required_memory_total, platform->capabilities->app_memory_max_bytes, sufficient ? "proceeding" : "aborting");
    platform->debug_log(debug_buff);

    if (sufficient)
    {
        platform->settings->app_memory_serializable_bytes = sizeof(AppSerializableState_t);
        platform->settings->app_memory_ephemeral_bytes = sizeof(AppEphemeralState_t);
    }
    else
    {
        platform->set_should_terminate(true);
    }
}

/**
 * @brief Called on app start,
 * TODO: also call after serializable state is loaded, or following an executable hot-reload.
 * Must match prototype @ref AppInitFunc.
 */
void app_init(const Platform_t *interface, AppMemoryPartition_t *memory)
{
    platform = interface;

    platform->debug_log("App initializing fresh memory partition.");

    if (memory == NULL)
    {
        platform->debug_log("Null memory partition pointer provided by platform.");
        platform->set_should_terminate(true);
        return;
    }

    input_buffer = memory->input_buffer;
    gfx_buffer = memory->gfx_buffer;
    audio_buffer = memory->audio_buffer;
    ephemerals = (AppEphemeralState_t *)memory->ephemeral->buffer;
    serializables = (AppSerializableState_t *)memory->serializable->buffer;

    /// EPHEMERALS
    load_ephemerals();

    /// SERIALIZABLES

    if (serializables->initialized)
    {
        platform->debug_log("Loaded serializable state already initialized.");
    }
    else
    {
        platform->debug_log("Initializing app serializable state.");

        explicit_bzero(serializables, sizeof(*serializables));

        /// initialize default hardcoded control mapping
        /// TODO: load control config from text file
        serializables->controller_mapping[APPCTRLIDX_MOVE_UP] = 'w';
        serializables->controller_mapping[APPCTRLIDX_MOVE_DOWN] = 's';
        serializables->controller_mapping[APPCTRLIDX_MOVE_LEFT] = 'a';
        serializables->controller_mapping[APPCTRLIDX_MOVE_RIGHT] = 'd';
        serializables->controller_mapping[APPCTRLIDX_SWITCH] = 'q';
        serializables->controller_mapping[APPCTRLIDX_SAVE] = '2';
        serializables->controller_mapping[APPCTRLIDX_LOAD] = '1';
        serializables->controller_mapping[APPCTRLIDX_QUIT] = 'x';
        serializables->controller_mapping[APPCTRLIDX_VOLUME_UP] = '=';
        serializables->controller_mapping[APPCTRLIDX_VOLUME_DOWN] = '-';
        serializables->controller_mapping[APPCTRLIDX_DEBUG_GFX] = '0';
        serializables->controller_mapping[APPCTRLIDX_RELOAD_EPH] = '9';

        load_scene_by_index(0);

        serializables->controlled_entity_idx = 0;
        serializables->focal_entity_idx = 0;
        serializables->mov_speed = 1;

        serializables->initialized = true;
    }

    entities_initialize_draw_order();
    entities_update_draw_order();
}

/// must match prototype @ref AppLoopFunc
void app_loop(void)
{
    input_read_all();
    input_process_all();
    entities_update_draw_order();
    gfx_clear_buffer();

    if(debug_gfx)
    {
        gfx_draw_all_entities_debug();
    }
    else
    {
        gfx_draw_all_entities();
    }
}

/// must match prototype @ref AppExitFunc
void app_exit(void)
{
}
