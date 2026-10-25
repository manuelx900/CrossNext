#pragma once
template <typename... Args>
inline void sdartTestLog(Args&&...) {}
#define LOG_ERR(...) sdartTestLog(__VA_ARGS__)
