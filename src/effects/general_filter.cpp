#include "general_filter.h"

static void center_transform(cv::Mat src) {
  for (int i = 0; i < src.rows; i++) {
    for (int j = 0; j < src.cols; j++) {
      src.at<float>(i, j) =
          ((i + j) & 1) ? -src.at<float>(i, j) : src.at<float>(i, j);
    }
  }
}

void general_filter(const cv::Mat &src, OutputImages &outputs) {

  cv::Mat gray;
  if (src.channels() > 1) {
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  } else {
    gray = src;
  }

  cv::Mat srcf;

  center_transform(srcf);

  cv::Mat fourier;

  dft(srcf, fourier, cv::DFT_COMPLEX_OUTPUT);

  cv::Mat dst;

  cv::normalize(fourier, dst, 0, 255, cv::NORM_MINMAX, CV_8UC1);

  outputs.push_back({"Fourier", fourier});

  // TODO: finish
}
