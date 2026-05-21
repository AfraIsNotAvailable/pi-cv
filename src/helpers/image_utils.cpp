#include "image_utils.h"

cv::Mat toGray8U(const cv::Mat &src) {
  cv::Mat gray;
  if (src.channels() == 1) {
    if (src.type() == CV_8UC1) {
      gray = src;
    } else {
      double minv = 0.0;
      double maxv = 0.0;
      cv::minMaxLoc(src, &minv, &maxv);
      const double span = std::max(1e-9, maxv - minv);
      src.convertTo(gray, CV_8UC1, 255.0 / span, -255.0 * minv / span);
    }
  } else {
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  }
  return gray;
}

cv::Mat ensureBgr(const cv::Mat &image) {
  if (image.channels() == 3) {
    return image.clone();
  }

  cv::Mat converted;
  cv::cvtColor(image, converted, cv::COLOR_GRAY2BGR);
  return converted;
}

cv::Mat makeBinaryForeground(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);
  cv::Mat binary;
  cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  return binary;
}

cv::Mat makeBlackForegroundMask(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);
  cv::Mat mask;
  cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
  return mask;
}

int countBorderForeground(const cv::Mat &binary) {
  CV_Assert(binary.type() == CV_8UC1);
  if (binary.empty())
    return 0;

  int count = 0;
  for (int x = 0; x < binary.cols; ++x) {
    if (binary.at<uchar>(0, x) != 0)
      ++count;
    if (binary.rows > 1 && binary.at<uchar>(binary.rows - 1, x) != 0)
      ++count;
  }
  for (int y = 1; y + 1 < binary.rows; ++y) {
    if (binary.at<uchar>(y, 0) != 0)
      ++count;
    if (binary.cols > 1 && binary.at<uchar>(y, binary.cols - 1) != 0)
      ++count;
  }
  return count;
}

cv::Mat makeLab7ForegroundMask(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);

  cv::Mat whiteForeground;
  cv::threshold(gray, whiteForeground, 0, 255,
                cv::THRESH_BINARY | cv::THRESH_OTSU);

  cv::Mat blackForeground;
  cv::bitwise_not(whiteForeground, blackForeground);

  const int whiteBorder = countBorderForeground(whiteForeground);
  const int blackBorder = countBorderForeground(blackForeground);
  if (whiteBorder < blackBorder)
    return whiteForeground;
  if (blackBorder < whiteBorder)
    return blackForeground;

  const int whiteArea = cv::countNonZero(whiteForeground);
  const int blackArea = cv::countNonZero(blackForeground);
  return (whiteArea <= blackArea) ? whiteForeground : blackForeground;
}
