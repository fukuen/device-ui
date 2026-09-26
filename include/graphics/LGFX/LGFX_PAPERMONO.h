#pragma once

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>
#include <lgfx/v1/panel/Panel_SSD1677.hpp>

// M5Stack PaperMono: 3.97" 4-level grayscale SSD1677 E-Paper, 480x800 portrait.
// Panel geometry mirrors M5GFX: native landscape 800x480, offset_rotation 3
// exposes the panel as the 480x800 portrait the UI is laid out for.

// The SSD1677 framebuffer is only ever written over SPI, so it has no DMA
// requirement. Panel_HasBuffer allocates it from internal RAM by default
// (96KB for the 4-gray planes); keeping it in PSRAM frees that internal RAM
// for the MUI/BLE stack.
class Panel_PaperMonoSSD1677 : public lgfx::Panel_SSD1677_4Gray
{
  public:
    bool init(bool use_reset) override
    {
        bool ok = lgfx::Panel_SSD1677_4Gray::init(use_reset);
        if (ok && _buf) {
            const size_t len = _get_buffer_length();
            lgfx::heap_free(_buf);
            _buf = static_cast<uint8_t *>(lgfx::heap_alloc_psram(len));
            if (_buf) {
                memset(_buf, 0xFF, len);
            }
        }
        return ok && _buf;
    }
};

class LGFX_PAPERMONO : public lgfx::LGFX_Device
{
    lgfx::Bus_SPI _bus_instance;
    Panel_PaperMonoSSD1677 _panel_instance;
    lgfx::Touch_FT5x06 _touch_instance;

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

        {
            auto cfg = _touch_instance.config();

            cfg.pin_int = 4;  // TP INT
            cfg.pin_sda = 47; // I2C_SDA
            cfg.pin_scl = 48; // I2C_SCL
            // Share the firmware's I2C_NUM_0 (the PMIC/IOE1 bus) so the pins stay
            // routed to it; a second port would tear the shared bus away from Wire.
            cfg.i2c_port = 0;
            cfg.freq = 400000;
            cfg.x_min = 0;
            cfg.x_max = 479;
            cfg.y_min = 0;
            cfg.y_max = 799;
            // The FT6336 reports in the panel's portrait orientation; offset 0 would
            // apply the panel's r=3 rotation on top and map the touch 90 degrees off.
            cfg.offset_rotation = 1;
            cfg.bus_shared = false;

            _touch_instance.config(cfg);
        }

        setPanel(&_panel_instance);
        _panel_instance.setTouch(&_touch_instance);
    }
};