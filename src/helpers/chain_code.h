#pragma once
#include "types.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

int chainDirectionCount(NeighborhoodType neighborhood);
cv::Point chainDirectionOffset(NeighborhoodType neighborhood, int direction);
int deltaToDirection(const cv::Point &delta, NeighborhoodType neighborhood);
void expandDiagonalToN4(const cv::Point &delta, std::vector<int> &out);
std::string joinCode(const std::vector<int> &code);
std::vector<int> computeChainDerivative(const std::vector<int> &code, NeighborhoodType neighborhood);
void appendContourStep(const cv::Point &from, const cv::Point &to, NeighborhoodType neighborhood, std::vector<int> &code, std::vector<cv::Point> &points);
ChainCodeTrace buildChainCodeTrace(const std::vector<cv::Point> &contour, NeighborhoodType neighborhood);
void drawPolyline(cv::Mat &image, const std::vector<cv::Point> &points, const cv::Scalar &color);
void logChainCodeTrace(int componentIndex, NeighborhoodType neighborhood, const ChainCodeTrace &trace);
bool parseChainCodeFile(const std::string &text, ChainCodeFileData &data);
std::vector<cv::Point> reconstructFromChainCode(const cv::Point &startPoint, const std::vector<int> &code, NeighborhoodType neighborhood);
