#pragma once
#include "types.h"
#include <opencv2/opencv.hpp>
#include <vector>

cv::Mat buildLabelsByConnectedComponents(const cv::Mat &srcGray8u);
cv::Mat buildLabelsFromGrayValues(const cv::Mat &gray8u);
cv::Mat buildLabelsFromColorValues(const cv::Mat &bgr);
cv::Mat buildLabelImage(const cv::Mat &src);
cv::Mat labelComponentsByTraversal(const cv::Mat &binary, NeighborhoodType neighborhood, bool useDfsStack = false);
void collectPreviousNeighborLabels(const cv::Mat &labels, int x, int y, NeighborhoodType neighborhood, std::vector<int> &outLabels);
TwoPassLabelingResult labelComponentsTwoPass(const cv::Mat &binary, NeighborhoodType neighborhood);
std::vector<int> collectLabels(const cv::Mat &labels);
cv::Mat maskForLabel(const cv::Mat &labels, int label);
cv::Mat colorizeLabels(const cv::Mat &labels);
