#include "game_gfx.h"

#include <stdint.h>
#include <string.h>

#include <xenos/xenos.h>
#include <console/console.h>

extern "C" {
#include <ppc/cache.h>
}

#include "gfx.h"

// We reuse the dxs360 Xenos scan-out path (xenon/source/gfx.cpp). The game
// renders into a 320x240 ARGB8888 surface; each frame we scale it (integer,
// nearest) into the Gfx shadow, then Gfx_Present() blits + cache-flushes.

static GfxContext *g_gfx = 0;
static int g_logged = 0;

int VvvvoGfx_Init(void) {
    g_gfx = Gfx_Create();
    return Gfx_Ready(g_gfx) ? 1 : 0;
}

int VvvvoGfx_Width(void)  { return g_gfx ? (int)Gfx_Width(g_gfx)  : 0; }
int VvvvoGfx_Height(void) { return g_gfx ? (int)Gfx_Height(g_gfx) : 0; }

void VvvvoGfx_Clear(uint32_t argb) {
    if (g_gfx) Gfx_Clear(g_gfx, argb);
}

void VvvvoGfx_Text(int x, int y, int scale, uint32_t argb, const char *text) {
    if (g_gfx) Gfx_DrawText(g_gfx, x, y, scale, argb, text);
}

/* One converted source row, reused for every duplicated scanline. VVVVVV's
 * screen is 320 px wide; the limit just keeps a stray huge surface safe. */
static uint32_t s_line[2048];

void VvvvoGfx_Present(const uint32_t *pixels, int w, int h, int pitch) {
    if (!g_gfx || !pixels || w <= 0 || h <= 0) return;
    if (w > (int)(sizeof(s_line)/sizeof(s_line[0]))) return;

    int dw = VvvvoGfx_Width();
    int dh = VvvvoGfx_Height();
    if (dw <= 0 || dh <= 0) return;

    /* Integer scale that fits, centred (letterboxed) on the scan-out. */
    int scale = dw / w;
    int sy = dh / h;
    if (sy < scale) scale = sy;
    if (scale < 1) scale = 1;

    int outw = w * scale;
    int outh = h * scale;
    int ox = (dw - outw) / 2;
    int oy = (dh - outh) / 2;

    uint32_t *dst = Gfx_Lock(g_gfx, 0);
    if (!dst) return;

    /* The game's screen surface is native ARGB8888 (masks R=0x00FF0000,
     * G=0x0000FF00, B=0x000000FF, A=0xFF000000), i.e. exactly the 0xAARRGGBB
     * layout the shadow framebuffer wants. Only force the alpha byte opaque.
     * No per-pixel channel shuffling and no divisions: convert each source row
     * once, then replicate it `scale` times horizontally and vertically. */
    const uint8_t *src = (const uint8_t *)pixels;

    for (int yy = 0; yy < h; ++yy) {
        const uint32_t *srow = (const uint32_t *)(src + (size_t)yy * (size_t)pitch);
        for (int xx = 0; xx < w; ++xx) {
            uint32_t c = srow[xx];
            s_line[xx] = ((c >> 24) == 0u) ? 0xFF000000u : (c | 0xFF000000u);
        }

        int dy0 = oy + yy * scale;
        for (int rep = 0; rep < scale; ++rep) {
            int dy = dy0 + rep;
            if (dy >= dh) break;
            uint32_t *drow = dst + (size_t)dy * (size_t)dw + ox;
            for (int xx = 0; xx < w; ++xx) {
                uint32_t v = s_line[xx];
                int k = scale;
                do { *drow++ = v; } while (--k);
            }
        }
    }

    Gfx_Present(g_gfx);

    if (!g_logged) {
        g_logged = 1;
    }
}
