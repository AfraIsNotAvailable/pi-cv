#include "geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>

double normalizeAngleDeg90(double deg) {
  while (deg < -90.0)
    deg += 180.0;
  while (deg >= 90.0)
    deg -= 180.0;
  return deg;
}

ObjectStats computeObjectStats(const cv::Mat &mask, int label) {
  ObjectStats stats;
  stats.label = label;

  cv::Moments moments = cv::moments(mask, true);
  stats.area = moments.m00;
  if (stats.area <= 0.0)
    return stats;

  stats.centroid =
      cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);

  const double mu20 = moments.mu20 / moments.m00;
  const double mu02 = moments.mu02 / moments.m00;
  const double mu11 = moments.mu11 / moments.m00;

  const double angleRad = 0.5 * std::atan2(2.0 * mu11, mu20 - mu02);
  stats.orientationDeg = normalizeAngleDeg90(angleRad * 180.0 / CV_PI);

  const double term = std::sqrt(
      std::max(0.0, 4.0 * mu11 * mu11 + (mu20 - mu02) * (mu20 - mu02)));
  const double lambda1 = std::max(0.0, (mu20 + mu02 + term) * 0.5);
  const double lambda2 = std::max(0.0, (mu20 + mu02 - term) * 0.5);

  stats.majorAxisLength = 4.0 * std::sqrt(lambda1);
  stats.minorAxisLength = 4.0 * std::sqrt(lambda2);
  stats.aspectRatio = (stats.minorAxisLength > 1e-9)
                          ? (stats.majorAxisLength / stats.minorAxisLength)
                          : std::numeric_limits<double>::infinity();

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
  for (const auto &contour : contours) {
    stats.perimeter += cv::arcLength(contour, true);
  }
  if (stats.perimeter > 1e-9) {
    stats.thinnessRatio =
        (4.0 * CV_PI * stats.area) / (stats.perimeter * stats.perimeter);
  }

  return stats;
}

std::vector<int> computeRowProjection(const cv::Mat &mask) {
  std::vector<int> projection(mask.rows, 0);
  for (int y = 0; y < mask.rows; ++y) {
    const uchar *row = mask.ptr<uchar>(y);
    int count = 0;
    for (int x = 0; x < mask.cols; ++x) {
      if (row[x] != 0)
        ++count;
    }
    projection[y] = count;
  }
  return projection;
}

std::vector<int> computeColProjection(const cv::Mat &mask) {
  std::vector<int> projection(mask.cols, 0);
  for (int y = 0; y < mask.rows; ++y) {
    const uchar *row = mask.ptr<uchar>(y);
    for (int x = 0; x < mask.cols; ++x) {
      if (row[x] != 0)
        ++projection[x];
    }
  }
  return projection;
}

void drawCombinedXYProjections(cv::Mat &canvas,
                               const std::vector<int> &xProjection,
                               const std::vector<int> &yProjection,
                               int sourceImageHeight) {
  if (canvas.empty() || xProjection.empty() || yProjection.empty())
    return;

  const int leftPad = std::max(40, canvas.cols / 15);
  const int rightPad = std::max(20, canvas.cols / 20);
  const int topPad = std::max(20, canvas.rows / 15);
  const int bottomPad = std::max(35, canvas.rows / 10);

  const int axisW = std::max(20, canvas.cols - leftPad - rightPad);
  const int axisH = std::max(20, canvas.rows - topPad - bottomPad);

  const int originX = leftPad;
  const int originY = topPad + axisH;

  cv::line(canvas, cv::Point(originX, originY),
           cv::Point(originX + axisW, originY), cv::Scalar(255, 255, 255), 2,
           cv::LINE_AA);
  cv::line(canvas, cv::Point(originX, originY), cv::Point(originX, topPad),
           cv::Scalar(255, 255, 255), 2, cv::LINE_AA);

  const int axisMax = std::max(1, sourceImageHeight);

  for (int dx = 0; dx <= axisW; ++dx) {
    const int idx =
        static_cast<int>((static_cast<long long>(dx) *
                          static_cast<long long>(xProjection.size() - 1)) /
                         std::max(1, axisW));
    const int value = xProjection[idx];
    const int clamped = std::clamp(value, 0, axisMax);
    const int height = static_cast<int>(
        (static_cast<double>(clamped) / static_cast<double>(axisMax)) *
        static_cast<double>(axisH));
    cv::line(canvas, cv::Point(originX + dx, originY),
             cv::Point(originX + dx, originY - height), cv::Scalar(0, 150, 255),
             1, cv::LINE_8);
  }

  for (int dy = 0; dy <= axisH; ++dy) {
    const int idxForward =
        static_cast<int>((static_cast<long long>(dy) *
                          static_cast<long long>(yProjection.size() - 1)) /
                         std::max(1, axisH));
    const int idx = static_cast<int>(yProjection.size() - 1) - idxForward;
    const int value = yProjection[idx];
    const int clamped = std::clamp(value, 0, axisMax);
    const int width = static_cast<int>(
        (static_cast<double>(clamped) / static_cast<double>(axisMax)) *
        static_cast<double>(axisW));
    const int y = originY - dy;
    cv::line(canvas, cv::Point(originX, y), cv::Point(originX + width, y),
             cv::Scalar(0, 255, 120), 1, cv::LINE_8);
  }

  cv::putText(canvas, "X", cv::Point(originX + axisW + 8, originY + 5),
              cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 150, 255), 2,
              cv::LINE_AA);
  cv::putText(canvas, "Y", cv::Point(originX - 15, topPad - 4),
              cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 120), 2,
              cv::LINE_AA);
  cv::putText(canvas, "Combined X/Y Projections",
              cv::Point(originX + 8, topPad - 6), cv::FONT_HERSHEY_SIMPLEX,
              0.55, cv::Scalar(220, 220, 220), 1, cv::LINE_AA);
}
