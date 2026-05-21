#include "floyd_steinberg.h"
#include "src/helpers/histogram.h"
#include <algorithm>
#include <cmath>

void floyd_steinberg(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  int WL = controls.getEffective("Levels (WL)");
  WL = std::clamp(WL, 2, 128);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat fimg;
  gray.convertTo(fimg, CV_32F);
  int rows = fimg.rows;
  int cols = fimg.cols;
  float scale = 255.0f / static_cast<float>(WL - 1);

  auto clampf = [](float v) { return std::clamp(v, 0.0f, 255.0f); };

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      float old = fimg.at<float>(i, j);
      float quant =
          std::round(old * static_cast<float>(WL - 1) / 255.0f) * scale;
      float err = old - quant;
      fimg.at<float>(i, j) = quant;
      if (j + 1 < cols)
        fimg.at<float>(i, j + 1) =
            clampf(fimg.at<float>(i, j + 1) + err * 7.0f / 16.0f);
      if (i + 1 < rows) {
        if (j > 0)
          fimg.at<float>(i + 1, j - 1) =
              clampf(fimg.at<float>(i + 1, j - 1) + err * 3.0f / 16.0f);
        fimg.at<float>(i + 1, j) =
            clampf(fimg.at<float>(i + 1, j) + err * 5.0f / 16.0f);
        if (j + 1 < cols)
          fimg.at<float>(i + 1, j + 1) =
              clampf(fimg.at<float>(i + 1, j + 1) + err * 1.0f / 16.0f);
      }
    }
  }

  cv::Mat dst;
  fimg.convertTo(dst, CV_8UC1);
  auto hist = computeHistogram(dst, WL);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Floyd-Steinberg", dst});
  outputs.push_back({"Histogram", histImg});
}
