#include "common.h"
#include "controls_manager.h"
#include "opencv2/opencv.hpp"
#include "slider.h"
#include "spaces.h"
#include "src/effects/assignment.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stack>

static constexpr const char *MAIN_WINDOW_NAME = "Image Processing";

enum class NeighborhoodType { N4 = 0, N8 = 1 };

static const std::array<cv::Point, 4> kNeighbors4 = {
    cv::Point(1, 0), cv::Point(-1, 0), cv::Point(0, 1), cv::Point(0, -1)};

static const std::array<cv::Point, 8> kNeighbors8 = {
    cv::Point(1, 0), cv::Point(-1, 0), cv::Point(0, 1),  cv::Point(0, -1),
    cv::Point(1, 1), cv::Point(1, -1), cv::Point(-1, 1), cv::Point(-1, -1)};

static const std::array<cv::Point, 4> kChainDirections4 = {
    cv::Point(1, 0), cv::Point(0, -1), cv::Point(-1, 0), cv::Point(0, 1)};

static const std::array<cv::Point, 8> kChainDirections8 = {
    cv::Point(1, 0),  cv::Point(1, -1), cv::Point(0, -1), cv::Point(-1, -1),
    cv::Point(-1, 0), cv::Point(-1, 1), cv::Point(0, 1),  cv::Point(1, 1)};

/**
 * @brief Runtime geometry required to map clicks from the rendered grid back to
 * source-image coordinates.
 */
struct RenderContext {
  /** @brief X origin of the source panel in the composed canvas. */
  int sourcePanelX = 0;
  /** @brief Y origin of the source panel in the composed canvas. */
  int sourcePanelY = 0;
  /** @brief Width of the source panel area in the canvas. */
  int sourcePanelW = 0;
  /** @brief Height of the source panel area in the canvas. */
  int sourcePanelH = 0;
  /** @brief Width of each grid cell used for rendering. */
  int cellW = 0;
  /** @brief Height of each image region inside a grid cell. */
  int cellH = 0;
  /** @brief Full cell height including image + label bar. */
  int totalCellH = 0;
  /** @brief Label bar height at the top of each cell. */
  int labelHeight = 30;
  /** @brief Source image width used for coordinate mapping. */
  int srcW = 0;
  /** @brief Source image height used for coordinate mapping. */
  int srcH = 0;
};

/**
 * @brief Geometric descriptors for one labeled object.
 */
struct ObjectStats {
  /** @brief Label value of the analyzed object. */
  int label = 0;
  /** @brief Object area in pixels. */
  double area = 0.0;
  /** @brief Center of mass in source-image coordinates. */
  cv::Point2d centroid{0.0, 0.0};
  /** @brief Principal orientation angle in degrees (normalized to [-90, 90)).
   */
  double orientationDeg = 0.0;
  /** @brief Perimeter estimated from external contour length. */
  double perimeter = 0.0;
  /** @brief Thinness ratio computed as 4*pi*area/perimeter^2. */
  double thinnessRatio = 0.0;
  /** @brief Aspect ratio estimated as major/minor axis lengths ratio. */
  double aspectRatio = 0.0;
  /** @brief Major axis length estimated from second-order moments. */
  double majorAxisLength = 0.0;
  /** @brief Minor axis length estimated from second-order moments. */
  double minorAxisLength = 0.0;
};

/**
 * @brief Click-based selection state shared between callback and effects.
 */
struct SelectionState {
  /** @brief Last clicked point mapped to source-image coordinates. */
  cv::Point imagePoint{-1, -1};
  /** @brief Last selected positive label, or -1 when nothing is selected. */
  int label = -1;
  /** @brief True if the user performed at least one valid click. */
  bool hasClick = false;
  /** @brief Marks whether selection-derived logs/overlays need refreshing. */
  bool dirty = true;
};

/** @brief Global render context updated by renderGrid() every frame. */
static RenderContext g_renderContext;
/** @brief Global object-selection state updated by mouse callback and effects.
 */
static SelectionState g_selectionState;

/**
 * @brief Updates the window title with the currently active effect name.
 * @param effectName Name of the selected processing effect.
 */
static void updateMainWindowTitle(const std::string &effectName) {
  std::string title = std::string(MAIN_WINDOW_NAME);
  if (!effectName.empty()) {
    title += " - " + effectName;
  }
  cv::setWindowTitle(MAIN_WINDOW_NAME, title);
}

/** @brief Global UI invalidation flag toggled by controls and user input. */
static bool g_needsUpdate = true;

/**
 * @brief Returns a 3-channel BGR version of the input image.
 * @param image Input image (grayscale or BGR).
 * @return BGR clone or converted image.
 */
static cv::Mat ensureBgr(const cv::Mat &image);

/**
 * @brief Normalizes an angle to the interval [-90, 90).
 * @param deg Input angle in degrees.
 * @return Normalized angle in degrees.
 */
static double normalizeAngleDeg90(double deg) {
  while (deg < -90.0)
    deg += 180.0;
  while (deg >= 90.0)
    deg -= 180.0;
  return deg;
}

/**
 * @brief Converts any input image to single-channel 8-bit grayscale.
 * @param src Input image.
 * @return Grayscale image of type CV_8UC1.
 */
static cv::Mat toGray8U(const cv::Mat &src) {
  cv::Mat gray;
  if (src.channels() == 1) {
    if (src.type() == CV_8UC1) {
      gray = src;
    } else {
      double minv = 0.0;
      double maxv = 0.0;
      cv::minMaxLoc(src, &minv, &maxv);
      const double span = std::max(1e-9, maxv - minv);
      src.convertTo(gray, CV_8UC1, 255.0 / span, -255.0 * minv / span);
    }
  } else {
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  }
  return gray;
}

/**
 * @brief Creates labels from a grayscale image using thresholding + connected
 * components.
 * @param srcGray8u Input grayscale image (CV_8UC1).
 * @return Label image (CV_32SC1), with 0 as background.
 */
static cv::Mat buildLabelsByConnectedComponents(const cv::Mat &srcGray8u) {
  cv::Mat binary;
  double minv = 0.0;
  double maxv = 0.0;
  cv::minMaxLoc(srcGray8u, &minv, &maxv);

  if (maxv <= 1.0) {
    cv::threshold(srcGray8u, binary, 0, 255, cv::THRESH_BINARY);
  } else {
    cv::threshold(srcGray8u, binary, 0, 255,
                  cv::THRESH_BINARY | cv::THRESH_OTSU);
  }

  cv::Mat labels;
  cv::connectedComponents(binary, labels, 8, CV_32S);
  return labels;
}

/**
 * @brief Interprets grayscale pixel values directly as labels.
 * @param gray8u Input grayscale label map.
 * @return Label image in CV_32SC1.
 */
static cv::Mat buildLabelsFromGrayValues(const cv::Mat &gray8u) {
  cv::Mat labels(gray8u.rows, gray8u.cols, CV_32SC1, cv::Scalar(0));
  for (int y = 0; y < gray8u.rows; ++y) {
    const uchar *srcRow = gray8u.ptr<uchar>(y);
    int *dstRow = labels.ptr<int>(y);
    for (int x = 0; x < gray8u.cols; ++x) {
      dstRow[x] = static_cast<int>(srcRow[x]);
    }
  }
  return labels;
}

/**
 * @brief Converts a pseudo-colored label map to compact integer labels.
 * @param bgr Input BGR image; black is treated as background.
 * @return Label image in CV_32SC1.
 */
static cv::Mat buildLabelsFromColorValues(const cv::Mat &bgr) {
  cv::Mat labels(bgr.rows, bgr.cols, CV_32SC1, cv::Scalar(0));
  std::map<int, int> colorToLabel;
  int nextLabel = 1;

  for (int y = 0; y < bgr.rows; ++y) {
    const cv::Vec3b *srcRow = bgr.ptr<cv::Vec3b>(y);
    int *dstRow = labels.ptr<int>(y);
    for (int x = 0; x < bgr.cols; ++x) {
      const cv::Vec3b color = srcRow[x];
      if (color == cv::Vec3b(0, 0, 0)) {
        dstRow[x] = 0;
        continue;
      }

      const int key = (static_cast<int>(color[0]) << 16) |
                      (static_cast<int>(color[1]) << 8) |
                      static_cast<int>(color[2]);
      auto it = colorToLabel.find(key);
      if (it == colorToLabel.end()) {
        colorToLabel[key] = nextLabel;
        dstRow[x] = nextLabel;
        ++nextLabel;
      } else {
        dstRow[x] = it->second;
      }
    }
  }

  return labels;
}

/**
 * @brief Builds a robust label image from various source formats.
 * @param src Input source image.
 * @return Label image (CV_32SC1).
 */
static cv::Mat buildLabelImage(const cv::Mat &src) {
  if (src.type() == CV_32SC1) {
    return src.clone();
  }

  if (src.channels() == 1) {
    cv::Mat gray = toGray8U(src);

    bool seen[256] = {false};
    int uniqueNonZero = 0;
    for (int y = 0; y < gray.rows; ++y) {
      const uchar *row = gray.ptr<uchar>(y);
      for (int x = 0; x < gray.cols; ++x) {
        const uchar value = row[x];
        if (value == 0 || seen[value])
          continue;
        seen[value] = true;
        ++uniqueNonZero;
      }
    }

    const bool looksLikeLabeled = (uniqueNonZero >= 2 && uniqueNonZero <= 64);
    if (looksLikeLabeled) {
      return buildLabelsFromGrayValues(gray);
    }

    return buildLabelsByConnectedComponents(gray);
  }

  cv::Mat bgr = ensureBgr(src);
  std::set<int> uniqueColors;
  for (int y = 0; y < bgr.rows; ++y) {
    const cv::Vec3b *row = bgr.ptr<cv::Vec3b>(y);
    for (int x = 0; x < bgr.cols; ++x) {
      const cv::Vec3b color = row[x];
      if (color == cv::Vec3b(0, 0, 0))
        continue;
      const int key = (static_cast<int>(color[0]) << 16) |
                      (static_cast<int>(color[1]) << 8) |
                      static_cast<int>(color[2]);
      uniqueColors.insert(key);
      if (uniqueColors.size() > 128)
        break;
    }
    if (uniqueColors.size() > 128)
      break;
  }

  if (uniqueColors.size() >= 2 && uniqueColors.size() <= 128) {
    return buildLabelsFromColorValues(bgr);
  }

  return buildLabelsByConnectedComponents(toGray8U(bgr));
}

static cv::Mat makeBinaryForeground(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);
  cv::Mat binary;
  cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  return binary;
}

static cv::Mat labelComponentsByTraversal(const cv::Mat &binary,
                                          NeighborhoodType neighborhood,
                                          bool useDfsStack = false) {
  CV_Assert(binary.type() == CV_8UC1);

  cv::Mat labels(binary.rows, binary.cols, CV_32SC1, cv::Scalar(0));
  int currentLabel = 0;

  auto inside = [&](int y, int x) {
    return y >= 0 && y < binary.rows && x >= 0 && x < binary.cols;
  };

  for (int y = 0; y < binary.rows; y++) {
    const uchar *binRow = binary.ptr<uchar>(y);
    for (int x = 0; x < binary.cols; x++) {
      if (binRow[x] == 0)
        continue;
      if (labels.at<int>(y, x) != 0)
        continue;

      ++currentLabel;

      if (!useDfsStack) {
        std::queue<cv::Point> q;
        q.push(cv::Point(x, y));
        labels.at<int>(y, x) = currentLabel;

        while (!q.empty()) {
          cv::Point p = q.front();
          q.pop();

          if (neighborhood == NeighborhoodType::N4) {
            for (const auto &d : kNeighbors4) {
              int nx = p.x + d.x;
              int ny = p.y + d.y;
              if (!inside(ny, nx))
                continue;
              if (binary.at<uchar>(ny, nx) == 0)
                continue;
              if (labels.at<int>(ny, nx) != 0)
                continue;

              labels.at<int>(ny, nx) = currentLabel;
              q.push(cv::Point(nx, ny));
            }
          } else {
            for (const auto &d : kNeighbors8) {
              int nx = p.x + d.x;
              int ny = p.y + d.y;
              if (!inside(ny, nx))
                continue;
              if (binary.at<uchar>(ny, nx) == 0)
                continue;
              if (labels.at<int>(ny, nx) != 0)
                continue;

              labels.at<int>(ny, nx) = currentLabel;
              q.push(cv::Point(nx, ny));
            }
          }
        }
      } else {
        std::stack<cv::Point> st;
        st.push(cv::Point(x, y));
        labels.at<int>(y, x) = currentLabel;

        while (!st.empty()) {
          cv::Point p = st.top();
          st.pop();

          if (neighborhood == NeighborhoodType::N4) {
            for (const auto &d : kNeighbors4) {
              int nx = p.x + d.x;
              int ny = p.y + d.y;
              if (!inside(ny, nx))
                continue;
              if (binary.at<uchar>(ny, nx) == 0)
                continue;
              if (labels.at<int>(ny, nx) != 0)
                continue;

              labels.at<int>(ny, nx) = currentLabel;
              st.push(cv::Point(nx, ny));
            }
          } else {
            for (const auto &d : kNeighbors8) {
              int nx = p.x + d.x;
              int ny = p.y + d.y;
              if (!inside(ny, nx))
                continue;
              if (binary.at<uchar>(ny, nx) == 0)
                continue;
              if (labels.at<int>(ny, nx) != 0)
                continue;

              labels.at<int>(ny, nx) = currentLabel;
              st.push(cv::Point(nx, ny));
            }
          }
        }
      }
    }
  }
  return labels;
}

struct TwoPassLabelingResult {
  cv::Mat firstPassLabels;
  cv::Mat finalLabels;
};

static void collectPreviousNeighborLabels(const cv::Mat &labels, int x, int y,
                                          NeighborhoodType neighborhood,
                                          std::vector<int> &outLabels) {
  outLabels.clear();

  auto pushIfPositive = [&](int nx, int ny) {
    if (nx < 0 || ny < 0 || nx >= labels.cols || ny >= labels.rows)
      return;
    int v = labels.at<int>(ny, nx);
    if (v > 0)
      outLabels.push_back(v);
  };

  // W, N
  pushIfPositive(x - 1, y);
  pushIfPositive(x, y - 1);

  if (neighborhood == NeighborhoodType::N8) {
    pushIfPositive(x - 1, y - 1);
    pushIfPositive(x + 1, y - 1);
  }
}

static TwoPassLabelingResult
labelComponentsTwoPass(const cv::Mat &binary, NeighborhoodType neighborhood) {
  CV_Assert(binary.type() == CV_8UC1);

  cv::Mat firstPass(binary.rows, binary.cols, CV_32SC1, cv::Scalar(0));

  std::vector<int> parent(1, 0);

  auto makeSet = [&]() -> int {
    int id = static_cast<int>(parent.size());
    parent.push_back(id);
    return id;
  };

  std::function<int(int)> findRoot = [&](int a) -> int {
    if (parent[a] != a)
      parent[a] = findRoot(parent[a]);
    return parent[a];
  };

  auto unite = [&](int a, int b) {
    int ra = findRoot(a);
    int rb = findRoot(b);
    if (ra == rb)
      return;
    if (ra < rb)
      parent[rb] = ra;
    else
      parent[ra] = rb;
  };

  std::vector<int> neighbors;
  neighbors.reserve(4);

  for (int y = 0; y < binary.rows; y++) {
    for (int x = 0; x < binary.cols; x++) {
      if (binary.at<uchar>(y, x) == 0)
        continue;

      collectPreviousNeighborLabels(firstPass, x, y, neighborhood, neighbors);

      if (neighbors.empty()) {
        firstPass.at<int>(y, x) = makeSet();
      } else {
        int minLabel = *std::min_element(neighbors.begin(), neighbors.end());
        firstPass.at<int>(y, x) = minLabel;
        for (int lb : neighbors)
          unite(minLabel, lb);
      }
    }
  }

  cv::Mat finalLabels(binary.rows, binary.cols, CV_32SC1, cv::Scalar(0));

  std::map<int, int> rootToCompact;
  int nextCompact = 1;

  for (int y = 0; y < firstPass.rows; y++) {
    for (int x = 0; x < firstPass.cols; x++) {
      int v = firstPass.at<int>(y, x);
      if (v == 0)
        continue;

      int root = findRoot(v);
      auto it = rootToCompact.find(root);
      if (it == rootToCompact.end()) {
        rootToCompact[root] = nextCompact;
        finalLabels.at<int>(y, x) = nextCompact;
        nextCompact++;
      } else {
        finalLabels.at<int>(y, x) = it->second;
      }
    }
  }
  return {firstPass, finalLabels};
}

/**
 * @brief Extracts all positive unique labels from a label image.
 * @param labels Label image (CV_32SC1).
 * @return Sorted vector of labels > 0.
 */
static std::vector<int> collectLabels(const cv::Mat &labels) {
  std::set<int> unique;
  for (int y = 0; y < labels.rows; ++y) {
    const int *row = labels.ptr<int>(y);
    for (int x = 0; x < labels.cols; ++x) {
      if (row[x] > 0)
        unique.insert(row[x]);
    }
  }
  return {unique.begin(), unique.end()};
}

/**
 * @brief Builds a binary mask for a single label value.
 * @param labels Label image (CV_32SC1).
 * @param label Target label value.
 * @return Binary mask (CV_8UC1), 255 where label matches.
 */
static cv::Mat maskForLabel(const cv::Mat &labels, int label) {
  cv::Mat mask;
  cv::compare(labels, label, mask, cv::CMP_EQ);
  return mask;
}

/**
 * @brief Converts a grayscale/binary image with black foreground on white
 * background into a binary mask.
 * @param src Input source image.
 * @return Binary mask with foreground in 255.
 */
static cv::Mat makeBlackForegroundMask(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);
  cv::Mat mask;
  cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
  return mask;
}

/**
 * @brief Counts non-zero border pixels for a binary mask.
 * @param binary Binary image (CV_8UC1).
 * @return Number of non-zero pixels that lie on the outer border.
 */
static int countBorderForeground(const cv::Mat &binary) {
  CV_Assert(binary.type() == CV_8UC1);
  if (binary.empty())
    return 0;

  int count = 0;
  for (int x = 0; x < binary.cols; ++x) {
    if (binary.at<uchar>(0, x) != 0)
      ++count;
    if (binary.rows > 1 && binary.at<uchar>(binary.rows - 1, x) != 0)
      ++count;
  }
  for (int y = 1; y + 1 < binary.rows; ++y) {
    if (binary.at<uchar>(y, 0) != 0)
      ++count;
    if (binary.cols > 1 && binary.at<uchar>(y, binary.cols - 1) != 0)
      ++count;
  }
  return count;
}

/**
 * @brief Produces a binary foreground mask suited for Lab 7 morphology
 * exercises.
 *
 * Chooses polarity automatically by favoring the thresholded result with fewer
 * foreground pixels touching the image border.
 *
 * @param src Input image.
 * @return Binary mask (CV_8UC1), foreground in 255.
 */
static cv::Mat makeLab7ForegroundMask(const cv::Mat &src) {
  cv::Mat gray = toGray8U(src);

  cv::Mat whiteForeground;
  cv::threshold(gray, whiteForeground, 0, 255,
                cv::THRESH_BINARY | cv::THRESH_OTSU);

  cv::Mat blackForeground;
  cv::bitwise_not(whiteForeground, blackForeground);

  const int whiteBorder = countBorderForeground(whiteForeground);
  const int blackBorder = countBorderForeground(blackForeground);
  if (whiteBorder < blackBorder)
    return whiteForeground;
  if (blackBorder < whiteBorder)
    return blackForeground;

  const int whiteArea = cv::countNonZero(whiteForeground);
  const int blackArea = cv::countNonZero(blackForeground);
  return (whiteArea <= blackArea) ? whiteForeground : blackForeground;
}

/**
 * @brief Applies one 8-neighborhood dilation step on a binary image.
 * @param binary Input binary mask (CV_8UC1).
 * @return Dilated binary mask.
 */
static cv::Mat dilate8Once(const cv::Mat &binary) {
  CV_Assert(binary.type() == CV_8UC1);
  cv::Mat dst(binary.rows, binary.cols, CV_8UC1, cv::Scalar(0));

  auto inside = [&](int y, int x) {
    return y >= 0 && y < binary.rows && x >= 0 && x < binary.cols;
  };

  for (int y = 0; y < binary.rows; ++y) {
    for (int x = 0; x < binary.cols; ++x) {
      if (binary.at<uchar>(y, x) == 0)
        continue;
      dst.at<uchar>(y, x) = 255;
      for (const auto &d : kNeighbors8) {
        const int ny = y + d.y;
        const int nx = x + d.x;
        if (!inside(ny, nx))
          continue;
        dst.at<uchar>(ny, nx) = 255;
      }
    }
  }

  return dst;
}

/**
 * @brief Applies one 8-neighborhood erosion step on a binary image.
 * @param binary Input binary mask (CV_8UC1).
 * @return Eroded binary mask.
 */
static cv::Mat erode8Once(const cv::Mat &binary) {
  CV_Assert(binary.type() == CV_8UC1);
  cv::Mat dst(binary.rows, binary.cols, CV_8UC1, cv::Scalar(0));

  auto inside = [&](int y, int x) {
    return y >= 0 && y < binary.rows && x >= 0 && x < binary.cols;
  };

  for (int y = 0; y < binary.rows; ++y) {
    for (int x = 0; x < binary.cols; ++x) {
      if (binary.at<uchar>(y, x) == 0)
        continue;

      bool keep = true;
      for (const auto &d : kNeighbors8) {
        const int ny = y + d.y;
        const int nx = x + d.x;
        if (!inside(ny, nx) || binary.at<uchar>(ny, nx) == 0) {
          keep = false;
          break;
        }
      }

      if (keep)
        dst.at<uchar>(y, x) = 255;
    }
  }

  return dst;
}

/**
 * @brief Repeats either dilation or erosion for a fixed number of iterations.
 * @param binary Input binary mask.
 * @param iterations Number of iterations.
 * @param useDilation True for dilation, false for erosion.
 * @return Resulting binary mask.
 */
static cv::Mat applyMorphIterations(const cv::Mat &binary, int iterations,
                                    bool useDilation) {
  cv::Mat result = binary.clone();
  const int safeIterations = std::max(0, iterations);
  for (int i = 0; i < safeIterations; ++i) {
    result = useDilation ? dilate8Once(result) : erode8Once(result);
  }
  return result;
}

/**
 * @brief Opening with one 8-neighborhood structuring element pass.
 * @param binary Input binary mask.
 * @return Opened binary mask.
 */
static cv::Mat opening8Once(const cv::Mat &binary) {
  return dilate8Once(erode8Once(binary));
}

/**
 * @brief Closing with one 8-neighborhood structuring element pass.
 * @param binary Input binary mask.
 * @return Closed binary mask.
 */
static cv::Mat closing8Once(const cv::Mat &binary) {
  return erode8Once(dilate8Once(binary));
}

/**
 * @brief Repeats opening operator to illustrate idempotence behavior.
 * @param binary Input binary mask.
 * @param repetitions Number of repeated opening applications.
 * @return Resulting binary mask.
 */
static cv::Mat repeatOpening(const cv::Mat &binary, int repetitions) {
  cv::Mat result = binary.clone();
  const int safeRepetitions = std::max(1, repetitions);
  for (int i = 0; i < safeRepetitions; ++i) {
    result = opening8Once(result);
  }
  return result;
}

/**
 * @brief Repeats closing operator to illustrate idempotence behavior.
 * @param binary Input binary mask.
 * @param repetitions Number of repeated closing applications.
 * @return Resulting binary mask.
 */
static cv::Mat repeatClosing(const cv::Mat &binary, int repetitions) {
  cv::Mat result = binary.clone();
  const int safeRepetitions = std::max(1, repetitions);
  for (int i = 0; i < safeRepetitions; ++i) {
    result = closing8Once(result);
  }
  return result;
}

/**
 * @brief Computes morphological boundary: beta(A) = A - (A erode B).
 * @param binary Input object mask A.
 * @return Boundary mask.
 */
static cv::Mat boundaryExtraction8(const cv::Mat &binary) {
  cv::Mat eroded = erode8Once(binary);
  cv::Mat boundary;
  cv::subtract(binary, eroded, boundary);
  return boundary;
}

/**
 * @brief Morphological region filling with 8-neighborhood until convergence.
 *
 * Implements Xk = (Xk-1 dilate B) - A, where A is the object mask, then returns
 * Xk.
 *
 * @param objectMask Binary object mask A (foreground in 255).
 * @param seed Seed point p inside the region to fill.
 * @param maxIterations Safety cap for iterations.
 * @param usedIterations Number of iterations consumed.
 * @param converged True when fixed point reached before maxIterations.
 * @return Filled region mask Xk. Empty matrix means invalid seed/input.
 */
static cv::Mat morphologicalRegionFill8(const cv::Mat &objectMask,
                                        const cv::Point &seed,
                                        int maxIterations, int &usedIterations,
                                        bool &converged) {
  CV_Assert(objectMask.type() == CV_8UC1);
  usedIterations = 0;
  converged = false;

  if (seed.x < 0 || seed.y < 0 || seed.x >= objectMask.cols ||
      seed.y >= objectMask.rows) {
    return cv::Mat();
  }
  if (objectMask.at<uchar>(seed.y, seed.x) != 0) {
    return cv::Mat();
  }

  cv::Mat backgroundMask;
  cv::bitwise_not(objectMask, backgroundMask);

  cv::Mat current(objectMask.rows, objectMask.cols, CV_8UC1, cv::Scalar(0));
  current.at<uchar>(seed.y, seed.x) = 255;

  const int safeMaxIterations = std::max(1, maxIterations);
  for (int iter = 0; iter < safeMaxIterations; ++iter) {
    cv::Mat dilated = dilate8Once(current);
    cv::Mat next;
    cv::bitwise_and(dilated, backgroundMask, next);

    cv::Mat diff;
    cv::absdiff(next, current, diff);
    if (cv::countNonZero(diff) == 0) {
      usedIterations = iter;
      converged = true;
      return current;
    }

    current = next;
    usedIterations = iter + 1;
  }

  return current;
}

/**
 * @brief Returns the number of chain-code directions for a neighborhood.
 * @param neighborhood Neighborhood type.
 * @return 4 for N4, 8 for N8.
 */
static int chainDirectionCount(NeighborhoodType neighborhood) {
  return (neighborhood == NeighborhoodType::N4) ? 4 : 8;
}

/**
 * @brief Returns one chain-code direction offset.
 * @param neighborhood Neighborhood type.
 * @param direction Direction index.
 * @return Offset for the selected direction.
 */
static cv::Point chainDirectionOffset(NeighborhoodType neighborhood,
                                      int direction) {
  return (neighborhood == NeighborhoodType::N4) ? kChainDirections4[direction]
                                                : kChainDirections8[direction];
}

/**
 * @brief Converts a step delta into a chain-code direction index.
 * @param delta Step delta between consecutive boundary points.
 * @param neighborhood Neighborhood type.
 * @return Direction index, or -1 when the delta is invalid for the selected
 * neighborhood.
 */
static int deltaToDirection(const cv::Point &delta,
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

/**
 * @brief Converts a diagonal N4 step into two cardinal steps.
 * @param delta Diagonal delta.
 * @param out Directions appended in order.
 */
static void expandDiagonalToN4(const cv::Point &delta, std::vector<int> &out) {
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

/**
 * @brief Formats a direction sequence for console logging.
 * @param code Direction sequence.
 * @return Space-separated string.
 */
static std::string joinCode(const std::vector<int> &code) {
  std::ostringstream oss;
  for (size_t i = 0; i < code.size(); ++i) {
    if (i > 0)
      oss << ' ';
    oss << code[i];
  }
  return oss.str();
}

/**
 * @brief Computes the circular derivative of a chain code.
 * @param code Input chain code.
 * @param neighborhood Neighborhood type.
 * @return Circular derivative sequence.
 */
static std::vector<int> computeChainDerivative(const std::vector<int> &code,
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

/**
 * @brief Expands one contour segment into chain-code steps and reconstructed
 * points.
 * @param from Segment start point.
 * @param to Segment end point.
 * @param neighborhood Neighborhood type.
 * @param code Output chain code sequence.
 * @param points Output reconstructed points, extended from the last point.
 */
static void appendContourStep(const cv::Point &from, const cv::Point &to,
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

/**
 * @brief Chain-code trace for a single contour/component.
 */
struct ChainCodeTrace {
  cv::Point startPoint{0, 0};
  std::vector<int> code;
  std::vector<int> derivative;
  std::vector<cv::Point> tracedPoints;
};

/**
 * @brief Builds a chain-code trace from an ordered contour.
 * @param contour Ordered contour points.
 * @param neighborhood Selected neighborhood for encoding.
 * @return Chain-code trace, including the reconstructed point path.
 */
static ChainCodeTrace buildChainCodeTrace(const std::vector<cv::Point> &contour,
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

/**
 * @brief Draws a traced boundary polyline in-place.
 * @param image Destination image.
 * @param points Ordered points to render.
 * @param color Polyline color.
 */
static void drawPolyline(cv::Mat &image, const std::vector<cv::Point> &points,
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

/**
 * @brief Logs a boundary chain code and its circular derivative.
 * @param componentIndex 1-based component index.
 * @param neighborhood Selected neighborhood.
 * @param trace Chain-code trace to print.
 */
static void logChainCodeTrace(int componentIndex, NeighborhoodType neighborhood,
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

/**
 * @brief Parsed chain-code file contents used for reconstruction.
 */
struct ChainCodeFileData {
  cv::Point startPoint{0, 0};
  int declaredLength = 0;
  std::vector<int> code;
};

/**
 * @brief Parses a chain-code text file with start point, length, and code
 * sequence.
 * @param text File contents.
 * @param data Output parsed data.
 * @return True if parsing succeeds.
 */
static bool parseChainCodeFile(const std::string &text,
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

/**
 * @brief Reconstructs a polyline from a chain-code sequence.
 * @param startPoint Starting coordinate.
 * @param code Chain-code sequence.
 * @param neighborhood Neighborhood type.
 * @return Ordered polyline points.
 */
static std::vector<cv::Point>
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

/**
 * @brief Renders a boundary code over the source image and prints chain-code
 * diagnostics.
 * @param src Input source image.
 * @param outputs Output image vector; appends the traced boundary overlay.
 * @param controls Controls manager; reads the neighborhood radio selection.
 */
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

/**
 * @brief Reconstructs a boundary from the sample chain-code file.
 * @param src Input source image; used only to size the default output context.
 * @param outputs Output image vector; appends the reconstructed boundary.
 * @param controls Controls manager; reads the neighborhood radio selection.
 */
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

/**
 * @brief Computes geometric measurements for one object mask.
 * @param mask Binary object mask.
 * @param label Label ID associated with the object.
 * @return Populated object statistics.
 */
static ObjectStats computeObjectStats(const cv::Mat &mask, int label) {
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

/**
 * @brief Computes row-wise projection (foreground counts per row).
 * @param mask Binary object mask.
 * @return Projection vector indexed by row.
 */
static std::vector<int> computeRowProjection(const cv::Mat &mask) {
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

/**
 * @brief Computes column-wise projection (foreground counts per column).
 * @param mask Binary object mask.
 * @return Projection vector indexed by column.
 */
static std::vector<int> computeColProjection(const cv::Mat &mask) {
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

/**
 * @brief Draws combined X/Y projections in one coordinate system with filled
 * support.
 * @param canvas Destination image where projections are rendered.
 * @param xProjection Column projection (X-axis signal).
 * @param yProjection Row projection (Y-axis signal).
 */
static void drawCombinedXYProjections(cv::Mat &canvas,
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

/**
 * @brief Creates a pseudo-color visualization from integer labels.
 * @param labels Label image (CV_32SC1).
 * @return BGR visualization image.
 */
static cv::Mat colorizeLabels(const cv::Mat &labels) {
  cv::Mat colored(labels.rows, labels.cols, CV_8UC3, cv::Scalar(0, 0, 0));
  for (int y = 0; y < labels.rows; ++y) {
    const int *row = labels.ptr<int>(y);
    cv::Vec3b *out = colored.ptr<cv::Vec3b>(y);
    for (int x = 0; x < labels.cols; ++x) {
      const int label = row[x];
      if (label <= 0) {
        out[x] = cv::Vec3b(0, 0, 0);
        continue;
      }
      const int b = (label * 67) % 256;
      const int g = (label * 131) % 256;
      const int r = (label * 197) % 256;
      out[x] = cv::Vec3b(static_cast<uchar>(b), static_cast<uchar>(g),
                         static_cast<uchar>(r));
    }
  }
  return colored;
}

/**
 * @brief Converts a click point in main window coordinates to source-image
 * coordinates.
 * @param windowPoint Click position in the rendered grid canvas.
 * @param sourcePoint Output source-image position.
 * @return True if mapping succeeded (point inside an image panel), otherwise
 * false.
 */
static bool mapMainWindowPointToSource(const cv::Point &windowPoint,
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

/**
 * @brief Mouse callback that updates object-selection state on left click.
 * @param event OpenCV mouse event.
 * @param x X coordinate in window space.
 * @param y Y coordinate in window space.
 * @param flags Unused event flags.
 * @param userdata Unused user data pointer.
 */
static void mainWindowMouseCallback(int event, int x, int y, int /*flags*/,
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

/**
 * @brief Converts source to grayscale and applies binary thresholding.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Bi-Level".
 * @param controls Controls manager; reads "Threshold".
 */

void bi_level_color_map(const cv::Mat &src, OutputImages &outputs,
                        ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  int threshold = controls.getEffective("Threshold");

  for (int i = 0; i < gray.rows; ++i)
    for (int j = 0; j < gray.cols; ++j)
      dst.at<uchar>(i, j) = gray.at<uchar>(i, j) > threshold ? 255 : 0;

  outputs.push_back({"Bi-Level", dst});
}

/**
 * @brief Produces grayscale negative image.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Negative".
 */
void negative(const cv::Mat &src, OutputImages &outputs) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  for (int i = 0; i < gray.rows; ++i)
    for (int j = 0; j < gray.cols; ++j)
      dst.at<uchar>(i, j) = 255 - gray.at<uchar>(i, j);

  outputs.push_back({"Negative", dst});
}

/**
 * @brief Adds user-selected intensity offset to grayscale image.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Adjusted".
 * @param controls Controls manager; reads "Additive Factor".
 */
void additive_factor(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  // Slider 0-255 maps to -128..+127
  int factor = controls.getEffective("Additive Factor");

  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int val = gray.at<uchar>(i, j) + factor;
      dst.at<uchar>(i, j) = static_cast<uchar>(std::clamp(val, 0, 255));
    }
  }

  outputs.push_back({"Adjusted", dst});
}

/**
 * @brief Multiplies grayscale image by a scalar factor using OpenCV saturating
 * arithmetic.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Multiplied".
 * @param controls Controls manager; reads "Multiplicative".
 */
void multiplicative(const cv::Mat &src, OutputImages &outputs,
                    ControlsManager &controls) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.rows, gray.cols, CV_8UC1);
  const float factor =
      static_cast<float>(controls.getEffective("Multiplicative"));

  cv::multiply(gray, cv::Scalar(factor), dst);

  outputs.push_back({"Multiplied", dst});
}

/**
 * @brief Generates a static colored 2x2 quadrant placeholder image.
 * @param src Unused source image parameter.
 * @param outputs Output image vector; appends "Placeholder".
 */
void view_placeholder(const cv::Mat & /*src*/, OutputImages &outputs) {
  cv::Mat dst(256, 256, CV_8UC3);

  const int half = 128;
  dst(cv::Rect(0, 0, half, half)).setTo(cv::Scalar(255, 255, 255)); // white
  dst(cv::Rect(half, 0, half, half)).setTo(cv::Scalar(0, 0, 255));  // red
  dst(cv::Rect(0, half, half, half)).setTo(cv::Scalar(0, 255, 0));  // green
  dst(cv::Rect(half, half, half, half))
      .setTo(cv::Scalar(0, 255, 255)); // yellow

  outputs.push_back({"Placeholder", dst});
}

/**
 * @brief Splits source into B, G, R channels and outputs each in colored or
 * grayscale mode.
 * @param src Input source image.
 * @param outputs Output image vector; appends Blue, Green, Red channel results.
 * @param controls Controls manager; reads radio group "Channel Mode".
 */
void rgb_channels(const cv::Mat &src, OutputImages &outputs,
                  ControlsManager &controls) {
  cv::Mat bgr;
  if (src.channels() == 1) {
    cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
  } else {
    bgr = src;
  }

  std::vector<cv::Mat> channels;
  cv::split(bgr, channels); // channels[0]=B, [1]=G, [2]=R

  const int mode = controls.getRadio("Channel Mode");
  // mode 0 = Colored (tinted), mode 1 = Grayscale

  if (mode == 1) {
    // Grayscale: output each channel as a single-channel image
    outputs.push_back({"Blue", channels[0]});
    outputs.push_back({"Green", channels[1]});
    outputs.push_back({"Red", channels[2]});
  } else {
    // Colored: tint each channel with its color
    auto makeTinted = [](const cv::Mat &ch, int channelIndex) {
      cv::Mat tinted(ch.rows, ch.cols, CV_8UC3, cv::Scalar(0, 0, 0));
      std::vector<cv::Mat> planes = {cv::Mat::zeros(ch.size(), CV_8UC1),
                                     cv::Mat::zeros(ch.size(), CV_8UC1),
                                     cv::Mat::zeros(ch.size(), CV_8UC1)};
      planes[channelIndex] = ch;
      cv::merge(planes, tinted);
      return tinted;
    };

    outputs.push_back({"Blue", makeTinted(channels[0], 0)});
    outputs.push_back({"Green", makeTinted(channels[1], 1)});
    outputs.push_back({"Red", makeTinted(channels[2], 2)});
  }
}

// forward declarations
/**
 * @brief Effect declaration for selected-object geometric analysis.
 */
void selected_object_features(const cv::Mat &src, OutputImages &outputs,
                              ControlsManager &controls);
/**
 * @brief Effect declaration for object filtering by area and orientation.
 */
void filter_objects_by_area_orientation(const cv::Mat &src,
                                        OutputImages &outputs,
                                        ControlsManager &controls);

// ---------------------------------------------------------------------------
// Histogram / PDF utilities used by several effects
// ---------------------------------------------------------------------------

/**
 * @brief Computes grayscale histogram with configurable number of bins.
 * @param gray Input grayscale image (CV_8UC1).
 * @param bins Number of bins used for accumulation.
 * @return Histogram count vector.
 */
static std::vector<int> computeHistogram(const cv::Mat &gray, int bins = 256) {
  std::vector<int> hist(bins, 0);
  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int val = gray.at<uchar>(i, j);
      int idx = (val * bins) / 256;
      if (idx >= bins)
        idx = bins - 1;
      hist[idx]++;
    }
  }
  return hist;
}

/**
 * @brief Converts histogram counts to normalized probabilities.
 * @param hist Histogram counts.
 * @param total Normalization denominator (typically number of pixels).
 * @return Probability vector with same length as histogram.
 */
static std::vector<float> computePDF(const std::vector<int> &hist, int total) {
  std::vector<float> pdf(hist.size());
  if (total <= 0)
    return pdf;
  for (size_t i = 0; i < hist.size(); ++i) {
    pdf[i] = static_cast<float>(hist[i]) / static_cast<float>(total);
  }
  return pdf;
}

/**
 * @brief Draws integer histogram as a bar image.
 * @param hist Histogram counts.
 * @param width Output image width.
 * @param height Output image height.
 * @return Histogram image (CV_8UC1).
 */
static cv::Mat drawHistogramInt(const std::vector<int> &hist, int width = 256,
                                int height = 200) {
  int bins = static_cast<int>(hist.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  int maxv = *std::max_element(hist.begin(), hist.end());
  if (maxv == 0)
    maxv = 1;
  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = static_cast<float>(hist[i]) / static_cast<float>(maxv);
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

/**
 * @brief Draws floating-point histogram/PDF as a bar image.
 * @param pdf Histogram/PDF values.
 * @param width Output image width.
 * @param height Output image height.
 * @return Plot image (CV_8UC1).
 */
static cv::Mat drawHistogramFloat(const std::vector<float> &pdf,
                                  int width = 256, int height = 200) {
  int bins = static_cast<int>(pdf.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  float maxv = 0.0f;
  for (float v : pdf)
    maxv = std::max(maxv, v);
  if (maxv == 0.0f)
    maxv = 1.0f;
  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = pdf[i] / maxv;
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

/**
 * @brief Computes the cumulative histogram C(k) = Σ_{i=0..k} h(i).
 *
 * This is the running sum of the histogram. C(255) always equals the total
 * number of pixels W*H. The shape of C is the "where most of the pixels live"
 * curve: a steep rise means many pixels in that intensity range, a flat
 * stretch means few. Equalization uses C normalized by W*H as the
 * intensity-remapping curve.
 */
static std::vector<int>
computeCumulativeHistogram(const std::vector<int> &hist) {
  std::vector<int> cdf(hist.size(), 0);
  if (hist.empty())
    return cdf;

  // Seed the recurrence: cdf[0] = h[0].
  cdf[0] = hist[0];
  // Accumulate: cdf[i] = cdf[i-1] + h[i].
  for (size_t i = 1; i < hist.size(); ++i) {
    cdf[i] = cdf[i - 1] + hist[i];
  }
  return cdf;
}

/**
 * @brief Draws a cumulative histogram as a filled-column plot.
 *
 * Normalizes by cdf.back() (the total pixel count) so the curve always
 * reaches full height at the right edge — that's the visual signature of
 * a cumulative histogram.
 */
static cv::Mat drawCumulativeHistogram(const std::vector<int> &cdf,
                                       int width = 256, int height = 200) {
  int bins = static_cast<int>(cdf.size());
  cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
  if (bins == 0)
    return img;

  // cdf.back() is monotonically the largest element, so use it as the denom.
  int maxv = cdf.back();
  if (maxv == 0)
    maxv = 1;

  float binW = static_cast<float>(width) / bins;
  for (int i = 0; i < bins; ++i) {
    float hval = static_cast<float>(cdf[i]) / static_cast<float>(maxv);
    int barH = static_cast<int>(hval * height);
    cv::rectangle(img, cv::Point(static_cast<int>(i * binW), height - barH),
                  cv::Point(static_cast<int>((i + 1) * binW), height),
                  cv::Scalar(0), cv::FILLED);
  }
  return img;
}

/**
 * @brief Computes mean and standard deviation from a 256-bin histogram.
 *
 * Textbook moments, computed from the histogram instead of raw pixels:
 *
 *   μ     = (1 / (W·H)) · Σ_{i=0..255} i · h(i)
 *   σ     = sqrt( (1 / (W·H)) · Σ_{i=0..255} (i − μ)^2 · h(i) )
 *
 * `hist` must be raw counts and `totalPixels` must equal Σ h(i).
 */
static void computeMeanStdDev(const std::vector<int> &hist, int totalPixels,
                              double &outMean, double &outStdDev) {
  outMean = 0.0;
  outStdDev = 0.0;
  if (totalPixels <= 0 || hist.size() < 256)
    return;

  // First moment — sum of (intensity · count), then divide by N.
  double mean = 0.0;
  for (int i = 0; i < 256; ++i) {
    mean += static_cast<double>(i) * hist[i];
  }
  mean /= static_cast<double>(totalPixels);

  // Second central moment — variance — then sqrt for stddev.
  double var = 0.0;
  for (int i = 0; i < 256; ++i) {
    double d = static_cast<double>(i) - mean;
    var += d * d * hist[i];
  }
  var /= static_cast<double>(totalPixels);

  outMean = mean;
  outStdDev = std::sqrt(var);
}

/**
 * @brief Renders a histogram with μ / σ printed in the top-left corner.
 *        Output is BGR (3 channels) because text is drawn in red.
 */
static cv::Mat drawHistogramIntWithOverlay(const std::vector<int> &hist,
                                           double mean, double stddev,
                                           int width = 256, int height = 220) {
  // Reuse the monochrome renderer, then convert to BGR for coloured text.
  cv::Mat gray = drawHistogramInt(hist, width, height);
  cv::Mat bgr;
  cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);

  char buf[96];
  std::snprintf(buf, sizeof(buf), "mu=%.2f  sigma=%.2f", mean, stddev);
  cv::putText(bgr, buf, cv::Point(6, 16), cv::FONT_HERSHEY_SIMPLEX, 0.45,
              cv::Scalar(0, 0, 200), 1, cv::LINE_AA);
  return bgr;
}

// ---------------------------------------------------------------------------
// New effects: histogram/PDF, m‑bin histogram, gray‑level reduction,
// Floyd‑Steinberg, and HSV hue quantization
// ---------------------------------------------------------------------------

/**
 * @brief Displays 256-bin grayscale histogram and PDF.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Histogram" and "PDF".
 * @param controls Unused controls parameter.
 */
void histogram_and_pdf(const cv::Mat &src, OutputImages &outputs,
                       ControlsManager & /*controls*/) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  auto hist = computeHistogram(gray, 256);
  auto pdf = computePDF(hist, gray.rows * gray.cols);
  cv::Mat histImg = drawHistogramInt(hist);
  cv::Mat pdfImg = drawHistogramFloat(pdf);

  outputs.push_back({"Histogram", histImg});
  outputs.push_back({"PDF", pdfImg});
}

/**
 * @brief Displays histogram using user-selected number of bins m.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Histogram (m bins)".
 * @param controls Controls manager; reads "Bins (m)".
 */
void histogram_m_bins(const cv::Mat &src, OutputImages &outputs,
                      ControlsManager &controls) {
  int m = controls.getEffective("Bins (m)");
  m = std::clamp(m, 2, 256);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  auto hist = computeHistogram(gray, m);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Histogram (m bins)", histImg});
}

/**
 * @brief Reduces grayscale to WL quantization levels and displays resulting
 * histogram.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Reduced" and "Histogram".
 * @param controls Controls manager; reads "Levels (WL)".
 */
void gray_level_reduction(const cv::Mat &src, OutputImages &outputs,
                          ControlsManager &controls) {
  int WL = controls.getEffective("Levels (WL)");
  WL = std::clamp(WL, 2, 128);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat dst(gray.size(), CV_8UC1);
  float scale = 255.0f / static_cast<float>(WL - 1);
  for (int i = 0; i < gray.rows; ++i) {
    for (int j = 0; j < gray.cols; ++j) {
      int g = gray.at<uchar>(i, j);
      int q = (g * WL) / 256; // 0..WL-1
      dst.at<uchar>(i, j) = static_cast<uchar>(q * scale);
    }
  }

  auto hist = computeHistogram(dst, WL);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Reduced", dst});
  outputs.push_back({"Histogram", histImg});
}

/**
 * @brief Applies Floyd-Steinberg error-diffusion quantization.
 * @param src Input source image.
 * @param outputs Output image vector; appends "Floyd-Steinberg" and
 * "Histogram".
 * @param controls Controls manager; reads "Levels (WL)".
 */
void floyd_steinberg(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  int WL = controls.getEffective("Levels (WL)");
  WL = std::clamp(WL, 2, 128);

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  cv::Mat fimg;
  gray.convertTo(fimg, CV_32F);
  int rows = fimg.rows;
  int cols = fimg.cols;
  float scale = 255.0f / static_cast<float>(WL - 1);

  auto clampf = [](float v) { return std::clamp(v, 0.0f, 255.0f); };

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      float old = fimg.at<float>(i, j);
      float quant =
          std::round(old * static_cast<float>(WL - 1) / 255.0f) * scale;
      float err = old - quant;
      fimg.at<float>(i, j) = quant;
      if (j + 1 < cols)
        fimg.at<float>(i, j + 1) =
            clampf(fimg.at<float>(i, j + 1) + err * 7.0f / 16.0f);
      if (i + 1 < rows) {
        if (j > 0)
          fimg.at<float>(i + 1, j - 1) =
              clampf(fimg.at<float>(i + 1, j - 1) + err * 3.0f / 16.0f);
        fimg.at<float>(i + 1, j) =
            clampf(fimg.at<float>(i + 1, j) + err * 5.0f / 16.0f);
        if (j + 1 < cols)
          fimg.at<float>(i + 1, j + 1) =
              clampf(fimg.at<float>(i + 1, j + 1) + err * 1.0f / 16.0f);
      }
    }
  }

  cv::Mat dst;
  fimg.convertTo(dst, CV_8UC1);
  auto hist = computeHistogram(dst, WL);
  cv::Mat histImg = drawHistogramInt(hist);
  outputs.push_back({"Floyd-Steinberg", dst});
  outputs.push_back({"Histogram", histImg});
}

/**
 * @brief Quantizes hue channel in HSV; optional mode forces S and V to maximum.
 * @param src Input source image.
 * @param outputs Output image vector; appends "HSV Quantized".
 * @param controls Controls manager; reads "Hue Levels" and "S/V Mode".
 */
void hsv_hue_quantization(const cv::Mat &src, OutputImages &outputs,
                          ControlsManager &controls) {
  int levels = controls.getEffective("Hue Levels");
  levels = std::clamp(levels, 2, 128);
  int mode = controls.getRadio("S/V Mode");

  cv::Mat bgr = ensureBgr(src);
  cv::Mat dst(bgr.size(), CV_8UC3);
  for (int i = 0; i < bgr.rows; ++i) {
    for (int j = 0; j < bgr.cols; ++j) {
      cv::Vec3b pix = bgr.at<cv::Vec3b>(i, j);
      HSV hsv(pix[2], pix[1], pix[0]); // convert BGR->HSV
      float h = hsv.h;
      float newh = std::round(h * levels / 360.0f) * (360.0f / levels);
      if (newh >= 360.0f)
        newh = 0.0f;
      hsv.h = newh;
      if (mode == 1) {
        hsv.s = 100.0f;
        hsv.v = 100.0f;
      }
      RGB rgb = hsv.toRGB();
      dst.at<cv::Vec3b>(i, j) = cv::Vec3b(rgb.B(), rgb.G(), rgb.R());
    }
  }
  outputs.push_back({"HSV Quantized", dst});
}

// ---------------------------------------------------------------------------
// Lab 8 — Statistical properties of intensity images
// ---------------------------------------------------------------------------

/**
 * @brief Lab 8, Exercise 1 — statistical properties display.
 */
void lab8_statistics(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager & /*controls*/) {
  cv::Mat gray = toGray8U(src);

  auto hist = computeHistogram(gray, 256);
  auto cdf = computeCumulativeHistogram(hist);
  auto pdf = computePDF(hist, gray.rows * gray.cols);

  double mean = 0.0;
  double stddev = 0.0;
  computeMeanStdDev(hist, gray.rows * gray.cols, mean, stddev);

  INFO("[Lab8] mean={:.4f} stddev={:.4f}", mean, stddev);

  outputs.push_back(
      {"Histogram", drawHistogramIntWithOverlay(hist, mean, stddev)});
  outputs.push_back({"Cumulative", drawCumulativeHistogram(cdf)});
  outputs.push_back({"PDF", drawHistogramFloat(pdf)});
}

/**
 * @brief Lab 8, Exercise 2 — automatic iterative binarization.
 */
void lab8_auto_binarize(const cv::Mat &src, OutputImages &outputs,
                        ControlsManager &controls) {
  int epsInt = controls.getEffective("Epsilon x100");
  if (epsInt < 1)
    epsInt = 1;
  double epsilon = static_cast<double>(epsInt) / 100.0;

  cv::Mat gray = toGray8U(src);
  auto hist = computeHistogram(gray, 256);

  int iMin = 0;
  int iMax = 255;
  while (iMin < 256 && hist[iMin] == 0)
    ++iMin;
  while (iMax > 0 && hist[iMax] == 0)
    --iMax;

  if (iMin >= iMax) {
    cv::Mat bin(gray.size(), CV_8UC1, cv::Scalar(0));
    outputs.push_back({"Binary", bin});
    return;
  }

  double T = 0.5 * (iMin + iMax);
  double prevT = T + 10.0 * epsilon + 1.0;
  int iterations = 0;
  const int maxIterations = 1000;

  while (std::abs(T - prevT) >= epsilon && iterations < maxIterations) {
    prevT = T;
    long long n1 = 0;
    long long n2 = 0;
    double s1 = 0.0;
    double s2 = 0.0;
    int Ti = static_cast<int>(std::floor(T));

    for (int i = iMin; i <= iMax; ++i) {
      if (i <= Ti) {
        n1 += hist[i];
        s1 += static_cast<double>(i) * hist[i];
      } else {
        n2 += hist[i];
        s2 += static_cast<double>(i) * hist[i];
      }
    }

    double mu1 =
        (n1 > 0) ? (s1 / static_cast<double>(n1)) : static_cast<double>(iMin);
    double mu2 =
        (n2 > 0) ? (s2 / static_cast<double>(n2)) : static_cast<double>(iMax);
    T = 0.5 * (mu1 + mu2);
    ++iterations;
  }

  int thresh = std::clamp(static_cast<int>(std::round(T)), 0, 255);
  INFO("[Lab8] auto-binarize: Imin={} Imax={} T={:.4f} iter={} eps={:.4f}",
       iMin, iMax, T, iterations, epsilon);

  cv::Mat bin(gray.size(), CV_8UC1);
  for (int r = 0; r < gray.rows; ++r) {
    const uchar *s = gray.ptr<uchar>(r);
    uchar *d = bin.ptr<uchar>(r);
    for (int c = 0; c < gray.cols; ++c) {
      d[c] = (s[c] > thresh) ? 255 : 0;
    }
  }

  cv::Mat binAnnotated;
  cv::cvtColor(bin, binAnnotated, cv::COLOR_GRAY2BGR);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "T = %d  (iter %d)", thresh, iterations);
  cv::putText(binAnnotated, buf, cv::Point(8, 20), cv::FONT_HERSHEY_SIMPLEX,
              0.55, cv::Scalar(0, 0, 220), 1, cv::LINE_AA);

  outputs.push_back({"Binary", binAnnotated});
}

/**
 * @brief Lab 8, Exercise 3 — pointwise intensity transforms.
 */
void lab8_transforms(const cv::Mat &src, OutputImages &outputs,
                     ControlsManager &controls) {
  int outMin = std::clamp(controls.getEffective("Iout Min"), 0, 255);
  int outMax = std::clamp(controls.getEffective("Iout Max"), 0, 255);
  if (outMax < outMin)
    std::swap(outMin, outMax);

  int gammaInt = controls.getEffective("Gamma x10");
  if (gammaInt < 1)
    gammaInt = 1;
  double gamma = static_cast<double>(gammaInt) / 10.0;

  int brightnessRaw = controls.getEffective("Brightness Offset");
  int offset = brightnessRaw - 128;

  cv::Mat gray = toGray8U(src);
  auto histOriginal = computeHistogram(gray, 256);

  double inMinD = 0.0;
  double inMaxD = 0.0;
  cv::minMaxLoc(gray, &inMinD, &inMaxD);
  int inMin = static_cast<int>(inMinD);
  int inMax = static_cast<int>(inMaxD);
  double inSpan = std::max(1.0, static_cast<double>(inMax - inMin));

  cv::Mat negative(gray.size(), CV_8UC1);
  cv::Mat contrast(gray.size(), CV_8UC1);
  cv::Mat gammaImg(gray.size(), CV_8UC1);
  cv::Mat brightness(gray.size(), CV_8UC1);

  uchar lutNeg[256];
  uchar lutCon[256];
  uchar lutGam[256];
  uchar lutBri[256];
  for (int v = 0; v < 256; ++v) {
    lutNeg[v] = static_cast<uchar>(255 - v);

    double stretched = outMin + (v - inMin) * (outMax - outMin) / inSpan;
    lutCon[v] = static_cast<uchar>(std::clamp(stretched, 0.0, 255.0));

    double g = 255.0 * std::pow(static_cast<double>(v) / 255.0, gamma);
    lutGam[v] = static_cast<uchar>(std::clamp(g, 0.0, 255.0));

    int b = v + offset;
    lutBri[v] = static_cast<uchar>(std::clamp(b, 0, 255));
  }

  auto applyLut = [&](const uchar lut[256], cv::Mat &dst) {
    for (int r = 0; r < gray.rows; ++r) {
      const uchar *s = gray.ptr<uchar>(r);
      uchar *d = dst.ptr<uchar>(r);
      for (int c = 0; c < gray.cols; ++c) {
        d[c] = lut[s[c]];
      }
    }
  };
  applyLut(lutNeg, negative);
  applyLut(lutCon, contrast);
  applyLut(lutGam, gammaImg);
  applyLut(lutBri, brightness);

  auto histNeg = computeHistogram(negative, 256);
  auto histCon = computeHistogram(contrast, 256);
  auto histGam = computeHistogram(gammaImg, 256);
  auto histBri = computeHistogram(brightness, 256);

  outputs.push_back({"Negative", negative});
  outputs.push_back({"Contrast", contrast});
  outputs.push_back({"Gamma", gammaImg});
  outputs.push_back({"Brightness", brightness});

  outputs.push_back({"Hist Original", drawHistogramInt(histOriginal)});
  outputs.push_back({"Hist Negative", drawHistogramInt(histNeg)});
  outputs.push_back({"Hist Contrast", drawHistogramInt(histCon)});
  outputs.push_back({"Hist Gamma", drawHistogramInt(histGam)});
  outputs.push_back({"Hist Brightness", drawHistogramInt(histBri)});

  INFO("[Lab8] transforms: outMin={} outMax={} gamma={:.2f} offset={}", outMin,
       outMax, gamma, offset);
}

/**
 * @brief Lab 8, Exercise 4 — histogram equalization.
 */
void lab8_histogram_equalization(const cv::Mat &src, OutputImages &outputs,
                                 ControlsManager & /*controls*/) {
  cv::Mat gray = toGray8U(src);
  int total = gray.rows * gray.cols;
  if (total <= 0) {
    outputs.push_back({"Equalized", gray});
    return;
  }

  auto hist = computeHistogram(gray, 256);

  double invTotal = 1.0 / static_cast<double>(total);
  uchar lut[256];
  double running = 0.0;
  for (int i = 0; i < 256; ++i) {
    running += static_cast<double>(hist[i]) * invTotal;
    int mapped = static_cast<int>(std::round(255.0 * running));
    lut[i] = static_cast<uchar>(std::clamp(mapped, 0, 255));
  }

  cv::Mat equalized(gray.size(), CV_8UC1);
  for (int r = 0; r < gray.rows; ++r) {
    const uchar *s = gray.ptr<uchar>(r);
    uchar *d = equalized.ptr<uchar>(r);
    for (int c = 0; c < gray.cols; ++c) {
      d[c] = lut[s[c]];
    }
  }

  auto histEq = computeHistogram(equalized, 256);

  double muOrig = 0.0;
  double sigOrig = 0.0;
  double muEq = 0.0;
  double sigEq = 0.0;
  computeMeanStdDev(hist, total, muOrig, sigOrig);
  computeMeanStdDev(histEq, total, muEq, sigEq);

  INFO(
      "[Lab8] equalize: orig mu={:.2f} sigma={:.2f}  eq mu={:.2f} sigma={:.2f}",
      muOrig, sigOrig, muEq, sigEq);

  outputs.push_back({"Equalized", equalized});
  outputs.push_back(
      {"Hist Original", drawHistogramIntWithOverlay(hist, muOrig, sigOrig)});
  outputs.push_back(
      {"Hist Equalized", drawHistogramIntWithOverlay(histEq, muEq, sigEq)});
}

/**
 * @brief Lab 7 combined morphology effect (N8 only): dilation, erosion,
 * opening, and closing.
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Controls manager; reads "Iterations (N)".
 */
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

/**
 * @brief Lab 7 contour extraction and iterative region filling effect (N8
 * only).
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Controls manager; reads "Max Fill Iter".
 */
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

/**
 * @brief Performs click-based labeled-object analysis and visualization.
 *
 * Computes and displays: area, center of mass, elongation axis, perimeter,
 * thinness ratio, aspect ratio, contour points, and projections.
 *
 * @param src Input source image.
 * @param outputs Output image vector; appends label visualization, annotated
 * object view, and projection view.
 * @param controls Unused controls parameter.
 */
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

/**
 * @brief Keeps only objects satisfying area and orientation constraints.
 * @param src Input source image.
 * @param outputs Output image vector; appends filtered object view and filtered
 * labels view.
 * @param controls Controls manager; reads TH_area, phi_LOW, and phi_HIGH.
 */
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

/**
 * @brief Loads image from path and resets selection/update state.
 * @param path Path to image file.
 * @param img Destination matrix for loaded image.
 * @return True if load succeeds, false otherwise.
 */
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

/**
 * @brief Returns a BGR image for rendering routines.
 * @param image Input grayscale/BGR image.
 * @return BGR clone/converted image.
 */
static cv::Mat ensureBgr(const cv::Mat &image) {
  if (image.channels() == 3) {
    return image.clone();
  }

  cv::Mat converted;
  cv::cvtColor(image, converted, cv::COLOR_GRAY2BGR);
  return converted;
}

/**
 * @brief Renders source and all output images into a labeled grid.
 * @param src Source image.
 * @param outputs Named output images.
 */
static void renderGrid(const cv::Mat &src, const OutputImages &outputs) {
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

/**
 * @brief Saves all output images to the export directory with timestamped
 * names.
 * @param outputs Named output images to save.
 */
static void saveAllOutputs(const OutputImages &outputs) {
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

/**
 * @brief Detects whether OpenCV was built with Qt GUI support.
 * @return True if Qt backend is available.
 */
static bool hasQtBackend() {
  const std::string info = cv::getBuildInformation();
  const bool hasGuiSection = info.find("GUI:") != std::string::npos;
  const bool hasQt = info.find("QT") != std::string::npos;
  return hasGuiSection && hasQt;
}

void center_transform(cv::Mat src) {
  for (int i = 0; i < src.rows; i++) {
    for (int j = 0; j < src.cols; j++) {
      src.at<float>(i, j) =
          ((i + j) & 1) ? -src.at<float>(i, j) : src.at<float>(i, j);
    }
  }
}

void general_filter(const cv::Mat &src, OutputImages &outputs) {

  cv::Mat gray;
  if (src.channels() > 1) {
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
  } else {
    gray = src;
  }

  cv::Mat srcf;

  center_transform(srcf);

  cv::Mat fourier;

  dft(srcf, fourier, cv::DFT_COMPLEX_OUTPUT);

  cv::Mat dst;

  cv::normalize(fourier, dst, 0, 255, cv::NORM_MINMAX, CV_8UC1);

  outputs.push_back({"Fourier", fourier});

  // TODO: finish
}

/**
 * @brief Program entry point; initializes app, runs UI loop, and handles
 * cleanup.
 * @return 0 on clean exit, -1 on initialization/load errors.
 */
int main() {
  Logger::init();

  if (!hasQtBackend()) {
    ERROR("OpenCV Qt backend not detected. Rebuild/install OpenCV with "
          "WITH_QT=ON.");
    Logger::destroy();
    return -1;
  }

  cv::namedWindow(MAIN_WINDOW_NAME,
                  cv::WINDOW_NORMAL | cv::WINDOW_GUI_EXPANDED);

  DEBUG("Opening file {}", IMAGE("PI-L4/trasaturi_geom.bmp"));
  cv::Mat img;
  if (!loadImage(IMAGE("PI-L4/trasaturi_geom.bmp"), img)) {
    Logger::destroy();
    return -1;
  }

  DEBUG("image loaded with {} {}", img.rows, img.cols);
  INFO("Controls:");
  INFO("  Left/Right arrows: cycle through processing functions");
  INFO("  O: open file dialog to load new image");
  INFO("  Enter: save all outputs");
  INFO("  Space: refresh windows");
  INFO("  ESC: exit");

  // Controls manager — the callback sets the global update flag
  ControlsManager controls(MAIN_WINDOW_NAME, []() { g_needsUpdate = true; });

  // Processing functions that the user can slide through.
  // Each entry declares the trackbars it needs — the Slider +
  // ControlsManager handle creation/teardown automatically.
  //
  // Effects use either the 3-arg form (src, outputs, controls)
  // or the 2-arg form (src, outputs) — see SliderEntry constructors.
  Slider slider(
      {{"Bi-Level",
        EffectFn3(bi_level_color_map),
        {{"Threshold", 127, 255, 127, true, 0}},
        "Binary thresholding",
        {} /* radio groups */,
        "Lab 3 — intensity & colour"},

       {"Negative",
        EffectFn2(negative),
        {} /* no trackbars */,
        "Pixel inversion"},

       {"Additive Factor",
        EffectFn3(additive_factor),
        {{"Additive Factor", 128, 255, 128, true, 0}},
        "Brightness shift"},

       {"Multiplicative",
        EffectFn3(multiplicative),
        {{"Multiplicative", 1, 35, 1, true, 0}},
        "Saturating multiplication"},

       {"View Placeholder",
        EffectFn2(view_placeholder),
        {} /* no trackbars */,
        "Static 4-quadrant reference"},

       {"RGB Channels",
        EffectFn3(rgb_channels),
        {} /* no trackbars */,
        "Split into color-tinted R, G, B channels",
        {{"Channel Mode", {"Colored", "Grayscale"}, 1}}},

       // L3 additions
       {"Histogram & PDF",
        EffectFn3(histogram_and_pdf),
        {} /* no trackbars */,
        "Compute and display histogram + PDF",
        {} /* radio groups */,
        "Lab 3 — histogram & quantization"},

       {"Histogram (m bins)",
        EffectFn3(histogram_m_bins),
        {{"Bins (m)", 256, 256, 256, true, 2}},
        "Histogram computed with m<=256 bins"},

       {"Gray Level Reduction",
        EffectFn3(gray_level_reduction),
        {{"Levels (WL)", 4, 128, 4, true, 2}},
        "Uniform quantization into WL gray levels"},

       {"Floyd-Steinberg",
        EffectFn3(floyd_steinberg),
        {{"Levels (WL)", 4, 128, 4, true, 2}},
        "Error-diffused quantization (Floyd–Steinberg)"},

       {"HSV Hue Quantization",
        EffectFn3(hsv_hue_quantization),
        {{"Hue Levels", 6, 128, 6, true, 2}},
        "Reduce hue levels in HSV space",
        {{"S/V Mode", {"Keep Original", "Set to Max"}, 0}}},

       {"Morphology Lab7",
        EffectFn3(morphology_lab7_combined),
        {{"Iterations (N)", 1, 15, 1, true, 1}},
        "N8-only morphology: dilation, erosion, opening, closing",
        {} /* radio groups */,
        "Lab 7 — morphology & contours"},

       {"Contour + Fill Lab7",
        EffectFn3(contour_and_region_fill_lab7),
        {{"Max Fill Iter", 3000, 50000, 3000, true, 1}},
        "N8-only boundary extraction and iterative region filling"},

       {"Boundary Margin Code",
        EffectFn3(boundary_margin_code),
        {} /* no trackbars */,
        "Trace black component margins and print chain code",
        {{"Neighborhood", {"N4", "N8"}, 1}}},

       {"Boundary Reconstruct",
        EffectFn3(boundary_reconstruct_from_code),
        {} /* no trackbars */,
        "Reconstruct boundary from chain-code file",
        {{"Neighborhood", {"N4", "N8"}, 1}}},

       {"Selected Object Features",
        EffectFn3(selected_object_features),
        {} /* no trackbars */,
        "Click-select object and compute geometric features",
        {} /* radio groups */,
        "Lab 4 — geometric features"},

       {"Filter Area+Orientation",
        EffectFn3(filter_objects_by_area_orientation),
        {{"TH_area", 1000, 200000, 1000, true, 1},
         {"phi_LOW", 0, 180, 0, true, -90},
         {"phi_HIGH", 180, 180, 180, true, -90}},
        "Keep objects with area < TH_area and phi in [phi_LOW, phi_HIGH]"},

       {"Labeling Compare",
        EffectFn3(labeling_compare),
        {} /* no trackbars */,
        "Compare traversal labeling with two-pass labeling",
        {{"Neighborhood", {"N4", "N8"}, 1}, {"Traversal", {"BFS", "DFS"}, 0}}},

       {"Lab8 Statistics",
        EffectFn3(lab8_statistics),
        {} /* no trackbars */,
        "Histogram, cumulative histogram, PDF, mean, stddev",
        {} /* radio groups */,
        "Lab 8 — statistical properties"},

       {"Lab8 Auto Binarize",
        EffectFn3(lab8_auto_binarize),
        {{"Epsilon x100", 10, 500, 10, true, 1}},
        "Iterative automatic thresholding (T stops when |dT|<eps)"},

       {"Lab8 Transforms",
        EffectFn3(lab8_transforms),
        {{"Iout Min", 30, 255, 0, true, 0},
         {"Iout Max", 220, 255, 255, true, 0},
         {"Gamma x10", 10, 30, 10, true, 1},
         {"Brightness Offset", 128, 255, 128, true, 0}},
        "Negative, contrast stretch, gamma, brightness + histograms"},

       {"Lab8 Equalize",
        EffectFn3(lab8_histogram_equalization),
        {} /* no trackbars */,
        "Histogram equalization (original + equalized histograms)"},

       {"Spatial Filter",
        EffectFn2(general_filter),
        {} /* no trackbars */,
        "test"},
       {"Contour Similarity",
        EffectFn3(contour_similarity),
        {{"Threshold x100", 80, 100, 0, true, 0}},
        "Label objects, trace extended contours, cross-correlation similarity",
        {} /* radio groups */,
        "Assignment — contour similarity"}},
      controls);

  // Main loop
  int keyCode = -1;
  bool running = true;
  std::string displayedEffectName;
  OutputImages lastOutputs; // keep last outputs for saving
  while (running) {
    if (slider.applyPendingSelection()) {
      g_needsUpdate = true;
    }

    const std::string currentEffectName = slider.currentName();
    if (currentEffectName != displayedEffectName) {
      updateMainWindowTitle(currentEffectName);
      displayedEffectName = currentEffectName;
      g_selectionState.dirty = true;
    }

    // Execute the current processing function.
    lastOutputs = slider.exec(img);
    renderGrid(img, lastOutputs);
    cv::setMouseCallback(MAIN_WINDOW_NAME, mainWindowMouseCallback, nullptr);

    // Wait for key (non-blocking with 30ms timeout)
    keyCode = WaitKey(30);

    // Detect window close via the X button
    if (cv::getWindowProperty(MAIN_WINDOW_NAME, cv::WND_PROP_VISIBLE) < 1) {
      running = false;
      break;
    }

    if (keyCode == -1)
      continue;

    KEY operation = resolvedKey(keyCode);

    switch (operation) {
    case KEY::LEFT_ARROW:
      slider.previous(); // also re-activates controls
      g_needsUpdate = true;
      break;
    case KEY::RIGHT_ARROW:
      slider.next(); // also re-activates controls
      g_needsUpdate = true;
      break;
    case KEY::ENTER:
      saveAllOutputs(lastOutputs);
      break;
    case KEY::SPACE:
      slider.reactivateControls();
      g_needsUpdate = true;
      break;
    case KEY::ESC:
      running = false;
      break;
    default:
      if (keyCode >= '1' && keyCode <= '9') {
        slider.selectByIndex(static_cast<std::size_t>(keyCode - '1'));
        g_needsUpdate = true;
      } else if (keyCode == 'o' || keyCode == 'O') {
        std::string newPath = FileUtils::openFileDialog();
        if (!newPath.empty()) {
          loadImage(newPath, img);
          slider.reactivateControls();
        }
      }
      break;
    }
  }

  controls.destroyControls();
  Logger::destroy();
  return 0;
}
