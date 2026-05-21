#pragma once
#include <array>
#include <opencv2/opencv.hpp>

enum class NeighborhoodType { N4 = 0, N8 = 1 };

inline const std::array<cv::Point, 4> kNeighbors4 = {
    cv::Point(1, 0), cv::Point(-1, 0), cv::Point(0, 1), cv::Point(0, -1)};

inline const std::array<cv::Point, 8> kNeighbors8 = {
    cv::Point(1, 0), cv::Point(-1, 0), cv::Point(0, 1),  cv::Point(0, -1),
    cv::Point(1, 1), cv::Point(1, -1), cv::Point(-1, 1), cv::Point(-1, -1)};

inline const std::array<cv::Point, 4> kChainDirections4 = {
    cv::Point(1, 0), cv::Point(0, -1), cv::Point(-1, 0), cv::Point(0, 1)};

inline const std::array<cv::Point, 8> kChainDirections8 = {
    cv::Point(1, 0),  cv::Point(1, -1), cv::Point(0, -1), cv::Point(-1, -1),
    cv::Point(-1, 0), cv::Point(-1, 1), cv::Point(0, 1),  cv::Point(1, 1)};

struct RenderContext {
  int sourcePanelX = 0;
  int sourcePanelY = 0;
  int sourcePanelW = 0;
  int sourcePanelH = 0;
  int cellW = 0;
  int cellH = 0;
  int totalCellH = 0;
  int labelHeight = 30;
  int srcW = 0;
  int srcH = 0;
};

struct ObjectStats {
  int label = 0;
  double area = 0.0;
  cv::Point2d centroid{0.0, 0.0};
  double orientationDeg = 0.0;
  double perimeter = 0.0;
  double thinnessRatio = 0.0;
  double aspectRatio = 0.0;
  double majorAxisLength = 0.0;
  double minorAxisLength = 0.0;
};

struct SelectionState {
  cv::Point imagePoint{-1, -1};
  int label = -1;
  bool hasClick = false;
  bool dirty = true;
};

struct ChainCodeTrace {
  cv::Point startPoint{0, 0};
  std::vector<int> code;
  std::vector<int> derivative;
  std::vector<cv::Point> tracedPoints;
};

struct ChainCodeFileData {
  cv::Point startPoint{0, 0};
  int declaredLength = 0;
  std::vector<int> code;
};

struct TwoPassLabelingResult {
  cv::Mat firstPassLabels;
  cv::Mat finalLabels;
};
