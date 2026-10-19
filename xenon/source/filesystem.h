#pragma once
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fat32.h"

extern "C" int xenon_mount_fat32();

namespace fs {
  inline bool exists(const char* name) {
    uint32_t sz=0; int isdir=0;
    if (Fat32V_Stat(NULL,name,&sz,&isdir)) return true;
    return false;
  }
  inline int makedir(const char* name) {
    return Fat32V_Mkdir(NULL,name);
  }
  inline int deletedir(const char* name) { return 0; }
  inline void makefile(const char* name,const char* contents="") {
    Fat32V_WriteFile(NULL,name,contents,(uint32_t)strlen(contents));
  }
  inline void overwritefile(const char* name,const char* contents) {
    Fat32V_WriteFile(NULL,name,contents,(uint32_t)strlen(contents));
  }
  inline void appendfile(const char* name,const char* contents) {
    uint32_t sz=0;
    Fat32V_Stat(NULL,name,&sz,NULL);
    char *buf=(char*)malloc(sz + strlen(contents)+1);
    if(sz) Fat32V_ReadFile(NULL,name,buf,sz);
    strcpy(buf+sz,contents);
    Fat32V_WriteFile(NULL,name,buf,(uint32_t)(sz+strlen(contents)));
    free(buf);
  }
  inline size_t filesize(const char* name) {
    uint32_t sz=0; Fat32V_Stat(NULL,name,&sz,NULL); return sz;
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
