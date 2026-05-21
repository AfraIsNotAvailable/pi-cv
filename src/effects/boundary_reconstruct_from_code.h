#pragma once
#include "src/common/output_images.h"
#include "src/controls/controls_manager.h"
#include <opencv2/opencv.hpp>

void boundary_reconstruct_from_code(const cv::Mat &src, OutputImages &outputs, ControlsManager &controls);
