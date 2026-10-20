/* platform_fs.cpp: mount a FAT32 volume on libXenon and expose it to the game.
 *
 * libXenon has no filesystem driver, so we locate a FAT32 volume ourselves.
 * The volume is read through libXenon's USB mass-storage driver: the EHCI
 * stack exposes USBStorage_Read_Sectors_0()/Write_Sectors_0(), which match the
 * Fat32ReadSectors/Fat32WriteSectors callback shape exactly.
 *
 * The mounted volume is the game's "D:\\" drive (assets + user data).
 */

#include "fat32.h"

#include <stdint.h>
#include <usb/usbmain.h>

/* libXenon's USB mass-storage sector callbacks (tinyehci/usbstorage.c). Their
 * header drags in CFE internals, so we forward-declare the ones we use.
 * libXenon supports up to three USB storage devices; probe all of them so the
 * stick is found regardless of which port it is plugged into. */
extern "C" int32_t USBStorage_Read_Sectors_0(uint32_t sector, uint32_t numSectors, void *buffer);
extern "C" int32_t USBStorage_Write_Sectors_0(uint32_t sector, uint32_t numSectors, const void *buffer);
extern "C" int32_t USBStorage_Inserted_0(void);
extern "C" int32_t USBStorage_Read_Sectors_1(uint32_t sector, uint32_t numSectors, void *buffer);
extern "C" int32_t USBStorage_Write_Sectors_1(uint32_t sector, uint32_t numSectors, const void *buffer);
extern "C" int32_t USBStorage_Inserted_1(void);
extern "C" int32_t USBStorage_Read_Sectors_2(uint32_t sector, uint32_t numSectors, void *buffer);
extern "C" int32_t USBStorage_Write_Sectors_2(uint32_t sector, uint32_t numSectors, const void *buffer);
extern "C" int32_t USBStorage_Inserted_2(void);

/* One Fat32 instance for the whole process; the game reaches it via
 * FsVolume(). */
static Fat32 *g_fs = 0;

struct UsbDevice {
    int32_t (*inserted)(void);
    int32_t (*read)(uint32_t, uint32_t, void *);
    int32_t (*write)(uint32_t, uint32_t, const void *);
};

static const UsbDevice g_devices[3] = {
    { USBStorage_Inserted_0, USBStorage_Read_Sectors_0, USBStorage_Write_Sectors_0 },
    { USBStorage_Inserted_1, USBStorage_Read_Sectors_1, USBStorage_Write_Sectors_1 },
    { USBStorage_Inserted_2, USBStorage_Read_Sectors_2, USBStorage_Write_Sectors_2 },
};

/* Device backing the currently mounted volume. */
static const UsbDevice *g_dev = 0;

static int UsbRead(void *ctx, uint32_t lba, uint32_t count, void *buf) {
    (void)ctx;
    return (g_dev && g_dev->read(lba, count, buf)) ? 1 : 0;
}

static int UsbWrite(void *ctx, uint32_t lba, uint32_t count, const void *buf) {
    (void)ctx;
    return (g_dev && g_dev->write(lba, count, buf)) ? 1 : 0;
}

Fat32 *FsVolume(void) {
    return g_fs;
}

/* Translate the Xbox build's Win32 paths ("D:\\data\\graphics\\tiles.png" into
 * volume-relative POSIX paths ("/data/graphics/tiles.png"). Uses a small
 * rotating buffer so several paths can be live at once. */
const char *MapPath(const char *name) {
    static char bufs[4][512];
    static int which = 0;
    if (!name) return "";
    char *out = bufs[which];
    which = (which + 1) & 3;

    /* Strip a drive prefix ("D:" or "d:"). */
    if (name[0] && name[1] == ':') name += 2;

    while (*name == '\\' || *name == '/') ++name;

    size_t i = 0;
    out[i++] = '/';
    for (; *name && i < sizeof(bufs[0]) - 1; ++name) {
        out[i++] = (*name == '\\') ? '/' : *name;
    }
    out[i] = '\0';
    return out;
}

extern "C" int xenon_mount_fat32(void) {
    if (g_fs) return 1;

    /* Bring up USB and let the mass-storage driver enumerate. USBStorage_Init()
     * is also called from the per-frame input poll, so a few calls here give a
     * freshly attached stick time to mount. */
    usb_init();
    for (int i = 0; i < 64; ++i) {
        if (g_devices[0].inserted() || g_devices[1].inserted() ||
            g_devices[2].inserted())
            break;
        usb_do_poll();
    }

    /* Prefer a volume that actually contains the game data. This makes the
     * search deterministic: "the FAT32 volume whose root has data/graphics/
     * font.png", independent of USB port order. If none has data, keep the
     * first mountable volume so FILESYSTEM_init() can report the missing-data
     * error instead of pretending there is no filesystem. */
    Fat32 *fallback = 0;
    const UsbDevice *fallback_dev = 0;
    for (int d = 0; d < 3; ++d) {
        const UsbDevice *dev = &g_devices[d];
        if (!dev->inserted()) continue;
        g_dev = dev;
        Fat32 *fs = Fat32_Open(UsbRead, UsbWrite, 0);
        if (!fs) continue;
        if (!Fat32_MountVolume(fs)) {
            Fat32_Close(fs);
            continue;
        }
        if (Fat32V_Stat(fs, "/data/graphics/font.png", 0, 0)) {
            g_fs = fs;
            return 1;
        }
        if (!fallback) {
            fallback = fs;
            fallback_dev = dev;
        } else {
            Fat32_Close(fs);
        }
    }

    if (fallback) {
        g_fs = fallback;
        g_dev = fallback_dev;
        return 1;
    }
    g_dev = 0;
    return 0;
}
