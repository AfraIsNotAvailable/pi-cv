#include "morphology.h"
#include "types.h"
#include <algorithm>

cv::Mat dilate8Once(const cv::Mat &binary) {
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

cv::Mat erode8Once(const cv::Mat &binary) {
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

cv::Mat applyMorphIterations(const cv::Mat &binary, int iterations,
                             bool useDilation) {
  cv::Mat result = binary.clone();
  const int safeIterations = std::max(0, iterations);
  for (int i = 0; i < safeIterations; ++i) {
    result = useDilation ? dilate8Once(result) : erode8Once(result);
  }
  return result;
}

cv::Mat opening8Once(const cv::Mat &binary) {
  return dilate8Once(erode8Once(binary));
}

cv::Mat closing8Once(const cv::Mat &binary) {
  return erode8Once(dilate8Once(binary));
}

cv::Mat repeatOpening(const cv::Mat &binary, int repetitions) {
  cv::Mat result = binary.clone();
  const int safeRepetitions = std::max(1, repetitions);
  for (int i = 0; i < safeRepetitions; ++i) {
    result = opening8Once(result);
  }
  return result;
}

cv::Mat repeatClosing(const cv::Mat &binary, int repetitions) {
  cv::Mat result = binary.clone();
  const int safeRepetitions = std::max(1, repetitions);
  for (int i = 0; i < safeRepetitions; ++i) {
    result = closing8Once(result);
  }
  return result;
}

cv::Mat boundaryExtraction8(const cv::Mat &binary) {
  cv::Mat eroded = erode8Once(binary);
  cv::Mat boundary;
  cv::subtract(binary, eroded, boundary);
  return boundary;
}

cv::Mat morphologicalRegionFill8(const cv::Mat &objectMask,
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
