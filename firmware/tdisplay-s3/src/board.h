#pragma once

// LilyGO T-Display-S3 Touch pin map (from LilyGO's pin_config.h).

#define PIN_POWER_ON 15 // must be HIGH to power the LCD when running on battery
#define PIN_BAT_VOLT 4  // battery voltage through a 1:2 divider
#define PIN_BUTTON_BOOT 0
#define PIN_BUTTON_KEY 14

#define PIN_LCD_BL 38
#define PIN_LCD_RST 5
#define PIN_LCD_CS 6
#define PIN_LCD_DC 7
#define PIN_LCD_WR 8
#define PIN_LCD_RD 9
#define PIN_LCD_D0 39
#define PIN_LCD_D1 40
#define PIN_LCD_D2 41
#define PIN_LCD_D3 42
#define PIN_LCD_D4 45
#define PIN_LCD_D5 46
#define PIN_LCD_D6 47
#define PIN_LCD_D7 48

#define PIN_TOUCH_SDA 18
#define PIN_TOUCH_SCL 17
#define PIN_TOUCH_INT 16
#define PIN_TOUCH_RST 21
