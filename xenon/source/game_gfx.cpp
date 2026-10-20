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

/* Startup diagnostics. Draws an accumulating "boot: 1 2 3 ..." line in the
 * top-left corner of the scan-out and presents it immediately, so boot progress
 * is visible on hardware even if the game never reaches its first frame. */
void BootCheckpoint(int n) {
    if (!g_gfx) return;

    const int scale  = 2;
    const int margin = 8;

    /* Accumulating "boot: <n> <n> ..." string, so a screenshot shows how far
     * boot got. Fixed storage: no allocation, no I/O. */
    static char buf[96];
    static int  len = 0;

    /* Do NOT clear the scan-out here: this is also called from inside the
     * preloader/game loop, and clearing would erase the frame the game just
     * rendered. The shadow is already black at boot. */
    if (len > (int)sizeof(buf) - 6) len = (int)sizeof(buf) - 6;

    if (len == 0) {
        const char *hdr = "boot:";
        while (*hdr) buf[len++] = *hdr++;
        buf[len++] = ' ';
    }

    char tmp[12];
    int t = 0;
    int v = n;
    if (v == 0) tmp[t++] = '0';
    while (v > 0 && t < (int)sizeof(tmp)) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = t - 1; i >= 0; --i) buf[len++] = tmp[i];
    buf[len++] = ' ';
    buf[len]   = '\0';

    Gfx_DrawText(g_gfx, margin, margin, scale, 0xFFFFFF00u, buf);
    Gfx_Present(g_gfx);
}

/* --- boot / loading screen ---------------------------------------------- */

static int TextWidth(const char *s, int scale) {
    int n = 0;
    for (; s && *s; ++s) n += (6 * scale);
    return n;
}

static void DrawCentered(const char *s, int y, int scale, uint32_t argb) {
    int dw = (int)Gfx_Width(g_gfx);
    int x = (dw - TextWidth(s, scale)) / 2;
    if (x < 0) x = 0;
    Gfx_DrawText(g_gfx, x, y, scale, argb, s);
}

static void Fill(uint32_t argb) { Gfx_Clear(g_gfx, argb); }

void VvvvoGfx_LoadScreen(int pct, const char *stage) {
    if (!g_gfx) return;
    int dw = (int)Gfx_Width(g_gfx);
    int dh = (int)Gfx_Height(g_gfx);

    Fill(0xFF141428u);

    DrawCentered("VVVVVV", dh / 4, 6, 0xFFFFFFFFu);
    DrawCentered("Xbox 360 - libXenon", dh / 4 + 9 * 6, 2, 0xFFB0B0C8u);

    /* Progress bar: outline + fill, centred. */
    int bw = dw / 2; if (bw > 640) bw = 640; if (bw < 200) bw = 200;
    int bh = 26;
    int bx = (dw - bw) / 2;
    int by = dh / 2 + 40;

    Gfx_FillRect(g_gfx, bx - 3, by - 3, bw + 6, bh + 6, 0xFFFFFFFFu);
    Gfx_FillRect(g_gfx, bx, by, bw, bh, 0xFF202040u);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int fillw = (int)((long long)bw * pct / 100);
    if (fillw > 0) Gfx_FillRect(g_gfx, bx, by, fillw, bh, 0xFF30C060u);

    char pcttxt[16];
    pcttxt[0] = (char)('0' + (pct / 100) % 10);
    pcttxt[1] = (char)('0' + (pct / 10) % 10);
    pcttxt[2] = '%';
    pcttxt[3] = '\0';
    /* Only show "100%" once actually complete. */
    if (pct < 100) {
        pcttxt[0] = (char)('0' + (pct / 10) % 10);
        pcttxt[1] = '%';
        pcttxt[2] = '\0';
    }

    if (stage) DrawCentered(stage, by + bh + 24, 3, 0xFFFFFFFFu);
    DrawCentered(pcttxt, by + bh + 24 + 3 * 9, 2, 0xFFB0B0C8u);

    Gfx_Present(g_gfx);
}

void VvvvoGfx_ErrorScreen(const char *title, const char *detail) {
    if (!g_gfx) return;
    int dh = (int)Gfx_Height(g_gfx);

    Fill(0xFF200000u);
    DrawCentered(title ? title : "ERROR", dh / 4, 4, 0xFFFF6060u);
    if (detail && *detail) DrawCentered(detail, dh / 4 + 4 * 9 + 24, 2, 0xFFFFFFFFu);
    DrawCentered("Check the USB drive and data/ folder.", dh - dh / 4, 2, 0xFFFFC040u);
    Gfx_Present(g_gfx);
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

    /* Integer scale that fits, centred (letterboxed) on the scan-out. If the
     * scan-out is smaller than the source, fall back to 1:1 and clip rather
     * than producing a negative origin (which would write before the row). */
    int scale = dw / w;
    int sy = dh / h;
    if (sy < scale) scale = sy;
    if (scale < 1) scale = 1;

    int outw = w * scale;
    int outh = h * scale;
    int ox = (dw - outw) / 2;
    int oy = (dh - outh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    uint32_t *dst = Gfx_Lock(g_gfx, 0);
    if (!dst) return;

    /* Clip the destination span to the framebuffer so no write can go out of
     * bounds regardless of source/scan-out sizes. */
    if (ox >= dw || oy >= dh) { Gfx_Present(g_gfx); return; }
    if (ox + outw > dw) outw = dw - ox;
    if (oy + outh > dh) outh = dh - oy;
    if (outw <= 0 || outh <= 0) { Gfx_Present(g_gfx); return; }

    /* The game's screen surface is native ARGB8888 (masks R=0x00FF0000,
     * G=0x0000FF00, B=0x000000FF, A=0xFF000000), i.e. exactly the 0xAARRGGBB
     * layout the shadow framebuffer wants. Only force the alpha byte opaque.
     * No per-pixel channel shuffling and no divisions: convert each source row
     * once, then replicate it `scale` times horizontally and vertically. */
    const uint8_t *src = (const uint8_t *)pixels;

    /* Number of source columns/rows that fit within the clipped destination. */
    int cols = outw / scale;
    if (cols > w) cols = w;
    int rows = outh / scale;
    if (rows > h) rows = h;

    for (int yy = 0; yy < rows; ++yy) {
        const uint32_t *srow = (const uint32_t *)(src + (size_t)yy * (size_t)pitch);
        for (int xx = 0; xx < cols; ++xx) {
            uint32_t c = srow[xx];
            s_line[xx] = ((c >> 24) == 0u) ? 0xFF000000u : (c | 0xFF000000u);
        }

        int dy0 = oy + yy * scale;
        for (int rep = 0; rep < scale; ++rep) {
            int dy = dy0 + rep;
            if (dy >= dh) break;
            uint32_t *drow = dst + (size_t)dy * (size_t)dw + ox;
            for (int xx = 0; xx < cols; ++xx) {
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
