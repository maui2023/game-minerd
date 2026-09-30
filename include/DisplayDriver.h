#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "Config.h"

#if defined(BOARD_CYD_2432S028)

// Konfigurasi paparan untuk ESP32-2432S028R ("Cheap Yellow Display")
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc   = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = TFT_CS;
            cfg.pin_rst          = TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0; // Standard 0 offset
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq   = 1000; // 1000 Hz piawai untuk transistor lampu belakang CYD
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 300;
            cfg.x_max      = 3900;
            cfg.y_min      = 3700;
            cfg.y_max      = 200;
            cfg.pin_int    = -1;
            cfg.bus_shared = false;
            cfg.spi_host   = -1; // Gunakan software SPI untuk XPT2046 CYD
            cfg.pin_sclk   = 25;
            cfg.pin_mosi   = 32;
            cfg.pin_miso   = 39;
            cfg.pin_cs     = 33;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

#elif defined(BOARD_TTGO_TDISPLAY)

// Konfigurasi paparan untuk LILYGO TTGO T-Display (ST7789)
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = VSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc   = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = TFT_CS;
            cfg.pin_rst          = TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 135;
            cfg.panel_height     = 240;
            cfg.offset_x         = 52;
            cfg.offset_y         = 40;
            cfg.offset_rotation  = 1;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

#else

// Paparan generik ILI9341 untuk ESP32 DevKit V1
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = VSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc   = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = TFT_CS;
            cfg.pin_rst  = TFT_RST;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_rotation = 1;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

#endif

#endif // DISPLAY_DRIVER_H
