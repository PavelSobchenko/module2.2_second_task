#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BTN_ON 0
#define BTN_OFF 1

constexpr uint8_t LED_PIN = 4;
constexpr uint8_t DOWN_BTN_PIN = 5;
constexpr uint8_t UP_BTN_PIN = 6;

constexpr uint8_t BTN_THROTTLE_MS = 200;
constexpr uint16_t PERIOD_MC = 10000;

constexpr uint8_t DUTY_MIN = 0;
constexpr uint8_t DUTY_MAX = 100;

typedef enum { LED_STATE_OFF, LED_STATE_ON } led_state_t;

void init() {
    gpio_config_t out_conf = {};
    out_conf.intr_type = GPIO_INTR_DISABLE;
    out_conf.mode = GPIO_MODE_OUTPUT;
    out_conf.pin_bit_mask = ((1ULL << LED_PIN));
    out_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&out_conf);

    gpio_config_t in_conf = {};
    in_conf.intr_type = GPIO_INTR_DISABLE;
    in_conf.pin_bit_mask = ((1ULL << DOWN_BTN_PIN) | (1ULL << UP_BTN_PIN));
    in_conf.mode = GPIO_MODE_INPUT;
    in_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&in_conf);
}

volatile uint64_t timeOnMc = 0;
volatile uint64_t timeOffMc = PERIOD_MC;

void changeDuty(int8_t *duty, int8_t delta) {
    *duty += delta;
    if (*duty < DUTY_MIN) *duty = DUTY_MIN;
    if (*duty > DUTY_MAX) *duty = DUTY_MAX;

    timeOnMc = (uint64_t)PERIOD_MC * *duty / DUTY_MAX;
    timeOffMc = PERIOD_MC - timeOnMc;

    printf("duty: %d, timeOn: %" PRIu64 ", timeOff: %" PRIu64 "\n",
       *duty, timeOnMc, timeOffMc);
}

void app_main() {
    init();

    int8_t duty = 0;
    uint64_t downBtnPressTimestampMs = 0;
    uint64_t upBtnPressTimestampMs = 0;
    led_state_t ledState = LED_STATE_OFF;
    uint64_t ledSwitchTimestampMc = 0;

    while(1) {
        uint64_t nowMc = esp_timer_get_time();
        uint64_t nowMs = nowMc / 1000;

        // printf("down btn: %d, up btn: %d\n", gpio_get_level(DOWN_BTN_PIN), gpio_get_level(UP_BTN_PIN));

        if (gpio_get_level(DOWN_BTN_PIN) == BTN_ON && nowMs - downBtnPressTimestampMs >= BTN_THROTTLE_MS) {
            downBtnPressTimestampMs = nowMs;
            changeDuty(&duty, -10);
        }

        if (gpio_get_level(UP_BTN_PIN) == BTN_ON && nowMs - upBtnPressTimestampMs >= BTN_THROTTLE_MS) {
            upBtnPressTimestampMs = nowMs;
            changeDuty(&duty, 10);
        }
        
        if (ledState == LED_STATE_ON && nowMc - ledSwitchTimestampMc >= timeOnMc) {
            ledState = LED_STATE_OFF;
            ledSwitchTimestampMc = nowMc;
            gpio_set_level(LED_PIN, 0);
        } else if (ledState == LED_STATE_OFF && nowMc - ledSwitchTimestampMc >= timeOffMc) {
            ledState = LED_STATE_ON;
            ledSwitchTimestampMc = nowMc;
            gpio_set_level(LED_PIN, 1);
        }
    }
}