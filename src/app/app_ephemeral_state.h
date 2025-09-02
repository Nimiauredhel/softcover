#ifndef APP_EPHEMERAL_STATE_H
#define APP_EPHEMERAL_STATE_H

#define APP_BUMP_SIZE (1024*2048)
#define APP_SCRATCH_SIZE (4096)

#define APP_TEXTURES_MAX_COUNT (64)
#define APP_SOUNDS_MAX_COUNT (64)

#define APP_LAYER_COUNT (6)
#define APP_ENTITY_DEFS_MAX_COUNT (128)

#include "common_interface.h"

#include "app_entity.h"
#include "app_scene.h"
#include "app_control.h"

typedef struct AppEphemeralState
{
    char debug_buff[DEBUG_MESSAGE_MAX_LEN];
    uint8_t scratch[APP_SCRATCH_SIZE];

    AppControllerState_t controller_state;

    uint16_t definitions_count;
    EntityDefinition_t definitions[APP_ENTITY_DEFS_MAX_COUNT];
    Sprite_t sprites[APP_ENTITY_DEFS_MAX_COUNT];
    Collider_t colliders[APP_ENTITY_DEFS_MAX_COUNT];
    SoundEmitter_t sound_emitters[APP_ENTITY_DEFS_MAX_COUNT];

    uint16_t sounds_count;
    size_t sound_offsets[APP_SOUNDS_MAX_COUNT];

    uint16_t textures_count;
    size_t texture_offsets[APP_TEXTURES_MAX_COUNT];

    int32_t entities_draw_order_layer_offsets[APP_LAYER_COUNT];
    uint16_t entities_draw_order[SCENE_ENTITIES_MAX_COUNT];

    size_t bump_used;
    uint8_t bump_buffer[APP_BUMP_SIZE];
} AppEphemeralState_t;

extern AppEphemeralState_t *ephemerals;

#endif
