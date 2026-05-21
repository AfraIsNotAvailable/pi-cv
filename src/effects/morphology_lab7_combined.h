#pragma once
#include "src/common/output_images.h"
#include "src/controls/controls_manager.h"
#include <opencv2/opencv.hpp>

void morphology_lab7_combined(const cv::Mat &src, OutputImages &outputs, ControlsManager &controls);
