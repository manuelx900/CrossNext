#pragma once
#include "HalStorage.h"
// The firmware/browser build exercises the real decoder. This fixed 2x2 BMP
// isolates media indexing, top-down rows and zero-is-black palette handling.
class PngToBmpConverter {
 public:
  static bool pngFileTo1BitBmpStreamFitWithin(HalFile&, Print& out, int, int) {
    uint8_t b[70]{};
    b[0] = 'B';
    b[1] = 'M';
    b[2] = 70;
    b[10] = 62;
    b[14] = 40;
    b[18] = 2;
    b[22] = 0xfe;
    b[23] = b[24] = b[25] = 0xff;
    b[26] = 1;
    b[28] = 1;
    b[58] = b[59] = b[60] = 255;
    b[62] = 0x40;
    b[66] = 0x80;
    return out.write(b, sizeof(b)) == sizeof(b);
  }
};
