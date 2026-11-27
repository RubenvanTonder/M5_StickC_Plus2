#pragma once

#include "driver/gpio.h"

/* Power management */
#define BOARD_POWER_HOLD_GPIO   GPIO_NUM_4   /* Drive HIGH to stay powered on battery */
#define BOARD_BAT_ADC_GPIO      GPIO_NUM_38  /* Battery voltage ADC input */

/* ST7789V2 LCD (135x240) - kept disabled in headless template */
#define BOARD_LCD_BL_GPIO       GPIO_NUM_27  /* Backlight control (LOW = off) */
#define BOARD_LCD_RST_GPIO      GPIO_NUM_12  /* Reset (active LOW) */
#define BOARD_LCD_CS_GPIO       GPIO_NUM_5
#define BOARD_LCD_DC_GPIO       GPIO_NUM_14
#define BOARD_LCD_CLK_GPIO      GPIO_NUM_13
#define BOARD_LCD_MOSI_GPIO     GPIO_NUM_15

/* On-board user I/O */
#define BOARD_BTN_A_GPIO        GPIO_NUM_37  /* Front button A (input-only, external pull-up) */
#define BOARD_BTN_B_GPIO        GPIO_NUM_39  /* Front button B (input-only, external pull-up) */
#define BOARD_BTN_PWR_GPIO      GPIO_NUM_35  /* Side power button (input-only) */
#define BOARD_LED_IR_GPIO       GPIO_NUM_19  /* Shared red LED / IR transmitter */
#define BOARD_BUZZER_GPIO       GPIO_NUM_2   /* Passive buzzer */

/* Internal & Grove I2C (IMU MPU6886 @ 0x68, RTC BM8563 @ 0x51) */
#define BOARD_I2C_SDA_GPIO      GPIO_NUM_21
#define BOARD_I2C_SCL_GPIO      GPIO_NUM_22
