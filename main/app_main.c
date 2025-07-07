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

    while (1) {
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
                        for (size_t i = 0; i < b64_len; i += 512) {
                            size_t len = (b64_len - i) > 512 ? 512 : (b64_len - i);
                            printf("%.*s", (int)len, b64_buf + i);
                            fflush(stdout);
                            vTaskDelay(10 / portTICK_PERIOD_MS);
                        }
                        printf("\n");
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
                vTaskDelay(30000 / portTICK_PERIOD_MS); // 60 seconds
    }
}



// /**
//  * This example takes a picture every 5s and print its size on serial monitor.
//  */

// // =============================== SETUP ======================================

// // 1. Board setup (Uncomment):
// // #define BOARD_WROVER_KIT
// #define BOARD_ESP32CAM_AITHINKER
// // #define BOARD_ESP32S3_WROOM

// /**
//  * 2. Kconfig setup
//  *
//  * If you have a Kconfig file, copy the content from
//  *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
//  * In case you haven't, copy and paste this Kconfig file inside the src directory.
//  * This Kconfig file has definitions that allows more control over the camera and
//  * how it will be initialized.
//  */

// /**
//  * 3. Enable PSRAM on sdkconfig:
//  *
//  * CONFIG_ESP32_SPIRAM_SUPPORT=y
//  *
//  * More info on
//  * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
//  */

// // ================================ CODE ======================================

// #include <esp_log.h>
// #include <esp_system.h>
// #include <nvs_flash.h>
// #include <sys/param.h>
// #include <string.h>
// #include "driver/ledc.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include <stdio.h>
// #include "mbedtls/base64.h"

// // support IDF 5.x
// #ifndef portTICK_RATE_MS
// #define portTICK_RATE_MS portTICK_PERIOD_MS
// #endif

// #include "esp_camera.h"

// // #define BOARD_WROVER_KIT 1

// // // WROVER-KIT PIN Map
// // #ifdef BOARD_WROVER_KIT

// // #define CAM_PIN_PWDN -1  //power down is not used
// // #define CAM_PIN_RESET -1 //software reset will be performed
// // #define CAM_PIN_XCLK 21
// // #define CAM_PIN_SIOD 26
// // #define CAM_PIN_SIOC 27

// // #define CAM_PIN_D7 35
// // #define CAM_PIN_D6 34
// // #define CAM_PIN_D5 39
// // #define CAM_PIN_D4 36
// // #define CAM_PIN_D3 19
// // #define CAM_PIN_D2 18
// // #define CAM_PIN_D1 5
// // #define CAM_PIN_D0 4
// // #define CAM_PIN_VSYNC 25
// // #define CAM_PIN_HREF 23
// // #define CAM_PIN_PCLK 22

// // #endif

// // ESP32Cam (AiThinker) PIN Map
// // #ifdef BOARD_ESP32CAM_AITHINKER

// #define CAM_PIN_PWDN 32
// #define CAM_PIN_RESET -1 //software reset will be performed
// #define CAM_PIN_XCLK 0
// #define CAM_PIN_SIOD 26
// #define CAM_PIN_SIOC 27

// #define CAM_PIN_D7 35
// #define CAM_PIN_D6 34
// #define CAM_PIN_D5 39
// #define CAM_PIN_D4 36
// #define CAM_PIN_D3 21
// #define CAM_PIN_D2 19
// #define CAM_PIN_D1 18
// #define CAM_PIN_D0 5
// #define CAM_PIN_VSYNC 25
// #define CAM_PIN_HREF 23
// #define CAM_PIN_PCLK 22

// // #endif
// // ESP32S3 (WROOM) PIN Map
// // #ifdef BOARD_ESP32S3_WROOM
// // #define CAM_PIN_PWDN 38
// // #define CAM_PIN_RESET -1   //software reset will be performed
// // #define CAM_PIN_VSYNC 6
// // #define CAM_PIN_HREF 7
// // #define CAM_PIN_PCLK 13
// // #define CAM_PIN_XCLK 15
// // #define CAM_PIN_SIOD 4
// // #define CAM_PIN_SIOC 5
// // #define CAM_PIN_D0 11
// // #define CAM_PIN_D1 9
// // #define CAM_PIN_D2 8
// // #define CAM_PIN_D3 10
// // #define CAM_PIN_D4 12
// // #define CAM_PIN_D5 18
// // #define CAM_PIN_D6 17
// // #define CAM_PIN_D7 16
// // #endif
// static const char *TAG = "example:take_picture";

// #if ESP_CAMERA_SUPPORTED
// static camera_config_t camera_config = {
//     .pin_pwdn = CAM_PIN_PWDN,
//     .pin_reset = CAM_PIN_RESET,
//     .pin_xclk = CAM_PIN_XCLK,
//     .pin_sccb_sda = CAM_PIN_SIOD,
//     .pin_sccb_scl = CAM_PIN_SIOC,

//     .pin_d7 = CAM_PIN_D7,
//     .pin_d6 = CAM_PIN_D6,
//     .pin_d5 = CAM_PIN_D5,
//     .pin_d4 = CAM_PIN_D4,
//     .pin_d3 = CAM_PIN_D3,
//     .pin_d2 = CAM_PIN_D2,
//     .pin_d1 = CAM_PIN_D1,
//     .pin_d0 = CAM_PIN_D0,
//     .pin_vsync = CAM_PIN_VSYNC,
//     .pin_href = CAM_PIN_HREF,
//     .pin_pclk = CAM_PIN_PCLK,

//     //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
//     .xclk_freq_hz = 20000000,
//     .ledc_timer = LEDC_TIMER_0,
//     .ledc_channel = LEDC_CHANNEL_0,

//     .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
//     .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

//     .jpeg_quality = 10, //0-63, for OV series camera sensors, lower number means higher quality
//     .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
//     .fb_location = CAMERA_FB_IN_PSRAM,
//     .grab_mode = CAMERA_GRAB_LATEST,
// };

// static esp_err_t init_camera(void)
// {
//     //initialize the camera
//     esp_err_t err = esp_camera_init(&camera_config);
    
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Camera Init Failed");
//         return err;
//     }

//     return ESP_OK;
// }
// #endif

// void init_flash_led()
// {
//     // Prepare and then apply the LEDC PWM timer config
//     ledc_timer_config_t ledc_timer = {
//         .speed_mode       = LEDC_LOW_SPEED_MODE,
//         .timer_num        = LEDC_TIMER_0,
//         .duty_resolution  = LEDC_TIMER_8_BIT,
//         .freq_hz          = 5000,  // 5 kHz PWM frequency
//         .clk_cfg          = LEDC_AUTO_CLK
//     };
//     ledc_timer_config(&ledc_timer);

//     // Configure the LEDC channel
//     ledc_channel_config_t ledc_channel = {
//         .speed_mode     = LEDC_LOW_SPEED_MODE,
//         .channel        = LEDC_CHANNEL_0,
//         // .timer_sel      = LEDC_TIMER_0,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = 4,  // Your LED flash pin
//         .duty           = 0,  // Start with LED off
//         .hpoint         = 0
//     };
//     ledc_channel_config(&ledc_channel);
// }


// void app_main(void) 
// {

//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);
// #if ESP_CAMERA_SUPPORTED
//     if(ESP_OK != init_camera()) {
//         return;
//     }

//     init_flash_led();
//     while (1)
//     {

// // Turn on the LED at max brightness
// ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, );
// vTaskDelay(100);
// ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
//         ESP_LOGI(TAG, "Taking picture...");
//         camera_fb_t *pic = esp_camera_fb_get();
//         if (!pic) {
//             ESP_LOGE(TAG, "Camera capture failed");
//             return;
//         }

//         // use pic->buf to access the image
//         // ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
//         // esp_camera_fb_return(pic);
//         size_t b64_len = 0;
//     esp_err_t enc_ret = mbedtls_base64_encode(NULL, 0, &b64_len, pic->buf, pic->len);
//     if (enc_ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
//         ESP_LOGE(TAG, "Failed to get Base64 buffer size");
//         esp_camera_fb_return(pic);
//         return;
//     }

//     uint8_t *b64_buf = (uint8_t *)malloc(b64_len + 1);
//     if (!b64_buf) {
//         ESP_LOGE(TAG, "Failed to allocate memory for Base64");
//         esp_camera_fb_return(pic);
//         return;
//     }

//     if (mbedtls_base64_encode(b64_buf, b64_len, &b64_len, pic->buf, pic->len) != 0) {
//         ESP_LOGE(TAG, "Base64 encoding failed");
//         free(b64_buf);
//         esp_camera_fb_return(pic);
//         return;
//     }

//     b64_buf[b64_len] = 0; // Null-terminate
//     // ESP_LOGI(TAG, "Image Base64 (%zu bytes):\n%s", b64_len, b64_buf);
//     for (size_t i = 0; i < b64_len; i += 512) {
//         size_t len = (b64_len - i) > 512 ? 512 : (b64_len - i);
//         printf("%.*s", (int)len, b64_buf + i);
//   // Avoids formatting overhead of ESP_LOGI
//         fflush(stdout);
//         vTaskDelay(10 / portTICK_PERIOD_MS); // Let watchdog breathe
//     }
//     printf("\n");
//     // Cleanup
//     free(b64_buf);
//     esp_camera_fb_return(pic);

//         vTaskDelay(1000000000);
//     }
// #else
//     ESP_LOGE(TAG, "Camera support is not available for this chip");
//     return;
// #endif
// }
