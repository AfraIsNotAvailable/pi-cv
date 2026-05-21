#include "histogram.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

std::vector<int> computeHistogram(const cv::Mat &gray, int bins) {
  std::vector<int> hist(bins, 0);
  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int val = gray.at<uchar>(i, j);
      int idx = (val * bins) / 256;
      if (idx >= bins)
        idx = bins - 1;
      hist[idx]++;
    }
  }
  return hist;
}

std::vector<float> computePDF(const std::vector<int> &hist, int total) {
  std::vector<float> pdf(hist.size());
  if (total <= 0)
    return pdf;
  for (size_t i = 0; i < hist.size(); ++i) {
    pdf[i] = static_cast<float>(hist[i]) / static_cast<float>(total);
  }
  return pdf;
}

cv::Mat drawHistogramInt(const std::vector<int> &hist, int width,
                         int height) {
  int bins = static_cast<int>(hist.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  int maxv = *std::max_element(hist.begin(), hist.end());
  if (maxv == 0)
    maxv = 1;
  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = static_cast<float>(hist[i]) / static_cast<float>(maxv);
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

cv::Mat drawHistogramFloat(const std::vector<float> &pdf,
                           int width, int height) {
  int bins = static_cast<int>(pdf.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  float maxv = 0.0f;
  for (float v : pdf)
    maxv = std::max(maxv, v);
  if (maxv == 0.0f)
    maxv = 1.0f;
  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = pdf[i] / maxv;
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

std::vector<int>
computeCumulativeHistogram(const std::vector<int> &hist) {
  std::vector<int> cdf(hist.size(), 0);
  if (hist.empty())
    return cdf;

  // Seed the recurrence: cdf[0] = h[0].
  cdf[0] = hist[0];
  // Accumulate: cdf[i] = cdf[i-1] + h[i].
  for (size_t i = 1; i < hist.size(); ++i) {
    cdf[i] = cdf[i - 1] + hist[i];
  }
  return cdf;
}

cv::Mat drawCumulativeHistogram(const std::vector<int> &cdf,
                                int width, int height) {
  int bins = static_cast<int>(cdf.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  if (bins == 0)
    return img;

  // cdf.back() is monotonically the largest element, so use it as the denom.
  int maxv = cdf.back();
  if (maxv == 0)
    maxv = 1;

  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = static_cast<float>(cdf[i]) / static_cast<float>(maxv);
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

void computeMeanStdDev(const std::vector<int> &hist, int totalPixels,
                       double &outMean, double &outStdDev) {
  outMean = 0.0;
  outStdDev = 0.0;
  if (totalPixels <= 0 || hist.size() < 256)
    return;

  // First moment — sum of (intensity · count), then divide by N.
  double mean = 0.0;
  for (int i = 0; i < 256; ++i) {
    mean += static_cast<double>(i) * hist[i];
  }
  mean /= static_cast<double>(totalPixels);

  // Second central moment — variance — then sqrt for stddev.
  double var = 0.0;
  for (int i = 0; i < 256; ++i) {
    double d = static_cast<double>(i) - mean;
    var += d * d * hist[i];
  }
  var /= static_cast<double>(totalPixels);

  outMean = mean;
  outStdDev = std::sqrt(var);
}

cv::Mat drawHistogramIntWithOverlay(const std::vector<int> &hist,
                                    double mean, double stddev,
                                    int width, int height) {
  // Reuse the monochrome renderer, then convert to BGR for coloured text.
  cv::Mat gray = drawHistogramInt(hist, width, height);
  cv::Mat bgr;
  cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);

  char buf[96];
  std::snprintf(buf, sizeof(buf), "mu=%.2f  sigma=%.2f", mean, stddev);
  cv::putText(bgr, buf, cv::Point(6, 16), cv::FONT_HERSHEY_SIMPLEX, 0.45,
              cv::Scalar(0, 0, 200), 1, cv::LINE_AA);
  return bgr;
}
