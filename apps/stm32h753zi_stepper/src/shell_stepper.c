#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <stdbool.h>
#include "l6470.h"
#include "stepper_models.h"
#include "status_poll.h"
#if defined(CONFIG_SETTINGS_FILE) && defined(CONFIG_APP_LINK_WITH_FS)
#include "persist.h"
#include <zephyr/fs/fs.h>
#endif

LOG_MODULE_REGISTER(shell_stepper, LOG_LEVEL_INF);

static int cmd_stepper_status(const struct shell *shell, size_t argc, char **argv)
{
    uint16_t statuses[L6470_DAISY_SIZE];
    int ret = l6470_get_status_all(statuses);
    if (ret) {
        shell_warn(shell, "Failed to read L6470 statuses: %d", ret);
        return ret;
    }
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        char buf0[128];
        char buf1[128];
        l6470_decode_status_verbose(statuses[0], buf0, sizeof(buf0));
        l6470_decode_status_verbose(statuses[1], buf1, sizeof(buf1));
        shell_print(shell, "L6470 verbose: dev0=%s; dev1=%s", buf0, buf1);
    } else {
        shell_print(shell, "L6470 status: dev0=0x%04x (%s), dev1=0x%04x (%s)",
                    statuses[0], l6470_decode_status(statuses[0]),
                    statuses[1], l6470_decode_status(statuses[1]));
    }
    return 0;
}

static int cmd_stepper_reset(const struct shell *shell, size_t argc, char **argv)
{
    int ret = l6470_reset_pulse();
    if (ret) {
        shell_warn(shell, "Reset failed: %d", ret);
        return ret;
    }
    shell_print(shell, "Reset pulse sent");
    return 0;
}

static int cmd_stepper_probe(const struct shell *shell, size_t argc, char **argv)
{
    /* Probe: attempt single GET_STATUS via l6470_get_status_all */
    uint16_t statuses[L6470_DAISY_SIZE];
    int ret = l6470_get_status_all(statuses);
    if (ret) {
        shell_warn(shell, "Probe failed: %d", ret);
        return ret;
    }
    shell_print(shell, "Probe OK: dev0=0x%04x dev1=0x%04x", statuses[0], statuses[1]);
    return 0;
}

static int cmd_stepper_run(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_print(shell, "Usage: stepper run <dev> <dir:0|1> <speed>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int dir = atoi(argv[2]);
    uint32_t speed = (uint32_t)atoi(argv[3]);
    int ret = l6470_run((uint8_t)dev, (uint8_t)dir, speed);
    if (ret == -ENOSYS) {
        shell_print(shell, "run: not implemented yet");
        return ret;
    }
    if (ret == -EACCES) {
        shell_print(shell, "run: power disabled - enable with 'stepper power on'");
        return ret;
    }
    if (ret == -EFAULT) {
        shell_print(shell, "run: device fault - check 'stepper status'");
        return ret;
    }
    if (ret) {
        shell_warn(shell, "run failed: %d", ret);
    }
    return ret;
}

static int cmd_stepper_move(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_print(shell, "Usage: stepper move <dev> <dir:0|1> <steps>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int dir = atoi(argv[2]);
    uint32_t steps = (uint32_t)atoi(argv[3]);
    int ret = l6470_move((uint8_t)dev, (uint8_t)dir, steps);
    if (ret == -ENOSYS) {
        shell_print(shell, "move: not implemented yet");
        return ret;
    }
    if (ret == -EACCES) {
        shell_print(shell, "move: power disabled - enable with 'stepper power on'");
        return ret;
    }
    if (ret == -EFAULT) {
        shell_print(shell, "move: device fault - check 'stepper status'");
        return ret;
    }
    if (ret) {
        shell_warn(shell, "move failed: %d", ret);
    }
    return ret;
}

static int cmd_stepper_power(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper power on|off|status");
        return -EINVAL;
    }
    if (strcmp(argv[1], "on") == 0) {
        int r = l6470_power_enable();
        if (r == 0) shell_print(shell, "Power enabled");
        else shell_warn(shell, "Power enable failed: %d", r);
        return r;
    } else if (strcmp(argv[1], "off") == 0) {
        int r = l6470_power_disable();
        if (r == 0) shell_print(shell, "Power disabled");
        else shell_warn(shell, "Power disable failed: %d", r);
        return r;
    } else if (strcmp(argv[1], "status") == 0) {
        int s = l6470_power_status();
        if (s < 0) {
            shell_warn(shell, "Power status error: %d", s);
            return s;
        }
        shell_print(shell, "Power: %s", s ? "ENABLED" : "DISABLED");
        return 0;
    }
    shell_print(shell, "Unknown power command");
    return -EINVAL;
}

static int cmd_stepper_disable(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper disable <dev> [hard]");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    bool hard = false;
    if (argc > 2 && strcmp(argv[2], "hard") == 0) {
        hard = true;
    }
    int ret = l6470_disable_outputs((uint8_t)dev, hard);
    if (ret) {
        shell_warn(shell, "disable failed: %d", ret);
    }
    return ret;
}

static int cmd_stepper_goto(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_print(shell, "Usage: stepper goto <dev> <abs_pos>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    uint32_t pos = (uint32_t)strtoul(argv[2], NULL, 0);
    int r = l6470_goto((uint8_t)dev, pos);
    if (r == -EACCES) shell_print(shell, "goto: power disabled - 'stepper power on'");
    else if (r) shell_warn(shell, "goto failed: %d", r);
    return r;
}

static int cmd_stepper_goto_dir(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_print(shell, "Usage: stepper goto_dir <dev> <dir:0|1> <abs_pos>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int dir = atoi(argv[2]);
    uint32_t pos = (uint32_t)strtoul(argv[3], NULL, 0);
    int r = l6470_goto_dir((uint8_t)dev, (uint8_t)dir, pos);
    if (r == -EACCES) shell_print(shell, "goto_dir: power disabled - 'stepper power on'");
    else if (r) shell_warn(shell, "goto_dir failed: %d", r);
    return r;
}

static int cmd_stepper_home(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper home <dev>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int r = l6470_go_home((uint8_t)dev);
    if (r == -EACCES) shell_print(shell, "home: power disabled - 'stepper power on'");
    else if (r) shell_warn(shell, "home failed: %d", r);
    return r;
}

static int cmd_stepper_zero(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper zero <dev>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int r = l6470_reset_pos((uint8_t)dev);
    if (r) shell_warn(shell, "zero failed: %d", r);
    else shell_print(shell, "ABS_POS reset to 0");
    return r;
}

static int cmd_stepper_pos(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper pos <dev>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int32_t pos = 0;
    int r = l6470_get_abs_pos((uint8_t)dev, &pos);
    if (r) {
        shell_warn(shell, "pos read failed: %d", r);
        return r;
    }
    shell_print(shell, "ABS_POS[%d] = %d (0x%06x)", dev, pos, (unsigned)(pos & 0x3FFFFF));
    return 0;
}

static int cmd_stepper_stop(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper stop <dev> [hard]");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    bool hard = (argc > 2 && strcmp(argv[2], "hard") == 0);
    int r = hard ? l6470_hardstop((uint8_t)dev) : l6470_softstop((uint8_t)dev);
    if (r) shell_warn(shell, "stop failed: %d", r);
    return r;
}

static int cmd_stepper_busy(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper busy <dev>");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    bool busy = false;
    int r = l6470_is_busy((uint8_t)dev, &busy);
    if (r) shell_warn(shell, "busy failed: %d", r);
    else shell_print(shell, "BUSY[%d] = %s", dev, busy ? "true" : "false");
    return r;
}

static int cmd_stepper_waitbusy(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper waitbusy <dev> [timeout_ms] [poll_ms]");
        return -EINVAL;
    }
    int dev = atoi(argv[1]);
    int32_t timeout = (argc > 2) ? (int32_t)strtol(argv[2], NULL, 0) : 5000;
    int32_t poll = (argc > 3) ? (int32_t)strtol(argv[3], NULL, 0) : 10;
    int r = l6470_wait_while_busy((uint8_t)dev, timeout, poll);
    if (r == -ETIMEDOUT) shell_print(shell, "waitbusy timed out");
    else if (r) shell_warn(shell, "waitbusy failed: %d", r);
    else shell_print(shell, "not busy");
    return r;
}

static int cmd_stepper_limits(const struct shell *shell, size_t argc, char **argv)
{
    shell_print(shell, "L6470 limits:");
    shell_print(shell, "  max_speed_steps_per_sec = %u", l6470_get_max_speed_steps_per_sec());
    shell_print(shell, "  max_current_mA         = %u", l6470_get_max_current_ma());
    shell_print(shell, "  max_steps_per_command  = %u", l6470_get_max_steps_per_command());
    shell_print(shell, "  max_microstep          = %u", l6470_get_max_microstep());
    return 0;
}

#if defined(CONFIG_SETTINGS_FILE) && defined(CONFIG_APP_LINK_WITH_FS)
static int cmd_stepper_persist(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper persist <status|dump [path]|clear [path]>");
        return -EINVAL;
    }
    if (strcmp(argv[1], "status") == 0) {
        /* Prints via printk() */
        persistence_status_print();
        return 0;
    } else if (strcmp(argv[1], "dump") == 0) {
        const char *path = (argc > 2) ? argv[2] : CONFIG_SETTINGS_FILE_PATH;
        struct fs_dirent ent;
        int rc = fs_stat(path, &ent);
        if (rc) {
            shell_print(shell, "fs_stat failed for %s: %d", path, rc);
            return rc;
        }
        shell_print(shell, "file: %s size=%d type=%s", path, (int)ent.size,
                    ent.type == FS_DIR_ENTRY_FILE ? "file" : "other");
        struct fs_file_t f;
        fs_file_t_init(&f);
        rc = fs_open(&f, path, FS_O_READ);
        if (rc) {
            shell_print(shell, "open failed: %d", rc);
            return rc;
        }
        uint8_t buf[128];
        ssize_t rd = fs_read(&f, buf, sizeof(buf));
        fs_close(&f);
        if (rd < 0) {
            shell_print(shell, "read failed: %d", (int)rd);
            return (int)rd;
        }
        shell_print(shell, "first %d bytes:", (int)rd);
        for (int i = 0; i < rd; i += 16) {
            char line[80];
            int off = 0;
            off += snprintk(line, sizeof(line), "%04x: ", i);
            for (int j = 0; j < 16 && (i + j) < rd; j++) {
                off += snprintk(line + off, sizeof(line) - off, "%02x ", buf[i + j]);
            }
            shell_print(shell, "%s", line);
        }
        return 0;
    } else if (strcmp(argv[1], "clear") == 0) {
        const char *path = (argc > 2) ? argv[2] : CONFIG_SETTINGS_FILE_PATH;
        int rc = fs_unlink(path);
        if (rc) {
            shell_print(shell, "unlink %s failed: %d", path, rc);
            return rc;
        }
        shell_print(shell, "cleared %s", path);
        return 0;
    } else if (strcmp(argv[1], "smoke") == 0) {
        /* 1) Mount status */
        persistence_status_print();
        /* 2) Settings file info */
        const char *path = (argc > 2) ? argv[2] : CONFIG_SETTINGS_FILE_PATH;
        struct fs_dirent ent;
        int rc = fs_stat(path, &ent);
        if (rc == 0) {
            shell_print(shell, "settings: %s size=%d type=%s", path, (int)ent.size,
                        ent.type == FS_DIR_ENTRY_FILE ? "file" : "other");
        } else {
            shell_print(shell, "settings: %s not found (rc=%d)", path, rc);
        }
        /* 3) Active models */
        const stepper_model_t *m0 = stepper_get_active(0);
        const stepper_model_t *m1 = stepper_get_active(1);
        shell_print(shell, "active: axis0=%s, axis1=%s",
                    m0 ? m0->name : "(none)", m1 ? m1->name : "(none)");
        return 0;
    }
    shell_print(shell, "Unknown subcommand. Usage: stepper persist <status|dump [path]|clear [path]|smoke [path]>");
    return -EINVAL;
}
#endif

static int cmd_stepper_poll(const struct shell *shell, size_t argc, char **argv)
{
#if !defined(CONFIG_STEPPER_STATUS_POLL)
    shell_print(shell, "Status poll not built-in. Enable CONFIG_STEPPER_STATUS_POLL.");
    return -ENOTSUP;
#else
    if (argc < 2) {
        shell_print(shell, "Usage: stepper poll enable|disable|dump <dev> [n]");
        return -EINVAL;
    }
    if (strcmp(argv[1], "enable") == 0) {
        stepper_poll_set_enabled(true);
        shell_print(shell, "poll: enabled");
        return 0;
    } else if (strcmp(argv[1], "disable") == 0) {
        stepper_poll_set_enabled(false);
        shell_print(shell, "poll: disabled");
        return 0;
    } else if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) {
            shell_print(shell, "Usage: stepper poll dump <dev> [n]");
            return -EINVAL;
        }
        uint8_t dev = (uint8_t)atoi(argv[2]);
        uint8_t n = (argc > 3) ? (uint8_t)atoi(argv[3]) : 8;
        uint16_t tmp[64];
        if (n > 64) n = 64;
        uint8_t got = stepper_poll_get_recent(dev, n, tmp);
        shell_print(shell, "recent[%u]: %u samples", dev, got);
        for (uint8_t i = 0; i < got; i++) {
            shell_print(shell, "  %u: 0x%04x (%s)", i, tmp[i], l6470_decode_status(tmp[i]));
        }
        return 0;
    }
    shell_print(shell, "Unknown poll command");
    return -EINVAL;
#endif
}

static int cmd_stepper_models_list(const struct shell *shell, size_t argc, char **argv)
{
    unsigned int cnt = stepper_get_model_count();
    shell_print(shell, "Available models (%u):", cnt);
    for (unsigned int i = 0; i < cnt; i++) {
        const stepper_model_t *m = stepper_get_model(i);
        if (m) {
            shell_print(shell, "  %u: %s (steps=%d max_speed=%d mA=%d micro=%d)", i, m->name,
                        m->steps_per_rev, m->max_speed, m->max_current_ma, m->max_microstep);
        }
    }
    return 0;
}

static int cmd_stepper_model_show(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: stepper model show <axis>");
        return -EINVAL;
    }
    int axis = atoi(argv[1]);
    const stepper_model_t *m = stepper_get_active(axis);
    if (!m) {
        shell_print(shell, "No active model for axis %d", axis);
        return -ENOENT;
    }
    shell_print(shell, "Axis %d active model: %s (steps=%d max_speed=%d mA=%d micro=%d)", axis,
                m->name, m->steps_per_rev, m->max_speed, m->max_current_ma, m->max_microstep);
    return 0;
}

static int cmd_stepper_model_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_print(shell, "Usage: stepper model set <axis> <name>");
        return -EINVAL;
    }
    int axis = atoi(argv[1]);
    const char *name = argv[2];
    int r = stepper_set_active_by_name(axis, name);
    if (r == 0) {
        shell_print(shell, "Axis %d active model set to %s", axis, name);
    } else if (r == -ENOENT) {
        shell_print(shell, "Model '%s' not found", name);
    } else {
        shell_warn(shell, "Failed to set model: %d", r);
    }
    return r;
}

SHELL_STATIC_SUBCMD_SET_CREATE(models_cmds,
    SHELL_CMD(list, NULL, "List available stepper models", cmd_stepper_models_list),
    SHELL_CMD(show, NULL, "Show active model for axis: show <axis>", cmd_stepper_model_show),
    SHELL_CMD(set, NULL, "Set active model: set <axis> <name>", cmd_stepper_model_set),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(stepper_cmds,
    SHELL_CMD(status, NULL, "Show L6470 statuses", cmd_stepper_status),
    SHELL_CMD(reset, NULL, "Hardware reset (placeholder)", cmd_stepper_reset),
    SHELL_CMD(probe, NULL, "Probe L6470 devices", cmd_stepper_probe),
    SHELL_CMD(run, NULL, "Run motor: run <dev> <dir> <speed>", cmd_stepper_run),
    SHELL_CMD(move, NULL, "Move motor: move <dev> <dir> <steps>", cmd_stepper_move),
    SHELL_CMD(goto, NULL, "Goto absolute position: goto <dev> <abs_pos>", cmd_stepper_goto),
    SHELL_CMD(goto_dir, NULL, "Goto with direction: goto_dir <dev> <dir> <abs_pos>", cmd_stepper_goto_dir),
    SHELL_CMD(home, NULL, "Go home (ABS_POS reference)", cmd_stepper_home),
    SHELL_CMD(zero, NULL, "Reset ABS_POS to 0", cmd_stepper_zero),
    SHELL_CMD(pos, NULL, "Read ABS_POS: pos <dev>", cmd_stepper_pos),
    SHELL_CMD(stop, NULL, "Stop motion: stop <dev> [hard]", cmd_stepper_stop),
    SHELL_CMD(busy, NULL, "Read BUSY flag: busy <dev>", cmd_stepper_busy),
    SHELL_CMD(waitbusy, NULL, "Block until not busy: waitbusy <dev> [timeout_ms] [poll_ms]", cmd_stepper_waitbusy),
    SHELL_CMD(disable, NULL, "Disable outputs: disable <dev> [hard]", cmd_stepper_disable),
    SHELL_CMD(limits, NULL, "Show configured L6470 safety limits", cmd_stepper_limits),
    SHELL_CMD(power, NULL, "Power control: power on|off|status", cmd_stepper_power),
    SHELL_CMD(poll, NULL, "Status poll: enable|disable|dump <dev> [n]", cmd_stepper_poll),
    SHELL_CMD(model, &models_cmds, "Stepper model commands (list/show/set)", NULL),
#if defined(CONFIG_SETTINGS_FILE) && defined(CONFIG_APP_LINK_WITH_FS)
    SHELL_CMD(persist, NULL, "Persistence helpers: persist status", cmd_stepper_persist),
#endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(stepper, &stepper_cmds, "Stepper control commands", NULL);
