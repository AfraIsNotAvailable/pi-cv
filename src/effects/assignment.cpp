#include "assignment.h"
#include <random>

void contour_similarity(const cv::Mat &src, OutputImages &outputs,
                        ControlsManager &controls) {

  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  static cv::Mat cachedSrc;
  static cv::Mat cachedVis;
  static std::vector<cv::Point2d> cachedCentroids;
  static std::vector<std::vector<double>> cachedSimMatrix;
  static int cachedNumOfObjects = 0;

  bool srcChanged = cachedSrc.empty() ||
                    cachedSrc.size() != gray.size() ||
                    cachedSrc.type() != gray.type();
  if (!srcChanged) {
    cv::Mat diff;
    cv::absdiff(gray, cachedSrc, diff);
    srcChanged = cv::countNonZero(diff) > 0;
  }

  if (srcChanged) {
    cachedSrc = gray.clone();

    cv::Mat labels(gray.size(), CV_32S, cv::Scalar(0));
    std::vector<cv::Point2d> centroids;
    std::vector<cv::Point> firstPixel; // (col, row) per label

    int nextLabel = 1;
    for (int i = 0; i < gray.rows; i++) {
      for (int j = 0; j < gray.cols; j++) {
        if (gray.at<uchar>(i, j) == 0 && labels.at<int>(i, j) == 0) {
          std::queue<cv::Point> q;
          q.push({j, i});
          labels.at<int>(i, j) = nextLabel;
          double sumI = 0, sumJ = 0;
          int count = 0;
          while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();
            sumI += y;
            sumJ += x;
            count++;
            for (auto [dy, dx] : std::array<std::pair<int, int>, 4>{
                     {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
              int ny = y + dy, nx = x + dx;
              if (ny >= 0 && ny < gray.rows && nx >= 0 && nx < gray.cols &&
                  gray.at<uchar>(ny, nx) == 0 && labels.at<int>(ny, nx) == 0) {
                labels.at<int>(ny, nx) = nextLabel;
                q.push({nx, ny});
              }
            }
          }
          centroids.push_back({sumJ / count, sumI / count});
          firstPixel.push_back({j, i}); // raster scan finds top-left-most pixel first
          nextLabel++;
        }
      }
    }
    int numOfObjects = nextLabel - 1;

    // corners

    static const int LUT[16][4][3] = {
        /* p= 0 unreachable */ {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        /* p= 1 -> Right    */ {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}},
        /* p= 2 -> Down     */ {{3, 1, 0}, {3, 1, 0}, {3, 1, 0}, {3, 1, 0}},
        /* p= 3 -> Right    */ {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}},
        /* p= 4 -> Up       */ {{1, -1, 0}, {1, -1, 0}, {1, -1, 0}, {1, -1, 0}},
        /* p= 5 -> Up       */ {{1, -1, 0}, {1, -1, 0}, {1, -1, 0}, {1, -1, 0}},
        /* p= 6 ambiguous   */ {{3, 1, 0}, {1, -1, 0}, {1, -1, 0}, {3, 1, 0}},
        /* p= 7 -> Up       */ {{1, -1, 0}, {1, -1, 0}, {1, -1, 0}, {1, -1, 0}},
        /* p= 8 -> Left     */ {{2, 0, -1}, {2, 0, -1}, {2, 0, -1}, {2, 0, -1}},
        /* p= 9 ambiguous   */ {{0, 0, 1}, {0, 0, 1}, {2, 0, -1}, {2, 0, -1}},
        /* p=10 -> Down     */ {{3, 1, 0}, {3, 1, 0}, {3, 1, 0}, {3, 1, 0}},
        /* p=11 -> Right    */ {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}},
        /* p=12 -> Left     */ {{2, 0, -1}, {2, 0, -1}, {2, 0, -1}, {2, 0, -1}},
        /* p=13 -> Left     */ {{2, 0, -1}, {2, 0, -1}, {2, 0, -1}, {2, 0, -1}},
        /* p=14 -> Down     */ {{3, 1, 0}, {3, 1, 0}, {3, 1, 0}, {3, 1, 0}},
        /* p=15 unreachable */ {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    };

    std::vector<std::vector<int>> chainCodes(numOfObjects + 1);
    std::vector<std::vector<cv::Point>> contours(numOfObjects + 1);

    int maxIters = 8 * gray.rows * gray.cols + 8;

    for (int lbl = 1; lbl <= numOfObjects; lbl++) {
      int startRow = firstPixel[lbl - 1].y;
      int startCol = firstPixel[lbl - 1].x;

      int ci = startRow, cj = startCol + 1;
      int dir = 3; // initial: walk Down
      std::vector<int> chainCode;
      std::vector<cv::Point> contourCorners;

      auto inside = [&](int r, int c) -> int {
        if (r < 0 || r >= gray.rows || c < 0 || c >= gray.cols)
          return 0;
        return labels.at<int>(r, c) == lbl ? 1 : 0;
      };

      bool firstIter = true;
      int iter = 0;
      while (true) {
        if (!firstIter && ci == startRow && cj == startCol + 1)
          break;
        if (++iter > maxIters)
          break;
        firstIter = false;

        contourCorners.push_back({cj, ci});

        int p = (inside(ci - 1, cj - 1) << 3) | (inside(ci - 1, cj) << 2) |
                (inside(ci, cj - 1) << 1) | (inside(ci, cj));

        int nd = LUT[p][dir][0];
        int dr = LUT[p][dir][1];
        int dc = LUT[p][dir][2];
        if (dr == 0 && dc == 0)
          break; // trapped state safety
        ci += dr;
        cj += dc;
        chainCode.push_back(nd);
        dir = nd;
      }

      chainCodes[lbl] = chainCode;
      contours[lbl] = contourCorners;
    }

    // vis

    cv::Mat vis(gray.size(), CV_8UC3, cv::Scalar(255, 255, 255));

    for (int i = 0; i < gray.rows; i++)
      for (int j = 0; j < gray.cols; j++)
        if (labels.at<int>(i, j) > 0)
          vis.at<cv::Vec3b>(i, j) = {0, 0, 0};

    std::default_random_engine gen(42);
    std::uniform_int_distribution<int> d(0, 255);

    for (int lbl = 1; lbl <= numOfObjects; lbl++) {
      cv::Vec3b color(d(gen), d(gen), d(gen));
      for (auto &pt : contours[lbl]) {
        int r = pt.y, c = pt.x;
        if (r >= 0 && r < vis.rows && c >= 0 && c < vis.cols)
          vis.at<cv::Vec3b>(r, c) = color;
      }
    }

    // sim
    auto similarity = [&](const std::vector<int> &ca,
                          const std::vector<int> &cb) -> double {
      const auto &a = (ca.size() <= cb.size()) ? ca : cb;
      const auto &b = (ca.size() <= cb.size()) ? cb : ca;
      int n = a.size(), m = b.size();
      if (n == 0 || m == 0)
        return 0.0;

      double maxS = -1.0;
      for (int k = 0; k < 4; ++k) {
        for (int j = 0; j < m; ++j) {
          double sum = 0;
          for (int i = 0; i < n; ++i) {
            int ai = (a[i] + k) % 4;
            int bi = b[(i + j) % m];
            int diff = std::abs(ai - bi);
            int dist = std::min(diff, 4 - diff);
            sum += std::cos(dist * M_PI / 2.0);
          }
          maxS = std::max(maxS, sum / n);
        }
      }
      return maxS;
    };

    //calc similarity
    std::vector<std::vector<double>> simMatrix(
        numOfObjects + 1, std::vector<double>(numOfObjects + 1, 0.0));
    for (int a = 1; a <= numOfObjects; a++)
      for (int b = a + 1; b <= numOfObjects; b++)
        simMatrix[a][b] = simMatrix[b][a] =
            similarity(chainCodes[a], chainCodes[b]);

    for (int a = 1; a <= numOfObjects; a++) {
      for (int b = 1; b <= numOfObjects; b++)
        printf("%6.3f ", simMatrix[a][b]);
      printf("\n");
    }

    cachedVis = vis;
    cachedCentroids = centroids;
    cachedSimMatrix = simMatrix;
    cachedNumOfObjects = numOfObjects;
  }

  outputs.push_back({"Contours", cachedVis});

  // add to effects
  double thresh = controls.getEffective("Threshold x100") / 100.0;
  cv::Mat result = cachedVis.clone();
  for (int a = 1; a <= cachedNumOfObjects; a++) {
    for (int b = a + 1; b <= cachedNumOfObjects; b++) {
      if (cachedSimMatrix[a][b] > thresh) {
        cv::Point pa(cachedCentroids[a - 1].x, cachedCentroids[a - 1].y);
        cv::Point pb(cachedCentroids[b - 1].x, cachedCentroids[b - 1].y);
        cv::line(result, pa, pb, cv::Scalar(255, 255, 0), 1);
      }
    }
  }
  outputs.push_back({"Similarity Links", result});
}
