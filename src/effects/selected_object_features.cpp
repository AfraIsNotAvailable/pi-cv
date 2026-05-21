#include "selected_object_features.h"
#include "src/helpers/geometry.h"
#include "src/helpers/global_state.h"
#include "src/helpers/image_utils.h"
#include "src/helpers/labeling.h"
#include <cmath>
#include <iostream>

void selected_object_features(const cv::Mat &src, OutputImages &outputs,
                              ControlsManager & /*controls*/) {
  cv::Mat labels = buildLabelImage(src);
  cv::Mat labelViz = colorizeLabels(labels);
  outputs.push_back({"Labels", labelViz});

  cv::Mat annotated = ensureBgr(src);
  cv::Mat projectionsView(src.rows, src.cols, CV_8UC3, cv::Scalar(0, 0, 0));

  int selectedLabel = -1;
  if (g_selectionState.hasClick) {
    const cv::Point p = g_selectionState.imagePoint;
    if (p.x >= 0 && p.y >= 0 && p.x < labels.cols && p.y < labels.rows) {
      selectedLabel = labels.at<int>(p.y, p.x);
    }
  }

  if (selectedLabel <= 0) {
    cv::putText(annotated, "Click on a foreground object in Source panel",
                cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    cv::putText(annotated, "Background clicks are ignored", cv::Point(20, 60),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 180, 255), 2,
                cv::LINE_AA);

    cv::putText(projectionsView, "No object selected", cv::Point(20, 35),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2,
                cv::LINE_AA);

    if (g_selectionState.dirty) {
      std::cout << "[Selected Object] No foreground object selected."
                << std::endl;
      g_selectionState.dirty = false;
    }

    outputs.push_back({"Selected Object", annotated});
    outputs.push_back({"Projections", projectionsView});
    return;
  }

  cv::Mat selectedMask = maskForLabel(labels, selectedLabel);
  ObjectStats stats = computeObjectStats(selectedMask, selectedLabel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(selectedMask, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_NONE);
  for (const auto &contour : contours) {
    for (const auto &point : contour) {
      cv::circle(annotated, point, 0, cv::Scalar(0, 0, 255), 1, cv::LINE_8);
    }
  }

  cv::Point center(static_cast<int>(std::lround(stats.centroid.x)),
                   static_cast<int>(std::lround(stats.centroid.y)));
  cv::drawMarker(annotated, center, cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 16,
                 2, cv::LINE_AA);

  const double angleRad = stats.orientationDeg * CV_PI / 180.0;
  const cv::Point2d direction(std::cos(angleRad), std::sin(angleRad));
  const double halfLen = std::max(20.0, stats.majorAxisLength * 0.75);
  cv::Point p1(
      static_cast<int>(std::lround(stats.centroid.x - direction.x * halfLen)),
      static_cast<int>(std::lround(stats.centroid.y - direction.y * halfLen)));
  cv::Point p2(
      static_cast<int>(std::lround(stats.centroid.x + direction.x * halfLen)),
      static_cast<int>(std::lround(stats.centroid.y + direction.y * halfLen)));
  cv::line(annotated, p1, p2, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);

  cv::putText(annotated, "Label: " + std::to_string(selectedLabel),
              cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 0.65,
              cv::Scalar(0, 0, 0), 2, cv::LINE_AA);

  const auto rowProjection = computeRowProjection(selectedMask);
  const auto colProjection = computeColProjection(selectedMask);
  drawCombinedXYProjections(projectionsView, colProjection, rowProjection,
                            src.rows);

  if (g_selectionState.dirty || g_selectionState.label != selectedLabel) {
    std::cout << "[Selected Object] Label=" << selectedLabel
              << " Area=" << stats.area << " Centroid=(" << stats.centroid.x
              << ", " << stats.centroid.y << ")"
              << " OrientationDeg=" << stats.orientationDeg
              << " Perimeter=" << stats.perimeter
              << " ThinnessRatio=" << stats.thinnessRatio
              << " AspectRatio=" << stats.aspectRatio << std::endl;
    g_selectionState.dirty = false;
  }
  g_selectionState.label = selectedLabel;

  outputs.push_back({"Selected Object", annotated});
  outputs.push_back({"Projections", projectionsView});
}
