#include "morphology_lab7_combined.h"
#include "src/helpers/image_utils.h"
#include "src/helpers/morphology.h"
#include <algorithm>

void morphology_lab7_combined(const cv::Mat &src, OutputImages &outputs,
                              ControlsManager &controls) {
  // Read the iteration slider and keep it inside a safe/visible range for UI
  // experimentation.
  const int iterations =
      std::clamp(controls.getEffective("Iterations (N)"), 1, 15);
  // Convert the source image into a binary mask expected by morphology
  // operators (foreground = 255).
  cv::Mat binary = makeLab7ForegroundMask(src);

  // Apply dilation N times to expand foreground components.
  cv::Mat dilated = applyMorphIterations(binary, iterations, true);
  // Apply erosion N times to shrink foreground components.
  cv::Mat eroded = applyMorphIterations(binary, iterations, false);
  // Apply opening N times (erosion then dilation), useful for removing small
  // bright noise.
  cv::Mat openedN = repeatOpening(binary, iterations);
  // Apply closing N times (dilation then erosion), useful for filling small
  // holes.
  cv::Mat closedN = repeatClosing(binary, iterations);
  // Compute one opening pass as a direct idempotence reference view.
  cv::Mat opened1 = opening8Once(binary);
  // Compute one closing pass as a direct idempotence reference view.
  cv::Mat closed1 = closing8Once(binary);

  // Publish the normalized binary input so all operation outputs can be
  // compared against the same baseline.
  outputs.push_back({"Binary (Lab7)", binary});
  // Publish dilation result after N iterations.
  outputs.push_back({"Dilate xN", dilated});
  // Publish erosion result after N iterations.
  outputs.push_back({"Erode xN", eroded});
  // Publish opening result after N repeated applications.
  outputs.push_back({"Open xN", openedN});
  // Publish closing result after N repeated applications.
  outputs.push_back({"Close xN", closedN});
  // Publish one-pass opening to visually compare with Open xN (idempotence
  // context).
  outputs.push_back({"Open x1", opened1});
  // Publish one-pass closing to visually compare with Close xN (idempotence
  // context).
  outputs.push_back({"Close x1", closed1});
}
