#include "terminal_portaudio.h"

#include <stdlib.h>
#include <string.h>

#include "portaudio.h"

static PaStream *audio_stream = NULL;
static float audio_volume = 1.0f;

static int paStreamCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    /* Prevent unused variable warning. */
    (void) inputBuffer;

    /* Cast data passed through stream to our structure. */
    FloatRing_t *data = (FloatRing_t*)userData; 

    float *out = (float*)outputBuffer;
    
    for(uint32_t i = 0; i < framesPerBuffer; i++)
    {
        if (data->length <= 0)
        {
            out[i] = 0.0f;
            continue;
        }

        out[i] = data->buffer[data->head] * audio_volume;
        data->head = data->head + 1;
        if (data->head >= data->capacity) data->head -= data->capacity;
        data->length--;
    }

    return 0;
}

static void set_audio(PaStream *stream, bool active)
{
    PaError err = paNoError;
    bool was_active = Pa_IsStreamActive(stream);

    if (active != was_active)
    {
        if (active) err = Pa_StartStream(stream);
        else err = Pa_StopStream(stream);
    }

    if(err != paNoError)
    {
        // TODO: handle error
    }
}

float audio_get_volume(void)
{
    return audio_volume;
}

void audio_set_volume(float value)
{
    if (value > 2.0f || value < 0.0f) return;
    audio_volume = value;
}

void audio_init(PlatformSettings_t *settings, FloatRing_t **audio_buffer_pptr)
{
    /// init audio buffer
    *audio_buffer_pptr = (FloatRing_t *)malloc(sizeof(FloatRing_t) + (sizeof(float) * settings->audio_buffer_capacity));
    FloatRing_t *audio_buffer = (FloatRing_t *)*audio_buffer_pptr;
    memset(audio_buffer, 0, sizeof(*audio_buffer));
    audio_buffer->capacity = settings->audio_buffer_capacity;

    /// init portaudio
    PaError err;

    err = Pa_Initialize();

    if(err != paNoError)
    {
        // TODO: handle error
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
        // TODO: handle error
    }

    set_audio(audio_stream, true);
}

void audio_deinit(void)
{
    PaError err;

    if (audio_stream != NULL)
    {
        set_audio(audio_stream, false);
        err = Pa_CloseStream(audio_stream);
        audio_stream = NULL;
    }

    err = Pa_Terminate();

    // TODO: handle PA errors ..
}
