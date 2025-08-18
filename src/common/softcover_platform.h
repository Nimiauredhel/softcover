#ifndef SOFTCOVER_PLATFORM_H
#define SOFTCOVER_PLATFORM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Platform Platform_t;
typedef struct Memory Memory_t;
typedef struct TextureRGB TextureRGB_t;

typedef void (*AppInitFunc)(const Platform_t *platform, Memory_t **memory_pptr);
typedef void (*AppLoopFunc)(const Platform_t *platform, Memory_t *memory);
typedef void (*AppExitFunc)(const Platform_t *platform, Memory_t *memory);

struct Platform
{
    // memory
    Memory_t* (*memory_allocate)(size_t size);
    void (*memory_release)(Memory_t **memory_pptr);
    // gfx
    void (*gfx_clear_buffer)(void);
    void (*gfx_load_texture)(char *name, TextureRGB_t *dest);
    void (*gfx_draw_texture)(TextureRGB_t *texture, int x, int y);
    // input
    volatile char (*input_read)(void);
    // audio
    void (*audio_play_chunk)(float *chunk, uint16_t len);
    // storage
    void (*storage_save_state)(char *state_name);
    void (*storage_load_state)(char *state_name);
    // utils
    bool (*get_should_terminate)(void);
    void (*set_should_terminate)(bool value);

};

struct Memory
{
    size_t size;
    uint8_t buffer[];
};

struct TextureRGB
{
    uint32_t width;
    uint32_t height;
    uint8_t *pixels;
};

#endif
