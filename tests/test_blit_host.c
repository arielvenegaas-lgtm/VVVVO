/* test_blit_host.c: host tests for the colour/blend math and the big-endian
 * word access used by the Xbox 360 SDL shim.
 *
 * HOST test (Linux). It cannot prove the GPU path works, but it does pin down
 * the two things that most often go wrong on a big-endian console:
 *
 *   1. The pure blend/mask math (sdlblit.h).
 *   2. Byte order: the shim reads a surface pixel with `Uint32 *row` and
 *      writes `0xAARRGGBB`-style into the shadow. On the big-endian Xenon the
 *      byte at the lowest address is the most significant byte. This test
 *      reproduces that by storing bytes in an explicitly big-endian buffer and
 *      decoding with a big-endian integer read, then checks the exact values
 *      the game expects (fonts = solid colour, images = RGBA in memory).
 *
 * Build/run: tests/run_host_tests.sh
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../xenon/source/sdlblit.h"

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } \
    else         { printf("  ok  : %s\n", msg); } \
} while (0)

/* --- big-endian word access, as on PPC --- */
static uint32_t be_read32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}
static void be_write32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

int main(void) {
    printf("SDL shim colour/blend host tests\n");

    /* --- div255 correctness across the full range --- */
    {
        int ok = 1;
        for (uint32_t x = 0; x <= 255 * 255; x++) {
            uint32_t got = sdl_div255(x);
            /* Must be exact at multiples of 255 and never be off by more than
             * one from the true quotient (floor for non-multiples). */
            if (x % 255 == 0) {
                if (got != x / 255) { ok = 0; break; }
            } else {
                uint32_t fl = x / 255;
                if (got != fl && got != fl + 1) { ok = 0; break; }
            }
        }
        CHECK(ok, "div255 exact at k*255, within 1 elsewhere, over 0..65025");
        CHECK(sdl_div255(0) == 0, "div255(0)==0");
        CHECK(sdl_div255(255) == 1, "div255(255)==1");
        CHECK(sdl_div255(255 * 255) == 255, "div255(65025)==255");
    }

    /* --- mask extraction: big-endian RGBA image word --- */
    {
        /* lodepng writes R,G,B,A bytes; on BE the word reads R<<24|G<<16|B<<8|A */
        uint8_t bytes[4] = { 0x80, 0x40, 0x20, 0x10 }; /* R,G,B,A */
        uint32_t w = be_read32(bytes);
        uint32_t Rmask = 0xff000000, Gmask = 0x00ff0000,
                 Bmask = 0x0000ff00, Amask = 0x000000ff;
        CHECK(sdl_get(w, Rmask, 24) == 0x80, "BE image R component");
        CHECK(sdl_get(w, Gmask, 16) == 0x40, "BE image G component");
        CHECK(sdl_get(w, Bmask, 8)  == 0x20, "BE image B component");
        CHECK(sdl_get(w, Amask, 0)  == 0x10, "BE image A component");
    }

    /* --- ARGB8888 screen surface round-trips 0x00RRGGBB --- */
    {
        uint32_t w = 0x00FF8000u; /* R=FF G=80 B=00 */
        uint32_t Rmask = 0x00FF0000, Gmask = 0x0000FF00, Bmask = 0x000000FF;
        CHECK(sdl_get(w, Rmask, 16) == 0xFF, "screen R");
        CHECK(sdl_get(w, Gmask, 8)  == 0x80, "screen G");
        CHECK(sdl_get(w, Bmask, 0)  == 0x00, "screen B");
        /* present path: force opaque, layout already correct */
        CHECK(sdl_opaque_rgb(w) == 0xFFFF8000u, "present makes opaque, keeps RGB");
    }

    /* --- blending: 50% red over blue = purple-ish, opaque --- */
    {
        uint32_t src_r = 255, src_g = 0, src_b = 0;
        uint32_t dst_r = 0,  dst_g = 0, dst_b = 255;
        uint32_t a = 128;
        uint32_t r = sdl_blend_channel(src_r, dst_r, a);
        uint32_t g = sdl_blend_channel(src_g, dst_g, a);
        uint32_t b = sdl_blend_channel(src_b, dst_b, a);
        CHECK(r > 120 && r < 135, "50% blend red channel ~128");
        CHECK(g == 0, "50% blend green channel 0");
        CHECK(b > 120 && b < 135, "50% blend blue channel ~128");
    }

    /* --- fully opaque blend is a pass-through --- */
    {
        /* blend uses floor division, so 200*255/255 = 200 exactly */
        CHECK(sdl_blend_channel(200, 10, 255) == 200, "opaque passes src");
        CHECK(sdl_blend_channel(200, 10, 0) == 10, "transparent passes dst");
    }

    /* --- endianness guard: prove a native LE read would be wrong --- */
    {
        uint8_t bytes[4] = { 0x80, 0x40, 0x20, 0x10 }; /* R,G,B,A as written */
        uint32_t be = be_read32(bytes);
        uint32_t le = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
                      ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
        CHECK(be != le, "BE and LE word reads differ (why SDL_BIG_ENDIAN matters)");
        CHECK(sdl_get(be, 0xff000000, 24) == 0x80, "BE gives correct R");
        CHECK(sdl_get(le, 0xff000000, 24) == 0x10, "LE would give wrong R");
    }

    /* --- present round-trip: write opaque ARGB, read back --- */
    {
        uint8_t buf[4];
        uint32_t pixel = 0xFF3366CCu; /* A=FF R=33 G=66 B=CC */
        be_write32(buf, pixel);
        CHECK(buf[0] == 0xFF && buf[1] == 0x33 && buf[2] == 0x66 && buf[3] == 0xCC,
              "BE write lands A,R,G,B in memory order");
        CHECK(be_read32(buf) == pixel, "BE write/read round-trip");
    }

    printf("\n%s (%d failure%s)\n",
           g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
