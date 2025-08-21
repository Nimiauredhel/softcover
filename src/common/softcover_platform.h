#ifndef SOFTCOVER_PLATFORM_H
#define SOFTCOVER_PLATFORM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Platform Platform_t;
typedef struct PlatformCapabilities PlatformCapabilities_t;
typedef struct PlatformSettings PlatformSettings_t;

typedef struct Memory Memory_t;

typedef struct Texture Texture_t;

typedef void (*AppSetupFunc)(const Platform_t *platform);
typedef void (*AppInitFunc)(const Platform_t *platform, Memory_t *memory);
typedef void (*AppLoopFunc)(const Platform_t *platform, Memory_t *memory);
typedef void (*AppExitFunc)(const Platform_t *platform, Memory_t *memory);

struct PlatformCapabilities
{
    size_t app_memory_limit_bytes;
    useconds_t gfx_frame_time_min_us;
};

struct PlatformSettings
{
    size_t app_memory_required_bytes;
    useconds_t gfx_frame_time_target_us;
};

struct Platform
{
    // configuration
    const PlatformCapabilities_t *capabilities;
    PlatformSettings_t *settings;
    // memory
    Memory_t* (*memory_allocate)(size_t size);
    void (*memory_release)(Memory_t **memory_pptr);
    // gfx
    void (*gfx_clear_buffer)(void);
    void (*gfx_load_texture)(char *name, Texture_t *dest);
    void (*gfx_draw_texture)(Texture_t *texture, int x, int y);
    // input
    char (*input_read)(void);
    // audio
    void (*audio_play_chunk)(float *chunk, uint16_t len);
    // storage
    void (*storage_save_state)(char *state_name);
    void (*storage_load_state)(char *state_name);
    // utils
    bool (*get_should_terminate)(void);
    void (*set_should_terminate)(bool value);
    void (*debug_log)(char *message);
    void (*debug_break)(void);

};

struct Memory
{
    size_t size_bytes;
    uint8_t buffer[];
};

struct Texture
{
    uint32_t width;
    uint32_t height;
    uint8_t pixel_size_bytes;
    uint8_t pixels[];
};

#endif
