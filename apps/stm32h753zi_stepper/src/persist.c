#include "persist.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>

LOG_MODULE_REGISTER(persist, LOG_LEVEL_INF);

#define PERSIST_MOUNT_POINT "/lfs"
#define PERSIST_SENTINEL     PERSIST_MOUNT_POINT "/.mounted"

/* LittleFS mount description on the fixed 'storage' partition */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);

static struct fs_mount_t lfs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
	.mnt_point = PERSIST_MOUNT_POINT,
};

static bool s_mounted = false;

static int ensure_sentinel(void)
{
	struct fs_dirent ent;
	int rc = fs_stat(PERSIST_SENTINEL, &ent);
	if (rc == 0 && ent.type == FS_DIR_ENTRY_FILE) {
		return 0; /* already there */
	}

	/* Create a small sentinel file */
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
		int rc = fs_mount(&lfs_mnt);
		if (rc != 0) {
			LOG_WRN("fs_mount failed: %d, trying mkfs", rc);
#if defined(CONFIG_APP_LINK_WITH_FS)
			/* Format the partition and retry mount */
			int frc = fs_mkfs(FS_LITTLEFS,
							  (uintptr_t)lfs_mnt.storage_dev,
							  &storage,
							  0);
			if (frc != 0) {
				LOG_ERR("mkfs failed: %d", frc);
				return frc;
			}
			rc = fs_mount(&lfs_mnt);
			if (rc != 0) {
				LOG_ERR("fs_mount after mkfs failed: %d", rc);
				return rc;
			}
#else
			return rc;
#endif
		}
		s_mounted = true;
	}
	/* Create sentinel file if missing */
	return ensure_sentinel();
}

int persistence_write_value(const char *path, const void *data, size_t len)
{
	if (!path || !data) {
		return -EINVAL;
	}
	if (!s_mounted) {
		int rc = persistence_init();
		if (rc) return rc;
	}

	struct fs_file_t f;
	fs_file_t_init(&f);
	int rc = fs_open(&f, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (rc) {
		return rc;
	}
	ssize_t wr = fs_write(&f, data, len);
	int saved = (wr < 0) ? (int)wr : 0;
	fs_close(&f);
	return saved;
}

int persistence_read_value(const char *path, void *data, size_t len, size_t *out_len)
{
	if (!path || !data) {
		return -EINVAL;
	}
	if (!s_mounted) {
		int rc = persistence_init();
		if (rc) return rc;
	}

	struct fs_file_t f;
	fs_file_t_init(&f);
	int rc = fs_open(&f, path, FS_O_READ);
	if (rc) {
		return rc;
	}
	ssize_t rd = fs_read(&f, data, len);
	fs_close(&f);
	if (rd < 0) {
		return (int)rd;
	}
	if (out_len) {
		*out_len = (size_t)rd;
	}
	return 0;
}

void persistence_status_print(void)
{
	printk("persist: mount=%s dev=0x%lx mnt=%s\n",
		   s_mounted ? "yes" : "no",
		   (unsigned long)lfs_mnt.storage_dev,
		   lfs_mnt.mnt_point);
}

