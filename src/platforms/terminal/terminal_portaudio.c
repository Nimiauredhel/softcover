#include "terminal_portaudio.h"

#include <stdio.h>
#include <string.h>

#include "portaudio.h"
#include "terminal_debug.h"

static bool audio_is_initialized = false;
static PaStream *audio_stream = NULL;
static float audio_volume = 1.0f;

static int paStreamCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    if (!audio_is_initialized) return 0;

    /* Prevent unused variable warning. */
    (void) inputBuffer;
    (void) timeInfo;
    (void) statusFlags;

    /* Cast data passed through stream to our structure. */
    UniformRing_t *data = (UniformRing_t*)userData; 
    float *out = (float*)outputBuffer;
    
    for(uint32_t i = 0; i < framesPerBuffer; i++)
    {
        if (ring_pop(data, out+i))
        {
            out[i] *= audio_volume;
        }
        else
        {
            out[i] = 0.0f;
            continue;
        }
    }

    return 0;
}

void audio_set_active(bool active)
{
    static char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    if (!audio_is_initialized) return;

    PaError err = paNoError;
    bool was_active = Pa_IsStreamActive(audio_stream);

    if (active != was_active)
    {
        if (active)
        {
            debug_log("Starting audio stream.");
            err = Pa_StartStream(audio_stream);
        }
        else
        {
            debug_log("Stopping audio stream.");
            err = Pa_StopStream(audio_stream); 
        }
    }

    if(err != paNoError)
    {
        snprintf(debug_buff, sizeof(debug_buff), "PortAudio error: %s\n", Pa_GetErrorText(err));
        debug_log(debug_buff);
    }
}

float audio_get_volume(void)
{
    return audio_volume;
}

void audio_set_volume(float value)
{
    if (value > 2.0f) value = 2.0f;
    else if (value < 0.0f) value = 0.0f;
    audio_volume = value;
}

void audio_init(PlatformSettings_t *settings, UniformRing_t **audio_buffer_pptr)
{
    static char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    if (audio_is_initialized) return;

    debug_log("Initializing PortAudio.");

    /// init audio buffer
    *audio_buffer_pptr = ring_create(settings->audio_buffer_capacity, sizeof(float));
    UniformRing_t *audio_buffer = (UniformRing_t *)*audio_buffer_pptr;

    /// init portaudio
    PaError err = paNoError;

    err = Pa_Initialize();

    if(err != paNoError)
    {
        snprintf(debug_buff, sizeof(debug_buff), "PortAudio error: %s\n", Pa_GetErrorText(err));
        debug_log(debug_buff);
    }

    PaStreamParameters output_parameters = {0};
    output_parameters.device = Pa_GetDefaultOutputDevice(); /* default input device */
    output_parameters.channelCount = 2;
    output_parameters.sampleFormat = paFloat32;
    output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&audio_stream,
                               NULL,
                               &output_parameters,
                               44100.0,
                               paFramesPerBufferUnspecified,        
                               paNoFlag,
                               paStreamCallback,
                               audio_buffer);
    if(err != paNoError)
    {
        snprintf(debug_buff, sizeof(debug_buff), "PortAudio error: %s\n", Pa_GetErrorText(err));
        debug_log(debug_buff);
    }

    audio_is_initialized = true;
    audio_set_active(true);
}

void audio_deinit(void)
{
    static char debug_buff[DEBUG_MESSAGE_MAX_LEN] = {0};

    if (!audio_is_initialized) return;

    debug_log("Deinitializing PortAudio.");

    PaError err = paNoError;

    if (audio_stream != NULL)
    {
        audio_set_active(false);
        err = Pa_CloseStream(audio_stream);
        audio_stream = NULL;

        if(err != paNoError)
        {
            snprintf(debug_buff, sizeof(debug_buff), "PortAudio error: %s\n", Pa_GetErrorText(err));
            debug_log(debug_buff);
        }
    }

    err = Pa_Terminate();

    if(err != paNoError)
    {
        snprintf(debug_buff, sizeof(debug_buff), "PortAudio error: %s\n", Pa_GetErrorText(err));
        debug_log(debug_buff);
    }

    audio_is_initialized = false;
}
