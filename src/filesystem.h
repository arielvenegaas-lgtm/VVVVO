#pragma once
// libXenon (Xbox 360) port of the filesystem shim.
//
// The Xbox build used Win32 FindFirstFile/CreateDirectory/CreateFile against a
// hardcoded D:\ drive. libXenon has no Win32 and no filesystem driver, so we
// mount a FAT32 volume ourselves (see ../xenon/source/fat32.c) and expose the
// same fs:: surface on top of it.
//
// Paths given by the game ("D:\\data\\graphics\\font.png") are mapped to
// volume-relative POSIX paths ("/data/graphics/font.png") by MapPath().

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern "C" {
#include "fat32.h"
int xenon_mount_fat32(void);
}

// Directory enumeration: Fat32V_ListDir reports names relative to `dir`, so a
// no-arg callback returns the current full path as a POSIX string.
struct FsEnumCtx {
	std::string dir;          // mapped, no trailing slash
	std::vector<std::string> out; // full POSIX paths ("/user/levels/x.vvv")
};

// The mounted default volume (NULL until xenon_mount_fat32() runs).
Fat32 *FsVolume(void);

// Maps a Win32-style path to a volume-relative POSIX path. Returns a pointer
// into a small rotating static buffer or the input if it cannot be mapped.
const char *MapPath(const char *name);

class fs
{
	public:
		static bool exists(const char* name)
		{
			uint32_t sz = 0; int isdir = 0;
			if (Fat32V_Stat(FsVolume(), MapPath(name), &sz, &isdir)) return true;
			return false;
		}

		static int makedir(const char* name)
		{
			return Fat32V_Mkdir(FsVolume(), MapPath(name));
		}

		static int deletedir(const char* name)
		{
			return Fat32V_Unlink(FsVolume(), MapPath(name));
		}

		static void makefile(const char* name, const char* contents = "")
		{
			Fat32V_WriteFile(FsVolume(), MapPath(name), contents, (uint32_t)strlen(contents));
		}

		static void overwritefile(const char* name, const char* contents)
		{
			if (exists(name))
			{
				Fat32V_WriteFile(FsVolume(), MapPath(name), contents, (uint32_t)strlen(contents));
			}
		}

		static void appendfile(const char* name, const char* contents)
		{
			if (exists(name))
			{
				uint32_t sz = 0;
				Fat32V_Stat(FsVolume(), MapPath(name), &sz, NULL);
				size_t clen = strlen(contents);
				if (sz > (uint32_t)(64u * 1024u * 1024u) || clen > (size_t)(64u * 1024u * 1024u))
					return; // refuse absurd sizes rather than overflow the size math
				char *buf = (char*)malloc((size_t)sz + clen + 1);
				if (!buf) return; // out of memory: drop the log line, don't crash
				if (sz) {
					long got = Fat32V_ReadFile(FsVolume(), MapPath(name), buf, sz);
					if (got < 0 || (uint32_t)got != sz) { free(buf); return; }
				}
				memcpy(buf + sz, contents, clen);
				buf[sz + clen] = '\0';
				Fat32V_WriteFile(FsVolume(), MapPath(name), buf, (uint32_t)(sz + clen));
				free(buf);
			}
		}

		static size_t filesize(const char* name)
		{
			uint32_t sz = 0;
			Fat32V_Stat(FsVolume(), MapPath(name), &sz, NULL);
			return sz;
		}

		static void log(const char* contents)
		{
			fs::appendfile("D:\\user\\logs\\log.txt", contents);
		}

		static void slog(std::string contents)
		{
			fs::appendfile("D:\\user\\logs\\log.txt", contents.c_str());
		}

		static size_t size(const char* name)
		{
			size_t s = filesize(name);
			if (s == 0)
			{
				std::string file = name;
				fs::slog("An error occured when getting size of file: " + file + "\n");
			}
			return s;
		}

		static std::vector<std::string> enumdir(std::string dir)
		{
			std::vector<std::string> list;
			// The game passes a Win32 wildcard ("D:\\user\\levels\\*").
			std::string d = dir;
			size_t star = d.find('*');
			if (star != std::string::npos) d = d.substr(0, star);
			const char *mapped = MapPath(d.c_str());

			FsEnumCtx ctx;
			ctx.dir = mapped;
			while (ctx.dir.size() > 1 && ctx.dir[ctx.dir.size()-1] == '/')
				ctx.dir.erase(ctx.dir.size()-1);

			Fat32V_ListDir(FsVolume(), mapped, enumdir_cb, &ctx);
			return ctx.out;
		}

	private:
		static int enumdir_cb(void *cookie, const char *name, uint32_t size, int is_dir)
		{
			FsEnumCtx *ctx = (FsEnumCtx *)cookie;
			(void)size;
			if (!ctx || !name || !name[0] || is_dir) return 1; // files only
			if (name[0] == '.') return 1;                      // skip . / ..
			ctx->out.push_back(ctx->dir + "/" + std::string(name));
			return 1;
		}
};
