#include "negative.h"

void negative(const cv::Mat &src, OutputImages &outputs) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  for (int i = 0; i < gray.rows; ++i)
    for (int j = 0; j < gray.cols; ++j)
      dst.at<uchar>(i, j) = 255 - gray.at<uchar>(i, j);

  outputs.push_back({"Negative", dst});
}
