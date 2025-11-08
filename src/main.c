#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_tx_button, LOG_LEVEL_INF);

// --- UART0 ---
#define UART_NODE DT_NODELABEL(uart0)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

// --- LED Verde ---
#define LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

// --- Botão em PTA16 ---
#define PORTA_NODE DT_NODELABEL(gpioa)
static const struct gpio_dt_spec button = {
    .port = DEVICE_DT_GET(PORTA_NODE),
    .pin = 16,
    .dt_flags = GPIO_ACTIVE_LOW,
};
static struct gpio_callback button_cb_data;

// --- Estado ---
volatile bool transmitindo = false;  // começa parado

// --- ISR do botão ---
void button_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t last_press = 0;
    int64_t now = k_uptime_get();

    if (now - last_press < 200) return; // debounce de 200 ms
    last_press = now;

    transmitindo = !transmitindo;  // alterna estado

    if (transmitindo) {
        LOG_INF("🟢 Transmissão INICIADA");
    } else {
        LOG_INF("⛔ Transmissão PAUSADA");
    }
}

void main(void)
{
    LOG_INF("Iniciando transmissor UART com controle por botão...");

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART não está pronta");
        return;
    }
    if (!gpio_is_ready_dt(&led) || !device_is_ready(button.port)) {
        LOG_ERR("GPIOs não estão prontos");
        return;
    }

    // Configura LED
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    // Configura botão
    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb_data, button_pressed_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    LOG_INF("Sistema pronto! Pressione o botão (PTA16) para iniciar/parar transmissão.");

    uint8_t byte = 0;

    while (1) {
        if (transmitindo) {
            uart_poll_out(uart_dev, byte++);  // envia byte
            gpio_pin_set_dt(&led, 1);         // pisca verde
            k_msleep(50);
            gpio_pin_set_dt(&led, 0);
            k_msleep(450);
        } else {
            k_msleep(100);  // modo pausado, dorme um pouco
        }
    }
}
