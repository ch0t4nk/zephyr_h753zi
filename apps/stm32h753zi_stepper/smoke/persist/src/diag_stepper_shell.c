#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <stdlib.h>
#include <string.h>
#include "l6470.h"

LOG_MODULE_REGISTER(diag_stepper, LOG_LEVEL_INF);

/* Lazy one-time SPI init guard; default off (no motion). */
static bool spi_inited;

static int stepper_try_init_spi(void)
{
    if (spi_inited && l6470_is_ready()) return 0;

    const struct device *spi_ctlr = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(spi_ctlr)) {
        LOG_WRN("SPI controller not ready (spi1)");
        return -ENODEV;
    }
    /* Use CS on spidev node if present; fall back to no-CS (external). */
    struct spi_cs_control cs = {0};
#if DT_NODE_EXISTS(DT_NODELABEL(spidev)) && DT_NODE_HAS_PROP(DT_NODELABEL(spidev), cs_gpios)
    cs = SPI_CS_CONTROL_INIT(DT_NODELABEL(spidev), 2);
#endif
    struct spi_config cfg = {
        .frequency = 1000000U,
        .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8),
        .slave = 0,
        .cs = cs,
    };
    int r = l6470_init(spi_ctlr, &cfg);
    if (r == 0) {
        spi_inited = true;
    }
    return r;
}

static int cmd_diag_init(const struct shell *shell, size_t argc, char **argv)
{
    int r = stepper_try_init_spi();
    if (r == 0) shell_print(shell, "L6470 SPI initialized");
    else shell_warn(shell, "SPI init failed: %d", r);
    return r;
}

static int cmd_diag_reset(const struct shell *shell, size_t argc, char **argv)
{
    int r = stepper_try_init_spi();
    if (r) return r;
    r = l6470_reset_pulse();
    if (r) shell_warn(shell, "reset failed: %d", r);
    else shell_print(shell, "reset pulse sent");
    return r;
}

static int cmd_diag_status(const struct shell *shell, size_t argc, char **argv)
{
    int r = stepper_try_init_spi();
    if (r) return r;
    uint16_t st[L6470_DAISY_SIZE] = {0};
    r = l6470_get_status_all(st);
    if (r) {
        shell_warn(shell, "get_status failed: %d", r);
        return r;
    }
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        char b0[128], b1[128];
        l6470_decode_status_verbose(st[0], b0, sizeof(b0));
        l6470_decode_status_verbose(st[1], b1, sizeof(b1));
        shell_print(shell, "dev0=%s; dev1=%s", b0, b1);
    } else {
        shell_print(shell, "dev0=0x%04x (%s), dev1=0x%04x (%s)",
                    st[0], l6470_decode_status(st[0]), st[1], l6470_decode_status(st[1]));
    }
    return 0;
}

static int cmd_diag_power(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "Usage: diag power on|off|status");
        return -EINVAL;
    }
    if (strcmp(argv[1], "on") == 0) {
        int r = l6470_power_enable();
        if (r) shell_warn(shell, "power on failed: %d", r);
        else shell_print(shell, "power enabled");
        return r;
    } else if (strcmp(argv[1], "off") == 0) {
        int r = l6470_power_disable();
        if (r) shell_warn(shell, "power off failed: %d", r);
        else shell_print(shell, "power disabled");
        return r;
    } else if (strcmp(argv[1], "status") == 0) {
        int s = l6470_power_status();
        if (s < 0) {
            shell_warn(shell, "power status error: %d", s);
            return s;
        }
        shell_print(shell, "power: %s", s ? "ENABLED" : "DISABLED");
        return 0;
    }
    shell_print(shell, "Unknown power command");
    return -EINVAL;
}

SHELL_STATIC_SUBCMD_SET_CREATE(diag_cmds,
    SHELL_CMD(init, NULL, "Init L6470 SPI", cmd_diag_init),
    SHELL_CMD(reset, NULL, "Hardware reset pulse", cmd_diag_reset),
    SHELL_CMD(status, NULL, "Get L6470 status [-v]", cmd_diag_status),
    SHELL_CMD(power, NULL, "Power control on|off|status", cmd_diag_power),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(diag, &diag_cmds, "IHM02A1 diagnostics (no motion)", NULL);
