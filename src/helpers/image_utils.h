#pragma once
#include <opencv2/opencv.hpp>

cv::Mat toGray8U(const cv::Mat &src);
cv::Mat ensureBgr(const cv::Mat &image);
cv::Mat makeBinaryForeground(const cv::Mat &src);
cv::Mat makeBlackForegroundMask(const cv::Mat &src);
int countBorderForeground(const cv::Mat &binary);
cv::Mat makeLab7ForegroundMask(const cv::Mat &src);
