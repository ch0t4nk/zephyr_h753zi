#include "persist.h"

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <string.h>

LOG_MODULE_REGISTER(persist, LOG_LEVEL_INF);

#define PERSIST_MOUNT_POINT "/lfs"
#define PERSIST_SENTINEL     PERSIST_MOUNT_POINT "/.mounted"

/* LittleFS configuration with moderate cache size */
FS_LITTLEFS_DECLARE_CUSTOM_CONFIG(app_storage, 32,
                                  32,   /* read_size */
                                  32,   /* prog_size */
                                  1024, /* cache_size - moderate size */
                                  32    /* lookahead_size */);

/* LittleFS mount point */
static struct fs_mount_t lfs_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &app_storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(app_storage),
    .mnt_point = PERSIST_MOUNT_POINT,
};

static bool s_mounted = false;

/* Override block_size to smaller value than erase block */
static int override_block_size(void)
{
    app_storage.cfg.block_size = 4096;
    return 0;
}

static int ensure_sentinel(void)
{
    struct fs_dirent ent;
    int rc = fs_stat(PERSIST_SENTINEL, &ent);
    if (rc == 0 && ent.type == FS_DIR_ENTRY_FILE) {
        return 0;
    }

    static const char msg[] = "ok\n";
    struct fs_file_t f;
    fs_file_t_init(&f);
    rc = fs_open(&f, PERSIST_SENTINEL, FS_O_CREATE | FS_O_WRITE);
    if (rc) {
        LOG_WRN("sentinel open failed: %d", rc);
        return rc;
    }
    ssize_t wr = fs_write(&f, msg, sizeof(msg));
    fs_close(&f);
    if (wr < 0) {
        LOG_WRN("sentinel write failed: %d", (int)wr);
        return (int)wr;
    }
    return 0;
}

int persistence_init(void)
{
    if (!s_mounted) {
        int orc = override_block_size();
        if (orc < 0) {
            LOG_ERR("Failed to override block_size: %d", orc);
            return orc;
        }

        /* Log configuration for diagnostics */
        const struct device *flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
        if (device_is_ready(flash_dev)) {
            const struct flash_parameters *params = flash_get_parameters(flash_dev);
            LOG_INF("flash geometry: write_block=%u erase=%u", 
                    (unsigned)params->write_block_size,
                    (unsigned)flash_get_write_block_size(flash_dev));
        }
        LOG_INF("lfs config: read=%u prog=%u cache=%u lookahead=%u",
                (unsigned)app_storage.cfg.read_size,
                (unsigned)app_storage.cfg.prog_size,
                (unsigned)app_storage.cfg.cache_size,
                (unsigned)app_storage.cfg.lookahead_size);

        /* Try mounting existing filesystem */
        int mrc = fs_mount(&lfs_mnt);
        if (mrc == 0) {
            LOG_INF("Mounted existing LittleFS");
            s_mounted = true;
            return ensure_sentinel();
        }

        /* Mount failed, format and try again */
        LOG_WRN("Mount failed: %d, formatting", mrc);
        int frc = fs_mkfs(FS_LITTLEFS, (uintptr_t)lfs_mnt.storage_dev, &app_storage.cfg, 0);
        if (frc < 0) {
            LOG_ERR("Format failed: %d", frc);
            return frc;
        }

        mrc = fs_mount(&lfs_mnt);
        if (mrc < 0) {
            LOG_ERR("Post-format mount failed: %d", mrc);
            return mrc;
        }

        LOG_INF("Successfully mounted fresh LittleFS");
        s_mounted = true;
        return ensure_sentinel();
    }
    return 0;
}

int persistence_write_value(const char *path, const void *data, size_t len)
{
    int rc = persistence_init();
    if (rc < 0) {
        return rc;
    }

    rc = settings_save_one(path, data, len);
    if (rc < 0) {
        LOG_WRN("Failed to write %s: %d", path, rc);
        return rc;
    }

    LOG_DBG("Wrote %zu bytes to %s", len, path);
    return 0;
}

int persistence_read_value(const char *path, void *data, size_t len, size_t *out_len)
{
    int rc = persistence_init();
    if (rc < 0) {
        return rc;
    }

    rc = settings_load_subtree(path);
    if (rc < 0) {
        LOG_WRN("Failed to load %s: %d", path, rc);
        return rc;
    }

    /* For now, return success with zero length */
    if (out_len) {
        *out_len = 0;
    }
    LOG_DBG("Read from %s", path);
    return 0;
}

void persistence_status_print(void)
{
    LOG_INF("=== Persistence Status ===");
    LOG_INF("Mounted: %s", s_mounted ? "yes" : "no");
    LOG_INF("Mount point: %s", PERSIST_MOUNT_POINT);
    
    if (s_mounted) {
        struct fs_statvfs stats;
        int rc = fs_statvfs(PERSIST_MOUNT_POINT, &stats);
        if (rc == 0) {
            LOG_INF("Free: %lu / %lu blocks",
                    (unsigned long)stats.f_bfree,
                    (unsigned long)stats.f_blocks);
        }

        struct fs_dirent ent;
        rc = fs_stat(PERSIST_SENTINEL, &ent);
        LOG_INF("Sentinel: %s", rc == 0 ? "present" : "missing");
    }
    LOG_INF("=========================");
}
