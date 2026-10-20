#pragma once
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fat32.h"

extern "C" int xenon_mount_fat32();

/* The mounted volume (defined in platform_fs.cpp). Pass this to the Fat32V_*
 * instance API; passing NULL makes every call a silent no-op, which used to
 * make fs::exists() always report "missing" and fs::log() write nowhere. */
Fat32 *FsVolume(void);

namespace fs {
  inline bool exists(const char* name) {
    uint32_t sz=0; int isdir=0;
    if (Fat32V_Stat(FsVolume(),name,&sz,&isdir)) return true;
    return false;
  }
  inline int makedir(const char* name) {
    return Fat32V_Mkdir(FsVolume(),name);
  }
  inline int deletedir(const char* name) { return 0; }
  inline void makefile(const char* name,const char* contents="") {
    Fat32V_WriteFile(FsVolume(),name,contents,(uint32_t)strlen(contents));
  }
  inline void overwritefile(const char* name,const char* contents) {
    Fat32V_WriteFile(FsVolume(),name,contents,(uint32_t)strlen(contents));
  }
  inline void appendfile(const char* name,const char* contents) {
    uint32_t sz=0;
    Fat32V_Stat(FsVolume(),name,&sz,NULL);
    char *buf=(char*)malloc(sz + strlen(contents)+1);
    if(!buf) return;
    if(sz && Fat32V_ReadFile(FsVolume(),name,buf,sz) != (long)sz) { free(buf); return; }
    strcpy(buf+sz,contents);
    Fat32V_WriteFile(FsVolume(),name,buf,(uint32_t)(sz+strlen(contents)));
    free(buf);
  }
  inline size_t filesize(const char* name) {
    uint32_t sz=0; Fat32V_Stat(FsVolume(),name,&sz,NULL); return sz;
  }
  inline size_t size(const char* name) { return filesize(name); }
  inline void log(const char* contents) {
    appendfile("user/logs/log.txt",contents);
  }
  inline void slog(const std::string& contents) {
    log(contents.c_str());
  }
  inline std::vector<std::string> enumdir(std::string dir) {
    std::vector<std::string> list;
    return list;
  }
}
