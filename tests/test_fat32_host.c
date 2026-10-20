/* test_fat32_host.c: host-side (Linux/aarch64) unit tests for the FAT32
 * driver used by the Xbox 360 port.
 *
 * This runs on the HOST, not on hardware. It exercises the same fat32.c the
 * ELF is built from, against a real FAT32 image produced by mkfs.fat and
 * populated with mtools, through a file-backed sector callback that plays the
 * role of the USB mass-storage driver.
 *
 * Build/run: tests/run_host_tests.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../xenon/source/fat32.h"

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } \
    else         { printf("  ok  : %s\n", msg); } \
} while (0)

struct Disk {
    int fd;
    const char *path;
};

static int Rd(void *ctx, uint32_t lba, uint32_t count, void *buf) {
    struct Disk *d = (struct Disk *)ctx;
    off_t off = (off_t)lba * 512;
    ssize_t n = pread(d->fd, buf, (size_t)count * 512, off);
    return n == (ssize_t)count * 512;
}

static int Wr(void *ctx, uint32_t lba, uint32_t count, const void *buf) {
    struct Disk *d = (struct Disk *)ctx;
    off_t off = (off_t)lba * 512;
    ssize_t n = pwrite(d->fd, buf, (size_t)count * 512, off);
    if (n != (ssize_t)count * 512) return 0;
    fsync(d->fd);
    return 1;
}

static long read_whole(Fat32 *fs, const char *path, char *buf, uint32_t n) {
    return Fat32V_ReadFile(fs, path, buf, n);
}

/* A file-scope thunk so the list callback has C linkage and no nesting. */
struct EnumCtx { int n; };
int enum_thunk(void *ctx, const char *name, uint32_t size, int is_dir) {
    struct EnumCtx *c = (struct EnumCtx *)ctx;
    (void)name; (void)size; (void)is_dir;
    if (c) c->n++;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s disk.img\n", argv[0]); return 2; }
    struct Disk disk;
    disk.path = argv[1];
    disk.fd = open(argv[1], O_RDWR);
    if (disk.fd < 0) { perror("open"); return 2; }

    printf("FAT32 driver host tests against %s\n", argv[1]);

    Fat32 *fs = Fat32_Open(Rd, Wr, &disk);
    CHECK(fs != NULL, "Fat32_Open");
    CHECK(Fat32_MountVolume(fs) == 1, "Fat32_MountVolume");
    CHECK(Fat32_IsWritable(fs) == 1, "volume is writable");

    /* --- stat / exists --- */
    uint32_t sz = 0;
    int isdir = 0;
    CHECK(Fat32V_Stat(fs, "/data/graphics/font.png", &sz, &isdir) == 1,
          "stat /data/graphics/font.png exists");
    CHECK(!isdir, "font.png is a file");
    CHECK(Fat32V_Stat(fs, "/data", &sz, &isdir) == 1, "stat /data exists");
    CHECK(isdir, "/data is a directory");
    CHECK(Fat32V_Stat(fs, "/nope", NULL, NULL) == 0, "stat missing path fails");

    /* --- read a known-content file --- */
    {
        char buf[256];
        memset(buf, 0, sizeof(buf));
        long n = read_whole(fs, "/data/hello.txt", buf, sizeof(buf) - 1);
        CHECK(n == 11, "read /data/hello.txt length == 11");
        CHECK(strcmp(buf, "hello world") == 0, "read /data/hello.txt contents");
    }

    /* --- read a multi-cluster file (forces FAT chain walking) --- */
    {
        uint32_t big = 200000; /* > 1 cluster at 32 KB/cluster */
        char *buf = (char *)malloc(big + 1);
        long n = Fat32V_ReadFile(fs, "/data/big.bin", buf, big);
        CHECK(n == (long)big, "read multi-cluster /data/big.bin length");
        int ok = 1;
        for (uint32_t i = 0; i < big; i++) if (buf[i] != (char)(i & 0xFF)) { ok = 0; break; }
        CHECK(ok, "multi-cluster file contents match pattern");
        free(buf);
    }

    /* --- write + read-back + unlink --- */
    {
        const char *contents = "level data 12345";
        CHECK(Fat32V_WriteFile(fs, "/user/saves/qsave.vvv", contents,
                               (uint32_t)strlen(contents)) == 1,
              "write /user/saves/qsave.vvv");
        char buf[64]; memset(buf, 0, sizeof(buf));
        long n = read_whole(fs, "/user/saves/qsave.vvv", buf, sizeof(buf) - 1);
        CHECK(n == (long)strlen(contents), "read-back length");
        CHECK(strcmp(buf, contents) == 0, "read-back contents");
        CHECK(Fat32V_Unlink(fs, "/user/saves/qsave.vvv") == 1, "unlink save");
        CHECK(Fat32V_Exists(fs, "/user/saves/qsave.vvv") == 0, "save gone after unlink");
    }

    /* --- overwrite shorter content must truncate --- */
    {
        CHECK(Fat32V_WriteFile(fs, "/user/saves/t.vvv", "AAAAAAAAAA", 10) == 1, "write 10 bytes");
        CHECK(Fat32V_WriteFile(fs, "/user/saves/t.vvv", "BB", 2) == 1, "overwrite 2 bytes");
        uint32_t s2 = 0;
        Fat32V_Stat(fs, "/user/saves/t.vvv", &s2, NULL);
        CHECK(s2 == 2, "overwrite truncated size");
        Fat32V_Unlink(fs, "/user/saves/t.vvv");
    }

    /* --- mkdir --- */
    {
        CHECK(Fat32V_Mkdir(fs, "/user/levels") == 1 || Fat32V_Exists(fs, "/user/levels"),
              "mkdir /user/levels (or exists)");
        int d = 0;
        CHECK(Fat32V_Stat(fs, "/user/levels", NULL, &d) == 1 && d, "/user/levels is a dir");
    }

    /* --- directory enumeration (used for custom level listing) --- */
    {
        struct EnumCtx cx = {0};
        long e = Fat32V_ListDir(fs, "/data/graphics", enum_thunk, &cx);
        CHECK(e >= 2, "ListDir finds >=2 graphics files");
        CHECK(cx.n >= 2, "callback invoked for each entry");
    }

    /* --- rename (same directory) --- */
    {
        const char *orig = "rename-source-data";
        CHECK(Fat32V_WriteFile(fs, "/user/saves/r1.vvv", orig, (uint32_t)strlen(orig)) == 1,
              "write rename source");
        CHECK(Fat32V_Rename(fs, "/user/saves/r1.vvv", "/user/saves/r2.vvv") == 1,
              "rename r1 -> r2");
        CHECK(Fat32V_Exists(fs, "/user/saves/r1.vvv") == 0, "rename removed source name");
        CHECK(Fat32V_Exists(fs, "/user/saves/r2.vvv") == 1, "rename created target name");
        char buf[64]; memset(buf, 0, sizeof(buf));
        long n = read_whole(fs, "/user/saves/r2.vvv", buf, sizeof(buf) - 1);
        CHECK(n == (long)strlen(orig) && strcmp(buf, orig) == 0,
              "rename preserved file contents");
        Fat32V_Unlink(fs, "/user/saves/r2.vvv");
    }

    /* --- rename over an existing file (atomic replace) --- */
    {
        CHECK(Fat32V_WriteFile(fs, "/user/saves/old.vvv", "OLDDATA", 7) == 1, "write old.vvv");
        CHECK(Fat32V_WriteFile(fs, "/user/saves/new.vvv", "NEWDATA-bigger", 14) == 1, "write new.vvv");
        CHECK(Fat32V_Rename(fs, "/user/saves/new.vvv", "/user/saves/old.vvv") == 1,
              "rename new.vvv over old.vvv");
        char buf[64]; memset(buf, 0, sizeof(buf));
        long n = read_whole(fs, "/user/saves/old.vvv", buf, sizeof(buf) - 1);
        CHECK(n == 14 && strcmp(buf, "NEWDATA-bigger") == 0, "replace yielded new contents");
        CHECK(Fat32V_Exists(fs, "/user/saves/new.vvv") == 0, "source name gone after replace");
        Fat32V_Unlink(fs, "/user/saves/old.vvv");
    }

    /* --- atomic write leaves no temp file and replaces content --- */
    {
        const char *v1 = "SAVE-VERSION-1-LONG";
        const char *v2 = "v2";
        CHECK(Fat32V_WriteFile(fs, "/user/saves/atomic.vvv", v1, (uint32_t)strlen(v1)) == 1,
              "atomic write v1");
        CHECK(Fat32V_WriteFile(fs, "/user/saves/atomic.vvv", v2, (uint32_t)strlen(v2)) == 1,
              "atomic overwrite v2");
        char buf[64]; memset(buf, 0, sizeof(buf));
        long n = read_whole(fs, "/user/saves/atomic.vvv", buf, sizeof(buf) - 1);
        CHECK(n == (long)strlen(v2) && strcmp(buf, v2) == 0, "atomic overwrite truncated + correct");
        CHECK(Fat32V_Exists(fs, "/user/saves/.vvv_tmp") == 0, "no stale temp file after write");
        Fat32V_Unlink(fs, "/user/saves/atomic.vvv");
    }

    /* --- truncated-chain read must not over-read (bounds) --- */
    Fat32_Close(fs);
    close(disk.fd);

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
