#pragma once
#include "types.h"
#include <opencv2/opencv.hpp>
#include <vector>

double normalizeAngleDeg90(double deg);
ObjectStats computeObjectStats(const cv::Mat &mask, int label);
std::vector<int> computeRowProjection(const cv::Mat &mask);
std::vector<int> computeColProjection(const cv::Mat &mask);
void drawCombinedXYProjections(cv::Mat &canvas, const std::vector<int> &xProjection, const std::vector<int> &yProjection, int sourceImageHeight);
