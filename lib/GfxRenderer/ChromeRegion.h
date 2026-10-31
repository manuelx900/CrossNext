#pragma once
// The render task records a logical full-width header band for output-polarity compensation.
namespace chrome {
inline int headerHeight = 0;
inline bool lightHeader = false;
inline bool invertHeaderForOutput(bool invertedDisplay) { return invertedDisplay != lightHeader; }
}  // namespace chrome
