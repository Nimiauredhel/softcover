#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include <ncurses.h>
#include "audio.h"

#include "softcover_platform.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

#define MOCK_TEXTURE_MAX_COUNT (32)
#define MOCK_SPRITE_MAX_COUNT (32)

#define WINDOW_HEIGHT (32)
#define WINDOW_WIDTH (128)

static const char *app_init_name = "app_init";
static const char *app_loop_name = "app_loop";

static char lib_path[256] = LIB_NAME;

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

static AppInitFunc app_init;
static AppLoopFunc app_loop;

static PaStream *audio_stream;

typedef struct MockTexture
{
    char name[32];
} MockTexture_t;

typedef struct MockSprite
{
    int texture_idx;
    int x;
    int y;
} MockSprite_t;

static uint8_t mock_texture_count = 0;
static uint8_t mock_sprite_count = 0;

static MockTexture_t mock_texture_storage[MOCK_TEXTURE_MAX_COUNT] = {0};
static MockSprite_t mock_sprite_buffer[MOCK_SPRITE_MAX_COUNT] = {0};

static WINDOW *main_window = NULL;

static AudioBuffer_t *audio_buffer = NULL;

Memory_t* memory_allocate(size_t size)
{
    Memory_t *memory = (Memory_t *)malloc(size + sizeof(Memory_t));
    explicit_bzero(memory->buffer, size);
    memory->size = size;
    return memory;
}

void memory_release(Memory_t *memory)
{
    free(memory);
}

void gfx_clear_buffer(void)
{
    mock_sprite_count = 0;
    //printf("gfx clear buffer.\n");
}

int gfx_load_texture(char *name)
{
    if (mock_texture_count >= MOCK_TEXTURE_MAX_COUNT) return -1;

    strncpy(mock_texture_storage[mock_texture_count].name, name, 32);
    //printf("gfx load texture: %s.\n", name);
    mock_texture_count++;
    return mock_texture_count-1;
}

void gfx_draw_texture(int idx, int x, int y)
{
    if (mock_sprite_count >=MOCK_SPRITE_MAX_COUNT) return;

    //printf("gfx draw texture: idx %d, x %d, y %d.\n", idx, x, y);
    mock_sprite_buffer[mock_sprite_count].texture_idx = idx;
    mock_sprite_buffer[mock_sprite_count].x = x;
    mock_sprite_buffer[mock_sprite_count].y = y;
    mock_sprite_count++;
}

void audio_play_chunk(float *chunk, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        audio_buffer->buffer[audio_buffer->tail] = chunk[i];

        audio_buffer->tail += 1;

        if (audio_buffer->tail >= audio_buffer->size) audio_buffer->tail = 0;

        if (audio_buffer->tail == audio_buffer->head)
        {
            break;
        }
    }
}

volatile char input_read(void)
{
    //printf("input read:\n");
    char c = wgetch(main_window);
    return c;
}

static void feed_audio_stream(void)
{
    static const uint16_t max_frames_per_feed = 1024;

    if (audio_buffer->head != audio_buffer->tail)
    {
        uint16_t head_portion = 0;
        uint16_t tail_portion = 0;

        if (audio_buffer->tail > audio_buffer->head)
        {
            head_portion = audio_buffer->tail - audio_buffer->head;
            tail_portion = 0;
        }
        else
        {
            head_portion = audio_buffer->size - audio_buffer->head;
            tail_portion = audio_buffer->tail;
        }

        if (head_portion >= max_frames_per_feed)
        {
            head_portion = max_frames_per_feed;
            tail_portion = 0;
        }
        else if (head_portion + tail_portion > max_frames_per_feed)
        {
            tail_portion = max_frames_per_feed - head_portion;
        }

        write_audio(audio_stream,
                    audio_buffer->buffer + audio_buffer->head,
                    head_portion);
        audio_buffer->head += head_portion;

        if (tail_portion > 0)
        {
            write_audio(audio_stream,
                        audio_buffer->buffer,
                        tail_portion);
            audio_buffer->head = tail_portion;
        }
    }
}

static void init_ncurses(void)
{
    initscr();

    noecho();
    cbreak();

    main_window = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, 0, 0);

    nodelay(main_window, true);

    werase(main_window);
    wrefresh(main_window);
}

static void gfx_sync_buffer(void)
{
    werase(main_window);

    for (int i = 0; i < mock_sprite_count; i++)
    {
        mvwprintw(main_window,
                (WINDOW_HEIGHT/2)+mock_sprite_buffer[i].y,
                (WINDOW_WIDTH/2)+mock_sprite_buffer[i].x,
                "%s [%d,%d]", mock_texture_storage[mock_sprite_buffer[i].texture_idx].name,
                mock_sprite_buffer[i].y,
                mock_sprite_buffer[i].x);
    }

    wrefresh(main_window);
}

static void load_dynamic(void)
{
    //printf("Loading dynamic library.\n");

    if (lib_handle != NULL)
    {
        //printf("Closing previously loaded handle.\n");
        dlclose(lib_handle);
        lib_handle = NULL;
        func_handle = NULL;
    }

    lib_handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);

    if (lib_handle == NULL )
    {
        //printf("Failed to load [%s]: %s\n", lib_path, dlerror());
        exit(EXIT_FAILURE);
    }

    app_init = dlsym(lib_handle, app_init_name);
    app_loop = dlsym(lib_handle, app_loop_name);

    struct stat lib_stat = {0};
    stat(lib_path, &lib_stat);
    lib_load_time = lib_stat.st_mtime;
}

int main(int argc, char **argv)
{
    static struct stat file_stat = {0};

    printf("Program started.\n");

    if (argc > 1)
    {
        snprintf(lib_path, sizeof(lib_path), "%s", argv[1]);
        printf("Using given library path: %s.\n", lib_path);
    }
    else
    {
        printf("Using default library path: %s.\n", lib_path);
    }

    load_dynamic();

    audio_buffer = malloc(sizeof(AudioBuffer_t) + (sizeof(float) * 2048));
    audio_buffer->head = 0;
    audio_buffer->tail = 0;
    audio_buffer->size = 2048;

    audio_stream = init_audio(audio_buffer);
    set_audio(audio_stream, true);

    printf("Initializing ncurses window.\n");
    init_ncurses();

    Platform_t platform =
    {
        .memory_allocate = memory_allocate,
        .memory_release = memory_release,
        .gfx_clear_buffer = gfx_clear_buffer,
        .gfx_draw_texture = gfx_draw_texture,
        .gfx_load_texture = gfx_load_texture,
        .input_read = input_read,
        .audio_play_chunk = audio_play_chunk,
    };

    Memory_t *app_state_mem = NULL;

    if (app_init != NULL)
    {
        app_init(&platform, &app_state_mem);
    }
    else
    {
    }

    for(;;)
    {
        if (app_loop != NULL)
        {
            app_loop(&platform, app_state_mem);
        }
        else
        {
        }

        gfx_sync_buffer();
        //feed_audio_stream();

        stat(lib_path, &file_stat);
        lib_modified_time = file_stat.st_mtime;

        if (lib_modified_time != lib_load_time)
        {
            //printf("Library modification detected.\n");
            load_dynamic();
        }

        usleep(16000);
    }

    return EXIT_SUCCESS;
}
