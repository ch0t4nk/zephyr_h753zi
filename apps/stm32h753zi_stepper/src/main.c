#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/version.h>
#include <zephyr/drivers/spi.h>
#include "l6470.h"
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define TCP_PORT 4242
#define STACK_SIZE 2048
#define BUF_SIZE 128

/* SPI frequency for L6470: choose a value supported by STM32H7 SPI and
 * within L6470 datasheet limits. The shield overlay sets a higher
 * nominal `spi-max-frequency` (20 MHz) to exercise controller limits, but
 * the application uses a conservative 5 MHz runtime configuration to
 * ensure reliable operation across voltage and cable conditions.
 *
 * References:
 *  - L6470 datasheet: maximum recommended SPI clock for reliable
 *    communications (check "Digital interface" section). Typical
 *    implementations use 5..10 MHz for stable operation.
 *  - STM32H7 reference manual / datasheet: SPI peripheral timings and
 *    supported max frequency depend on APB prescalers and core clocks.
 *
 * If you need higher throughput, validate with the shield schematic
 * and scope the MOSI/MISO lines at the target SPI speed. When raising the
 * speed, ensure the SPI controller's `spi-max-frequency` in the overlay
 * matches the runtime `.frequency` in `spi_config`.
 */
#define L6470_SPI_FREQ_HZ 5000000U

// LD1 (Green LED) - typically aliased as led0 in the device tree overlay
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* IHM02A1 control pins (Arduino D2/D3/D4 mapped on this board):
 *  - FLAG:  PG14, active low, input
 *  - BUSY:  PE13, active high, input
 *  - RESET: PE14, active low, output
 */
#define IHM_FLAG_PORT_NODE   DT_NODELABEL(gpiog)
#define IHM_FLAG_PIN         14
#define IHM_BUSY_PORT_NODE   DT_NODELABEL(gpioe)
#define IHM_BUSY_PIN         13
#define IHM_RESET_PORT_NODE  DT_NODELABEL(gpioe)
#define IHM_RESET_PIN        14

void heartbeat_thread(void)
{
    static uint32_t heartbeat_count = 0;
    
    if (!device_is_ready(led.port)) {
        printk("ERROR: LED device not ready\n");
        LOG_ERR("LED device not ready");
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    
    printk("=== LED Heartbeat thread started ===\n");
    LOG_INF("LED heartbeat started");

    while (1) {
        gpio_pin_toggle_dt(&led);
        heartbeat_count++;
        
        // Print heartbeat status every 10 seconds (20 toggles)
        if (heartbeat_count % 20 == 0) {
            printk("Heartbeat: %u (LED toggled %u times)\n", 
                   heartbeat_count / 2, heartbeat_count);
            LOG_DBG("Heartbeat count: %u", heartbeat_count);
        }
        
        k_sleep(K_MSEC(500));
    }
}

K_THREAD_DEFINE(heartbeat_tid, 512, heartbeat_thread, NULL, NULL, NULL, 7, 0, 0);

/* Status poll thread: polls both L6470 devices every 2 seconds and logs decoded status */
void status_poll_thread(void)
{
    uint16_t statuses[L6470_DAISY_SIZE];
    while (1) {
    if (l6470_is_ready() && l6470_get_status_all(statuses) == 0) {
            char s0[64];
            char s1[64];
            strncpy(s0, l6470_decode_status(statuses[0]), sizeof(s0));
            s0[sizeof(s0)-1] = '\0';
            strncpy(s1, l6470_decode_status(statuses[1]), sizeof(s1));
            s1[sizeof(s1)-1] = '\0';
            printk("[L6470 Poll] dev0=%s dev1=%s\n", s0, s1);
            LOG_INF("L6470 poll: dev0=0x%04x dev1=0x%04x", statuses[0], statuses[1]);
        } else {
            LOG_WRN("L6470 poll failed");
        }
        k_sleep(K_SECONDS(2));
    }
}

K_THREAD_DEFINE(status_poll_tid, 768, status_poll_thread, NULL, NULL, NULL, 6, 0, 0);

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback link_cb;

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        char buf[NET_IPV4_ADDR_LEN];
        struct in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_DHCP);
        
        if (addr) {
            net_addr_ntop(AF_INET, addr, buf, sizeof(buf));
            printk("*** Network interface UP - IPv4 address: %s ***\n", buf);
            LOG_INF("IPv4 address assigned: %s", buf);
        }
    } else if (mgmt_event == NET_EVENT_IPV4_ADDR_DEL) {
        printk("*** Network interface DOWN - IPv4 address removed ***\n");
        LOG_INF("IPv4 address removed");
    }
}

static void link_event_handler(struct net_mgmt_event_callback *cb,
                               uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IF_UP) {
        printk("*** Interface UP (carrier present) ***\n");
        LOG_INF("Interface up");
    } else if (mgmt_event == NET_EVENT_IF_DOWN) {
        printk("*** Interface DOWN (carrier lost) ***\n");
        LOG_WRN("Interface down");
    }
}

static void wait_for_network(void)
{
    struct net_if *iface;
    int timeout = 30; // 30 seconds timeout
    
    printk("=== Initializing Network Interface ===\n");
    
    iface = net_if_get_default();
    if (!iface) {
        printk("ERROR: No network interface available\n");
        LOG_ERR("No network interface available");
        return;
    }
    
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL);
    net_mgmt_add_event_callback(&mgmt_cb);

    /* Monitor interface up/down so we know if the link is up */
    net_mgmt_init_event_callback(&link_cb, link_event_handler,
                                 NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
    net_mgmt_add_event_callback(&link_cb);
    
    /* Print interface MAC and link state */
    uint8_t mac[6];
    const struct net_linkaddr *ll = net_if_get_link_addr(iface);
    if (ll && ll->len >= 6U) {
        memcpy(mac, ll->addr, 6U);
        printk("Interface MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    bool link_up = net_if_is_carrier_ok(iface);
    printk("Ethernet link state: %s\n", link_up ? "UP" : "DOWN");

    /* Make sure the interface is up before starting DHCP */
    if (!net_if_is_up(iface)) {
        printk("Bringing interface up...\n");
        net_if_up(iface);
    }

    printk("Starting DHCP client...\n");
    net_dhcpv4_start(iface);
    
    printk("Waiting for IP address assignment (timeout: %d seconds)...\n", timeout);
    
    while (timeout > 0 && !net_if_ipv4_get_global_addr(iface, NET_ADDR_DHCP)) {
        k_sleep(K_MSEC(1000));
        timeout--;
        if (timeout % 5 == 0) {
            printk("Still waiting for IP address... (%d seconds remaining)\n", timeout);
        }
    }
    
    if (timeout <= 0) {
        printk("WARNING: DHCP timeout - continuing without IP address\n");
        LOG_WRN("DHCP timeout");
    }
}

void main(void)
{
    int ret, server_fd, client_fd;
    struct sockaddr_in addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char rx_buf[BUF_SIZE];

    printk("\n=== STM32H753ZI Zephyr Application Starting ===\n");
    printk("Features: TCP Echo Server, LED Heartbeat, VCP Serial\n");
    printk("Board: Nucleo-H753ZI, Zephyr RTOS\n");
    
    LOG_INF("Application startup complete");

    /* Configure control pins: reset as output, busy/flag as inputs */
    const struct device *gpio_g = DEVICE_DT_GET(IHM_FLAG_PORT_NODE);
    const struct device *gpio_e = DEVICE_DT_GET(IHM_BUSY_PORT_NODE);
    if (device_is_ready(gpio_e)) {
        gpio_pin_configure(gpio_e, IHM_RESET_PIN, GPIO_OUTPUT_ACTIVE);
        /* Deassert reset after short delay (active low) */
        k_sleep(K_MSEC(5));
        gpio_pin_set(gpio_e, IHM_RESET_PIN, 1);
        gpio_pin_configure(gpio_e, IHM_BUSY_PIN, GPIO_INPUT);
    }
    if (device_is_ready(gpio_g)) {
        gpio_pin_configure(gpio_g, IHM_FLAG_PIN, GPIO_INPUT);
    }
    
    /* Configure SPI for L6470 daisy-chain */
    const struct device *spi_ctlr = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(spi_ctlr)) {
        LOG_WRN("SPI controller not ready (spi1)");
    } else {
        struct spi_cs_control cs_ctrl = SPI_CS_CONTROL_INIT(DT_NODELABEL(spidev), 2);
        struct spi_config cfg = {
            .frequency = L6470_SPI_FREQ_HZ,
            /* L6470 (dSPIN) uses SPI mode 3 (CPOL=1, CPHA=1) */
            .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |
                         SPI_MODE_CPOL | SPI_MODE_CPHA |
                         SPI_WORD_SET(8),
            .slave = 0,
            .cs = cs_ctrl,
        };
        /* Initialize L6470 helper for daisy-chain (2 devices) */
        ret = l6470_init(spi_ctlr, &cfg);
        if (ret == 0) {
            /* Issue a proper reset pulse (active-low) to both drivers */
            (void)l6470_reset_pulse();
            k_sleep(K_MSEC(5));
            uint16_t statuses[2] = {0};
            int sret = l6470_get_status_all(statuses);
            if (sret == 0) {
                printk("L6470 statuses: dev0=0x%04x dev1=0x%04x\n", statuses[0], statuses[1]);
                LOG_INF("L6470 statuses: dev0=0x%04x dev1=0x%04x", statuses[0], statuses[1]);
            } else {
                LOG_WRN("Failed to get daisy-chain statuses: %d", sret);
            }
        } else {
            LOG_WRN("l6470_init failed: %d", ret);
        }
    }

    /* Status poll thread is defined at file scope */

    // Wait for network to be ready
    wait_for_network();
    
    printk("\n=== Starting TCP Echo Server on port %d ===\n", TCP_PORT);
    LOG_INF("TCP echo server starting on port %d", TCP_PORT);

    server_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        printk("ERROR: Failed to create socket: %d\n", server_fd);
        LOG_ERR("Failed to create socket: %d", server_fd);
        return;
    }
    printk("Socket created successfully (fd: %d)\n", server_fd);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = zsock_bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        printk("ERROR: Bind failed: %d\n", ret);
        LOG_ERR("Bind failed: %d", ret);
        zsock_close(server_fd);
        return;
    }
    printk("Socket bound to port %d\n", TCP_PORT);

    ret = zsock_listen(server_fd, 1);
    if (ret < 0) {
        printk("ERROR: Listen failed: %d\n", ret);
        LOG_ERR("Listen failed: %d", ret);
        zsock_close(server_fd);
        return;
    }
    printk("Server listening for connections...\n");
    LOG_INF("TCP server ready - waiting for clients");

    while (1) {
        printk("\n--- Waiting for client connection ---\n");
        client_fd = zsock_accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            printk("ERROR: Accept failed: %d\n", client_fd);
            LOG_ERR("Accept failed: %d", client_fd);
            continue;
        }
        
        char client_ip[NET_IPV4_ADDR_LEN];
        net_addr_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printk("*** CLIENT CONNECTED from %s:%d ***\n", 
               client_ip, ntohs(client_addr.sin_port));
        LOG_INF("Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));

        while (1) {
            ret = zsock_recv(client_fd, rx_buf, sizeof(rx_buf), 0);
            if (ret < 0) {
                printk("ERROR: Recv failed: %d\n", ret);
                LOG_ERR("Recv failed: %d", ret);
                break;
            } else if (ret == 0) {
                printk("*** CLIENT DISCONNECTED ***\n");
                LOG_INF("Client disconnected");
                break;
            }
            
            rx_buf[ret] = '\0'; // Null terminate for safe printing
            printk("RX[%d bytes]: %s", ret, rx_buf);
            LOG_DBG("Received %d bytes: %s", ret, rx_buf);

            int sent = zsock_send(client_fd, rx_buf, ret, 0);
            if (sent < 0) {
                printk("ERROR: Send failed: %d\n", sent);
                LOG_ERR("Send failed: %d", sent);
                break;
            }
            printk("TX[%d bytes]: Echo sent\n", sent);
            __ASSERT(sent == ret, "Partial send");
        }
        zsock_close(client_fd);
    }
    zsock_close(server_fd);
}
