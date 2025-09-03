#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#if defined(CONFIG_NVS)
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#endif
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>

#if defined(CONFIG_FILE_SYSTEM_LITTLEFS)
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
/* small helpers */
#include <string.h>
#include <errno.h>
#endif

#if defined(CONFIG_FLASH_MAP)
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#endif

LOG_MODULE_REGISTER(smoke_persist, LOG_LEVEL_INF);

#if defined(CONFIG_FLASH_MAP)
/* Simple append-only KV log stored in the fixed partition. Each record is
 * KV_REC_SIZE bytes long and contains: [uint32_t seq][uint8_t key][uint8_t val][2 bytes reserved]
 * This is intentionally tiny (8 bytes) to work with large erase units and
 * minimize complexity. On write we append a new record; if the partition
 * is full we erase it and start at offset 0.
 */
/* payload: 8 bytes (seq(4)+key(1)+val(1)+2 reserved)
 * slot size must match device program unit; use 32 bytes which matches
 * STM32H7 write-block-size as reported by the DTS (/zephyr.dts write-block-size = 0x20)
 */
#define KV_PAYLOAD_SIZE 8
#define KV_SLOT_SIZE 32

static int kv_store_write(uint8_t key, uint8_t value)
{
    const struct flash_area *fa;
    int rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    if (rc) {
        return rc;
    }

    const size_t fa_size = fa->fa_size;
    off_t off = 0;
    uint8_t buf[KV_SLOT_SIZE];
    uint32_t last_seq = 0;

    while (off + KV_SLOT_SIZE <= fa_size) {
        rc = flash_area_read(fa, off, buf, KV_SLOT_SIZE);
        if (rc) {
            flash_area_close(fa);
            return rc;
        }
        bool erased = true;
        for (int i = 0; i < KV_SLOT_SIZE; i++) {
            if (buf[i] != 0xFF) { erased = false; break; }
        }
        if (erased) break;

        uint32_t seq = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
        uint8_t rkey = buf[4];
        if (rkey == key && seq > last_seq) {
            last_seq = seq;
        }
        off += KV_SLOT_SIZE;
    }

    uint32_t new_seq = last_seq + 1;
    if (off + KV_SLOT_SIZE > fa_size) {
        /* No space left; erase whole partition and start at 0 */
        rc = flash_area_erase(fa, 0, fa_size);
        if (rc) {
            flash_area_close(fa);
            return rc;
        }
        off = 0;
    }

    uint8_t wbuf[KV_SLOT_SIZE];
    /* default to erased pattern */
    for (int i = 0; i < KV_SLOT_SIZE; i++) {
        wbuf[i] = 0xFF;
    }
    wbuf[0] = (uint8_t)(new_seq & 0xFF);
    wbuf[1] = (uint8_t)((new_seq >> 8) & 0xFF);
    wbuf[2] = (uint8_t)((new_seq >> 16) & 0xFF);
    wbuf[3] = (uint8_t)((new_seq >> 24) & 0xFF);
    wbuf[4] = key;
    wbuf[5] = value;

    rc = flash_area_write(fa, off, wbuf, KV_SLOT_SIZE);
    flash_area_close(fa);
    return rc;
}

static int kv_store_read(uint8_t key, uint8_t *value)
{
    const struct flash_area *fa;
    int rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    if (rc) {
        return rc;
    }

    const size_t fa_size = fa->fa_size;
    off_t off = 0;
    uint8_t buf[KV_SLOT_SIZE];
    uint32_t best_seq = 0;
    uint8_t best_val = 0;
    bool found = false;

    while (off + KV_SLOT_SIZE <= fa_size) {
        rc = flash_area_read(fa, off, buf, KV_SLOT_SIZE);
        if (rc) {
            flash_area_close(fa);
            return rc;
        }
        bool erased = true;
        for (int i = 0; i < KV_SLOT_SIZE; i++) {
            if (buf[i] != 0xFF) { erased = false; break; }
        }
        if (erased) break;

        uint32_t seq = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
        uint8_t rkey = buf[4];
        uint8_t rval = buf[5];
        if (rkey == key) {
            if (!found || seq > best_seq) {
                best_seq = seq;
                best_val = rval;
                found = true;
            }
        }
        off += KV_SLOT_SIZE;
    }

    flash_area_close(fa);
    if (!found) return -ENOENT;
    *value = best_val;
    return 0;
}
#endif /* CONFIG_FLASH_MAP */

#if defined(CONFIG_FILE_SYSTEM_LITTLEFS)
/* LittleFS mount and simple file helpers (mount-once) using Zephyr FS API */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_storage);
static struct fs_mount_t lfs_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .fs_data = &lfs_storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
};

static bool lfs_mounted = false;

static int lfs_mount_if_needed(void)
{
    int rc;
    if (lfs_mounted) {
        return 0;
    }
    rc = fs_mount(&lfs_mnt);
    if (rc == 0) {
        lfs_mounted = true;
    }
    return rc;
}

static int lfs_store_write(const char *path, const void *data, size_t len)
{
    int rc = lfs_mount_if_needed();
    if (rc < 0) return rc;

    struct fs_file_t f;
    fs_file_t_init(&f);
    rc = fs_open(&f, path, FS_O_CREATE | FS_O_TRUNC | FS_O_WRITE);
    if (rc < 0) return rc;
    ssize_t w = fs_write(&f, data, len);
    fs_close(&f);
    if (w < 0) return (int)w;
    if ((size_t)w != len) return -EIO;
    return 0;
}

static int lfs_store_read(const char *path, void *data, size_t len, size_t *out_len)
{
    int rc = lfs_mount_if_needed();
    if (rc < 0) return rc;

    struct fs_file_t f;
    fs_file_t_init(&f);
    rc = fs_open(&f, path, FS_O_READ);
    if (rc < 0) return rc;
    ssize_t r = fs_read(&f, data, len);
    fs_close(&f);
    if (r < 0) return (int)r;
    if (out_len) *out_len = (size_t)r;
    return 0;
}

/* Sentinel file to detect unexpected reformat of the LittleFS partition.
 * If the sentinel is missing we treat this as a freshly formatted FS and
 * write the sentinel. The caller may inspect `was_missing` to know whether
 * the sentinel had to be created.
 */
#define LFS_SENTINEL_PATH "/lfs/.lfs_version"
#define LFS_SENTINEL_VALUE "lfsv1"

static int lfs_ensure_sentinel(bool *was_missing)
{
    char buf[32];
    size_t out = 0;
    int rc = lfs_store_read(LFS_SENTINEL_PATH, buf, sizeof(buf) - 1, &out);
    if (rc == 0) {
        /* ensure null termination */
        if (out >= sizeof(buf)) out = sizeof(buf) - 1;
        buf[out] = '\0';
        if (strcmp(buf, LFS_SENTINEL_VALUE) == 0) {
            if (was_missing) *was_missing = false;
            return 0;
        }
        LOG_WRN("LittleFS sentinel mismatch: '%s' (rewriting)", buf);
        rc = lfs_store_write(LFS_SENTINEL_PATH, LFS_SENTINEL_VALUE, strlen(LFS_SENTINEL_VALUE));
        if (was_missing) *was_missing = false;
        return rc;
    } else if (rc == -ENOENT) {
        /* sentinel missing - likely newly formatted */
        if (was_missing) *was_missing = true;
        rc = lfs_store_write(LFS_SENTINEL_PATH, LFS_SENTINEL_VALUE, strlen(LFS_SENTINEL_VALUE));
        if (rc == 0) {
            LOG_INF("LittleFS sentinel written");
        } else {
            LOG_ERR("LittleFS sentinel write failed: %d", rc);
        }
        return rc;
    } else {
        LOG_ERR("LittleFS sentinel read error: %d", rc);
        return rc;
    }
}
#endif /* CONFIG_FILE_SYSTEM_LITTLEFS */

int main(void)
{
    LOG_INF("smoke persist test starting");

    /* Configure USER button (B1) as a software RESET - presses trigger a cold reboot.
     * Use the board alias 'sw0' / nodelabel 'user_button'.
     */
#if DT_NODE_EXISTS(DT_ALIAS(sw0)) && DT_NODE_HAS_PROP(DT_ALIAS(sw0), gpios)
    {
        const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
        if (device_is_ready(user_btn.port)) {
            int rc = gpio_pin_configure_dt(&user_btn, GPIO_INPUT);
            if (rc == 0) {
                /* setup callback */
                static struct gpio_callback user_cb;
                void user_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
                {
                    ARG_UNUSED(cb);
                    ARG_UNUSED(pins);
                    LOG_INF("USER button pressed - rebooting");
                    sys_reboot(SYS_REBOOT_COLD);
                }
                gpio_init_callback(&user_cb, user_button_pressed, BIT(user_btn.pin));
                gpio_add_callback(user_btn.port, &user_cb);
                gpio_pin_interrupt_configure_dt(&user_btn, GPIO_INT_EDGE_TO_ACTIVE);
            } else {
                LOG_WRN("failed to configure USER button: %d", rc);
            }
        } else {
            LOG_WRN("USER button device not ready");
        }
    }
#endif

/* Use the board's fixed partition named storage_partition */
/* Try NVS first; fall back to LittleFS on targets with large erase pages */
littlefs_fallback:

    /* LittleFS is now the only persistence backend */
    int rc2 = lfs_mount_if_needed();
    if (rc2 < 0) {
        LOG_ERR("LittleFS mount failed: %d", rc2);
        return 0;
    }

    /* Check or create sentinel so we can detect unexpected reformat */
    bool sentinel_missing = false;
    rc2 = lfs_ensure_sentinel(&sentinel_missing);
    if (rc2 < 0) {
        LOG_WRN("LittleFS sentinel check/write failed: %d", rc2);
    } else if (sentinel_missing) {
        LOG_INF("LittleFS sentinel was missing - filesystem likely newly formatted");
    } else {
        LOG_INF("LittleFS sentinel present and valid");
    }

    const char *path = "/lfs/persist.bin";
    uint8_t key = 0x42;
    rc2 = lfs_store_write(path, &key, sizeof(key));
    if (rc2 == 0) {
        LOG_INF("LittleFS: wrote persist file");
        uint8_t rv = 0;
        size_t out = 0;
        rc2 = lfs_store_read(path, &rv, sizeof(rv), &out);
        if (rc2 == 0 && out == sizeof(rv)) {
            LOG_INF("LittleFS read key val=0x%02x", rv);
        } else {
            LOG_ERR("LittleFS read failed: %d out=%zu", rc2, out);
        }
    } else {
        LOG_ERR("LittleFS write failed: %d", rc2);
    }

    /* LED feedback: LD1 (led0) blinks if sentinel was missing (fresh FS),
     * otherwise stays solid ON when sentinel present/valid.
     */
#if DT_NODE_EXISTS(DT_ALIAS(led0)) && DT_NODE_HAS_PROP(DT_ALIAS(led0), gpios)
    {
        const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
        if (device_is_ready(led0.port)) {
            int rc = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
            if (rc) {
                LOG_WRN("failed to configure LED0: %d", rc);
            } else if (sentinel_missing) {
                LOG_INF("LED0: blinking (sentinel missing)");
                while (1) {
                    gpio_pin_toggle_dt(&led0);
                    k_sleep(K_MSEC(250));
                }
            } else {
                LOG_INF("LED0: solid ON (sentinel OK)");
                gpio_pin_set_dt(&led0, 1);
                /* Keep thread alive so LED stays ON and button IRQ stays active */
                while (1) {
                    k_sleep(K_SECONDS(5));
                }
            }
        } else {
            LOG_WRN("LED0 device not ready");
        }
    }
#endif

    return 0;
}
