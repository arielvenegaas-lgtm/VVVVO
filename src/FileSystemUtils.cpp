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

	/* Log file; printf alternative */
	fs::makedir("D:\\user\\logs");
	if(!fs::exists(dateLog.c_str())) fs::makefile(dateLog.c_str());
	fs::overwritefile(dateLog.c_str(), "");

	if(!fs::exists("D:\\user")) fs::makedir("D:\\user");
	if(!fs::exists("D:\\user\\saves")) fs::makedir("D:\\user\\saves");
	if(!fs::exists("D:\\user\\levels")) fs::makedir("D:\\user\\levels");
	if(!fs::exists("D:\\data")) fs::makedir("D:\\data");
	if(!fs::exists("D:\\data\\music")) fs::makedir("D:\\data\\music");

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
	int length = (int)fs::size(name);
	*mem = (char*) malloc(length);
	if (length > 0)
	{
		Fat32V_ReadFile(FsVolume(), MapPath(name), *mem, length);
	}
	if (len) *len = length;
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
