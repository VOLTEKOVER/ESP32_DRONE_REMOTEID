#include "led_status.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "LED"

static int r_pin = CONFIG_RID_LED_R_GPIO;
static int g_pin = CONFIG_RID_LED_G_GPIO;
static int b_pin = CONFIG_RID_LED_B_GPIO;

static void set_pin(int pin, bool on)
{
    if (pin < 0) return;
    gpio_set_level(pin, on ? 1 : 0);
}

static void all_off(void)
{
    set_pin(r_pin, 0);
    set_pin(g_pin, 0);
    set_pin(b_pin, 0);
}

void led_status_init(void)
{
    if (r_pin < 0 && g_pin < 0 && b_pin < 0) {
        ESP_LOGI(TAG, "No LED pins configured");
        return;
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    int pins[] = {r_pin, g_pin, b_pin};
    for (int i = 0; i < 3; i++) {
        if (pins[i] < 0) continue;
        io_conf.pin_bit_mask = (1ULL << pins[i]);
        gpio_config(&io_conf);
    }

    all_off();
    ESP_LOGI(TAG, "LED init R=%d G=%d B=%d", r_pin, g_pin, b_pin);
}

void led_status_update(bool gps_valid, bool transmitting)
{
    if (r_pin < 0 && g_pin < 0 && b_pin < 0) return;

    if (!gps_valid) {
        set_pin(r_pin, 1);
        set_pin(g_pin, 0);
        set_pin(b_pin, 0);
    } else {
        set_pin(r_pin, 0);
        set_pin(g_pin, 1);
        set_pin(b_pin, 0);
    }
}