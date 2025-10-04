#ifndef APP_ENTITY_H
#define APP_ENTITY_H

#include "app_common.h"

#define ENTITY_DEF_NAME_MAX_LEN (16)

typedef enum EntityFlags
{
    ENTITY_FLAGS_NONE = 0x00,
    ENTITY_FLAGS_TRANSFORM = 0x01,
    ENTITY_FLAGS_TEXTURE = 0x02,
    ENTITY_FLAGS_COLLISION = 0x04,
    ENTITY_FLAGS_MOVEMENT = 0x08,
    ENTITY_FLAGS_CONTROL = 0x10,
    ENTITY_FLAGS_SOUND = 0x20,
} EntityFlags_t;

typedef enum CollisionFlags
{
    COLL_FLAGS_NONE = 0x00,
    COLL_FLAGS_BLOCK = 0x01,
    COLL_FLAGS_PLAY_SOUND = 0x02,
    COLL_FLAGS_SET_SCENE = 0x04,
    COLL_FLAGS_SET_POSITION = 0x08,
    COLL_FLAGS_CALLBACK = 0x10,
} CollisionFlags_t;

typedef struct EntityDefinition
{
    char name[ENTITY_DEF_NAME_MAX_LEN];
    EntityFlags_t flags;
} EntityDefinition_t;

typedef struct Sprite
{
    size_t texture_idx;
    int16_t x_offset;
    int16_t y_offset;
} Sprite_t;

typedef struct Collider
{
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
    CollisionFlags_t flags;
    /// will hold sound idx, scene idx, position x, position y, callback index,
    /// until morale/architecture improves
    uint16_t params[5];
} Collider_t;

typedef struct SoundEmitter
{
    size_t move_sfx_idx;
} SoundEmitter_t;

typedef struct Transform
{
    int16_t x_pos;
    int16_t y_pos;
} Transform_t;

typedef struct MoveTriggerLocal
{
    bool relative;
    int16_t x_pos;
    int16_t y_pos;
} MoveTriggerLocal_t;

typedef struct MoveTriggerScene
{
    bool relative;
    int32_t index;
} MoveTriggerScene_t;

typedef struct Entity
{
    bool used;
    bool definition_is_local;
    uint16_t definition_idx;
    uint8_t layer;
    Transform_t transform;
} Entity_t;

EntityDefinition_t* entity_get_definition(uint16_t entity_idx);
Sprite_t* entity_get_sprite(uint16_t entity_idx);
Collider_t* entity_get_collider(uint16_t entity_idx);
SoundEmitter_t* entity_get_sounds(uint16_t entity_idx);

int32_t entity_create(int32_t scene_idx, uint16_t definition_idx, bool local_def, uint8_t layer, int32_t x, int32_t y);
void entities_initialize_draw_order(void);
void entities_update_draw_order(void);
void entities_get_distance(uint16_t first_entity_id, uint16_t second_entity_id, int32_t *x_dist_out, int32_t *y_dist_out);
uint16_t entity_test_collision(uint16_t entity_id);
void entity_move(uint16_t entity_idx, int16_t x_delta, int16_t y_delta);
void entity_set_pos(uint32_t entity_id, int32_t x, int32_t y);
int8_t entities_compare_y(uint16_t first_id, uint16_t second_id);
int8_t entities_compare_layer(uint16_t first_id, uint16_t second_id);
void entities_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(uint16_t, uint16_t));

#endif

