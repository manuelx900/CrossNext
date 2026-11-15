#pragma once
template <typename... Args>
inline void sdartTestLog(Args&&...) {}
#define LOG_ERR(...) sdartTestLog(__VA_ARGS__)

#define LOG_DBG(...) sdartTestLog(__VA_ARGS__)
