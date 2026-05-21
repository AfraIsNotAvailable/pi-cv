#include "lab8_statistics.h"
#include "src/common/logger/logger.h"
#include "src/helpers/histogram.h"
#include "src/helpers/image_utils.h"

void lab8_statistics(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager & /*controls*/) {
  cv::Mat gray = toGray8U(src);

  auto hist = computeHistogram(gray, 256);
  auto cdf = computeCumulativeHistogram(hist);
  auto pdf = computePDF(hist, gray.rows * gray.cols);

  double mean = 0.0;
  double stddev = 0.0;
  computeMeanStdDev(hist, gray.rows * gray.cols, mean, stddev);

  INFO("[Lab8] mean={:.4f} stddev={:.4f}", mean, stddev);

  outputs.push_back(
      {"Histogram", drawHistogramIntWithOverlay(hist, mean, stddev)});
  outputs.push_back({"Cumulative", drawCumulativeHistogram(cdf)});
  outputs.push_back({"PDF", drawHistogramFloat(pdf)});
}
