#include "lab8_histogram_equalization.h"
#include "src/common/logger/logger.h"
#include "src/helpers/histogram.h"
#include "src/helpers/image_utils.h"
#include <algorithm>
#include <cmath>

void lab8_histogram_equalization(const cv::Mat &src, OutputImages &outputs,
                                 ControlsManager & /*controls*/) {
  cv::Mat gray = toGray8U(src);
  int total = gray.rows * gray.cols;
  if (total <= 0) {
    outputs.push_back({"Equalized", gray});
    return;
  }

  auto hist = computeHistogram(gray, 256);

  double invTotal = 1.0 / static_cast<double>(total);
  uchar lut[256];
  double running = 0.0;
  for (int i = 0; i < 256; ++i) {
    running += static_cast<double>(hist[i]) * invTotal;
    int mapped = static_cast<int>(std::round(255.0 * running));
    lut[i] = static_cast<uchar>(std::clamp(mapped, 0, 255));
  }

  cv::Mat equalized(gray.size(), CV_8UC1);
  for (int r = 0; r < gray.rows; ++r) {
    const uchar *s = gray.ptr<uchar>(r);
    uchar *d = equalized.ptr<uchar>(r);
    for (int c = 0; c < gray.cols; ++c) {
      d[c] = lut[s[c]];
    }
  }

  auto histEq = computeHistogram(equalized, 256);

  double muOrig = 0.0;
  double sigOrig = 0.0;
  double muEq = 0.0;
  double sigEq = 0.0;
  computeMeanStdDev(hist, total, muOrig, sigOrig);
  computeMeanStdDev(histEq, total, muEq, sigEq);

  INFO(
      "[Lab8] equalize: orig mu={:.2f} sigma={:.2f}  eq mu={:.2f} sigma={:.2f}",
      muOrig, sigOrig, muEq, sigEq);

  outputs.push_back({"Equalized", equalized});
  outputs.push_back(
      {"Hist Original", drawHistogramIntWithOverlay(hist, muOrig, sigOrig)});
  outputs.push_back(
      {"Hist Equalized", drawHistogramIntWithOverlay(histEq, muEq, sigEq)});
}
