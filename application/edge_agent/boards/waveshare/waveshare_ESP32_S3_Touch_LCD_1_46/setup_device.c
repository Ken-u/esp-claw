/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_spd2010.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_spd2010.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "waveshare_lcd_1_46";

/* TCA9554 EXIO mapping from Waveshare wiki / demo:
 *   EXIO1 (pin 0) = TP_RST
 *   EXIO2 (pin 1) = LCD_RST
 *   EXIO3 (pin 2) = SD_CS
 */
#define WS_EXIO_TP_RST   IO_EXPANDER_PIN_NUM_0
#define WS_EXIO_LCD_RST  IO_EXPANDER_PIN_NUM_1
#define WS_EXIO_SD_CS    IO_EXPANDER_PIN_NUM_2
#define WS_EXIO_RESET_MASK (WS_EXIO_TP_RST | WS_EXIO_LCD_RST)

static const spd2010_vendor_config_t s_lcd_vendor_config = {
    .flags = {
        .use_qspi_interface = 1,
    },
};

esp_err_t io_expander_factory_entry_t(i2c_master_bus_handle_t i2c_handle,
                                      const uint16_t dev_addr,
                                      esp_io_expander_handle_t *handle_ret)
{
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_handle, dev_addr, handle_ret);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TCA9554 IO expander: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Match Waveshare demo power-up/reset: pulse TP_RST + LCD_RST low, then high.
     * Leave SD_CS high so the card stays deselected until SDMMC mounts.
     */
    ret = esp_io_expander_set_dir(*handle_ret, WS_EXIO_RESET_MASK | WS_EXIO_SD_CS, IO_EXPANDER_OUTPUT);
    if (ret == ESP_OK) {
        ret = esp_io_expander_set_level(*handle_ret, WS_EXIO_SD_CS, 1);
    }
    if (ret == ESP_OK) {
        ret = esp_io_expander_set_level(*handle_ret, WS_EXIO_RESET_MASK, 0);
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = esp_io_expander_set_level(*handle_ret, WS_EXIO_RESET_MASK, 1);
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IO expander reset sequence failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(panel_dev_cfg));
    panel_dev_cfg.vendor_config = (void *)&s_lcd_vendor_config;

    esp_err_t ret = esp_lcd_new_panel_spd2010(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SPD2010 panel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_touch_config_t *touch_dev_config,
                                    esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(touch_cfg));

    touch_cfg.rst_gpio_num = GPIO_NUM_NC;
    touch_cfg.int_gpio_num = GPIO_NUM_4;
    touch_cfg.levels.reset = 0;
    touch_cfg.levels.interrupt = 0;
    touch_cfg.flags.swap_xy = 0;
    touch_cfg.flags.mirror_x = 0;
    touch_cfg.flags.mirror_y = 0;

    esp_err_t ret = esp_lcd_touch_new_i2c_spd2010(io, &touch_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SPD2010 touch: %s", esp_err_to_name(ret));
    }
    return ret;
}
