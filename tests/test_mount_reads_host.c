/* test_mount_reads_host.c: host-side guard that mounting a large FAT32 volume
 * does NOT scan the entire FAT.
 *
 * The bug on hardware: Fat32_MountVolume() called CountFreeClusters(), which
 * walked every FAT sector through the USB mass-storage driver one 512-byte read
 * at a time. On a multi-GB stick that is tens of thousands of synchronous USB
 * transactions and took ~10 minutes to boot.
 *
 * Mount now trusts the FSInfo hint. This test mounts a large (sparse) image and
 * asserts the whole mount stays within a small, fixed number of sector reads.
 * It also checks that reading a multi-cluster file batches sectors per cluster
 * rather than issuing one USB read per 512 bytes.
 *
 * Runs on the HOST only; it does not prove hardware behaviour.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "../xenon/source/fat32.h"

static long g_read_calls = 0;
static long g_read_sectors = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } \
    else         { printf("  ok  : %s\n", msg); } \
} while (0)

static int Rd(void *ctx, uint32_t lba, uint32_t count, void *buf) {
    int fd = *(int *)ctx;
    g_read_calls++;
    g_read_sectors += count;
    ssize_t n = pread(fd, buf, (size_t)count * 512, (off_t)lba * 512);
    return n == (ssize_t)count * 512;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s big.img\n", argv[0]); return 2; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 2; }

    Fat32 *fs = Fat32_Open(Rd, NULL, &fd);
    CHECK(fs != NULL, "Fat32_Open");
    if (!fs) return 1;

    g_read_calls = 0;
    g_read_sectors = 0;
    CHECK(Fat32_MountVolume(fs) == 1, "Fat32_MountVolume");

    printf("  info: mount issued %ld read call(s), %ld sector(s)\n",
           g_read_calls, g_read_sectors);

    /* A full-FAT scan on this image is thousands of sectors. Any correct mount
     * that reads only the boot sector, FSInfo, and the root directory is far
     * below this bound regardless of volume size. */
    CHECK(g_read_sectors < 200,
          "mount reads < 200 sectors (no full-FAT scan)");
    CHECK(g_read_calls < 200, "mount issues < 200 read calls");

    /* Batched file reads: one read per cluster, not per sector. */
    g_read_calls = 0;
    g_read_sectors = 0;
    uint32_t sz = 0;
    if (Fat32V_Stat(fs, "/data/big.bin", &sz, NULL) == 1 && sz > 0) {
        char *buf = (char *)malloc(sz);
        if (buf) {
            long n = Fat32V_ReadFile(fs, "/data/big.bin", buf, sz);
            printf("  info: read %ld bytes in %ld call(s), %ld sector(s)\n",
                   n, g_read_calls, g_read_sectors);
            CHECK(n == (long)sz, "multi-cluster read returns full size");
            CHECK(g_read_calls * 2 <= g_read_sectors,
                  "file reads are batched (fewer calls than sectors)");
            free(buf);
        }
    } else {
        printf("  info: /data/big.bin absent, skipping batch check\n");
    }

    Fat32_Close(fs);
    close(fd);

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
