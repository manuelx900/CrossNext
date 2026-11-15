#pragma once
struct Metrics {
  int statusBarVerticalMargin = 26, progressBarMarginTop = 4, statusBarHorizontalMargin = 12;
};
struct UITheme {
  static UITheme& getInstance() {
    static UITheme instance;
    return instance;
  }
  const Metrics& getMetrics() const {
    static Metrics m;
    return m;
  }
};
