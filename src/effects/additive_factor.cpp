#include "additive_factor.h"
#include <algorithm>

void additive_factor(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  // Slider 0-255 maps to -128..+127
  int factor = controls.getEffective("Additive Factor");

  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int val = gray.at<uchar>(i, j) + factor;
      dst.at<uchar>(i, j) = static_cast<uchar>(std::clamp(val, 0, 255));
    }
  }

  outputs.push_back({"Adjusted", dst});
}
