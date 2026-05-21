#include "spaces.h"
#include "logger.h"
#include <algorithm>
#include <cmath>

// Currently this function is not exposed.
HSV rgb_to_hsv(float r, float g, float b)
{
  r = r / 255.0;
  g = g / 255.0;
  b = b / 255.0;

  // h, s, v = hue, saturation, value
  float cmax = std::max(r, std::max(g, b)); // maximum of r, g, b
  float cmin = std::min(r, std::min(g, b)); // minimum of r, g, b
  float diff = cmax - cmin; // diff of cmax and cmin.
  float h = -1, s = -1;
    
  // if cmax and cmax are equal then h = 0
  if (cmax == cmin)
    h = 0;

  // if cmax equal r then compute h
  else if (cmax == r)
    h = int(60 * ((g - b) / diff) + 360) % 360;

  // if cmax equal g then compute h
  else if (cmax == g)
    h = int(60 * ((b - r) / diff) + 120) % 360;

  // if cmax equal b then compute h
  else if (cmax == b)
    h = int(60 * ((r - g) / diff) + 240) % 360;

  // if cmax equal zero
  if (cmax == 0)
    s = 0;
  else
    s = (diff / cmax) * 100;

  // compute v
  float v = cmax * 100;

  return HSV(h, s, v);
}

// inverse conversion: H in degrees [0,360), s/v 0-100
RGB hsv_to_rgb(float h, float s, float v)
{
    float hh = h;
    if (hh >= 360.0f) hh = std::fmod(hh, 360.0f);
    float ss = s / 100.0f;
    float vv = v / 100.0f;
    float c = vv * ss;
    float x = c * (1.0f - std::fabs(std::fmod(hh / 60.0f, 2.0f) - 1.0f));
    float m = vv - c;
    float r_, g_, b_;
    if (hh < 60) {
        r_ = c; g_ = x; b_ = 0;
    } else if (hh < 120) {
        r_ = x; g_ = c; b_ = 0;
    } else if (hh < 180) {
        r_ = 0; g_ = c; b_ = x;
    } else if (hh < 240) {
        r_ = 0; g_ = x; b_ = c;
    } else if (hh < 300) {
        r_ = x; g_ = 0; b_ = c;
    } else {
        r_ = c; g_ = 0; b_ = x;
    }
    return RGB((r_ + m) * 255.0f,
               (g_ + m) * 255.0f,
               (b_ + m) * 255.0f);
}


RGB::RGB(float r, float g, float b)
  :r(r), g(g), b(b)
{}

RGB::RGB(uchar r, uchar g, uchar b)
  :r((float)r), g((float)g), b((float)b)
{}

uchar RGB::R()
{
  return (uchar) r;
}

uchar RGB::G()
{
  return (uchar) g;
}

uchar RGB::B()
{
  return (uchar) b;
}

HSV::HSV(float h, float s, float v)
  :h(h), s(s), v(v)
{}

HSV::HSV(uchar r, uchar g, uchar b)
  :h(0), s(0), v(0)
{
  HSV tmp = rgb_to_hsv((float)r, (float)g, (float)b);
  this->h = tmp.h;
  this->s = tmp.s;
  this->v = tmp.v;
}

RGB HSV::toRGB() const
{
    return hsv_to_rgb(h, s, v);
}

HSV::HSV(RGB rgb)
  :h(0), s(0), v(0)
{
  HSV tmp = rgb_to_hsv(rgb.r, rgb.g, rgb.b);
  this->h = tmp.h;
  this->s = tmp.s;
  this->v = tmp.v;
}