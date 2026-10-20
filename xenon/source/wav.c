/* wav.c: minimal RIFF/WAVE PCM decoder. See wav.h. */
#include "wav.h"
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

int Wav_LoadMem(const void *data, uint32_t len, WavData *out) {
    const uint8_t *p = (const uint8_t *)data;
    if (!p || !out || len < 44) return 0;
    if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) return 0;

    uint32_t pos = 12;
    uint16_t fmt = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t *pcm = 0;
    uint32_t pcm_len = 0;

    while (pos + 8 <= len) {
        const uint8_t *ch = p + pos;
        uint32_t csize = rd32(ch + 4);
        const uint8_t *body = ch + 8;
        if (pos + 8 + csize > len) csize = len - (pos + 8);

        if (memcmp(ch, "fmt ", 4) == 0 && csize >= 16) {
            fmt      = rd16(body + 0);
            channels = rd16(body + 2);
            rate     = rd32(body + 4);
            bits     = rd16(body + 14);
        } else if (memcmp(ch, "data", 4) == 0) {
            pcm = body;
            pcm_len = csize;
        }
        pos += 8 + csize + (csize & 1u); /* chunks are word-aligned */
    }

    /* Only uncompressed PCM with the shapes we can mix. */
    if (fmt != 1 || !pcm || pcm_len == 0) return 0;
    if (channels != 1 && channels != 2) return 0;
    if (bits != 8 && bits != 16) return 0;
    if (rate == 0 || rate > 192000) return 0;

    uint32_t bytes_per_frame = (uint32_t)channels * (bits / 8u);
    uint32_t frames = pcm_len / bytes_per_frame;
    if (frames == 0) return 0;

    void *copy = malloc(pcm_len);
    if (!copy) return 0;
    memcpy(copy, pcm, pcm_len);

    out->samples  = copy;
    out->frames   = frames;
    out->rate     = rate;
    out->channels = channels;
    out->bits     = bits;
    return 1;
}

void Wav_Free(WavData *w) {
    if (!w) return;
    free(w->samples);
    w->samples = 0;
    w->frames = 0;
}

/* Decode frame `i` (clamped) to stereo int16. WAV PCM is little-endian, so we
 * read bytes explicitly rather than via int16_t* (the Xenon is big-endian). */
static void frame_at(const WavData *w, uint32_t i, int16_t *l, int16_t *r) {
    if (i >= w->frames) i = w->frames - 1;
    if (w->bits == 8) {
        const uint8_t *s = (const uint8_t *)w->samples + (size_t)i * w->channels;
        int16_t a = (int16_t)(((int)s[0] - 128) << 8);
        int16_t b = (w->channels == 2) ? (int16_t)(((int)s[1] - 128) << 8) : a;
        *l = a; *r = b;
    } else {
        const uint8_t *s = (const uint8_t *)w->samples + (size_t)i * w->channels * 2u;
        int16_t a = (int16_t)(uint16_t)(s[0] | (s[1] << 8));
        int16_t b = a;
        if (w->channels == 2) b = (int16_t)(uint16_t)(s[2] | (s[3] << 8));
        *l = a; *r = b;
    }
}

void Wav_SampleStereo(const WavData *w, double pos, int16_t *l, int16_t *r) {
    if (!w || !w->samples || w->frames == 0) { *l = *r = 0; return; }
    uint32_t i;
    double frac;
    if (pos <= 0.0) {
        i = 0;
        frac = 0.0;
    } else {
        uint32_t base = (uint32_t)pos;
        frac = pos - (double)base;
        i = base;
    }
    int16_t l0, r0, l1, r1;
    frame_at(w, i, &l0, &r0);
    frame_at(w, i + 1, &l1, &r1);
    *l = (int16_t)(l0 + (int16_t)(frac * (double)(l1 - l0)));
    *r = (int16_t)(r0 + (int16_t)(frac * (double)(r1 - r0)));
}
