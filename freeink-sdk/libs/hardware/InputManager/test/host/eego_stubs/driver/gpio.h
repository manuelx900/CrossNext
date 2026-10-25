#pragma once

using gpio_num_t = int;
inline bool gpioHeld[49] = {};
inline void gpio_hold_dis(int pin) { gpioHeld[pin] = false; }
inline void gpio_hold_en(int pin) { gpioHeld[pin] = true; }
