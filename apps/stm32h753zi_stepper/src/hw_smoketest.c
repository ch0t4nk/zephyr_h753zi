/* Hardware smoke-test CLI: safe VMOT toggle and status via Zephyr shell
 * Commands:
 *   vmot on
 *   vmot off
 *   vmot toggle
 *   vmot status
 */
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(hw_smoke, LOG_LEVEL_INF);

#define IHM_VMOT_NODE DT_NODELABEL(ihm02a1_vmot_switch)
#if DT_NODE_EXISTS(IHM_VMOT_NODE)
static const struct gpio_dt_spec vmot_spec = GPIO_DT_SPEC_GET(IHM_VMOT_NODE, gpios);
#else
static const struct gpio_dt_spec vmot_spec = { .port = NULL };
#endif

static int cmd_vmot(const struct shell *sh, size_t argc, char **argv)
{
    if (vmot_spec.port == NULL) {
        shell_print(sh, "VMOT control not available in device-tree");
        return -ENOTSUP;
    }
    if (!device_is_ready(vmot_spec.port)) {
        shell_print(sh, "VMOT GPIO not ready");
        return -ENODEV;
    }

    if (argc < 2) {
        shell_print(sh, "usage: vmot <on|off|toggle|status>");
        return -EINVAL;
    }

    if (strcmp(argv[1], "on") == 0) {
        gpio_pin_configure_dt(&vmot_spec, GPIO_OUTPUT_ACTIVE);
        gpio_pin_set_dt(&vmot_spec, 1);
        shell_print(sh, "VMOT enabled");
        return 0;
    } else if (strcmp(argv[1], "off") == 0) {
        gpio_pin_configure_dt(&vmot_spec, GPIO_OUTPUT_ACTIVE);
        gpio_pin_set_dt(&vmot_spec, 0);
        shell_print(sh, "VMOT disabled");
        return 0;
    } else if (strcmp(argv[1], "toggle") == 0) {
        static bool state = false;
        gpio_pin_configure_dt(&vmot_spec, GPIO_OUTPUT_ACTIVE);
        state = !state;
        gpio_pin_set_dt(&vmot_spec, state ? 1 : 0);
        shell_print(sh, "VMOT toggled -> %s", state ? "ON" : "OFF");
        return 0;
    } else if (strcmp(argv[1], "status") == 0) {
        int v = gpio_pin_get_dt(&vmot_spec);
        if (v < 0) {
            shell_print(sh, "VMOT read failed: %d", v);
            return v;
        }
        shell_print(sh, "VMOT is %s", v ? "ON" : "OFF");
        return 0;
    }

    shell_print(sh, "Unknown vmot command: %s", argv[1]);
    return -EINVAL;
}

SHELL_CMD_REGISTER(vmot, NULL, "VMOT control (on|off|toggle|status)", cmd_vmot);
