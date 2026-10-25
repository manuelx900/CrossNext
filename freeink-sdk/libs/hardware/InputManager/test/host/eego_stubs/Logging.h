#pragma once

inline unsigned loggedErrors = 0;
#define LOG_ERR(...) (++loggedErrors)
