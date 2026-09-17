#pragma once

#include <LovyanGFX.hpp>

#include "app_config.hpp"

namespace wordmon {

class CydDisplay : public lgfx::LGFX_Device {
 public:
  CydDisplay() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = kTftSclk;
      cfg.pin_mosi = kTftMosi;
      cfg.pin_miso = kTftMiso;
      cfg.pin_dc = kTftDc;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = kTftCs;
      cfg.pin_rst = kTftRst;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
#if defined(WORDMON_PANEL_ST7789)
      cfg.invert = true;
#else
      cfg.invert = false;
#endif
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }

    {
      auto cfg = light_.config();
      cfg.pin_bl = kBacklightPin;
      cfg.invert = false;
      // Keep dimming above the range normally visible to the eye and reduce
      // rolling bands when the panel is photographed by a phone camera.
      cfg.freq = 20000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }

    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
#if defined(WORDMON_PANEL_ST7789)
  lgfx::Panel_ST7789 panel_;
#else
  lgfx::Panel_ILI9341 panel_;
#endif
  lgfx::Light_PWM light_;
};

}  // namespace wordmon
