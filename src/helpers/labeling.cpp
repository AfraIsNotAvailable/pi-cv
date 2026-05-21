#include "labeling.h"
#include "image_utils.h"
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <stack>

cv::Mat buildLabelsByConnectedComponents(const cv::Mat &srcGray8u) {
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

cv::Mat buildLabelsFromGrayValues(const cv::Mat &gray8u) {
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

cv::Mat buildLabelsFromColorValues(const cv::Mat &bgr) {
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

cv::Mat buildLabelImage(const cv::Mat &src) {
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

cv::Mat labelComponentsByTraversal(const cv::Mat &binary,
                                   NeighborhoodType neighborhood,
                                   bool useDfsStack) {
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

void collectPreviousNeighborLabels(const cv::Mat &labels, int x, int y,
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

TwoPassLabelingResult
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

std::vector<int> collectLabels(const cv::Mat &labels) {
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

cv::Mat maskForLabel(const cv::Mat &labels, int label) {
  cv::Mat mask;
  cv::compare(labels, label, mask, cv::CMP_EQ);
  return mask;
}

cv::Mat colorizeLabels(const cv::Mat &labels) {
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
