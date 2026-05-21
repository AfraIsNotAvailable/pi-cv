#include "boundary_reconstruct_from_code.h"
#include "src/common/file/file_utils.h"
#include "src/common/logger/logger.h"
#include "src/common/paths.h"
#include "src/helpers/chain_code.h"
#include <algorithm>
#include <iostream>

void boundary_reconstruct_from_code(const cv::Mat &src, OutputImages &outputs,
                                    ControlsManager &controls) {
  const NeighborhoodType neighborhood = (controls.getRadio("Neighborhood") == 0)
                                            ? NeighborhoodType::N4
                                            : NeighborhoodType::N8;

  const std::string codePath = IMAGE("PI-L6/reconstruct.txt");
  const std::string text = FileUtils::readFile(codePath);
  ChainCodeFileData data;
  if (text.empty() || !parseChainCodeFile(text, data)) {
    cv::Mat fallback(src.rows > 0 ? src.rows : 256,
                     src.cols > 0 ? src.cols : 256, CV_8UC3,
                     cv::Scalar(255, 255, 255));
    cv::putText(fallback, "Failed to parse chain-code file", cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2,
                cv::LINE_AA);
    outputs.push_back({"Reconstructed Boundary", fallback});
    ERROR("Failed to parse chain-code file: {}", codePath);
    return;
  }

  std::vector<cv::Point> points =
      reconstructFromChainCode(data.startPoint, data.code, neighborhood);
  if (points.empty()) {
    cv::Mat fallback(src.rows > 0 ? src.rows : 256,
                     src.cols > 0 ? src.cols : 256, CV_8UC3,
                     cv::Scalar(255, 255, 255));
    cv::putText(fallback, "Empty chain code", cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2,
                cv::LINE_AA);
    outputs.push_back({"Reconstructed Boundary", fallback});
    return;
  }

  int minX = points.front().x;
  int maxX = points.front().x;
  int minY = points.front().y;
  int maxY = points.front().y;
  for (const auto &p : points) {
    minX = std::min(minX, p.x);
    minY = std::min(minY, p.y);
    maxX = std::max(maxX, p.x);
    maxY = std::max(maxY, p.y);
  }

  const int margin = 20;
  const int width = std::max(1, maxX - minX + 1 + 2 * margin);
  const int height = std::max(1, maxY - minY + 1 + 2 * margin);
  cv::Mat reconstructed(height, width, CV_8UC3, cv::Scalar(255, 255, 255));

  std::vector<cv::Point> shifted;
  shifted.reserve(points.size());
  const cv::Point offset(margin - minX, margin - minY);
  for (const auto &p : points) {
    shifted.push_back(p + offset);
  }

  drawPolyline(reconstructed, shifted, cv::Scalar(255, 0, 255));
  cv::drawMarker(reconstructed, shifted.front(), cv::Scalar(0, 255, 0),
                 cv::MARKER_CROSS, 14, 2, cv::LINE_AA);

  std::cout << "[Boundary Reconstruct] File=" << codePath << " Neighborhood="
            << (neighborhood == NeighborhoodType::N4 ? "N4" : "N8")
            << " Start=(" << data.startPoint.x << ", " << data.startPoint.y
            << ")"
            << " DeclaredLength=" << data.declaredLength
            << " ParsedLength=" << data.code.size() << std::endl;
  std::cout << "[Boundary Reconstruct] Code=" << joinCode(data.code)
            << std::endl;

  outputs.push_back({"Reconstructed Boundary", reconstructed});
}
