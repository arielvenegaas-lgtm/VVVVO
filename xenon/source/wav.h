/* wav.h: minimal RIFF/WAVE PCM decoder (libXenon Xbox 360 port).
 *
 * Supports uncompressed PCM (format 1): 8- and 16-bit, mono or stereo, any
 * sample rate. The decoded samples are kept in the source format and channel
 * count; the mixer resamples/converts at play time.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void    *samples;      /* malloc'd PCM data                          */
    uint32_t frames;       /* number of sample frames (per channel)      */
    uint32_t rate;         /* sample rate in Hz                          */
    uint16_t channels;     /* 1 or 2                                     */
    uint16_t bits;         /* 8 or 16                                    */
} WavData;

/* Loads a WAV file (already mapped/opened through the game's FILE layer or
 * read into memory). Returns 1 on success, 0 on failure. */
int  Wav_LoadMem(const void *data, uint32_t len, WavData *out);
void Wav_Free(WavData *w);

/* Reads one stereo frame at fractional frame position `pos` (linear
 * interpolation), scaled to signed 16-bit. Returns frames consumed = 1. */
void Wav_SampleStereo(const WavData *w, double pos, int16_t *l, int16_t *r);

#ifdef __cplusplus
}
#endif
