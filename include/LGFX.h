#pragma once

#include <LovyanGFX.hpp>

// Pimoroni Pico Display Pack 2.0 (320x240 IPS, ST7789) on a Raspberry Pi Pico W.
// Wired to SPI0: SCK=GP18, MOSI=GP19. Control: CS=GP17, DC=GP16, backlight=GP20.
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host = 0; // RP2040 spi0
            cfg.spi_mode = 0;
            cfg.freq_write = 62500000;
            cfg.pin_miso = -1;
            cfg.pin_mosi = 19;
            cfg.pin_sclk = 18;
            cfg.pin_dc = 16;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = 17;
            cfg.pin_rst = -1;
            cfg.pin_busy = -1;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = 20;
            cfg.invert = false;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};
