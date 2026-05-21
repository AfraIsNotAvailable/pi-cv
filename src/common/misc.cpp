#include "misc.h"

KEY resolvedKey(const int key)
{
  switch (key) {
    case KEY_ESC:
    case KEY_SPACE:
    case KEY_ENTER:
    case KEY_DOWN_ARROW:
    case KEY_RIGHT_ARROW:
    case KEY_UP_ARROW:
    case KEY_LEFT_ARROW:
      return static_cast<KEY>(key);
    default:
      return KEY::NONE;
  }
}
