#include "Screen.h"

#include "FileSystemUtils.h"
#include "GraphicsUtil.h"

#include "game_gfx.h"

#include <stdlib.h>

// libXenon (Xbox 360) port: the SDL2 window/renderer/texture path is replaced
// by the Xenos framebuffer. The game still owns a 320x240 ARGB8888 m_screen
// surface; FlipScreen() hands that surface to the bare-metal blitter.

Screen::Screen()
{
    m_window = NULL;
    m_renderer = NULL;
    m_screenTexture = NULL;
    isWindowed = false;
    stretchMode = 0;
    isFiltered = false;
    filterSubrect.x = 1;
    filterSubrect.y = 1;
    filterSubrect.w = 318;
    filterSubrect.h = 238;

	// XXXX_XXXX_ARGB8888 surface (same masks the rest of the game uses).
	m_screen = SDL_CreateRGBSurface(
		0,
		320,
		240,
		32,
		0x00FF0000,
		0x0000FF00,
		0x000000FF,
		0xFF000000
	);

    glScreen = true;
}

void Screen::ResizeScreen(int x , int y)
{
	(void)x; (void)y;
}

void Screen::GetWindowSize(int* x, int* y)
{
	if (x) *x = VvvvoGfx_Width();
	if (y) *y = VvvvoGfx_Height();
}

void Screen::UpdateScreen(SDL_Surface* buffer, SDL_Rect* rect )
{
    if((buffer == NULL) && (m_screen == NULL) )
    {
        return;
    }

    FillRect(m_screen, 0x000);
    BlitSurfaceStandard(buffer,NULL,m_screen,rect);

}

const SDL_PixelFormat* Screen::GetFormat()
{
    return m_screen->format;
}

void Screen::FlipScreen()
{
	VvvvoGfx_Present((const Uint32*)m_screen->pixels, m_screen->w, m_screen->h, m_screen->pitch);
	SDL_FillRect(m_screen, NULL, 0x00000000);
}

void Screen::toggleFullScreen()
{
	isWindowed = !isWindowed;
}

void Screen::toggleStretchMode()
{
	stretchMode = (stretchMode + 1) % 3;
}

void Screen::toggleLinearFilter()
{
	isFiltered = !isFiltered;
}

void Screen::ClearScreen( int colour )
{
    //FillRect(m_screen, colour) ;
}
