#include "chain_code.h"
#include <iostream>
#include <sstream>

int chainDirectionCount(NeighborhoodType neighborhood) {
  return (neighborhood == NeighborhoodType::N4) ? 4 : 8;
}

cv::Point chainDirectionOffset(NeighborhoodType neighborhood,
                               int direction) {
  return (neighborhood == NeighborhoodType::N4) ? kChainDirections4[direction]
                                                : kChainDirections8[direction];
}

int deltaToDirection(const cv::Point &delta,
                     NeighborhoodType neighborhood) {
  if (neighborhood == NeighborhoodType::N4) {
    if (delta == cv::Point(1, 0))
      return 0;
    if (delta == cv::Point(0, -1))
      return 1;
    if (delta == cv::Point(-1, 0))
      return 2;
    if (delta == cv::Point(0, 1))
      return 3;
    return -1;
  }

  if (delta == cv::Point(1, 0))
    return 0;
  if (delta == cv::Point(1, -1))
    return 1;
  if (delta == cv::Point(0, -1))
    return 2;
  if (delta == cv::Point(-1, -1))
    return 3;
  if (delta == cv::Point(-1, 0))
    return 4;
  if (delta == cv::Point(-1, 1))
    return 5;
  if (delta == cv::Point(0, 1))
    return 6;
  if (delta == cv::Point(1, 1))
    return 7;
  return -1;
}

void expandDiagonalToN4(const cv::Point &delta, std::vector<int> &out) {
  if (delta == cv::Point(1, -1)) {
    out.push_back(0);
    out.push_back(1);
  } else if (delta == cv::Point(-1, -1)) {
    out.push_back(1);
    out.push_back(2);
  } else if (delta == cv::Point(-1, 1)) {
    out.push_back(2);
    out.push_back(3);
  } else if (delta == cv::Point(1, 1)) {
    out.push_back(0);
    out.push_back(3);
  }
}

std::string joinCode(const std::vector<int> &code) {
  std::ostringstream oss;
  for (size_t i = 0; i < code.size(); ++i) {
    if (i > 0)
      oss << ' ';
    oss << code[i];
  }
  return oss.str();
}

std::vector<int> computeChainDerivative(const std::vector<int> &code,
                                        NeighborhoodType neighborhood) {
  std::vector<int> derivative;
  if (code.empty())
    return derivative;

  const int modulus = chainDirectionCount(neighborhood);
  derivative.reserve(code.size());
  for (size_t i = 0; i < code.size(); ++i) {
    const int current = code[i];
    const int next = code[(i + 1) % code.size()];
    derivative.push_back((next - current + modulus) % modulus);
  }
  return derivative;
}

void appendContourStep(const cv::Point &from, const cv::Point &to,
                       NeighborhoodType neighborhood,
                       std::vector<int> &code,
                       std::vector<cv::Point> &points) {
  const cv::Point delta = to - from;
  const int direction = deltaToDirection(delta, neighborhood);
  if (direction >= 0) {
    code.push_back(direction);
    points.push_back(points.back() +
                     chainDirectionOffset(neighborhood, direction));
    return;
  }

  if (neighborhood == NeighborhoodType::N4) {
    std::vector<int> expanded;
    expandDiagonalToN4(delta, expanded);
    for (int step : expanded) {
      code.push_back(step);
      points.push_back(points.back() +
                       chainDirectionOffset(neighborhood, step));
    }
  }
}

ChainCodeTrace buildChainCodeTrace(const std::vector<cv::Point> &contour,
                                   NeighborhoodType neighborhood) {
  ChainCodeTrace trace;
  if (contour.empty())
    return trace;

  trace.startPoint = contour.front();
  trace.tracedPoints.push_back(trace.startPoint);

  for (size_t i = 0; i < contour.size(); ++i) {
    const cv::Point &from = contour[i];
    const cv::Point &to = contour[(i + 1) % contour.size()];
    appendContourStep(from, to, neighborhood, trace.code, trace.tracedPoints);
  }

  trace.derivative = computeChainDerivative(trace.code, neighborhood);
  return trace;
}

void drawPolyline(cv::Mat &image, const std::vector<cv::Point> &points,
                  const cv::Scalar &color) {
  if (image.empty() || points.empty())
    return;
  if (points.size() == 1) {
    cv::circle(image, points.front(), 0, color, 1, cv::LINE_8);
    return;
  }

  for (size_t i = 1; i < points.size(); ++i) {
    cv::line(image, points[i - 1], points[i], color, 1, cv::LINE_8);
  }
}

void logChainCodeTrace(int componentIndex, NeighborhoodType neighborhood,
                       const ChainCodeTrace &trace) {
  const int repeatCount = std::min<int>(2, static_cast<int>(trace.code.size()));
  std::vector<int> closedCode = trace.code;
  std::vector<int> closedDerivative = trace.derivative;
  for (int i = 0; i < repeatCount; ++i) {
    closedCode.push_back(trace.code[i]);
    if (!trace.derivative.empty()) {
      closedDerivative.push_back(trace.derivative[i]);
    }
  }

  std::cout << "[Boundary Code] Component=" << componentIndex
            << " Neighborhood="
            << (neighborhood == NeighborhoodType::N4 ? "N4" : "N8")
            << " Start=(" << trace.startPoint.x << ", " << trace.startPoint.y
            << ")"
            << " Length=" << trace.code.size() << std::endl;
  std::cout << "[Boundary Code] Code=" << joinCode(trace.code) << std::endl;
  std::cout << "[Boundary Code] ClosedCode=" << joinCode(closedCode)
            << std::endl;
  std::cout << "[Boundary Code] Derivative=" << joinCode(trace.derivative)
            << std::endl;
  std::cout << "[Boundary Code] ClosedDerivative=" << joinCode(closedDerivative)
            << std::endl;
}

bool parseChainCodeFile(const std::string &text,
                        ChainCodeFileData &data) {
  std::istringstream iss(text);
  if (!(iss >> data.startPoint.x >> data.startPoint.y))
    return false;
  if (!(iss >> data.declaredLength))
    return false;

  data.code.clear();
  int value = 0;
  while (iss >> value) {
    data.code.push_back(value);
  }

  if (data.declaredLength > 0 &&
      static_cast<int>(data.code.size()) > data.declaredLength) {
    data.code.resize(data.declaredLength);
  }

  return !data.code.empty();
}

std::vector<cv::Point>
reconstructFromChainCode(const cv::Point &startPoint,
                         const std::vector<int> &code,
                         NeighborhoodType neighborhood) {
  std::vector<cv::Point> points;
  points.push_back(startPoint);

  const int directionCount = chainDirectionCount(neighborhood);
  for (int step : code) {
    if (step < 0 || step >= directionCount)
      continue;
    points.push_back(points.back() + chainDirectionOffset(neighborhood, step));
  }

  return points;
}
