/* platform_file.cpp: route libc FILE I/O (fopen/fread/fwrite/fseek/...) through
 * the mounted FAT32 volume.
 *
 * lodepng (PNG load), TinyXML (save files, custom levels) and libstdc++'s
 * std::ifstream (BinaryBlob) all use plain fopen(). libXenon/newlib has no
 * filesystem, so without this every asset load and every save fails.
 *
 * We wrap fopen at link time (see -Wl,--wrap=fopen in the Makefile) and hand
 * newlib a in-memory FILE* via funopen(). Small files (PNG/XML) are read in
 * one go; writes are buffered and committed on fclose.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fat32.h"

extern "C" {
int xenon_mount_fat32(void);
}

const char *MapPath(const char *name);
Fat32 *FsVolume(void);

namespace {

struct MemFile {
    unsigned char *data;
    size_t         len;
    size_t         cap;
    size_t         pos;
    int            writing;
    char          *path;   /* mapped FAT32 path, freed on close */
};

static int MemGrow(MemFile *m, size_t need) {
    if (need <= m->cap) return 1;
    size_t cap = m->cap ? m->cap : 4096;
    while (cap < need) cap *= 2;
    unsigned char *p = (unsigned char *)realloc(m->data, cap);
    if (!p) return 0;
    m->data = p;
    m->cap = cap;
    return 1;
}

static int MemRead(void *cookie, char *buf, _READ_WRITE_BUFSIZE_TYPE n) {
    MemFile *m = (MemFile *)cookie;
    if (!m || m->writing || n <= 0) return 0;
    size_t left = m->len - m->pos;
    size_t take = ((size_t)n < left) ? (size_t)n : left;
    if (take) {
        memcpy(buf, m->data + m->pos, take);
        m->pos += take;
    }
    return (int)take;
}

static int MemWrite(void *cookie, const char *buf, _READ_WRITE_BUFSIZE_TYPE n) {
    MemFile *m = (MemFile *)cookie;
    if (!m || !m->writing || n <= 0) return 0;
    if (!MemGrow(m, m->pos + (size_t)n)) return 0;
    memcpy(m->data + m->pos, buf, (size_t)n);
    m->pos += (size_t)n;
    if (m->pos > m->len) m->len = m->pos;
    return (int)n;
}

#ifdef __LARGE64_FILES
static _fpos64_t MemSeek(void *cookie, _fpos64_t off, int whence)
#else
static fpos_t MemSeek(void *cookie, fpos_t off, int whence)
#endif
{
    MemFile *m = (MemFile *)cookie;
    if (!m) return (fpos_t)-1;
    long base = (long)m->pos, end = (long)m->len;
    long np;
    switch (whence) {
        case SEEK_SET: np = (long)off; break;
        case SEEK_CUR: np = base + (long)off; break;
        case SEEK_END: np = end + (long)off; break;
        default: return (fpos_t)-1;
    }
    if (np < 0) return (fpos_t)-1;
    m->pos = (size_t)np;
    return (fpos_t)np;
}

static int MemClose(void *cookie) {
    MemFile *m = (MemFile *)cookie;
    if (!m) return 0;
    if (m->writing && m->path) {
        Fat32 *fs = FsVolume();
        if (!fs || !Fat32V_WriteFile(fs, m->path, m->data ? m->data : (const void *)"",
                                     (uint32_t)m->len)) {
            /* nothing else we can do; keep the old file if any */
        }
    }
    free(m->data);
    free(m->path);
    free(m);
    return 0;
}

} /* namespace */

extern "C" FILE *__wrap_fopen(const char *path, const char *mode) {
    if (!path || !mode) { errno = EINVAL; return NULL; }
    if (!xenon_mount_fat32()) { errno = ENODEV; return NULL; }

    const char *mapped = MapPath(path);
    int writing = (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) != NULL;
    int append  = strchr(mode, 'a') != NULL;

    MemFile *m = (MemFile *)calloc(1, sizeof(MemFile));
    if (!m) { errno = ENOMEM; return NULL; }
    m->writing = writing;
    m->path = (char *)malloc(strlen(mapped) + 1);
    if (m->path) strcpy(m->path, mapped);
    if (!m->path) { free(m); errno = ENOMEM; return NULL; }

    Fat32 *fs = FsVolume();
    uint32_t size = 0;
    int isdir = 0;
    int exists = fs && Fat32V_Stat(fs, mapped, &size, &isdir) && !isdir;

    if (!writing && !exists) { free(m->path); free(m); errno = ENOENT; return NULL; }

    if ((exists && !append) || (append && exists)) {
        if (size) {
            if (!MemGrow(m, size)) { free(m->path); free(m); errno = ENOMEM; return NULL; }
            long got = Fat32V_ReadFile(fs, mapped, m->data, size);
            if (got < 0) { free(m->data); free(m->path); free(m); errno = EIO; return NULL; }
            m->len = (size_t)got;
        }
        if (append) m->pos = m->len;
    }

    if (writing && !append) { m->pos = 0; m->len = 0; }

    FILE *f = funopen(m, MemRead, MemWrite, MemSeek, MemClose);
    if (!f) {
        free(m->data); free(m->path); free(m); errno = ENOMEM; return NULL;
    }
    return f;
}

/* The game deletes quick/teleporter saves with remove(); map it onto the
 * mounted volume as well (we wrap it at link time, see -Wl,--wrap=remove). */
extern "C" int __wrap_remove(const char *path) {
    if (!path) { errno = EINVAL; return -1; }
    if (!xenon_mount_fat32()) { errno = ENODEV; return -1; }
    const char *mapped = MapPath(path);
    if (!Fat32V_Exists(FsVolume(), mapped)) { errno = ENOENT; return -1; }
    return Fat32V_Unlink(FsVolume(), mapped) ? 0 : -1;
}
