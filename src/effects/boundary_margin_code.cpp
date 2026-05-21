#include "boundary_margin_code.h"
#include "src/helpers/chain_code.h"
#include "src/helpers/image_utils.h"
#include <iostream>

void boundary_margin_code(const cv::Mat &src, OutputImages &outputs,
                          ControlsManager &controls) {
  const NeighborhoodType neighborhood = (controls.getRadio("Neighborhood") == 0)
                                            ? NeighborhoodType::N4
                                            : NeighborhoodType::N8;

  cv::Mat mask = makeBlackForegroundMask(src);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

  cv::Mat annotated = ensureBgr(src);
  int componentIndex = 0;

  for (const auto &contour : contours) {
    if (contour.size() < 2)
      continue;

    ChainCodeTrace trace = buildChainCodeTrace(contour, neighborhood);
    if (trace.code.empty() || trace.tracedPoints.size() < 2)
      continue;

    ++componentIndex;
    drawPolyline(annotated, trace.tracedPoints, cv::Scalar(255, 0, 255));
    logChainCodeTrace(componentIndex, neighborhood, trace);
  }

  if (componentIndex == 0) {
    cv::putText(annotated, "No black foreground components detected",
                cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    std::cout << "[Boundary Code] No black foreground components detected."
              << std::endl;
  }

  outputs.push_back({"Boundary Margin", annotated});
}
