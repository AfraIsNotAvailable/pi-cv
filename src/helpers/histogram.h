#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

std::vector<int> computeHistogram(const cv::Mat &gray, int bins = 256);
std::vector<float> computePDF(const std::vector<int> &hist, int total);
std::vector<int> computeCumulativeHistogram(const std::vector<int> &hist);
void computeMeanStdDev(const std::vector<int> &hist, int totalPixels, double &outMean, double &outStdDev);
cv::Mat drawHistogramInt(const std::vector<int> &hist, int width = 256, int height = 200);
cv::Mat drawHistogramFloat(const std::vector<float> &pdf, int width = 256, int height = 200);
cv::Mat drawCumulativeHistogram(const std::vector<int> &cdf, int width = 256, int height = 200);
cv::Mat drawHistogramIntWithOverlay(const std::vector<int> &hist, double mean, double stddev, int width = 256, int height = 220);
