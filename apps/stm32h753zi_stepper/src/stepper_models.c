#include "stepper_models_autogen.h"
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#endif
#if defined(CONFIG_SETTINGS_FILE) && defined(CONFIG_APP_LINK_WITH_FS)
#include "persist.h"
#else
/* In unit tests or when FS is not linked, provide a no-op stub so we don't
 * need to link the filesystem persistence module.
 */
static inline int persistence_init(void) { return 0; }
#endif

LOG_MODULE_DECLARE(l6470, LOG_LEVEL_INF);

static int active_model_idx[2] = {0, 0}; /* axis 0 and 1 */

/* Settings key: "stepper/active" stores two bytes: index for axis0 and axis1 */
static int stepper_settings_set(const char *name, size_t len_rd, settings_read_cb read_cb, void *cb_arg)
{
    ARG_UNUSED(cb_arg);
    if (strcmp(name, "active") != 0) {
        return -ENOENT;
    }
    if (len_rd != sizeof(active_model_idx)) {
        LOG_WRN("Unexpected settings length %zu for stepper/active", len_rd);
        return -EINVAL;
    }
    ssize_t rc = read_cb(cb_arg, (void *)active_model_idx, sizeof(active_model_idx));
    if (rc < 0) {
        LOG_ERR("Failed to read settings for stepper/active: %zd", rc);
        return rc;
    }
    LOG_INF("Loaded active models: axis0=%d axis1=%d", active_model_idx[0], active_model_idx[1]);
    return 0;
}

static struct settings_handler stepper_settings = {
    .name = "stepper",
    .h_set = stepper_settings_set,
};

int stepper_persist_active(void)
{
#ifdef CONFIG_SETTINGS
    int rc = settings_save_one("stepper/active", active_model_idx, sizeof(active_model_idx));
    if (rc == 0) {
        LOG_INF("Persisted active model indices");
    } else {
        LOG_ERR("Failed to persist active model indices: %d", rc);
    }
    return rc;
#else
    ARG_UNUSED(active_model_idx);
    LOG_WRN("Settings subsystem not enabled; cannot persist active models");
    return -ENOTSUP;
#endif
}

const stepper_model_t *stepper_get_model(unsigned int idx)
{
    if (idx >= stepper_model_count) return NULL;
    return &stepper_models[idx];
}

const stepper_model_t *stepper_get_active(unsigned int axis)
{
    if (axis >= 2) return NULL;
    return &stepper_models[active_model_idx[axis]];
}

int stepper_set_active_by_name(unsigned int axis, const char *name)
{
    if (axis >= 2) return -EINVAL;
    for (unsigned int i = 0; i < stepper_model_count; i++) {
        if (strcmp(stepper_models[i].name, name) == 0) {
            active_model_idx[axis] = i;
            LOG_INF("Active model for axis %u set to %s", axis, name);
            /* Persist selection asynchronously */
            stepper_persist_active();
            return 0;
        }
    }
    return -ENOENT;
}

unsigned int stepper_get_model_count(void)
{
    return stepper_model_count;
}

#ifdef CONFIG_SETTINGS
static int stepper_settings_init(void)
{
    /* If using the file backend, ensure FS is mounted before settings init. */
#if defined(CONFIG_SETTINGS_FILE) && defined(CONFIG_APP_LINK_WITH_FS)
    (void)persistence_init();
#endif
    settings_subsys_init();
    settings_register(&stepper_settings);
    /* Attempt to load settings; ignore errors so defaults remain intact */
    settings_load();
    return 0;
}

SYS_INIT(stepper_settings_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#else
/* Settings not enabled: no-op init and persist is handled by stub above */
#endif
