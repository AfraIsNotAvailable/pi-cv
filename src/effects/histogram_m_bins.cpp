#include "histogram_m_bins.h"
#include "src/helpers/histogram.h"
#include <algorithm>

void histogram_m_bins(const cv::Mat &src, OutputImages &outputs,
                      ControlsManager &controls) {
  int m = controls.getEffective("Bins (m)");
  m = std::clamp(m, 2, 256);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  auto hist = computeHistogram(gray, m);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Histogram (m bins)", histImg});
}
