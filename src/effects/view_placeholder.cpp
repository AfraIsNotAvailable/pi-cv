#include "view_placeholder.h"

void view_placeholder(const cv::Mat & /*src*/, OutputImages &outputs) {
  cv::Mat dst(256, 256, CV_8UC3);

  const int half = 128;
  dst(cv::Rect(0, 0, half, half)).setTo(cv::Scalar(255, 255, 255)); // white
  dst(cv::Rect(half, 0, half, half)).setTo(cv::Scalar(0, 0, 255));  // red
  dst(cv::Rect(0, half, half, half)).setTo(cv::Scalar(0, 255, 0));  // green
  dst(cv::Rect(half, half, half, half))
      .setTo(cv::Scalar(0, 255, 255)); // yellow

  outputs.push_back({"Placeholder", dst});
}
