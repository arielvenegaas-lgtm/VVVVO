#pragma once
// libXenon (Xbox 360) silent audio stub.
//
// The Xbox build drove audio through DirectMusic8 (XDK, unavailable on the
// 360). For the first hardware milestone audio is a no-op: the classes keep
// their public surface so Music.cpp / the game compile and run unchanged.

#include <SDL.h>

class MusicTrack
{
public:
	MusicTrack(const char* fileName);
	MusicTrack(SDL_RWops *rw);
	bool m_isValid;
};

class SoundTrack
{
public:
	SoundTrack(const char* fileName);
};

class SoundSystem
{
public:
	SoundSystem();
	void playMusic(MusicTrack* music);
};

class CSound
{
public:
	CSound();
	void Create(int t, int p);
	void CreateMusic(int t, int p);
	void playsound();
	void stop();
	void Release();
};
