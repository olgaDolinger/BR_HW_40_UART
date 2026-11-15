#include "driver/uart.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    17
#define UART_RX_PIN    18
#define UART_BAUD      115200
#define UART_BUF_SIZE  256

static const char *TAG = "UART_LINK";

// На ESP32-S3-DevKitC-1 вбудований світлодіод — адресний RGB (WS2812):
// GPIO48 на платі v1.0, GPIO38 на v1.1
#define LED_PIN  48

static bool led_state = false;
static led_strip_handle_t led;

#define BUTTON_PIN 4

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void led_init(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = LED_PIN,
        .max_leds = 1,                       // на платі один світлодіод
    };
    led_strip_rmt_config_t rmt_cfg = {
        .resolution_hz = 10 * 1000 * 1000,   // 10 МГц
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &led));
    led_strip_clear(led);                    // початковий стан: вимкнений
}

void led_set(bool on)
{
    if (on) {
        led_strip_set_pixel(led, 0, 0, 16, 0);   // R, G, B — зелений, неяскраво
        led_strip_refresh(led);
    } else {
        led_strip_clear(led);
    }
}

void uart_link_init(void)
{

    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,     // 8 біт даних
        .parity     = UART_PARITY_DISABLE,  // "N" у 8N1 — без парності
        .stop_bits  = UART_STOP_BITS_1,     // "1" у 8N1 — один стоп-біт
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_pullup_en(UART_RX_PIN); // увімкнути підтягування для RX піна
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    ESP_LOGI(TAG, "UART1 готовий: %d 8N1 на GPIO%d(TX)/GPIO%d(RX)", UART_BAUD, UART_TX_PIN, UART_RX_PIN);
}

void app_main(void)
{
    led_init();
    button_init();
    uart_link_init();

    while (1) {

        uint8_t rx_byte;
        int len = uart_read_bytes(UART_PORT, &rx_byte, 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            ESP_LOGI(TAG, "Отримано: 0x%02X ('%c')", rx_byte, rx_byte);
            led_state = !led_state;
        }
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(30)); // антидребезг
            if (gpio_get_level(BUTTON_PIN) == 0) {
                uint8_t byte_to_send = 0x41;   // 'A' = 01000001 — той самий приклад, що й на слайдах
                uart_write_bytes(UART_PORT, (const char *)&byte_to_send, 1);
                ESP_LOGI(TAG, "Кнопка натиснута, відправлено: 0x%02X ('%c')", byte_to_send, byte_to_send);
            }
        }

        led_set(led_state);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
