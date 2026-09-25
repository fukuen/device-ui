#pragma once

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>
#include <lgfx/v1/panel/Panel_SSD1677.hpp>

// M5Stack PaperMono: 3.97" 4-level grayscale SSD1677 E-Paper, 480x800 portrait.
// Panel geometry mirrors M5GFX: native landscape 800x480, offset_rotation 3
// exposes the panel as the 480x800 portrait the UI is laid out for.
class LGFX_PAPERMONO : public lgfx::LGFX_Device
{
    lgfx::Bus_SPI _bus_instance;
    lgfx::Panel_SSD1677_4Gray _panel_instance;

  public:
    const uint32_t screenWidth = 480;
    const uint32_t screenHeight = 800;

    bool hasButton(void) { return false; }

    LGFX_PAPERMONO(void)
    {
        {
            auto cfg = _bus_instance.config();

            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000; // EPD SPI clock
            cfg.freq_read = 10000000;
            cfg.spi_3wire = true;
            cfg.use_lock = true;
            cfg.pin_sclk = 15; // EPD_SCLK
            cfg.pin_mosi = 14; // EPD_MOSI
            cfg.pin_miso = -1;
            cfg.pin_dc = 17; // EPD_D/C

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();

            cfg.pin_cs = 16;   // EPD_CS
            cfg.pin_rst = -1;  // Reset is driven through the M5IOE1 (variant.cpp)
            cfg.pin_busy = 18; // EPD_BUSY

            // SSD1677 native landscape 800x480; offset_rotation 3 exposes portrait.
            cfg.panel_width = 800;
            cfg.panel_height = 480;
            cfg.memory_width = 800;
            cfg.memory_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 3;
            cfg.readable = false;
            cfg.invert = false;
            cfg.bus_shared = false;

            _panel_instance.config(cfg);
            _panel_instance.setRotation(0);
        }

        setPanel(&_panel_instance);
    }
};