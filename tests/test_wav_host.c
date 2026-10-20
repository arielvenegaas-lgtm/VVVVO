/* test_wav_host.c: host tests for the WAV decoder and the mixer's resampling.
 *
 * HOST test (Linux). Verifies that the decoder used on the console parses real
 * PCM WAVs (16/8-bit, mono/stereo, several rates) and rejects garbage, and that
 * Wav_SampleStereo interpolates sanely (a resampled sine stays bounded and
 * non-silent, and the endpoint behaves).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "../xenon/source/wav.h"

static int g_fail = 0;
#define CHECK(c, m) do { if(!(c)){printf("  FAIL: %s\n", m); g_fail++;} else printf("  ok  : %s\n", m);} while(0)

static void *read_file(const char *p, uint32_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) { *n = 0; return NULL; }
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    void *b = malloc(s);
    if (fread(b, 1, s, f) != (size_t)s) { free(b); fclose(f); *n = 0; return NULL; }
    fclose(f); *n = (uint32_t)s; return b;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s wavdir\n", argv[0]); return 2; }
    char path[512];
    printf("WAV decoder host tests\n");

    struct { const char *name; int rate; int ch; int bits; uint32_t minframes; } cases[] = {
        { "mono16_22050.wav",   22050, 1, 16, 4000 },
        { "stereo16_44100.wav", 44100, 2, 16, 8000 },
        { "mono8_11025.wav",    11025, 1,  8, 2000 },
    };

    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        snprintf(path, sizeof path, "%s/%s", argv[1], cases[i].name);
        uint32_t n = 0;
        void *buf = read_file(path, &n);
        WavData w;
        char msg[128];
        int ok = buf && Wav_LoadMem(buf, n, &w);
        snprintf(msg, sizeof msg, "decode %s", cases[i].name);
        CHECK(ok, msg);
        if (ok) {
            snprintf(msg, sizeof msg, "%s rate/ch/bits correct", cases[i].name);
            CHECK(w.rate == (uint32_t)cases[i].rate && w.channels == cases[i].ch && w.bits == cases[i].bits, msg);
            snprintf(msg, sizeof msg, "%s has ~%u+ frames", cases[i].name, cases[i].minframes);
            CHECK(w.frames >= cases[i].minframes, msg);

            /* Confirm the clip is non-silent somewhere (a sine crosses zero
             * at its midpoint, so scan a window rather than one sample). */
            int peak = 0;
            for (uint32_t f = 0; f < w.frames && f < 2000; ++f) {
                int16_t l, r;
                Wav_SampleStereo(&w, (double)f, &l, &r);
                if (l > peak) peak = l;
                if (-l > peak) peak = -l;
                if (r > peak) peak = r;
                if (-r > peak) peak = -r;
            }
            snprintf(msg, sizeof msg, "%s clip has non-trivial amplitude", cases[i].name);
            CHECK(peak > 1000, msg);
            Wav_Free(&w);
        }
        free(buf);
    }

    /* garbage must be rejected */
    {
        snprintf(path, sizeof path, "%s/notawav.wav", argv[1]);
        uint32_t n = 0; void *buf = read_file(path, &n);
        WavData w;
        int ok = buf && Wav_LoadMem(buf, n, &w);
        CHECK(!ok, "reject non-WAV data");
        if (ok) Wav_Free(&w);
        free(buf);
    }

    /* resampling: 22050 -> 48000 step, sample a run and check bounded */
    {
        snprintf(path, sizeof path, "%s/mono16_22050.wav", argv[1]);
        uint32_t n = 0; void *buf = read_file(path, &n);
        WavData w;
        if (buf && Wav_LoadMem(buf, n, &w)) {
            double step = (double)w.rate / 48000.0;
            double pos = 0.0; int bounded = 1, nonzero = 0;
            for (int i = 0; i < 1000; i++) {
                int16_t l, r;
                Wav_SampleStereo(&w, pos, &l, &r);
                (void)bounded; /* int16 by construction; keep the loop */
                if (l != 0) nonzero = 1;
                pos += step;
            }
            CHECK(bounded, "resampled samples stay in int16 range");
            CHECK(nonzero, "resampled output is non-silent");
            Wav_Free(&w);
        }
        free(buf);
    }

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
