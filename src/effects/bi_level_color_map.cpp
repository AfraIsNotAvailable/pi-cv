#include "bi_level_color_map.h"

void bi_level_color_map(const cv::Mat &src, OutputImages &outputs,
                        ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  int threshold = controls.getEffective("Threshold");

  for (int i = 0; i < gray.rows; ++i)
    for (int j = 0; j < gray.cols; ++j)
      dst.at<uchar>(i, j) = gray.at<uchar>(i, j) > threshold ? 255 : 0;

  outputs.push_back({"Bi-Level", dst});
}
