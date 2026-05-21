#include "filter_objects_by_area_orientation.h"
#include "src/helpers/geometry.h"
#include "src/helpers/image_utils.h"
#include "src/helpers/labeling.h"

void filter_objects_by_area_orientation(const cv::Mat &src,
                                        OutputImages &outputs,
                                        ControlsManager &controls) {
  const int thArea = controls.getEffective("TH_area");
  const int phiLowRaw = controls.getEffective("phi_LOW");
  const int phiHighRaw = controls.getEffective("phi_HIGH");
  const double phiLow = normalizeAngleDeg90(static_cast<double>(phiLowRaw));
  const double phiHigh = normalizeAngleDeg90(static_cast<double>(phiHighRaw));

  cv::Mat labels = buildLabelImage(src);
  cv::Mat keepMask(labels.rows, labels.cols, CV_8UC1, cv::Scalar(0));
  cv::Mat keptLabels(labels.rows, labels.cols, CV_32SC1, cv::Scalar(0));

  const auto labelIds = collectLabels(labels);
  for (int labelId : labelIds) {
    cv::Mat objectMask = maskForLabel(labels, labelId);
    ObjectStats stats = computeObjectStats(objectMask, labelId);

    const bool areaOk = stats.area < static_cast<double>(thArea);
    bool orientationOk = false;
    if (phiLow <= phiHigh) {
      orientationOk =
          (stats.orientationDeg >= phiLow && stats.orientationDeg <= phiHigh);
    } else {
      orientationOk =
          (stats.orientationDeg >= phiLow || stats.orientationDeg <= phiHigh);
    }

    if (areaOk && orientationOk) {
      keepMask.setTo(255, objectMask);
      keptLabels.setTo(labelId, objectMask);
    }
  }

  cv::Mat filteredObjects;
  cv::Mat srcBgr = ensureBgr(src);
  filteredObjects = cv::Mat::zeros(srcBgr.size(), CV_8UC3);
  srcBgr.copyTo(filteredObjects, keepMask);

  cv::Mat filteredLabelViz = colorizeLabels(keptLabels);
  outputs.push_back({"Filtered Objects", filteredObjects});
  outputs.push_back({"Filtered Labels", filteredLabelViz});
}
