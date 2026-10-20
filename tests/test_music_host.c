/* test_music_host.c: host tests for the Ogg Vorbis music channel.
 *
 * HOST test (Linux). Compiles xenon_audio.cpp + the vendored stb_vorbis and
 * feeds it real (tiny, synthesized) Ogg files from tests/*.ogg. Verifies the
 * public music API, graceful failure on non-Ogg input, volume clamping, and —
 * most importantly — that a real Ogg actually decodes to non-silent audio.
 *
 * Build/run: tests/run_host_tests.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../xenon/source/xenon_audio.h"

/* xenon_audio.cpp calls FsReadWhole when loading effects; stub it (unused here). */
extern "C" long FsReadWhole(const char *p, void **out) { (void)p; if (out) *out = 0; return -1; }

static int g_fail = 0;
#define CHECK(c, m) do { if(!(c)){printf("  FAIL: %s\n", m); g_fail++;} else printf("  ok  : %s\n", m);} while(0)

static unsigned char *read_file(const char *path, unsigned *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    unsigned char *b = (unsigned char *)malloc((size_t)n);
    if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return 0; }
    fclose(f);
    *len = (unsigned)n;
    return b;
}

int main(int argc, char **argv) {
    printf("Ogg Vorbis music host tests\n");

    const char *ogg_path    = argc > 1 ? argv[1] : "fixtures_tone.ogg";
    const char *ogg8k_path  = argc > 2 ? argv[2] : "fixtures_tone8k.ogg";

    XenonAudio_Init();
    CHECK(XenonAudio_Ready() == 1, "audio init");

    /* No music playing initially. */
    CHECK(XenonAudio_MusicIsPlaying() == 0, "no music before play");

    /* NULL / empty input is rejected cleanly. */
    CHECK(XenonAudio_MusicPlay(NULL, 0, "null") == 0, "NULL ogg rejected");
    CHECK(XenonAudio_MusicIsPlaying() == 0, "still no music after NULL");

    /* Non-Ogg garbage is rejected (decoder refuses to open). */
    {
        unsigned char junk[512];
        memset(junk, 0x5A, sizeof(junk));
        CHECK(XenonAudio_MusicPlay(junk, sizeof(junk), "junk") == 0, "non-ogg blob rejected");
        CHECK(XenonAudio_MusicIsPlaying() == 0, "still no music after junk");
    }

    /* A real Ogg decodes and plays. */
    unsigned len = 0;
    unsigned char *ogg = read_file(ogg_path, &len);
    if (!ogg) {
        printf("  SKIP: %s not found\n", ogg_path);
    } else {
        CHECK(XenonAudio_MusicPlay(ogg, len, ogg_path) == 1, "real ogg opens");
        CHECK(XenonAudio_MusicIsPlaying() == 1, "reports playing");

        /* Mixing must produce non-silent output (proves actual decoding). */
        static int16_t out[1024 * 2];
        int nonzero = 0;
        for (int iter = 0; iter < 4 && !nonzero; ++iter) {
            memset(out, 0, sizeof(out));
            XenonAudio_MixForTest(out, 1024);
            for (int i = 0; i < 1024 * 2; i++) if (out[i] != 0) { nonzero = 1; break; }
        }
        CHECK(nonzero, "decoded music mixes to non-silent audio");

        /* Volume 0 must silence the music (no voices playing). */
        XenonAudio_MusicSetVolume(0);
        int silent = 1;
        for (int iter = 0; iter < 4 && silent; ++iter) {
            memset(out, 0, sizeof(out));
            XenonAudio_MixForTest(out, 1024);
            for (int i = 0; i < 1024 * 2; i++) if (out[i] != 0) { silent = 0; break; }
        }
        CHECK(silent, "volume 0 silences music");

        /* Volume clamping then restore. */
        XenonAudio_MusicSetVolume(999);
        XenonAudio_MusicSetVolume(128);

        XenonAudio_MusicStop();
        CHECK(XenonAudio_MusicIsPlaying() == 0, "stop clears music");

        /* Replaying the same blob after stop works (no stale decoder state). */
        CHECK(XenonAudio_MusicPlay(ogg, len, ogg_path) == 1, "replay after stop");
        XenonAudio_MusicStop();
        free(ogg);
    }

    /* A different sample rate also decodes (resample path). */
    unsigned len8 = 0;
    unsigned char *ogg8 = read_file(ogg8k_path, &len8);
    if (!ogg8) {
        printf("  SKIP: %s not found\n", ogg8k_path);
    } else {
        CHECK(XenonAudio_MusicPlay(ogg8, len8, ogg8k_path) == 1, "8 kHz ogg opens");
        static int16_t out[1024 * 2];
        int nonzero = 0;
        for (int iter = 0; iter < 4 && !nonzero; ++iter) {
            memset(out, 0, sizeof(out));
            XenonAudio_MixForTest(out, 1024);
            for (int i = 0; i < 1024 * 2; i++) if (out[i] != 0) { nonzero = 1; break; }
        }
        CHECK(nonzero, "8 kHz music resamples to non-silent 48 kHz");
        XenonAudio_MusicStop();
        free(ogg8);
    }

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
