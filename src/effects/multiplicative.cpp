#include "multiplicative.h"

void multiplicative(const cv::Mat &src, OutputImages &outputs,
                    ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  const float factor =
      static_cast<float>(controls.getEffective("Multiplicative"));

  cv::multiply(gray, cv::Scalar(factor), dst);

  outputs.push_back({"Multiplied", dst});
}
