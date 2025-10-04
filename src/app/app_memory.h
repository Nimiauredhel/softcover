#ifndef APP_MEMORY_H
#define APP_MEMORY_H

#include "common_structs.h"
#include "app_control.h"
#include "app_scene.h"
#include "app_entity.h"

#define APP_BUMP_SIZE (1024*2048)
#define APP_SCRATCH_SIZE (4096)

#define APP_TEXTURES_MAX_COUNT (64)
#define APP_SOUNDS_MAX_COUNT (64)

#define APP_LAYER_COUNT (6)
#define APP_ENTITY_DEFS_MAX_COUNT (128)

#define APP_STATE_MAX_SCENES (16)

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

typedef struct AppSerializableState
{
    bool initialized;
    int32_t controller_mapping[APP_CONTROL_COUNT];
    uint16_t mov_speed;
    uint16_t controlled_entity_idx;
    uint16_t focal_entity_idx;
    uint8_t current_scene_index;
    uint8_t scene_count;
    Scene_t scenes[APP_STATE_MAX_SCENES];
} AppSerializableState_t;

extern UniformRing_t *input_buffer;
extern Texture_t *gfx_buffer;
extern UniformRing_t *audio_buffer;

extern AppEphemeralState_t *ephemerals;
extern AppSerializableState_t *serializables;

size_t load_texture_to_memory(char *name);
size_t load_wav_to_memory(char *name);

void load_definitions_all(void);

void load_ephemerals(void);

int32_t global_definition_get_idx_by_name(char *name);
int32_t local_definition_get_idx_by_name(char *name);
int32_t definition_clone_to_local(int32_t src_idx, bool src_is_local, char *clone_name);

#endif
