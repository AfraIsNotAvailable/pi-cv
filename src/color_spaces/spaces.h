#ifndef __SPACES_H__
#define __SPACES_H__

#include "opencv2/opencv.hpp"

class RGB {
public:
  float r;
  float g;
  float b;

  RGB(float r, float g, float b);
  RGB(uchar r, uchar g, uchar b);

  uchar R();
  uchar G();
  uchar B();
};

class HSV {
public:
  float h; // 0-360
  float s; // 0-1 (actually stored as percentage 0-100 by rgb_to_hsv)
  float v; // 0-1 (percentage)

  HSV(float h, float s, float v);
  HSV(uchar r, uchar g, uchar b);
  HSV(RGB rgb);

  // convert this HSV color back to RGB
  RGB toRGB() const;
};

// helper: build an RGB from HSV components (h in degrees, s/v as 0-100)
RGB hsv_to_rgb(float h, float s, float v);

#endif