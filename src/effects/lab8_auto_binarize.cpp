#include "lab8_auto_binarize.h"
#include "src/common/logger/logger.h"
#include "src/helpers/histogram.h"
#include "src/helpers/image_utils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void lab8_auto_binarize(const cv::Mat &src, OutputImages &outputs,
                        ControlsManager &controls) {
  int epsInt = controls.getEffective("Epsilon x100");
  if (epsInt < 1)
    epsInt = 1;
  double epsilon = static_cast<double>(epsInt) / 100.0;

  cv::Mat gray = toGray8U(src);
  auto hist = computeHistogram(gray, 256);

  int iMin = 0;
  int iMax = 255;
  while (iMin < 256 && hist[iMin] == 0)
    ++iMin;
  while (iMax > 0 && hist[iMax] == 0)
    --iMax;

  if (iMin >= iMax) {
    cv::Mat bin(gray.size(), CV_8UC1, cv::Scalar(0));
    outputs.push_back({"Binary", bin});
    return;
  }

  double T = 0.5 * (iMin + iMax);
  double prevT = T + 10.0 * epsilon + 1.0;
  int iterations = 0;
  const int maxIterations = 1000;

  while (std::abs(T - prevT) >= epsilon && iterations < maxIterations) {
    prevT = T;
    long long n1 = 0;
    long long n2 = 0;
    double s1 = 0.0;
    double s2 = 0.0;
    int Ti = static_cast<int>(std::floor(T));

    for (int i = iMin; i <= iMax; ++i) {
      if (i <= Ti) {
        n1 += hist[i];
        s1 += static_cast<double>(i) * hist[i];
      } else {
        n2 += hist[i];
        s2 += static_cast<double>(i) * hist[i];
      }
    }

    double mu1 =
        (n1 > 0) ? (s1 / static_cast<double>(n1)) : static_cast<double>(iMin);
    double mu2 =
        (n2 > 0) ? (s2 / static_cast<double>(n2)) : static_cast<double>(iMax);
    T = 0.5 * (mu1 + mu2);
    ++iterations;
  }

  int thresh = std::clamp(static_cast<int>(std::round(T)), 0, 255);
  INFO("[Lab8] auto-binarize: Imin={} Imax={} T={:.4f} iter={} eps={:.4f}",
       iMin, iMax, T, iterations, epsilon);

  cv::Mat bin(gray.size(), CV_8UC1);
  for (int r = 0; r < gray.rows; ++r) {
    const uchar *s = gray.ptr<uchar>(r);
    uchar *d = bin.ptr<uchar>(r);
    for (int c = 0; c < gray.cols; ++c) {
      d[c] = (s[c] > thresh) ? 255 : 0;
    }
  }

  cv::Mat binAnnotated;
  cv::cvtColor(bin, binAnnotated, cv::COLOR_GRAY2BGR);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "T = %d  (iter %d)", thresh, iterations);
  cv::putText(binAnnotated, buf, cv::Point(8, 20), cv::FONT_HERSHEY_SIMPLEX,
              0.55, cv::Scalar(0, 0, 220), 1, cv::LINE_AA);

  outputs.push_back({"Binary", binAnnotated});
}
