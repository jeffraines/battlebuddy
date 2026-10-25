#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "board.h"

// ST7789 on an 8-bit parallel bus, plus CST816 touch over I2C.
class Display : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_Parallel8 bus_;
    lgfx::Light_PWM light_;
    lgfx::Touch_CST816S touch_;

public:
    Display()
    {
        {
            auto cfg = bus_.config();
            cfg.freq_write = 20000000;
            cfg.pin_wr = PIN_LCD_WR;
            cfg.pin_rd = PIN_LCD_RD;
            cfg.pin_rs = PIN_LCD_DC;
            cfg.pin_d0 = PIN_LCD_D0;
            cfg.pin_d1 = PIN_LCD_D1;
            cfg.pin_d2 = PIN_LCD_D2;
            cfg.pin_d3 = PIN_LCD_D3;
            cfg.pin_d4 = PIN_LCD_D4;
            cfg.pin_d5 = PIN_LCD_D5;
            cfg.pin_d6 = PIN_LCD_D6;
            cfg.pin_d7 = PIN_LCD_D7;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = PIN_LCD_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 170;
            cfg.panel_height = 320;
            cfg.offset_x = 35;
            cfg.offset_y = 0;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }
        {
            auto cfg = light_.config();
            cfg.pin_bl = PIN_LCD_BL;
            cfg.invert = false;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }
        {
            auto cfg = touch_.config();
            cfg.x_min = 0;
            cfg.x_max = 169;
            cfg.y_min = 0;
            cfg.y_max = 319;
            cfg.pin_int = PIN_TOUCH_INT;
            cfg.pin_rst = PIN_TOUCH_RST;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x15;
            cfg.pin_sda = PIN_TOUCH_SDA;
            cfg.pin_scl = PIN_TOUCH_SCL;
            cfg.freq = 400000;
            touch_.config(cfg);
            panel_.setTouch(&touch_);
        }
        setPanel(&panel_);
    }
};
