#pragma once

#include <LovyanGFX.hpp>
#include "PlayerConfig.hpp"

// Perfil PROVISORIO para ESP32-3248S035R (touch resistivo).
// Nao usar na variante C (touch capacitivo).
class Display : public lgfx::LGFX_Device {
 public:
  Display() {
    auto spi = bus_.config();
    spi.spi_host = HSPI_HOST;
    spi.spi_mode = 0;
    spi.freq_write = config::displayFrequency;
    spi.freq_read = 16000000;
    spi.spi_3wire = true;
    spi.use_lock = true;
    spi.dma_channel = SPI_DMA_CH_AUTO;
    spi.pin_sclk = 14;
    spi.pin_mosi = 13;
    spi.pin_miso = 12;
    spi.pin_dc = 2;
    bus_.config(spi);
    panel_.setBus(&bus_);

    auto panel = panel_.config();
    panel.pin_cs = 15;
    panel.pin_rst = -1;
    panel.panel_width = 320;
    panel.panel_height = 480;
    panel.bus_shared = true;
    panel_.config(panel);

    auto light = light_.config();
    light.pin_bl = 27;
    light.invert = false;
    light.freq = 1200;
    light.pwm_channel = 7;
    light_.config(light);
    panel_.setLight(&light_);

    auto touch = touch_.config();
    touch.spi_host = HSPI_HOST;
    touch.freq = 1000000;
    touch.bus_shared = true;
    touch.pin_sclk = 14;
    touch.pin_mosi = 13;
    touch.pin_miso = 12;
    touch.pin_cs = 33;
    touch.pin_int = -1;  // Leitura por polling.
    touch.x_min = 200;
    touch.x_max = 3800;
    touch.y_min = 3900;
    touch.y_max = 300;
    touch_.config(touch);
    panel_.setTouch(&touch_);
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_ST7796 panel_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;
};
