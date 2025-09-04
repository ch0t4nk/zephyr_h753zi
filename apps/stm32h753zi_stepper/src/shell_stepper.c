#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <stdbool.h>
#include "l6470.h"
#include "stepper_models.h"

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

static int cmd_stepper_limits(const struct shell *shell, size_t argc, char **argv)
{
    shell_print(shell, "L6470 limits:");
    shell_print(shell, "  max_speed_steps_per_sec = %u", l6470_get_max_speed_steps_per_sec());
    shell_print(shell, "  max_current_mA         = %u", l6470_get_max_current_ma());
    shell_print(shell, "  max_steps_per_command  = %u", l6470_get_max_steps_per_command());
    shell_print(shell, "  max_microstep          = %u", l6470_get_max_microstep());
    return 0;
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
    SHELL_CMD(disable, NULL, "Disable outputs: disable <dev> [hard]", cmd_stepper_disable),
    SHELL_CMD(limits, NULL, "Show configured L6470 safety limits", cmd_stepper_limits),
    SHELL_CMD(power, NULL, "Power control: power on|off|status", cmd_stepper_power),
    SHELL_CMD(model, &models_cmds, "Stepper model commands (list/show/set)", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(stepper, &stepper_cmds, "Stepper control commands", NULL);
