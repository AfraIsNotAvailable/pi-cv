#include "rendering.h"
#include "global_state.h"
#include "image_utils.h"
#include "src/common/file/file_utils.h"
#include "src/common/logger/logger.h"
#include "src/common/paths.h"
#include <algorithm>
#include <chrono>

void updateMainWindowTitle(const std::string &effectName) {
  std::string title = std::string(MAIN_WINDOW_NAME);
  if (!effectName.empty()) {
    title += " - " + effectName;
  }
  cv::setWindowTitle(MAIN_WINDOW_NAME, title);
}

bool mapMainWindowPointToSource(const cv::Point &windowPoint,
                                cv::Point &sourcePoint) {
  if (g_renderContext.srcW <= 0 || g_renderContext.srcH <= 0)
    return false;

  if (g_renderContext.cellW <= 0 || g_renderContext.totalCellH <= 0) {
    return false;
  }

  if (windowPoint.x < 0 || windowPoint.y < 0)
    return false;

  const int localX = windowPoint.x % g_renderContext.cellW;
  const int localY = windowPoint.y % g_renderContext.totalCellH;

  if (localY < g_renderContext.labelHeight)
    return false;
  if (localY >= g_renderContext.labelHeight + g_renderContext.cellH)
    return false;

  const int imageY = localY - g_renderContext.labelHeight;

  const double tx = static_cast<double>(localX) /
                    static_cast<double>(std::max(1, g_renderContext.cellW - 1));
  const double ty = static_cast<double>(imageY) /
                    static_cast<double>(std::max(1, g_renderContext.cellH - 1));

  sourcePoint.x =
      std::clamp(static_cast<int>(std::round(
                     tx * static_cast<double>(g_renderContext.srcW - 1))),
                 0, g_renderContext.srcW - 1);
  sourcePoint.y =
      std::clamp(static_cast<int>(std::round(
                     ty * static_cast<double>(g_renderContext.srcH - 1))),
                 0, g_renderContext.srcH - 1);
  return true;
}

void mainWindowMouseCallback(int event, int x, int y, int /*flags*/,
                             void * /*userdata*/) {
  if (event != cv::EVENT_LBUTTONDOWN)
    return;

  cv::Point sourcePoint;
  if (!mapMainWindowPointToSource(cv::Point(x, y), sourcePoint)) {
    return;
  }

  g_selectionState.imagePoint = sourcePoint;
  g_selectionState.hasClick = true;
  g_selectionState.dirty = true;
  g_needsUpdate = true;
}

void renderGrid(const cv::Mat &src, const OutputImages &outputs) {
  if (src.empty())
    return;

  // Total panels: source + all outputs
  const int N = 1 + static_cast<int>(outputs.size());
  if (N == 0)
    return;

  // Compute grid dimensions
  const int gridCols =
      static_cast<int>(std::ceil(std::sqrt(static_cast<double>(N))));
  const int gridRows =
      static_cast<int>(std::ceil(static_cast<double>(N) / gridCols));

  // Determine uniform cell size (based on the largest image)
  static constexpr int LABEL_H = 30;

  int cellW = src.cols;
  int cellH = src.rows;
  for (const auto &[name, mat] : outputs) {
    if (!mat.empty()) {
      cellW = std::max(cellW, mat.cols);
      cellH = std::max(cellH, mat.rows);
    }
  }

  const int totalCellH = cellH + LABEL_H;
  cv::Mat canvas(gridRows * totalCellH, gridCols * cellW, CV_8UC3,
                 cv::Scalar(40, 40, 40));

  g_renderContext.sourcePanelX = 0;
  g_renderContext.sourcePanelY = 0;
  g_renderContext.sourcePanelW = cellW;
  g_renderContext.sourcePanelH = cellH;
  g_renderContext.cellW = cellW;
  g_renderContext.cellH = cellH;
  g_renderContext.totalCellH = totalCellH;
  g_renderContext.labelHeight = LABEL_H;
  g_renderContext.srcW = src.cols;
  g_renderContext.srcH = src.rows;

  // Helper: place one panel at grid position
  auto placePanel = [&](int idx, const std::string &label,
                        const cv::Mat &image) {
    const int row = idx / gridCols;
    const int col = idx % gridCols;
    const int x0 = col * cellW;
    const int y0 = row * totalCellH;

    // Draw label bar
    cv::rectangle(canvas, cv::Point(x0, y0),
                  cv::Point(x0 + cellW, y0 + LABEL_H), cv::Scalar(30, 30, 30),
                  cv::FILLED);
    cv::putText(canvas, label, cv::Point(x0 + 6, y0 + 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(220, 220, 220), 1,
                cv::LINE_AA);

    // Resize image to cell and place it
    if (!image.empty()) {
      cv::Mat bgr = ensureBgr(image);
      cv::Mat resized;
      cv::resize(bgr, resized, cv::Size(cellW, cellH), 0.0, 0.0,
                 cv::INTER_AREA);
      resized.copyTo(canvas(cv::Rect(x0, y0 + LABEL_H, cellW, cellH)));
    }
  };

  // Panel 0: source image
  placePanel(0, "Source", src);

  // Remaining panels: outputs in insertion order
  for (int i = 0; i < static_cast<int>(outputs.size()); ++i) {
    placePanel(i + 1, outputs[i].first, outputs[i].second);
  }

  cv::imshow(MAIN_WINDOW_NAME, canvas);
}

void saveAllOutputs(const OutputImages &outputs) {
  if (outputs.empty()) {
    WARN("No outputs to save.");
    return;
  }
  for (const auto &[name, mat] : outputs) {
    if (mat.empty())
      continue;

    // Build a filename: timestamp_name.bmp
    auto secSinceEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Sanitize the name for use in a filename
    std::string safeName = name;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');

    std::string path = std::string(ExportFolder) + "/" +
                       std::to_string(secSinceEpoch) + "_" + safeName + ".bmp";
    FileUtils::saveImage(mat, path);
  }
}

bool loadImage(const std::string &path, cv::Mat &img) {
  cv::Mat newImg = FileUtils::readImage(path, cv::IMREAD_COLOR);
  if (newImg.empty()) {
    ERROR("Failed to load image: {}", path);
    return false;
  }
  img = newImg;
  g_selectionState = SelectionState{};
  g_needsUpdate = true;
  DEBUG("Image loaded: {} ({}x{})", path, img.cols, img.rows);
  return true;
}

bool hasQtBackend() {
  const std::string info = cv::getBuildInformation();
  const bool hasGuiSection = info.find("GUI:") != std::string::npos;
  const bool hasQt = info.find("QT") != std::string::npos;
  return hasGuiSection && hasQt;
}
