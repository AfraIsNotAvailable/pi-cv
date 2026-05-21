#include "contour_and_region_fill_lab7.h"
#include "src/helpers/global_state.h"
#include "src/helpers/image_utils.h"
#include "src/helpers/morphology.h"
#include <algorithm>
#include <string>

void contour_and_region_fill_lab7(const cv::Mat &src, OutputImages &outputs,
                                  ControlsManager &controls) {
  // Read and clamp the safety cap for iterative filling to avoid unbounded
  // loops.
  const int maxFillIterations =
      std::clamp(controls.getEffective("Max Fill Iter"), 1, 50000);

  // Build a consistent binary object mask from the input image.
  cv::Mat binary = makeLab7ForegroundMask(src);
  // Extract boundary pixels with beta(A) = A - (A erode B), B being the
  // 8-neighborhood element.
  cv::Mat contourMask = boundaryExtraction8(binary);

  // Start contour visualization from the source image so structure remains
  // recognizable.
  cv::Mat contourView = ensureBgr(src);
  // Paint contour pixels in red for immediate visual separation.
  contourView.setTo(cv::Scalar(0, 0, 255), contourMask);
  // Overlay the exact formula used by this visualization.
  cv::putText(contourView, "Boundary: beta(A)=A-(A erode B), B=N8",
              cv::Point(20, 28), cv::FONT_HERSHEY_SIMPLEX, 0.55,
              cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

  // Prepare a second panel for the region-filling result.
  cv::Mat fillView = ensureBgr(src);
  // Read the latest click from the global selection state.
  cv::Point seed = g_selectionState.imagePoint;
  // Validate that the user clicked and the seed is inside image bounds.
  const bool seedInBounds = g_selectionState.hasClick && seed.x >= 0 &&
                            seed.y >= 0 && seed.x < binary.cols &&
                            seed.y < binary.rows;

  // If no valid click exists, show guidance and return contour + empty fill
  // preview.
  if (!seedInBounds) {
    cv::putText(fillView, "Click inside a hole/background region to start fill",
                cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.65,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    // Always include contour output even when filling cannot start.
    outputs.push_back({"Contour", contourView});
    // Include the instructional fill panel.
    outputs.push_back({"Region Fill", fillView});
    return;
  }

  // Region filling requires a background seed; reject clicks directly on object
  // pixels.
  if (binary.at<uchar>(seed.y, seed.x) != 0) {
    cv::putText(fillView, "Invalid seed: click inside a hole (background)",
                cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.65,
                cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    // Mark the invalid seed so users understand what was selected.
    cv::drawMarker(fillView, seed, cv::Scalar(0, 0, 255),
                   cv::MARKER_TILTED_CROSS, 14, 2, cv::LINE_AA);
    outputs.push_back({"Contour", contourView});
    outputs.push_back({"Region Fill", fillView});
    return;
  }

  // Track iteration count consumed by the iterative fill algorithm.
  int usedIterations = 0;
  // Track whether convergence happened before hitting the maximum allowed
  // iterations.
  bool converged = false;
  // Run morphological region filling: Xk = (Xk-1 dilate B) - A, with B=N8.
  cv::Mat fillMask = morphologicalRegionFill8(binary, seed, maxFillIterations,
                                              usedIterations, converged);
  // Defensive check for invalid/failed fill execution.
  if (fillMask.empty()) {
    cv::putText(fillView, "Fill failed: invalid seed or input mask",
                cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.65,
                cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    outputs.push_back({"Contour", contourView});
    outputs.push_back({"Region Fill", fillView});
    return;
  }

  // Union of original object and filled interior gives final filled object
  // mask.
  cv::Mat filledObject;
  cv::bitwise_or(binary, fillMask, filledObject);

  // Compute just the newly filled pixels for distinct coloring.
  cv::Mat newlyFilled;
  // Invert object mask to isolate background domain.
  cv::Mat objectInverse;
  cv::bitwise_not(binary, objectInverse);
  // Keep only fill pixels that were originally background.
  cv::bitwise_and(fillMask, objectInverse, newlyFilled);

  // Color original object pixels (blue-ish).
  fillView.setTo(cv::Scalar(255, 80, 80), binary);
  // Color newly filled pixels (green-ish).
  fillView.setTo(cv::Scalar(80, 255, 80), newlyFilled);
  // Draw seed marker in yellow for traceability.
  cv::drawMarker(fillView, seed, cv::Scalar(0, 255, 255), cv::MARKER_CROSS, 14,
                 2, cv::LINE_AA);

  // Build user-facing status text that reports convergence or capped execution.
  std::string status =
      converged
          ? ("Fill converged in " + std::to_string(usedIterations) + " steps")
          : ("Fill reached max iterations: " +
             std::to_string(maxFillIterations));
  // Render status text; green when converged, orange when capped.
  cv::putText(fillView, status, cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX,
              0.6,
              converged ? cv::Scalar(0, 255, 120) : cv::Scalar(0, 165, 255), 2,
              cv::LINE_AA);

  // Final outputs for this effect: contour-only view.
  outputs.push_back({"Contour", contourView});
  // Final outputs for this effect: color-coded fill visualization.
  outputs.push_back({"Region Fill", fillView});
  // Final outputs for this effect: binary mask of the fully filled object.
  outputs.push_back({"Filled Mask", filledObject});
}
