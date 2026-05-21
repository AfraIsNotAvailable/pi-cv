#pragma once
#include "src/common/output_images.h"
#include "src/controls/controls_manager.h"
#include <opencv2/opencv.hpp>

void contour_and_region_fill_lab7(const cv::Mat &src, OutputImages &outputs, ControlsManager &controls);
