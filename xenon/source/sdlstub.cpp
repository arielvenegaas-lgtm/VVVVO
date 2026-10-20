#include "sdlshim.h"
#include "sdlblit.h"
#include "gfx.h"
#include "input.h"
#include <sys/time.h>
#include <ppc/timebase.h>
#include <xenon_soc/xenon_power.h>

extern "C" {
#include <ppc/cache.h>
}

static Uint32 ticks_base_mftb = 0;
static Uint32 sdl_init_flags = 0;
static int sdl_textinput = 0;
static SDL_Joystick dummy_joy; 
static SDL_GameController dummy_ctrl;

int SDL_Init(Uint32 flags) {
  sdl_init_flags = flags;
  xenon_make_it_faster(XENON_SPEED_FULL);
  ticks_base_mftb = 0; // use mftb relative
  Input_Init();
  return 0;
}

void SDL_Quit(void) {}

int SDL_CreateWindowAndRenderer(int width, int height, Uint32 flags, SDL_Window **window, SDL_Renderer **renderer) {
  (void)width; (void)height; (void)flags;
  static int wdummy, rdummy;
  *window = (SDL_Window*)&wdummy;
  *renderer = (SDL_Renderer*)&rdummy;
  return 0;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
  (void)flags; (void)depth;
  SDL_Surface *s = (SDL_Surface*)malloc(sizeof(SDL_Surface));
  SDL_PixelFormat *f = (SDL_PixelFormat*)malloc(sizeof(SDL_PixelFormat));
  memset(f,0,sizeof(*f));
  f->format = SDL_PIXELFORMAT_ARGB8888;
  f->BitsPerPixel = 32; f->BytesPerPixel = 4;
  f->Rmask=Rmask; f->Gmask=Gmask; f->Bmask=Bmask; f->Amask=Amask;
  // Derive shifts from the masks (the only source of truth).
  { Uint32 m; int sh;
    m=Rmask; sh=0; while(m && !(m&1)){m>>=1;sh++;} f->Rshift=sh;
    m=Gmask; sh=0; while(m && !(m&1)){m>>=1;sh++;} f->Gshift=sh;
    m=Bmask; sh=0; while(m && !(m&1)){m>>=1;sh++;} f->Bshift=sh;
    m=Amask; sh=0; while(m && !(m&1)){m>>=1;sh++;} f->Ashift=sh;
  }
  s->format = f;
  s->w=width; s->h=height;
  s->pitch = width*4;
  s->pixels = malloc((size_t)width*height*4);
  memset(s->pixels,0,(size_t)width*height*4);
  s->flags = SDL_SWSURFACE;
  s->locked=0; s->refcount=1;
  s->userdata = 0; s->lock_data = 0; s->map = 0;
  s->blendMode = Amask ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
  s->alphaMod = 255;
  return s;
}

void SDL_FreeSurface(SDL_Surface *s) {
  if(!s) return;
  free(s->pixels);
  free(s->format);
  free(s);
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h) {
  (void)renderer; (void)format; (void)access; (void)w; (void)h;
  return (SDL_Texture*)1;
}
void SDL_DestroyTexture(SDL_Texture *t) {(void)t;}
int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
  (void)texture; (void)rect; (void)pixels; (void)pitch; return 0;
}
int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect) {
  (void)renderer; (void)texture; (void)srcrect; (void)dstrect; return 0;
}
void SDL_RenderPresent(SDL_Renderer *renderer) {(void)renderer;}
void SDL_RenderClear(SDL_Renderer *renderer) {(void)renderer;}
void SDL_SetHint(const char *name, const char *value) {(void)name;(void)value;}
void SDL_SetHintWithPriority(const char *name, const char *value, int priority) {(void)name;(void)value;(void)priority;}
void SDL_GetWindowSize(SDL_Window *window, int *w,int *h) {(void)window; if(w)*w=1280; if(h)*h=720;}
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {(void)window;(void)flags; return 0;}
void SDL_RenderSetLogicalSize(SDL_Renderer *r,int w,int h){(void)r;(void)w;(void)h;}
void SDL_RenderSetIntegerScale(SDL_Renderer *r, SDL_bool b){(void)r;(void)b;}
void SDL_ShowWindow(SDL_Window *w){(void)w;}
int SDL_SetSurfaceBlendMode(SDL_Surface *s, SDL_BlendMode m){if(s) s->blendMode=(Uint8)m; return 0;}
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
  if(!dst||!dst->pixels) return -1;
  int x0=0,y0=0,x1=dst->w,y1=dst->h;
  if(rect){ x0=rect->x; y0=rect->y; x1=rect->x+rect->w; y1=rect->y+rect->h; }
  if(x0<0)x0=0; if(y0<0)y0=0; if(x1>dst->w)x1=dst->w; if(y1>dst->h)y1=dst->h;
  for(int y=y0;y<y1;y++){
    Uint32 *row=(Uint32*)((char*)dst->pixels + y*dst->pitch);
    for(int x=x0;x<x1;x++) row[x]=color;
  }
  return 0;
}
/* x / 255 for x <= 65535 without a divide (exact at multiples of 255). */
static inline Uint32 Div255(Uint32 x) { return sdl_div255(x); }

/* SDL2 uses the *source* surface's blend mode and converts pixel formats
 * during the blit. The game relies on both: images are RGBA8888 (R in the high
 * byte) while the screen/back/fore/tower buffers are ARGB8888, so a raw word
 * copy would swap channels and paint transparent sprite pixels black. */
int SDL_BlitSurface(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect) {
  if(!src||!dst||!src->pixels||!dst->pixels||!src->format||!dst->format) return -1;
  int sx=0,sy=0,sw=src->w,sh=src->h;
  if(srcrect){ sx=srcrect->x; sy=srcrect->y; sw=srcrect->w; sh=srcrect->h; }
  int dx=0,dy=0;
  if(dstrect){ dx=dstrect->x; dy=dstrect->y; }
  if(sx<0){ sw+=sx; sx=0;} if(sy<0){ sh+=sy; sy=0;}
  if(dx<0){ sw+=dx; sx-=dx; dx=0;} if(dy<0){ sh+=dy; sy-=dy; dy=0;}
  if(sx+sw>src->w) sw=src->w-sx; if(sy+sh>src->h) sh=src->h-sy;
  if(dx+sw>dst->w) sw=dst->w-dx; if(dy+sh>dst->h) sh=dst->h-dy;
  if(sw<=0||sh<=0) return 0;

  const SDL_PixelFormat *sfp=src->format, *dfp=dst->format;
  SdlFmt sf = { sfp->Rmask, sfp->Gmask, sfp->Bmask, sfp->Amask,
                (uint8_t)sfp->Rshift, (uint8_t)sfp->Gshift, (uint8_t)sfp->Bshift, (uint8_t)sfp->Ashift };
  SdlFmt df = { dfp->Rmask, dfp->Gmask, dfp->Bmask, dfp->Amask,
                (uint8_t)dfp->Rshift, (uint8_t)dfp->Gshift, (uint8_t)dfp->Bshift, (uint8_t)dfp->Ashift };
  int same = (sf.Rshift==df.Rshift && sf.Gshift==df.Gshift &&
              sf.Bshift==df.Bshift && sf.Ashift==df.Ashift &&
              sf.Amask==df.Amask);
  int blend = (src->blendMode==SDL_BLENDMODE_BLEND) && sf.Amask && src->alphaMod;

  for(int y=0;y<sh;y++){
    Uint32 *srow=(Uint32*)((char*)src->pixels + (size_t)(sy+y)*src->pitch) + sx;
    Uint32 *drow=(Uint32*)((char*)dst->pixels + (size_t)(dy+y)*dst->pitch) + dx;

    if(same && !blend){                 /* hot path: raw copy */
      memcpy(drow, srow, (size_t)sw*4u);
      continue;
    }

    for(int x=0;x<sw;x++){
      Uint32 c=srow[x];
      Uint32 out;
      Uint32 A = sf.Amask ? ((c & sf.Amask) >> sf.Ashift) : 255u;

      if(blend){
        A = Div255(A * src->alphaMod);
        if(A==0) continue;
        if(A==255){ sdl_blit_pixel(c,&sf,&df,0,255,&out); drow[x]=out; continue; }
        drow[x] = sdl_blit_over(c, drow[x], &sf, &df, A);
      } else {
        sdl_blit_pixel(c,&sf,&df,0,255,&out);
        drow[x]=out;
      }
    }
  }
  return 0;
}
void SDL_LockSurface(SDL_Surface *s){if(s)s->locked++;}
void SDL_UnlockSurface(SDL_Surface *s){if(s&&s->locked)s->locked--;}
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r,Uint8 g,Uint8 b){
  if(!fmt) return 0xFF000000u | ((Uint32)r<<16) | ((Uint32)g<<8) | (Uint32)b;
  return ((Uint32)r << fmt->Rshift) | ((Uint32)g << fmt->Gshift) | ((Uint32)b << fmt->Bshift) | fmt->Amask;
}
Uint32 SDL_MapRGBA(const SDL_PixelFormat *fmt, Uint8 r,Uint8 g,Uint8 b,Uint8 a){
  if(!fmt) return ((Uint32)a<<24) | ((Uint32)r<<16) | ((Uint32)g<<8) | (Uint32)b;
  return ((Uint32)r << fmt->Rshift) | ((Uint32)g << fmt->Gshift) | ((Uint32)b << fmt->Bshift) | ((Uint32)a << fmt->Ashift);
}
SDL_RWops *SDL_RWFromMem(void *mem, int size){(void)mem;(void)size; return 0;}
int SDL_RWread(SDL_RWops *rw,void *ptr,int size,int n){(void)rw;(void)ptr;(void)size;(void)n; return 0;}
int SDL_RWclose(SDL_RWops *rw){(void)rw; return 0;}
/* KeyPoll.cpp checks the *joystick-range* event numbers used by the Xbox
 * SDL2x build: 0x600 axis, 0x603/0x604 button down/up. We synthesise those
 * from libXenon pad reports, plus one CONTROLLERDEVICEADDED at startup. */
static int s_prev_buttons[16];
static int s_have_pad = 0;
static int s_axis_lx = 0, s_axis_ly = 0;
static int s_sent_added = 0;

static int emit_button(SDL_Event *e, int button, int down) {
  memset(e, 0, sizeof(*e));
  e->type = down ? 0x603 : 0x604;   /* JOYBUTTONDOWN / JOYBUTTONUP */
  e->cbutton.button = (Uint8)button;
  e->cbutton.state = down ? 1 : 0;
  e->cbutton.which = 0;
  return 1;
}

int SDL_PollEvent(SDL_Event *event){
  if (!event) return 0;
  Input_Poll();
  const PadState *p = Input_GetPad(0);

  if (!s_sent_added) {
    s_sent_added = 1;
    memset(event, 0, sizeof(*event));
    event->type = SDL_CONTROLLERDEVICEADDED;
    event->cdevice.which = 0;
    return 1;
  }

  if (!p || !p->connected) {
    if (p && p->disconnected && s_have_pad) {
      s_have_pad = 0;
      memset(s_prev_buttons, 0, sizeof(s_prev_buttons));
      s_axis_lx = s_axis_ly = 0;
      memset(event, 0, sizeof(*event));
      event->type = SDL_CONTROLLERDEVICEREMOVED;
      event->cdevice.which = 0;
      return 1;
    }
    return 0;
  }
  s_have_pad = 1;

  /* Physical button -> SDL_GameControllerButton mapping. */
  static const struct { int pad; int sdl; } map[] = {
    { PAD_A, 0 }, { PAD_B, 1 }, { PAD_X, 2 }, { PAD_Y, 3 },
    { PAD_BACK, 4 }, { PAD_START, 6 },
    { PAD_GUIDE, 5 }, { PAD_LS, 7 }, { PAD_RS, 8 },
    { PAD_LB, 9 }, { PAD_RB, 10 },
    { PAD_UP, 11 }, { PAD_DOWN, 12 }, { PAD_LEFT, 13 }, { PAD_RIGHT, 14 },
  };

  for (unsigned i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
    int down = (p->buttons & map[i].pad) ? 1 : 0;
    int b = map[i].sdl;
    if (down != s_prev_buttons[b]) {
      s_prev_buttons[b] = down;
      return emit_button(event, b, down);
    }
  }

  /* Left stick -> axis events (only when the sign/magnitude changes).
   * libXenon reports stick-up as positive Y, but SDL's axis convention (and the
   * game's menu navigation) treats positive as down, so invert Y. */
  int lx = (int)(p->lx * 32767.0f);
  int ly = (int)(-p->ly * 32767.0f);
  if (lx != s_axis_lx) {
    s_axis_lx = lx;
    memset(event, 0, sizeof(*event));
    event->type = 0x600; /* JOYAXISMOTION */
    event->caxis.axis = 0; /* SDL_CONTROLLER_AXIS_LEFTX */
    event->caxis.value = (Sint16)lx;
    event->caxis.which = 0;
    return 1;
  }
  if (ly != s_axis_ly) {
    s_axis_ly = ly;
    memset(event, 0, sizeof(*event));
    event->type = 0x600;
    event->caxis.axis = 1; /* SDL_CONTROLLER_AXIS_LEFTY */
    event->caxis.value = (Sint16)ly;
    event->caxis.which = 0;
    return 1;
  }

  return 0;
}
void SDL_StartTextInput(void){sdl_textinput=1;}
void SDL_StopTextInput(void){sdl_textinput=0;}
SDL_Joystick *SDL_JoystickOpen(int index){(void)index; return &dummy_joy;}
SDL_GameController *SDL_GameControllerOpen(int index){(void)index; return &dummy_ctrl;}
void SDL_JoystickClose(SDL_Joystick *j){(void)j;}
void SDL_GameControllerClose(SDL_GameController *c){(void)c;}
Uint32 SDL_GetTicks(void){
  // use timebase
  return (Uint32)((double)mftb() / (double)PPC_TIMEBASE_FREQ * 1000.0);
}
void SDL_Delay(Uint32 ms){
  // crude busy wait
  Uint32 start = SDL_GetTicks();
  while(SDL_GetTicks() - start < ms){}
}
void SDL_assert(int cond){(void)cond;}
const char *SDL_GetError(void){return "stub";}
int SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r,Uint8 *g,Uint8 *b){
  (void)fmt; if(r)*r=(pixel>>16)&0xFF; if(g)*g=(pixel>>8)&0xFF; if(b)*b=pixel&0xFF; return 0;
}
int SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *fmt, Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a){
  (void)fmt; if(a)*a=(pixel>>24)&0xFF; if(r)*r=(pixel>>16)&0xFF; if(g)*g=(pixel>>8)&0xFF; if(b)*b=pixel&0xFF; return 0;
}
int SDL_UpperBlit(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect){
  return SDL_BlitSurface(src,srcrect,dst,dstrect);
}
int SDL_LowerBlit(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect){
  return SDL_BlitSurface(src,srcrect,dst,dstrect);
}
int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key){(void)surface;(void)flag;(void)key; return 0;}
int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r,Uint8 g,Uint8 b){(void)surface;(void)r;(void)g;(void)b; return 0;}
int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 a){if(surface) surface->alphaMod=a; return 0;}
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags){(void)fmt;(void)flags; return src;}
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface){return surface;}
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller){(void)gamecontroller; return &dummy_joy;}
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick){(void)joystick; return 0;}
int SDL_GameControllerGetAttached(SDL_GameController *gamecontroller){(void)gamecontroller; return 1;}
