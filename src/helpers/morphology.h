#pragma once
#include <opencv2/opencv.hpp>

cv::Mat dilate8Once(const cv::Mat &binary);
cv::Mat erode8Once(const cv::Mat &binary);
cv::Mat applyMorphIterations(const cv::Mat &binary, int iterations, bool useDilation);
cv::Mat opening8Once(const cv::Mat &binary);
cv::Mat closing8Once(const cv::Mat &binary);
cv::Mat repeatOpening(const cv::Mat &binary, int repetitions);
cv::Mat repeatClosing(const cv::Mat &binary, int repetitions);
cv::Mat boundaryExtraction8(const cv::Mat &binary);
cv::Mat morphologicalRegionFill8(const cv::Mat &objectMask, const cv::Point &seed, int maxIterations, int &usedIterations, bool &converged);
