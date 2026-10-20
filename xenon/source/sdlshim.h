#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;

#define SDL_TRUE 1
#define SDL_FALSE 0

typedef int SDL_bool;

typedef struct SDL_Rect {
  Sint16 x, y;
  Uint16 w, h;
} SDL_Rect;

typedef struct SDL_Color {
  Uint8 r,g,b,a;
} SDL_Color;

typedef struct SDL_Palette {
  int ncolors;
  SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
  Uint32 format;
  SDL_Palette *palette;
  Uint8 BitsPerPixel, BytesPerPixel;
  Uint8 padding[2];
  Uint32 Rmask,Gmask,Bmask,Amask;
  Uint8 Rloss,Gloss,Bloss,Aloss;
  Uint8 Rshift,Gshift,Bshift,Ashift;
  int refcount;
  struct SDL_PixelFormat *next;
} SDL_PixelFormat;

typedef struct SDL_Surface {
  Uint32 flags;
  SDL_PixelFormat *format;
  int w,h;
  int pitch;
  void *pixels;
  void *userdata;
  int locked;
  void *lock_data;
  SDL_Rect clip_rect;
  struct SDL_Surface *map;
  int refcount;
  Uint8 blendMode;
  Uint8 alphaMod;
} SDL_Surface;

typedef enum {
  SDL_TEXTUREACCESS_STATIC,
  SDL_TEXTUREACCESS_STREAMING,
  SDL_TEXTUREACCESS_TARGET
} SDL_TextureAccess;

typedef enum {
  SDL_BLENDMODE_NONE,
  SDL_BLENDMODE_BLEND,
  SDL_BLENDMODE_ADD,
  SDL_BLENDMODE_MOD
} SDL_BlendMode;

#define SDL_PIXELFORMAT_ARGB8888 0x16362004
#define SDL_SWSURFACE 0x00000000

typedef void SDL_Window;
typedef void SDL_Renderer;
typedef void SDL_Texture;
typedef struct _SDL_Joystick { int dummy; } SDL_Joystick;
typedef struct _SDL_GameController { int dummy; } SDL_GameController;
typedef Sint32 SDL_JoystickID;
typedef Sint32 SDL_Keycode;

typedef enum {
  SDL_INIT_VIDEO = 0x00000020,
  SDL_INIT_AUDIO = 0x00000010,
  SDL_INIT_JOYSTICK = 0x00000200,
  SDL_INIT_GAMECONTROLLER = 0x00002000,
  SDL_INIT_TIMER = 0x00000001,
  SDL_INIT_EVENTS = 0x00000004
} SDL_InitFlags;

typedef enum {
  SDL_WINDOW_FULLSCREEN = 0x00000001,
  SDL_WINDOW_FULLSCREEN_DESKTOP = 0x00001000,
  SDL_WINDOW_SHOWN = 0x00000004
} SDL_WindowFlags;

typedef enum {
  SDL_RENDERER_ACCELERATED = 0x00000002,
  SDL_RENDERER_PRESENTVSYNC = 0x00000004,
  SDL_RENDERER_TARGETTEXTURE = 0x00000008
} SDL_RendererFlags;

#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"
#define SDL_HINT_OVERRIDE 0x00000001
#define SDL_BYTEORDER SDL_LIL_ENDIAN
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321

typedef enum {
  SDL_KEYDOWN = 0x300,
  SDL_KEYUP = 0x301,
  SDL_TEXTEDITING = 0x302,
  SDL_TEXTINPUT = 0x303,
  SDL_MOUSEMOTION = 0x400,
  SDL_MOUSEBUTTONDOWN = 0x401,
  SDL_MOUSEBUTTONUP = 0x402,
  SDL_MOUSEWHEEL = 0x403,
  SDL_JOYAXISMOTION = 0x600,
  SDL_JOYBALLMOTION = 0x601,
  SDL_JOYHATMOTION = 0x602,
  SDL_JOYBUTTONDOWN = 0x603,
  SDL_JOYBUTTONUP = 0x604,
  SDL_JOYDEVICEADDED = 0x605,
  SDL_JOYDEVICEREMOVED = 0x606,
  SDL_CONTROLLERAXISMOTION = 0x650,
  SDL_CONTROLLERBUTTONDOWN = 0x651,
  SDL_CONTROLLERBUTTONUP = 0x652,
  SDL_CONTROLLERDEVICEADDED = 0x653,
  SDL_CONTROLLERDEVICEREMOVED = 0x654,
  SDL_QUIT = 0x100
} SDL_EventType;

typedef struct SDL_Keysym {
  SDL_Keycode sym;
  Uint16 mod;
  Uint32 unused;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
  Uint32 type;
  Uint32 timestamp;
  Uint32 windowID;
  Uint8 state;
  Uint8 repeat;
  Uint8 padding2;
  Uint8 padding3;
  SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
  Uint32 type; Uint32 timestamp; Uint32 windowID;
  Uint32 which; Uint8 state;
  Uint32 x,y,xrel,yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
  Uint32 type,timestamp,windowID,which;
  Uint8 button,state,clicks,padding1;
  Sint32 x,y;
} SDL_MouseButtonEvent;

typedef struct SDL_ControllerAxisEvent {
  Uint32 type,timestamp,which;
  Uint8 axis,padding1,padding2,padding3;
  Sint16 value;
  Uint16 padding4;
} SDL_ControllerAxisEvent;

typedef struct SDL_ControllerButtonEvent {
  Uint32 type,timestamp,which;
  Uint8 button,state,padding1,padding2;
} SDL_ControllerButtonEvent;

typedef struct SDL_ControllerDeviceEvent {
  Uint32 type,timestamp,which;
} SDL_ControllerDeviceEvent;

typedef union SDL_Event {
  Uint32 type;
  SDL_KeyboardEvent key;
  SDL_MouseMotionEvent motion;
  SDL_MouseButtonEvent button;
  SDL_ControllerAxisEvent caxis;
  SDL_ControllerButtonEvent cbutton;
  SDL_ControllerDeviceEvent cdevice;
  Uint8 padding[56];
} SDL_Event;

typedef enum {
  SDLK_UNKNOWN = 0,
  SDLK_BACKSPACE = '\b',
  SDLK_RETURN = '\r',
  SDLK_ESCAPE = 27,
  SDLK_SPACE = ' ',
  SDLK_0='0',SDLK_1='1',SDLK_2='2',SDLK_3='3',SDLK_4='4',SDLK_5='5',SDLK_6='6',SDLK_7='7',SDLK_8='8',SDLK_9='9',
  SDLK_a='a',SDLK_b='b',SDLK_c='c',SDLK_d='d',SDLK_e='e',SDLK_f='f',SDLK_g='g',SDLK_h='h',SDLK_i='i',SDLK_j='j',SDLK_k='k',SDLK_l='l',SDLK_m='m',SDLK_n='n',SDLK_o='o',SDLK_p='p',SDLK_q='q',SDLK_r='r',SDLK_s='s',SDLK_t='t',SDLK_u='u',SDLK_v='v',SDLK_w='w',SDLK_x='x',SDLK_y='y',SDLK_z='z',
  SDLK_F1=282,SDLK_F2=283,SDLK_F3=284,SDLK_F4=285,SDLK_F5=286,SDLK_F6=287,SDLK_F7=288,SDLK_F8=289,SDLK_F9=290,SDLK_F10=291,
  SDLK_UP=1073741906,SDLK_DOWN=1073741905,SDLK_LEFT=1073741904,SDLK_RIGHT=1073741903,
  SDLK_KP_ENTER=1073741912,
  SDLK_LSHIFT=1073742049,SDLK_RSHIFT=1073742053,
  SDLK_LCTRL=1073742048,SDLK_RCTRL=1073742052,
  SDLK_COMMA=',',SDLK_PERIOD='.',SDLK_MINUS='-',SDLK_PLUS='+',SDLK_EQUALS='='
} SDLKey;

typedef enum {
  SDL_CONTROLLER_BUTTON_INVALID = -1,
  SDL_CONTROLLER_BUTTON_A=0,SDL_CONTROLLER_BUTTON_B=1,SDL_CONTROLLER_BUTTON_X=2,SDL_CONTROLLER_BUTTON_Y=3,
  SDL_CONTROLLER_BUTTON_BACK=4,SDL_CONTROLLER_BUTTON_GUIDE=5,SDL_CONTROLLER_BUTTON_START=6,
  SDL_CONTROLLER_BUTTON_LEFTSTICK=7,SDL_CONTROLLER_BUTTON_RIGHTSTICK=8,
  SDL_CONTROLLER_BUTTON_LEFTSHOULDER=9,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER=10,
  SDL_CONTROLLER_BUTTON_DPAD_UP=11,SDL_CONTROLLER_BUTTON_DPAD_DOWN=12,
  SDL_CONTROLLER_BUTTON_DPAD_LEFT=13,SDL_CONTROLLER_BUTTON_DPAD_RIGHT=14
} SDL_GameControllerButton;

typedef enum {
  SDL_CONTROLLER_AXIS_INVALID = -1,
  SDL_CONTROLLER_AXIS_LEFTX = 0,
  SDL_CONTROLLER_AXIS_LEFTY = 1,
  SDL_CONTROLLER_AXIS_RIGHTX = 2,
  SDL_CONTROLLER_AXIS_RIGHTY = 3,
  SDL_CONTROLLER_AXIS_TRIGGERLEFT = 4,
  SDL_CONTROLLER_AXIS_TRIGGERRIGHT = 5
} SDL_GameControllerAxis;

typedef struct SDL_RWops {} SDL_RWops;
SDL_RWops *SDL_RWFromMem(void *mem, int size);
int SDL_RWread(SDL_RWops *rw,void *ptr,int size,int n);
int SDL_RWclose(SDL_RWops *rw);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *fmt, Uint8 r,Uint8 g,Uint8 b,Uint8 a);
int SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r,Uint8 *g,Uint8 *b);
int SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a);
int SDL_UpperBlit(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect);
int SDL_LowerBlit(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect);
#define SDL_ConvertSurfaceFormat(s,f,fl) (SDL_Surface*)(s)
int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key);
int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r,Uint8 g,Uint8 b);
int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 a);
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags);
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface);
const char *SDL_GetError(void);
void SDL_assert(int cond);

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
int SDL_CreateWindowAndRenderer(int width, int height, Uint32 flags, SDL_Window **window, SDL_Renderer **renderer);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface *s);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h);
void SDL_DestroyTexture(SDL_Texture *t);
int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect);
void SDL_RenderPresent(SDL_Renderer *renderer);
void SDL_RenderClear(SDL_Renderer *renderer);
void SDL_SetHint(const char *name, const char *value);
void SDL_SetHintWithPriority(const char *name, const char *value, int priority);
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h);
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags);
void SDL_RenderSetLogicalSize(SDL_Renderer *r, int w, int h);
void SDL_RenderSetIntegerScale(SDL_Renderer *r, SDL_bool b);
void SDL_ShowWindow(SDL_Window *w);
int SDL_SetSurfaceBlendMode(SDL_Surface *s, SDL_BlendMode m);
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
void SDL_LockSurface(SDL_Surface *s);
void SDL_UnlockSurface(SDL_Surface *s);
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b);
int SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);
int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
int SDL_LowerBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key);
int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b);
int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 a);
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags);
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface);
int SDL_PollEvent(SDL_Event *event);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);
SDL_Joystick *SDL_JoystickOpen(int index);
SDL_GameController *SDL_GameControllerOpen(int index);
void SDL_JoystickClose(SDL_Joystick *j);
void SDL_GameControllerClose(SDL_GameController *c);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller);
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick);
int SDL_GameControllerGetAttached(SDL_GameController *gamecontroller);

#ifdef __cplusplus
}
#endif

