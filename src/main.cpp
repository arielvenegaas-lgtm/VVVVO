#include <SDL.h>
#include "filesystem.h"
#include "game_gfx.h"
#include "xenon_audio.h"

#include "states/Preloader.h"

#include "SoundSystem.h"

#include "Utility.h"
#include "Game.h"
#include "Graphics.h"
#include "KeyPoll.h"
#include "titlerender.h"

#include "Map.h"

#include "Screen.h"

#include "Script.h"

#include "Logic.h"

#include "Input.h"
#include "Editor.h"

#include "FileSystemUtils.h"


#include <stdio.h>
#include <string.h>
#include <time.h>

ScriptClass script;
edentities edentity[3000];

EditorClass ed;


int main()
{
	// Bring up the Xenos framebuffer before anything can draw.
	if (!VvvvoGfx_Init())
	{
		for (;;) { } // no display: spin so the failure is obvious
	}
	// Real loading screen, drawn with the built-in font so it is visible before
	// any game asset exists. Progress advances only when a stage actually
	// completes.
	VvvvoGfx_LoadScreen(2, "Video ready");

	// Mount the FAT32 volume. This is fast now: Fat32_MountVolume reads the
	// FSInfo hint instead of scanning the whole FAT (which took minutes on
	// large USB sticks).
	int fs_ok = FILESYSTEM_init();
	VvvvoGfx_LoadScreen(8, fs_ok ? "Filesystem ready" : "Filesystem unavailable");

	// Sound effects are registered + decoded here, now that the FAT/USB
	// filesystem is initialized. Doing this from the CSound global constructor
	// (static init, before the FS existed) stalled boot. See SoundSystem.cpp.
	SoundSystem_Startup();
	VvvvoGfx_LoadScreen(14, "Audio ready");

	srand(time(NULL));


    SDL_Init(
        SDL_INIT_VIDEO |
        SDL_INIT_AUDIO |
        SDL_INIT_JOYSTICK |
        SDL_INIT_GAMECONTROLLER
    );

	SDL_JoystickOpen(0);
	SDL_GameControllerOpen(0);
	VvvvoGfx_LoadScreen(20, "Input ready");

	

    Screen gameScreen;

	/* This looked cool and I didn't want to just get rid of it */
	fs::log("\t\t\n");
	fs::log("\t\t\n");
	fs::log("\t\t       VVVVVV\n");
	fs::log("\t\t\n");
	fs::log("\t\t\n");
	fs::log("\t\t  8888888888888888  \n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t888888    8888    88\n");
	fs::log("\t\t888888    8888    88\n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t888888            88\n");
	fs::log("\t\t88888888        8888\n");
	fs::log("\t\t  8888888888888888  \n");
	fs::log("\t\t      88888888      \n");
	fs::log("\t\t  8888888888888888  \n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t88888888888888888888\n");
	fs::log("\t\t8888  88888888  8888\n");
	fs::log("\t\t8888  88888888  8888\n");
	fs::log("\t\t    888888888888    \n");
	fs::log("\t\t    8888    8888    \n");
	fs::log("\t\t  888888    888888  \n");
	fs::log("\t\t  888888    888888  \n");
	fs::log("\t\t  888888    888888  \n");
	fs::log("\t\t\n");
	fs::log("\t\t\n");

	// Loading the PNG assets happens in the Graphics constructor; LoadImage()
	// advances this screen once per decoded file (see GraphicsResources.cpp).
	VvvvoGfx_LoadScreen(24, "Loading graphics");

    Graphics graphics;
	CSound sfx;
    MusicClass music;
    Game game;
    game.infocus = true;
	VvvvoGfx_LoadScreen(60, "Building sprites");

    graphics.MakeTileArray();
    graphics.MakeSpriteArray();
    graphics.maketelearray();

    graphics.images.push_back(graphics.grphx.im_image0);
    graphics.images.push_back(graphics.grphx.im_image1);
    graphics.images.push_back(graphics.grphx.im_image2);
    graphics.images.push_back(graphics.grphx.im_image3);
    graphics.images.push_back(graphics.grphx.im_image4);
    graphics.images.push_back(graphics.grphx.im_image5);
    graphics.images.push_back(graphics.grphx.im_image6);

    graphics.images.push_back(graphics.grphx.im_image7);
    graphics.images.push_back(graphics.grphx.im_image8);
    graphics.images.push_back(graphics.grphx.im_image9);
    graphics.images.push_back(graphics.grphx.im_image10);
    graphics.images.push_back(graphics.grphx.im_image11);
    graphics.images.push_back(graphics.grphx.im_image12);

    const SDL_PixelFormat* fmt = gameScreen.GetFormat();
    VvvvoGfx_LoadScreen(78, "Allocating buffers");
    graphics.backBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 32, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    SDL_SetSurfaceBlendMode(graphics.backBuffer, SDL_BLENDMODE_NONE);
    graphics.Makebfont();

    graphics.foregroundBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    SDL_SetSurfaceBlendMode(graphics.foregroundBuffer, SDL_BLENDMODE_NONE);

    graphics.screenbuffer = &gameScreen;

    graphics.menubuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    SDL_SetSurfaceBlendMode(graphics.menubuffer, SDL_BLENDMODE_NONE);

    graphics.towerbuffer =  SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    SDL_SetSurfaceBlendMode(graphics.towerbuffer, SDL_BLENDMODE_NONE);

	graphics.tempBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    SDL_SetSurfaceBlendMode(graphics.tempBuffer, SDL_BLENDMODE_NONE);

    // Skip the original fake preloader: all real loading already happened
    // above with genuine progress, so go straight to the title screen instead
    // of animating a second, fake 0-100%.
    game.gamestate = TITLEMODE;

    game.menustart = false;
    game.mainmenu = 0;
    game.shouldQuit = false;

    KeyPoll key;
    key.isActive = true;

    /* Validate every required graphic before entering the game. A missing or
       corrupt PNG used to show up only as an unexplained black screen, because
       the bitmap font and menu/map sprites silently became blank surfaces. The
       list below must match release/required-assets.txt (guarded by
       tests/test_asset_manifest_host.py). */
    {
        struct { const char *name; SDL_Surface *s; } need[] = {
            {"tiles.png",            graphics.grphx.im_tiles},
            {"tiles2.png",           graphics.grphx.im_tiles2},
            {"tiles3.png",           graphics.grphx.im_tiles3},
            {"entcolours.png",       graphics.grphx.im_entcolours},
            {"sprites.png",          graphics.grphx.im_sprites},
            {"flipsprites.png",      graphics.grphx.im_flipsprites},
            {"font.png",             graphics.grphx.im_bfont},
            {"fontmask.png",         graphics.grphx.im_bfontmask},
            {"teleporter.png",       graphics.grphx.im_teleporter},
            {"levelcomplete.png",    graphics.grphx.im_image0},
            {"minimap.png",          graphics.grphx.im_image1},
            {"covered.png",          graphics.grphx.im_image2},
            {"elephant.png",         graphics.grphx.im_image3},
            {"gamecomplete.png",     graphics.grphx.im_image4},
            {"fliplevelcomplete.png",graphics.grphx.im_image5},
            {"flipgamecomplete.png", graphics.grphx.im_image6},
            {"site.png",             graphics.grphx.im_image7},
            {"site2.png",            graphics.grphx.im_image8},
            {"site3.png",            graphics.grphx.im_image9},
            {"ending.png",           graphics.grphx.im_image10},
            {"site4.png",            graphics.grphx.im_image11},
        };
        char names[220];
        names[0] = '\0';
        int nmiss = 0, nshown = 0;
        for (unsigned i = 0; i < sizeof(need) / sizeof(need[0]); ++i) {
            if (need[i].s) continue;
            nmiss++;
            if (nshown < 5) {
                if (nshown) strcat(names, " ");
                strncat(names, need[i].name,
                        sizeof(names) - strlen(names) - 1);
                nshown++;
            }
        }
        if (nmiss) {
            char detail[256];
            if (nmiss > nshown)
                snprintf(detail, sizeof(detail), "%s (+%d more)",
                         names, nmiss - nshown);
            else
                snprintf(detail, sizeof(detail), "%s", names);
            VvvvoGfx_ErrorScreen("Missing game data",
                                 "Extract the game data into data/ at the USB root.",
                                 detail);
            while (!key.quitProgram) { SDL_Delay(100); key.Poll(); }
            SDL_Quit();
            return 1;
        }
    }

    MapClass map;

    map.ypos = (700 - 29) * 8;
    map.bypos = map.ypos / 2;

    VvvvoGfx_LoadScreen(92, "Loading save data");
    game.loadstats(map, graphics);

    if (game.usingmmmmmm == 0) music.usingmmmmmm = false;
    if (game.usingmmmmmm == 1) music.usingmmmmmm = true;
    if (game.slowdown == 0) game.slowdown = 30;

	// VVVVVV's simulation is frame-based at 30 Hz. Keep the original frame
	// targets (34/41/55/83 ms = 30/24/18/12 FPS): the renderer is fast enough
	// now that artificially speeding the game up is neither needed nor right.
    switch (game.slowdown){
        case 30:
            game.gameframerate = 34;
            break;

        case 24:
            game.gameframerate = 41;
            break;

        case 18:
            game.gameframerate = 55;
            break;

        case 12:
            game.gameframerate = 83;
            break;
        
        default:
            game.gameframerate = 34;
            break;
    }

    EntityClass obj;
    obj.init();

    VvvvoGfx_LoadScreen(100, "Ready");

    volatile Uint32 time, timePrev = 0;
    game.infocus = true;
    key.isActive = true;

    while(!key.quitProgram)
    {
        time = SDL_GetTicks();

        //framerate limit to 30
        Uint32 timetaken = time - timePrev;

        if (game.gamestate == EDITORMODE)
		{
            if (timetaken < 24)
            {
                volatile Uint32 delay = 24 - timetaken;
                SDL_Delay( delay );
                time = SDL_GetTicks();
            }
            timePrev = time;
        }
        else
        {
            if (timetaken < game.gameframerate)
            {
                volatile Uint32 delay = game.gameframerate - timetaken;
                SDL_Delay( delay );
                time = SDL_GetTicks();
            }
            timePrev = time;
        }

        key.Poll();

        game.infocus = true;

        switch (game.gamestate)
        {
            case PRELOADER:
                states::Preloader::input(key, game);
                states::Preloader::render(graphics, game);
                break;

            case EDITORMODE:
                graphics.flipmode = false;
                editorinput(key, graphics, game, map, obj, music);
                editorrender(key, graphics, game, map, obj);
                editorlogic(key, graphics, game, obj, music, map);
                break;

            case TITLEMODE:
                titleinput(key, graphics, map, game, obj, music);
                titlerender(graphics, map, game, obj, music);
                titlelogic(graphics, game, obj, music, map);
                break;

            case GAMEMODE:
                if (map.towermode)
                {
                    gameinput(key, graphics, game, map, obj, music);
                    towerrender(graphics, game, map, obj);
                    towerlogic(graphics, game, obj, music, map);
                }
                else
                {
                    if (script.running)
                    {
                        script.run(key, graphics, game, map, obj, music);
                    }

                    gameinput(key, graphics, game, map, obj, music);
                    gamerender(graphics,map, game, obj);
                    gamelogic(graphics, game, obj, music, map);
                }
                break;

            case MAPMODE:
                mapinput(key, graphics, game, map, obj, music);
                maprender(graphics, game, map, obj);
                maplogic(graphics, game, obj, music, map);
                break;

            case TELEPORTERMODE:
                if(game.useteleporter)
                {
                    teleporterinput(key, graphics, game, map, obj, music);
                }
                else
                {
                    if (script.running)
                    {
                        script.run(key, graphics, game, map, obj, music);
                    }

                    gameinput(key, graphics, game, map, obj, music);
                }
                teleporterrender(graphics, game, map, obj);
                maplogic(graphics, game, obj, music, map);
                break;

            case GAMECOMPLETE:
                gamecompleteinput(key, graphics, game, map, obj, music);
                gamecompleterender(graphics, game, obj, map);
                gamecompletelogic(graphics, game, obj, music, map);
                break;

            case GAMECOMPLETE2:
                gamecompleteinput2(key, graphics, game, map, obj, music);
                gamecompleterender2(graphics, game, obj);
                gamecompletelogic2(graphics, game, obj, music, map);
                break;

            case CLICKTOSTART:
                vvvvvv::Utility::updateGlow();
                break;

            default:
                break;
        }

        if (game.savemystats)
        {
            game.savemystats = false;
            game.savestats(map, graphics);
        }

        music.processmusic();
        XenonAudio_Pump();
        graphics.processfade();
        game.gameclock();
        gameScreen.FlipScreen();
    }

    SDL_Quit();

    return 0;
}
