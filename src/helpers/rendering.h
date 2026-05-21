#pragma once
#include "src/common/output_images.h"
#include <opencv2/opencv.hpp>
#include <string>

inline constexpr const char *MAIN_WINDOW_NAME = "Image Processing";

void renderGrid(const cv::Mat &src, const OutputImages &outputs);
void saveAllOutputs(const OutputImages &outputs);
bool loadImage(const std::string &path, cv::Mat &img);
bool hasQtBackend();
void updateMainWindowTitle(const std::string &effectName);
bool mapMainWindowPointToSource(const cv::Point &windowPoint, cv::Point &sourcePoint);
void mainWindowMouseCallback(int event, int x, int y, int flags, void *userdata);
