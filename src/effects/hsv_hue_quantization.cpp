#include "hsv_hue_quantization.h"
#include "src/color_spaces/spaces.h"
#include "src/helpers/image_utils.h"
#include <algorithm>
#include <cmath>

void hsv_hue_quantization(const cv::Mat &src, OutputImages &outputs,
                          ControlsManager &controls) {
  int levels = controls.getEffective("Hue Levels");
  levels = std::clamp(levels, 2, 128);
  int mode = controls.getRadio("S/V Mode");

  cv::Mat bgr = ensureBgr(src);
  cv::Mat dst(bgr.size(), CV_8UC3);
  for (int i = 0; i < bgr.rows; ++i) {
    for (int j = 0; j < bgr.cols; ++j) {
      cv::Vec3b pix = bgr.at<cv::Vec3b>(i, j);
      HSV hsv(pix[2], pix[1], pix[0]); // convert BGR->HSV
      float h = hsv.h;
      float newh = std::round(h * levels / 360.0f) * (360.0f / levels);
      if (newh >= 360.0f)
        newh = 0.0f;
      hsv.h = newh;
      if (mode == 1) {
        hsv.s = 100.0f;
        hsv.v = 100.0f;
      }
      RGB rgb = hsv.toRGB();
      dst.at<cv::Vec3b>(i, j) = cv::Vec3b(rgb.B(), rgb.G(), rgb.R());
    }
  }
  outputs.push_back({"HSV Quantized", dst});
}
