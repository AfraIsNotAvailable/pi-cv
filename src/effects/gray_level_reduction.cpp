#include "gray_level_reduction.h"
#include "src/helpers/histogram.h"
#include <algorithm>

void gray_level_reduction(const cv::Mat &src, OutputImages &outputs,
                          ControlsManager &controls) {
  int WL = controls.getEffective("Levels (WL)");
  WL = std::clamp(WL, 2, 128);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.size(), CV_8UC1);
  float scale = 255.0f / static_cast<float>(WL - 1);
  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int g = gray.at<uchar>(i, j);
      int q = (g * WL) / 256; // 0..WL-1
      dst.at<uchar>(i, j) = static_cast<uchar>(q * scale);
    }
  }

  auto hist = computeHistogram(dst, WL);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Reduced", dst});
  outputs.push_back({"Histogram", histImg});
}
