#pragma once
#define LOG_ERR(...) \
  do {               \
    ++mockErrors;    \
  } while (0)
