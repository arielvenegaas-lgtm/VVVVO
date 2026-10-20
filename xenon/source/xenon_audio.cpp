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
#define MAX_SLOTS     64

/* The DAC ring in libXenon holds 65536 bytes. We keep it topped up to this
 * high-water mark each Pump() so one mix per 30 Hz game frame (33 ms ≈ 1600
 * frames ≈ 6400 bytes) cannot underrun; we stop early if the ring is full. */
#define RING_BYTES        65536
#define RING_HIGH_WATER   (RING_BYTES - 8192)

struct Voice {
    int      active;
    int      index;        /* sfx index                              */
    double   pos;          /* fractional source frame position       */
    double   step;         /* source frames per output frame         */
    float    gain;         /* 0..1   (reserved for release ramps)    */
    uint32_t seq;          /* start order, for oldest-voice stealing */
};

struct SfxSlot {
    char    *path;         /* volume-relative path                   */
    int      state;        /* 0 = unloaded, 1 = loaded, -1 = failed  */
    WavData  wav;
};

static SfxSlot  g_slots[MAX_SLOTS];
static int      g_slot_count = 0;
static Voice    g_voices[MAX_VOICES];
static int      g_ready = 0;
static int      g_failures = 0;
static uint32_t g_seq = 0;

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

static void FreeSlots(void) {
    for (int i = 0; i < MAX_SLOTS; ++i) {
        free(g_slots[i].path);
        g_slots[i].path = 0;
        if (g_slots[i].wav.samples) { Wav_Free(&g_slots[i].wav); }
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
    }
    g_slot_count = 0;
}

static void ResetSlots(const char *const *paths, int count) {
    FreeSlots();
    if (count > MAX_SLOTS) count = MAX_SLOTS;
    for (int i = 0; i < count; ++i) {
        g_slots[i].path = strdup(paths[i] ? paths[i] : "");
        g_slots[i].state = 0;
        ++g_slot_count;
    }
}

/* Rebuilding the table invalidates any in-flight voices, so stop first. */
extern "C" void XenonAudio_SetSfxList(const char *const *paths, int count) {
    XenonAudio_StopAll();
    ResetSlots(paths, count);
}

static SfxSlot *ValidSlot(int index) {
    if (index < 0 || index >= g_slot_count) return 0;
    return &g_slots[index];
}

static int EnsureLoaded(int index) {
    SfxSlot *s = ValidSlot(index);
    if (!s) return 0;
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

/* Decodes every effect now (called once at startup) so the first use of an
 * effect does not stall the render thread on USB I/O. Returns #loaded. */
extern "C" int XenonAudio_Preload(void) {
    int n = 0;
    for (int i = 0; i < g_slot_count; ++i) if (EnsureLoaded(i)) ++n;
    return n;
}

extern "C" void XenonAudio_PlaySfx(int index) {
    if (!EnsureLoaded(index)) return;

    /* Prefer a free voice; otherwise steal the one started longest ago. */
    int v = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!g_voices[i].active) { v = i; break; }
        if (g_voices[i].seq < oldest) { oldest = g_voices[i].seq; v = i; }
    }
    if (v < 0) v = 0;

    const WavData *w = &g_slots[index].wav;
    g_voices[v].active = 1;
    g_voices[v].index  = index;
    g_voices[v].pos    = 0.0;
    g_voices[v].step   = (double)w->rate / (double)OUT_RATE;
    g_voices[v].gain   = 1.0f;
    g_voices[v].seq    = ++g_seq;
}

/* --- mixing core (host-testable) --------------------------------------- */

static void MixFrames(int16_t *out, int frames) {
    memset(out, 0, (size_t)frames * 2u * sizeof(int16_t));

    for (int vi = 0; vi < MAX_VOICES; ++vi) {
        Voice *v = &g_voices[vi];
        if (!v->active) continue;
        SfxSlot *s = ValidSlot(v->index);
        /* A voice must reference a loaded slot; otherwise retire it. */
        if (!s || s->state != 1 || !s->wav.samples || s->wav.frames == 0) {
            v->active = 0;
            continue;
        }
        const WavData *w = &s->wav;

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
    /* Keep the DAC ring filled across the whole 30 Hz frame interval. A single
     * OUT_FRAMES block (10.7 ms) would underrun between frames, so submit
     * blocks until we reach the high-water mark or the ring is full. */
    int guard = (RING_BYTES / (OUT_FRAMES * 4)) + 1;
    while (guard-- > 0) {
        if (xenon_sound_get_unplayed() >= RING_HIGH_WATER) break;
        if (xenon_sound_get_free() < OUT_FRAMES * 4) break;
        MixFrames(g_out, OUT_FRAMES);
        /* xenon_sound_submit() flushes the buffer out of the data cache itself
         * (see libxenon drivers/xenon_sound/sound.c), so no explicit memdcbst()
         * is needed here. */
        xenon_sound_submit(g_out, OUT_FRAMES * 4);
    }
#else
    (void)MixFrames; /* host build has no sink */
#endif
}

/* Exposed for host tests: mix `frames` of audio into `out` without a device. */
extern "C" void XenonAudio_MixForTest(int16_t *out, int frames) {
    MixFrames(out, frames);
}

extern "C" int XenonAudio_ActiveVoices(void) {
    int n = 0;
    for (int i = 0; i < MAX_VOICES; ++i) if (g_voices[i].active) ++n;
    return n;
}

extern "C" int XenonAudio_LoadSlotForTest(int index, const void *wav_bytes, unsigned len) {
    if (index < 0 || index >= g_slot_count) return 0;
    SfxSlot *s = &g_slots[index];
    if (s->state == 1 && s->wav.samples) Wav_Free(&s->wav);
    if (!Wav_LoadMem(wav_bytes, len, &s->wav)) { s->state = -1; return 0; }
    s->state = 1;
    return 1;
}
