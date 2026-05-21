#include "lab8_transforms.h"
#include "src/common/logger/logger.h"
#include "src/helpers/histogram.h"
#include "src/helpers/image_utils.h"
#include <algorithm>
#include <cmath>

void lab8_transforms(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  int outMin = std::clamp(controls.getEffective("Iout Min"), 0, 255);
  int outMax = std::clamp(controls.getEffective("Iout Max"), 0, 255);
  if (outMax < outMin)
    std::swap(outMin, outMax);

  int gammaInt = controls.getEffective("Gamma x10");
  if (gammaInt < 1)
    gammaInt = 1;
  double gamma = static_cast<double>(gammaInt) / 10.0;

  int brightnessRaw = controls.getEffective("Brightness Offset");
  int offset = brightnessRaw - 128;

  cv::Mat gray = toGray8U(src);
  auto histOriginal = computeHistogram(gray, 256);

  double inMinD = 0.0;
  double inMaxD = 0.0;
  cv::minMaxLoc(gray, &inMinD, &inMaxD);
  int inMin = static_cast<int>(inMinD);
  int inMax = static_cast<int>(inMaxD);
  double inSpan = std::max(1.0, static_cast<double>(inMax - inMin));

  cv::Mat negative(gray.size(), CV_8UC1);
  cv::Mat contrast(gray.size(), CV_8UC1);
  cv::Mat gammaImg(gray.size(), CV_8UC1);
  cv::Mat brightness(gray.size(), CV_8UC1);

  uchar lutNeg[256];
  uchar lutCon[256];
  uchar lutGam[256];
  uchar lutBri[256];
  for (int v = 0; v < 256; ++v) {
    lutNeg[v] = static_cast<uchar>(255 - v);

    double stretched = outMin + (v - inMin) * (outMax - outMin) / inSpan;
    lutCon[v] = static_cast<uchar>(std::clamp(stretched, 0.0, 255.0));

    double g = 255.0 * std::pow(static_cast<double>(v) / 255.0, gamma);
    lutGam[v] = static_cast<uchar>(std::clamp(g, 0.0, 255.0));

    int b = v + offset;
    lutBri[v] = static_cast<uchar>(std::clamp(b, 0, 255));
  }

  auto applyLut = [&](const uchar lut[256], cv::Mat &dst) {
    for (int r = 0; r < gray.rows; ++r) {
      const uchar *s = gray.ptr<uchar>(r);
      uchar *d = dst.ptr<uchar>(r);
      for (int c = 0; c < gray.cols; ++c) {
        d[c] = lut[s[c]];
      }
    }
  };
  applyLut(lutNeg, negative);
  applyLut(lutCon, contrast);
  applyLut(lutGam, gammaImg);
  applyLut(lutBri, brightness);

  auto histNeg = computeHistogram(negative, 256);
  auto histCon = computeHistogram(contrast, 256);
  auto histGam = computeHistogram(gammaImg, 256);
  auto histBri = computeHistogram(brightness, 256);

  outputs.push_back({"Negative", negative});
  outputs.push_back({"Contrast", contrast});
  outputs.push_back({"Gamma", gammaImg});
  outputs.push_back({"Brightness", brightness});

  outputs.push_back({"Hist Original", drawHistogramInt(histOriginal)});
  outputs.push_back({"Hist Negative", drawHistogramInt(histNeg)});
  outputs.push_back({"Hist Contrast", drawHistogramInt(histCon)});
  outputs.push_back({"Hist Gamma", drawHistogramInt(histGam)});
  outputs.push_back({"Hist Brightness", drawHistogramInt(histBri)});

  INFO("[Lab8] transforms: outMin={} outMax={} gamma={:.2f} offset={}", outMin,
       outMax, gamma, offset);
}
