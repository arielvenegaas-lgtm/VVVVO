/* xenon_audio.cpp: software S16-stereo mixer feeding libXenon's xenon_sound.
 *
 * HOST-BUILDABLE CORE is separated from the hardware submit so the mixing and
 * resampling logic can be unit-tested on Linux (see tests/). On the console the
 * mixed buffer is handed to xenon_sound_submit().
 */
#include "xenon_audio.h"
#include "wav.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#ifdef XENON
#include <xenon_sound/sound.h>
#endif

#define OUT_RATE      48000
#define OUT_FRAMES    512              /* frames mixed per submit (= 2 KB) */
#define MAX_VOICES    16

struct Voice {
    int      active;
    int      index;        /* sfx index                              */
    double   pos;          /* fractional source frame position       */
    double   step;         /* source frames per output frame         */
    float    gain;         /* 0..1 fade to avoid clicks on cut-off   */
};

struct SfxSlot {
    char    *path;         /* volume-relative path                   */
    int      state;        /* 0 = unloaded, 1 = loaded, -1 = failed  */
    WavData  wav;
};

static SfxSlot  g_slots[64];
static int      g_slot_count = 0;
static Voice    g_voices[MAX_VOICES];
static int      g_ready = 0;
static int      g_failures = 0;

static int16_t  g_out[OUT_FRAMES * 2];

/* Provided by platform_fs / platform_file: read a whole mapped file. */
extern "C" long FsReadWhole(const char *mapped_path, void **out_buf);

/* --- public API --------------------------------------------------------- */

extern "C" void XenonAudio_Init(void) {
    memset(g_voices, 0, sizeof(g_voices));
    g_ready = 0;
#ifdef XENON
    xenon_sound_init();
    g_ready = 1;
#else
    /* Host build: no DAC, but the mixer still runs for tests. */
    g_ready = 1;
#endif
}

extern "C" int XenonAudio_Ready(void) { return g_ready; }

extern "C" void XenonAudio_StopAll(void) {
    for (int i = 0; i < MAX_VOICES; ++i) g_voices[i].active = 0;
}

extern "C" int XenonAudio_LoadFailures(void) {
    int n = 0;
    for (int i = 0; i < g_slot_count; ++i) if (g_slots[i].state < 0) ++n;
    return n;
}

extern "C" void XenonAudio_SetSfxList(const char *const *paths, int count) {
    if (count > (int)(sizeof(g_slots) / sizeof(g_slots[0])))
        count = (int)(sizeof(g_slots) / sizeof(g_slots[0]));
    for (int i = 0; i < g_slot_count; ++i) free(g_slots[i].path);
    g_slot_count = 0;
    for (int i = 0; i < count; ++i) {
        g_slots[i].path = strdup(paths[i] ? paths[i] : "");
        g_slots[i].state = 0;
        g_slots[i].wav.samples = 0;
        g_slots[i].wav.frames = 0;
        ++g_slot_count;
    }
}

static int EnsureLoaded(int index) {
    if (index < 0 || index >= g_slot_count) return 0;
    SfxSlot *s = &g_slots[index];
    if (s->state == 1) return 1;
    if (s->state == -1) return 0;

    void *buf = 0;
    long len = FsReadWhole(s->path, &buf);
    if (len <= 0 || !buf) { s->state = -1; ++g_failures; return 0; }
    int ok = Wav_LoadMem(buf, (uint32_t)len, &s->wav);
    free(buf);
    if (!ok) { s->state = -1; ++g_failures; return 0; }
    s->state = 1;
    return 1;
}

extern "C" void XenonAudio_PlaySfx(int index) {
    if (!EnsureLoaded(index)) return;

    /* Find a free voice, else steal the oldest (lowest index). */
    int v = -1;
    for (int i = 0; i < MAX_VOICES; ++i) if (!g_voices[i].active) { v = i; break; }
    if (v < 0) v = 0;

    const WavData *w = &g_slots[index].wav;
    g_voices[v].active = 1;
    g_voices[v].index  = index;
    g_voices[v].pos    = 0.0;
    g_voices[v].step   = (double)w->rate / (double)OUT_RATE;
    g_voices[v].gain   = 1.0f;
}

/* --- mixing core (host-testable) --------------------------------------- */

static void MixFrames(int16_t *out, int frames) {
    memset(out, 0, (size_t)frames * 2u * sizeof(int16_t));

    for (int vi = 0; vi < MAX_VOICES; ++vi) {
        Voice *v = &g_voices[vi];
        if (!v->active) continue;
        const WavData *w = &g_slots[v->index].wav;

        for (int f = 0; f < frames; ++f) {
            if (v->pos >= (double)w->frames) { v->active = 0; break; }
            int16_t l, r;
            Wav_SampleStereo(w, v->pos, &l, &r);
            int32_t ol  = out[f * 2 + 0] + (int32_t)(l * v->gain);
            int32_t orr = out[f * 2 + 1] + (int32_t)(r * v->gain);
            if (ol > 32767) ol = 32767; else if (ol < -32768) ol = -32768;
            if (orr > 32767) orr = 32767; else if (orr < -32768) orr = -32768;
            out[f * 2 + 0] = (int16_t)ol;
            out[f * 2 + 1] = (int16_t)orr;
            v->pos += v->step;
        }
    }
}

extern "C" void XenonAudio_Pump(void) {
    if (!g_ready) return;
#ifdef XENON
    /* Only mix when the DAC has room for a full block, so we never submit a
     * huge backlog. */
    if (xenon_sound_get_free() < OUT_FRAMES * 4) return;
    MixFrames(g_out, OUT_FRAMES);
    /* xenon_sound_submit() flushes the submitted buffer out of the data cache
     * itself (see libxenon drivers/xenon_sound/sound.c), so no explicit
     * memdcbst() is required here. */
    xenon_sound_submit(g_out, OUT_FRAMES * 4);
#else
    (void)MixFrames; /* host build has no sink */
#endif
}

/* Exposed for host tests: mix `frames` of audio into `out` without a device. */
extern "C" void XenonAudio_MixForTest(int16_t *out, int frames) {
    MixFrames(out, frames);
}
