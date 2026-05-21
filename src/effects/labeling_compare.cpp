#include "labeling_compare.h"
#include "src/helpers/image_utils.h"
#include "src/helpers/labeling.h"

void labeling_compare(const cv::Mat &src, OutputImages &outputs,
                      ControlsManager &controls) {
  cv::Mat binary = makeBinaryForeground(src);
  NeighborhoodType nType = (controls.getRadio("Neighborhood") == 0)
                               ? NeighborhoodType::N4
                               : NeighborhoodType::N8;
  bool useDfs = (controls.getRadio("Traversal") == 1);

  cv::Mat traversalLabels = labelComponentsByTraversal(binary, nType, useDfs);
  TwoPassLabelingResult twoPass = labelComponentsTwoPass(binary, nType);

  outputs.push_back({"Binary", binary});
  outputs.push_back({"Traversal Labels", colorizeLabels(traversalLabels)});
  outputs.push_back(
      {"Two-Pass First Pass", colorizeLabels(twoPass.firstPassLabels)});
  outputs.push_back({"Two-Pass Final", colorizeLabels(twoPass.finalLabels)});
}
