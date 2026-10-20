/* test_mixer_host.c: host tests for the audio mixer in xenon_audio.cpp.
 *
 * HOST test (Linux). xenon_audio.cpp is compiled without -DXENON so it has no
 * device dependency; we inject PCM through XenonAudio_LoadSlotForTest and drive
 * the mixer through XenonAudio_MixForTest. Verifies voice lifetime, voice
 * stealing, and mixing/clipping behaviour.
 *
 * Build/run: tests/run_host_tests.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "../xenon/source/xenon_audio.h"

/* The audio backend calls FsReadWhole to load effects; the host test injects
 * slots directly, so provide a stub returning failure. */
extern "C" long FsReadWhole(const char *p, void **out) { (void)p; if (out) *out = 0; return -1; }

static int g_fail = 0;
#define CHECK(c, m) do { if(!(c)){printf("  FAIL: %s\n", m); g_fail++;} else printf("  ok  : %s\n", m);} while(0)

static void wr32(unsigned char *b, uint32_t v){ b[0]=v; b[1]=v>>8; b[2]=v>>16; b[3]=v>>24; }
static void wr16(unsigned char *b, uint16_t v){ b[0]=v; b[1]=v>>8; }

/* Build a 16-bit PCM WAV in memory: `frames` samples at `rate`, value `val`. */
static unsigned make_wav(unsigned char *out, int rate, int channels, int frames, int16_t val) {
    unsigned data_len = (unsigned)frames * channels * 2;
    unsigned char *p = out;
    memcpy(p, "RIFF", 4); p += 4; wr32(p, 36 + data_len); p += 4;
    memcpy(p, "WAVE", 4); p += 4;
    memcpy(p, "fmt ", 4); p += 4; wr32(p, 16); p += 4;
    wr16(p, 1); p += 2; wr16(p, channels); p += 2;
    wr32(p, rate); p += 4; wr32(p, (uint32_t)rate*channels*2); p += 4;
    wr16(p, channels*2); p += 2; wr16(p, 16); p += 2;
    memcpy(p, "data", 4); p += 4; wr32(p, data_len); p += 4;
    for (int i = 0; i < frames*channels; i++) { wr16(p, (uint16_t)val); p += 2; }
    return (unsigned)(p - out);
}

int main(void) {
    printf("Audio mixer host tests\n");

    const char *names[3] = { "/s0", "/s1", "/s2" };
    XenonAudio_Init();
    XenonAudio_SetSfxList(names, 3);

    unsigned char wav[4096];
    unsigned n = make_wav(wav, 48000, 1, 480, 30000);  /* 10 ms of a loud tone */
    CHECK(XenonAudio_LoadSlotForTest(0, wav, n) == 1, "load slot 0");
    CHECK(XenonAudio_LoadSlotForTest(1, wav, n) == 1, "load slot 1");

    /* No voices at rest. */
    CHECK(XenonAudio_ActiveVoices() == 0, "no voices before play");

    /* Playing starts exactly one voice. */
    XenonAudio_PlaySfx(0);
    CHECK(XenonAudio_ActiveVoices() == 1, "play starts one voice");

    /* Mixing produces non-silent output. */
    static int16_t out[512*2];
    XenonAudio_MixForTest(out, 512);
    int nonzero = 0;
    for (int i = 0; i < 512*2; i++) if (out[i] != 0) { nonzero = 1; break; }
    CHECK(nonzero, "mixer produces non-silent output");

    /* Two voices sum without exceeding int16 range. */
    XenonAudio_StopAll();
    XenonAudio_PlaySfx(0);
    XenonAudio_PlaySfx(1);
    CHECK(XenonAudio_ActiveVoices() == 2, "two independent voices active");
    XenonAudio_MixForTest(out, 512);
    /* int16 cannot exceed its own range; assert the mixer returned int16 values
     * (i.e. did not alias) by checking the accumulation saturated instead. */
    int sawClip = 0;
    for (int i = 0; i < 512*2; i++) {
        if (out[i] == 32767 || out[i] == -32768) sawClip = 1;
    }
    CHECK(sawClip, "two loud voices saturate instead of wrapping");

    /* A short effect retires itself after it is consumed. */
    XenonAudio_StopAll();
    XenonAudio_PlaySfx(0);
    /* 480 frames at 48 kHz = 480 output frames; mix 512 to run past the end. */
    XenonAudio_MixForTest(out, 512);
    CHECK(XenonAudio_ActiveVoices() == 0, "short effect retires when finished");

    /* Voice stealing: play more than MAX_VOICES, count stays capped (16). */
    XenonAudio_StopAll();
    for (int i = 0; i < 40; i++) XenonAudio_PlaySfx(i % 2);
    int nv = XenonAudio_ActiveVoices();
    CHECK(nv <= 16, "active voices capped at 16 under overflow");
    CHECK(nv >= 1, "voices still active after overflow");

    /* Out-of-range play is ignored (no crash, no voice). */
    XenonAudio_StopAll();
    XenonAudio_PlaySfx(-1);
    XenonAudio_PlaySfx(99);
    CHECK(XenonAudio_ActiveVoices() == 0, "out-of-range play ignored");

    /* Rebuilding the slot list must clear voices (stale index safety). */
    XenonAudio_PlaySfx(0);
    CHECK(XenonAudio_ActiveVoices() == 1, "voice active before list rebuild");
    XenonAudio_SetSfxList(names, 3);
    CHECK(XenonAudio_ActiveVoices() == 0, "SetSfxList clears stale voices");

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
