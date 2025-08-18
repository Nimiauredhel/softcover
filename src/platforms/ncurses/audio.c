#include "audio.h"
#include <stdlib.h>
#include <stdio.h>

static int paStreamCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    /* Prevent unused variable warning. */
    (void) inputBuffer;

    /* Cast data passed through stream to our structure. */
    AudioBuffer_t *data = (AudioBuffer_t*)userData; 

    float *out = (float*)outputBuffer;
    
    for(uint32_t i = 0; i < framesPerBuffer; i++)
    {
        if (data->head == data->tail)
        {
            out[i] = 0.0f;
            continue;
        }

        out[i] = data->buffer[data->head];
        data->head++;
        if (data->head >= data->size) data->head = 0;
    }

    return 0;
}

PaStream* init_audio(AudioBuffer_t *user_buffer)
{
    PaStream *new_stream;
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

    err = Pa_OpenStream(&new_stream,
                               NULL,
                               &output_parameters,
                               44100.0,
                               paFramesPerBufferUnspecified,        
                               paNoFlag,
                               paStreamCallback,
                               user_buffer);
    if(err != paNoError)
    {
        // TODO: handle error
    }

    return new_stream;
}

void set_audio(PaStream *stream, bool active)
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

void write_audio(PaStream *stream, const float *input, const uint16_t len)
{
    PaError err = paNoError;

    Pa_WriteStream(stream, input, len);

    if(err != paNoError)
    {
        // TODO: handle error
    }
}

void deinit_audio(PaStream *stream)
{
    /*
    PaError err;

    err = Pa_CloseStream(stream);
    err = Pa_Terminate();

    // TODO: handle PA errors ..
    */
}
