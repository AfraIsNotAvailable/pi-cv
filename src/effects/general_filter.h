#pragma once
#include "src/common/output_images.h"
#include <opencv2/opencv.hpp>

void general_filter(const cv::Mat &src, OutputImages &outputs);
