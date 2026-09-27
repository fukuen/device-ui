#pragma once

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>
#include <Wire.h>
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

// FT6336U touch read through the shared IDF Wire bus instead of lgfx::i2c.
// LovyanGFX's register-level control of I2C_NUM_0 corrupts the IDF i2c_master bus
// the firmware uses for the M5PM1/M5IOE1, so this driver keeps a single I2C driver
// (Wire) in control of the shared pins.
namespace lgfx
{
inline namespace v1
{
class Touch_FT6x06 : public ITouch
{
  private:
    bool _flg_released = true; // last INT edge was a release (INT high)

  public:
    bool init(void) override
    {
        if (_cfg.pin_int >= 0) {
            // Arduino pinMode so the peripheral manager claims GPIO4 and digitalRead
            // below reports a consistent level instead of warning every tick.
            ::pinMode(_cfg.pin_int, INPUT_PULLUP);
            // Polling mode: the INT line stays low while a touch is latched, so the
            // press/release edges in getTouchRaw align with the controller's state.
            Wire.beginTransmission(_cfg.i2c_addr);
            Wire.write(0xA4); // FT5x06_INTMODE_REG
            Wire.write(0x00); // INT Polling mode
            Wire.endTransmission();
            _flg_released = true;
        }
        return true; // Wire is already brought up by the firmware
    }
    void wakeup(void) override {}
    void sleep(void) override {}
    uint_fast8_t getTouchRaw(touch_point_t *tp, uint_fast8_t count) override
    {
        if (count == 0) return 0;
        // Report the point once per press edge: the FT6336 pulls its INT low while a
        // touch is latched and lets it go after the data is read, so a single PR/REL
        // pair per tap keeps LVGL's click detection on the first touch.
        if (_cfg.pin_int >= 0) {
            if (_flg_released != (bool)digitalRead(_cfg.pin_int)) {
                _flg_released = !_flg_released;
            }
            if (_flg_released) return 0;
        }
        uint8_t addr = _cfg.i2c_addr;
        uint8_t data[5];
        Wire.beginTransmission(addr);
        Wire.write(0x02);
        if (Wire.endTransmission(false) != 0) return 0;
        if (Wire.requestFrom((int)addr, (int)5) != 5) return 0;
        for (int i = 0; i < 5; i++) data[i] = Wire.read();
        uint8_t points = data[0] & 0x0F;
        if (points == 0) return 0;
        tp[0].id = 0;
        tp[0].size = 1;
        // The panel reports 480x800 raw coords; LVGL renders at 240x320 with the
        // flush upscaling 2x, so halve here to keep the touch aligned.
        tp[0].x = (((data[1] & 0x0F) << 8) | data[2]) >> 1;
        tp[0].y = (((data[3] & 0x0F) << 8) | data[4]) >> 1;
        return 1;
    }
};
}
}

class LGFX_PAPERMONO : public lgfx::LGFX_Device
{
    lgfx::Bus_SPI _bus_instance;
    Panel_PaperMonoSSD1677 _panel_instance;
    lgfx::Touch_FT6x06 _touch_instance;

  public:
    // LVGL renders at 240x400 (the portrait view stretched to the panel's 3:5 aspect)
    // and the throttled flush upscales each area 2x into the panel's 480x800 buffer.
    const uint32_t screenWidth = 240;
    const uint32_t screenHeight = 400;

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
            cfg.i2c_addr = 0x38; // FT6336U
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