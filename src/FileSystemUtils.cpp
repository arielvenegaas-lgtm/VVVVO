#include "FileSystemUtils.h"
#include "filesystem.h"
#include "Utility.h"

#include <vector>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

// The game's asset tree and user data live on the mounted FAT32 volume. These
// are volume-relative POSIX paths (see MapPath() in filesystem.h).
char* saveDir = "D:\\user\\saves\\";
char* levelDir = "D:\\user\\levels";
std::string dateLog = "D:\\user\\logs\\log.txt";

int FILESYSTEM_init()
{
	if (!xenon_mount_fat32())
	{
		return 0;
	}

	// Create the user tree parents-first (Fat32V_Mkdir needs the parent to
	// exist); without this the leading "logs" mkdir can fail and disable
	// logging for the whole session.
	if(!fs::exists("D:\\user")) fs::makedir("D:\\user");
	if(!fs::exists("D:\\user\\logs")) fs::makedir("D:\\user\\logs");
	if(!fs::exists("D:\\user\\saves")) fs::makedir("D:\\user\\saves");
	if(!fs::exists("D:\\user\\levels")) fs::makedir("D:\\user\\levels");
	if(!fs::exists("D:\\data")) fs::makedir("D:\\data");
	if(!fs::exists("D:\\data\\music")) fs::makedir("D:\\data\\music");

	/* Log file; printf alternative */
	if(!fs::exists(dateLog.c_str())) fs::makefile(dateLog.c_str());
	fs::overwritefile(dateLog.c_str(), "");

	//Check if data exists; would be better to check for all but ehh
	if(!fs::exists("D:\\data\\graphics\\font.png"))
	{
		fs::makefile("D:\\README.txt", "Please extract your data.zip into the data folder.\nYou may delete this file afterwards if you want to.");
		return 0;
	}
	else
	{
		return 1;
	}
}

char *FILESYSTEM_getUserSaveDirectory()
{
	return saveDir;
}

char *FILESYSTEM_getUserLevelDirectory()
{
	return levelDir;
}

void FILESYSTEM_loadFileToMemory(const char* name, char** mem, size_t* len)
{
	*mem = NULL;
	if (len) *len = 0;

	long length = (long)fs::size(name);
	if (length <= 0 || length > (long)(64 * 1024 * 1024))
	{
		// Missing, empty, or absurdly large: treat as "not found" so callers
		// take their error branch instead of parsing an empty buffer.
		return;
	}

	// Allocate one extra byte so callers that treat the result as a text
	// buffer (TinyXML, level parsing) always see a terminating NUL.
	char *buf = (char*) malloc((size_t)length + 1);
	if (!buf) return;

	long got = Fat32V_ReadFile(FsVolume(), MapPath(name), buf, (uint32_t)length);
	if (got < 0 || got != length)
	{
		// Short/corrupt read: free and report not-found rather than handing
		// back a truncated document.
		free(buf);
		return;
	}
	buf[length] = '\0';
	*mem = buf;
	if (len) *len = (size_t)length;
}

void FILESYSTEM_freeMemory(char **mem)
{
	free(*mem);
	*mem = NULL;
}

std::vector<std::string> FILESYSTEM_getLevelDirFileNames()
{
	return fs::enumdir("D:\\user\\levels\\*");
}
