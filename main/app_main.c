
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mbedtls/base64.h"

#define FLASH_LED_PIN      4
#define FLASH_LED_CHANNEL  LEDC_CHANNEL_0
#define FLASH_LED_TIMER    LEDC_TIMER_0
#define FLASH_LED_SPEED    LEDC_LOW_SPEED_MODE

static const char *TAG = "CAM_FLASH";

// Camera pin configuration for AI-Thinker ESP32-CAM
camera_config_t camera_config = {
    .pin_pwdn       = 32,
    .pin_reset      = -1,
    .pin_xclk       = 0,
    .pin_sccb_sda   = 26,
    .pin_sccb_scl   = 27,
    .pin_d7         = 35,
    .pin_d6         = 34,
    .pin_d5         = 39,
    .pin_d4         = 36,
    .pin_d3         = 21,
    .pin_d2         = 19,
    .pin_d1         = 18,
    .pin_d0         = 5,
    .pin_vsync      = 25,
    .pin_href       = 23,
    .pin_pclk       = 22,
    .xclk_freq_hz   = 10000000,
    .ledc_timer     = FLASH_LED_TIMER,
    .ledc_channel   = FLASH_LED_CHANNEL,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAMESIZE_QVGA,
    .jpeg_quality   = 10,
    .fb_count       = 1,
    .fb_location    = CAMERA_FB_IN_PSRAM,
    .grab_mode      = CAMERA_GRAB_LATEST,
};

void init_flash_led()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = FLASH_LED_SPEED,
        .timer_num        = FLASH_LED_TIMER,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = FLASH_LED_SPEED,
        .channel        = FLASH_LED_CHANNEL,
        .timer_sel      = FLASH_LED_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = FLASH_LED_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ledc_set_duty(FLASH_LED_SPEED, FLASH_LED_CHANNEL, 0);
    ledc_update_duty(FLASH_LED_SPEED, FLASH_LED_CHANNEL);
}

void set_flash_led_brightness(uint8_t brightness)
{
    ledc_set_duty(FLASH_LED_SPEED, FLASH_LED_CHANNEL, brightness);
    ledc_update_duty(FLASH_LED_SPEED, FLASH_LED_CHANNEL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_flash_led();

    ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return;
    }

    // while (1) {
        ESP_LOGI(TAG, "Reinitializing camera...");
    esp_camera_deinit();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_camera_init(&camera_config);
    sensor_t *s = esp_camera_sensor_get();
if (s != NULL) {
    s->set_whitebal(s, 1);     // Enable white balance
    s->set_awb_gain(s, 1);     // Enable AWB gain
    s->set_saturation(s, -2);  // Reduce green tint
    s->set_brightness(s, 1);   // Slight brightness boost
    s->set_contrast(s, 1);     // Improve visual sharpness
    s->set_exposure_ctrl(s, 0);   // Turn OFF auto exposure
        s->set_gain_ctrl(s, 0);       // Turn OFF auto gain
    s->set_aec_value(s, 100);     // Fixed exposure (try 500–1200)
    s->set_agc_gain(s, 8);     
}
    vTaskDelay(200 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Turning on flash LED");
        set_flash_led_brightness(60);
        vTaskDelay(200 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();
            vTaskDelay(100 / portTICK_PERIOD_MS);  // Keep flash on briefly after
        set_flash_led_brightness(0);

        if (!pic) {
            ESP_LOGE(TAG, "Camera capture failed");
            esp_camera_fb_return(pic);
        } else {
            ESP_LOGI(TAG, "Picture taken! Size: %zu bytes", pic->len);

            size_t b64_len = 0;
            esp_err_t enc_ret = mbedtls_base64_encode(NULL, 0, &b64_len, pic->buf, pic->len);
            if (enc_ret == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && b64_len > 0) {
                uint8_t *b64_buf = (uint8_t *)malloc(b64_len + 1);
                if (b64_buf) {
                    if (mbedtls_base64_encode(b64_buf, b64_len, &b64_len, pic->buf, pic->len) == 0) {
                        b64_buf[b64_len] = '\0';
                         printf("-----BEGIN IMAGE-----\n");
                        for (size_t i = 0; i < b64_len; i += 512) {
                            size_t len = (b64_len - i) > 512 ? 512 : (b64_len - i);
                            printf("%.*s", (int)len, b64_buf + i);
                            fflush(stdout);
                            vTaskDelay(10 / portTICK_PERIOD_MS);
                        }
                         printf("\n-----END IMAGE-----\n");
                    } else {
                        ESP_LOGE(TAG, "Base64 encoding failed");
                    }
                    free(b64_buf);
                } else {
                    ESP_LOGE(TAG, "Failed to allocate memory for Base64 buffer");
                }
            } else {
                ESP_LOGE(TAG, "Failed to determine Base64 buffer size");
            }

            esp_camera_fb_return(pic);
        }

        // return;
    //             vTaskDelay(30000 / portTICK_PERIOD_MS); // 60 seconds
    // }
}
