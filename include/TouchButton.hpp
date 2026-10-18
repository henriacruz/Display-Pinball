#pragma once
#include <stdint.h>

// Estado independente do SPI e da velocidade de decodificacao dos GIFs.
class TouchButton {
 public:
  bool update(bool pressed, uint32_t now) {
    if (pressed != candidate_) {
      candidate_ = pressed;
      changedAt_ = now;
    }
    const uint32_t debounce = candidate_ ? 30 : 60;
    if (candidate_ != stable_ && uint32_t(now - changedAt_) >= debounce) {
      stable_ = candidate_;
      return stable_;
    }
    return false;
  }
 private:
  bool candidate_ = false;
  bool stable_ = false;
  uint32_t changedAt_ = 0;
};
