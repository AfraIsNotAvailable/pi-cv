#ifndef __MISC_H__
#define __MISC_H__

#include "opencv2/opencv.hpp"
#include <string>

// Key definitions
#define KEY_ESC 27
#define KEY_SPACE 32
#define KEY_ENTER 13
// Maybe KEY_RETURN for MAC?

#define KEY_DOWN_ARROW 84
#define KEY_RIGHT_ARROW 83
#define KEY_UP_ARROW 82
#define KEY_LEFT_ARROW 81

// Key enum (prefer to use this)
enum KEY {
  ESC = KEY_ESC,
  SPACE = KEY_SPACE,
  ENTER = KEY_ENTER,

  DOWN_ARROW = KEY_DOWN_ARROW,
  RIGHT_ARROW = KEY_RIGHT_ARROW,
  UP_ARROW = KEY_UP_ARROW,
  LEFT_ARROW = KEY_LEFT_ARROW,

  NONE = 0,
};

KEY resolvedKey(const int key);

// Wait for key press and return a consistent key across platforms
#define WaitKey(D) (cv::waitKey(D) & 0xEFFFFF)

#endif